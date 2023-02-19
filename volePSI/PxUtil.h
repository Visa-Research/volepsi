#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <array>
#include <vector>
#include <set>
#include "volePSI/Defines.h"

#ifdef ENABLE_SSE
	#define LIBDIVIDE_AVX2
#endif

#include "libdivide.h"

namespace volePSI
{

	// A permutation (and its inverse) over the set [n].
	template<typename IdxType>
	struct Permutation
	{
		// An indexed from the source domain.
		struct SrcIdx
		{
			IdxType mIdx;
		};

		// an index from the distination domain (range)
		struct DstIdx
		{
			IdxType mIdx;

			bool operator>(const DstIdx& o) const
			{
				return (mIdx > o.mIdx);
			}
			bool operator<(const DstIdx& o) const
			{
				return (mIdx < o.mIdx);
			}
			bool operator>=(const DstIdx& o) const
			{
				return (mIdx >= o.mIdx);
			}
			bool operator<=(const DstIdx& o) const
			{
				return (mIdx <= o.mIdx);
			}
			bool operator==(const DstIdx& o) const
			{
				return (mIdx == o.mIdx);
			}
			bool operator!=(const DstIdx& o) const
			{
				return (mIdx != o.mIdx);
			}

			void operator++()
			{
				++mIdx;
			}

			DstIdx operator-(IdxType o) const
			{
				return { mIdx - o };
			}
		};


		// the permutation in the forward direction, 
		// mSrc2Dst[i] returns the location i is mapped 
		// in the output.
		std::vector<IdxType> mSrc2Dst;


		// the permutation in the backwards direction, 
		// mDst2Src[i] returns the output location i is mapped 
		// from in the input.
		std::vector<IdxType> mDst2Src;

		Permutation() = default;
		Permutation(const Permutation&) = default;
		Permutation(Permutation&&) = default;
		Permutation(IdxType n)
		{
			init(n);
		}

		Permutation& operator=(const Permutation&) = default;
		Permutation& operator=(Permutation&&) = default;

		bool operator==(const Permutation& p) const
		{
			return
				mSrc2Dst == p.mSrc2Dst &&
				mDst2Src == p.mDst2Src;
		}

		// construct a no-op permutation over [n]
		void init(IdxType n)
		{
			mSrc2Dst.resize(n);
			mDst2Src.resize(n);
			std::iota(mSrc2Dst.begin(), mSrc2Dst.end(), IdxType(0));
			std::iota(mDst2Src.begin(), mDst2Src.end(), IdxType(0));
		}

		// construct permutation over [n] given the destination to source mapping.
		// the mapping dst2Src can be incomplete.
		void initDst2Src(IdxType n, span<IdxType> dst2Src)
		{
			std::set<IdxType> srcs;
			for (IdxType i = 0; i < n; ++i)
				srcs.insert(i);

			mDst2Src.clear();
			mDst2Src.reserve(n);
			mDst2Src.insert(mDst2Src.end(), dst2Src.begin(), dst2Src.end());

			mSrc2Dst.resize(n);

			for (IdxType dst = 0; dst < dst2Src.size(); ++dst)
			{
				auto src = dst2Src[dst];
				auto iter = srcs.find(src);
				if (iter == srcs.end())
				{
					IdxType d1 = 0;
					while (dst2Src[d1] != src) ++d1;

					std::cout << "duplicates src " << src << " with dst " << d1 << " and " << dst << std::endl;
					throw RTE_LOC;
				}
				srcs.erase(iter);

				mSrc2Dst[src] = dst;
			}

			for (auto src : srcs)
			{
				auto dst = static_cast<IdxType>(mDst2Src.size());

				mDst2Src.push_back(src);
				mSrc2Dst[src] = dst;
			}
		}

		// encode i as a source index.
		SrcIdx src(IdxType i)const
		{
			return SrcIdx{ i };
		}

