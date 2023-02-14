#include "Paxos_Tests.h"
#include "volePSI/Paxos.h"
#include "cryptoTools/Crypto/PRNG.h"
#include <thread>
#include <cmath>
#include <unordered_set>
#include <random>
using namespace volePSI;

auto& ZeroBlock = oc::ZeroBlock;
auto& lout = oc::lout;
using PRNG = oc::PRNG;
using CLP = oc::CLP;
using PointList = oc::PointList;
using SparseMtx = oc::SparseMtx;


void Paxos_buildRow_Test(const oc::CLP& cmd)
{

	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
	u64 t = cmd.getOr("t", 1ull << cmd.getOr("tt", 4));
	auto s = cmd.getOr("s", 0);
	PaxosHash<u16> h;
	h.init(block(0, s), 3, n);
	std::vector<block> hash(32);
	oc::Matrix<u16> rows(32, 3);
	//oc::Matrix<u16> rows(32, 3);
	PRNG prng(block(1, s));

	std::array<std::array<u16, 3>, 32> exp
	{ {
		{{858, 751, 414}},
		{{677 , 590, 375 }},
		{{857 , 240, 0   }},
		{{18  , 373, 879 }},
		{{990 , 62, 458  }},
		{{894 , 667, 301 }},
		{{1023, 438, 301 }},
		{{532 , 815, 202 }},
		{{64  , 507, 82  }},
		{{664 , 739, 158 }},
		{{4   , 523, 573 }},
		{{719 , 282, 86  }},
		{{156 , 396, 473 }},
		{{810 , 916, 850 }},
		{{959 , 1017, 449}},
		{{3   , 841, 546 }},
		{{703 , 146, 19  }},
		{{935 , 983, 830 }},
		{{689 , 804, 550 }},
		{{237 , 661, 393 }},
		{{25  , 817, 387 }},
		{{112 , 531, 45  }},
		{{799 , 747, 158 }},
		{{986 , 444, 949 }},
		{{916 , 954, 410 }},
		{{736 , 219, 732 }},
		{{111 , 628, 750 }},
		{{272 , 627, 160 }},
		{{191 , 610, 628 }},
		{{1018, 213, 894 }},
		{{1   , 609, 948 }},
		{{570 , 60, 896  }},
	} };

	for (u64 tt = 0; tt < t; ++tt)
	{
		prng.get<block>(hash);
		h.buildRow32(hash.data(), rows.data());

		for (u64 i = 0; i < 32; ++i)
		{
			u16 rr[3];
			h.buildRow(hash[i], rr);

			for (u64 j = 0; j < 3; ++j)
			{
				if (rows(i, j) != rr[j])
					throw RTE_LOC;

				//if (tt == 0)
				//{
				//	std::cout << " " << rows(i, j);
				//	if (j) std::cout << ",";
				//}

				if (tt == 0)
				{

					if (rows(i, j) != exp[i][j])
					{
						std::cout << i << ", " << j << " e "
							<< exp[i][j] << " a " << rows(i, j) << std::endl;
						throw RTE_LOC;
					} 
				}
			}
			//if (tt == 0)
			//	std::cout << "\n";
		}


	}

}


void Paxos_solve_Test(const oc::CLP& cmd)
{


	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 15));
	//double e = cmd.getOr("e", 2.4);
	u64 w = cmd.getOr("w", 3);
	u64 s = cmd.getOr("s", 0);
	u64 t = cmd.getOr("t", 1);

	for (auto dt : { PaxosParam::Binary , PaxosParam::GF128 })
	{
		for (u64 tt = 0; tt < t; ++tt)
		{
			Paxos<u16> paxos;
			Paxos<u32> px2;
			paxos.init(n, w, 40, dt, ZeroBlock);
			px2.init(n, w, 40, dt, ZeroBlock);

			std::vector<block> items(n), values(n), values2(n), p(paxos.size());
			PRNG prng(block(tt, s));
			prng.get(items.data(), items.size());
			prng.get(values.data(), values.size());


			paxos.setInput(items);
			px2.setInput(items);

			for (u64 i = 0; i < paxos.mRows.rows(); ++i)
			{
				for (u64 j = 0; j < w; ++j)
				{
					auto v0 = paxos.mRows(i, j);
					auto v1 = px2.mRows(i, j);
					if (v0 != v1)
					{
						throw RTE_LOC;
					}
				}
			}

			paxos.encode<block>(values, p);
			paxos.decode<block>(items, values2, p);

			if (values2 != values)
			{
				throw RTE_LOC;
			}
		}
	}
}


