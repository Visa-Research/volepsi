#include "RsCpsi_Tests.h"
#include "volePSI/RsPsi.h"
#include "volePSI/RsCpsi.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Network/Session.h"
#include "cryptoTools/Network/IOService.h"
#include "Common.h"

using coproto::LocalAsyncSocket;
using namespace oc;
using namespace volePSI;

namespace
{

    std::vector<u64> runCpsi(
        PRNG& prng,
        std::vector<block>& recvSet,
        std::vector<block>& sendSet,
        u64 nt = 1,
        ValueShareType type = ValueShareType::Xor)
    {
        auto sockets = LocalAsyncSocket::makePair();


        RsCpsiReceiver recver;
        RsCpsiSender sender;

        auto byteLength = sizeof(block);
        oc::Matrix<u8> senderValues(sendSet.size(), sizeof(block));
        std::memcpy(senderValues.data(), sendSet.data(), sendSet.size() * sizeof(block));

        recver.init(sendSet.size(), recvSet.size(), byteLength, 40, prng.get(), nt);
        sender.init(sendSet.size(), recvSet.size(), byteLength, 40, prng.get(), nt);

        RsCpsiReceiver::Sharing rShare;
        RsCpsiSender::Sharing sShare;

        auto p0 = recver.receive(recvSet, rShare, sockets[0]);
        auto p1 = sender.send(sendSet, senderValues, sShare, sockets[1]);

        eval(p0, p1);
        
        bool failed = false;
        std::vector<u64> intersection;
        for (u64 i = 0; i < recvSet.size(); ++i)
        {
            auto k = rShare.mMapping[i];

            if (rShare.mFlagBits[k] ^ sShare.mFlagBits[k])
            {
                intersection.push_back(i);

                if (type == ValueShareType::Xor)
                {
                    auto rv = *(block*)&rShare.mValues(k, 0);
                    auto sv = *(block*)&sShare.mValues(k, 0);
                    auto act = (rv ^ sv);
                    if (recvSet[i] != act)
                    {
                        if(!failed)
                            std::cout << i << " ext " << recvSet[i] << ", act " << act << " = " << rv << " " << sv << std::endl;
                        failed = true;
                        //throw RTE_LOC;
                    }
                }
                else
                {

                    for (u64 j = 0; j < 4; ++j)
                    {
                        auto rv = (u32*)&rShare.mValues(i, 0);
                        auto sv = (u32*)&sShare.mValues(i, 0);

                        if (recvSet[i].get<u32>(j) != (sv[j] + rv[j]))
                        {
                            throw RTE_LOC;
                        }
                    }
                }
            }
        }


        return intersection;
    }
}


void Cpsi_Rs_empty_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 133);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    prng.get(sendSet.data(), sendSet.size());

    auto inter = runCpsi(prng, recvSet, sendSet);

    if (inter.size())
        throw RTE_LOC;
}


void Cpsi_Rs_partial_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 128);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    prng.get(sendSet.data(), sendSet.size());

    for (u64 i = 0; i < n; ++i)
    {
        sendSet[i].set<u64>(0, i);
        recvSet[i].set<u64>(0, i);
    }

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
    {
        if (prng.getBit())
        {
            recvSet[i] = sendSet[(i + 312) % n];
            exp.insert(i);
        }
    }

    auto inter = runCpsi(prng, recvSet, sendSet);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
    {
        auto rem = exp;
        for (auto a : act)
            rem.erase(a);

        std::cout << "missing " << *rem.begin() << " " << recvSet[*rem.begin()] << std::endl;
        throw RTE_LOC;
    }
}


void Cpsi_Rs_full_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 243);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    sendSet = recvSet;

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
        exp.insert(i);

    auto inter = runCpsi(prng, recvSet, sendSet);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
        throw RTE_LOC;
}

void Cpsi_Rs_full_asym_test(const CLP& cmd)
{
    u64 ns = cmd.getOr("ns", 2432);
    u64 nr = cmd.getOr("nr", 212);
    std::vector<block> recvSet(nr), sendSet(ns);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    prng.SetSeed(ZeroBlock);
    prng.get(sendSet.data(), sendSet.size());

    std::set<u64> exp;
    for (u64 i = 0; i < std::min<u64>(ns,nr); ++i)
        exp.insert(i);

    auto inter = runCpsi(prng, recvSet, sendSet);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
        throw RTE_LOC;
}




void Cpsi_Rs_full_add32_test(const CLP& cmd)
{
    //u64 n = cmd.getOr("n", 13243);
    //std::vector<block> recvSet(n), sendSet(n);
    //PRNG prng(ZeroBlock);
    //prng.get(recvSet.data(), recvSet.size());
    //sendSet = recvSet;

    //std::set<u64> exp;
    //for (u64 i = 0; i < n; ++i)
    //    exp.insert(i);

    //auto inter = runCpsi(prng, recvSet, sendSet, 1, ValueShareType::add32);
    //std::set<u64> act(inter.begin(), inter.end());
    //if (act != exp)
    //    throw RTE_LOC;
}