#include "tests/UnitTests.h"
#include "perf.h"

#include "messagePassingExample.h"
#include "networkSocketExample.h"
#include "volePSI/fileBased.h"
#include "tests/Paxos_Tests.h"

int main(int argc, char** argv)
{
    oc::CLP cmd(argc, argv);

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
            << "   -indexSet: output the index set of the intersection instead of the set element itself\n"
            << "   -malicious: run the protocol with malicious security\n"
            << "   -useSilver: run the protocol with the Silver Vole encoder (experimental)\n"
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