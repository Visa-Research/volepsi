#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "cryptoTools/Common/Defines.h"
#include "cryptoTools/Common/Matrix.h"
#include "cryptoTools/Common/BitVector.h"
#include "cryptoTools/Crypto/PRNG.h"
#include "coproto/coproto.h"

namespace volePSI
{

	using u64 = oc::u64;
	using u32 = oc::u32;
	using u16 = oc::u16;
	using u8 = oc::u8;

	using i64 = oc::i64;
	using i32 = oc::i32;
	using i16 = oc::i16;
	using i8 = oc::i8;

	using block  = oc::block;

	template<typename T>
	using span = oc::span<T>;
	template<typename T>
	using Matrix = oc::Matrix<T>;
	template<typename T>
	using MatrixView = oc::MatrixView<T>;

	enum Mode {
		Sender = 1,
		Receiver = 2
		//Dual = 3
	};

	struct RequiredBase
	{
		u64 mNumSend;
		oc::BitVector mRecvChoiceBits;
	};


	using PRNG = oc::PRNG;
	using Socket = coproto::Socket;
	using Proto = coproto::task<void>;

}