void Paxos_solve_u8_Test(const oc::CLP& cmd)
{


	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 15));
	//double e = cmd.getOr("e", 2.4);
	u64 w = cmd.getOr("w", 3);
	u64 s = cmd.getOr("s", 0);
	u64 t = cmd.getOr("t", 1);

	for (auto dt : { PaxosParam::Binary /*, PaxosParam::GF128*/ })
	{
		for (u64 tt = 0; tt < t; ++tt)
		{
			Paxos<u16> paxos;
			Paxos<u32> px2;
			paxos.init(n, w, 40, dt, ZeroBlock);
			px2.init(n, w, 40, dt, ZeroBlock);

			std::vector<block> items(n);
			std::vector<u8> values(n), values2(n), p(paxos.size());
			PRNG prng(block(tt, s));
			prng.get(items.data(), items.size());
			prng.get(values.data(), values.size());

			paxos.setInput(items);
			px2.setInput(items);

			for (u64 i = 0; i < paxos.mRows.rows(); ++i)
			{
				for (u64 j = 0; j < w; ++j)
				{
					auto v0 = paxos.mRows(i, j);
					auto v1 = px2.mRows(i, j);
					if (v0 != v1)
					{
						throw RTE_LOC;
					}
				}
			}

			paxos.encode<u8>(values, p);
			paxos.decode<u8>(items, values2, p);

			if (values2 != values)
			{
				throw RTE_LOC;
			}
		}
	}
}

void Paxos_solve_mtx_Test(const oc::CLP& cmd)
{


	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 15));
	u64 c = 4;
	//double e = cmd.getOr("e", 2.4);
	u64 w = cmd.getOr("w", 3);
	u64 s = cmd.getOr("s", 0);
	u64 t = cmd.getOr("t", 1);

	for (auto dt : { PaxosParam::Binary /*, PaxosParam::GF128*/ })
	{
		for (u64 tt = 0; tt < t; ++tt)
		{
			Paxos<u16> paxos;
			Paxos<u32> px2;
			paxos.init(n, w, 40, dt, ZeroBlock);
			px2.init(n, w, 40, dt, ZeroBlock);

			std::vector<block> items(n);
			Matrix<u8> values(n, c), values2(n, c), p(paxos.size(), c);
			PRNG prng(block(tt, s));
			prng.get(items.data(), items.size());
			prng.get(values.data(), values.size());

			paxos.setInput(items);
			px2.setInput(items);

			for (u64 i = 0; i < paxos.mRows.rows(); ++i)
			{
				for (u64 j = 0; j < w; ++j)
				{
					auto v0 = paxos.mRows(i, j);
					auto v1 = px2.mRows(i, j);
					if (v0 != v1)
					{
						throw RTE_LOC;
					}
				}
			}

			paxos.encode<u8>(values, p);
			paxos.decode<u8>(items, values2, p);

			if (!(values2 == values))
			{
				throw RTE_LOC;
			}
		}
	}
}



void insertDuplicates(
	Paxos<u64>& paxos,
	Matrix<u64>& rows,
	span<block> dense,
	u64 g,
	PRNG& prng,
	PaxosParam::DenseType dt)
{

	auto w = rows.cols();
	auto n = rows.rows();
	PaxosParam param;
	param.mSparseSize = n * 1.4;
	param.mDenseSize = dt == PaxosParam::DenseType::Binary ? g + 40 : g;
	param.mG = g;
	param.mDt = dt;
	param.mSsp = 40;
	param.mWeight = w;
	paxos.init(n, param, ZeroBlock);


	std::unordered_set<block> rowSets;
	prng.get<block>(dense);

	for (u64 i = 0; i < n - g; ++i)
	{
		while (true)
		{
			paxos.mHasher.buildRow(dense[i], rows[i].data());

			oc::RandomOracle hash(sizeof(block));
			hash.Update(rows[i].data(), w);
			block hh;
			hash.Final(hh);
			if (rowSets.insert(hh).second)
				break;
		}
	}

	std::set<u64> gapRows;
	u64 j = n - g;
	while (gapRows.size() != g)
	{
		auto  i = prng.get<u64>() % (n - g);

		if (gapRows.insert(i).second)
		{
			std::memcpy(rows[j].data(), rows[i].data(), rows[i].size_bytes());
			//dense[j] = prng.get();
			++j;
		}
	}


	paxos.setInput(rows, dense);

}




void insertCycle(
	Paxos<u64>& paxos,
	Matrix<u64>& rows,
	span<block> dense,
	u64 g,
	PRNG& prng,
	PaxosParam::DenseType dt)
{
	auto w = rows.cols();
	auto n = rows.rows();
	PaxosParam param;
	param.mSparseSize = n + w - g;
	param.mDenseSize = dt == PaxosParam::DenseType::Binary ? g + 40 : g;
	param.mWeight = w;
	param.mG = g;
	param.mSsp = 40;
	param.mDt = dt;

	for (u64 i = 0; i < n - g; ++i)
	{
		for (u64 j = 0; j < w; ++j)
			rows(i, j) = i + j;
	}
	for (u64 i = n - g; i < n; ++i)
	{
		std::set<u64> ss;
		while (ss.size() < w)
			ss.insert(prng.get<u64>() % param.mSparseSize);
		u64 j = 0;
		for (auto s : ss)
			rows(i, j++) = s;
	}

	prng.get<block>(dense);
	paxos.init(n, param, ZeroBlock);
	paxos.setInput(rows, dense);
}

