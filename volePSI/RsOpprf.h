#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "volePSI/Defines.h"
#include "volePSI/config.h"
#ifdef VOLE_PSI_ENABLE_OPPRF

#include "volePSI/RsOprf.h"

namespace volePSI
{
	template<typename T>
	struct SharedContainer
	{
		std::shared_ptr<T> mBuffer;

		using value_type = typename T::value_type;
		using size_type = typename T::size_type;

		//SharedContainer() = default;
		SharedContainer(const SharedContainer&) = default;
		SharedContainer(SharedContainer&&) = default;
		SharedContainer& operator=(SharedContainer&&) = default;
		SharedContainer& operator=(const SharedContainer&) = default;

		template<typename... Args>
		SharedContainer(Args... args)
			:mBuffer(std::make_shared<T>(std::forward<Args>(args)...))
		{ }

		void resize(u64 s) { mBuffer->resize(s); }
		auto size() const { return mBuffer->size(); }
		auto data() { return mBuffer->data(); }
		auto data() const { return mBuffer->data(); }

		auto begin() { return mBuffer->begin(); }
		auto end() { return mBuffer->end(); }
		auto begin() const { return mBuffer->begin(); }
		auto end() const { return mBuffer->end(); }

		operator T&() { return *mBuffer; }
		operator const T&() const { return *mBuffer; }
	};


	template<typename T>
	struct BasicVector : public span<T>
	{
		BasicVector() = default;
		BasicVector(BasicVector&& o) { asSpan() = o.asSpan(); o.asSpan() = {}; };
		BasicVector& operator=(BasicVector&& o) { clear();  asSpan() = o.asSpan(); o.asSpan() = {}; return *this; };

		span<T>& asSpan() { return static_cast<span<T>&>(*this); }
		operator span<T>& () { return asSpan(); }

		BasicVector(u64 size)
		{
			resize(size);
		}

		auto clear()
		{
			if (asSpan().data())
				delete[] asSpan().data();
		}

		~BasicVector()
		{
			clear();
		}

		void resize(u64 size)
		{
			clear();
			asSpan() = span<T>(new T[size], size);
		}



	};



	class RsOpprfSender : public oc::TimerAdapter
	{
	public:
		RsOprfSender mOprfSender;
		void setMultType(oc::MultType type) { mOprfSender.setMultType(type); };

		//struct PP
		//{
		//	u8* mData = nullptr;
		//	u64 mRows = 0;
		//	u64 mCols = 0;

		//	~PP()
		//	{
		//		delete[] mData;
		//	}

		//	PP(u64 n, u64 m)
		//		: mData(new u8[n * m])
		//		, mRows(n)
		//		, mCols(m)
		//	{ }
		//};

		//std::shared_ptr<PP> mPBacking;
		SharedContainer<BasicVector<u8>> mP;

		u64 mPaxosByteWidth = 0;
		Baxos mPaxos;

		Proto send(u64 recverSize, span<const block> X, span<block> val, PRNG& prng, u64 numThreads, Socket& chl)
		{
			return send(recverSize, X, MatrixView<u8>((u8*)val.data(), val.size(), sizeof(block)), prng, numThreads, chl);
		}

		template<typename ValueType>
		Proto send(u64 recverSize, span<const block> X, MatrixView<ValueType> val, PRNG& prng, u64 numThreads, Socket& chl)
		{
			return send(recverSize, X, MatrixView<u8>((u8*)val.data(), val.rows(), val.cols() * sizeof(ValueType)), prng, numThreads, chl);
		}

		template<typename ValueType>
		Proto send(u64 recverSize, span<const block> X, span<ValueType> val, PRNG& prng, u64 numThreads, Socket& chl)
		{
			return send(recverSize, X, MatrixView<u8>((u8*)val.data(), val.size(), sizeof(ValueType)), prng, numThreads, chl);
		}


		template<typename T>
		T eval(block v)
		{
			T r;
			MatrixView<T> mm(&r, 1, 1);
			eval<T>({ &v, 1 }, mm, 1);
			return r;
			//return mOprfSender.eval(v) ^ mPaxos.decode<T>(v, mP);
		}

		void eval(span<const block> val, span<block> output, u64 numThreads)
		{
			eval(val, MatrixView<u8>((u8*)output.data(), output.size(), sizeof(block)), numThreads);
		}


		template<typename ValueType>
		void eval(span<const block> val, MatrixView<ValueType> output, u64 numThreads)
		{
			eval(val, MatrixView<u8>((u8*)output.data(), output.rows(), output.cols() * sizeof(ValueType)), numThreads);
		}


		void oprfEval(span<const block> val, span<u8> out, span<const u8> add, u64 stride, u64 numThreads);
		Proto send(u64 recverSize, span<const block> X, MatrixView<u8> val, PRNG& prng, u64 numThreads, Socket& chl);
		void eval(span<const block> val, MatrixView<u8> output, u64 numThreads);
	};



	class RsOpprfReceiver : public oc::TimerAdapter
	{

	public:
		RsOprfReceiver mOprfReceiver;
		void setMultType(oc::MultType type) { mOprfReceiver.setMultType(type); };

		Proto receive(u64 senderSize, span<const block> values, span<block> outputs, PRNG& prng, u64 numThreads, Socket& chl)
		{
			return receive(senderSize, values, MatrixView<u8>((u8*)outputs.data(), outputs.size(), sizeof(block)), prng, numThreads, chl);
		}

		template<typename ValueType>
		Proto receive(u64 senderSize, span<const block> values, MatrixView<ValueType> outputs, PRNG& prng, u64 numThreads, Socket& chl)
		{
			return receive(senderSize, values, MatrixView<u8>((u8*)outputs.data(), outputs.rows(), outputs.cols() * sizeof(ValueType)), prng, numThreads, chl);
		}

		template<typename ValueType>
		Proto receive(u64 senderSize, span<const block> values, span<ValueType> outputs, PRNG& prng, u64 numThreads, Socket& chl)
		{
			return receive(senderSize, values, MatrixView<u8>((u8*)outputs.data(), outputs.size(), sizeof(ValueType)), prng, numThreads, chl);
		}


		Proto receive(u64 senderSize, span<const block> values, MatrixView<u8> outputs, PRNG& prng, u64 numThreads, Socket& chl);

	};

}
#endif
