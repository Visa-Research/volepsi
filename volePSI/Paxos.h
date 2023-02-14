#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <cassert>
#include <vector>
#include <algorithm>
#include <ostream>
#include <memory>
#include <numeric>
#include <iomanip>
#include <cmath>

#include "volePSI/Defines.h"

#include "cryptoTools/Common/Log.h"
#include "cryptoTools/Common/Timer.h"
#include "cryptoTools/Crypto/AES.h"
#include "cryptoTools/Crypto/PRNG.h"
#include "cryptoTools/Crypto/RandomOracle.h"
#include "libOTe/Tools/LDPC/Mtx.h"
#include "volePSI/PxUtil.h"

namespace volePSI
{
	struct PaxosParam {

		// the type of dense columns.
		enum DenseType
		{
			Binary,
			GF128
		};

		u64 mSparseSize = 0,
			mDenseSize = 0,
			mWeight = 0,
			mG = 0,
			mSsp = 40;
		DenseType mDt = GF128;

		PaxosParam() = default;
		PaxosParam(const PaxosParam&) = default;
		PaxosParam& operator=(const PaxosParam&) = default;

		PaxosParam(u64 numItems, u64 weight = 3, u64 ssp = 40, DenseType dt = DenseType::GF128)
		{
			init(numItems, weight, ssp, dt);
		}

		// computes the paxos parameters based the parameters.
		void init(u64 numItems, u64 weight = 3, u64 ssp = 40, DenseType dt = DenseType::GF128);

		// the size of the paxos data structure.
		u64 size() const
		{
			return mSparseSize + mDenseSize;
		}
	};


	// The core Paxos algorithm. The template parameter
	// IdxType should be in {u8,u16,u32,u64} and large
	// enough to fit the paxos size value.
	template<typename IdxType>
	class Paxos : public PaxosParam, public oc::TimerAdapter
	{
	public:

		// the number of items to be encoded.
		IdxType mNumItems = 0;

		// the encoding/decoding seed.
		block mSeed;

		bool mVerbose = false;
		bool mDebug = false;

		// when decoding, add the decoded value to the 
		// output, as opposed to overwriting.
		bool mAddToDecode = false;

		// the method for generating the row data based on the input value.
		PaxosHash<IdxType> mHasher;

		// an allocate used for the encoding algorithm
		std::unique_ptr<u8[]> mAllocation;
		u64 mAllocationSize = 0;

		// The dense part of the paxos matrix
		span<block> mDense;

		// The sparse part of the paxos matrix
		MatrixView<IdxType> mRows;

		// the sparse columns of the matrix
		span<span<IdxType>> mCols;

		// the memory used to store the column data.
		span<IdxType> mColBacking;

		// A data structure used to track the current weight of the rows.s
		WeightData<IdxType> mWeightSets;

		Paxos() = default;
		Paxos(const Paxos&) = default;
		Paxos(Paxos&&) = default;
		Paxos& operator=(const Paxos&) = default;
		Paxos& operator=(Paxos&&) = default;


		// initialize the paxos with the given parameters.
		void init(u64 numItems, u64 weight, u64 ssp, PaxosParam::DenseType dt, block seed)
		{
			PaxosParam p(numItems, weight, ssp, dt);
			init(numItems, p, seed);
		}

		// initialize the paxos with the given parameters.
		void init(u64 numItems, PaxosParam p, block seed);

		// solve/encode the given inputs,value pair. The paxos data 
		// structure is written to output. input,value should be numItems 
		// in size, output should be Paxos::size() in size. If the paxos
		// should be randomized, then provide a PRNG.
		template<typename ValueType>
		void solve(span<const block> inputs, span<const ValueType> values, span<ValueType> output, oc::PRNG* prng = nullptr)
		{
			setInput(inputs);
			encode<ValueType>(values, output, prng);
		}

