
#define NO_INCLUDE_PAXOS_IMPL
#include "Paxos.h"
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include <unordered_set>
#include <numeric>
#include "libOTe/Tools/LDPC/Util.h"
#include "volePSI/SimpleIndex.h"
#include <future>

namespace volePSI
{

	using AES = oc::AES;
	using PRNG = oc::PRNG;
	using SparseMtx = oc::SparseMtx;
	using DenseMtx = oc::DenseMtx;
	using BitIterator = oc::BitIterator;
	using PointList = oc::PointList;
	//#define POLY_DENSE


	inline block gf128Inv(block x)
	{

		/* calculate el^{-1} as el^{2^{128}-2}. the addition chain below
		   requires 142 mul/sqr operations total. */
		   //gf128 gf128::inverse() const {
		block a = x;

		block result = oc::ZeroBlock;
		for (int64_t i = 0; i <= 6; ++i) {
			/* entering the loop a = el^{2^{2^i}-1} */
			block b = a;
			for (int64_t j = 0; j < (1 << i); ++j) {
				b = b.gf128Mul(b);
			}
			/* after the loop b = a^{2^i} = el^{2^{2^i}*(2^{2^i}-1)} */
			a = a.gf128Mul(b);
			/* now a = el^{2^{2^{i+1}}-1} */

			if (i == 0) {
				result = b;
			}
			else {
				result = result.gf128Mul(b);
			}
		}

		assert(result.gf128Mul(x) == oc::OneBlock);

		/* now result = el^{2^128-2} */
		return result;
	}

	inline Matrix<block> gf128Inv(Matrix<block> mtx)
	{
		assert(mtx.rows() == mtx.cols());

		auto n = mtx.rows();

		auto Inv = Matrix<block>(n, n);
		for (u64 i = 0; i < n; ++i)
			Inv(i, i) = oc::OneBlock;

		// for the ith row
		for (u64 i = 0; i < n; ++i)
		{
			// if the i,i position is zero,
			// then find a lower row to swap with.
			if (mtx(i, i) == oc::ZeroBlock)
			{
				// for all lower row j, check if (j,i)
				// is non-zero
				for (u64 j = i + 1; j < n; ++j)
				{
					if (mtx(j, i) == oc::OneBlock)
					{
						// found one, swap the rows and break
						for (u64 k = 0; k < n; ++k)
						{
							std::swap(mtx(i, k), mtx(j, k));
							std::swap(Inv(i, k), Inv(j, k));
						}
						break;
					}
				}

				// double check that we found a swap. If not,
				// then this matrix is not invertable. Return
				// the empty matrix to denote this.
				if (mtx(i, i) == oc::ZeroBlock)
					return {};
			}


			// Make the i'th column the zero column with 
			// a one on the diagonal. First we will make row 
			// i have a one on the diagonal by computing
			//  row(i) = row(i) / entry(i,i)
			auto mtx_ii_inv = gf128Inv(mtx(i, i));
			for (u64 j = 0; j < n; ++j)
			{
				mtx(i, j) = mtx(i, j).gf128Mul(mtx_ii_inv);
				Inv(i, j) = Inv(i, j).gf128Mul(mtx_ii_inv);
			}

			// next we will make all other rows have a zero
			// in the i'th column by computing
			//  row(j) = row(j) - row(i) * entry(j,i)
			for (u64 j = 0; j < n; ++j)
			{
				if (j != i)
				{
					auto mtx_ji = mtx(j, i);
					for (u64 k = 0; k < n; ++k)
					{
						mtx(j, k) = mtx(j, k) ^ mtx(i, k).gf128Mul(mtx_ji);
						Inv(j, k) = Inv(j, k) ^ Inv(i, k).gf128Mul(mtx_ji);
					}
				}
			}
		}

		return Inv;
	}

	inline Matrix<block> gf128Mul(const Matrix<block>& m0, const Matrix<block>& m1)
	{
		assert(m0.cols() == m1.rows());

		Matrix<block> ret(m0.rows(), m1.cols());

		for (u64 i = 0; i < ret.rows(); ++i)
		{
			for (u64 j = 0; j < ret.cols(); ++j)
			{
				auto& v = ret(i, j);
				for (u64 k = 0; k < m0.cols(); ++k)
				{
					v = v ^ m0(i, k).gf128Mul(m1(k, j));
				}
			}
		}
		return ret;
	}


	// See paper https://eprint.iacr.org/2022/320.pdf
	inline void  PaxosParam::init(u64 numItems, u64 weight, u64 ssp, DenseType dt)
	{
		if (weight < 2)
			throw std::runtime_error("weight must be 2 or greater.");

		mWeight = weight;
		mSsp = ssp;
		mDt = dt;

		double logN = std::log2(numItems);

		if (weight == 2)
		{
			double a = 7.529;
			double b = 0.61;
			double c = 2.556;
			double lambdaVsGap = a / (logN - c) + b;

			// lambda = lambdaVsGap * g - 1.9 * lambdaVsGap
			// g = (lambda + 1.9 * lambdaVsGap) /  lambdaVsGap
			//   = lambda / lambdaVsGap + 1.9

			mG = static_cast<u64>(std::ceil(ssp / lambdaVsGap + 1.9));

			mDenseSize = mG + (dt == DenseType::Binary) * ssp;
			mSparseSize = 2 * numItems;
		}
		else
		{
			double ee = 0;
			if (weight == 3) ee = 1.223;
			if (weight == 4) ee = 1.293;
			if (weight >= 5) ee = 0.1485 * weight + 0.6845;


			double logW = std::log2(weight);
			double logLambdaVsE = 0.555 * logN + 0.093 * std::pow(logW, 3) - 1.01 * std::pow(logW, 2) + 2.925 * logW - 0.133;
			double lambdaVsE = std::pow(2, logLambdaVsE);

			// -9.2 = lambdaVsE * ee + b
			// b = -9.2 - lambdaVsE * ee

			double b = -9.2 - lambdaVsE * ee;

			// lambda = lambdaVsE * e + b
			// e = (lambda - b) / lambdaVsE

			double e = (ssp - b) / lambdaVsE;
			mG = std::floor(ssp / ((weight - 2) * std::log2(e * numItems)));

			mDenseSize = mG + (dt == DenseType::Binary) * ssp;
			mSparseSize = numItems * e;
		}
	}

#ifdef ENABLE_SSE
	using block256 = __m256i;
	inline block256 my_libdivide_u64_do_vec256(const block256& x, const libdivide::libdivide_u64_t* divider)
	{
		return libdivide::libdivide_u64_do_vec256(x, divider);
	}
#else
	using block256 = std::array<block, 2>;

	inline block256 _mm256_loadu_si256(block256* p) { return *p; }

	inline block256 my_libdivide_u64_do_vec256(const block256& x, const libdivide::libdivide_u64_t* divider)
	{
		block256 y;
		auto x64 = (u64*)&x;
		auto y64 = (u64*)&y;
		for (u64 i = 0; i < 4; ++i)
		{
			y64[i] = libdivide::libdivide_u64_do(x64[i], divider);
		}

		return y;
	}
#endif

	inline void doMod32(u64* vals, const libdivide::libdivide_u64_t* divider, const u64& modVal)
	{



		//std::array<u64, 4> temp64;
		//for (u64 i = 0; i < 32; i += 16)
		{
			u64 i = 0;
			block256 row256a = _mm256_loadu_si256((block256*)&vals[i]);
			block256 row256b = _mm256_loadu_si256((block256*)&vals[i + 4]);
			block256 row256c = _mm256_loadu_si256((block256*)&vals[i + 8]);
			block256 row256d = _mm256_loadu_si256((block256*)&vals[i + 12]);
			block256 row256e = _mm256_loadu_si256((block256*)&vals[i + 16]);
			block256 row256f = _mm256_loadu_si256((block256*)&vals[i + 20]);
			block256 row256g = _mm256_loadu_si256((block256*)&vals[i + 24]);
			block256 row256h = _mm256_loadu_si256((block256*)&vals[i + 28]);
			auto tempa = my_libdivide_u64_do_vec256(row256a, divider);
			auto tempb = my_libdivide_u64_do_vec256(row256b, divider);
			auto tempc = my_libdivide_u64_do_vec256(row256c, divider);
			auto tempd = my_libdivide_u64_do_vec256(row256d, divider);
			auto tempe = my_libdivide_u64_do_vec256(row256e, divider);
			auto tempf = my_libdivide_u64_do_vec256(row256f, divider);
			auto tempg = my_libdivide_u64_do_vec256(row256g, divider);
			auto temph = my_libdivide_u64_do_vec256(row256h, divider);
			//auto temp = libdivide::libdivide_u64_branchfree_do_vec256(row256, divider);
			auto temp64a = (u64*)&tempa;
			auto temp64b = (u64*)&tempb;
			auto temp64c = (u64*)&tempc;
			auto temp64d = (u64*)&tempd;
			auto temp64e = (u64*)&tempe;
			auto temp64f = (u64*)&tempf;
			auto temp64g = (u64*)&tempg;
			auto temp64h = (u64*)&temph;
			vals[i + 0] -= temp64a[0] * modVal;
			vals[i + 1] -= temp64a[1] * modVal;
			vals[i + 2] -= temp64a[2] * modVal;
			vals[i + 3] -= temp64a[3] * modVal;
			vals[i + 4] -= temp64b[0] * modVal;
			vals[i + 5] -= temp64b[1] * modVal;
			vals[i + 6] -= temp64b[2] * modVal;
			vals[i + 7] -= temp64b[3] * modVal;
			vals[i + 8] -= temp64c[0] * modVal;
			vals[i + 9] -= temp64c[1] * modVal;
			vals[i + 10] -= temp64c[2] * modVal;
			vals[i + 11] -= temp64c[3] * modVal;
			vals[i + 12] -= temp64d[0] * modVal;
			vals[i + 13] -= temp64d[1] * modVal;
			vals[i + 14] -= temp64d[2] * modVal;
			vals[i + 15] -= temp64d[3] * modVal;
			vals[i + 16] -= temp64e[0] * modVal;
			vals[i + 17] -= temp64e[1] * modVal;
			vals[i + 18] -= temp64e[2] * modVal;
			vals[i + 19] -= temp64e[3] * modVal;
			vals[i + 20] -= temp64f[0] * modVal;
			vals[i + 21] -= temp64f[1] * modVal;
			vals[i + 22] -= temp64f[2] * modVal;
			vals[i + 23] -= temp64f[3] * modVal;
			vals[i + 24] -= temp64g[0] * modVal;
			vals[i + 25] -= temp64g[1] * modVal;
			vals[i + 26] -= temp64g[2] * modVal;
			vals[i + 27] -= temp64g[3] * modVal;
			vals[i + 28] -= temp64h[0] * modVal;
			vals[i + 29] -= temp64h[1] * modVal;
			vals[i + 30] -= temp64h[2] * modVal;
			vals[i + 31] -= temp64h[3] * modVal;
		}
	}

	//inline void doMod32(u64* vals, const libdivide::libdivide_u64_branchfree_t* divider, const u64& modVal)
	//{
	//	//std::array<u64, 4> temp64;
	//	for (u64 i = 0; i < 32; i += 4)
	//	{
	//		__m256i row256 = _mm256_loadu_si256((__m256i*) & vals[i]);
	//		//auto temp = libdivide::libdivide_u64_do_vec256(row256, divider);
	//		auto temp = libdivide::libdivide_u64_branchfree_do_vec256(row256, divider);
	//		auto temp64 = (u64*)&temp;
	//		vals[i + 0] -= temp64[0] * modVal;
	//		vals[i + 1] -= temp64[1] * modVal;
	//		vals[i + 2] -= temp64[2] * modVal;
	//		vals[i + 3] -= temp64[3] * modVal;
	//	}
	//}

	template<typename IdxType>
	void PaxosHash<IdxType>::mod32(u64* vals, u64 modIdx) const
	{
		auto divider = &mMods[modIdx];
		auto modVal = mModVals[modIdx];
		doMod32(vals, divider, modVal);
	}

#ifndef ENABLE_SSE


	// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpgt_epi64&ig_expand=1038
	inline block _mm_cmpgt_epi64(const block& a, const block& b)
	{
		std::array<u64, 2> ret;
		ret[0] = a.get<u64>()[0] > b.get<u64>()[0] ? -1ull : 0ull;
		ret[1] = a.get<u64>()[1] > b.get<u64>()[1] ? -1ull : 0ull;

		//auto t = ::_mm_cmpgt_epi64(*(__m128i*) & a, *(__m128i*) & b);;
		//block ret2 = *(block*)&t;
		//assert(ret2 == ret);

		return ret;
	}

	// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpeq_epi64&ig_expand=1038,900
	inline  block _mm_cmpeq_epi64(const block& a, const block& b)
	{
		std::array<u64, 2> ret;
		ret[0] = a.get<u64>()[0] == b.get<u64>()[0] ? -1ull : 0ull;
		ret[1] = a.get<u64>()[1] == b.get<u64>()[1] ? -1ull : 0ull;

		//auto t = ::_mm_cmpeq_epi64(*(__m128i*) & a, *(__m128i*) & b);;
		//block ret2 = *(block*)&t;
		//assert(ret2 == ret);

		return ret;
	}

	// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sub_epi64&ig_expand=1038,900,6922
	inline block _mm_sub_epi64(const block& a, const block& b)
	{
		std::array<u64, 2> ret;
		ret[0] = a.get<u64>(0) - b.get<u64>(0);
		ret[1] = a.get<u64>(1) - b.get<u64>(1);

		//auto t = ::_mm_sub_epi64(*(__m128i*) & a, *(__m128i*) & b);;
		//block ret2 = *(block*)&t;
		//assert(ret2 == ret);

		return ret;
	}



#endif

