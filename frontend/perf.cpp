#include "perf.h"
#include "cryptoTools/Network/IOService.h"
#include "volePSI/RsPsi.h"
#include "volePSI/RsCpsi.h"
#include "volePSI/SimpleIndex.h"

#include "libdivide.h"
using namespace oc;
using namespace volePSI;;

void perfMod(oc::CLP& cmd)
{
	auto n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));

	std::vector<u64> vals(n);
	PRNG prng_(oc::ZeroBlock);
	u64 mod = prng_.get<u32>();


	auto rand = [&](oc::span<u64> v) {
		PRNG prng(oc::ZeroBlock);
		prng.get<u64>(v);
	};
	auto check = [&](std::string name) {
		std::vector<u64> v2(n);
		rand(v2);

		for (u64 i = 0; i < n; ++i)
		{
			if (vals[i] != (v2[i] % mod))
			{
				std::cout << name << std::endl;
				throw RTE_LOC;
			}
		}
	};

	auto cRoutine = [&] {
		for (u64 i = 0; i < n; ++i)
			vals[i] = vals[i] % mod;
	};


	//auto avxRoutine = [&] {
	//	auto n32 = n / 32;
	//	auto mod256 = ::_mm256_set1_epi64x(mod);


	//	//row256[0] = _mm256_loadu_si256(llPtr);
	//	//row256[1] = _mm256_loadu_si256(llPtr + 1);
	//	//row256[2] = _mm256_loadu_si256(llPtr + 2);
	//	//row256[3] = _mm256_loadu_si256(llPtr + 3);
	//	//row256[4] = _mm256_loadu_si256(llPtr + 4);
	//	//row256[5] = _mm256_loadu_si256(llPtr + 5);
	//	//row256[6] = _mm256_loadu_si256(llPtr + 6);
	//	//row256[7] = _mm256_loadu_si256(llPtr + 7);

	//	for (u64 i = 0; i < n; i += 32)
	//	{
	//		__m256i* row256 = (__m256i*)&vals[i];
	//		mod256x8(row256, mod256);
	//	}
	//};

	auto libDivRoutine = [&] {

		PaxosHash<u32> hasher;
		hasher.mMods.emplace_back(libdivide::libdivide_u64_gen(mod));
		hasher.mModVals.emplace_back(mod);
		//libdivide::libdivide_u64_t mod2 = libdivide::libdivide_u64_gen(mod);
		//__m256i temp;
		for (u64 i = 0; i < n; i += 32)
		{
			hasher.mod32(&vals[i], 0);
			//__m256i* row256 = (__m256i*) & vals[i];
			//temp = libdivide::libdivide_u64_do_vec256(*row256, &mod2);
			//auto temp64 = (u64*)&temp;
			//vals[i + 0] -= temp64[0] * mod;
			//vals[i + 1] -= temp64[1] * mod;
			//vals[i + 2] -= temp64[2] * mod;
			//vals[i + 3] -= temp64[3] * mod;
		}
	};
	oc::Timer timer;
	rand(vals);
	timer.setTimePoint("start");
	cRoutine();
	timer.setTimePoint("c");

	check("baseline");
	rand(vals);
	timer.setTimePoint("rand");

	//avxRoutine();
	//timer.setTimePoint("avx");

	//check("avx");
	//rand(vals);
	//timer.setTimePoint("rand");

	libDivRoutine();
	timer.setTimePoint("ibdivide");
	check("libDiv");

	std::cout << timer << std::endl;

}


void perfBaxos(oc::CLP& cmd)
{
	auto n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
	auto t = cmd.getOr("t", 1ull);
	//auto rand = cmd.isSet("rand");
	auto v = cmd.getOr("v", cmd.isSet("v") ? 1 : 0);
	auto w = cmd.getOr("w", 3);
	auto ssp = cmd.getOr("ssp", 40);
	auto dt = cmd.isSet("binary") ? PaxosParam::Binary : PaxosParam::GF128;
	auto nt = cmd.getOr("nt", 0);

	//PaxosParam pp(n, w, ssp, dt);
	auto binSize = 1 << cmd.getOr("lbs", 15);
	u64 baxosSize;
	{
		Baxos paxos;
		paxos.init(n, binSize, w, ssp, dt, oc::ZeroBlock);
		baxosSize = paxos.size();
	}
	std::vector<block> key(n), val(n), pax(baxosSize);
	PRNG prng(ZeroBlock);
	prng.get<block>(key);
	prng.get<block>(val);

	Timer timer;
	auto start = timer.setTimePoint("start");
	auto end = start;
	for (u64 i = 0; i < t; ++i)
	{
		Baxos paxos;
		paxos.init(n, binSize, w, ssp, dt, block(i, i));

		//if (v > 1)
		//	paxos.setTimer(timer);

		paxos.solve<block>(key, val, pax, nullptr, nt);
		timer.setTimePoint("s" + std::to_string(i));

		paxos.decode<block>(key, val, pax, nt);

		end = timer.setTimePoint("d" + std::to_string(i));
	}

	if (v)
		std::cout << timer << std::endl;

	auto tt = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / double(1000);
	std::cout << "total " << tt << "ms, e=" << double(baxosSize) / n << std::endl;
}


