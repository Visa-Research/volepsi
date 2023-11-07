#include "fileBased.h"
#include "cryptoTools/Crypto/RandomOracle.h"
#include "RsPsi.h"

#include "coproto/Socket/AsioSocket.h"

namespace volePSI
{

    std::ifstream::pos_type filesize(std::ifstream& file)
    {
        auto pos = file.tellg();
        file.seekg(0, std::ios_base::end);
        auto size = file.tellg();
        file.seekg(pos, std::ios_base::beg);
        return size;
    }

    bool hasSuffix(std::string const& value, std::string const& ending)
    {
        if (ending.size() > value.size()) return false;
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    }

    bool isHexBlock(const std::string& buff)
    {
        if (buff.size() != 32)
            return false;
        auto ret = true;
        for (u64 i = 0; i < 32; ++i)
            ret &= (bool)std::isxdigit(buff[i]);
        return ret;
    }

    block hexToBlock(const std::string& buff)
    {
        assert(buff.size() == 32);

        std::array<u8, 16> vv;
        char b[3];
        b[2] = 0;

        for (u64 i = 0; i < 16; ++i)
        {
            b[0] = buff[2 * i + 0];
            b[1] = buff[2 * i + 1];
            vv[15 - i] = (char)strtol(b, nullptr, 16);;
        }
        return oc::toBlock(vv.data());
    }

    std::vector<block> readSet(const std::string& path, FileType ft, bool debug)
    {
        std::vector<block> ret;
        if (ft == FileType::Bin)
        {
            std::ifstream file(path, std::ios::binary | std::ios::in);
            if (file.is_open() == false)
                throw std::runtime_error("failed to open file: " + path);
            auto size = filesize(file);
            if (size % 16)
                throw std::runtime_error("Bad file size. Expecting a binary file with 16 byte elements");

            ret.resize(size / 16);
            file.read((char*)ret.data(), size);
        }
        else if (ft == FileType::Csv)
        {
            // we will use this to hash large inputs
            oc::RandomOracle hash(sizeof(block));

            std::ifstream file(path, std::ios::in);
            if (file.is_open() == false)
                throw std::runtime_error("failed to open file: " + path);
            std::string buffer;
            while (std::getline(file, buffer))
            {
                // if the input is already a 32 char hex 
                // value, just parse it as is.
                if (isHexBlock(buffer))
                {
                    ret.push_back(hexToBlock(buffer));
                }
                else
                {
                    ret.emplace_back();
                    hash.Reset();
                    hash.Update(buffer.data(), buffer.size());
                    hash.Final(ret.back());
                }
            }
        }
        else
        {
            throw std::runtime_error("unknown file type");
        }

        if (debug)
        {
            u64 maxPrint = 40;
            std::unordered_map<block, u64> hashes;
            for (u64 i = 0; i < ret.size(); ++i)
            {
                auto r = hashes.insert({ ret[i], i });
                if (r.second == false)
                {
                    std::cout << "duplicate at index " << i << " & " << r.first->second << std::endl;
                    --maxPrint;

                    if (!maxPrint)
                        break;
                }
            }


            if (maxPrint != 40)
                throw RTE_LOC;
        }

        return ret;
    }

    template<typename InputIterator >
    void counting_sort(InputIterator first, InputIterator last, u64 endIndex)
    {
        using ValueType = typename std::iterator_traits<InputIterator>::value_type;
        std::vector<u64> counts(endIndex);

        for (auto value = first; value < last; ++value) {
            ++counts[*value];
        }

        for (u64 i = 0; i < counts.size(); ++i) {
            ValueType& value = i;
            u64& size = counts[i];
            std::fill_n(first, size, value);
            std::advance(first, size);
        }
    }