		// solve/encode the given inputs,value pair. The paxos data 
		// structure is written to output. input,value should have numItems 
		// rows, output should have Paxos::size() rows. All should have the 
		// same number of columns. If the paxos should be randomized, then 
		// provide a PRNG.
		template<typename ValueType>
		void solve(span<const block> inputs, MatrixView<const ValueType> values, MatrixView<ValueType> output, oc::PRNG* prng = nullptr)
		{
			setInput(inputs);
			encode<ValueType>(values, output, prng);

			if(mDebug)
				check(inputs, values, output);
		}

		// set the input keys which define the paxos matrix. After that,
		// encode can be called more than once.
		void setInput(span<const block> inputs);

		// encode the given inputs,value pair based on the already set input. The paxos data 
		// structure is written to output. input,value should be numItems 
		// in size, output should be Paxos::size() in size. If the paxos
		// should be randomized, then provide a PRNG.
		template<typename ValueType>
		void encode(span<const ValueType> values, span<ValueType> output, oc::PRNG* prng = nullptr)
		{
			PxVector<const ValueType> V(values);
			PxVector<ValueType> P(output);
			auto h = P.defaultHelper();
			encode(V, P, h, prng);
		}

		// encode the given inputs,value pair based on the already set input. The paxos data 
		// structure is written to output. input,value should have numItems 
		// rows, output should have Paxos::size() rows. All should have the 
		// same number of columns. If the paxos should be randomized, then 
		// provide a PRNG.
		template<typename ValueType>
		void encode(MatrixView<const ValueType> values, MatrixView<ValueType> output, oc::PRNG* prng = nullptr)
		{
			if (values.cols() != output.cols())
				throw RTE_LOC;

			if (values.cols() == 1)
			{
				// reduce matrix to span if possible.
				encode(span<const ValueType>(values), span<ValueType>(output), prng);
			}
			else if (
				values.cols() * sizeof(ValueType) % sizeof(block) == 0 && 
				std::is_same<ValueType, block>::value == false)
			{
				// reduce ValueType to block if possible.

				auto n = values.rows();
				auto m = values.cols() * sizeof(ValueType) / sizeof(block);

				encode<block>(
					MatrixView<const block>((block*)values.data(), n, m), 
					MatrixView<block>((block*)output.data(), n, m),
					prng);
			}
			else
			{
				PxMatrix<const ValueType> V(values);
				PxMatrix<ValueType> P(output);
				auto h = P.defaultHelper();
				encode(V, P, h, prng);
			}
		}

		// encode the given input with the given paxos p. Vec and ConstVec should
		// meet the PxVector concept... Helper used to perform operations on values.
		template<typename Vec, typename ConstVec, typename Helper>
		void encode(ConstVec& values, Vec& output, Helper& h, oc::PRNG* prng = nullptr);

		// Decode the given input based on the data paxos structure p. The
		// output is written to values.
		template<typename ValueType>
		void decode(span<const block> input, span<ValueType> values, span<const ValueType> p);

		// Decode the given input based on the data paxos structure p. The
		// output is written to values. values and p should have the same 
		// number of columns.
		template<typename ValueType>
		void decode(span<const block> input, MatrixView<ValueType> values, MatrixView<const ValueType> p);


		// decode the given input with the given paxos p. Vec and ConstVec should
		// meet the PxVector concept... Helper used to perform operations on values.
		template<typename Helper, typename Vec, typename ConstVec>
		void decode(span<const block> input, Vec& values, ConstVec& p, Helper& h);


		struct Triangulization
		{
			PaxosPermutation<IdxType> mPerm;
			u64 mGap = 0;
			oc::SparseMtx mH;


			oc::SparseMtx getA() const;
			oc::SparseMtx getC() const;
			oc::SparseMtx getB() const;
			oc::SparseMtx getD() const;
			oc::SparseMtx getE() const;
			oc::SparseMtx getF() const;
		};

		// returns a printable version of the paxos matrix after
		// beign triangulized. setInput(...) should be called first.
		Triangulization getTriangulization();


		////////////////////////////////////////
		// private functions
		////////////////////////////////////////


		// allocate the memory needed to triangulate.
		void allocate();