template<typename T>
void perfBuildRowImpl(oc::CLP& cmd)
{
	auto n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
	u64 maxN = std::numeric_limits<T>::max() - 1;
	auto t = cmd.getOr("t", 1ull);
	//auto rand = cmd.isSet("rand");
	auto v = cmd.isSet("v");
	auto w = cmd.getOr("w", 3);
	auto ssp = cmd.getOr("ssp", 40);
	auto dt = cmd.isSet("binary") ? PaxosParam::Binary : PaxosParam::GF128;

	PaxosParam pp(n, w, ssp, dt);
	//std::cout << "e=" << pp.size() / double(n) << std::endl;
	if (maxN < pp.size())
	{
		std::cout << "n must be smaller than the index type max value. " LOCATION << std::endl;
		throw RTE_LOC;
	}
	std::vector<block> key(n);
	PRNG prng(ZeroBlock);
	prng.get<block>(key);

	Timer timer;
	auto start32 = timer.setTimePoint("start");
	auto end32 = start32;
	oc::Matrix<T> rows(32, w);
	std::vector<block> hash(32);
	for (u64 i = 0; i < t; ++i)
	{
		Paxos<T> paxos;
		paxos.init(n, pp, block(i, i));

		auto k = key.data();
		auto main = n / 32 * 32;
		for (u64 j = 0; j < main; j += 32)
		{
			paxos.mHasher.hashBuildRow32(k + j, rows.data(), hash.data());
		}
		end32 = timer.setTimePoint("32." + std::to_string(i));
	}

	auto tt32 = std::chrono::duration_cast<std::chrono::microseconds>(end32 - start32).count() / double(1000);
	std::cout << "total32 " << tt32 << "ms" << std::endl;


	if (cmd.isSet("single"))
	{

		auto start1 = timer.setTimePoint("start");
		auto end1 = start1;
		for (u64 i = 0; i < t; ++i)
		{
			Paxos<T> paxos;
			paxos.init(n, pp, block(i, i));

			auto k = key.data();
			for (u64 j = 0; j < n; ++j)
			{
				paxos.mHasher.hashBuildRow1(k + j, rows.data(), hash.data());
			}
			end1 = timer.setTimePoint("1." + std::to_string(i));
		}
		auto tt1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1).count() / double(1000);
		std::cout << "total1  " << tt1 << "ms" << std::endl;

	}
	if (v)
		std::cout << timer << std::endl;

}

void perfBuildRow(oc::CLP& cmd)
{
	auto bits = cmd.getOr("b", 16);
	switch (bits)
	{
	case 8:
		perfBuildRowImpl<u8>(cmd);
		break;
	case 16:
		perfBuildRowImpl<u16>(cmd);
		break;
	case 32:
		perfBuildRowImpl<u32>(cmd);
		break;
	case 64:
		perfBuildRowImpl<u64>(cmd);
		break;
	default:
		std::cout << "b must be 8,16,32 or 64. " LOCATION << std::endl;
		throw RTE_LOC;
	}

}
template<typename T>
void perfPaxosImpl(oc::CLP& cmd)
{
	auto n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
	u64 maxN = std::numeric_limits<T>::max() - 1;
	auto t = cmd.getOr("t", 1ull);
	//auto rand = cmd.isSet("rand");
	auto v = cmd.getOr("v", cmd.isSet("v") ? 1 : 0);
	auto w = cmd.getOr("w", 3);
	auto ssp = cmd.getOr("ssp", 40);
	auto dt = cmd.isSet("binary") ? PaxosParam::Binary : PaxosParam::GF128;
	auto cols = cmd.getOr("cols", 0);

	PaxosParam pp(n, w, ssp, dt);
	//std::cout << "e=" << pp.size() / double(n) << std::endl;
	if (maxN < pp.size())
	{
		std::cout << "n must be smaller than the index type max value. " LOCATION << std::endl;
		throw RTE_LOC;
	}

	auto m = cols ? cols : 1;
	std::vector<block> key(n);
	oc::Matrix<block> val(n, m), pax(pp.size(), m);
	PRNG prng(ZeroBlock);
	prng.get<block>(key);
	prng.get<block>(val);

	Timer timer;
	auto start = timer.setTimePoint("start");
	auto end = start;
	for (u64 i = 0; i < t; ++i)
	{
		Paxos<T> paxos;
		paxos.init(n, pp, block(i, i));

		if (v > 1)
			paxos.setTimer(timer);

		if (cols)
		{
			paxos.setInput(key);
			paxos.template encode<block>(val, pax);
			timer.setTimePoint("s" + std::to_string(i));
			paxos.template decode<block>(key, val, pax);
		}
		else
		{

			paxos.template solve<block>(key, oc::span<block>(val), oc::span<block>(pax));
			timer.setTimePoint("s" + std::to_string(i));
			paxos.template decode<block>(key, oc::span<block>(val), oc::span<block>(pax));
		}


		end = timer.setTimePoint("d" + std::to_string(i));
	}

	if (v)
		std::cout << timer << std::endl;

	auto tt = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / double(1000);
	std::cout << "total " << tt << "ms" << std::endl;
}