    void writeOutput(std::string outPath, FileType ft, const std::vector<u64>& intersection, bool indexOnly, std::string inPath)
    {
        std::ofstream file;

        if (ft == FileType::Bin)
            file.open(outPath, std::ios::out | std::ios::trunc | std::ios::binary);
        else
            file.open(outPath, std::ios::out | std::ios::trunc);

        if (file.is_open() == false)
            throw std::runtime_error("failed to open the output file: " + outPath);

        if (indexOnly)
        {

            if (ft == FileType::Bin)
            {
                file.write((char*)intersection.data(), intersection.size() * sizeof(u64));
            }
            else
            {
                for (auto i : intersection)
                    file << i << "\n";
            }
        }
        else
        {
            //std::set<u64> set(intersection.begin(), intersection.end());
            if (ft == FileType::Bin)
            {
                std::ifstream inFile(inPath, std::ios::binary | std::ios::in);
                if (inFile.is_open() == false)
                    throw std::runtime_error("failed to open file: " + inPath);
                auto size = filesize(inFile);
                if (size % 16)
                    throw std::runtime_error("Bad file size. Expecting a binary file with 16 byte elements");

                auto n = size / 16;
                std::vector<block> fData(n);
                inFile.read((char*)fData.data(), size);
                for (u64 i = 0; i < intersection.size(); ++i)
                {
                    file.write((char*)fData[intersection[i]].data(), sizeof(block));
                }

            }
            else if (ft == FileType::Csv)
            {
                // we will use this to hash large inputs
                oc::RandomOracle hash(sizeof(block));

                std::ifstream inFile(inPath, std::ios::in);
                if (inFile.is_open() == false)
                    throw std::runtime_error("failed to open file: " + inPath);

                u64 size = filesize(inFile);
                std::vector<char> fData(size);
                inFile.read(fData.data(), size);


                std::vector<span<char>> beg;
                auto iter = fData.begin();
                for (u64 i = 0; i < size; ++i)
                {
                    if (fData[i] == '\n')
                    {
                        beg.push_back(span<char>(iter, fData.begin() + i));
                        iter = fData.begin() + i + 1;
                        assert(beg.back().size());
                    }
                }

                if (iter != fData.end())
                    beg.push_back(span<char>(iter, fData.end()));

                for (u64 i = 0; i < intersection.size(); ++i)
                {
                    auto w = beg[intersection[i]];
                    file.write(w.data(), w.size());
                    file << '\n';
                }
            }
            else
            {
                throw std::runtime_error("unknown file type");
            }
        }
    }