		// decodes 32 instances. rows should contain the row indicies, dense the dense 
		// part. values is where the values are written to. p is the Paxos, h is the value op. helper.
		template<typename ValueType, typename Helper, typename Vec>
		void decode32(const IdxType* rows, const block* dense, ValueType* values, Vec& p, Helper& h);

		// decodes 8 instances. rows should contain the row indicies, dense the dense 
		// part. values is where the values are written to. p is the Paxos, h is the value op. helper.
		template<typename ValueType, typename Helper, typename Vec>
		void decode8(const IdxType* rows, const block* dense, ValueType* values, Vec& p, Helper& h);


		// decodes one instances. rows should contain the row indicies, dense the dense 
		// part. values is where the values are written to. p is the Paxos, h is the value op. helper.
		template<typename ValueType, typename Helper, typename Vec>
		void decode1(
			const IdxType* rows,
			const block* dense,
			ValueType* values,
			Vec& p,
			Helper& h);

		// manually set the row indicies and the dense values.
		void setInput(MatrixView<IdxType> rows, span<block> dense);

		// manually set the row indicies and the dense values. In 
		// addition, provide the memory that us needed to to perform
		// encoding.
		void setInput(
			MatrixView<IdxType> rows,
			span<block> dense,
			span<span<IdxType>> cols,
			span<IdxType> colBacking,
			span<IdxType> colWeights);

		// perform the tringulization algorithm for encoding. This
		// populates mainRows,mainCols with the rows/columns of C
		// gapRows are all the rows that are in the gap.
		void triangulate(
			std::vector<IdxType>& mainRows,
			std::vector<IdxType>& mainCols,
			std::vector<std::array<IdxType, 2>>& gapRows);

		// once triangulated, this is used to assign values 
		// to output (paxos).
		template<typename Vec, typename ConstVec, typename Helper>
		void backfill(
			span<IdxType> mainRows,
			span<IdxType> mainCols,
			span<std::array<IdxType, 2>> gapRows,
			ConstVec& values,
			Vec& output,
			Helper& h,
			oc::PRNG* prng);

		// once triangulated, this is used to assign values 
		// to output (paxos). Use the gf128 dense algorithm.
		template<typename Vec, typename ConstVec, typename Helper>
		void backfillGf128(
			span<IdxType> mainRows,
			span<IdxType> mainCols,
			span<std::array<IdxType, 2>> gapRows,
			ConstVec& values,
			Vec& output,
			Helper& h,
			oc::PRNG* prng);

		// once triangulated, this is used to assign values 
		// to output (paxos). Use the classic binary dense algorithm.
		template<typename Vec, typename ConstVec, typename Helper>
		void backfillBinary(
			span<IdxType> mainRows,
			span<IdxType> mainCols,
			span<std::array<IdxType, 2>> gapRows,
			ConstVec& values,
			Vec& output,
			Helper&h,
			oc::PRNG* prng);

		// helper function used for getTriangulization();
		std::pair<PaxosPermutation<IdxType>, u64> computePermutation(
			span<IdxType> mainRows,
			span<IdxType> mainCols,
			span<std::array<IdxType, 2>> gapRows,
			bool withDense);
		
		// helper function used for getTriangulization(); returns
		// the rows/columns permuted by perm.
		oc::SparseMtx getH(PaxosPermutation<IdxType>& perm) const;

		// helper function that generates the column data given that 
		// the row data has been populated (via setInput(...)).
		void rebuildColumns(span<IdxType> colWeights, u64 totalWeight);

		// A sparse representation of the F * C^-1 matrix.
		struct FCInv
		{
			FCInv(u64 n)
				: mMtx(n)
			{}
			std::vector<std::vector<IdxType>> mMtx;
		};

		// returns the sparse representation of the F * C^-1 matrix.
		FCInv getFCInv(
			span<IdxType> mainRows,
			span<IdxType> mainCols,
			span<std::array<IdxType, 2>> gapRows) const;