		// encode i as a destination index.
		DstIdx dst(IdxType i)const
		{
			return DstIdx{ i };
		}

		// get the source index for the destination index i.
		SrcIdx src(DstIdx i)const
		{
			return SrcIdx{ mDst2Src[i.mIdx] };
		}

		// get the destination index for the source index i.
		DstIdx dst(SrcIdx i)const
		{
			return DstIdx{ mSrc2Dst[i.mIdx] };
		}

		// invert the pemutation
		Permutation invert() const
		{
			Permutation p;
			p.mSrc2Dst = mDst2Src;
			p.mDst2Src = mSrc2Dst;
			return p;
		}

		// swap the destination position.
		void swap(DstIdx& i, DstIdx& j)
		{
			auto si = mDst2Src[i.mIdx];
			auto sj = mDst2Src[j.mIdx];

			std::swap(mSrc2Dst[si], mSrc2Dst[sj]);
			std::swap(mDst2Src[i.mIdx], mDst2Src[j.mIdx]);
		}
	};


	// An efficient data structure for tracking the weight of the 
	// paxos columns (node), which excludes the rows which have 
	// already been fixed (assigned to C). 
	template<typename IdxType>
	struct WeightData
	{
		static constexpr IdxType NullNode = ~IdxType(0);

		struct WeightNode
		{

			// The current of this column.
			IdxType mWeight;

			// The previous column with the same weight.
			IdxType mPrevWeightNode = NullNode;

			// the next column with the same weight
			IdxType mNextWeightNode = NullNode;
		};

		std::vector<WeightNode*> mWeightSets;
		//std::vector<WeightNode> mNodes;

		span<WeightNode> mNodes;
		std::unique_ptr<u8[]> mNodeBacking;
		u64 mNodeAllocSize = 0;

		// returns the index of the node
		IdxType idxOf(WeightNode& node)
		{
			return static_cast<IdxType>(&node - mNodes.data());
		}

		// returns the size of the set of weight w columns.
		u64 setSize(u64 w)
		{
			u64 s = 0;
			auto cur = mWeightSets[w];
			while (cur)
			{
				++s;

				if (cur->mNextWeightNode != NullNode)
					cur = &mNodes[cur->mNextWeightNode];
				else
					cur = nullptr;
			}

			return s;
		}

		// add the node/column to the data structure. Assumes
		// it is no already in it. 
		void pushNode(WeightNode& node)
		{
			assert(node.mNextWeightNode == NullNode);
			assert(node.mPrevWeightNode == NullNode);

			if (mWeightSets.size() <= node.mWeight)
			{
				mWeightSets.resize(node.mWeight + 1, nullptr);
			}

			if (mWeightSets[node.mWeight] == nullptr)
			{
				mWeightSets[node.mWeight] = &node;
			}
			else
			{
				assert(mWeightSets[node.mWeight]->mPrevWeightNode == NullNode);
				mWeightSets[node.mWeight]->mPrevWeightNode = idxOf(node);
				node.mNextWeightNode = idxOf(*mWeightSets[node.mWeight]);
				mWeightSets[node.mWeight] = &node;
			}
		}

		// remove the given node/column from the data structure.
		void popNode(WeightNode& node)
		{
			if (node.mPrevWeightNode == NullNode)
			{
				assert(mWeightSets[node.mWeight] == &node);
				if (node.mNextWeightNode == NullNode)
				{
					mWeightSets[node.mWeight] = nullptr;
					while (mWeightSets.back() == nullptr)
						mWeightSets.pop_back();
				}
				else
				{

					mWeightSets[node.mWeight] = &mNodes[node.mNextWeightNode];
					mWeightSets[node.mWeight]->mPrevWeightNode = NullNode;
				}
			}
			else
			{
				auto& prev = mNodes[node.mPrevWeightNode];

				if (node.mNextWeightNode == NullNode)
				{
					prev.mNextWeightNode = NullNode;
				}
				else
				{
					auto& next = mNodes[node.mNextWeightNode];
					prev.mNextWeightNode = idxOf(next);
					next.mPrevWeightNode = idxOf(prev);
				}

			}

			node.mPrevWeightNode = NullNode;
			node.mNextWeightNode = NullNode;
		}