void Paxos_solve_gap_Test(const oc::CLP& cmd)
{


	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
	//double e = cmd.getOr("e", 2.4);
	u64 w = cmd.getOr("w", 3);
	u64 s = cmd.getOr("s", 0);
	//u64 v = cmd.getOr("v", cmd.isSet("v") ? 1 : 0);
	u64 t = cmd.getOr("t", 1);
	u64 g = cmd.getOr("g", 3);

	for (auto dup : { false, true })
	{

		for (auto dt : { PaxosParam::Binary , PaxosParam::GF128 })
		{
			for (u64 tt = 0; tt < t; ++tt)
			{
				Paxos<u64> paxos;
				Matrix<u64> rows(n, w);
				PRNG prng(block(tt, s));

				std::vector<block> dense(n);

				if (dup)
					insertDuplicates(paxos, rows, dense, g, prng, dt);
				else
					insertCycle(paxos, rows, dense, g, prng, dt);

				std::vector<block> values(n), p(paxos.size());
				prng.get<block>(values);
				paxos.encode<block>(values, p);


				PxVector<block> PP(p);
				auto h = PP.defaultHelper();
				for (u64 i = 0; i < n; ++i)
				{
					block values2;
					paxos.decode1<block>(rows[i].data(), &dense[i], &values2, PP, h);
					if (values2 != values[i])
					{
						throw RTE_LOC;
					}

				}
			}
		}
	}
}

void Paxos_solve_rand_Test(const oc::CLP& cmd)
{
	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
	u64 s = cmd.getOr("s", 0);
	u64 t = cmd.getOr("t", 1);

	for (auto dt : { PaxosParam::Binary , PaxosParam::GF128 })
	{
		for (u64 tt = 0; tt < t; ++tt)
		{
			Paxos<u64> paxos;
			//n = 4000;
			paxos.init(n, PaxosParam(n, 3, 40, dt), ZeroBlock);
			std::vector<block> items(n), values(n), values2(n), p(paxos.size(), ZeroBlock);
			PRNG prng(block(tt, s));
			prng.get(items.data(), items.size());
			prng.get(values.data(), values.size());

			//for (u64 i = 0; i < p.size(); ++i)
			//	p[i] = block(342 * i, i);

			paxos.solve<block>(items, values, p, &prng);

			for (u64 i = 0; i < p.size(); ++i)
			{
				auto pp = p[i];
				if (pp == oc::ZeroBlock)
					throw RTE_LOC;
			}

			paxos.decode<block>(items, values2, p);

			if (values2 != values)
			{
				u64 count = 0;
				for (u64 i = 0; i < n; ++i)
				{
					if (values[i] != values2[i])
					{
						if (count < 10)
							std::cout << i << " " << values[i] << " " << values2[i] << std::endl;
						else
						{

							std::cout << "..." << std::endl;
							break;
						}

						++count;
					}
				}
				throw RTE_LOC;
			}
		}
	}
}

void Paxos_solve_rand_gap_Test(const oc::CLP& cmd)
{
	u64 n = 1ull << 10;
	double e = 1.4;
	u64 w = 3;
	u64 s = 0;
	u64 g = 1;

	for (auto dt : { PaxosParam::Binary , PaxosParam::GF128 })
	{
		Paxos<u64> paxos;

		PaxosParam param;
		param.mSparseSize = n * e;
		param.mDenseSize = 40 + g;
		param.mDt = dt;
		param.mG = g;
		param.mSsp = 40;
		param.mWeight = w;
		paxos.init(n, param, ZeroBlock);
		std::vector<block> values(n), p(paxos.size()), dense(n);


		Matrix<u64> rows(n, w);
		PRNG prng(block(0, s));

		std::unordered_set<block> rowSets;
		prng.get<block>(dense);
		for (u64 i = 0; i < n - g; ++i)
		{
			while (true)
			{
				paxos.mHasher.buildRow(dense[i], rows[i].data());

				oc::RandomOracle hash(sizeof(block));
				hash.Update(rows[i].data(), w);
				block hh;
				hash.Final(hh);
				if (rowSets.insert(hh).second)
					break;
			}
		}

		std::set<u64> gapRows;
		u64 j = n - g;
		while (gapRows.size() != g)
		{
			auto  i = prng.get<u64>() % (n - g);
			if (gapRows.insert(i).second)
			{
				std::memcpy(rows[j].data(), rows[i].data(), rows[i].size_bytes());
				++j;
			}
		}

		prng.get(values.data(), values.size());

		paxos.setInput(rows, dense);

		for (u64 i = 0; i < p.size(); ++i)
			p[i] = oc::ZeroBlock;

		paxos.mDebug = true;
		paxos.encode<block>(values, p, &prng);

		for (u64 i = 0; i < p.size(); ++i)
		{
			auto pp = p[i];
			if (pp == oc::ZeroBlock)
				throw RTE_LOC;
		}



		PxVector<block> PP(p);
		auto h = PP.defaultHelper();
		for (u64 i = 0; i < n; ++i)
		{
			block values2;
			paxos.decode1<block>(rows[i].data(), &dense[i], &values2, PP, h);
			if (values2 != values[i])
			{
				std::cout << (values2 ^ values[i]) << std::endl;
				throw RTE_LOC;
			}

		}
	}
}

namespace volePSI
{

	block hexToBlock(const std::string& buff);
}


