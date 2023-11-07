#include "cryptoTools/Common/CLP.h"
#include "volePSI/RsPsi.h"
#include <fstream>
#include "coproto/Socket/AsioSocket.h"
#include "volePSI/fileBased.h"

void networkSocketExampleRun(const oc::CLP& cmd)
{
    try {
        auto recver = cmd.get<int>("r");
        bool client = !cmd.getOr("server", recver);
        auto ip = cmd.getOr<std::string>("ip", "localhost:1212");

        auto ns = cmd.getOr("senderSize", 100ull);
        auto nr = cmd.getOr("receiverSize", 100ull);

        // The statistical security parameter.
        auto ssp = cmd.getOr("ssp", 40ull);

        // Malicious Security.
        auto mal = cmd.isSet("malicious");

        // The vole type, default to expand accumulate.
        auto type = oc::DefaultMultType;
#ifdef ENABLE_INSECURE_SILVER
        type = cmd.isSet("useSilver") ? oc::MultType::slv5 : type;
    #endif
#ifdef ENABLE_BITPOLYMUL
        type = cmd.isSet("useQC") ? oc::MultType::QuasiCyclic : type;
#endif

        // use fewer rounds of communication but more computation.
        auto useReducedRounds = cmd.isSet("reducedRounds");


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

            // Require that the both parties verify using their cert.
            ctx.set_verify_mode(
                boost::asio::ssl::verify_peer |
                boost::asio::ssl::verify_fail_if_no_peer_cert);
            ctx.load_verify_file(CACert);
            ctx.use_private_key_file(privateKey, boost::asio::ssl::context::file_format::pem);
            ctx.use_certificate_file(publicKey, boost::asio::ssl::context::file_format::pem);

            // Perform the TCP/IP and TLS handshake.
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
            // Perform the TCP/IP.
            sock = coproto::asioConnect(ip, !client);
#else
            throw std::runtime_error("COPROTO_ENABLE_BOOST must be define (via cmake) to use tcp sockets. " COPROTO_LOCATION);
#endif
        }

        std::cout << "connected" << std::endl;
        std::vector<oc::block> set;
        if (!recver)
        {
            // Use dummy set {0,1,...}
            set.resize(ns);
            for (oc::u64 i = 0; i < ns; ++i)
                set[i] = oc::block(0, i);

            // configure
            volePSI::RsPsiSender sender;
            sender.setMultType(type);
            sender.init(ns, nr, ssp, oc::sysRandomSeed(), mal, 1, useReducedRounds);

            std::cout << "sender start\n";
            auto start = std::chrono::system_clock::now();

            // Run the protocol.
            macoro::sync_wait(sender.run(set, sock));

            auto done = std::chrono::system_clock::now();
            std::cout << "sender done, " << std::chrono::duration_cast<std::chrono::microseconds>(done-start).count() <<"ms" << std::endl;
        }
        else
        {
            // Use dummy set {0,1,...}
            set.resize(nr);
            for (oc::u64 i = 0; i < nr; ++i)
                set[i] = oc::block(0, i);

            // Configure.
            volePSI::RsPsiReceiver recevier;
            recevier.setMultType(type);
            recevier.init(ns, nr, ssp, oc::sysRandomSeed(), mal, 1, useReducedRounds);

            std::cout << "recver start\n";
            auto start = std::chrono::system_clock::now();

            // Run the protocol.
            macoro::sync_wait(recevier.run(set, sock));

            auto done = std::chrono::system_clock::now();
            std::cout << "sender done, " << std::chrono::duration_cast<std::chrono::microseconds>(done-start).count() <<"ms" << std::endl;
        }

    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
}


void networkSocketExample(const oc::CLP& cmd)
{
    // If the user specified -sender or -recver, then run that party.
    // Otherwisew run both parties.
    if (cmd.isSet("sender") || cmd.isSet("recver"))
    {
        networkSocketExampleRun(cmd);
    }
    else
    {
        auto s = cmd;
        s.set("sender");
        s.set("server");
        auto a = std::async([&]() {networkSocketExampleRun(s); });
        networkSocketExampleRun(cmd);
        a.get();
    }
}