		// decrease the weight of a given node/column
		void decementWeight(WeightNode& node)
		{
			assert(node.mWeight);
			popNode(node);
			--node.mWeight;
			pushNode(node);
		}

		// returns the node with minimum weight.
		WeightNode& getMinWeightNode()
		{
			for (u64 i = 1; i < mWeightSets.size(); ++i)
			{
				if (mWeightSets[i])
				{
					auto& node = *mWeightSets[i];
					return node;
				}
			}

			throw RTE_LOC;
		}

		// initialize the data structure with the current set of 
		// node/column weights.
		void init(span<IdxType> weights)
		{
			//mNodes.clear();
			if (mNodeAllocSize < weights.size())
			{
				mNodeBacking.reset(new u8[sizeof(WeightNode) * weights.size()]);
				mNodeAllocSize = weights.size();
				mNodes = span<WeightNode>((WeightNode*)mNodeBacking.get(), mNodeAllocSize);
			}

			mWeightSets.clear();
			mWeightSets.resize(200);
			//mNodes.resize(weights.size());

			for (IdxType i = 0; i < weights.size(); ++i)
			{
				mNodes[i].mWeight = weights[i];

				{
					auto& node = mNodes[i];
					node.mNextWeightNode = NullNode;
					node.mPrevWeightNode = NullNode;

					//if (GSL_UNLIKELY(mWeightSets.size() <= node.mWeight))
					//{
					//	mWeightSets.resize(node.mWeight + 1, nullptr);
					//}
					assert(node.mWeight < mWeightSets.size() && "something went wrong, maybe duplicate inputs.");

					auto& ws = mWeightSets[node.mWeight];
					if (!!(ws != nullptr))
					{
						assert(ws->mPrevWeightNode == NullNode);
						ws->mPrevWeightNode = idxOf(node);
						node.mNextWeightNode = idxOf(*ws);
					}

					ws = &node;
				}
			}

			for (u64 i = mWeightSets.size() - 1; i < mWeightSets.size(); --i)
			{
				if (mWeightSets[i])
				{
					mWeightSets.resize(i + 1);
					break;
				}
			}
#ifndef NDEBUG
			validate();
#endif // !NDEBUG
		}



		void validate()
		{
			std::set<u64> nodes;
			for (u64 i = 0; i < mWeightSets.size(); ++i)
			{
				auto node = mWeightSets[i];

				while (node)
				{
					assert(node->mWeight == i);

					assert(nodes.insert(idxOf(*node)).second);

					if (node->mNextWeightNode != NullNode)
					{
						assert(mNodes[node->mNextWeightNode].mPrevWeightNode == idxOf(*node));

						node = &mNodes[node->mNextWeightNode];
					}
					else
						node = nullptr;

				}
			}

			assert(nodes.size() == mNodes.size());

		}

	};

	template<typename IdxType>
	class Paxos;

	//template<typename IdxType>
	//struct PaxosDiff
	//{
	//	Paxos<IdxType>& mL, & mR;
	//	PaxosDiff(Paxos<IdxType>& a, Paxos<IdxType>& b)
	//		:mL(a), mR(b)
	//	{}
	//};

	//template<typename IdxType>
	//std::ostream& operator<<(std::ostream& o, const Paxos<IdxType>&);
	//template<typename IdxType>
	//std::ostream& operator<<(std::ostream& o, const PaxosDiff<IdxType>&);

	// A permutation over the rows and columns of the paxos matrix.
	template<typename IdxType>
	struct PaxosPermutation
	{
		Permutation<IdxType> mColPerm, mRowPerm;
	};