    void doFilePSI(const oc::CLP& cmd)
    {
        try {
            
            auto path = cmd.get<std::string>("in");
            auto outPath = cmd.getOr<std::string>("out", path + ".out");
            bool debug = cmd.isSet("debug");
            bool mal = cmd.isSet("malicious");
            bool indexOnly = cmd.isSet("indexSet");
            bool sortOutput = !cmd.isSet("noSort");
            bool tls = cmd.isSet("tls");
            bool quiet = cmd.isSet("quiet");
            bool verbose = cmd.isSet("v");

            block seed;
            if (cmd.hasValue("seed"))
            {
                auto seedStr = cmd.get<std::string>("seed");
                oc::RandomOracle ro(sizeof(block));
                ro.Update(seedStr.data(), seedStr.size());
                ro.Final(seed);
            }
            else
                seed = oc::sysRandomSeed();

            // The vole type, default to expand accumulate.
            auto type = oc::DefaultMultType;
#ifdef ENABLE_INSECURE_SILVER
            type = cmd.isSet("useSilver") ? oc::MultType::slv5 : type;
#endif
#ifdef ENABLE_BITPOLYMUL
            type = cmd.isSet("useQC") ? oc::MultType::QuasiCyclic : type;
#endif

            FileType ft = FileType::Unspecified;
            if (cmd.isSet("bin")) ft = FileType::Bin;
            if (cmd.isSet("csv")) ft = FileType::Csv;
            if (ft == FileType::Unspecified)
            {
                if (hasSuffix(path, ".bin"))
                    ft = FileType::Bin;
                else if (hasSuffix(path, ".csv"))
                    ft = FileType::Csv;
            }
            if (ft == FileType::Unspecified)
                throw std::runtime_error("unknown file extension, must be .csv or .bin or you must specify the -bin or -csv flags.");

            u64 statSetParam = cmd.getOr("ssp", 40);
            auto ip = cmd.getOr<std::string>("ip", "localhost:1212");
            auto r = (Role)cmd.getOr<int>("r", 2);
            if (r != Role::Sender && r != Role::Receiver)
                throw std::runtime_error("-r tag must be set with value 0 (sender) or 1 (receiver).");

            auto isServer = cmd.getOr<int>("server", (int)r);
            if (r != Role::Sender && r != Role::Receiver)
                throw std::runtime_error("-server tag must be set with value 0 or 1.");
            oc::Timer timer;

            if (!quiet)
                std::cout << "reading set... " << std::flush;
            auto readBegin = timer.setTimePoint("");
            std::vector<block> set = readSet(path, ft, debug);
            auto readEnd = timer.setTimePoint("");
            if (!quiet)
                std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(readEnd - readBegin).count() << "ms" << std::endl;


            if (!quiet)
                std::cout << "connecting as " << (tls ? "tls " : "") << (isServer ? "server" : "client") << " at address " << ip << std::flush;
            coproto::Socket chl;
            auto connBegin = timer.setTimePoint("");
            if (tls)
            {
                std::string CACert = cmd.get<std::string>("CA");
                auto privateKey = cmd.get<std::string>("sk");
                auto publicKey = cmd.get<std::string>("pk");

                if (!exist(CACert) || !exist(privateKey) || !exist(privateKey))
                {
                    std::cout << "\n";
                    if (!exist(CACert))
                        std::cout << "CA cert " << CACert << " does not exist" << std::endl;
                    if (!exist(privateKey))
                        std::cout << "private key " << privateKey << " does not exist" << std::endl;
                    if (!exist(publicKey))
                        std::cout << "public key " << publicKey << " does not exist" << std::endl;

                    std::cout << "Please correctly set -CA=<path> -sk=<path> -pk=<path> to the CA cert, user private key "
                        << " and public key respectively." << std::endl;

                    throw std::runtime_error("bad TLS parameter.");
                }

#ifdef COPROTO_ENABLE_OPENSSL
                boost::asio::ssl::context ctx(!isServer ?
                    boost::asio::ssl::context::tlsv13_client :
                    boost::asio::ssl::context::tlsv13_server
                );

                ctx.set_verify_mode(
                    boost::asio::ssl::verify_peer |
                    boost::asio::ssl::verify_fail_if_no_peer_cert);
                ctx.load_verify_file(CACert);
                ctx.use_private_key_file(privateKey, boost::asio::ssl::context::file_format::pem);
                ctx.use_certificate_file(publicKey, boost::asio::ssl::context::file_format::pem);

                chl = coproto::sync_wait(
                    !isServer ?
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
                chl = coproto::asioConnect(ip, isServer);
#else
                throw std::runtime_error("COPROTO_ENABLE_BOOST must be define (via cmake) to use tcp sockets. " COPROTO_LOCATION);
#endif
            }
            auto connEnd = timer.setTimePoint("");
            if (!quiet)
                std::cout << ' ' << std::chrono::duration_cast<std::chrono::milliseconds>(connEnd - connBegin).count()
                << "ms\nValidating set sizes... " << std::flush;

            if (set.size() != cmd.getOr((r == Role::Sender) ? "senderSize" : "receiverSize", set.size()))
                throw std::runtime_error("File does not contain the specified set size.");
            u64 theirSize;
            macoro::sync_wait(chl.send(set.size()));
            macoro::sync_wait(chl.recv(theirSize));

            if (theirSize != cmd.getOr((r != Role::Sender) ? "senderSize" : "receiverSize", theirSize))
                throw std::runtime_error("Other party's set size does not match.");



            auto valEnd = timer.setTimePoint("");
            if (!quiet)
                std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(valEnd - connEnd).count()
                << "ms\nrunning PSI... " << std::flush;

            if (r == Role::Sender)
            {
                RsPsiSender sender;

                sender.mDebug = debug;
                sender.setMultType(type);
                sender.init(set.size(), theirSize, statSetParam, seed, mal, 1);
                macoro::sync_wait(sender.run(set, chl));
                macoro::sync_wait(chl.flush());

                auto psiEnd = timer.setTimePoint("");
                if (!quiet)
                    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(psiEnd - valEnd).count()
                    << "ms\nDone" << std::endl;
            }
            else
            {
                RsPsiReceiver recver;

                recver.mDebug = debug;
                recver.setMultType(type);
                recver.init(theirSize, set.size(), statSetParam, seed, mal, 1);
                macoro::sync_wait(recver.run(set, chl));
                macoro::sync_wait(chl.flush());


                auto psiEnd = timer.setTimePoint("");
                if (!quiet)
                    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(psiEnd - valEnd).count()
                    << "ms\nWriting output to " << outPath << std::flush;

                if (sortOutput)
                    counting_sort(recver.mIntersection.begin(), recver.mIntersection.end(), set.size());

                writeOutput(outPath, ft, recver.mIntersection, indexOnly, path);

                auto outEnd = timer.setTimePoint("");
                if (!quiet)
                    std::cout << " " << std::chrono::duration_cast<std::chrono::milliseconds>(outEnd - psiEnd).count()
                    << "ms\n" << std::flush;


                if (verbose)
                    std::cout << "intesection_size = " << recver.mIntersection.size() << std::endl;
            }

        }
        catch (std::exception& e)
        {
            std::cout << oc::Color::Red << "Exception: " << e.what() << std::endl << oc::Color::Default;

            std::cout << "Try adding command line argument -debug" << std::endl;
        }
    }

}