void Baxos_solve_Test(const oc::CLP& cmd)
{

	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
	u64 b = cmd.getOr("b", n/4);
	u64 w = cmd.getOr("w", 3);
	u64 s = cmd.getOr("s", 0);
	//u64 v = cmd.getOr("v", cmd.isSet("v") ? 1 : 0);
	u64 t = cmd.getOr("t", 1);
	bool v = cmd.isSet("v");
	//u64 g = cmd.getOr("g", 3);

	//for (u64 n = 10; n < nn; ++n)
	{
		PRNG prng(block(0, s));
		Baxos paxos;
		paxos.init(n, b, w, 40, PaxosParam::GF128, prng.get<block>());

		std::vector<block> items(n), values(n), values2(n), p(paxos.size());
		prng.get(items.data(), items.size());
		prng.get(values.data(), values.size());

		for (u64 tt = 0; tt < t; ++tt)
		{
			PRNG prng(block(tt, s));
			paxos.init(n, b, w, 40, PaxosParam::GF128, prng.get<block>());

			paxos.solve<block>(items, values, p);
			paxos.decode<block>(items, values2, p);

			if (v)
				std::cout << "." << std::flush;
			if (values2 != values)
			{
				u64 count = 0;
				for (u64 i = 0; i < n; ++i)
				{
					if (values[i] != values2[i])
					{
						if (count < 10)
							std::cout << i << " " << values[i] << " " << values2[i] << std::endl;
						else
						{

							std::cout << "..." << std::endl;
							break;
						}

						++count;
					}
				}

				std::cout << "seed " << prng.getSeed() << " "<<tt << std::endl;
				throw RTE_LOC;
			}
		}
	}

}


void Baxos_solve_mtx_Test(const oc::CLP& cmd)
{

	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
	u64 b = cmd.getOr("b", n / 4);
	u64 w = cmd.getOr("w", 3);
	u64 s = cmd.getOr("s", 0);
	//u64 v = cmd.getOr("v", cmd.isSet("v") ? 1 : 0);
	u64 t = cmd.getOr("t", 1);
	u64 c = cmd.getOr("c", 3);

	//for (u64 n = 10; n < nn; ++n)
	{


		for (u64 tt = 0; tt < t; ++tt)
		{
			Baxos paxos;
			paxos.init(n, b, w, 40, PaxosParam::Binary, ZeroBlock);
			std::vector<block> items(n);
			Matrix<u8> values(n, c), values2(n, c), p(paxos.size(), c);
			PRNG prng(block(tt, s));
			prng.get(items.data(), items.size());
			prng.get(values.data(), values.size());

			paxos.solve<u8>(items, values, p);


			paxos.decode<u8>(items, values2, p);

			if (!(values2 == values))
			{
				u64 count = 0;
				for (u64 i = 0; i < n; ++i)
				{
					if (std::memcmp(values[i].data(), values2[i].data(), c))
					{
						if (count < 10)
							std::cout << i << " " << values[i][0] << " " << values2[i][0] << std::endl;
						else
						{

							std::cout << "..." << std::endl;
							break;
						}

						++count;
					}
				}
				throw RTE_LOC;
			}
		}
	}

}

void Baxos_solve_par_Test(const oc::CLP& cmd)
{
	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 16));
	u64 b = cmd.getOr("b", n / 16);
	u64 w = cmd.getOr("w", 3);
	u64 s = cmd.getOr("s", 0);
	u64 t = cmd.getOr("t", 1);
	u64 nt = cmd.getOr("nt", 8);

	//for (u64 n = 10; n < nn; ++n)
	{


		for (u64 tt = 0; tt < t; ++tt)
		{
			Baxos paxos;
			paxos.init(n, b, w, 40, PaxosParam::Binary, ZeroBlock);
			std::vector<block> items(n), values(n), values2(n), p(paxos.size());
			PRNG prng(block(tt, s));
			prng.get(items.data(), items.size());
			prng.get(values.data(), values.size());

			paxos.solve<block>(items, values, p, nullptr, 1);

			paxos.solve<block>(items, values, p, nullptr, nt);


			paxos.decode<block>(items, values2, p, nt);

			if (values2 != values)
			{
				u64 count = 0;
				for (u64 i = 0; i < n; ++i)
				{
					if (values[i] != values2[i])
					{
						if (count < 10)
							std::cout << i << " " << values[i] << " " << values2[i] << std::endl;
						else
						{

							std::cout << "..." << std::endl;
							break;
						}

						++count;
					}
				}
				throw RTE_LOC;
			}
		}
	}
}

void Baxos_solve_rand_Test(const oc::CLP& cmd)
{
	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 16));
	//u64 b = cmd.getOr("b", n / 4);
	//double e = cmd.getOr("e", 2.4);
	//u64 w = cmd.getOr("w", 3);
	u64 s = cmd.getOr("s", 0);
	//u64 v = cmd.getOr("v", cmd.isSet("v") ? 1 : 0);
	u64 t = cmd.getOr("t", 1);
	//u64 g = cmd.getOr("g", 3);

	//for (u64 n = 10; n < nn; ++n)
	{

		for (u64 tt = 0; tt < t; ++tt)
		{
			Baxos paxos;
			//n = 4000;
			paxos.init(n, 1 << 10, 3, 40, PaxosParam::Binary, ZeroBlock);
			std::vector<block> items(n), values(n), values2(n), p(paxos.size(), ZeroBlock);
			PRNG prng(block(tt, s));
			prng.get(items.data(), items.size());
			prng.get(values.data(), values.size());

			for (u64 i = 0; i < p.size(); ++i)
				p[i] = block(342 * i, i);

			paxos.solve<block>(items, values, p, &prng);

			for (u64 i = 0; i < p.size(); ++i)
			{
				auto pp = p[i];
				if (pp == block(342 * i, i))
					throw RTE_LOC;
			}


			paxos.decode<block>(items, values2, p);

			if (values2 != values)
			{
				u64 count = 0;
				for (u64 i = 0; i < n; ++i)
				{
					if (values[i] != values2[i])
					{
						if (count < 10)
							std::cout << i << " " << values[i] << " " << values2[i] << std::endl;
						else
						{

							std::cout << "..." << std::endl;
							break;
						}

						++count;
					}
				}
				throw RTE_LOC;
			}
		}
	}
}

