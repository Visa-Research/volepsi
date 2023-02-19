#include "RsOprf.h"

namespace volePSI
{
    Proto RsOprfSender::send(u64 n, PRNG& prng, Socket& chl, u64 numThreads, bool reducedRounds)
    {
        MC_BEGIN(Proto, this, n, &prng, &chl, numThreads, reducedRounds,
            ws = block{},
            hBuff = std::array<u8, 32> {},
            ro = oc::RandomOracle(32),
            pPtr = std::unique_ptr<block[]>{},
            pp = span<block>{},
            subPp = span<block>{},
            remB = span<block>{},
            subB = span<block>{},
            fu = std::move(macoro::eager_task<void>{}),
            recvIdx = u64{ 0 },
            fork = Socket{}
        );

        setTimePoint("RsOprfSender::send-begin");
        ws = prng.get();

        mPaxos.init(n, mBinSize, 3, mSsp, PaxosParam::GF128, oc::ZeroBlock);

        mD = prng.get();

        if (mMalicious)
        {
            mVoleSender.mMalType = oc::SilentSecType::Malicious;
            ro.Update(ws);
            ro.Final(hBuff);
            MC_AWAIT(chl.send(std::move(hBuff)));
        }

        if (mTimer)
            mVoleSender.setTimer(*mTimer);

        numThreads = std::max<u64>(1, numThreads);
        mVoleSender.mNumThreads = numThreads;
        // a + b  = c * d
        fork = chl.fork();
        fu = genVole(prng, fork, reducedRounds)
            | macoro::make_eager();

        MC_AWAIT(chl.recv(mPaxos.mSeed));
        setTimePoint("RsOprfSender::recv-seed");
        MC_AWAIT(fu);
        mB = mVoleSender.mB;
        setTimePoint("RsOprfSender::send-vole");




        if (mMalicious)
        {
            MC_AWAIT(chl.recv(mW));
            MC_AWAIT(chl.send(std::move(ws)));
            mW = mW ^ ws;
            setTimePoint("RsOprfSender::recv-mal");
        }

        pPtr.reset(new block[mPaxos.size()]);
        pp = span<block>(pPtr.get(), mPaxos.size());

        setTimePoint("RsOprfSender::alloc ");

        if (0)
        {
            MC_AWAIT(chl.recv(pp));

            setTimePoint("RsOprfSender::send-recv");
            {

                auto main = mB.size() / 8 * 8;
                auto b = mB.data();
                auto p = pp.data();
                for (u64 i = 0; i < main; i += 8)
                {
                    b[0] = b[0] ^ mD.gf128Mul(p[0]);
                    b[1] = b[1] ^ mD.gf128Mul(p[1]);
                    b[2] = b[2] ^ mD.gf128Mul(p[2]);
                    b[3] = b[3] ^ mD.gf128Mul(p[3]);
                    b[4] = b[4] ^ mD.gf128Mul(p[4]);
                    b[5] = b[5] ^ mD.gf128Mul(p[5]);
                    b[6] = b[6] ^ mD.gf128Mul(p[6]);
                    b[7] = b[7] ^ mD.gf128Mul(p[7]);

                    b += 8;
                    p += 8;
                }

                for (u64 i = main; i < mB.size(); ++i)
                {
                    mB[i] = mB[i] ^ mD.gf128Mul(pp[i]);
                }
            }
            setTimePoint("RsOprfSender::send-gf128Mul");
        }
        else
        {
            remB = mB;

            while (pp.size())
            {

                subPp = pp.subspan(0, std::min<u64>(pp.size(), 1 << 28));
                pp = pp.subspan(subPp.size());

                subB = remB.subspan(0, subPp.size());
                remB = remB.subspan(subPp.size());

                setTimePoint("RsOprfSender::pre*-" + std::to_string(recvIdx));
                MC_AWAIT(chl.recv(subPp));
                setTimePoint("RsOprfSender::recv-" + std::to_string(recvIdx));

                {

                    auto main = subB.size() / 8 * 8;
                    auto b = subB.data();
                    auto p = subPp.data();
                    for (u64 i = 0; i < main; i += 8)
                    {
                        b[0] = b[0] ^ mD.gf128Mul(p[0]);
                        b[1] = b[1] ^ mD.gf128Mul(p[1]);
                        b[2] = b[2] ^ mD.gf128Mul(p[2]);
                        b[3] = b[3] ^ mD.gf128Mul(p[3]);
                        b[4] = b[4] ^ mD.gf128Mul(p[4]);
                        b[5] = b[5] ^ mD.gf128Mul(p[5]);
                        b[6] = b[6] ^ mD.gf128Mul(p[6]);
                        b[7] = b[7] ^ mD.gf128Mul(p[7]);

                        b += 8;
                        p += 8;
                    }

                    for (u64 i = main; i < subB.size(); ++i, ++b, ++p)
                    {
                        *b = *b ^ mD.gf128Mul(*p);
                    }
                }
                setTimePoint("RsOprfSender::gf128Mul-" + std::to_string(recvIdx));

                ++recvIdx;

            }


        }

        MC_END();
    }