	template<typename IdxType>
	void PaxosHash<IdxType>::buildRow32(const block* hash, IdxType* row) const
	{
		if (mWeight == 3 /* && mSparseSize < std::numeric_limits<u32>::max()*/)
		{
			const auto weight = 3;
			block row128_[3][16];

			for (u64 i = 0; i < weight; ++i)
			{
				auto ll = (u64*)row128_[i];

				for (u64 j = 0; j < 32; ++j)
				{
					memcpy(&ll[j], hash[j].data() + sizeof(u32) * i, sizeof(u64));
				}
				mod32(ll, i);
			}


			for (u64 i = 0; i < 2; ++i)
			{
				std::array<block, 8> mask, max, min;
				//auto& row128 = *(std::array<std::array<block, 16>, 3>*)(((block*)row128_) + 8 * i);

				std::array<block*, 3> row128{
					row128_[0] + i * 8,
					row128_[1] + i * 8,
					row128_[2] + i * 8 };

				//if (i)
				//{
				//	memcpy(row128[0], &row128[0][i * 8], sizeof(block) * 8);
				//	memcpy(row128[1], &row128[1][i * 8], sizeof(block) * 8);
				//	memcpy(row128[2], &row128[2][i * 8], sizeof(block) * 8);
				//}

				// mask = a > b ? -1 : 0;
				mask[0] = _mm_cmpgt_epi64(row128[0][0], row128[1][0]);
				mask[1] = _mm_cmpgt_epi64(row128[0][1], row128[1][1]);
				mask[2] = _mm_cmpgt_epi64(row128[0][2], row128[1][2]);
				mask[3] = _mm_cmpgt_epi64(row128[0][3], row128[1][3]);
				mask[4] = _mm_cmpgt_epi64(row128[0][4], row128[1][4]);
				mask[5] = _mm_cmpgt_epi64(row128[0][5], row128[1][5]);
				mask[6] = _mm_cmpgt_epi64(row128[0][6], row128[1][6]);
				mask[7] = _mm_cmpgt_epi64(row128[0][7], row128[1][7]);


				min[0] = row128[0][0] ^ row128[1][0];
				min[1] = row128[0][1] ^ row128[1][1];
				min[2] = row128[0][2] ^ row128[1][2];
				min[3] = row128[0][3] ^ row128[1][3];
				min[4] = row128[0][4] ^ row128[1][4];
				min[5] = row128[0][5] ^ row128[1][5];
				min[6] = row128[0][6] ^ row128[1][6];
				min[7] = row128[0][7] ^ row128[1][7];


				// max = max(a,b)
				max[0] = (min[0]) & mask[0];
				max[1] = (min[1]) & mask[1];
				max[2] = (min[2]) & mask[2];
				max[3] = (min[3]) & mask[3];
				max[4] = (min[4]) & mask[4];
				max[5] = (min[5]) & mask[5];
				max[6] = (min[6]) & mask[6];
				max[7] = (min[7]) & mask[7];
				max[0] = max[0] ^ row128[1][0];
				max[1] = max[1] ^ row128[1][1];
				max[2] = max[2] ^ row128[1][2];
				max[3] = max[3] ^ row128[1][3];
				max[4] = max[4] ^ row128[1][4];
				max[5] = max[5] ^ row128[1][5];
				max[6] = max[6] ^ row128[1][6];
				max[7] = max[7] ^ row128[1][7];

				// min = min(a,b)
				min[0] = min[0] ^ max[0];
				min[1] = min[1] ^ max[1];
				min[2] = min[2] ^ max[2];
				min[3] = min[3] ^ max[3];
				min[4] = min[4] ^ max[4];
				min[5] = min[5] ^ max[5];
				min[6] = min[6] ^ max[6];
				min[7] = min[7] ^ max[7];

				//if (max == b)
				//  ++b
				//  ++max
				mask[0] = _mm_cmpeq_epi64(max[0], row128[1][0]);
				mask[1] = _mm_cmpeq_epi64(max[1], row128[1][1]);
				mask[2] = _mm_cmpeq_epi64(max[2], row128[1][2]);
				mask[3] = _mm_cmpeq_epi64(max[3], row128[1][3]);
				mask[4] = _mm_cmpeq_epi64(max[4], row128[1][4]);
				mask[5] = _mm_cmpeq_epi64(max[5], row128[1][5]);
				mask[6] = _mm_cmpeq_epi64(max[6], row128[1][6]);
				mask[7] = _mm_cmpeq_epi64(max[7], row128[1][7]);
				row128[1][0] = _mm_sub_epi64(row128[1][0], mask[0]);
				row128[1][1] = _mm_sub_epi64(row128[1][1], mask[1]);
				row128[1][2] = _mm_sub_epi64(row128[1][2], mask[2]);
				row128[1][3] = _mm_sub_epi64(row128[1][3], mask[3]);
				row128[1][4] = _mm_sub_epi64(row128[1][4], mask[4]);
				row128[1][5] = _mm_sub_epi64(row128[1][5], mask[5]);
				row128[1][6] = _mm_sub_epi64(row128[1][6], mask[6]);
				row128[1][7] = _mm_sub_epi64(row128[1][7], mask[7]);
				max[0] = _mm_sub_epi64(max[0], mask[0]);
				max[1] = _mm_sub_epi64(max[1], mask[1]);
				max[2] = _mm_sub_epi64(max[2], mask[2]);
				max[3] = _mm_sub_epi64(max[3], mask[3]);
				max[4] = _mm_sub_epi64(max[4], mask[4]);
				max[5] = _mm_sub_epi64(max[5], mask[5]);
				max[6] = _mm_sub_epi64(max[6], mask[6]);
				max[7] = _mm_sub_epi64(max[7], mask[7]);

				// if (c >= min)
				//   ++c
				mask[0] = _mm_cmpgt_epi64(min[0], row128[2][0]);
				mask[1] = _mm_cmpgt_epi64(min[1], row128[2][1]);
				mask[2] = _mm_cmpgt_epi64(min[2], row128[2][2]);
				mask[3] = _mm_cmpgt_epi64(min[3], row128[2][3]);
				mask[4] = _mm_cmpgt_epi64(min[4], row128[2][4]);
				mask[5] = _mm_cmpgt_epi64(min[5], row128[2][5]);
				mask[6] = _mm_cmpgt_epi64(min[6], row128[2][6]);
				mask[7] = _mm_cmpgt_epi64(min[7], row128[2][7]);
				mask[0] = mask[0] ^ oc::AllOneBlock;
				mask[1] = mask[1] ^ oc::AllOneBlock;
				mask[2] = mask[2] ^ oc::AllOneBlock;
				mask[3] = mask[3] ^ oc::AllOneBlock;
				mask[4] = mask[4] ^ oc::AllOneBlock;
				mask[5] = mask[5] ^ oc::AllOneBlock;
				mask[6] = mask[6] ^ oc::AllOneBlock;
				mask[7] = mask[7] ^ oc::AllOneBlock;
				row128[2][0] = _mm_sub_epi64(row128[2][0], mask[0]);
				row128[2][1] = _mm_sub_epi64(row128[2][1], mask[1]);
				row128[2][2] = _mm_sub_epi64(row128[2][2], mask[2]);
				row128[2][3] = _mm_sub_epi64(row128[2][3], mask[3]);
				row128[2][4] = _mm_sub_epi64(row128[2][4], mask[4]);
				row128[2][5] = _mm_sub_epi64(row128[2][5], mask[5]);
				row128[2][6] = _mm_sub_epi64(row128[2][6], mask[6]);
				row128[2][7] = _mm_sub_epi64(row128[2][7], mask[7]);

				// if (c >= max)
				//   ++c
				mask[0] = _mm_cmpgt_epi64(max[0], row128[2][0]);
				mask[1] = _mm_cmpgt_epi64(max[1], row128[2][1]);
				mask[2] = _mm_cmpgt_epi64(max[2], row128[2][2]);
				mask[3] = _mm_cmpgt_epi64(max[3], row128[2][3]);
				mask[4] = _mm_cmpgt_epi64(max[4], row128[2][4]);
				mask[5] = _mm_cmpgt_epi64(max[5], row128[2][5]);
				mask[6] = _mm_cmpgt_epi64(max[6], row128[2][6]);
				mask[7] = _mm_cmpgt_epi64(max[7], row128[2][7]);
				mask[0] = mask[0] ^ oc::AllOneBlock;
				mask[1] = mask[1] ^ oc::AllOneBlock;
				mask[2] = mask[2] ^ oc::AllOneBlock;
				mask[3] = mask[3] ^ oc::AllOneBlock;
				mask[4] = mask[4] ^ oc::AllOneBlock;
				mask[5] = mask[5] ^ oc::AllOneBlock;
				mask[6] = mask[6] ^ oc::AllOneBlock;
				mask[7] = mask[7] ^ oc::AllOneBlock;
				row128[2][0] = _mm_sub_epi64(row128[2][0], mask[0]);
				row128[2][1] = _mm_sub_epi64(row128[2][1], mask[1]);
				row128[2][2] = _mm_sub_epi64(row128[2][2], mask[2]);
				row128[2][3] = _mm_sub_epi64(row128[2][3], mask[3]);
				row128[2][4] = _mm_sub_epi64(row128[2][4], mask[4]);
				row128[2][5] = _mm_sub_epi64(row128[2][5], mask[5]);
				row128[2][6] = _mm_sub_epi64(row128[2][6], mask[6]);
				row128[2][7] = _mm_sub_epi64(row128[2][7], mask[7]);

				//if (sizeof(IdxType) == 2)
				//{
				//	std::array<__m256i*, 3> row256{
				//		(__m256i*)row128[0],
				//		(__m256i*)row128[1],
				//		(__m256i*)row128[2]
				//	};

				//	//
				// r[0][0],r[1][1],
				// r[2][2],r[1][0],
				// r[1][1],r[1][2], 
				//
				//}
				//else 
				{
					for (u64 j = 0; j < mWeight; ++j)
					{
						IdxType* __restrict rowi = row + mWeight * 16 * i;
						u64* __restrict row64 = (u64*)(row128[j]);
						rowi[mWeight * 0 + j] = row64[0];
						rowi[mWeight * 1 + j] = row64[1];
						rowi[mWeight * 2 + j] = row64[2];
						rowi[mWeight * 3 + j] = row64[3];
						rowi[mWeight * 4 + j] = row64[4];
						rowi[mWeight * 5 + j] = row64[5];
						rowi[mWeight * 6 + j] = row64[6];
						rowi[mWeight * 7 + j] = row64[7];

						rowi += 8 * mWeight;
						row64 += 8;

						rowi[mWeight * 0 + j] = row64[0];
						rowi[mWeight * 1 + j] = row64[1];
						rowi[mWeight * 2 + j] = row64[2];
						rowi[mWeight * 3 + j] = row64[3];
						rowi[mWeight * 4 + j] = row64[4];
						rowi[mWeight * 5 + j] = row64[5];
						rowi[mWeight * 6 + j] = row64[6];
						rowi[mWeight * 7 + j] = row64[7];
					}
				}
				//for (u64 k = 0; k < 16; ++k)
				//{
				//	IdxType row2[3];
				//	buildRow(hash[k + i * 16], row2);
				//	auto rowi = row + mWeight * 16 * i;
				//	//assert(rowi == row + mWeight * k);
				//	assert(row2[0] == rowi[mWeight * k + 0]);
				//	assert(row2[1] == rowi[mWeight * k + 1]);
				//	assert(row2[2] == rowi[mWeight * k + 2]);
				//}
			}
		}
		else
		{
			for (u64 k = 0; k < 32; ++k)
			{
				buildRow(hash[k], row);
				row += mWeight;
			}
		}
	}


	template<typename IdxType>
	void PaxosHash<IdxType>::buildRow(const block& hash, IdxType* row) const
	{

		//auto h = hash;
		//std::set<u64> ss;
		//u64 i = 0;
		//while (ss.size() != mWeight)
		//{
		//	auto hh = oc::AES(h).ecbEncBlock(block(0,i++));
		//	ss.insert(hh.as<u64>()[0] % mSparseSize);
		//}
		//std::copy(ss.begin(), ss.end(), row);
		//return;

		if (mWeight == 3)
		{
			u32* rr = (u32*)&hash;
			auto rr0 = *(u64*)(&rr[0]);
			auto rr1 = *(u64*)(&rr[1]);
			auto rr2 = *(u64*)(&rr[2]);
			row[0] = (IdxType)(rr0 % mSparseSize);
			row[1] = (IdxType)(rr1 % (mSparseSize - 1));
			row[2] = (IdxType)(rr2 % (mSparseSize - 2));

			assert(row[0] < mSparseSize);
			assert(row[1] < mSparseSize);
			assert(row[2] < mSparseSize);

			auto min = std::min<IdxType>(row[0], row[1]);
			auto max = row[0] + row[1] - min;

			if (max == row[1])
			{
				++row[1];
				++max;
			}

			if (row[2] >= min)
				++row[2];

			if (row[2] >= max)
				++row[2];
		}
		else
		{
			auto hh = hash;
			for (u64 j = 0; j < mWeight; ++j)
			{
				auto modulus = (mSparseSize - j);

				hh = hh.gf128Mul(hh);
				//std::memcpy(&h, (u8*)&hash + byteIdx, mIdxSize);
				auto colIdx = hh.get<u64>(0) % modulus;

				auto iter = row;
				auto end = row + j;
				while (iter != end)
				{
					if (*iter <= colIdx)
						++colIdx;
					else
						break;
					++iter;
				}


				while (iter != end)
				{
					end[0] = end[-1];
					--end;
				}

				*iter = static_cast<IdxType>(colIdx);
			}
		}
	}


	template<typename IdxType>
	void PaxosHash<IdxType>::hashBuildRow32(
		const block* inIter,
		IdxType* rows,
		block* hash) const
	{
		mAes.hashBlocks(span<const block>(inIter, 32), span<block>(hash, 32));
		buildRow32(hash, rows);
	}

	template<typename IdxType>
	void PaxosHash<IdxType>::hashBuildRow1(
		const block* inIter,
		IdxType* rows,
		block* hash) const
	{
		*hash = mAes.hashBlock(inIter[0]);
		buildRow(*hash, rows);
	}