void Paxos_experiment(const CLP& cmd)
{
	// the number of items inserted
	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));

	// the expansion ratio.
	double e = cmd.getOr("e", 2.4);

	// the step size if more than one expansion should be used.
	double eStep = cmd.getOr("es", 0.01);
	double eEnd = cmd.getOr("ee", e);

	// the weight
	u64 w = cmd.getOr("w", 3);

	// the seed
	u64 s = cmd.getOr("s", 0);

	// verbose
	u64 v = cmd.getOr("v", cmd.isSet("v") ? 1 : 0);

	// number of trials
	u64 t = cmd.getOr("t", 1);

	// the trial that we should start on
	u64 ts = cmd.getOr("ts", 0);

	// numnber of threads
	u64 nt = cmd.getOr("nt", std::thread::hardware_concurrency());


	u64 maxPrint = cmd.getOr("mp", 5);

	auto window = cmd.getOr("window", 0.0);
	auto expo = cmd.getOr("expo", 0.0);
	auto lap = cmd.isSet("lap");
	auto uniform = cmd.isSet("uniform");
	auto ignoreDup = cmd.isSet("ignoreDup");

	bool noPrint = cmd.isSet("noPrint");
	while (e <= eEnd)
	{
		u64 total = 0;
		std::vector<u64> counts;
		std::vector<u64> badSeeds;
		std::mutex mtx;
		bool done = false;
		auto routine = [&](u64 thrdIdx)
		{
			Paxos<u32> paxos;

			PaxosParam param;
			param.mSparseSize = n * e;
			param.mDenseSize = 40;
			param.mDt = PaxosParam::Binary;
			param.mSsp = 40;
			param.mWeight = w;
			paxos.init(n, param, ZeroBlock);
			if (v > 1)
				paxos.mVerbose = true;


			std::vector<block> items(n), p(paxos.mSparseSize);


			for (u64 i = thrdIdx + ts; i < t; i += nt)
			{
				PRNG prng(block(i, s));


				if (window)
				{
					throw RTE_LOC;
					if (window > 1 || window < 0)
					{
						std::cout << " window must be in [0,1]" << std::endl;
						throw RTE_LOC;
					}
					auto mod = u64(paxos.mSparseSize * window);
					oc::Matrix<u32> rows(n, w);
					for (u64 i = 0; i < n; ++i)
					{
						u32 p = prng.get<u32>() % paxos.mSparseSize;
						std::set<u32> ss;
						while (ss.size() != w)
						{
							ss.insert((p + (prng.get<u32>() % mod)) % paxos.mSparseSize);
						}

						std::copy(ss.begin(), ss.end(), rows[i].begin());
					}
					std::vector<block> dense(n);
					prng.get<block>(dense);
					paxos.setInput(rows, dense);
				}
				else if (expo)
				{
					throw RTE_LOC;
					std::exponential_distribution<double> x(expo);
					oc::Matrix<u32> rows(n, w);
					for (u64 i = 0; i < n; ++i)
					{
						u32 p = prng.get<u32>() % paxos.mSparseSize;
						std::set<u32> ss;
						while (ss.size() != w)
						{
							auto v = u32(x(prng) * paxos.mSparseSize);

							// negate
							if (lap && prng.getBit())
								v = paxos.mSparseSize - v;
							//std::cout << v << std::endl;
							ss.insert((p + v) % paxos.mSparseSize);
						}

						std::copy(ss.begin(), ss.end(), rows[i].begin());
					}
					std::vector<block> dense(n);
					prng.get<block>(dense);
					paxos.setInput(rows, dense);
				}
				else if (uniform)
				{
					//PRNG prng(block(i, s));
					//std::cout << "s " << prng.getSeed() << std::endl;
					oc::Matrix<u32> rows(n, w);
					for (u64 i = 0; i < n; ++i)
					{
						std::set<u32> ss;
						while (ss.size() != w)
						{
							ss.insert(prng.get<u32>() % paxos.mSparseSize);
						}

						std::copy(ss.begin(), ss.end(), rows[i].begin());
					}
					std::vector<block> dense(n);
					prng.get<block>(dense);
					paxos.setInput(rows, dense);
				}
				else
				{
					prng.get(items.data(), items.size());
					paxos.setInput(items);
				}
				//auto src = paxos;
				//for (u64 j = 0; j < 3; ++j)
				{
					std::vector<u32> mr, mc;
					std::vector<std::array<u32, 2>> gap;
					paxos.triangulate(mr, mc, gap);

					if (ignoreDup)
					{
						std::vector<std::array<u32, 2>> gap2;
						for (u64 i = 0; i < gap.size(); ++i)
						{
							auto r0 = paxos.mRows[gap[i][0]].data();
							auto r1 = paxos.mRows[gap[i][1]].data();
							auto cmp = memcmp(r0, r1, paxos.mRows.cols() * sizeof(u64));
							if (cmp)
								gap2.push_back(gap[i]);
						}
						gap = std::move(gap2);
					}
					auto g = gap.size();

					//auto a = paxos.apply(perm);

					//if (paxos != a.unapply())
					//    throw RTE_LOC;

					//lout << paxos << std::endl
					//    << a << std::endl;
					//auto g = a.gap();
					//std::cout << "g " << g << std::endl;;

					//paxos = paxos.shuffle(prng);
					if (v)
					{
						auto tri = paxos.getTriangulization();
						std::cout << g << "\n" << tri.mH << std::endl;
					}
					//else
					//{
					//    //std::cout << g << " ";
					//}


					std::lock_guard<std::mutex> lock(mtx);
					if (counts.size() <= g)
						counts.resize(g + 1);

					++counts[g];
					++total;

					if (g)
						badSeeds.push_back(i);

					//if (g)
					//{

					//    auto pp = paxos.shuffle(prng);
					//    auto perm2 = pp.triangulate();
					//    pp = pp.apply(perm2);
					//    //std::cout << pp << std::endl;
					//    auto g1 = pp.gap();

					//    auto perm3 = paxos.core(false);
					//    auto g2 = paxos.apply(perm3).gap();
					//    if (g != g2 || g1 != g)
					//    {

					//        std::cout << "bad gap " << g << " " << g1 << " " << g2 << std::endl;
					//        paxos.core(true);

					//        std::cout << g << " " << g2 << "\n" << paxos.apply(perm) << std::endl;

					//        abort();
					//    }
					//    //
					//    //abort();
					//}
				}
				//std::cout <<"\n-----------------------------\n "<< std::endl;

			}

			done = true;
		};

		auto print = [&]()
		{
			if (noPrint)
				return;
			double p = 0;
			double nn, avg = 0;
			std::vector<double> ssp;
			u64 min = ~0ull, max = 0, maxCount = 0;
			std::vector<u64>bs;
			{
				std::lock_guard<std::mutex> lock(mtx);
				if (total)
				{
					max = counts.size() - 1;
					u64 goodCount = 0;
					maxCount = counts.back();

					p = double(total) * 100 / t;
					nn = std::log2(std::max<u64>(1, total));
					for (u64 i = 0; i < counts.size(); ++i)
					{
						goodCount += counts[i];
						auto badCount = total - goodCount;
						ssp.push_back(nn - std::log2(std::max<u64>(1, badCount)));

						if (counts[i])
							min = std::min<u64>(min, i);

						avg += counts[i] * i;
					}

					bs = badSeeds;
					avg = avg / total;
				}
			}
			if (p)
			{
				std::stringstream ss;
				ss << "\r"
					<< min << " "
					<< avg << " "
					<< max << " (" << maxCount << ") ~ ";

				for (u64 i = 0; i < std::min<u64>(ssp.size(), maxPrint); ++i)
					ss << ssp[i] << " ";

				ss << " ~ " << nn << " " << p << "  ~ ";

				u64 maxBS = 4;
				for (u64 i = 0; i < std::min<u64>(4, bs.size()); ++i)
					ss << bs[i] << " ";

				if (bs.size() > maxBS)
					ss << "... (" << bs.size() << ")";
				ss << "       ";
				lout << ss.str() << std::flush;
			}
		};


		std::vector<std::thread> thrds(nt);
		for (u64 i = 0; i < thrds.size(); ++i)
			thrds[i] = std::thread(routine, i);

		std::cout << "min avg max (maxCount) ~ gap0-ssp gap1-ssp ... ~ log2(trials) percentDone ~ badSeeds " << std::endl;
		while (!done)
		{
			print();
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		for (u64 i = 0; i < thrds.size(); ++i)
			thrds[i].join();

		print();
		std::cout << std::endl;

		e += eStep;
	}
}