void perfPaxos(oc::CLP& cmd)
{
	auto bits = cmd.getOr("b", 16);
	switch (bits)
	{
	case 8:
		perfPaxosImpl<u8>(cmd);
		break;
	case 16:
		perfPaxosImpl<u16>(cmd);
		break;
	case 32:
		perfPaxosImpl<u32>(cmd);
		break;
	case 64:
		perfPaxosImpl<u64>(cmd);
		break;
	default:
		std::cout << "b must be 8,16,32 or 64. " LOCATION << std::endl;
		throw RTE_LOC;
	}

}

void perfPSI(oc::CLP& cmd)
{
	auto n = 1ull << cmd.getOr("nn", 10);
	auto t = cmd.getOr("t", 1ull);
	auto mal = cmd.isSet("malicious");
	auto v = cmd.isSet("v") ? cmd.getOr("v", 1) : 0;
	auto nt = cmd.getOr("nt", 1);
	bool fakeBase = cmd.isSet("fakeBase");
	bool noCompress = cmd.isSet("nc");

	// The vole type, default to expand accumulate.
	auto type = oc::DefaultMultType;
#ifdef ENABLE_INSECURE_SILVER
	type = cmd.isSet("useSilver") ? oc::MultType::slv5 : type;
#endif
#ifdef ENABLE_BITPOLYMUL
	type = cmd.isSet("useQC") ? oc::MultType::QuasiCyclic : type;
#endif
	//IOService ios;

	//auto chl0 = Session(ios, "localhost:1212", SessionMode::Server).addChannel();
	//auto chl1 = Session(ios, "localhost:1212", SessionMode::Client).addChannel();
	PRNG prng(ZeroBlock);
	Timer timer, s, r;
	std::cout << "nt " << nt << " fakeBase " << int(fakeBase) << " n " << n << std::endl;
	RsPsiReceiver recv;
	RsPsiSender send;

	if (fakeBase)
	{
		std::vector<std::array<block, 2>> sendBase(128);
		std::vector<block> recvBase(128);
		BitVector recvChoice(128);
		recvChoice.randomize(prng);
		prng.get(sendBase.data(), sendBase.size());
		for (u64 i = 0; i < 128; ++i)
			recvBase[i] = sendBase[i][recvChoice[i]];
		recv.mRecver.mVoleRecver.mOtExtSender.emplace();
		send.mSender.mVoleSender.mOtExtRecver.emplace();
		recv.mRecver.mVoleRecver.mOtExtSender->setBaseOts(recvBase, recvChoice);
		send.mSender.mVoleSender.mOtExtRecver->setBaseOts(sendBase);
		timer.setTimePoint("fakeBase");
	}

	recv.init(n, n, 40, ZeroBlock, mal, nt);
	send.init(n, n, 40, ZeroBlock, mal, nt);

	recv.setMultType(type);
	send.setMultType(type);

	if (noCompress)
	{
		recv.mCompress = false;
		send.mCompress = false;
		recv.mMaskSize = sizeof(block);
		send.mMaskSize = sizeof(block);
	}


	if (cmd.hasValue("bs") || cmd.hasValue("lbs"))
	{
		u64 binSize = cmd.getOr("bs", 1ull << cmd.getOr("lbs", 15));
		recv.mRecver.mBinSize = binSize;
		send.mSender.mBinSize = binSize;
	}

	std::vector<block> recvSet(n), sendSet(n);
	prng.get<block>(recvSet);
	prng.get<block>(sendSet);

	recv.setTimer(r);
	send.setTimer(s);

	auto sockets = cp::LocalAsyncSocket::makePair();

	for (u64 i = 0; i < t; ++i)
	{
		auto p0 = recv.run(recvSet, sockets[0]);
		auto p1 = send.run(sendSet, sockets[1]);
		s.setTimePoint("begin");
		r.setTimePoint("begin");
		timer.setTimePoint("begin");
		auto r = macoro::sync_wait(macoro::when_all_ready(std::move(p0), std::move(p1)));
		try{ std::get<0>(r).result(); } catch(std::exception& e) {std::cout << e.what() << std::endl; }
		try{ std::get<1>(r).result(); } catch(std::exception& e) {std::cout << e.what() << std::endl; }
		timer.setTimePoint("end");

	}
	//auto thrd = std::thread([&] {
	//	timer.setTimePoint("");
	//	timer.setTimePoint("end");

	//	});


	//thrd.join();

	if (v)
	{

		std::cout << timer << std::endl;
		std::cout << sockets[0].bytesSent() << " " << sockets[1].bytesSent() << std::endl;
		if (v > 1)
			std::cout << "s\n" << s << "\nr\n" << r << std::endl;
		//std::cout <<"-------------log--------------------\n" << coproto::getLog() << std::endl;
	}
}