	namespace {
		template<typename T>
		span<T> initSpan(u8*& iter, u64 size)
		{
			auto ret = span<T>((T*)iter, size);
			iter += sizeof(T) * size;
			return ret;
		}
		template<typename T>
		MatrixView<T> initMV(u8*& iter, u64 rows, u64 cols)
		{
			auto ret = MatrixView<T>((T*)iter, rows, cols);
			iter += sizeof(T) * rows * cols;
			return ret;
		}
	}


	template<typename IdxType>
	void Paxos<IdxType>::allocate()
	{
		auto size =
			sizeof(IdxType) * (mNumItems * mWeight) +
			sizeof(span<IdxType>) * mSparseSize +
			sizeof(IdxType) * (mNumItems * mWeight) +
			sizeof(block) * mNumItems
			;

		if (mAllocationSize < size)
		{
			mAllocation = {};
			mAllocation.reset(new u8[size]);
		}

		auto iter = mAllocation.get();

		mDense = initSpan<block>(iter, mNumItems);
		mRows = initMV<IdxType>(iter, mNumItems, mWeight);
		mColBacking = initSpan<IdxType>(iter, mNumItems * mWeight);
		mCols = initSpan<span<IdxType>>(iter, mSparseSize);
		assert(iter == mAllocation.get() + size);
	}

	constexpr u8 gPaxosBuildRowSize = 32;

	template<typename IdxType>
	void Paxos<IdxType>::init(u64 numItems, PaxosParam p, block seed)
	{
		if (p.mSparseSize >= u64(std::numeric_limits<IdxType>::max()))
		{
			std::cout << "the size of the paxos is too large for the index type. "
				<< p.mSparseSize << " vs "
				<< u64(std::numeric_limits<IdxType>::max()) << " " << LOCATION << std::endl;
			throw RTE_LOC;
		}

		if (p.mSparseSize + p.mDenseSize < numItems)
			throw RTE_LOC;

		static_cast<PaxosParam&>(*this) = p;
		mNumItems = static_cast<IdxType>(numItems);
		mSeed = seed;
		mHasher.init(mSeed, mWeight, mSparseSize);
	}

	template<typename IdxType>
	void Paxos<IdxType>::setInput(span<const block> inputs)
	{
		setTimePoint("setInput begin");
		if (inputs.size() != mNumItems)
			throw RTE_LOC;

		allocate();

		std::vector<IdxType> colWeights(mSparseSize);

#ifndef NDEBUG
		{
			std::unordered_set<block> inputSet;
			for (auto i : inputs)
			{
				assert(inputSet.insert(i).second);
			}
	}
#endif
		setTimePoint("setInput alloc");


		{
			auto main = inputs.size() / gPaxosBuildRowSize * gPaxosBuildRowSize;
			auto inIter = inputs.data();

			for (u64 i = 0; i < main; i += gPaxosBuildRowSize, inIter += gPaxosBuildRowSize)
			{
				auto rr = mRows[i].data();

				//if (gPaxosBuildRowSize == 8)
				//	mHasher.hashBuildRow8(inIter, rr, &mDense[i]);
				//else 
				if (gPaxosBuildRowSize == 32)
					mHasher.hashBuildRow32(inIter, rr, &mDense[i]);
				else
					throw RTE_LOC;

				span<IdxType> cols(rr, gPaxosBuildRowSize * mWeight);
				for (auto c : cols)
				{
					++colWeights[c];
				}
			}

			for (u64 i = main; i < mNumItems; ++i, ++inIter)
			{
				mHasher.hashBuildRow1(inIter, mRows[i].data(), &mDense[i]);
				for (auto c : mRows[i])
				{
					++colWeights[c];
				}
			}
		}
		setTimePoint("setInput buildRow");

		rebuildColumns(colWeights, mWeight * mNumItems);
		setTimePoint("setInput rebuildColumns");

		mWeightSets.init(colWeights);

		setTimePoint("setInput end");

}


	template<typename IdxType>
	void Paxos<IdxType>::setInput(MatrixView<IdxType> rows, span<block> dense)
	{
		if (rows.rows() != mNumItems || dense.size() != mNumItems)
			throw RTE_LOC;
		if (rows.cols() != mWeight)
			throw RTE_LOC;

		allocate();

		std::vector<IdxType> colWeights(mSparseSize);

		std::memcpy(mDense.data(), dense.data(), dense.size_bytes());
		for (u64 i = 0; i < mNumItems; ++i)
		{
			for (u64 j = 0; j < mWeight; ++j)
			{
				auto v = rows(i, j);
				assert(v < mSparseSize);
				mRows(i, j) = v;
				++colWeights[v];
			}
		}

		rebuildColumns(colWeights, mWeight * mNumItems);
		mWeightSets.init(colWeights);
	}


	template<typename IdxType>
	template<typename ValueType>
	void Paxos<IdxType>::decode(span<const block> inputs, span<ValueType> values, span<const ValueType> p)
	{
		PxVector<ValueType> VV(values);
		PxVector<const ValueType> PP(p);
		auto h = PP.defaultHelper();
		decode(inputs, VV, PP, h);
	}


	template<typename IdxType>
	template<typename ValueType>
	void Paxos<IdxType>::decode(span<const block> inputs, MatrixView<ValueType> values, MatrixView<const ValueType> p)
	{
		if (values.cols() != p.cols())
			throw RTE_LOC;

		if (values.cols() == 1)
		{
			decode(inputs, span<ValueType>(values), span<const ValueType>(p));
		}
		else if (
			values.cols() * sizeof(ValueType) % sizeof(block) == 0 &&
			std::is_same<ValueType, block>::value == false)
		{
			// reduce ValueType to block if possible.

			auto n = values.rows();
			auto m = values.cols() * sizeof(ValueType) / sizeof(block);

			decode<block>(
				inputs,
				MatrixView<block>((block*)values.data(), n, m),
				MatrixView<const block>((block*)p.data(), p.rows(), m));
		}
		else
		{
			PxMatrix<ValueType> VV(values);
			PxMatrix<const ValueType> PP(p);
			auto h = PP.defaultHelper();
			decode(inputs, VV, PP, h);
		}
	}

	template<typename IdxType>
	template<typename Helper, typename Vec, typename ConstVec>
	void Paxos<IdxType>::decode(span<const block> inputs, Vec& values, ConstVec& PP, Helper& h)
	{
		setTimePoint("decode begin");

		if (PP.size() != size())
			throw RTE_LOC;

		auto main = inputs.size() / gPaxosBuildRowSize * gPaxosBuildRowSize;

		auto inIter = inputs.data();

		Matrix<IdxType> rows(gPaxosBuildRowSize, mWeight);
		std::vector<block> dense(gPaxosBuildRowSize);
		if (mAddToDecode)
		{
			auto v = h.newVec(gPaxosBuildRowSize);
			for (u64 i = 0; i < main; i += gPaxosBuildRowSize, inIter += gPaxosBuildRowSize)
			{
				assert(gPaxosBuildRowSize == 32);
				mHasher.hashBuildRow32(inIter, rows.data(), dense.data());
				decode32(rows.data(), dense.data(), v[0], PP, h);
				for (u64 j = 0; j < 32; j += 8)
				{
					h.add(values[i + j + 0], v[j + 0]);
					h.add(values[i + j + 1], v[j + 1]);
					h.add(values[i + j + 2], v[j + 2]);
					h.add(values[i + j + 3], v[j + 3]);
					h.add(values[i + j + 4], v[j + 4]);
					h.add(values[i + j + 5], v[j + 5]);
					h.add(values[i + j + 6], v[j + 6]);
					h.add(values[i + j + 7], v[j + 7]);
				}
			}

			for (u64 i = main; i < inputs.size(); ++i, ++inIter)
			{
				mHasher.hashBuildRow1(inIter, rows.data(), dense.data());
				decode1(rows.data(), dense.data(), v[0], PP, h);
				h.add(values[i], v[0]);
			}
		}
		else
		{

			for (u64 i = 0; i < main; i += gPaxosBuildRowSize, inIter += gPaxosBuildRowSize)
			{
				assert(gPaxosBuildRowSize == 32);
				mHasher.hashBuildRow32(inIter, rows.data(), dense.data());
				decode32(rows.data(), dense.data(), values[i], PP, h);
			}

			for (u64 i = main; i < inputs.size(); ++i, ++inIter)
			{
				mHasher.hashBuildRow1(inIter, rows.data(), dense.data());
				decode1(rows.data(), dense.data(), values[i], PP, h);
			}
		}

		setTimePoint("decode done");
	}



	template<typename IdxType>
	void Paxos<IdxType>::setInput(
		MatrixView<IdxType> rows,
		span<block> dense,
		span<span<IdxType>> cols,
		span<IdxType> colBacking,
		span<IdxType> colWeights)
	{
		if (rows.rows() != mNumItems || dense.size() != mNumItems)
			throw RTE_LOC;
		if (rows.cols() != mWeight)
			throw RTE_LOC;
		if (cols.size() != mSparseSize)
			throw RTE_LOC;
		if (colBacking.size() != mNumItems * mWeight)
			throw RTE_LOC;
		if (colWeights.size() != mSparseSize)
			throw RTE_LOC;

		mRows = (rows);
		mDense = (dense);
		mCols = cols;
		mColBacking = colBacking;

		rebuildColumns(colWeights, mWeight * mNumItems);
		mWeightSets.init(colWeights);
	}



	template<typename IdxType>
	std::pair<PaxosPermutation<IdxType>, u64> Paxos<IdxType>::computePermutation(
		span<IdxType> mainRows,
		span<IdxType> mainCols,
		span<std::array<IdxType, 2>> gapRows,
		bool withDense)
	{
		auto cols = mSparseSize + mDenseSize * withDense;

		std::unordered_set<IdxType> cColSet, gapColSet;
		cColSet.insert(mainCols.begin(), mainCols.end());
		auto fcinv = getFCInv(mainRows, mainCols, gapRows);
		auto gapCols = getGapCols(fcinv, gapRows);

		if (withDense)
			for (auto cc : gapCols)
				gapColSet.insert(cc + mSparseSize);


		std::vector<IdxType> colPerm;

		// push back the D columns
		for (u64 i = 0; i < cols; ++i)
		{
			if (cColSet.find(i) == cColSet.end() &&
				gapColSet.find(i) == gapColSet.end())
			{
				colPerm.push_back(i);
			}
		}

		if (withDense)
		{
			for (auto cc : gapCols)
				colPerm.push_back(cc + mSparseSize);
		}

		for (u64 i = mainCols.size() - 1; i != u64(-1); --i)
			colPerm.push_back(mainCols[i]);

		std::vector<IdxType> rowPerm(mainRows.size());
		std::copy(mainRows.rbegin(), mainRows.rend(), rowPerm.begin());

		for (u64 i = 0; i < gapRows.size(); ++i)
			rowPerm.push_back(gapRows[i][0]);

		PaxosPermutation<IdxType> perm;
		perm.mRowPerm.initDst2Src(static_cast<IdxType>(rowPerm.size()), rowPerm);
		perm.mColPerm.initDst2Src(static_cast<IdxType>(colPerm.size()), colPerm);

		return { perm, gapRows.size() };
	}

	template<typename IdxType>
	typename Paxos<IdxType>::Triangulization Paxos<IdxType>::getTriangulization()
	{

		std::vector<IdxType> mainRows;
		std::vector<IdxType> mainCols;
		std::vector<std::array<IdxType, 2>> gapRows;
		triangulate(mainRows, mainCols, gapRows);
		auto permGap = computePermutation(mainRows, mainCols, gapRows, true);

		Triangulization ret;
		ret.mPerm = std::move(permGap.first);
		ret.mGap = std::move(permGap.second);
		ret.mH = getH(ret.mPerm);

		return ret;
	}


	template<typename IdxType>
	void Paxos<IdxType>::triangulate(
		std::vector<IdxType>& mainRows,
		std::vector<IdxType>& mainCols,
		std::vector<std::array<IdxType, 2>>& gapRows)
	{
		setTimePoint("triangulate begin");


		if (mWeightSets.mWeightSets.size() <= 1)
		{
			std::vector<IdxType> colWeights(mSparseSize);
			for (u64 i = 0; i < mCols.size(); ++i)
			{
				colWeights[i] = static_cast<IdxType>(mCols[i].size());
			}
			mWeightSets.init(colWeights);
		}

		std::vector<u8> rowSet(mNumItems);
		while (mWeightSets.mWeightSets.size() > 1)
		{
			auto& col = mWeightSets.getMinWeightNode();

			mWeightSets.popNode(col);
			col.mWeight = 0;
			auto colIdx = mWeightSets.idxOf(col);

			bool first = true;

			// iterate over all the rows in the next column
			for (auto rowIdx : mCols[colIdx])
			{
				// check if this row has already been set.
				if (rowSet[rowIdx] == 0)
				{
					rowSet[rowIdx] = 1;

					// iterate over the other columns in this row.
					for (auto colIdx2 : mRows[rowIdx])
					{
						auto& node = mWeightSets.mNodes[colIdx2];

						// if this column still hasn't been fixed,
						// then decrement it's weight.
						if (node.mWeight)
						{
							assert(node.mWeight);

							// decrement the weight.
							mWeightSets.popNode(node);
							--node.mWeight;
							mWeightSets.pushNode(node);

							// as an optimization, prefetch this next 
							// column if its ready to be used..
							if (node.mWeight == 1)
							{
								_mm_prefetch((const char*)&mCols[colIdx2], _MM_HINT_T0);
							}
						}
					}

					// if this is the first row, then we will use 
					// it as the row on the diagonal. Otherwise
					// we will place the extra rows in the "gap"
					if (!!(first))
					{
						mainCols.push_back(colIdx);
						mainRows.push_back(rowIdx);
						first = false;
					}
					else
					{
						// check that we dont have duplicates
						if (mDense[mainRows.back()] == mDense[rowIdx])
						{
							std::cout <<
								"Paxos error, Duplicate keys were detected at idx " << mainRows.back() << " " << rowIdx << ", key="
								<< mDense[mainRows.back()] << std::endl;
							throw RTE_LOC;
						}
						gapRows.emplace_back(
							std::array<IdxType, 2>{ rowIdx, mainRows.back() });
					}
				}
			}

			if (first)
				throw RTE_LOC;
		}

		setTimePoint("triangulate end");

	}

