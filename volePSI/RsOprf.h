#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "volePSI/Defines.h"
#include "volePSI/Paxos.h"
#include "libOTe/Vole/Silent/SilentVoleSender.h"
#include "libOTe/Vole/Silent/SilentVoleReceiver.h"

namespace volePSI
{

    class RsOprfSender : public oc::TimerAdapter
    {
    public:
        oc::SilentVoleSender mVoleSender;
        span<block> mB;
        block mD;
        Baxos mPaxos;
        bool mMalicious = false;
        block mW;
        u64 mBinSize = 1 << 14;
        u64 mSsp = 40;
        bool mDebug = false;

        void setMultType(oc::MultType type) { mVoleSender.mMultType = type; };

        Proto send(u64 n, PRNG& prng, Socket& chl, u64 mNumThreads = 0, bool reducedRounds = false);


        block eval(block v);


        void eval(span<const block> val, span<block> output, u64 mNumThreads = 0);


        Proto genVole(PRNG& prng, Socket& chl, bool reducedRounds);
    };



    class RsOprfReceiver : public oc::TimerAdapter
    {

    public:
        bool mMalicious = false;
        oc::SilentVoleReceiver mVoleRecver;
        u64 mBinSize = 1 << 14;
        u64 mSsp = 40;
        bool mDebug = false;

        void setMultType(oc::MultType type) { mVoleRecver.mMultType = type; };

        Proto receive(span<const block> values, span<block> outputs, PRNG& prng, Socket& chl, u64 mNumThreads = 0, bool reducedRounds = false);


        Proto genVole(u64 n, PRNG& prng, Socket& chl, bool reducedRounds);

    };
}