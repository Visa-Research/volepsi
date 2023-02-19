#include "RsPsi.h"
#include <array>
#include <future>
//#include "thirdparty/parallel-hashmap/parallel_hashmap/phmap.h"
namespace volePSI
{



    template <typename T>
    struct Buffer : public span<T>
    {
        std::unique_ptr<T[]> mPtr;

        void resize(u64 s)
        {
            mPtr.reset(new T[s]);
            static_cast<span<T>&>(*this) = span<T>(mPtr.get(), s);
        }
    };

    void details::RsPsiBase::init(
        u64 senderSize,
        u64 recverSize,
        u64 statSecParam,
        block seed,
        bool malicious,
        u64 numThreads, 
        bool useReducedRounds)
    {
        mSenderSize = senderSize;
        mRecverSize = recverSize;
        mSsp = statSecParam;
        mPrng.SetSeed(seed);
        mMalicious = malicious;

        mMaskSize = malicious  ?
            sizeof(block) :
            std::min<u64>(oc::divCeil(mSsp + oc::log2ceil(mSenderSize * mRecverSize), 8), sizeof(block));
        mCompress = mMaskSize != sizeof(block);

        mNumThreads = numThreads;
        mUseReducedRounds = useReducedRounds;
    }

    Proto RsPsiSender::run(span<block> inputs, Socket& chl)
    {

        MC_BEGIN(Proto,this, inputs, &chl,
            hashes = std::move(Buffer<u8>{})
        );
        setTimePoint("RsPsiSender::run-begin");

        if (mTimer)
            mSender.setTimer(getTimer());

        mSender.mMalicious = mMalicious;
        mSender.mSsp = mSsp;
        mSender.mDebug = mDebug;

        MC_AWAIT(mSender.send(mRecverSize, mPrng, chl, mNumThreads, mUseReducedRounds));

        setTimePoint("RsPsiSender::run-opprf");

        hashes.resize(inputs.size() * sizeof(block));
        mSender.eval(inputs, span<block>((block*)hashes.data(), inputs.size()), mNumThreads);

        setTimePoint("RsPsiSender::run-eval");
        if (mCompress)
        {
            auto src = (block*)hashes.data();
            auto dest = (u8*)hashes.data();
            u64 i = 0;

            for (; i < std::min<u64>(mSenderSize, 100); ++i)
            {
                memmove(dest, src, mMaskSize);
                dest += mMaskSize;
                src += 1;
            }
            for (; i < mSenderSize; ++i)
            {
                memcpy(dest, src, mMaskSize);
                dest += mMaskSize;
                src += 1;
            }
            static_cast<span<u8>&>(hashes) = span<u8>((u8*)hashes.data(), dest);
        }

        MC_AWAIT(chl.send(std::move(hashes)));
        setTimePoint("RsPsiSender::run-sendHash");

        MC_END();
    }

    namespace {
        struct NoHash
        {
            inline size_t operator()(const block& v) const
            {
                return v.get<size_t>(0);
            }
        };
    }

