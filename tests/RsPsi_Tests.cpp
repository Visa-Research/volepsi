#include "RsPsi_Tests.h"
#include "volePSI/RsPsi.h"
#include "volePSI/RsCpsi.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Network/Session.h"
#include "cryptoTools/Network/IOService.h"
#include "Common.h"
using namespace oc;
using namespace volePSI;
using coproto::LocalAsyncSocket;

namespace
{
    std::vector<u64> run(PRNG& prng, std::vector<block>& recvSet, std::vector<block> &sendSet, bool mal, u64 nt = 1, bool reduced = false)
    {
        auto sockets = LocalAsyncSocket::makePair();

        RsPsiReceiver recver;
        RsPsiSender sender;

        recver.init(sendSet.size(), recvSet.size(), 40, prng.get(), mal, nt, reduced);
        sender.init(sendSet.size(), recvSet.size(), 40, prng.get(), mal, nt, reduced);

        auto p0 = recver.run(recvSet, sockets[0]); 
        auto p1 = sender.run(sendSet, sockets[1]);

        eval(p0, p1);
        
        return recver.mIntersection;
    }


}

void Psi_Rs_empty_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 13243);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    prng.get(sendSet.data(), sendSet.size());

    auto inter = run(prng, recvSet, sendSet, false);

    if (inter.size())
        throw RTE_LOC;
}


void Psi_Rs_partial_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 13243);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    prng.get(sendSet.data(), sendSet.size());

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
    {
        if (prng.getBit())
        {
            recvSet[i] = sendSet[(i + 312) % n];
            exp.insert(i);
        }
    }

    auto inter = run(prng, recvSet, sendSet, false);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
    {
        std::cout << "exp size " << exp.size() << std::endl;
        std::cout << "act size " << act.size() << std::endl;
        throw RTE_LOC;
    }
}


void Psi_Rs_full_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 13243);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    sendSet = recvSet;

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
        exp.insert(i);

    auto inter = run(prng, recvSet, sendSet, false);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
        throw RTE_LOC;
}



void Psi_Rs_reduced_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 13243);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    sendSet = recvSet;

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
        exp.insert(i);

    auto inter = run(prng, recvSet, sendSet, false, 1, true);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
        throw RTE_LOC;
}


void Psi_Rs_multiThrd_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 13243);
    u64 nt = cmd.getOr("nt", 8);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    sendSet = recvSet;

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
        exp.insert(i);

    auto inter = run(prng, recvSet, sendSet, false, nt);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
        throw RTE_LOC;
}


void Psi_Rs_mal_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 13243);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    prng.get(sendSet.data(), sendSet.size());

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
    {
        if (prng.getBit())
        {
            recvSet[i] = sendSet[(i + 312) % n];
            exp.insert(i);
        }
    }

    auto inter = run(prng, recvSet, sendSet, true);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
        throw RTE_LOC;
}
