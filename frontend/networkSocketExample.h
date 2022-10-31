#include "cryptoTools/Common/CLP.h"
#include "volePSI/RsPsi.h"
#include <fstream>
#include "coproto/Socket/AsioSocket.h"
#include "volePSI/fileBased.h"

void networkSocketExampleRun(const oc::CLP& cmd)
{
    try {
        bool client = cmd.isSet("client");
        auto ip = cmd.getOr<std::string>("ip", "localhost:1212");

        auto ns = cmd.getOr("ns", 100);
        auto nr = cmd.getOr("nr", 100);
        auto verbose = cmd.isSet("v");

        bool useSilver = cmd.isSet("useSilver");

        std::cout << "connecting as " << (client ? "client" : "server") << " at ip " << ip << std::endl;

        coproto::Socket sock;
        if (cmd.isSet("tls"))
        {
            std::string certDir = cmd.getOr<std::string>("cert", "./thirdparty/libOTe/thirdparty/coproto/tests/cert/");
            std::string CACert = cmd.getOr("CA", certDir + "/ca.cert.pem");
            auto privateKey = cmd.getOr("sk", certDir + "/" + (client ? "client" : "server") + "-0.key.pem");
            auto publicKey = cmd.getOr("pk", certDir + "/" + (client ? "client" : "server") + "-0.cert.pem");

            if (!volePSI::exist(CACert) || !volePSI::exist(privateKey) || !volePSI::exist(privateKey))
            {
                if (!volePSI::exist(CACert))
                    std::cout << "CA cert " << CACert << "does not exist" << std::endl;
                if (!volePSI::exist(privateKey))
                    std::cout << "private key " << privateKey << "does not exist" << std::endl;
                if (!volePSI::exist(privateKey))
                    std::cout << "public key " << publicKey << "does not exist" << std::endl;

                std::cout << "Please correctly set -CA=<path> -sk=<path> -pk=<path> to the CA cert, user private key "
                    << " and public key respectively. Or run the program from the volePSI root directory to use the "
                    << oc::Color::Red << "insecure " << oc::Color::Default
                    << " sample certs/keys that are provided by coproto. " << std::endl;
            }

#ifdef COPROTO_ENABLE_OPENSSL
            boost::asio::ssl::context ctx(client ?
                boost::asio::ssl::context::tlsv13_client :
                boost::asio::ssl::context::tlsv13_server
            );

            ctx.set_verify_mode(
                boost::asio::ssl::verify_peer |
                boost::asio::ssl::verify_fail_if_no_peer_cert);
            ctx.load_verify_file(CACert);
            ctx.use_private_key_file(privateKey, boost::asio::ssl::context::file_format::pem);
            ctx.use_certificate_file(publicKey, boost::asio::ssl::context::file_format::pem);

            sock = coproto::sync_wait(
                client ?
                macoro::make_task(coproto::AsioTlsConnect(ip, coproto::global_io_context(), ctx)) :
                macoro::make_task(coproto::AsioTlsAcceptor(ip, coproto::global_io_context(), ctx))
            );
#else
            throw std::runtime_error("COPROTO_ENABLE_OPENSSL must be define (via cmake) to use TLS sockets. " COPROTO_LOCATION);
#endif
        }
        else
        {
#ifdef COPROTO_ENABLE_BOOST
            sock = coproto::asioConnect(ip, !client);
#else
            throw std::runtime_error("COPROTO_ENABLE_BOOST must be define (via cmake) to use tcp sockets. " COPROTO_LOCATION);
#endif
        }

        std::cout << "connected" << std::endl;


        std::vector<oc::block> set;
        if (cmd.isSet("sender"))
        {
            volePSI::RsPsiSender sender;
            set.resize(ns);

            for (oc::u64 i = 0; i < ns; ++i)
                set[i] = oc::block(0, i);

            sender.setMultType(useSilver ? oc::MultType::slv5 : oc::MultType::QuasiCyclic);
            sender.init(ns, nr, 40, oc::sysRandomSeed(), false, 1, true);

            std::cout << "sender start\n";

            macoro::sync_wait(sender.run(set, sock));

            std::cout << "sender done\n";
        }
        else
        {
            volePSI::RsPsiReceiver recevier;
            recevier.setMultType(useSilver ? oc::MultType::slv5 : oc::MultType::QuasiCyclic);
            set.resize(ns);

            for (oc::u64 i = 0; i < ns; ++i)
                set[i] = oc::block(0, i);

            recevier.init(ns, nr, 40, oc::sysRandomSeed(), false, 1, true);

            std::cout << "recver start\n";
            macoro::sync_wait(recevier.run(set, sock));

            std::cout << "recver done\n";
        }

    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
}


void networkSocketExample(const oc::CLP& cmd)
{
    if (cmd.isSet("sender") || cmd.isSet("recver"))
    {
        networkSocketExampleRun(cmd);
    }
    else
    {
        auto s = cmd;
        s.set("sender");
        s.set("client");
        auto a = std::async([&]() {networkSocketExampleRun(s); });
        networkSocketExampleRun(cmd);
        a.get();
    }
}
