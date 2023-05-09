/// Circuit-PSI example.
/// * How to compile: python3 build.py -DVOLE_PSI_ENABLE_BOOST=ON -DVOLE_PSI_ENABLE_CPSI=ON
/// * How to run both sender and receiver in one process: ./out/build/linux/frontend/frontend -cpsiExample -nn 10 -v -both
/// * How to run both sender and receiver in separate processes:
///     ./out/build/linux/frontend/frontend -cpsiExample -nn 10 -v -ip localhost:12123 -r 0
///     ./out/build/linux/frontend/frontend -cpsiExample -nn 10 -v -ip localhost:12123 -r 1

#ifndef VOLEPSI_CPSI_EXAMPLE_H
#define VOLEPSI_CPSI_EXAMPLE_H

#include "volePSI/RsCpsi.h"
#include "coproto/Socket/AsioSocket.h"

using namespace oc;
using namespace volePSI;

void cpsiExampleRun(oc::CLP& cmd)
{
    oc::Timer timer;
    auto n = 1ull << cmd.getOr("nn", 10);
    std::cout << "Running Circuit-PSI with " << n << " elements" << std::endl;

    auto role = (volePSI::Role) cmd.getOr<int>("r", 2);
    u64 num_columns = cmd.getOr<int>("cols", 4); // Number of columns for payload
    auto byteLength = num_columns * sizeof(block);
    auto ip = cmd.getOr<std::string>("ip", "localhost:1212");

    // The statistical security parameter.
    auto ssp = cmd.getOr("ssp", 40);
    auto verbose = cmd.isSet("v");

    if (role != volePSI::Role::Sender && role != volePSI::Role::Receiver)
        throw std::runtime_error("-r tag must be set with value 0 (sender) or 1 (receiver).");

#ifdef COPROTO_ENABLE_BOOST
    coproto::Socket chl;
    chl = coproto::asioConnect(ip, (role == volePSI::Role::Sender));
#else
    throw std::runtime_error("COPROTO_ENABLE_BOOST must be define (via cmake) to use tcp sockets. " COPROTO_LOCATION);
#endif
    u64 their_n;
    macoro::sync_wait(chl.send(n));
    macoro::sync_wait(chl.recv(their_n));
    if (their_n != n)
        throw std::runtime_error("Other party's set size does not match.");
    else
        std::cout << "Set sizes match. Proceeding." << std::endl;

    auto t_start = timer.setTimePoint("");

    u64 their_columns;
    macoro::sync_wait(chl.send(num_columns));
    macoro::sync_wait(chl.recv(their_columns));
    if (their_columns != num_columns)
        throw std::runtime_error("Other party's payload columns size does not match.");
    else
        std::cout << "Num columns match. Proceeding." << std::endl;

    block seed;
    auto seedStr = cmd.getOr<std::string>("seed", "myseed");
    oc::RandomOracle ro(sizeof(block));
    ro.Update(seedStr.data(), seedStr.size());
    ro.Final(seed);
    PRNG prng(seed);

    if (role == Role::Sender)
    {
        RsCpsiSender sender;
        RsCpsiSender::Sharing ss;
        std::vector<block> sendSet(n);
        oc::Matrix<u8> senderValues(sendSet.size(), byteLength);
        std::memcpy(senderValues.data(), sendSet.data(), sendSet.size() * byteLength);

        if (verbose)
            std::cout << "sender start\n";
        sender.init(n, n, byteLength, ssp, seed, 1);
        std::cout << "Init done" << std::endl;
        prng.get<block>(sendSet);

        macoro::sync_wait(sender.send(sendSet, senderValues, ss, chl));
        std::cout << "send done" << std::endl;
    }
    else
    {
        RsCpsiReceiver recv;
        RsCpsiReceiver::Sharing rs;
        std::vector<block> recvSet(n);

        if (verbose)
            std::cout << "receiver start\n";
        recv.init(n, n, byteLength, ssp, seed, 1);
        std::cout << "Init done" << std::endl;
        prng.get<block>(recvSet);

        macoro::sync_wait(recv.receive(recvSet, rs,chl));
        std::cout << "receive done" << std::endl;
    }
    macoro::sync_wait(chl.flush());

    auto t_end = timer.setTimePoint("");
    std::cout << "Bytes sent: " << chl.bytesSent() << std::endl;
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count()
              << "ms\nDone" << std::endl;
}

// This sample is how to run both parties in a single program.
void cpsiExampleBoth(oc::CLP& cmd)
{
    // The set size.
    auto n = 1ull << cmd.getOr("nn", 10);
    std::cout << "Running Circuit-PSI with " << n << " elements" << std::endl;
    auto num_columns = cmd.getOr<int>("cols", 4); // Number of columns for payload
    auto byteLength = num_columns * sizeof(block);
    auto ip = cmd.getOr<std::string>("ip", "localhost:1212");

    // The statistical security parameter.
    auto ssp = cmd.getOr("ssp", 40);
    auto verbose = cmd.isSet("v");

    auto socket = cp::LocalAsyncSocket::makePair();
    oc::Timer timer;

    RsCpsiReceiver recv;
    RsCpsiSender send;
    RsCpsiReceiver::Sharing rs;
    RsCpsiSender::Sharing ss;
    std::vector<block> recvSet(n), sendSet(n);
    oc::Matrix<u8> senderValues(sendSet.size(), byteLength);

    std::memcpy(senderValues.data(), sendSet.data(), sendSet.size() * byteLength);

    Timer s, r;
    recv.setTimer(r);
    send.setTimer(s);

    if (verbose)
        std::cout << "receiver start\n";
    recv.init(n, n, byteLength, ssp, ZeroBlock, 1);
    if (verbose)
        std::cout << "sender start\n";
    send.init(n, n, byteLength, ssp, ZeroBlock, 1);

    PRNG prng(ZeroBlock);
    prng.get<block>(recvSet);
    prng.get<block>(sendSet);
    timer.setTimePoint("");

    if (verbose)
        std::cout << "sender send\n";
    auto p0 = send.send(sendSet, senderValues, ss, socket[0]);

    if (verbose)
        std::cout << "receiver receive\n";
    auto p1 = recv.receive(recvSet, rs, socket[1]);

    auto rr = macoro::sync_wait(macoro::when_all_ready(std::move(p0), std::move(p1)));
    std::get<0>(rr).result();
    std::get<1>(rr).result();
    timer.setTimePoint("end");

    std::cout << timer << "\ns\n" << s << "\nr" << r << std::endl;
    std::cout << "comm " << socket[0].bytesSent() << " + " << socket[1].bytesSent()
              << " = " << (socket[0].bytesSent() + socket[1].bytesSent())
              << std::endl;

    std::cout << "Circuit-PSI done. " << std::endl;
}

void cpsiExample(oc::CLP& cmd)
{
    if (cmd.isSet("both"))
    {
        cpsiExampleBoth(cmd);
    }
    else
    {
        // If the user specified -r, then run that party.
        // Otherwise, run both parties.
        if (cmd.hasValue("r"))
        {
            cpsiExampleRun(cmd);
        }
        else
        {
            auto s = cmd;
            s.setDefault("r", 0);
            cmd.setDefault("r", 1);
            auto a = std::async([&]() { cpsiExampleRun(s); });
            cpsiExampleRun(cmd);
            a.get();
        }
    }
}

#endif //VOLEPSI_CPSI_EXAMPLE_H
