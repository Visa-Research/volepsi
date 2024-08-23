#include "RsCpsi.h"

#include <sstream>

namespace volePSI
{

    Proto RsCpsiSender::send(span<block> Y, oc::MatrixView<u8> values, Sharing& ret, Socket& chl)
    {
            auto cuckooSeed = block{};
            auto params = oc::CuckooParam{};
            auto numBins = u64{};
            auto sIdx = SimpleIndex{};
            auto keyBitLength = u64{};
            auto keyByteLength = u64{};
            auto hashers = std::array<oc::AES, 3> {};
            auto Ty = std::vector<block>{};
            auto Tv = Matrix<u8>{};
            auto r = Matrix<u8>{};
            auto TyIter = std::vector<block>::iterator{};
            auto TvIter = Matrix<u8>::iterator{};
            auto rIter = Matrix<u8>::iterator{};
            auto opprf = std::make_unique<RsOpprfSender>();
            auto cmp = std::make_unique<Gmw>();
            auto cir = BetaCircuit{};

        setTimePoint("RsCpsiSender::send begin");
        if (mSenderSize != Y.size() || mValueByteLength != values.cols())
        {
            co_await chl.close();
            throw RTE_LOC;
        }

        co_await (chl.recv(cuckooSeed));
        setTimePoint("RsCpsiSender::send recv");

        params = oc::CuckooIndex<>::selectParams(mRecverSize, mSsp, 0, 3);
        numBins = params.numBins();
        sIdx.init(numBins, mSenderSize, mSsp, 3);
        sIdx.insertItems(Y, cuckooSeed);

        setTimePoint("RsCpsiSender::send simpleHash");


        keyBitLength = mSsp + oc::log2ceil(params.numBins());
        keyByteLength = oc::divCeil(keyBitLength, 8);

        hashers[0].setKey(block(3242, 23423) ^ cuckooSeed);
        hashers[1].setKey(block(4534, 45654) ^ cuckooSeed);
        hashers[2].setKey(block(5677, 67867) ^ cuckooSeed);

        // The OPPRF input value of the i'th input under the j'th cuckoo
        // hash function.
        Ty.resize(Y.size() * 3);

        // The value associated with the k'th OPPRF input
        Tv.resize(Y.size() * 3, keyByteLength + values.cols(), oc::AllocType::Uninitialized);

        // The special value assigned to the i'th bin.
        r.resize(numBins, keyByteLength, oc::AllocType::Uninitialized);

        TyIter = Ty.begin();
        TvIter = Tv.begin();
        rIter = r.begin();
        ret.mValues.resize(numBins, values.cols(), oc::AllocType::Uninitialized);
        mPrng.get<u8>(r);
        mPrng.get<u8>(ret.mValues);



        for (u64 i = 0; i < numBins; ++i)
        {
            auto bin = sIdx.mBins[i];
            auto size = sIdx.mBinSizes[i];

            for (u64 p = 0; p < size; ++p)
            {
                auto j = bin[p].hashIdx();
                auto& hj = hashers[j];
                auto b = bin[p].idx();
                *TyIter = hj.hashBlock(Y[b]);
                memcpy(&*TvIter, &*rIter, keyByteLength);
                TvIter += keyByteLength;

                if (values.size())
                {
                    memcpy(&*TvIter, &values(b, 0), values.cols());

                    if (mType == ValueShareType::Xor)
                    {
                        for (u64 k = 0; k < values.cols(); ++k)
                        {
                            TvIter[k] ^= ret.mValues(i, k);
                        }
                    }
                    else if (mType == ValueShareType::add32)
                    {
                        assert(values.cols() % sizeof(u32) == 0);
                        auto ss = values.cols() / sizeof(u32);
                        auto tv = (u32*)&*TvIter;
                        auto rr = (u32*)&ret.mValues(i, 0);
                        for (u64 k = 0; k < ss; ++k)
                            tv[k] -= rr[k];
                    }
                    else
                    {
                        co_await chl.close();
                        throw RTE_LOC;
                    }
                    TvIter += values.cols();
                }

                ++TyIter;
            }
            rIter += keyByteLength;
        }

        while (TyIter != Ty.end())
        {
            *TyIter = mPrng.get();
            ++TyIter;
        }

        setTimePoint("RsCpsiSender::send setValues");

        if (mTimer)
            opprf->setTimer(*mTimer);

        co_await (opprf->send(numBins, Ty, Tv, mPrng, mNumThreads, chl));

        if (mTimer)
            cmp->setTimer(*mTimer);

        cir = isZeroCircuit(keyBitLength);
        cmp->init(r.rows(), cir, mNumThreads, 1, mPrng.get());

        cmp->setInput(0, r);
        co_await (cmp->run(chl));

        {

            auto ss = cmp->getOutputView(0);
            ret.mFlagBits.resize(numBins);
            std::copy(ss.begin(), ss.begin() + ret.mFlagBits.sizeBytes(), ret.mFlagBits.data());
        }
    }