	template<typename IdxType>
	struct PaxosHash
	{
		u64 mWeight, mSparseSize, mIdxSize;
		oc::AES mAes;
		std::vector<libdivide::libdivide_u64_t> mMods;
		//std::vector<libdivide::libdivide_u64_branchfree_t> mModsBF;
		std::vector<u64> mModVals;
		void init(block seed, u64 weight, u64 paxosSize)
		{
			mWeight = weight;
			mSparseSize = paxosSize;
			mIdxSize = static_cast<IdxType>(oc::roundUpTo(oc::log2ceil(mSparseSize), 8) / 8);
			mAes.setKey(seed);

			mModVals.resize(weight);
			mMods.resize(weight);
			//mModsBF.resize(weight);
			for (u64 i = 0; i < weight; ++i)
			{
				mModVals[i] = mSparseSize - i;
				mMods[i] = libdivide::libdivide_u64_gen(mModVals[i]);
				//mModsBF[i] = libdivide::libdivide_u64_branchfree_gen(mModVals[i]);
			}
		}



		void mod32(u64* vals, u64 modIdx) const;

		void hashBuildRow32(const block* input, IdxType* rows, block* hash) const;
		//void hashBuildRow8(const block* input, IdxType* rows, block* hash) const;
		void hashBuildRow1(const block* input, IdxType* rows, block* hash) const;

		void buildRow(const block& hash, IdxType* row) const;
		//void buildRow8(const block* hash, IdxType* row) const;
		void buildRow32(const block* hash, IdxType* row) const;

	};

	// A Paxos vector type when the elements are of type T.
	// This differs from PxMatrix which has elements that 
	// each a vector of type T's. PxVector are more efficient
	// since we can remove an "inner for-loop." 
	template<typename T>
	struct PxVector
	{
		using value_type = T;
		using iterator = T*;
		using const_iterator = const T*;

		std::unique_ptr<value_type[]> mOwning;
		span<value_type> mElements;

		PxVector() = default;
		PxVector(const PxVector& v)
			: mElements(v.mElements)
		{}
		PxVector(PxVector&& v)
			: mOwning(std::move(v.mOwning))
			, mElements(std::move(v.mElements))
		{}

		PxVector(span<value_type> e)
			: mElements(e)
		{}

		PxVector(u64 size)
			: mOwning(new value_type[size])
			, mElements(mOwning.get(), size)
		{ }


		// return a iterator to the i'th element. Should be pointer symmatics
		inline iterator operator[](u64 i) { return &mElements[i]; }

		// return a iterator to the i'th element. Should be pointer symmatics
		inline const_iterator operator[](u64 i) const { return &mElements[i]; }

		// return the size of the vector
		inline auto size() const { return mElements.size(); }

		// return a subset of the vector, starting at index offset and of size count. 
		// if count = -1, then get the rest of the vector.
		inline PxVector subspan(u64 offset, u64 count = -1) { return mElements.subspan(offset, count); }

		// return a subset of the vector, starting at index offset and of size count. 
		// if count = -1, then get the rest of the vector.
		inline PxVector<const value_type> subspan(u64 offset, u64 count = -1) const { 
			return PxVector<const value_type>(mElements.subspan(offset, count));
		}

		// populate the vector with the zero element.
		inline void zerofill() { memset(mElements.data(), 0, mElements.size_bytes()); }

		// The default implementation of helper for PxVector.
		// This class performs operations of the elements of PxVector.
		struct Helper
		{
			// mutable version of value_type
			using mut_value_type = std::remove_const_t<value_type>;
			using mut_iterator = mut_value_type*;

			// internal mask used to multiply a value with a bit. 
			// Assumes the zero bit string is the zero element.
			std::array<mut_value_type, 2> zeroOneMask;

			Helper() {
				memset(&zeroOneMask[0], 0, sizeof(T));
				memset(&zeroOneMask[1], -1, sizeof(T));
			}

			Helper(const Helper&) = default;