void Paxos_invE_Test(const oc::CLP& cmd)
{

	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 5));
	//double e = cmd.getOr("e", 1.4);
	//double eStep = cmd.getOr("es", 0.01);
	//double eEnd = cmd.getOr("ee", e);
	u64 w = cmd.getOr("w", 3);
	u64 s = cmd.getOr("s", 0);
	u64 v = cmd.getOr("v", cmd.isSet("v") ? 1 : 0);
	u64 g = cmd.getOr("g", 4);
	u64 t = cmd.getOr("t", 1);
	//u64 ts = cmd.getOr("ts", 0);
	//u64 nt = cmd.getOr("nt", std::thread::hardware_concurrency());

	{
		u64 nn = 10;
		Matrix<block> mtx(nn, nn);
		PRNG prng(ZeroBlock);
		prng.get<block>(mtx);

		auto inv = gf128Inv(mtx);

		auto I = gf128Mul(inv, mtx);
		for (u64 i = 0; i < nn; ++i)
		{
			if (I(i, i) != oc::OneBlock)
				throw RTE_LOC;
			for (u64 j = 0; j < nn; ++j)
			{
				if (i != j && I(i, j) != oc::ZeroBlock)
					throw RTE_LOC;
			}

		}
	}

	for (auto dup : { false, true })
	{
		for (u64 i = 0; i < t; ++i)
		{
			Paxos<u64> paxos;
			Matrix<u64> rows(n, w);
			PRNG prng(block(i, s));

			std::vector<block> dense(n);

			if (dup)
				insertDuplicates(paxos, rows, dense, g, prng, PaxosParam::Binary);
			else
				insertCycle(paxos, rows, dense, g, prng, PaxosParam::Binary);


			PointList Hpl(n, paxos.size());
			for (u64 i = 0; i < n; ++i)
				for (u64 j = 0; j < w; ++j)
					Hpl.push_back({ i, rows(i,j) });

			if (v)
				std::cout << "H*\n" << SparseMtx(Hpl) << std::endl;

			auto tri = paxos.getTriangulization();
			auto A = tri.getA();
			auto B = tri.getB();
			auto C = tri.getC();
			auto D = tri.getD();
			auto E = tri.getE();
			auto F = tri.getF();
			if (v)
			{

				std::cout << "H\n" << tri.mH << std::endl;
				std::cout << "A\n" << A << std::endl;
				std::cout << "B\n" << B << std::endl;
				std::cout << "C\n" << C << std::endl;
				std::cout << "D\n" << D << std::endl;
				std::cout << "E\n" << E << std::endl;
				std::cout << "F\n" << F << std::endl;
			}

			auto CInv = C.invert();
			if (CInv.rows() == 0)
				throw RTE_LOC;

			//  D' = -FC^-1A + D
			//  E' = -FC^-1B + E

			auto FCinv = F * CInv;
			auto FCinvB = FCinv * B;
			auto DD = F * CInv * A + D;
			auto EE = FCinvB + E;

			if (v)
			{

				std::cout << "DD\n" << DD << std::endl;
				std::cout << "EE\n" << EE << std::endl;
				std::cout << "FC\n" << FCinv << std::endl;
				std::cout << "FCB\n" << FCinvB << std::endl;
			}

			auto EEInv = EE.invert();
			if (EEInv.rows() == 0)
				throw RTE_LOC;


			//  r <- $
			//  x2' = x2 - D'r - FC^-1 x1
			//  p1 = -E'^-1 x2'
			//  x1' = x1 - A r - B p1
			//  p2 = -C^-1 x1'
			// 
			//  P = | r p2 p1 |

			std::vector<u8> x(n), p(paxos.size());
			prng.get<u8>(x);

			span<u8> r(p.begin(), p.begin() + A.cols());
			span<u8> p1(p.begin() + A.cols(), p.begin() + A.cols() + B.cols());
			span<u8> p2(p.begin() + A.cols() + B.cols(), p.end());

			std::vector<u8> x1(x.begin(), x.begin() + C.cols());
			std::vector<u8> x2(x.begin() + C.cols(), x.end());

			// optionally randomize r (ie p).
			prng.get<u8>(r);

			// x2' = x2 - D' r - FC^-1 x1
			std::vector<u8> xx2(x2.begin(), x2.end());
			DD.multAdd(r, xx2);
			FCinv.multAdd(x1, xx2);

			// p1 = E'^-1 x2'
			EEInv.multAdd(xx2, p1);

			//  x1' = x1 - A r - B p1
			std::vector<u8> xx1(x1.begin(), x1.end());
			A.multAdd(r, xx1);
			B.multAdd(p1, xx1);

			//  p2 = C^-1 x1'
			CInv.multAdd(xx1, p2);

			if (C.mult(p2) != xx1)
				throw RTE_LOC;

			std::vector<u8> yy = tri.mH.mult(p);

			std::vector<u8> yy1(yy.begin(), yy.begin() + C.cols());
			std::vector<u8> yy2(yy.begin() + C.cols(), yy.end());

			if (yy1 != x1)
				throw RTE_LOC;
			if (yy2 != x2)
				throw RTE_LOC;
		}
	}
}


