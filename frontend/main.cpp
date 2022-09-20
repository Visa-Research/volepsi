#include "tests/UnitTests.h"
#include "tests/Paxos_Tests.h"
#include "cryptoTools/Common/CLP.h"
#include "perf.h"
#include "coproto/Socket/BufferingSocket.h"
#include "volePSI/RsPsi.h"
#include <fstream>


auto communicate(
	macoro::eager_task<>& protocol,
	bool sender,
	coproto::BufferingSocket& sock,
	int& s, int& r, 
	bool verbose)
{
	std::string me = sender ? "sender" : "recver";
	std::string them = !sender ? "sender" : "recver";

	// write any outgoing data to a file me_i.bin where i in the message index.
	auto write = [&]()
	{
		auto b = sock.getOutbound();
		if (b && b->size())
		{
			std::ofstream message;
			auto temp = me + ".tmp";
			auto file = me + "_" + std::to_string(s) + ".bin";
			message.open(temp, std::ios::binary | std::ios::trunc);
			message.write((char*)b->data(), b->size());
			message.close();


			oc::RandomOracle hash(16);
			hash.Update(b->data(), b->size());
			oc::block h; hash.Final(h);
			
			if (verbose)
				std::cout << me << " write " << std::to_string(s) << " " << h << "\n";

			if (rename(temp.c_str(), file.c_str()) != 0)
				std::cout << me << " file renamed failed\n";
			else if(verbose)
				std::cout << me << " file renamed successfully\n";

			++s;
		}

	};

	// write incoming data from a file them_i.bin where i in the message index.
	auto read = [&]() {

		std::ifstream message;
		auto file = them + "_" + std::to_string(r) + ".bin";
		while (message.is_open() == false)
		{
			message.open(file, std::ios::binary);
			if ((message.is_open() == false))
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		auto fsize = message.tellg();
		message.seekg(0, std::ios::end);
		fsize = message.tellg() - fsize;
		message.seekg(0, std::ios::beg);
		std::vector<oc::u8> buff(fsize);
		message.read((char*)buff.data(), fsize);
		message.close();
		std::remove(file.c_str());


		oc::RandomOracle hash(16);
		hash.Update(buff.data(), buff.size());
		oc::block h; hash.Final(h);

		if(verbose)
			std::cout << me << " read " << std::to_string(r) << " " << h << "\n";
		++r;

		sock.processInbound(buff);
	};

	if (!sender)
		write();

	while (protocol.is_ready() == false)
	{
		read();
		write();
	}
}

void example(oc::CLP& cmd)
{
	auto ns = cmd.getOr("ns", 100);
	auto nr = cmd.getOr("nr", 100);
	auto verbose = cmd.isSet("v");

	coproto::BufferingSocket sock;
	std::vector<oc::block> set;
	if (cmd.isSet("sender"))
	{
		volePSI::RsPsiSender sender;
		set.resize(ns);

		for (oc::u64 i = 0; i < ns; ++i)
			set[i] = oc::block(0, i);

		sender.setMultType(oc::MultType::QuasiCyclic);
		sender.init(ns, nr, 40, oc::sysRandomSeed(), false, 1, true);

		if(verbose)
			std::cout << "sender start\n";
		auto protocol = sender.run(set, sock) | macoro::make_eager();

		int s = 0, r = 0;
		communicate(protocol, true, sock, s, r, verbose);

		std::cout << "sender done\n";
	}
	else
	{
		volePSI::RsPsiReceiver recevier;
		recevier.setMultType(oc::MultType::QuasiCyclic);
		set.resize(ns);

		for (oc::u64 i = 0; i < ns; ++i)
			set[i] = oc::block(0, i);

		recevier.init(ns, nr, 40, oc::sysRandomSeed(), false, 1, true);

		if(verbose)
			std::cout << "recver start\n";
		auto protocol = recevier.run(set, sock) | macoro::make_eager();

		int s = 0, r = 0;
		communicate(protocol, false, sock, s, r, verbose);

		std::cout << "recver done\n";
	}
}



int main(int argc, char** argv)
{
	oc::CLP cmd(argc, argv);

	if (cmd.isSet("example"))
	{
		if (cmd.isSet("sender") || cmd.isSet("recver"))
		{
			example(cmd);
		}
		else
		{
			auto s = cmd;
			s.set("sender");
			auto a = std::async([&]() {example(s); });
			example(cmd);
			a.get();
		}
		return 0;
	}
	else if (cmd.isSet("exp"))
	{
		Paxos_experiment(cmd);
		return 0;
	}
	else if (cmd.isSet("perf"))
	{
		perf(cmd);
		return 0;
	}
	else if (cmd.isSet("balls"))
	{
		overflow(cmd);
		return 0;
	}
	else
	{
		cmd.set("u");
		auto r = volePSI_Tests::Tests.runIf(cmd);
		return r == oc::TestCollection::Result::failed;
	}
}