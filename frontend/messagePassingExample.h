#include "cryptoTools/Common/CLP.h"
#include "coproto/Socket/BufferingSocket.h"
#include "volePSI/RsPsi.h"
#include <fstream>

// This example demonstates how one can get and manually send the protocol messages
// that are generated. This communicate method is one possible way of doing this.
// It takes a protocol that has been started and coproto buffering socket as input.
// It alternates between "sending" and "receiving" protocol messages. Instead of
// sending the messages on a socket, this program writes them to a file and the other
// party reads that file to get the message. In a real program the communication could 
// handled in any way the user decides.
auto communicate(
    macoro::eager_task<>& protocol,
    bool sender,
    coproto::BufferingSocket& sock,
    bool verbose)
{

    int s = 0, r = 0;
    std::string me = sender ? "sender" : "recver";
    std::string them = !sender ? "sender" : "recver";

    // write any outgoing data to a file me_i.bin where i in the message index.
    auto write = [&]()
    {
        // the the outbound messages that the protocol has generated.
        // This will consist of all the outbound messages that can be 
        // generated without receiving the next inbound message.
        auto b = sock.getOutbound();

        // If we do have outbound messages, then lets write them to a file.
        if (b && b->size())
        {
            std::ofstream message;
            auto temp = me + ".tmp";
            auto file = me + "_" + std::to_string(s) + ".bin";
            message.open(temp, std::ios::binary | std::ios::trunc);
            message.write((char*)b->data(), b->size());
            message.close();

            if (verbose)
            {
                // optional for debug purposes.
                oc::RandomOracle hash(16);
                hash.Update(b->data(), b->size());
                oc::block h; hash.Final(h);

                std::cout << me << " write " << std::to_string(s) << " " << h << "\n";
            }

            if (rename(temp.c_str(), file.c_str()) != 0)
                std::cout << me << " file renamed failed\n";
            else if (verbose)
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

        if (verbose)
        {
            oc::RandomOracle hash(16);
            hash.Update(buff.data(), buff.size());
            oc::block h; hash.Final(h);

            std::cout << me << " read " << std::to_string(r) << " " << h << "\n";
        }
        ++r;

        // This gives this socket the message which forwards it to the protocol and
        // run the protocol forward, possibly generating more outbound protocol
        // messages.
        sock.processInbound(buff);
    };

    // The sender we generate the first message.
    if (sender)
        write();

    // While the protocol is not done we alternate between reading and writing messages.
    while (protocol.is_ready() == false)
    {
        read();
        write();
    }
}

void messagePassingExampleRun(oc::CLP& cmd)
{
    auto receiver = cmd.get<int>("r");

    // The sender set size.
    auto ns = cmd.getOr("senderSize", 100ull);

    // The receiver set size.
    auto nr = cmd.getOr("receiverSize", 100ull);
    auto verbose = cmd.isSet("v");

    // The statistical security parameter.
    auto ssp = cmd.getOr("ssp", 40);

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

    // A buffering socket. This socket type internally buffers the 
    // protocol messages. It is then up to the user to manually send
    // and receive messages via the getOutbond(...) and processInbount(...)
    // methods.
    coproto::BufferingSocket sock;

    // Sets are always represented as 16 byte values. To support longer elements one can hash them.
    std::vector<oc::block> set;
    if (!receiver)
    {
        volePSI::RsPsiSender sender;
        set.resize(ns);

        // in this example we use the set {0,1,...}.
        for (oc::u64 i = 0; i < ns; ++i)
            set[i] = oc::block(0, i);

        sender.setMultType(type);
        sender.init(ns, nr, ssp, oc::sysRandomSeed(), mal, 1, useReducedRounds);

        if (verbose)
            std::cout << "sender start\n";

        // Eagerly start the protocol. This will run the protocol up to the point
        // that it need to receive a message from the other party.
        auto protocol = sender.run(set, sock) | macoro::make_eager();

        // Perform the communication and complete the protocol.
        communicate(protocol, true, sock, verbose);

        std::cout << "sender done\n";
    }
    else
    {
        // in this example we use the set {0,1,...}.
        set.resize(nr);
        for (oc::u64 i = 0; i < nr; ++i)
            set[i] = oc::block(0, i);

        volePSI::RsPsiReceiver recevier;
        recevier.setMultType(type);
        recevier.init(ns, nr, ssp, oc::sysRandomSeed(), mal, 1, useReducedRounds);

        if (verbose)
            std::cout << "recver start\n";


        // Eagerly start the protocol. This will run the protocol up to the point
        // that it need to receive a message from the other party.
        auto protocol = recevier.run(set, sock) | macoro::make_eager();

        // Perform the communication and complete the protocol.
        communicate(protocol, false, sock, verbose);

        std::cout << "recver done " << recevier.mIntersection.size() << " \n";
    }
}

// This sample is how to run both parties in  single program.
void messagePassingExampleBoth(oc::CLP& cmd)
{
    // The sender set size.
    auto ns = cmd.getOr("senderSize", 100);

    // The receiver set size.
    auto nr = cmd.getOr("receiverSize", 100);
    auto verbose = cmd.isSet("v");

    // The statistical security parameter.
    auto ssp = cmd.getOr("ssp", 40);

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

    // A buffering socket. This socket type internally buffers the 
    // protocol messages. It is then up to the user to manually send
    // and receive messages via the getOutbond(...) and processInbount(...)
    // methods.
    coproto::BufferingSocket senderSock, recverSock;

    // Sets are always represented as 16 byte values. To support longer elements one can hash them.
    std::vector<oc::block> senderSet, recverSet;
    volePSI::RsPsiSender sender;
    senderSet.resize(ns);

    // in this example we use the set {0,1,...}.
    for (oc::u64 i = 0; i < ns; ++i)
        senderSet[i] = oc::block(0, i);

    sender.setMultType(type);
    sender.init(ns, nr, ssp, oc::sysRandomSeed(), mal, 1, useReducedRounds);

    if (verbose)
        std::cout << "sender start\n";

    // Eagerly start the protocol. This will run the protocol up to the point
    // that it need to receive a message from the other party.
    coproto::optional<macoro::eager_task<>> senderProto =
        sender.run(senderSet, senderSock) | macoro::make_eager();


    // in this example we use the set {0,1,...}.
    recverSet.resize(nr);
    for (oc::u64 i = 0; i < nr; ++i)
        recverSet[i] = oc::block(0, i);

    volePSI::RsPsiReceiver recevier;
    recevier.setMultType(type);
    recevier.init(ns, nr, ssp, oc::sysRandomSeed(), mal, 1, useReducedRounds);

    if (verbose)
        std::cout << "recver start\n";


    // Eagerly start the protocol. This will run the protocol up to the point
    // that it need to receive a message from the other party.
    coproto::optional<macoro::eager_task<>> recverProto =
        recevier.run(recverSet, recverSock) | macoro::make_eager();

    // pass messages between the parties until they are done.
    try
    {
        while (recverProto || senderProto)
        {
            if (recverProto && recverProto->is_ready())
            {
                // get the result of the protocol. Might throw.
                coproto::sync_wait(*recverProto);
                std::cout << "recver done " << recevier.mIntersection.size() << " \n";
                recverProto.reset();
            }

            if (senderProto && senderProto->is_ready())
            {
                // get the result of the protocol. Might throw.
                coproto::sync_wait(*senderProto);
                std::cout << "sender done " << " \n";
                senderProto.reset();
            }
            coproto::optional<std::vector<oc::u8>> recverMsg = recverSock.getOutbound();
            if (recverMsg.has_value())
                senderSock.processInbound(*recverMsg);

            coproto::optional<std::vector<oc::u8>> senderMsg = senderSock.getOutbound();
            if (senderMsg)
                recverSock.processInbound(*senderMsg);
        }

    }
    catch (std::exception& e)
    {
        std::cout << "Exception: " << e.what() << std::endl;
    }
}


void messagePassingExample(oc::CLP& cmd)
{
    if (cmd.isSet("both"))
    {

        messagePassingExampleBoth(cmd);
    }
    else
    {


        // If the user specified -r, then run that party.
        // Otherwisew run both parties.
        if (cmd.hasValue("r"))
        {
            messagePassingExampleRun(cmd);
        }
        else
        {
            auto s = cmd;
            s.setDefault("r", 0);
            cmd.setDefault("r", 1);
            auto a = std::async([&]() {messagePassingExampleRun(s); });
            messagePassingExampleRun(cmd);
            a.get();
        }
    }
}

