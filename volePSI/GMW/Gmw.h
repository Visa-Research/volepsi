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

#include "volePSI/GMW/Circuit.h"
#include "volePSI/GMW/SilentTripleGen.h"
#include <list>
#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Common/Matrix.h>
namespace volePSI
{

    enum class OtExtType
    {
        IKNP,
        Silent,
        InsecureMock
    };
    class Gmw : public oc::TimerAdapter
    {
    public:

        struct Debug
        {
            bool mDebug = false;
            std::vector<block> mA, mB, mC, mD;
            std::list<std::array<std::vector<block>,2>> mU, mW;
            Matrix<int> mVals;
            Matrix<block> mWords;
        };


        Debug mO;

        // allow the circuit to be reordered into levels
        // based on their AND depth.
        BetaCircuit::LevelizeType mLevelize = BetaCircuit::LevelizeType::Reorder;

        u64 mN = 0, mNumOts = 0, mIdx;
        OtExtType mOtExtType;
        //u64 mBitCount;
        Matrix<block> mWords;
        u64 mRoundIdx = 0, mNumRounds;
        BetaCircuit mCir;
        span<oc::BetaGate> mGates;

        oc::PRNG mPrng, mPhiPrng;
                
        span<block> mA, mB, mC, mC2, mD;

        u64 mDebugPrintIdx = -1;
        BetaCircuit::PrintIter mPrint;


        void init(
            u64 n,
            BetaCircuit& cir,
            u64 numThreads, 
            u64 pIdx, 
            block seed);

        void setTriples(span<block> a, span<block> b, span<block> c, span<block> d)
        {
            mA = a;
            mB = b;
            mC = c;
            mC2 = c;
            mD = d;
        }

        Proto generateTriple(
            u64 batchSize,
            u64 numThreads,
            coproto::Socket& chl);

        template<typename T>
        void setInput(u64 i, oc::MatrixView<T> input)
        {
            static_assert(std::is_trivially_copyable<T>::value, "expecting trivial");
            oc::MatrixView<u8> ii((u8*)input.data(), input.rows(), input.cols() * sizeof(T));
            implSetInput(i, ii, sizeof(T));
        }

        void setZeroInput(u64 i);

        Proto run(coproto::Socket& chl);

        template<typename T>
        void getOutput(u64 i, oc::MatrixView<T> out)
        {
            static_assert(std::is_trivially_copyable<T>::value, "expecting trivial");
            oc::MatrixView<u8> ii((u8*)out.data(), out.rows(), out.cols() * sizeof(T));
            implGetOutput(i, ii, sizeof(T));
        }


        void implSetInput(u64 i, oc::MatrixView<u8> input, u64 alignment);
        void implGetOutput(u64 i, oc::MatrixView<u8> out, u64 alignment);

        oc::MatrixView<u8> getInputView(u64 i);
        oc::MatrixView<u8> getOutputView(u64 i);
        oc::MatrixView<u8> getMemView(BetaBundle& wires);

        SilentTripleGen mSilent;
        //IknpTripleGen mIknp;

        u64 numRounds()
        {
            return mNumRounds;
        }

        Proto roundFunction(coproto::Socket& chl);


        Proto multSendP1(span<block> x, coproto::Socket& chl, oc::GateType gt);
        Proto multSendP2(span<block> x, coproto::Socket& chl, oc::GateType gt);


        Proto multRecvP1(span<block> x, span<block> z, coproto::Socket& chl, oc::GateType gt);
        Proto multRecvP2(span<block> x,  span<block> z, coproto::Socket& chl);


        Proto multSend(span<block> x, span<block> y, coproto::Socket& chl, oc::GateType gt)
        {
            if (mIdx == 0)
                return multSendP1(x, y, chl, gt);
            else
                return multSendP2(x, y, chl);
        }
        Proto multSendP1(span<block> x, span<block> y, coproto::Socket& chl, oc::GateType gt);
        Proto multSendP2(span<block> x, span<block> y, coproto::Socket& chl);


        Proto multRecv(span<block> x, span<block> y, span<block> z, coproto::Socket& chl, oc::GateType gt)
        {
            if (mIdx == 0)
                return multRecvP1(x, y, z, chl, gt);
            else
                return multRecvP2(x, y, z, chl);
        }

        Proto multRecvP1(span<block> x, span<block> y, span<block> z, coproto::Socket& chl, oc::GateType gt);
        Proto multRecvP2(span<block> x, span<block> y, span<block> z, coproto::Socket& chl);
    };
}
#endif