	template<typename IdxType>
	template<typename Vec, typename ConstVec, typename Helper>
	void Paxos<IdxType>::encode(ConstVec& values, Vec& output, Helper& h, PRNG* prng)
	{
		if (static_cast<u64>(output.size()) != size())
			throw RTE_LOC;

		std::vector<IdxType> mainRows, mainCols;
		mainRows.reserve(mNumItems); mainCols.reserve(mNumItems);
		std::vector<std::array<IdxType, 2>> gapRows;

		triangulate(mainRows, mainCols, gapRows);

		output.zerofill();

		if (prng)
		{
			typename WeightData<IdxType>::WeightNode* node = mWeightSets.mWeightSets[0];
			while (node != nullptr)
			{
				auto colIdx = mWeightSets.idxOf(*node);
				//prng->get(output[colIdx], output.stride());
				h.randomize(output[colIdx], *prng);

				if (node->mNextWeightNode == mWeightSets.NullNode)
					node = nullptr;
				else
					node = mWeightSets.mNodes.data() + node->mNextWeightNode;
			}
		}

		backfill(mainRows, mainCols, gapRows, values, output, h, prng);
	}

	template<typename IdxType>
	template<typename Vec, typename ConstVec, typename Helper>
	Vec Paxos<IdxType>::getX2Prime(
		FCInv& fcinv,
		span<std::array<IdxType, 2>> gapRows,
		span<u64> gapCols,
		const ConstVec& X,
		const Vec& P,
		Helper& helper)
	{
		assert(X.size() == mNumItems);
		bool randomized = P.size() != 0;


		auto g = gapRows.size();
		Vec xx2 = helper.newVec(g);

		for (u64 i = 0; i < g; ++i)
		{
			// x2' = x2 - FC^-1 x1
			helper.assign(xx2[i], X[gapRows[i][0]]);
			for (auto j : fcinv.mMtx[i])
				helper.add(xx2[i], X[j]);
		}

		// x2' = x2 - D' r - FC^-1 x1
		if (randomized)
		{
			assert(P.size() == mDenseSize + mSparseSize);
			auto p2 = P.subspan(mSparseSize);

			// note that D' only has a dense part because we
			// assume only duplcate rows in the gap, can 
			// therefore the sparse part of D' cancels.
			for (u64 i = 0; i < mDenseSize; ++i)
			{
				if (std::find(gapCols.begin(), gapCols.end(), i) == gapCols.end())
				{
					for (u64 j = 0; j < g; ++j)
					{
						auto dense = mDense[gapRows[j][0]];//^ mDense[gapRows[j][1]];
						for (auto k : fcinv.mMtx[j])
							dense = dense ^ mDense[k];

						if (*oc::BitIterator((u8*)&dense, i))
						{
							helper.add(xx2[j], p2[i]);
						}
					}
				}
			}
		}

		return xx2;
	}

	template<typename IdxType>
	oc::DenseMtx Paxos<IdxType>::getEPrime(
		FCInv& fcinv,
		span<std::array<IdxType, 2>> gapRows,
		span<u64> gapCols)
	{
		auto g = gapRows.size();

		// E' = E - FC^-1 B 
		DenseMtx EE(g, g);

		for (u64 i = 0; i < g; ++i)
		{
			// EERow    = E - FC^-1 B
			block EERow = mDense[gapRows[i][0]];
			for (auto j : fcinv.mMtx[i])
				EERow = EERow ^ mDense[j];

			// select the gap columns bits.
			for (u64 j = 0; j < g; ++j)
			{
				EE(i, j) =
					*BitIterator((u8*)&EERow, gapCols[j]);
			}
		}

		return EE;
	}

	template<typename IdxType>
	template<typename Vec, typename Helper>
	void Paxos<IdxType>::randomizeDenseCols(Vec& p2, Helper& h, span<u64> gapCols, PRNG* prng)
	{
		assert(prng);

		// handle the dense part of D.
		for (u64 i = 0; i < mDenseSize; ++i)
		{
			if (std::find(gapCols.begin(), gapCols.end(), i) == gapCols.end())
			{
				h.randomize(p2[i], *prng);
				//prng->get(p2[i], p2.stride());
			}
		}

	}



	template<typename IdxType>
	template<typename Vec, typename ConstVec, typename Helper>
	void Paxos<IdxType>::backfill(
		span<IdxType> mainRows,
		span<IdxType> mainCols,
		span<std::array<IdxType, 2>> gapRows,
		ConstVec& X,
		Vec& P,
		Helper& h,
		PRNG* prng)
	{
		// We are solving the system 
		// 
		// H * P = X
		//     
		// for the unknown "P". Divide the 
		// matrix into
		// 
		//       | A B C |
		//   H = | D E F |
		//
		// where C is a square lower-triangular matrix, 
		// E is a dense g*g matrix with the gap rows.
		// 
		// In particular, C have columns mainCols and 
		// rows mainCols. Rows of E are indexed by 
		// { gapRows[i][0] | i in [g] }. The columns of E will 
		// consists of a subset of the dense columns.
		// 
		// Then compute
		// 
		//      |I        0 |					 |I        0 |
		// H' = |-FC^-1   I | * H		 , X' =  |-FC^-1   I | * X
		// 
		//    = | A  B  C |					  = | x1  |
		//      | D' E' 0 |						| x2' |
		// 
		// where
		// 
		//  D' = -FC^-1A + D
		//  E' = -FC^-1B + E
		//  x2' = -FC^-1 x1 + x2
		// 
		// We require E' be invertible. There are a 
		// few ways of ensuring this.
		// 
		// One is to let E' be binary and then have 40 
		// extra dense columns and find an invertible E'
		// out of the 2^40 options. This succeeds with 
		// Pr 1-2^-40. 
		// 
		// Another option is to make E,B consist of 
		// random elements in a large field. Then E' 
		// is invertible  with overwhelming Pr. (1- 1/fieldSize).
		//
		// Observe that there are many solutions. In 
		// particular, the first columns corresponding 
		// to A,D can be set arbitrarially. Let p = | r p1 p2 | 
		// and rewrite the above as 
		//  
		//   | B  C | * | p1 | = | x1  | - | A  | * | r |
		//   | E' 0 |   | p2 | = | x2' |   | D' |
		// 
		// Therefore we have
		//
		//  r <- $ or r = 0
		//  x2' = x2 - D' r - FC^-1 x1
		//  p1  = -E'^-1 x2'
		//  x1' = x1 - A r - B p1
		//  p2  = -C^-1 x1'
		// 
		//  P = | r p2 p1 |
		//
		// Here r is either set to zero or random.
		// 
		// In the common case g will be zero and then we
		// have H = | A C | and we solve just using back 
		// propegation (C is trivially invertible). Ie
		// 
		// r <- $ or r = 0
		// x' = x1 - A r
		// p1 = -C^-1 x'
		//
		// P = | r p1 |

		setTimePoint("backFill begin");

		// select the method based on the dense type.
		// Both perform the same basic algorithm,
		if (mDt == DenseType::GF128)
		{
			backfillGf128(mainRows, mainCols, gapRows, X, P, h, prng);
		}
		else
		{
			backfillBinary(mainRows, mainCols, gapRows, X, P, h, prng);
		}
	}

	template<typename IdxType>
	template<typename Vec, typename ConstVec, typename Helper>
	void Paxos<IdxType>::backfillBinary(
		span<IdxType> mainRows,
		span<IdxType> mainCols,
		span<std::array<IdxType, 2>> gapRows,
		ConstVec& X,
		Vec& P,
		Helper& h,
		oc::PRNG* prng)
	{
		auto g = gapRows.size();

		// the dense columns which index the gap.
		std::vector<u64> gapCols;

		// masks that will be used to select the 
		// bits of the dense columns.
		std::vector<u64> denseMasks(g);

		// the dense part of the paxos.
		auto p2 = P.subspan(mSparseSize);

		if (g > mG)
			throw RTE_LOC;

		if (g)
		{

			auto fcinv = getFCInv(mainRows, mainCols, gapRows);

			// get the columns for the gap which define
			// B, E and therefore EE.
			gapCols = getGapCols(fcinv, gapRows);

			if (prng)
				randomizeDenseCols(p2, h, gapCols, prng);

			//auto PP = 
			// x2' = x2 - D r - FC^-1 x1
			auto xx2 = getX2Prime(fcinv, gapRows, gapCols, X, prng ? P : Vec{}, h);

			// E' = E - FC^-1 B 
			DenseMtx EE = getEPrime(fcinv, gapRows, gapCols);
			auto EEInv = EE.invert();

			// now we compute
			// p2 = E'^-1            * x2'
			//    = (-FC^-1B + E)^-1 * (x2 - D r - FC^-1 x1)
			for (u64 i = 0; i < g; ++i)
			{
				auto pp = p2[gapCols[i]];
				//if (pp != oc::ZeroBlock)
				//	throw RTE_LOC;

				for (u64 j = 0; j < g; ++j)
				{
					if (EEInv(i, j))
					{
						// pp = pp ^ xx2[j]
						h.add(pp, xx2[j]);
					}
				}
			}

			for (u64 i = 0; i < g; ++i)
			{
				denseMasks[i] = 1ull << gapCols[i];
			}

		}
		else if (prng)
		{
			for (u64 i = 0; i < static_cast<u64>(p2.size()); ++i)
				h.randomize(p2[i], *prng);
			//prng->get(p2.data(), p2.size());
		}

		auto outColIter = mainCols.rbegin();
		auto rowIter = mainRows.rbegin();

		// get a temporary element.
		auto yy = h.newElement();

		// get its pointer
		auto y = h.asPtr(yy);

		for (u64 k = 0; k < mainRows.size(); ++k)
		{
			auto i = *rowIter;
			auto c = *outColIter;
			++outColIter;
			++rowIter;

			// y = X[i]
			h.assign(y, X[i]);

			auto row = &mRows(i, 0);
			for (u64 j = 0; j < mWeight; ++j)
			{
				auto cc = row[j];

				// y += X[i]
				h.add(y, P[cc]);
			}

			assert(mDenseSize <= 64);
			// TODO, merge these two if statements
			if (prng)
			{
				auto d = mDense[i].template get<u64>(0);
				for (u64 j = 0; j < mDenseSize; ++j)
				{
					if (d & 1) {
						// y += p2[j]
						h.add(y, p2[j]);
					}
					d >>= 1;
				}
			}
			else
			{
				for (u64 j = 0; j < g; ++j) {
					assert(mDenseSize <= 64);
					if (mDense[i].template get<u64>(0) & denseMasks[j])
					{
						h.add(y, p2[gapCols[j]]);
					}
				}
			}

			h.assign(P[c], y);
		}
	}


	template<typename IdxType>
	template<typename Vec, typename ConstVec, typename Helper>
	void Paxos<IdxType>::backfillGf128(
		span<IdxType> mainRows,
		span<IdxType> mainCols,
		span<std::array<IdxType, 2>> gapRows,
		ConstVec& X,
		Vec& P,
		Helper& helper,
		PRNG* prng)
	{
		assert(mDt == DenseType::GF128);
		auto g = gapRows.size();
		auto p2 = P.subspan(mSparseSize);

		if (g > mDenseSize)
			throw RTE_LOC;

		if (g)
		{
			auto fcinv = getFCInv(mainRows, mainCols, gapRows);
			auto size = prng ? mDenseSize : g;

			//      |dense[r0]^1, dense[r0]^2, ... |
			// E =  |dense[r1]^1, dense[r1]^2, ... |
			//      |dense[r2]^1, dense[r2]^2, ... |
			//      ...
			// EE = E - FC^-1 B
			Matrix<block> EE(size, size);

			// xx = x' - FC^-1 x
			Vec xx = helper.newVec(size);
			//std::vector<block> xx(size);
			std::vector<block> fcb(size);

			for (u64 i = 0; i < g; ++i)
			{
				block e = mDense[gapRows[i][0]];
				block ej = e;
				EE(i, 0) = e;//^ fcb;
				for (u64 j = 1; j < size; ++j)
				{
					ej = ej.gf128Mul(e);
					EE(i, j) = ej;
				}


				helper.assign(xx[i], X[gapRows[i][0]]);
				for (auto j : fcinv.mMtx[i])
				{
					helper.add(xx[i], X[j]);
					auto fcb = mDense[j];
					auto fcbk = fcb;
					EE(i, 0) = EE(i, 0) ^ fcbk;
					for (u64 k = 1; k < size; ++k)
					{
						fcbk = fcbk.gf128Mul(fcb);
						EE(i, k) = EE(i, k) ^ fcbk;
					}
				}
			}

			if (prng)
			{
				for (u64 i = g; i < mDenseSize; ++i)
				{
					prng->get<block>(EE[i]);
					helper.randomize(xx[i], *prng);
				}
			}

			EE = gf128Inv(EE);
			if (EE.size() == 0)
				throw std::runtime_error("E' not invertable. " LOCATION);

			// now we compute
			// p' = (E - FC^-1 B)^-1 * (x'-FC^-1 x)
			//    = EE * xx
			for (u64 i = 0; i < size; ++i)
			{
				auto pp = p2[i];
				for (u64 j = 0; j < size; ++j)
				{
					//pp = pp ^ xx[j] * EE(i, j);
					helper.multAdd(pp, xx[j], EE(i, j));
				}
			}
		}
		else if (prng)
		{
			for (u64 i = 0; i < static_cast<u64>(p2.size()); ++i)
				helper.randomize(p2[i], *prng);
		}

		auto outColIter = mainCols.rbegin();
		auto rowIter = mainRows.rbegin();
		bool doDense = g || prng;

#define GF128_DENSE_BACKFILL										\
        if(doDense){														\
            block d = mDense[i];								\
            block x = d;										\
            helper.multAdd(y, p2[0], x);						\
                                                                \
            for (u64 i = 1; i < mDenseSize; ++i)				\
            {													\
                x = x.gf128Mul(d);								\
                helper.multAdd(y, p2[i], x);					\
            }													\
        }

		auto yy = helper.newElement();
		auto y = helper.asPtr(yy);

		if (mWeight == 3)
		{
			for (u64 k = 0; k < mainRows.size(); ++k)
			{
				auto i = *rowIter;
				auto c = *outColIter;
				++outColIter;
				++rowIter;

				//auto y = X[i];
				helper.assign(y, X[i]);

				auto row = mRows.data() + i * mWeight;
				auto cc0 = row[0];
				auto cc1 = row[1];
				auto cc2 = row[2];
				//y = y ^ P[cc0] ^ P[cc1] ^ P[cc2];
				helper.add(y, P[cc0]);
				helper.add(y, P[cc1]);
				helper.add(y, P[cc2]);

				//GF128_DENSE_BACKFILL;
				if (doDense) {

					block d = mDense[i];
					block x = d;
					helper.multAdd(y, p2[0], x);

					for (u64 i = 1; i < mDenseSize; ++i)
					{
						x = x.gf128Mul(d);
						helper.multAdd(y, p2[i], x);
					}
				}

				//P[c] = y;
				helper.assign(P[c], y);
			}
		}
		else
		{
			for (u64 k = 0; k < mainRows.size(); ++k)
			{
				auto i = *rowIter;
				auto c = *outColIter;
				++outColIter;
				++rowIter;

				//auto y = X[i];
				helper.assign(y, X[i]);
				//assert(P[c] == oc::ZeroBlock);

				auto row = &mRows(i, 0);
				for (u64 j = 0; j < mWeight; ++j)
				{
					auto cc = row[j];
					helper.add(y, P[cc]);
					//y = y ^ P[cc];
				}

				GF128_DENSE_BACKFILL;

				//P[c] = y;
				helper.assign(P[c], y);
			}
		}

	}


