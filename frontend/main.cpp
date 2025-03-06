#include "tests/UnitTests.h"
#include "perf.h"

#include "messagePassingExample.h"
#include "networkSocketExample.h"
#include "volePSI/fileBased.h"
#include "tests/Paxos_Tests.h"

int main(int argc, char** argv)
{
    oc::CLP cmd(argc, argv);

    //std::ofstream file("./set.csv");
    //for (oc::u64 i = 0; i < 10000000; ++i)
    //    file << std::setfill('0') << std::setw(32) << i << "\n";
    //return 0;
    if (cmd.isSet("in"))
    {
        volePSI::doFilePSI(cmd);
    }
    else if (cmd.isSet("messagePassing"))
    {
        messagePassingExample(cmd);
    }
    else if (cmd.isSet("net"))
    {
        networkSocketExample(cmd);
    }
    else if (cmd.isSet("exp"))
    {
        Paxos_experiment(cmd);
    }
    else if (cmd.isSet("perf"))
    {
        perf(cmd);
    }
    else if (cmd.isSet("balls"))
    {
        overflow(cmd);
    }
    else if(cmd.isSet("u"))
    {
        auto r = volePSI_Tests::Tests.runIf(cmd);
        return r == oc::TestCollection::Result::failed;
    }
    else
    {

        std::cout << oc::Color::Green << "File based PSI Parameters:\n" << oc::Color::Default
            << "   -in <value>: The path to the party's set. Should either be a binary file containing 16 byte elements with a .bin extension. "
            << "Otherwise the path should have a .csv extension and have one element per row, 32 char hex rows are preferred. \n"

            << "   -r <value>: value should be in { 0, 1 } where 0 means PSI sender.\n"

            << "   -out <value>: The output file path. Will be written in the same format as the input. (Default = in || \".out\")\n"
            << "   -quiet: print less info.\n"
            << "   -v: print more info.\n"
            << "   -indexSet: output the index set of the intersection instead of the set element itself\n"
            << "   -noSort: do not require the output to be in the same order and the input (slightly faster).\n"
            << "   -malicious: run the protocol with malicious security\n"
            << "   -useSilver: run the protocol with the Silver Vole encoder (experimental, default is expand accumulate)\n"
            << "   -useQC: run the protocol with the QuasiCyclic Vole encoder (default is expand accumulate)\n"
            << "   -ssp: Statistical Security parameter, default = 40.\n\n"

            << "   -ip <value>: IP address and port of the server = PSI receiver. (Default = localhost:1212)\n"
            << "   -server <value>: Value should be in {0, 1} and indicates if this party should be the IP server. (Default = r)\n"
            << "   -tls: run the protocol with TLS. Must also set -CA,-sk,-pk\n"
            << "   -CA <value>: if tls, then this must be the path to the CA cert file in pem format\n"
            << "   -pk <value>: if tls, then this must be the path to this parties public key cert file in pem format\n"
            << "   -sk <value>: if tls, then this must be the path to this parties private key file in pem format\n\n"

            << "   -bin: Optional flag to always interpret the input file as binary.\n"
            << "   -csv: Optional flag to always interpret the input file as a CSV.\n"
            << "   -receiverSize <value>: An optional parameter to specify the receiver's set size.\n"
            << "   -senderSize <value>: An optional parameter to specify the sender's set size.\n\n"
            
            ;


        std::cout << oc::Color::Green << "Example programs: \n" << oc::Color::Default
            << "   -messagePassing: Runs the message passing example program. This example shows how to manually pass messages between the PSI parties. Same parameters as File base PSI can be used.\n"
            << "   -net: Run the network socket (TCP/IP or TLS) example program. This example shows how to run the protocol on the coproto network socket. Same parameters as File base PSI can be used.\n\n"

            ;


        std::cout << oc::Color::Green << "Benchmark programs: \n" << oc::Color::Default
            << "   -perf: required flag to run benchmarking\n"
            << "   -psi: Run the PSI benchmark.\n"
            << "      -nn <value>: the log2 size of the sets.\n"
            << "      -t <value>: the number of trials.\n"
            << "      -malicious: run with malicious security.\n"
            << "      -v: verbose.\n"
            << "      -nt: number of threads.\n"
            << "      -fakeBase: use fake base OTs.\n"
            << "      -nc: do not compress the OPRF outputs.\n"
            << "      -useSilver: run the protocol with the Silver Vole encoder (experimental, default is expand accumulate)\n"
            << "      -useQC: run the protocol with the QuasiCyclic Vole encoder (default is expand accumulate)\n"
            << "      -bs: the okvs bin size.\n"
            << "   -cpsi: Run the circuit psi benchmark.\n"
            << "      -nn <value>: the log2 size of the sets.\n"
            << "      -t <value>: the number of trials.\n"
            << "      -v: verbose.\n"
            << "      -nt: number of threads.\n"
            << "   -paxos: Run the okvs benchmark.\n"
            << "      -n <value>: The set size. Can also set n using -nn wher n=2^nn.\n"
            << "      -t <value>: the number of trials.\n"
            << "      -b <value>: The bitcount of the index type. Must by a multiple of 8 and greater than 1.3*n.\n"
            << "      -v: verbose.\n"
            << "      -w <value>: The okvs weight.\n"
            << "      -ssp <value>: statistical security parameter.\n"
            << "      -binary: binary okvs dense columns.\n"
            << "      -cols: The size of the okvs elemenst in multiples of 16 bytes. default = 1.\n"
            << "   -baxos: The the bin okvs benchmark. Same parameters as -paxos plus.\n"
            << "      -lbs <value>: the log2 bin size.\n"
            << "      -nt: number of threads.\n"

            ;





        std::cout << oc::Color::Green << "Unit tests: \n" << oc::Color::Default
            << "   -u: Run all of the unit tests.\n"
            << "   -u -list: List run all of the unit tests.\n"
            << "   -u 10 15: Run unit test 10 and 15.\n"
            << "   -u 10..15: Run unit test 10 to 15 (exclusive).\n"
            << "   -u psi: Run unit test that contain \"psi\" is the title.\n\n"
            ;
    }

    return 0;
}
