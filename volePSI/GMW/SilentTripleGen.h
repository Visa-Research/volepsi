#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "volePSI/Defines.h"
#include "volePSI/config.h"
#ifdef VOLE_PSI_ENABLE_GMW

#include <vector>
#include <libOTe/TwoChooseOne/Silent/SilentOtExtSender.h>
#include <libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h>

namespace volePSI
{


    Proto generateBase(
        RequiredBase base,
        u64 partyIdx,
        oc::PRNG& prng,
        coproto::Socket& chl,
        span<block> recvMsg,
        span<std::array<block, 2>> sendMsg,
        oc::Timer* timer = nullptr);

    Proto extend(
        RequiredBase b,
        span<std::array<block, 2>> baseMsg,
        oc::PRNG& prng,
        coproto::Socket& chl,
        span<block> recvMsgP,
        span<std::array<block, 2>> sendMsgP);

    Proto extend(
        RequiredBase b,
        oc::BitVector baseChoice,
        span<block> baseMsg,
        oc::PRNG& prng,
        coproto::Socket& chl,
        span<block> recvMsgP,
        span<std::array<block, 2>> sendMsgP);

    class SilentTripleGen : public oc::TimerAdapter
    {
    public:
        bool mHasBase = false;
        u64 mN, mBatchSize, mNumPer;
        Mode mMode;
        oc::PRNG mPrng;

        std::unique_ptr<oc::SilentOtExtSender[]> mBackingSenderOT;
        std::unique_ptr<oc::SilentOtExtReceiver[]> mBackingRecverOT;

        span<oc::SilentOtExtSender> mSenderOT;
        span<oc::SilentOtExtReceiver> mRecverOT;

        void init(u64 n, u64 batchSize, u64 numThreads, Mode mode, block seed);


        RequiredBase mBase;

        RequiredBase requiredBaseOts();

        void setBaseOts(span<block> recvOts, span<std::array<block, 2>> sendOts);

        Proto expand(coproto::Socket& chl);

        bool hasBaseOts()  { return mHasBase; }


        Proto generateBaseOts(u64 partyIdx, oc::PRNG& prng, coproto::Socket& chl)
        {
            
            auto rMsg = std::vector<block>{};
            auto sMsg = std::vector<std::array<block, 2>>{};
            auto b = RequiredBase{ };

            setTimePoint("TripleGen::generateBaseOts begin");
            b = requiredBaseOts();

            if (b.mNumSend || b.mRecvChoiceBits.size())
            {
                rMsg.resize(b.mRecvChoiceBits.size());
                sMsg.resize(b.mNumSend);
                co_await (generateBase(b, partyIdx, prng, chl, rMsg, sMsg, mTimer));
                setBaseOts(rMsg, sMsg);
            }
            setTimePoint("TripleGen::generateBaseOts end");

        }


        // A * C = B + D
        span<block> mMult, mAdd;
        std::vector<block> mAVec, mBVec, mDVec;
        oc::BitVector mCBitVec;
    };



}
#endif