	template<typename IdxType>
	template<typename ValueType, typename Helper, typename Vec>
	void Paxos<IdxType>::decode32(
		const IdxType* rows_,
		const block* dense_,
		ValueType* values_,
		Vec& p_,
		Helper& h)
	{
		//{
		//	auto r = rows_;
		//	auto d = dense_;
		//	auto v = values_;
		//	auto& p = p_;

		//	for (u64 j = 0; j < 4; ++j)
		//	{
		//		decode8(r, d, v, p, h);
		//		r += 8 * mWeight;
		//		d += 8;
		//		v = h.iterPlus(v, 8);
		//	}
		//	return;
		//}

		const ValueType* __restrict p = p_[0];

		for (u64 j = 0; j < 4; ++j)
		{
			const IdxType* __restrict rows = rows_ + j * 8 * mWeight;
			ValueType* __restrict values = h.iterPlus(values_, j * 8);


			auto c00 = rows[mWeight * 0 + 0];
			auto c01 = rows[mWeight * 1 + 0];
			auto c02 = rows[mWeight * 2 + 0];
			auto c03 = rows[mWeight * 3 + 0];
			auto c04 = rows[mWeight * 4 + 0];
			auto c05 = rows[mWeight * 5 + 0];
			auto c06 = rows[mWeight * 6 + 0];
			auto c07 = rows[mWeight * 7 + 0];
			//auto c08 = rows[mWeight * 8 + 0];
			//auto c09 = rows[mWeight * 9 + 0];
			//auto c10 = rows[mWeight * 10 + 0];
			//auto c11 = rows[mWeight * 11 + 0];
			//auto c12 = rows[mWeight * 12 + 0];
			//auto c13 = rows[mWeight * 13 + 0];
			//auto c14 = rows[mWeight * 14 + 0];
			//auto c15 = rows[mWeight * 15 + 0];

			auto v00 = h.iterPlus(values, 0);
			auto v01 = h.iterPlus(values, 1);
			auto v02 = h.iterPlus(values, 2);
			auto v03 = h.iterPlus(values, 3);
			auto v04 = h.iterPlus(values, 4);
			auto v05 = h.iterPlus(values, 5);
			auto v06 = h.iterPlus(values, 6);
			auto v07 = h.iterPlus(values, 7);
			//auto v08 = h.iterPlus(values, 8);
			//auto v09 = h.iterPlus(values, 9);
			//auto v10 = h.iterPlus(values, 10);
			//auto v11 = h.iterPlus(values, 11);
			//auto v12 = h.iterPlus(values, 12);
			//auto v13 = h.iterPlus(values, 13);
			//auto v14 = h.iterPlus(values, 14);
			//auto v15 = h.iterPlus(values, 15);

			auto p00 = h.iterPlus(p, c00);
			auto p01 = h.iterPlus(p, c01);
			auto p02 = h.iterPlus(p, c02);
			auto p03 = h.iterPlus(p, c03);
			auto p04 = h.iterPlus(p, c04);
			auto p05 = h.iterPlus(p, c05);
			auto p06 = h.iterPlus(p, c06);
			auto p07 = h.iterPlus(p, c07);
			//auto p08 = h.iterPlus(p, c08);
			//auto p09 = h.iterPlus(p, c09);
			//auto p10 = h.iterPlus(p, c10);
			//auto p11 = h.iterPlus(p, c11);
			//auto p12 = h.iterPlus(p, c12);
			//auto p13 = h.iterPlus(p, c13);
			//auto p14 = h.iterPlus(p, c14);
			//auto p15 = h.iterPlus(p, c15);

			h.assign(v00, p00);
			h.assign(v01, p01);
			h.assign(v02, p02);
			h.assign(v03, p03);
			h.assign(v04, p04);
			h.assign(v05, p05);
			h.assign(v06, p06);
			h.assign(v07, p07);
			//h.assign(v08, p08);
			//h.assign(v09, p09);
			//h.assign(v10, p10);
			//h.assign(v11, p11);
			//h.assign(v12, p12);
			//h.assign(v13, p13);
			//h.assign(v14, p14);
			//h.assign(v15, p15);
		}

		for (u64 j = 1; j < mWeight; ++j)
		{

			for (u64 k = 0; k < 4; ++k)
			{
				const IdxType* __restrict rows = rows_ + k * 8 * mWeight;
				ValueType* __restrict values = h.iterPlus(values_, k * 8);

				auto c0 = rows[mWeight * 0 + j];
				auto c1 = rows[mWeight * 1 + j];
				auto c2 = rows[mWeight * 2 + j];
				auto c3 = rows[mWeight * 3 + j];
				auto c4 = rows[mWeight * 4 + j];
				auto c5 = rows[mWeight * 5 + j];
				auto c6 = rows[mWeight * 6 + j];
				auto c7 = rows[mWeight * 7 + j];

				auto v0 = h.iterPlus(values, 0);
				auto v1 = h.iterPlus(values, 1);
				auto v2 = h.iterPlus(values, 2);
				auto v3 = h.iterPlus(values, 3);
				auto v4 = h.iterPlus(values, 4);
				auto v5 = h.iterPlus(values, 5);
				auto v6 = h.iterPlus(values, 6);
				auto v7 = h.iterPlus(values, 7);

				auto p0 = h.iterPlus(p, c0);
				auto p1 = h.iterPlus(p, c1);
				auto p2 = h.iterPlus(p, c2);
				auto p3 = h.iterPlus(p, c3);
				auto p4 = h.iterPlus(p, c4);
				auto p5 = h.iterPlus(p, c5);
				auto p6 = h.iterPlus(p, c6);
				auto p7 = h.iterPlus(p, c7);

				h.add(v0, p0);
				h.add(v1, p1);
				h.add(v2, p2);
				h.add(v3, p3);
				h.add(v4, p4);
				h.add(v5, p5);
				h.add(v6, p6);
				h.add(v7, p7);

				//h.add(h.iterPlus(values, 0), h.iterPlus(p, c0));
				//h.add(h.iterPlus(values, 1), h.iterPlus(p, c1));
				//h.add(h.iterPlus(values, 2), h.iterPlus(p, c2));
				//h.add(h.iterPlus(values, 3), h.iterPlus(p, c3));
				//h.add(h.iterPlus(values, 4), h.iterPlus(p, c4));
				//h.add(h.iterPlus(values, 5), h.iterPlus(p, c5));
				//h.add(h.iterPlus(values, 6), h.iterPlus(p, c6));
				//h.add(h.iterPlus(values, 7), h.iterPlus(p, c7));
			}
		}


		if (mDt == DenseType::GF128)
		{
			const ValueType* __restrict p2 = h.iterPlus(p, mSparseSize);

			std::array<block, 32> xx;
			memcpy(xx.data(), dense_, sizeof(block) * 32);


			for (u64 k = 0; k < 4; ++k)
			{
				ValueType* __restrict values = h.iterPlus(values_, k * 8);
				auto x = xx.data() + k * 8;

				h.multAdd(h.iterPlus(values, 0), p2, x[0]);
				h.multAdd(h.iterPlus(values, 1), p2, x[1]);
				h.multAdd(h.iterPlus(values, 2), p2, x[2]);
				h.multAdd(h.iterPlus(values, 3), p2, x[3]);
				h.multAdd(h.iterPlus(values, 4), p2, x[4]);
				h.multAdd(h.iterPlus(values, 5), p2, x[5]);
				h.multAdd(h.iterPlus(values, 6), p2, x[6]);
				h.multAdd(h.iterPlus(values, 7), p2, x[7]);
			}

			for (u64 i = 1; i < mDenseSize; ++i)
			{
				p2 = h.iterPlus(p2, 1);

				for (u64 k = 0; k < 4; ++k)
				{
					auto x = xx.data() + k * 8;
					auto dense = dense_ + k * 8;
					ValueType* __restrict values = h.iterPlus(values_, k * 8);

					x[0] = x[0].gf128Mul(dense[0]);
					x[1] = x[1].gf128Mul(dense[1]);
					x[2] = x[2].gf128Mul(dense[2]);
					x[3] = x[3].gf128Mul(dense[3]);
					x[4] = x[4].gf128Mul(dense[4]);
					x[5] = x[5].gf128Mul(dense[5]);
					x[6] = x[6].gf128Mul(dense[6]);
					x[7] = x[7].gf128Mul(dense[7]);


					h.multAdd(h.iterPlus(values, 0), p2, x[0]);
					h.multAdd(h.iterPlus(values, 1), p2, x[1]);
					h.multAdd(h.iterPlus(values, 2), p2, x[2]);
					h.multAdd(h.iterPlus(values, 3), p2, x[3]);
					h.multAdd(h.iterPlus(values, 4), p2, x[4]);
					h.multAdd(h.iterPlus(values, 5), p2, x[5]);
					h.multAdd(h.iterPlus(values, 6), p2, x[6]);
					h.multAdd(h.iterPlus(values, 7), p2, x[7]);
				}
			}
		}
		else
		{

			std::array<u64, 8> d2;
			std::array<u8, 8> b;

			for (u64 k = 0; k < 4; ++k)
			{
				auto values = h.iterPlus(values_, k * 8);
				auto dense = dense_ + k * 8;
				//std::array<ValueType, 8> pp;
				//auto& pp = h.getTempArray8();
				//std::array<ValueType, 2> zeroAndAllOne;
				//memset(&zeroAndAllOne[0], 0, sizeof(ValueType));
				//memset(&zeroAndAllOne[1], -1, sizeof(ValueType));

				assert(mDenseSize <= 64);
				for (u64 i = 0; i < 8; ++i)
					d2[i] = dense[i].get<u64>(0);
				//memcpy(d2.data(), dense, sizeof(u64) * 8);
				for (u64 i = 0; i < mDenseSize; ++i)
				{
					b[0] = d2[0] & 1;
					b[1] = d2[1] & 1;
					b[2] = d2[2] & 1;
					b[3] = d2[3] & 1;
					b[4] = d2[4] & 1;
					b[5] = d2[5] & 1;
					b[6] = d2[6] & 1;
					b[7] = d2[7] & 1;

					d2[0] = d2[0] >> 1;
					d2[1] = d2[1] >> 1;
					d2[2] = d2[2] >> 1;
					d2[3] = d2[3] >> 1;
					d2[4] = d2[4] >> 1;
					d2[5] = d2[5] >> 1;
					d2[6] = d2[6] >> 1;
					d2[7] = d2[7] >> 1;

					//pp[0] = zeroAndAllOne[b[0]];
					//pp[1] = zeroAndAllOne[b[1]];
					//pp[2] = zeroAndAllOne[b[2]];
					//pp[3] = zeroAndAllOne[b[3]];
					//pp[4] = zeroAndAllOne[b[4]];
					//pp[5] = zeroAndAllOne[b[5]];
					//pp[6] = zeroAndAllOne[b[6]];
					//pp[7] = zeroAndAllOne[b[7]];

					//pp[0] = pp[0] & p2[i];
					//pp[1] = pp[1] & p2[i];
					//pp[2] = pp[2] & p2[i];
					//pp[3] = pp[3] & p2[i];
					//pp[4] = pp[4] & p2[i];
					//pp[5] = pp[5] & p2[i];
					//pp[6] = pp[6] & p2[i];
					//pp[7] = pp[7] & p2[i];
					//h.bitMult8(temp, b, p2[i]);
					auto p2 = h.iterPlus(p, mSparseSize + i);

					h.multAdd(h.iterPlus(values, 0), p2, b[0]);
					h.multAdd(h.iterPlus(values, 1), p2, b[1]);
					h.multAdd(h.iterPlus(values, 2), p2, b[2]);
					h.multAdd(h.iterPlus(values, 3), p2, b[3]);
					h.multAdd(h.iterPlus(values, 4), p2, b[4]);
					h.multAdd(h.iterPlus(values, 5), p2, b[5]);
					h.multAdd(h.iterPlus(values, 6), p2, b[6]);
					h.multAdd(h.iterPlus(values, 7), p2, b[7]);
				}
			}
		}

	}