		// returns which columns are used for the gap. This
		// is only used for binary dense method.
		std::vector<u64> getGapCols(
			FCInv& fcinv,
			span<std::array<IdxType, 2>> gapRows) const;

		// returns x2' = x2 - D' r - FC^-1 x1
		template<typename Vec, typename ConstVec, typename Helper>
		Vec getX2Prime(
			FCInv &fcinv,
			span<std::array<IdxType, 2>> gapRows, 
			span<u64> gapCols,
			const ConstVec& X,
			const Vec& P,
			Helper& h);

		// returns E' = -FC^-1B + E
		oc::DenseMtx getEPrime(
			FCInv &fcinv,
			span<std::array<IdxType, 2>> gapRows,
			span<u64> gapCols);

		template<typename Vec, typename Helper>
		void randomizeDenseCols(Vec&, Helper&, span<u64> gapCols, oc::PRNG* prng);



		template<typename ValueType>
		void check(span<const block> inputs, MatrixView<const ValueType> values, MatrixView<const ValueType> output)
		{
			oc::Matrix<ValueType> v2(values.rows(), values.cols());
			decode<ValueType>(inputs, v2, output);

			for (u64 i = 0; i < values.rows(); ++i)
			{
				for(u64 j =0; j < values.cols(); ++j)
					if (v2(i, j) != values(i, j))
					{
						std::cout << "paxos failed to encode. \n"
							<< "inputs["<< i << "]" << inputs[i] << "\n"
							<< "seed " << mSeed  <<"\n"
							<< "n " << size() << "\n"
							<< "m " << inputs.size() << "\n"
							<< std::endl;
						throw RTE_LOC;
					}
			}
		}


	};

	// a binned version of paxos. Internally calls paxos.
	class Baxos
	{
	public:
		u64 mNumItems = 0, mNumBins = 0, mItemsPerBin = 0, mWeight = 0, mSsp = 0;

		// the parameters used on a single bim.
		PaxosParam mPaxosParam;
		block mSeed;

		bool mDebug = false;

		// when decoding, add the decoded value to the 
		// output, as opposed to overwriting.
		bool mAddToDecode = false;

		// initialize the paxos with the given parameter.
		void init(u64 numItems, u64 binSize, u64 weight, u64 ssp, PaxosParam::DenseType dt, block seed)
		{
			mNumItems = numItems;
			mWeight = weight;
			mNumBins = (numItems + binSize - 1) / binSize;
			mItemsPerBin = getBinSize(mNumBins, mNumItems, ssp + std::log2(mNumBins));
			mSsp = ssp;
			mSeed = seed;
			mPaxosParam.init(mItemsPerBin, weight, ssp, dt);
		}

		// solve the system for the given input vectors.
		// inputs are the keys
		// values are the desired values that inputs should decode to.
		// output is the paxos.
		// prng should be non-null if randomized paxos is desired.
		template<typename ValueType>
		void solve(
			span<const block> inputs,
			span<const ValueType> values,
			span<ValueType> output,
			oc::PRNG* prng = nullptr,
			u64 numThreads = 0);


		// solve the system for the given input matrices.
		// inputs are the keys
		// values are the desired values that inputs should decode to.
		// output is the paxos.
		// prng should be non-null if randomized paxos is desired.
		template<typename ValueType>
		void solve(
			span<const block> inputs,
			MatrixView<const ValueType> values,
			MatrixView<ValueType> output,
			oc::PRNG* prng = nullptr,
			u64 numThreads = 0);


		// solve/encode the system.
		template<typename Vec, typename ConstVec, typename Helper>
		void solve(
			span<const block> inputs,
			ConstVec& values,
			Vec& output,
			oc::PRNG* prng,
			u64 numThreads,
			Helper& h);


		// decode a single input given the paxos p.
		template<typename ValueType>
		ValueType decode(const block& input, span<const ValueType> p)
		{
			ValueType r;
			decode(span<const block>(&input, 1), span<ValueType>(&r, 1), p);
			return r;
		}