			// return a element that the user can use.
			inline static mut_value_type newElement() { return {}; }

			// return the iterator for the return type of newElement().
			mut_iterator asPtr(mut_value_type& t) { return &t; }

			// return a vector of elements that the user can use.
			inline static PxVector<mut_value_type> newVec(u64 size) { return { size }; }

			// assign the src to the dst, ie *dst = *src.
			inline static void assign(mut_iterator dst, const_iterator src) { *dst = *src; }

			// add the src to the dst, ie *dst += *src.
			inline static void add(mut_iterator dst, const_iterator src1) { *dst = *dst ^ *src1; }

			// multiply src1 with m and add the result to the dst, ie *dst += (*src) * m.
			inline static void multAdd(mut_iterator dst, const_iterator src1, const block& m) {

				if constexpr (std::is_same<block, mut_value_type>::value)
					*dst = *dst ^ src1->gf128Mul(m);
				else
					throw std::runtime_error("the gf128 dense encoding is only implemented for block type. " LOCATION);
			}

			// multiply src1 with bit and add the result to the dst, ie *dst += (*src) * bit.
			inline void multAdd(mut_iterator dst, const_iterator src1, const u8& bit) {

				assert(bit < 2);
				*dst = *dst ^ (*src1 & zeroOneMask[bit]);
			}

			// return the iterator plus the given number of rows
			inline static auto iterPlus(const_iterator p, u64 i) { return p + i; }

			// return the iterator plus the given number of rows
			inline static auto iterPlus(mut_iterator p, u64 i) { return p + i; }

			// randomize the given element.
			inline static void randomize(mut_iterator p, oc::PRNG& prng)
			{
				prng.get(p, 1);
			}


			inline static auto eq(iterator p0, iterator p1)
			{
				return *p0 == *p1;
			}
		};

		// return the default helper for this vector type.
		static Helper defaultHelper()
		{
			return {};
		}
	};



	// A Paxos vector type when the elements are each 
	// an array of type T's with length mCols. mCols can 
	// be set at runtime. This differs from PxVector in
	// that PxVector has mCols hard coded to 1.
	template<typename T>
	struct PxMatrix
	{
		using value_type = T;
		using iterator = T*;
		using const_iterator = const T*;

		std::unique_ptr<value_type[]> mOwning;
		span<T> mElements;
		u64 mRows = 0, mCols = 0;

		PxMatrix() = default;
		PxMatrix(const PxMatrix& v)
			: mElements(v.mElements)
			, mRows(v.mRows)
			, mCols(v.mCols)
		{}

		PxMatrix(PxMatrix&& v) = default;

		PxMatrix(MatrixView<value_type> e)
			: mElements(e.data(), e.size())
			, mRows(e.rows())
			, mCols(e.cols())
		{}

		PxMatrix(u64 rows, u64 cols)
			: mOwning(new value_type[rows * cols])
			, mElements(mOwning.get(), rows * cols)
			, mRows(rows)
			, mCols(cols)
		{}


		PxMatrix(iterator ptr, u64 rows, u64 cols)
			: mElements(ptr, rows* cols)
			, mRows(rows)
			, mCols(cols)
		{}

		// return a iterator to the i'th element. Should be pointer symmatics
		inline iterator operator[](u64 i) { return &mElements[i * mCols]; }

		// return a iterator to the i'th element. Should be pointer symmatics
		inline const_iterator operator[](u64 i) const { return &mElements[i * mCols]; }
		
		// return the size of the vector
		inline auto size() const { return mRows; }

		// return a subset of the vector, starting at index offset.
		inline PxMatrix subspan(u64 offset) {
			assert(mRows >= offset);
			auto count = mRows - offset;
			return MatrixView<value_type>(mElements.data() + offset * mCols, count, mCols);
		}