    Proto RsPsiReceiver::run(span<block> inputs, Socket& chl)
    {
        setTimePoint("RsPsiReceiver::run-enter");
        static const u64 batchSize = 128;

        struct MultiThread
        {
            std::promise<void> prom;
            std::shared_future<void> fu;
            std::vector<std::thread> thrds;
            std::function<void(u64)>routine;
            std::atomic<u64> numDone;
            std::promise<void> hashingDoneProm;
            std::shared_future<void> hashingDoneFu;
            std::mutex mMergeMtx;

            u64 numThreads;
            u64 binSize;
            libdivide::libdivide_u32_t divider;
        };

        MC_BEGIN(Proto,this, inputs, &chl,
            data = std::unique_ptr<u8[]>{},
            myHashes = span<block>{},
            theirHashes = oc::MatrixView<u8>{},
            map = google::dense_hash_map<block, u64, NoHash>{},
            i = u64{},
            main = u64{},
            hh = std::array<std::pair<block, u64>, 128> {},
            mt = std::unique_ptr<MultiThread>{},
            mask = block{}
        );

        setTimePoint("RsPsiReceiver::run-begin");
        mIntersection.clear();

        data = std::unique_ptr<u8[]>(new u8[
                mSenderSize * mMaskSize +
                mRecverSize * sizeof(block)]);

        myHashes = span<block>((block*)data.get(), mRecverSize);
        theirHashes = oc::MatrixView<u8>((u8*)((block*)data.get() + mRecverSize), mSenderSize, mMaskSize);

        setTimePoint("RsPsiReceiver::run-alloc");

        if (mTimer)
            mRecver.setTimer(getTimer());

        mRecver.mMalicious = mMalicious;
        mRecver.mSsp = mSsp;
        mRecver.mDebug = mDebug;

        // todo, parallelize these two
        MC_AWAIT(mRecver.receive(inputs, myHashes, mPrng, chl, mNumThreads, mUseReducedRounds));
        setTimePoint("RsPsiReceiver::run-opprf");

        mask = oc::ZeroBlock;
        for (i = 0; i < mMaskSize; ++i)
            mask.set<u8>(i,~0);

        if (mNumThreads < 2)
        {

            map.resize(myHashes.size());
            setTimePoint("RsPsiReceiver::run-reserve");
            map.set_empty_key(oc::ZeroBlock);
            setTimePoint("RsPsiReceiver::run-set_empty_key");

            main = mRecverSize / batchSize * batchSize;

            if (!mCompress)
            {

                for (i = 0; i < main; i += batchSize)
                {
                    for (u64 j = 0; j < batchSize; ++j)
                        hh[j] = { myHashes[i + j], i + j };

                    map.insert(hh.begin(), hh.end());
                }
                for (; i < mRecverSize; ++i)
                    map.insert({ myHashes[i], i });
            }
            else
            {

                for (i = 0; i < main; i += batchSize)
                {
                    for (u64 j = 0; j < batchSize; ++j)
                        hh[j] = { myHashes[i + j] & mask, i + j };

                    map.insert(hh.begin(), hh.end());
                }
                for (; i < mRecverSize; ++i)
                    map.insert({ myHashes[i] & mask, i });
            }

            setTimePoint("RsPsiReceiver::run-insert");

            MC_AWAIT(chl.recv(theirHashes));

            setTimePoint("RsPsiReceiver::run-recv");

            {
                block h = oc::ZeroBlock;
                auto iter = theirHashes.data();
                for (i = 0; i < mSenderSize; ++i)
                {
                    memcpy(&h, iter, mMaskSize);
                    iter += mMaskSize;
                    
                    auto iter = map.find(h);
                    if (iter != map.end())
                    {
                        mIntersection.push_back(iter->second);
                    }
                }
            }

            setTimePoint("RsPsiReceiver::run-find");
        }
        else
        {
            mt.reset(new MultiThread);

            mt->fu = mt->prom.get_future().share();

            setTimePoint("RsPsiReceiver::run-reserve");

            mt->numDone = 0;
            mt->hashingDoneFu = mt->hashingDoneProm.get_future().share();

            mt->numThreads = std::max<u64>(1, mNumThreads);
            mt->binSize = Baxos::getBinSize(mNumThreads, mRecverSize, mSsp);
            mt->divider = libdivide::libdivide_u32_gen(mt->numThreads);

            mt->routine = [&](u64 thrdIdx)
            {
                if (!thrdIdx)
                    setTimePoint("RsPsiReceiver::run-threadBegin");

                auto& divider = mt->divider;
                google::dense_hash_map<block, u64, NoHash> map(mt->binSize);
                map.set_empty_key(oc::ZeroBlock);

                if (!thrdIdx)
                    setTimePoint("RsPsiReceiver::run-set_empty_key_par");

                u64 i = 0;
                std::array<std::pair<block, u64>, batchSize> hh;
                for (; i < myHashes.size();)
                {
                    u64 j = 0;
                    while (j != batchSize && i < myHashes.size())
                    {
                        auto v = myHashes[i].get<u32>(0);
                        auto k = libdivide::libdivide_u32_do(v, &divider);
                        v -= k * mNumThreads;
                        if (v == thrdIdx)
                        {
                            hh[j] = { myHashes[i] & mask, i };
                            ++j;
                        }
                        ++i;
                    }
                    map.insert(hh.begin(), hh.begin() + j);
                }

                if (++mt->numDone == mt->numThreads)
                    mt->hashingDoneProm.set_value();
                else
                    mt->hashingDoneFu.get();

                if (!thrdIdx)
                    setTimePoint("RsPsiReceiver::run-insert_par");

                mt->fu.get();
                if (!thrdIdx)
                    setTimePoint("RsPsiReceiver::run-recv_par");

                auto begin = thrdIdx * myHashes.size() / mNumThreads;
                u64 intersectionSize = 0;
                u64* intersection = (u64*)&myHashes[begin];

                {
                    block h = oc::ZeroBlock;
                    auto iter = theirHashes.data();
                    for (i = 0; i < mSenderSize; ++i)
                    {
                        memcpy(&h, iter, mMaskSize);
                        iter += mMaskSize;

                        auto v = h.get<u32>(0);
                        auto k = libdivide::libdivide_u32_do(v, &divider);
                        v -= k * mNumThreads;
                        if (v == thrdIdx)
                        {
                            auto iter = map.find(h);
                            if (iter != map.end())
                            {
                                intersection[intersectionSize] = iter->second;
                                ++intersectionSize;
                            }
                        }
                    }
                }

                if (!thrdIdx)
                    setTimePoint("RsPsiReceiver::run-find_par");
                if (intersectionSize)
                {
                    std::lock_guard<std::mutex> lock(mt->mMergeMtx);
                    mIntersection.insert(mIntersection.end(), intersection, intersection + intersectionSize);
                }
            };


            mt->thrds.resize(mt->numThreads);
            for (i = 0; i < mt->thrds.size(); ++i)
                mt->thrds[i] = std::thread(mt->routine, i);
            MC_AWAIT(chl.recv(theirHashes));
            mt->prom.set_value();

            for (i = 0; i < mt->thrds.size(); ++i)
                mt->thrds[i].join();

            setTimePoint("RsPsiReceiver::run-done");

        }

        MC_END();
    }

}