void perfCPSI(oc::CLP& cmd)
{
#ifdef VOLE_PSI_ENABLE_CPSI
	auto n = 1ull << cmd.getOr("nn", 10);
	auto t = cmd.getOr("t", 1ull);
	//auto mal = cmd.isSet("mal");
	auto v = cmd.isSet("v");
	auto nt = cmd.getOr("nt", 0);
	auto socket = cp::LocalAsyncSocket::makePair();


	RsCpsiReceiver recv;
	RsCpsiSender send;
	RsCpsiReceiver::Sharing rs;
	RsCpsiSender::Sharing ss;

	recv.init(n, n, 0, 40, ZeroBlock, nt);
	send.init(n, n, 0, 40, ZeroBlock, nt);


	std::vector<block> recvSet(n), sendSet(n);
	PRNG prng(ZeroBlock);
	prng.get<block>(recvSet);
	prng.get<block>(sendSet);
	Timer timer, s, r;

	recv.setTimer(r);
	send.setTimer(s);

	timer.setTimePoint("");
	for (u64 i = 0; i < t; ++i)
	{
		auto p0 = send.send(sendSet, {}, ss, socket[0]);
		auto p1 = recv.receive(recvSet, rs, socket[1]);

		auto r = macoro::sync_wait(macoro::when_all_ready(std::move(p0), std::move(p1)));
		std::get<0>(r).result();
		std::get<1>(r).result();

		//ev.eval(p0, p1);
	}
	timer.setTimePoint("end");


	if (v)
	{

		std::cout << timer << "\ns\n" << s << "\nr" << r << std::endl;
		std::cout << "comm " << socket[0].bytesSent() << " + " << socket[1].bytesSent()
			<< " = " << (socket[0].bytesSent() + socket[1].bytesSent())
			<< std::endl;

	}
#else
	std::cout << "cpsi disabled. " << std::endl;
#endif
}

void perf(oc::CLP& cmd)
{
	if (cmd.isSet("psi"))
		return perfPSI(cmd);
	if (cmd.isSet("cpsi"))
		perfCPSI(cmd);
	if (cmd.isSet("paxos"))
		perfPaxos(cmd);
	if (cmd.isSet("baxos"))
		perfBaxos(cmd);
	if (cmd.isSet("buildRow"))
		perfBuildRow(cmd);
	if (cmd.isSet("mod"))
		perfMod(cmd);
}



void overflow(CLP& cmd)
{
	auto statSecParam = 40;
	std::vector<std::vector<u64>> sizes;
	for (u64 numBins = 1; numBins <= (1ull << 32); numBins *= 2)
	{
		sizes.emplace_back();
		try {
			for (u64 numBalls = 1; numBalls <= (1ull << 32); numBalls *= 2)
			{
				auto s0 = SimpleIndex::get_bin_size(numBins, numBalls, statSecParam, true);
				sizes.back().push_back(s0);
				std::cout << numBins << " " << numBalls << " " << s0 << std::endl;
			}
		}
		catch (...) {}
	}

	for (u64 i = 0; i < sizes.size(); ++i)
	{
		std::cout << "/*" << i << "*/ {{ ";
		for (u64 j = 0; j < sizes[i].size(); ++j)
		{
			if (j) std::cout << ", ";
			std::cout << std::log2(sizes[i][j]);
		}
		std::cout << " }}," << std::endl;
	}
}