    block RsOprfSender::eval(block v)
    {
        block o;
        eval({ &v,1 }, { &o,1 }, 1);
        return o;
    }
    void RsOprfSender::eval(span<const block> val, span<block> output, u64 numThreads)
    {
        setTimePoint("RsOprfSender::eval-begin");

        mPaxos.decode<block>(val, output, mB, numThreads);

        setTimePoint("RsOprfSender::eval-decode");

        auto main = val.size() / 8 * 8;
        auto o = output.data();
        auto v = val.data();
        std::array<block, 8> h;

        // todo, parallelize this.
        if (mMalicious)
        {
            oc::MultiKeyAES<8> hasher;

            //main = 0;
            for (u64 i = 0; i < main; i += 8)
            {
                oc::mAesFixedKey.hashBlocks<8>(v, h.data());
                o[0] = o[0] ^ mD.gf128Mul(h[0]);
                o[1] = o[1] ^ mD.gf128Mul(h[1]);
                o[2] = o[2] ^ mD.gf128Mul(h[2]);
                o[3] = o[3] ^ mD.gf128Mul(h[3]);
                o[4] = o[4] ^ mD.gf128Mul(h[4]);
                o[5] = o[5] ^ mD.gf128Mul(h[5]);
                o[6] = o[6] ^ mD.gf128Mul(h[6]);
                o[7] = o[7] ^ mD.gf128Mul(h[7]);


                o[0] = o[0] ^ mW;
                o[1] = o[1] ^ mW;
                o[2] = o[2] ^ mW;
                o[3] = o[3] ^ mW;
                o[4] = o[4] ^ mW;
                o[5] = o[5] ^ mW;
                o[6] = o[6] ^ mW;
                o[7] = o[7] ^ mW;

                hasher.setKeys({ o, 8 });
                hasher.hashNBlocks(v, o);

                o += 8;
                v += 8;
            }
            for (u64 i = main; i < val.size(); ++i)
            {
                auto h = oc::mAesFixedKey.hashBlock(val[i]);
                output[i] = output[i] ^ mD.gf128Mul(h);

                output[i] = output[i] ^ mW;
                output[i] = oc::AES(output[i]).hashBlock(val[i]);
            }
        }
        else
        {

            for (u64 i = 0; i < main; i += 8)
            {
                oc::mAesFixedKey.hashBlocks<8>(v, h.data());
                //auto h = v;

                o[0] = o[0] ^ mD.gf128Mul(h[0]);
                o[1] = o[1] ^ mD.gf128Mul(h[1]);
                o[2] = o[2] ^ mD.gf128Mul(h[2]);
                o[3] = o[3] ^ mD.gf128Mul(h[3]);
                o[4] = o[4] ^ mD.gf128Mul(h[4]);
                o[5] = o[5] ^ mD.gf128Mul(h[5]);
                o[6] = o[6] ^ mD.gf128Mul(h[6]);
                o[7] = o[7] ^ mD.gf128Mul(h[7]);

                oc::mAesFixedKey.hashBlocks<8>(o, o);

                o += 8;
                v += 8;
            }



            for (u64 i = main; i < val.size(); ++i)
            {
                auto h = oc::mAesFixedKey.hashBlock(val[i]);
                output[i] = output[i] ^ mD.gf128Mul(h);
                output[i] = oc::mAesFixedKey.hashBlock(output[i]);
            }
        }

        setTimePoint("RsOprfSender::eval-hash");

    }