    Proto RsCpsiReceiver::receive(span<block> X, Sharing& ret, Socket& chl)
    {
        auto cuckooSeed = block{};
            auto cuckoo = oc::CuckooIndex<>{};
            auto Tx = std::vector<block>{};
            auto hashers = std::array<oc::AES, 3> {};
            auto numBins = u64{};
            auto keyBitLength = u64{};
            auto keyByteLength = u64{};
            auto r = Matrix<u8>{};
            auto opprf = std::make_unique<RsOpprfReceiver>();
            auto cmp = std::make_unique<Gmw>();
            auto cir = BetaCircuit{};

        if (mRecverSize != X.size())
            throw RTE_LOC;

        setTimePoint("RsCpsiReceiver::receive begin");

        cuckooSeed = mPrng.get();
        co_await (chl.send(std::move(cuckooSeed)));
        cuckoo.init(mRecverSize, mSsp, 0, 3);

        cuckoo.insert(X, cuckooSeed);
        Tx.resize(cuckoo.mNumBins);

        setTimePoint("RsCpsiReceiver::receive cuckoo");

        hashers[0].setKey(block(3242, 23423) ^ cuckooSeed);
        hashers[1].setKey(block(4534, 45654) ^ cuckooSeed);
        hashers[2].setKey(block(5677, 67867) ^ cuckooSeed);

        ret.mMapping.resize(X.size(), ~u64(0));
        numBins = cuckoo.mBins.size();
        for (u64 i = 0; i < numBins; ++i)
        {
            auto& bin = cuckoo.mBins[i];
            if (bin.isEmpty() == false)
            {
                auto j = bin.hashIdx();
                auto b = bin.idx();
            
                //j = oc::CuckooIndex<>::minCollidingHashIdx(i, cuckoo.mHashes[b], 3, numBins);

                auto& hj = hashers[j];
                Tx[i] = hj.hashBlock(X[b]);
                ret.mMapping[b] = i;
            }
            else
            {
                Tx[i] = block(i, 0);
            }
        }
        setTimePoint("RsCpsiReceiver::receive values");

        keyBitLength = mSsp + oc::log2ceil(Tx.size());
        keyByteLength = oc::divCeil(keyBitLength, 8);

        r.resize(Tx.size(), keyByteLength + mValueByteLength, oc::AllocType::Uninitialized);

        if (mTimer)
            opprf->setTimer(*mTimer);

        co_await (opprf->receive(mSenderSize * 3, Tx, r, mPrng, mNumThreads, chl));

        if (mTimer)
            cmp->setTimer(*mTimer);

        cir = isZeroCircuit(keyBitLength);
        cmp->init(r.rows(), cir, mNumThreads, 0, mPrng.get());

        cmp->implSetInput(0, r, r.cols());

        co_await (cmp->run(chl));

        {
            auto ss = cmp->getOutputView(0);

            ret.mFlagBits.resize(numBins);
            std::copy(ss.begin(), ss.begin() + ret.mFlagBits.sizeBytes(), ret.mFlagBits.data());

            if (mValueByteLength)
            {
                ret.mValues.resize(numBins, mValueByteLength);

                for (u64 i = 0; i < numBins; ++i)
                {
                    std::memcpy(&ret.mValues(i, 0), &r(i, keyByteLength), mValueByteLength);
                }
            }
        }

        setTimePoint("RsCpsiReceiver::receive done");
    }


}