		// decode the given input vector and write the result to values.
		// inputs are the keys.
		// values are the output.
		// p is the paxos vector.
		template<typename ValueType>
		void decode(span<const block> input, span<ValueType> values, span<const ValueType> p, u64 numThreads = 0);


		// decode the given input matrix and write the result to values.
		// inputs are the keys.
		// values are the output.
		// p is the paxos matrix.
		template<typename ValueType>
		void decode(span<const block> input, MatrixView<ValueType> values, MatrixView<const ValueType> p, u64 numThreads = 0);


		template<typename Vec, typename ConstVec, typename Helper>
		void decode(
			span<const block> inputs,
			Vec& values,
			ConstVec& p,
			Helper& h,
			u64 numThreads);


		//////////////////////////////////////////
		// private impl
		//////////////////////////////////////////



		// solve/encode the system.
		template<typename IdxType, typename Vec, typename ConstVec, typename Helper>
		void implParSolve(
			span<const block> inputs,
			ConstVec& values,
			Vec& output,
			oc::PRNG* prng,
			u64 numThreads,
			Helper& h);

		// create the desired number of threads and split up the work.
		template<typename IdxType, typename Vec, typename ConstVec, typename Helper>
		void implParDecode(
			span<const block> inputs,
			Vec& values,
			ConstVec& p,
			Helper& h,
			u64 numThreads);


		// decode the given inputs based on the paxos p. The output is written to values.
		template<typename IdxType, typename Vec, typename ConstVec, typename Helper>
		void implDecodeBatch(span<const block> inputs, Vec& values, ConstVec& p, Helper& h);

		// decode the given inputs based on the paxos p. The output is written to values.
		// this differs from implDecode in that all inputs must be for the same paxos bin.
		template<typename IdxType, typename Vec, typename ConstVec, typename Helper>
		void implDecodeBin(
			u64 binIdx,
			span<block> hashes,
			Vec& values,
			Vec& valuesBuff,
			span<u64> inIdxs,
			ConstVec& p,
			Helper& h,
			Paxos<IdxType>& paxos);

		// the size of the paxos.
		u64 size()
		{
			return u64(mNumBins * (mPaxosParam.mSparseSize + mPaxosParam.mDenseSize));
		}

		static u64 getBinSize(u64 numBins, u64 numItems, u64 ssp);

		u64 binIdxCompress(const block& h)
		{
			return (h.get<u64>(0) ^ h.get<u64>(1) ^ h.get<u32>(3));
		}

		u64 modNumBins(const block& h)
		{
			return binIdxCompress(h) % mNumBins;
		}


		template<typename Vec, typename Vec2>
		void check(span<const block> inputs, Vec values, Vec2 output)
		{
			auto h = values.defaultHelper();
			auto v2 = h.newVec(values.size());
			decode(inputs, v2, output, h, 1);

			for (u64 i = 0; i < values.size(); ++i)
			{
				if (h.eq(v2[i],values[i]) == false)
				{
					std::cout << "paxos failed to encode. \n"
						<< "inputs[" << i << "]" << inputs[i] << "\n"
						<< "seed " << mSeed << "\n"
						<< "n " << size() << "\n"
						<< "m " << inputs.size() << "\n"
						<< std::endl;
					throw RTE_LOC;
				}
			}
		}

	};

	// invert a gf128 matrix.
	Matrix<block> gf128Inv(Matrix<block> mtx);

	// multiply two gf128 matricies.
	Matrix<block> gf128Mul(const Matrix<block>& m0, const Matrix<block>& m1);

	template<typename IdxType>
	std::ostream& operator<<(std::ostream& o, const Paxos<IdxType>& p);
	//template<typename IdxType>
	//std::ostream& operator<<(std::ostream& o, const PaxosDiff<IdxType>& s);

}




// Since paxos is a template, we include the impl file.
#ifndef NO_INCLUDE_PAXOS_IMPL
#include "PaxosImpl.h"
#endif // !NO_INCLUDE_PAXOS_IMPL