		// return a subset of the vector, starting at index offset.
		inline PxMatrix<const value_type> subspan(u64 offset) const {
			assert(mRows >= offset);
			auto count = mRows - offset;
			return MatrixView<const value_type>(mElements.data() + offset * mCols, count, mCols);
		}
		// return a subset of the vector, starting at index offset and of size count. 
		inline PxMatrix subspan(u64 offset, u64 count) {
			assert(mRows >= offset);
			assert(count <= mRows - offset);
			return MatrixView<value_type>(mElements.data() + offset * mCols, count, mCols);
		}

		// return a subset of the vector, starting at index offset and of size count. 
		inline PxMatrix<const value_type> subspan(u64 offset, u64 count) const {
			assert(mRows >= offset);
			assert(count <= mRows - offset);
			return MatrixView<const value_type>(mElements.data() + offset * mCols, count, mCols);
		}

		// populate the vector with the zero element.
		inline void zerofill() { memset(mElements.data(), 0, mElements.size_bytes()); }

		// The default implementation of helper for PxVector.
		// This class performs operations of the elements of PxVector.
		struct Helper
		{
			// mutable version of value_type
			using mut_value_type = std::remove_const_t<value_type>;
			using mut_iterator = mut_value_type*;

			// the number of columns we should have.
			u64 mCols = 0;

			// internal mask used to multiply a value with a bit. 
			// Assumes the zero bit string is the zero element.
			std::array<mut_value_type, 2> zeroOneMask;
			Helper() {
				memset(&zeroOneMask[0], 0, sizeof(T));
				memset(&zeroOneMask[1], -1, sizeof(T));
			}

			Helper(const Helper&) = default;


			// return a element that the user can use.
			inline std::vector<value_type> newElement() { return std::vector<value_type>(mCols); }
			
			// return the iterator for the return type of newElement().
			iterator asPtr(std::vector<value_type>& t) { return t.data(); }

			// return a vector of elements that the user can use.
			inline PxMatrix<mut_value_type> newVec(u64 size) { return { size, mCols }; }

			// assign the src to the dst, ie *dst = *src.
			inline void assign(mut_iterator dst, const_iterator src) {
				memcpy(dst, src, sizeof(value_type) * mCols);
			}

			// add the src to the dst, ie *dst += *src.
			inline void add(mut_iterator dst, const_iterator src1) {
				for (u64 i = 0; i < mCols; ++i)
					dst[i] = dst[i] ^ src1[i];
			}

			// multiply src1 with m and add the result to the dst, ie *dst += (*src) * m.
			inline void multAdd(mut_iterator dst, const_iterator src1, const block& m) {

				if constexpr (std::is_same<block, mut_value_type>::value)
					for (u64 i = 0; i < mCols; ++i)
						dst[i] = dst[i] ^ src1[i].gf128Mul(m);
				else
					throw std::runtime_error("the gf128 dense encoding is only implemented for block type. " LOCATION);
			}

			// multiply src1 with bit and add the result to the dst, ie *dst += (*src) * bit.
			inline void multAdd(mut_iterator dst, const_iterator src1, const u8& bit) {

				assert(bit < 2);
				for (u64 i = 0; i < mCols; ++i)
					dst[i] = dst[i] ^ (src1[i] & zeroOneMask[bit]);
			}

			// return the iterator plus the given number of rows
			inline auto iterPlus(const_iterator p, u64 i) { return p + i * mCols; }

			// return the iterator plus the given number of rows
			inline auto iterPlus(mut_iterator p, u64 i) { return p + i * mCols; }

			// randomize the given element.
			inline void randomize(mut_iterator p, oc::PRNG& prng)
			{
				prng.get(p, mCols);
			}



			inline auto eq(iterator p0, iterator p1)
			{
				return memcmp(p0, p1, sizeof(value_type) * mCols) == 0;
			}
		};

		// return the default helper for this vector type.
		Helper defaultHelper() const
		{
			Helper h ;
			h.mCols = mCols;
			return h;
		}
	};

}