void Paxos_invE_g3_Test(const oc::CLP& cmd)
{
	return;
	//u64 n = 50;
	//double e = cmd.getOr("e", 1.35);
	////double eStep = cmd.getOr("es", 0.01);
	////double eEnd = cmd.getOr("ee", e);
	//u64 w = cmd.getOr("w", 3);
	//u64 s = cmd.getOr("s", 0);
	//u64 v = cmd.getOr("v", cmd.isSet("v") ? 1 : 0);
	//u64 g = cmd.getOr("g", 4);
	//u64 t = cmd.getOr("t", 1);

	////for (u64 i = 0; i < t; ++i)
	//{
	//	Paxos<u64> paxos;

	//	PaxosParam param;
	//	param.mSparseSize = n * 1.4;
	//	param.mDenseSize = 40;
	//	param.mDt = PaxosParam::Binary;
	//	param.mSsp = 40;
	//	param.mWeight = w;
	//	paxos.init(n, param, ZeroBlock);
	//	PRNG prng(block(2, 0));

	//	std::vector<block> dense(n);

	//	oc::Matrix<u64> rows(n, w);
	//	for (u64 i = 0; i < n; ++i)
	//	{
	//		std::set<u64> ss;
	//		while (ss.size() != w)
	//		{
	//			ss.insert(prng.get<u64>() % paxos.mSparseSize);
	//		}

	//		std::copy(ss.begin(), ss.end(), rows[i].begin());
	//	}
	//	prng.get<block>(dense);

	//	paxos.setInput(rows, dense);

	//	//insertDuplicates(paxos, rows, dense, g, prng, PaxosParam::Binary);
	//	//paxos.getFCInv();

	//	std::vector<u64> mainRows, mainCols;
	//	std::vector<std::array<u64, 2>> gapRows;
	//	paxos.triangulate(mainRows, mainCols, gapRows);
	//	auto fcinv = paxos.getFCInv(mainRows, mainCols, gapRows);

	//	auto tri = paxos.getTriangulization();

	//	//std::cout << tri.mH << std::endl;

	//	auto A = tri.getA();
	//	auto B = tri.getB();
	//	auto C = tri.getC();
	//	auto D = tri.getD();
	//	auto E = tri.getE();
	//	auto F = tri.getF();
	//	if (v)
	//	{

	//		std::cout << "H\n" << tri.mH << std::endl;
	//		std::cout << "A\n" << A << std::endl;
	//		std::cout << "B\n" << B << std::endl;
	//		std::cout << "C\n" << C << std::endl;
	//		std::cout << "D\n" << D << std::endl;
	//		std::cout << "E\n" << E << std::endl;
	//		std::cout << "F\n" << F << std::endl;
	//	}

	//	auto CInv = C.invert();
	//	if (CInv.rows() == 0)
	//		throw RTE_LOC;

	//	//  D' = -FC^-1A + D
	//	//  E' = -FC^-1B + E

	//	auto FCinv = F * CInv;
	//	auto FCinvB = FCinv * B;
	//	auto DD = F * CInv * A + D;
	//	auto EE = FCinvB + E;

	//	if (v)
	//	{

	//		std::cout << "DD\n" << DD << std::endl;
	//		std::cout << "EE\n" << EE << std::endl;
	//		std::cout << "FC\n" << FCinv << std::endl;
	//		std::cout << "FCB\n" << FCinvB << std::endl;
	//	}

	//	for (u64 i = 0; i < gapRows.size(); ++i)
	//	{
	//		std::vector<u64> r0(FCinv.mRows[i].begin(), FCinv.mRows[i].end());
	//		std::vector<u64> r1 = fcinv.mMtx[i];
	//		//auto ww = A.cols() + B.cols();
	//		//for (auto& c : r1)
	//		//	c = tri.mPerm.mColPerm.mSrc2Dst[c];// -ww;

	//		std::sort(r1.begin(), r1.end());

	//		if (r0.size() != r1.size())
	//			throw RTE_LOC;

	//		for (u64 j = 0; j < r0.size(); ++j)
	//		{
	//			if (r0[j] != r1[j])
	//				throw RTE_LOC;
	//		}
	//	}

	//	auto EEInv = EE.invert();
	//	if (EEInv.rows() == 0)
	//		throw RTE_LOC;


	//	//  r <- $
	//	//  x2' = x2 - D'r - FC^-1 x1
	//	//  p1 = -E'^-1 x2'
	//	//  x1' = x1 - A r - B p1
	//	//  p2 = -C^-1 x1'
	//	// 
	//	//  P = | r p2 p1 |

	//	std::vector<u8> x(n), p(paxos.size());
	//	prng.get<u8>(x);

	//	span<u8> r(p.begin(), p.begin() + A.cols());
	//	span<u8> p1(p.begin() + A.cols(), p.begin() + A.cols() + B.cols());
	//	span<u8> p2(p.begin() + A.cols() + B.cols(), p.end());

	//	std::vector<u8> x1(x.begin(), x.begin() + C.cols());
	//	std::vector<u8> x2(x.begin() + C.cols(), x.end());

	//	// optionally randomize r (ie p).
	//	prng.get<u8>(r);

	//	// x2' = x2 - D' r - FC^-1 x1
	//	std::vector<u8> xx2(x2.begin(), x2.end());
	//	DD.multAdd(r, xx2);
	//	FCinv.multAdd(x1, xx2);

	//	// p1 = E'^-1 x2'
	//	EEInv.multAdd(xx2, p1);

	//	//  x1' = x1 - A r - B p1
	//	std::vector<u8> xx1(x1.begin(), x1.end());
	//	A.multAdd(r, xx1);
	//	B.multAdd(p1, xx1);

	//	//  p2 = C^-1 x1'
	//	CInv.multAdd(xx1, p2);

	//	if (C.mult(p2) != xx1)
	//		throw RTE_LOC;

	//	std::vector<u8> yy = tri.mH.mult(p);

	//	std::vector<u8> yy1(yy.begin(), yy.begin() + C.cols());
	//	std::vector<u8> yy2(yy.begin() + C.cols(), yy.end());

	//	if (yy1 != x1)
	//		throw RTE_LOC;
	//	if (yy2 != x2)
	//		throw RTE_LOC;
	//}

}