	template<typename IdxType>
	template<typename ValueType, typename Helper, typename Vec>
	void Paxos<IdxType>::decode8(
		const IdxType* rows_,
		const block* dense_,
		ValueType* values_,
		Vec& p_,
		Helper& h)
	{
		//const block* __restrict xx = xx_;
		const IdxType* __restrict rows = rows_;
		const block* __restrict dense = dense_;
		ValueType* __restrict values = values_;
		const ValueType* __restrict p = p_[0];


		auto c0 = rows[mWeight * 0 + 0];
		auto c1 = rows[mWeight * 1 + 0];
		auto c2 = rows[mWeight * 2 + 0];
		auto c3 = rows[mWeight * 3 + 0];
		auto c4 = rows[mWeight * 4 + 0];
		auto c5 = rows[mWeight * 5 + 0];
		auto c6 = rows[mWeight * 6 + 0];
		auto c7 = rows[mWeight * 7 + 0];

		h.assign(h.iterPlus(values, 0), h.iterPlus(p, c0));
		h.assign(h.iterPlus(values, 1), h.iterPlus(p, c1));
		h.assign(h.iterPlus(values, 2), h.iterPlus(p, c2));
		h.assign(h.iterPlus(values, 3), h.iterPlus(p, c3));
		h.assign(h.iterPlus(values, 4), h.iterPlus(p, c4));
		h.assign(h.iterPlus(values, 5), h.iterPlus(p, c5));
		h.assign(h.iterPlus(values, 6), h.iterPlus(p, c6));
		h.assign(h.iterPlus(values, 7), h.iterPlus(p, c7));

		for (u64 j = 1; j < mWeight; ++j)
		{
			c0 = rows[mWeight * 0 + j];
			c1 = rows[mWeight * 1 + j];
			c2 = rows[mWeight * 2 + j];
			c3 = rows[mWeight * 3 + j];
			c4 = rows[mWeight * 4 + j];
			c5 = rows[mWeight * 5 + j];
			c6 = rows[mWeight * 6 + j];
			c7 = rows[mWeight * 7 + j];

			h.add(h.iterPlus(values, 0), h.iterPlus(p, c0));
			h.add(h.iterPlus(values, 1), h.iterPlus(p, c1));
			h.add(h.iterPlus(values, 2), h.iterPlus(p, c2));
			h.add(h.iterPlus(values, 3), h.iterPlus(p, c3));
			h.add(h.iterPlus(values, 4), h.iterPlus(p, c4));
			h.add(h.iterPlus(values, 5), h.iterPlus(p, c5));
			h.add(h.iterPlus(values, 6), h.iterPlus(p, c6));
			h.add(h.iterPlus(values, 7), h.iterPlus(p, c7));
		}


		if (mDt == DenseType::GF128)
		{
			const ValueType* __restrict p2 = h.iterPlus(p, mSparseSize);

			std::array<block, 8> x;
			memcpy(x.data(), dense, sizeof(block) * 8);
			h.multAdd(h.iterPlus(values, 0), p2, x[0]);
			h.multAdd(h.iterPlus(values, 1), p2, x[1]);
			h.multAdd(h.iterPlus(values, 2), p2, x[2]);
			h.multAdd(h.iterPlus(values, 3), p2, x[3]);
			h.multAdd(h.iterPlus(values, 4), p2, x[4]);
			h.multAdd(h.iterPlus(values, 5), p2, x[5]);
			h.multAdd(h.iterPlus(values, 6), p2, x[6]);
			h.multAdd(h.iterPlus(values, 7), p2, x[7]);

			for (u64 i = 1; i < mDenseSize; ++i)
			{
				p2 = h.iterPlus(p2, 1);

				x[0] = x[0].gf128Mul(dense[0]);
				x[1] = x[1].gf128Mul(dense[1]);
				x[2] = x[2].gf128Mul(dense[2]);
				x[3] = x[3].gf128Mul(dense[3]);
				x[4] = x[4].gf128Mul(dense[4]);
				x[5] = x[5].gf128Mul(dense[5]);
				x[6] = x[6].gf128Mul(dense[6]);
				x[7] = x[7].gf128Mul(dense[7]);

				h.multAdd(h.iterPlus(values, 0), p2, x[0]);
				h.multAdd(h.iterPlus(values, 1), p2, x[1]);
				h.multAdd(h.iterPlus(values, 2), p2, x[2]);
				h.multAdd(h.iterPlus(values, 3), p2, x[3]);
				h.multAdd(h.iterPlus(values, 4), p2, x[4]);
				h.multAdd(h.iterPlus(values, 5), p2, x[5]);
				h.multAdd(h.iterPlus(values, 6), p2, x[6]);
				h.multAdd(h.iterPlus(values, 7), p2, x[7]);
			}
		}
		else
		{

			std::array<u64, 8> d2;
			std::array<u8, 8> b;
			//std::array<ValueType, 8> pp;
			//auto& pp = h.getTempArray8();
			//std::array<ValueType, 2> zeroAndAllOne;
			//memset(&zeroAndAllOne[0], 0, sizeof(ValueType));
			//memset(&zeroAndAllOne[1], -1, sizeof(ValueType));

			assert(mDenseSize <= 64);
			for (u64 i = 0; i < 8; ++i)
				d2[i] = dense[i].get<u64>(0);
			//memcpy(d2.data(), dense, sizeof(u64) * 8);
			for (u64 i = 0; i < mDenseSize; ++i)
			{
				b[0] = d2[0] & 1;
				b[1] = d2[1] & 1;
				b[2] = d2[2] & 1;
				b[3] = d2[3] & 1;
				b[4] = d2[4] & 1;
				b[5] = d2[5] & 1;
				b[6] = d2[6] & 1;
				b[7] = d2[7] & 1;

				d2[0] = d2[0] >> 1;
				d2[1] = d2[1] >> 1;
				d2[2] = d2[2] >> 1;
				d2[3] = d2[3] >> 1;
				d2[4] = d2[4] >> 1;
				d2[5] = d2[5] >> 1;
				d2[6] = d2[6] >> 1;
				d2[7] = d2[7] >> 1;

				//pp[0] = zeroAndAllOne[b[0]];
				//pp[1] = zeroAndAllOne[b[1]];
				//pp[2] = zeroAndAllOne[b[2]];
				//pp[3] = zeroAndAllOne[b[3]];
				//pp[4] = zeroAndAllOne[b[4]];
				//pp[5] = zeroAndAllOne[b[5]];
				//pp[6] = zeroAndAllOne[b[6]];
				//pp[7] = zeroAndAllOne[b[7]];

				//pp[0] = pp[0] & p2[i];
				//pp[1] = pp[1] & p2[i];
				//pp[2] = pp[2] & p2[i];
				//pp[3] = pp[3] & p2[i];
				//pp[4] = pp[4] & p2[i];
				//pp[5] = pp[5] & p2[i];
				//pp[6] = pp[6] & p2[i];
				//pp[7] = pp[7] & p2[i];
				//h.bitMult8(temp, b, p2[i]);
				auto p2 = h.iterPlus(p, mSparseSize + i);

				h.multAdd(h.iterPlus(values, 0), p2, b[0]);
				h.multAdd(h.iterPlus(values, 1), p2, b[1]);
				h.multAdd(h.iterPlus(values, 2), p2, b[2]);
				h.multAdd(h.iterPlus(values, 3), p2, b[3]);
				h.multAdd(h.iterPlus(values, 4), p2, b[4]);
				h.multAdd(h.iterPlus(values, 5), p2, b[5]);
				h.multAdd(h.iterPlus(values, 6), p2, b[6]);
				h.multAdd(h.iterPlus(values, 7), p2, b[7]);
			}
		}
	}

	template<typename IdxType>
	template<typename ValueType, typename Helper, typename Vec>
	void Paxos<IdxType>::decode1(
		const IdxType* rows,
		const block* dense,
		ValueType* values,
		Vec& p,
		Helper& h)
	{
		h.assign(values, p[rows[0]]);
		for (u64 j = 1; j < mWeight; ++j)
		{

			h.add(values, p[rows[j]]);
		}

		//auto p2 = p.subspan(mSparseSize);

		if (mDt == DenseType::GF128)
		{
			block x = *dense;
			h.multAdd(values, p[mSparseSize], x);

			for (u64 i = 1; i < mDenseSize; ++i)
			{
				x = x.gf128Mul(dense[0]);
				h.multAdd(values, p[i + mSparseSize], x);
				//values[0] = values[0] ^ x.gf128Mul(p2[i + mSparseSize]);

			}
		}
		else
		{

			for (u64 i = 0; i < mDenseSize; ++i)
			{
				if (*BitIterator((u8*)dense, i))
				{
					//values[0] = values[0] ^ p[i + mSparseSize];
					h.add(values, p[i + mSparseSize]);
				}
			}
		}
	}

	template<typename IdxType>
	void Paxos<IdxType>::rebuildColumns(span<IdxType> colWeights, u64 totalWeight)
	{
		//std::vector<IdxType> backing(totalWeight);
		if (mColBacking.size() != totalWeight)
			throw RTE_LOC;

		auto colIter = mColBacking.data();
		for (u64 i = 0; i < mSparseSize; ++i)
		{
			mCols[i] = span<IdxType>(colIter, colIter);
			colIter += colWeights[i];
		}

		if (colIter != mColBacking.data() + mColBacking.size())
			throw RTE_LOC;

		if (mRows.cols() == 3)
		{
			auto iter = mRows.data();
			for (IdxType i = 0; i < mNumItems; ++i)
			{
				auto& c0 = mCols[iter[0]];
				auto& c1 = mCols[iter[1]];
				auto& c2 = mCols[iter[2]];
				iter += 3;

				auto s0 = c0.size();
				auto s1 = c1.size();
				auto s2 = c2.size();

				auto d0 = c0.data();
				auto d1 = c1.data();
				auto d2 = c2.data();

				c0 = span<IdxType>(d0, s0 + 1);
				c1 = span<IdxType>(d1, s1 + 1);
				c2 = span<IdxType>(d2, s2 + 1);

				c0[s0] = i;
				c1[s1] = i;
				c2[s2] = i;
			}
		}
		else
		{

			for (IdxType i = 0; i < mNumItems; ++i)
			{
				for (auto c : mRows[i])
				{
					auto& col = mCols[c];
					auto s = col.size();
					col = span<IdxType>(col.data(), s + 1);
					col[s] = i;
				}
			}
		}
	}

	template<typename IdxType>
	typename Paxos<IdxType>::FCInv Paxos<IdxType>::getFCInv(
		span<IdxType> mainRows,
		span<IdxType> mainCols,
		span<std::array<IdxType, 2>> gapRows) const
	{

		// maps the H column indexes to F,C column Indexes
		std::vector<IdxType> colMapping;
		FCInv ret(gapRows.size());
		auto m = mainRows.size();

		// the input rows are in reverse order compared to the
		// logical algorithm. This inverts the row index.
		auto invertRowIdx = [m](auto i) { return m - i - 1; };

		for (u64 i = 0; i < gapRows.size(); ++i)
		{
			if (std::memcmp(
				mRows[gapRows[i][0]].data(),
				mRows[gapRows[i][1]].data(),
				mWeight * sizeof(IdxType)) == 0)
			{
				// special/common case where FC^-1 [i] = 0000100000
				// where the 1 is at position gapRows[i][1]. This code is
				// used to speed up this common case.
				ret.mMtx[i].push_back(gapRows[i][1]);
			}
			else
			{
				// for the general case we need to implicitly create the C
				// matrix. The issue is that currently C is defined by mainRows
				// and mainCols and this form isn't ideal for the computation 
				// of computing F C^-1. In particular, we will need to know which
				// columns of the overall matrix H live in C. To do this we will construct
				// colMapping. For columns of H that are in C, colMapping will give us
				// the column in C. We only construct this mapping when its needed.
				if (colMapping.size() == 0)
				{
					colMapping.resize(size(), -1);
					for (u64 i = 0; i < m; ++i)
						colMapping[mainCols[invertRowIdx(i)]] = i;
				}

				// the current row of F. We initialize this as just F_i
				// and then Xor in rows of C until its the zero row.
				std::set<IdxType, std::greater<IdxType>> row;
				for (u64 j = 0; j < mWeight; ++j)
				{
					auto c1 = mRows(gapRows[i][0], j);
					if (colMapping[c1] != IdxType(-1))
						row.insert(colMapping[c1]);
				}

				while (row.size())
				{
					// the column of C, F that we will cancel (by adding
					// the corresponding row of C to F_i. We will pick the 
					// row of C as the row with index CCol.
					auto CCol = *row.begin();

					// the row of C we will add to F_i
					auto CRow = CCol;

					// the row of H that we will add to F_i
					auto HRow = mainRows[invertRowIdx(CRow)];
					ret.mMtx[i].push_back(HRow);

					for (auto HCol : mRows[HRow])
					{
						auto CCol2 = colMapping[HCol];
						if (CCol2 != IdxType(-1))
						{
							assert(CCol2 <= CCol);

							// Xor in the row CRow from C into the current
							// row of F
							auto iter = row.find(CCol2);
							if (iter == row.end())
								row.insert(CCol2);
							else
								row.erase(iter);
						}
					}

					assert(row.size() == 0 || *row.begin() != CCol);
				}
			}
		}

		return ret;
	}