    Proto RsOprfSender::genVole(PRNG& prng, Socket& chl, bool reduceRounds)
    {
        if (reduceRounds)
            mVoleSender.configure(mPaxos.size(), oc::SilentBaseType::Base);

        return mVoleSender.silentSendInplace(mD, mPaxos.size(), prng, chl);
    }

    struct UninitVec : span<block>
    {
        std::unique_ptr<block[]> ptr;

        void resize(u64 s)
        {
            ptr.reset(new block[s]);
            static_cast<span<block>&>(*this) = span<block>(ptr.get(), s);
        }
    };

    Proto RsOprfReceiver::receive(span<const block> values, span<block> outputs, PRNG& prng, Socket& chl, u64 numThreads, bool reducedRounds)
    {

        MC_BEGIN(Proto, this, values, outputs, &prng, &chl, numThreads, reducedRounds,
            hashingSeed = block{},
            wr = block{},
            ws = block{},
            Hws = std::array<u8, 32> {},
            paxos = Baxos{},
            hPtr = std::unique_ptr<block[]>{},
            h = span<block>{},
            p = std::move(UninitVec{}),
            subP = span<block>{},
            subC = span<block>{},
            a = span<block>{},
            c = span<block>{},
            fu = std::move(macoro::eager_task<void>{}),
            ii = u64{},
            fork = Socket{}
        );

        setTimePoint("RsOprfReceiver::receive-begin");

        if (values.size() != outputs.size())
            throw RTE_LOC;

        hashingSeed = prng.get(), wr = prng.get();
        paxos.mDebug = mDebug;
        paxos.init(values.size(), mBinSize, 3, mSsp, PaxosParam::GF128, hashingSeed);

        MC_AWAIT(chl.send(std::move(hashingSeed)));

        if (mMalicious)
        {
            mVoleRecver.mMalType = oc::SilentSecType::Malicious;
            MC_AWAIT(chl.recv(Hws));
        }

        if (mTimer)
            mVoleRecver.setTimer(*mTimer);

        fork = chl.fork();
        fu = genVole(paxos.size(), prng, fork, reducedRounds)
            | macoro::make_eager();



        hPtr.reset(new block[values.size()]);
        h = span<block>(hPtr.get(), values.size());

        oc::mAesFixedKey.hashBlocks(values, h);
        setTimePoint("RsOprfReceiver::receive-hash");

        //auto pPtr = std::make_shared<std::vector<block>>(paxos.size());
        //span<block> p = *pPtr;

        p.resize(paxos.size());

        setTimePoint("RsOprfReceiver::receive-alloc");

        paxos.solve<block>(values, h, p, nullptr, numThreads);
        setTimePoint("RsOprfReceiver::receive-solve");
        MC_AWAIT(fu);

        // a + b  = c * d
        a = mVoleRecver.mA;
        c = mVoleRecver.mC;

        setTimePoint("RsOprfReceiver::receive-vole");


        if (mMalicious)
        {
            MC_AWAIT(chl.send(std::move(wr)));
        }


        //if (numThreads > 1)
        //    thrd.join();
        if (0)
        {
            {
                auto main = (p.size() / 8) * 8;
                block* __restrict pp = p.data();
                block* __restrict cc = c.data();
                for (u64 i = 0; i < main; i += 8)
                {
                    pp[0] = pp[0] ^ cc[0];
                    pp[1] = pp[1] ^ cc[1];
                    pp[2] = pp[2] ^ cc[2];
                    pp[3] = pp[3] ^ cc[3];
                    pp[4] = pp[4] ^ cc[4];
                    pp[5] = pp[5] ^ cc[5];
                    pp[6] = pp[6] ^ cc[6];
                    pp[7] = pp[7] ^ cc[7];

                    pp += 8;
                    cc += 8;
                }
                for (u64 i = main; i < p.size(); ++i)
                    p[i] = p[i] ^ c[i];
            }

            setTimePoint("RsOprfReceiver::receive-xor");

            // send c ^ p
            MC_AWAIT(chl.send(std::move(p)));
        }
        else
        {
            ii = 0;

            while (c.size())
            {

                subP = p.subspan(0, std::min<u64>(p.size(), 1 << 28));
                subC = c.subspan(0, subP.size());

                if (p.size() != subP.size())
                    static_cast<span<block>&>(p) = p.subspan(subP.size());
                c = c.subspan(subP.size());

                {

                    auto main = (subP.size() / 8) * 8;
                    block* __restrict pp = subP.data();
                    block* __restrict cc = subC.data();
                    for (u64 i = 0; i < main; i += 8)
                    {
                        pp[0] = pp[0] ^ cc[0];
                        pp[1] = pp[1] ^ cc[1];
                        pp[2] = pp[2] ^ cc[2];
                        pp[3] = pp[3] ^ cc[3];
                        pp[4] = pp[4] ^ cc[4];
                        pp[5] = pp[5] ^ cc[5];
                        pp[6] = pp[6] ^ cc[6];
                        pp[7] = pp[7] ^ cc[7];

                        pp += 8;
                        cc += 8;
                    }
                    for (u64 i = main; i < p.size(); ++i, ++pp, ++cc)
                        *pp = *pp ^ *cc;
                }

                setTimePoint("RsOprfReceiver::receive-xor");

                if (p.size() != subP.size())
                    MC_AWAIT(chl.send(std::move(subP)));
                else
                    MC_AWAIT(chl.send(std::move(p)));

                setTimePoint("RsOprfReceiver::receive-send");

                ++ii;
            }

        }

        paxos.decode<block>(values, outputs, a, numThreads);

        setTimePoint("RsOprfReceiver::receive-decode");

        if (mMalicious)
        {
            MC_AWAIT(chl.recv(ws));

            {
                std::array<u8, 32> Hws2;
                oc::RandomOracle ro(32);
                ro.Update(ws);
                ro.Final(Hws2);
                if (Hws != Hws2)
                    throw RTE_LOC;

                auto w = ws ^ wr;

                // todo, parallelize this.

                // compute davies-meyer F
                //    F(x) = H( Decode(x, a) + w, x)
                // where 
                //    H(u,v) = AES_u(v) ^ v

                oc::MultiKeyAES<8> hasher;
                auto main = outputs.size() / 8 * 8;
                auto o = outputs.data();
                auto v = values.data();
                for (u64 i = 0; i < main; i += 8)
                {
                    o[0] = o[0] ^ w;
                    o[1] = o[1] ^ w;
                    o[2] = o[2] ^ w;
                    o[3] = o[3] ^ w;
                    o[4] = o[4] ^ w;
                    o[5] = o[5] ^ w;
                    o[6] = o[6] ^ w;
                    o[7] = o[7] ^ w;

                    // o = H(o, v)
                    hasher.setKeys({ o, 8 });
                    hasher.hashNBlocks(v, o);

                    o += 8;
                    v += 8;
                }

                for (u64 i = main; i < outputs.size(); ++i)
                {
                    outputs[i] = outputs[i] ^ w;
                    outputs[i] = oc::AES(outputs[i]).hashBlock(values[i]);
                }
            }
        }
        else
        {
            // todo, parallelize this.

            // compute davies-meyer-Oseas F
            //      F(x) = H(Decode(x, a))
            // where
            //      H(u) = AES_fixed(u) ^ u
            oc::mAesFixedKey.hashBlocks(outputs, outputs);
        }

        setTimePoint("RsOprfReceiver::receive-hash");
        MC_END();
    }

    Proto RsOprfReceiver::genVole(u64 n, PRNG& prng, Socket& chl, bool reducedRounds)
    {
        if (reducedRounds)
            mVoleRecver.configure(n, oc::SilentBaseType::Base);
        return mVoleRecver.silentReceiveInplace(n, prng, chl);
    }

}