	template<typename IdxType>
	std::vector<u64> Paxos<IdxType>::getGapCols(
		FCInv& fcinv,
		span<std::array<IdxType, 2>> gapRows) const
	{
		if (gapRows.size() == 0)
			return {};

		auto g = gapRows.size();
		u64 ci = 0;
		u64 e = oc::choose(mDenseSize, g);

		// E' = -FC^-1B + E
		DenseMtx EE;


		while (true)
		{
			// TDOD, make the linear time.
			auto gapCols = oc::ithCombination(ci, mDenseSize, g);
			++ci;
			if (ci > e)
				throw std::runtime_error("failed to find invertible matrix. " LOCATION);

			EE.resize(g, g);
			for (u64 i = 0; i < g; ++i)
			{
				block FCB = oc::ZeroBlock;
				for (auto c : fcinv.mMtx[i])
					FCB = FCB ^ mDense[c];

				// EE =       E                     + FC^-1 B
				block EERow = mDense[gapRows[i][0]] ^ FCB;
				for (u64 j = 0; j < g; ++j)
				{
					EE(i, j) =
						*BitIterator((u8*)&EERow, gapCols[j]);
				}
			}

			auto EEInv = EE.invert();
			if (EEInv.rows())
			{
				//std::cout << "EE*\n" << EE << std::endl;
				return gapCols;
			}
		}
	}

	template<typename IdxType>
	SparseMtx Paxos<IdxType>::getH(PaxosPermutation<IdxType>& perm) const
	{
		PointList points(mNumItems, mSparseSize + mDenseSize);

		for (u64 r = 0; r < mRows.rows(); ++r)
		{
			u64 rr = perm.mRowPerm.dst(perm.mRowPerm.src(r)).mIdx;
			for (auto c : mRows[r])
			{
				auto cc = perm.mColPerm.dst(perm.mColPerm.src(c)).mIdx;

				points.push_back({ rr,cc });
			}

			BitIterator iter((u8*)&mDense[r]);
			for (u64 j = 0; j < mDenseSize; ++j)
			{
				if (*iter)
				{
					auto c = j + mSparseSize;
					auto cc = perm.mColPerm.dst(perm.mColPerm.src(c)).mIdx;
					points.push_back({ rr, cc });
				}
				++iter;
			}
		}

		return points;
	}


	template<typename IdxType>
	SparseMtx Paxos<IdxType>::Triangulization::getA() const
	{
		// size of C
		IdxType s1 = mH.rows() - mGap;
		// size of A, including dense.
		IdxType s2 = mH.cols() - mH.rows();

		u64 cBegin = 0, cSize = s2;
		u64 rBegin = 0, rSize = s1;

		return mH.subMatrix(rBegin, cBegin, rSize, cSize);
	}

	template<typename IdxType>
	SparseMtx Paxos<IdxType>::Triangulization::getB() const
	{
		// size of C
		IdxType s1 = mH.rows() - mGap;
		// size of A, including dense.
		IdxType s2 = mH.cols() - mH.rows();

		u64 cBegin = s2, cSize = mGap;
		u64 rBegin = 0, rSize = s1;

		return mH.subMatrix(rBegin, cBegin, rSize, cSize);
	}


	template<typename IdxType>
	SparseMtx Paxos<IdxType>::Triangulization::getC() const
	{
		// size of C
		IdxType s1 = mH.rows() - mGap;
		// size of A, including dense.
		IdxType s2 = mH.cols() - mH.rows();

		u64 cBegin = s2 + mGap, cSize = s1;
		u64 rBegin = 0, rSize = s1;

		return mH.subMatrix(rBegin, cBegin, rSize, cSize);
	}

	template<typename IdxType>
	SparseMtx Paxos<IdxType>::Triangulization::getD() const
	{
		// size of C
		IdxType s1 = mH.rows() - mGap;
		// size of A, including dense.
		IdxType s2 = mH.cols() - mH.rows();

		u64 cBegin = 0, cSize = s2;
		u64 rBegin = s1, rSize = mGap;

		return mH.subMatrix(rBegin, cBegin, rSize, cSize);
	}

	template<typename IdxType>
	SparseMtx Paxos<IdxType>::Triangulization::getE() const
	{
		// size of C
		IdxType s1 = mH.rows() - mGap;
		// size of A, including dense.
		IdxType s2 = mH.cols() - mH.rows();

		u64 cBegin = s2, cSize = mGap;
		u64 rBegin = s1, rSize = mGap;

		return mH.subMatrix(rBegin, cBegin, rSize, cSize);
	}

	template<typename IdxType>
	SparseMtx Paxos<IdxType>::Triangulization::getF() const
	{
		// size of C
		IdxType s1 = mH.rows() - mGap;
		// size of A, including dense.
		IdxType s2 = mH.cols() - mH.rows();

		u64 cBegin = s2 + mGap, cSize = s1;
		u64 rBegin = s1, rSize = mGap;

		return mH.subMatrix(rBegin, cBegin, rSize, cSize);
	}


	template class Paxos<u64>;
	template class Paxos<u32>;
	template class Paxos<u16>;
	template class Paxos<u8>;

	inline u64 Baxos::getBinSize(u64 numBins, u64 numBalls, u64 statSecParam)
	{
		return SimpleIndex::get_bin_size(numBins, numBalls, statSecParam);
	}

	template<typename ValueType>
	void Baxos::solve(span<const block> inputs, span<const ValueType> values, span<ValueType> output, PRNG* prng, u64 numThreads)
	{
		PxVector<const ValueType> V(values);
		PxVector<ValueType> P(output);
		auto h = P.defaultHelper();
		solve(inputs, V, P, prng, numThreads, h);
	}

	template<typename ValueType>
	void Baxos::solve(span<const block> inputs, MatrixView<const ValueType> values, MatrixView<ValueType> output, PRNG* prng, u64 numThreads)
	{
		if (values.cols() != output.cols())
			throw RTE_LOC;

		if (values.cols() == 1)
		{
			solve(inputs, span<const ValueType>(values), span<ValueType>(output), prng, numThreads);
		}
		else if (
			values.cols() * sizeof(ValueType) % sizeof(block) == 0 &&
			std::is_same<ValueType, block>::value == false)
		{
			// reduce ValueType to block if possible.

			auto n = values.rows();
			auto m = values.cols() * sizeof(ValueType) / sizeof(block);

			solve<block>(
				inputs,
				MatrixView<const block>((block*)values.data(), n, m),
				MatrixView<block>((block*)output.data(), output.rows(), m),
				prng,
				numThreads);
		}
		else
		{
			PxMatrix<const ValueType> V(values);
			PxMatrix<ValueType> P(output);
			auto h = P.defaultHelper();
			solve(inputs, V, P, prng, numThreads, h);
		}
	}

	template<typename Vec, typename ConstVec, typename Helper>
	void Baxos::solve(
		span<const block> inputs,
		ConstVec& V,
		Vec& P,
		PRNG* prng,
		u64 numThreads,
		Helper& h)
	{
		// select the smallest index type which will work.
		auto bitLength = oc::roundUpTo(oc::log2ceil((u64)(mPaxosParam.mSparseSize + 1)), 8);

		if (bitLength <= 8)
			implParSolve<u8>(inputs, V, P, prng, numThreads, h);
		else if (bitLength <= 16)
			implParSolve<u16>(inputs, V, P, prng, numThreads, h);
		else if (bitLength <= 32)
			implParSolve<u32>(inputs, V, P, prng, numThreads, h);
		else
			implParSolve<u64>(inputs, V, P, prng, numThreads, h);


		if (mDebug)
			this->check(inputs, V, P);
	}

	template<typename IdxType, typename Vec, typename ConstVec, typename Helper>
	void Baxos::implParSolve(
		span<const block> inputs_,
		ConstVec& vals_,
		Vec& p_,
		PRNG* prng,
		u64 numThreads,
		Helper& h)
	{
#ifndef NDEBUG
		{
			std::unordered_set<block> inputSet;
			for (u64 i = 0; i < inputs_.size(); ++i)
			{
				assert(inputSet.insert(inputs_[i]).second);
			}
		}
#endif
		if (p_.size() != size())
			throw RTE_LOC;

		if (mNumBins == 1)
		{
			Paxos<IdxType> paxos;
			paxos.init(mNumItems, mPaxosParam, mSeed);
			paxos.setInput(inputs_);
			paxos.encode(vals_, p_, h, prng);

			//auto v2 = h.newVec(vals_.size());
			//paxos.decode(inputs_, v2, p_, h);
			//for (auto i = 0; i < v2.size(); ++i)
			//{
			//    assert(*vals_[i] == *v2[i]);
			//}
			return;
		}

		numThreads = std::max<u64>(1, numThreads);

		static constexpr const u64 batchSize = 32;

		// total number of bins between all threads.
		auto totalNumBins = mNumBins * numThreads;

		auto itemsPerThrd = (mNumItems + numThreads - 1) / numThreads;

		// maximum number of items any single thread will map to a bin.
		auto perThrdMaxBinSize = getBinSize(mNumBins, itemsPerThrd, mSsp);

		// the combined max size of the i'th bin held by each thread.
		u64 combinedMaxBinSize = perThrdMaxBinSize * numThreads;

		// keeps track of the size of each bin for each thread. Additional spaces to prevent false sharing
		Matrix<u64> thrdBinSizes(numThreads, mNumBins);

		// keeps track of input index of each item in each bin,thread.
		std::unique_ptr<u64[]> inputMapping(new u64[totalNumBins * perThrdMaxBinSize]);

		// for the given thread, bin, return the list which map the bin 
		// value back to the input value.
		auto getInputMapping = [&](u64 thrdIdx, u64 binIdx)
		{
			auto binBegin = combinedMaxBinSize * binIdx;
			auto thrdBegin = perThrdMaxBinSize * thrdIdx;
			span<u64> mapping(inputMapping.get() + binBegin + thrdBegin, perThrdMaxBinSize);
			assert(inputMapping.get() + totalNumBins * perThrdMaxBinSize >= mapping.data() + mapping.size());
			return mapping;
		};

		auto valBacking = h.newVec(totalNumBins * perThrdMaxBinSize);

		// get the values mapped to the given bin by the given thread.
		auto getValues = [&](u64 thrdIdx, u64 binIdx)
		{
			auto binBegin = combinedMaxBinSize * binIdx;
			auto thrdBegin = perThrdMaxBinSize * thrdIdx;

			return valBacking.subspan(binBegin + thrdBegin, perThrdMaxBinSize);
		};

		std::unique_ptr<block[]> hashBacking(new block[totalNumBins * perThrdMaxBinSize]);

		// get the hashes mapped to the given bin by the given thread.
		auto getHashes = [&](u64 thrdIdx, u64 binIdx)
		{
			auto binBegin = combinedMaxBinSize * binIdx;
			auto thrdBegin = perThrdMaxBinSize * thrdIdx;

			return span<block>(hashBacking.get() + binBegin + thrdBegin, perThrdMaxBinSize);
		};


		libdivide::libdivide_u64_t divider = libdivide::libdivide_u64_gen(mNumBins);
		AES hasher(mSeed);

		std::atomic<u64> numDone(0);
		std::promise<void> hashingDoneProm;
		auto hashingDoneFu = hashingDoneProm.get_future().share();

		auto routine = [&](u64 thrdIdx)
		{
			auto begin = (inputs_.size() * thrdIdx) / numThreads;
			auto end = (inputs_.size() * (thrdIdx + 1)) / numThreads;
			auto inputs = inputs_.subspan(begin, end - begin);


			// hash all the inputs in my range [begin, end) into their bin.
			// Each thread will have its own set of bins. ie a total of numThreads * numBins
			// bins in total. These bin will need to be merged. Each thread will also
			// get its own reverse mapping. 
			{
				auto binSizes = thrdBinSizes[thrdIdx];
				//auto hashes = span<block>(hashes_.data() + begin, end - begin);

				auto inIter = inputs.data();
				std::array<block, batchSize> hashes;
				//auto hashIter = hashes.data();

				auto main = inputs.size() / batchSize * batchSize;
				std::array<u64, batchSize> binIdxs;

				u64 i = 0;
				auto inIdx = begin;
				for (; i < main;
					i += batchSize,
					inIter += batchSize)
				{
					hasher.hashBlocks<8>(inIter + 0, hashes.data() + 0);
					hasher.hashBlocks<8>(inIter + 8, hashes.data() + 8);
					hasher.hashBlocks<8>(inIter + 16, hashes.data() + 16);
					hasher.hashBlocks<8>(inIter + 24, hashes.data() + 24);

					for (u64 k = 0; k < batchSize; ++k)
						binIdxs[k] = binIdxCompress(hashes[k]);

					doMod32(binIdxs.data(), &divider, mNumBins);

					for (u64 k = 0; k < batchSize; ++k, ++inIdx)
					{
						auto binIdx = binIdxs[k];
						auto bs = binSizes[binIdx]++;
						getInputMapping(thrdIdx, binIdx)[bs] = inIdx;
						h.assign(getValues(thrdIdx, binIdx)[bs], vals_[inIdx]);
						getHashes(thrdIdx, binIdx)[bs] = hashes[k];
					}
				}

				for (u64 k = 0; i < inputs.size(); ++i, ++inIter, ++k, ++inIdx)
				{
					hashes[k] = hasher.hashBlock(*inIter);

					auto binIdx = modNumBins(hashes[k]);
					auto bs = binSizes[binIdx]++;
					assert(bs < perThrdMaxBinSize);

					if (inIdx == 9355778)
						std::cout << "in " << inIdx << " -> bin " << binIdx << " @ " << bs << std::endl;
					getInputMapping(thrdIdx, binIdx)[bs] = inIdx;
					h.assign(getValues(thrdIdx, binIdx)[bs], vals_[inIdx]);
					getHashes(thrdIdx, binIdx)[bs] = hashes[k];
				}
			}

			auto paxosSizePer = mPaxosParam.size();
			auto allocSize =
				sizeof(IdxType) * (
					mItemsPerBin * mWeight * 2 +
					mPaxosParam.mSparseSize
					) +
				sizeof(span<IdxType>) * mPaxosParam.mSparseSize /*+
				sizeof(block) * mItemsPerBin*/;

			std::unique_ptr<u8[]> allocation(new u8[allocSize]);


			// block until all threads have mapped all items. 
			if (++numDone == numThreads)
				hashingDoneProm.set_value();
			else
				hashingDoneFu.get();

			Paxos<IdxType> paxos;


			// this thread will iterator over its assigned bins. This thread 
			// will aggregate all the items mapped to the ith bin (which are currently
			// stored in a per thread local).
			for (u64 binIdx = thrdIdx; binIdx < mNumBins; binIdx += numThreads)
			{
				// get the actual bin size.
				u64 binSize = 0;
				for (u64 i = 0; i < numThreads; ++i)
					binSize += thrdBinSizes(i, binIdx);

				if (binSize > mItemsPerBin)
					throw RTE_LOC;

				paxos.init(binSize, mPaxosParam, mSeed);

				auto iter = allocation.get();
				//span<block> hashes = initSpan<block>(iter, binSize);
				MatrixView<IdxType> rows = initMV<IdxType>(iter, binSize, mWeight);
				span<IdxType> colBacking = initSpan<IdxType>(iter, binSize * mWeight);
				span<IdxType> colWeights = initSpan<IdxType>(iter, mPaxosParam.mSparseSize);
				span<span<IdxType>> cols = initSpan<span<IdxType>>(iter, mPaxosParam.mSparseSize);

				if (iter > allocation.get() + allocSize)
					throw RTE_LOC;

				auto binBegin = combinedMaxBinSize * binIdx;
				auto values = valBacking.subspan(binBegin, binSize);
				auto hashes = span<block>(hashBacking.get() + binBegin, binSize);
				auto output = p_.subspan(paxosSizePer * binIdx, paxosSizePer);

				//for each thread, copy the hashes,values that it mapped
				//to this bin. 
				u64 binPos = thrdBinSizes(0, binIdx);
				assert(binPos <= perThrdMaxBinSize);
				assert(hashes.data() == getHashes(0, binIdx).data());

				for (u64 i = 1; i < numThreads; ++i)
				{
					auto size = thrdBinSizes(i, binIdx);
					assert(size <= perThrdMaxBinSize);
					auto thrdHashes = getHashes(i, binIdx);
					auto thrdVals = getValues(i, binIdx);

					//u8* db = (u8*)(hashes.data() + binPos);
					//u8* de = db + size * sizeof(block);
					//u8* sb = (u8*)thrdHashes.data();
					//u8* se = sb + size * sizeof(block);

					//if ((block*)sb != hashes.data() + i * perThrdMaxBinSize)
					//	throw RTE_LOC;

					//if(de > sb)
					//	throw RTE_LOC;

					memmove(hashes.data() + binPos, thrdHashes.data(), size * sizeof(block));
					//memcpy(values.data() + binPos, thrdVals.data() , size * sizeof(block));
					for (u64 j = 0; j < size; ++j)
						h.assign(values[binPos + j], thrdVals[j]);

					binPos += size;
					//auto mapping = getThrdMapping(i);
					//auto m = thrdBinSizes(i, binIdx);
					//for (u64 j = 0; j < m; ++j, ++binPos)
					//{
					//    auto inIdx = mapping(binIdx, j);
					//    hashes[binPos] = hashes_[inIdx];
					//    h.assign(values[binPos], vals_[inIdx]);
					//}
				}

				// compute the rows and count the column weight.
				std::memset(colWeights.data(), 0, colWeights.size() * sizeof(IdxType));
				auto rIter = rows.data();
				if (mWeight == 3)
				{
					auto main = binSize / batchSize * batchSize;

					u64 i = 0;
					for (; i < main; i += batchSize)
					{
						paxos.mHasher.buildRow32(&hashes[i], rIter);
						for (u64 j = 0; j < batchSize; ++j)
						{
							++colWeights[rIter[0]];
							++colWeights[rIter[1]];
							++colWeights[rIter[2]];
							rIter += mWeight;
						}
					}
					for (; i < binSize; ++i)
					{
						paxos.mHasher.buildRow(hashes[i], rIter);

						++colWeights[rIter[0]];
						++colWeights[rIter[1]];
						++colWeights[rIter[2]];
						rIter += mWeight;
					}
				}
				else
				{
					for (u64 i = 0; i < binSize; ++i)
					{
						paxos.mHasher.buildRow(hashes[i], rIter);
						for (u64 k = 0; k < mWeight; ++k)
							++colWeights[rIter[k]];
						rIter += mWeight;
					}
				}

				paxos.setInput(rows, hashes, cols, colBacking, colWeights);
				paxos.encode(values, output, h, prng);

			}
		};

		std::vector<std::thread> thrds(numThreads - 1);

		for (u64 i = 0; i < thrds.size(); ++i)
			thrds[i] = std::thread(routine, i);

		routine(thrds.size());

		for (u64 i = 0; i < thrds.size(); ++i)
			thrds[i].join();
	}

	template<typename ValueType>
	void Baxos::decode(span<const block> inputs, span<ValueType> values, span<const ValueType> p, u64 numThreads)
	{
		PxVector<ValueType> V(values);
		PxVector<const ValueType> P(p);
		auto h = V.defaultHelper();

		decode(inputs, V, P, h, numThreads);
	}


	template<typename ValueType>
	void Baxos::decode(span<const block> inputs, MatrixView<ValueType> values, MatrixView<const ValueType> p, u64 numThreads)
	{

		if (values.cols() != p.cols())
			throw RTE_LOC;

		if (values.cols() == 1)
		{
			decode(inputs, span<ValueType>(values), span<const ValueType>(p), numThreads);
		}
		else if (
			values.cols() * sizeof(ValueType) % sizeof(block) == 0 &&
			std::is_same<ValueType, block>::value == false)
		{
			// reduce ValueType to block if possible.

			auto n = values.rows();
			auto m = values.cols() * sizeof(ValueType) / sizeof(block);

			decode<block>(
				inputs,
				MatrixView<block>((block*)values.data(), n, m),
				MatrixView<const block>((block*)p.data(), p.rows(), m));
		}
		else
		{
			PxMatrix<ValueType> V(values);
			PxMatrix<const ValueType> P(p);
			auto h = V.defaultHelper();

			decode(inputs, V, P, h, numThreads);
		}
	}

	template<typename Vec, typename ConstVec, typename Helper>
	void Baxos::decode(
		span<const block> inputs,
		Vec& V,
		ConstVec& P,
		Helper& h,
		u64 numThreads)
	{
		auto bitLength = oc::roundUpTo(oc::log2ceil((u64)(mPaxosParam.mSparseSize + 1)), 8);
		if (bitLength <= 8)
			implParDecode<u8>(inputs, V, P, h, numThreads);
		else if (bitLength <= 16)
			implParDecode<u16>(inputs, V, P, h, numThreads);
		else if (bitLength <= 32)
			implParDecode<u32>(inputs, V, P, h, numThreads);
		else
			implParDecode<u64>(inputs, V, P, h, numThreads);
	}


	template<typename IdxType, typename Vec, typename ConstVec, typename Helper>
	void Baxos::implDecodeBin(
		u64 binIdx,
		span<block> hashes,
		Vec& values,
		Vec& valuesBuff,
		span<u64> inIdxs,
		ConstVec& PP,
		Helper& h,
		Paxos<IdxType>& paxos)
	{
		constexpr u64 batchSize = 32;
		constexpr u64 maxWeightSize = 20;

		auto main = (hashes.size() / batchSize) * batchSize;

		assert(mWeight <= maxWeightSize);
		std::array<IdxType, maxWeightSize* batchSize> _backing;
		MatrixView<IdxType> row(_backing.data(), batchSize, mWeight);
		assert(valuesBuff.size() >= batchSize);
		//std::array<block, decodeSize> vals;

		// todo, parallelize this
		u64 i = 0;
		//std::array<block, decodeSize> dense;
		for (; i < main; i += batchSize)
		{
			//for (u64 k = 0; k < decodeSize; ++k)
			//	paxos.mHasher.buildRow(hashes[i + k], row.data() + mWeight * k);
			paxos.mHasher.buildRow32(&hashes[i], row.data());
			paxos.decode32(row.data(), &hashes[i], valuesBuff[0], PP, h);


			if (mAddToDecode)
			{
				for (u64 k = 0; k < batchSize; ++k)
					h.add(values[inIdxs[i + k]], valuesBuff[k]);
			}
			else
			{
				for (u64 k = 0; k < batchSize; ++k)
					h.assign(values[inIdxs[i + k]], valuesBuff[k]);
			}
		}

		for (; i < hashes.size(); ++i)
		{
			paxos.mHasher.buildRow(hashes[i], row.data());
			auto v = values[inIdxs[i]];

			if (mAddToDecode)
			{
				paxos.decode1(row.data(), &hashes[i], valuesBuff[0], PP, h);
				h.add(v, valuesBuff[0]);
			}
			else
				paxos.decode1(row.data(), &hashes[i], v, PP, h);
		}
	}


	template<typename IdxType, typename Vec, typename ConstVec, typename Helper>
	void Baxos::implDecodeBatch(span<const block> inputs, Vec& values, ConstVec& pp, Helper& h)
	{
		u64 decodeSize = std::min<u64>(512, inputs.size());
		Matrix<block> batches(mNumBins, decodeSize);
		Matrix<u64> inIdxs(mNumBins, decodeSize);
		std::vector<u64> batchSizes(mNumBins);

		AES hasher(mSeed);
		auto inIter = inputs.data();
		Paxos<IdxType> paxos;
		auto sizePer = size() / mNumBins;
		paxos.init(1, mPaxosParam, mSeed);
		auto buff = h.newVec(32);

		//Matrix<IdxType> rows(8, mWeight);
		static const u32 batchSize = 32;
		auto main = inputs.size() / batchSize * batchSize;
		std::array<block, batchSize> buffer;
		std::array<u64, batchSize> binIdxs;
		//std::vector<u64> dense(8);
		u64 i = 0;
		libdivide::libdivide_u64_t divider = libdivide::libdivide_u64_gen(mNumBins);

		// iterate over the input
		for (; i < main; i += batchSize, inIter += batchSize)
		{
			hasher.hashBlocks<8>(inIter, buffer.data());
			hasher.hashBlocks<8>(inIter + 8, buffer.data() + 8);
			hasher.hashBlocks<8>(inIter + 16, buffer.data() + 16);
			hasher.hashBlocks<8>(inIter + 24, buffer.data() + 24);

			for (u64 j = 0; j < batchSize; j += 8)
			{
				binIdxs[j + 0] = binIdxCompress(buffer[j + 0]);
				binIdxs[j + 1] = binIdxCompress(buffer[j + 1]);
				binIdxs[j + 2] = binIdxCompress(buffer[j + 2]);
				binIdxs[j + 3] = binIdxCompress(buffer[j + 3]);
				binIdxs[j + 4] = binIdxCompress(buffer[j + 4]);
				binIdxs[j + 5] = binIdxCompress(buffer[j + 5]);
				binIdxs[j + 6] = binIdxCompress(buffer[j + 6]);
				binIdxs[j + 7] = binIdxCompress(buffer[j + 7]);
			}

			doMod32(binIdxs.data(), &divider, mNumBins);

			for (u64 k = 0; k < batchSize; ++k)
			{
				auto binIdx = binIdxs[k];

				batches(binIdx, batchSizes[binIdx]) = buffer[k];
				inIdxs(binIdx, batchSizes[binIdx]) = i + k;
				++batchSizes[binIdx];

				if (batchSizes[binIdx] == decodeSize)
				{
					auto p = pp.subspan(binIdx * sizePer, sizePer);
					auto idxs = inIdxs[binIdx];
					implDecodeBin(binIdx, batches[binIdx], values, buff, idxs, p, h, paxos);

					batchSizes[binIdx] = 0;
				}
			}
		}

		for (; i < inputs.size(); ++i, ++inIter)
		{
			auto k = 0;
			buffer[k] = hasher.hashBlock(*inIter);

			//auto binIdx = buffer[k].as<u64>()[0] % mNumBins;
			auto binIdx = modNumBins(buffer[k]);

			batches(binIdx, batchSizes[binIdx]) = buffer[k];
			inIdxs(binIdx, batchSizes[binIdx]) = i + k;
			++batchSizes[binIdx];

			//if (i + k == 557)
			//{
			//    std::cout << "decode * " << binIdx << std::endl;
			//}

			if (batchSizes[binIdx] == decodeSize)
			{
				auto p = pp.subspan(binIdx * sizePer, sizePer);
				implDecodeBin(binIdx, batches[binIdx], values, buff, inIdxs[binIdx], p, h, paxos);

				batchSizes[binIdx] = 0;
			}
		}

		for (u64 binIdx = 0; binIdx < mNumBins; ++binIdx)
		{
			if (batchSizes[binIdx])
			{
				auto p = pp.subspan(binIdx * sizePer, sizePer);
				auto b = batches[binIdx].subspan(0, batchSizes[binIdx]);
				implDecodeBin(binIdx, b, values, buff, inIdxs[binIdx], p, h, paxos);
			}
		}
	}


	template<typename IdxType, typename Vec, typename ConstVec, typename Helper>
	void Baxos::implParDecode(
		span<const block> inputs,
		Vec& values,
		ConstVec& pp,
		Helper& h,
		u64 numThreads)
	{
		if (mNumBins == 1)
		{
			Paxos<IdxType> paxos;
			paxos.init(1, mPaxosParam, mSeed);
			paxos.mAddToDecode = mAddToDecode;
			paxos.decode(inputs, values, pp, h);
			return;
		}


		numThreads = std::max<u64>(numThreads, 1ull);

		std::vector<std::thread> thrds(numThreads - 1);
		auto routine = [&](u64 i)
		{
			auto begin = (inputs.size() * i) / numThreads;
			auto end = (inputs.size() * (i + 1)) / numThreads;
			span<const block> in(inputs.begin() + begin, inputs.begin() + end);
			auto va = values.subspan(begin, end - begin);
			implDecodeBatch<IdxType>(in, va, pp, h);
		};

		for (u64 i = 0; i < thrds.size(); ++i)
			thrds[i] = std::thread(routine, i);

		routine(thrds.size());

		for (u64 i = 0; i < thrds.size(); ++i)
			thrds[i].join();
	}


}
