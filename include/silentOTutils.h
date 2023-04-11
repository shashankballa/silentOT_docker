#include <libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h>
#include <libOTe/TwoChooseOne/Silent/SilentOtExtSender.h>
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtReceiver.h>
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtSender.h>
#include <libOTe/Tools/LDPC/LdpcImpulseDist.h>
#include <libOTe/Tools/LDPC/Util.h>
#include <libOTe_Tests/Common.h>
#include <cryptoTools/Common/TestCollection.h>
#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Crypto/RandomOracle.h>
#include <coproto/Socket/AsioSocket.h>
#include <coproto/Socket/BufferingSocket.h>

#include <iomanip>

#include <Millionaire/millionaire.h>

using namespace osuCrypto;
using namespace std;
using namespace tests_libOTe;

template<typename Choice>
void checkCorrelated( span<block> Ar, span<block> Bs,
    Choice& choice, block delta, u64 n, bool verbose,
    ChoiceBitPacking packing)
{

    if (Ar.size() != n)
        throw RTE_LOC;
    if (Bs.size() != n)
        throw RTE_LOC;
    if (packing == ChoiceBitPacking::False &&
        (u64)choice.size() != n)
        throw RTE_LOC;
    bool passed = true;
    //bool first = true;
    block mask = AllOneBlock ^ OneBlock;


    for (u64 i = 0; i < n; ++i)
    {
        block m1 = Ar[i];
        block m2a = Bs[i];
        block m2b = (Bs[i] ^ delta);
        u8 c, c2;

        if (packing == ChoiceBitPacking::True)
        {
            c = u8((m1 & OneBlock) == OneBlock) & 1;
            m1 = m1 & mask;
            m2a = m2a & mask;
            m2b = m2b & mask;

            if (choice.size())
            {
                c2 = choice[i];

                if (c2 != c)
                    throw RTE_LOC;
            }
        }
        else
        {
            c = choice[i];
        }

        std::array<bool, 2> eqq{
            eq(m1, m2a),
            eq(m1, m2b)
        };

        bool good = true;
        if (eqq[c] == false || eqq[c ^ 1] == true)
        {
            good = passed = false;
            //if (verbose)
            std::cout << Color::Pink;
        }
        if (eqq[0] == false && eqq[1] == false)
        {
            good = passed = false;
            //if (verbose)
            std::cout << Color::Red;
        }

        if (!good /*&& first*/)
        {
            //first = false;
            std::cout << i << " m " << mask << std::endl;
            std::cout << "r " << m1 << " " << int(c) << std::endl;
            std::cout << "s " << m2a << " " << m2b << std::endl;
            std::cout << "d " << (m1 ^ m2a) << " " << (m1 ^ m2b) << std::endl;
        }

        std::cout << Color::Default;
    }

    if (passed == false)
        throw RTE_LOC;
    else
        if(verbose)
            std::cout << "checkCorrelated: passed!" << std::endl;
}

void checkRandom(span<block> messages, span<std::array<block, 2>>messages2,
    BitVector& choice, u64 n, bool verbose)
{

    if (messages.size() != n)
        throw RTE_LOC;
    if (messages2.size() != n)
        throw RTE_LOC;
    if (choice.size() != n)
        throw RTE_LOC;
    bool passed = true;

    for (u64 i = 0; i < n; ++i)
    {
        block m1 = messages[i];
        block m2a = messages2[i][0];
        block m2b = (messages2[i][1]);
        u8 c = choice[i];


        std::array<bool, 2> eqq{
            eq(m1, m2a),
            eq(m1, m2b)
        };
        if (eqq[c ^ 1] == true)
        {
            passed = false;
            if (verbose)
                std::cout << Color::Pink;
        }
        if (eqq[0] == false && eqq[1] == false)
        {
            passed = false;
            if (verbose)
                std::cout << Color::Red;
        }

        if (eqq[c] == false && verbose)
            std::cout << "m" << i << " " << m1 << " != (" << m2a << " " << m2b << ")_" << (int)c << "\n";

    }

    if (passed == false)
        throw RTE_LOC;
    else
        if(verbose)
            std::cout << "checkRandom: passed!" << std::endl;
}

void checkChosen(BitVector& choiceBits, span<block> recv, 
    span<std::array<block, 2>>  sender)
{
    for (u64 i = 0; i < choiceBits.size(); ++i)
    {

        u8 choice = choiceBits[i];
        const block& revcBlock = recv[i];
        //(i, choice, revcBlock);
        const block& senderBlock = sender[i][choice];

        if (i%512==0) {
           std::cout << "[" << i << ",0]--" << sender[i][0] << std::endl;
           std::cout << "[" << i << ",1]--" << sender[i][1] << std::endl;
           std::cout << "recvd: " << (int)choice << " -- " << recv[i] << std::endl;
        }

        if (revcBlock == ZeroBlock)
            throw RTE_LOC;

        if (neq(revcBlock, senderBlock))
            throw UnitTestFail();

        if (eq(revcBlock, sender[i][1 ^ choice]))
            throw UnitTestFail();
    }
}

void silent_rot_recv(std::vector<block> &messages, BitVector &choices, 
    std::string ip, PRNG &prng, u64 numThreads, bool verbose = false)
{
    std::string tag = "silent_rot_recv";
    u64 numOTs = messages.size();

    auto malicious = SilentSecType::SemiHonest;
    auto multType  = MultType::slv5;
        
    // get up the networking
    auto chl = cp::asioConnect(ip, false);

    macoro::thread_pool threadPool;
    auto work = threadPool.make_work();
    if (numThreads > 1)
        threadPool.create_threads(numThreads);

    Timer timer;
    auto start = timer.setTimePoint("start");

    SilentOtExtReceiver recver;

    // optionally request the LPN encoding matrix.
    recver.mMultType = multType;

    // configure the sender. optional for semi honest security...
    recver.configure(numOTs, 2, numThreads, malicious);

    // optional. You can request that the base ot are generated either
    // using just base OTs (few rounds, more computation) or 128 base OTs and then extend those. 
    // The default is the latter, base + extension.
    // type == SilentBaseType::BaseExtend
    cp::sync_wait(recver.genSilentBaseOts(prng, chl, true));

    // std::vector<block> messages(numOTs);
    // BitVector choices(numOTs);

    // create the protocol object.
    auto protocol = recver.silentReceive(choices, messages, prng, chl);

    // run the protocol
    if (numThreads <= 1)
        cp::sync_wait(protocol);
    else
        // launch the protocol on the thread pool.
        cp::sync_wait(std::move(protocol) | macoro::start_on(threadPool));

    // choices, messages has been populated with random OT messages.
    // messages[i] = sender.message[i][choices[i]]
    // See the header for other options.
    
    auto end = timer.setTimePoint("end");
    auto milli = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    u64 com = chl.bytesReceived() + chl.bytesSent();

    if (verbose)
    {
        std::string typeStr = "be ";
        lout << tag <<
        " n:" << Color::Green << std::setw(6) << std::setfill(' ') << numOTs << Color::Default <<
        " type: " << Color::Green << typeStr << Color::Default <<
        "   ||   " << Color::Green <<
        std::setw(6) << std::setfill(' ') << milli << " ms   " <<
        std::setw(6) << std::setfill(' ') << com << " bytes" << std::endl << Color::Default;

        lout << gTimer << std::endl;
        lout << " **** receiver ****\n" << timer << std::endl;
    }
}

void silent_rot_send(std::vector<std::array<block, 2>> &messages, 
    std::string ip, PRNG &prng, u64 numThreads, bool verbose = false)
{
    std::string tag = "silent_rot_send";
    u64 numOTs = messages.size();

    auto malicious = SilentSecType::SemiHonest;
    auto multType = MultType::slv5;

    std::vector<SilentBaseType> types;
    types.push_back(SilentBaseType::BaseExtend);
        
    // get up the networking
    auto chl = cp::asioConnect(ip, true);

    macoro::thread_pool threadPool;
    auto work = threadPool.make_work();
    if (numThreads > 1)
        threadPool.create_threads(numThreads);

    Timer timer;
    auto start = timer.setTimePoint("start");

    SilentOtExtSender sender;

    // optionally request the LPN encoding matrix.
    sender.mMultType = multType;

    // optionally configure the sender. default is semi honest security.
    sender.configure(numOTs, 2, numThreads, malicious);

    // optional. You can request that the base ot are generated either
    // using just base OTs (few rounds, more computation) or 128 base OTs and then extend those. 
    // The default is the latter, base + extension.
    // type == SilentBaseType::BaseExtend
    cp::sync_wait(sender.genSilentBaseOts(prng, chl, true));

    // std::vector<std::array<block, 2>> messages(numOTs);

    // create the protocol object.
    auto protocol = sender.silentSend(messages, prng, chl);

/*
    chain of functions from silentSend to LPN with slv5 codes:
    silentSend
        silentSendInplace
            configure
            genSilentBaseOts
            expand - pprf (ggm tree) expansion
            ferretMalCheck - only for malicious
            compress
                mEncoder.dualEncode
                    mR.template dualEncode<T>
                    mL.template dualEncode<T>
        hash
*/

    // run the protocol
    if (numThreads <= 1)
        cp::sync_wait(protocol);
    else
        // launch the protocol on the thread pool.
        cp::sync_wait(std::move(protocol) | macoro::start_on(threadPool));

    // messages has been populated with random OT messages.
    // See the header for other options.

    auto end = timer.setTimePoint("end");
    auto milli = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    u64 com = chl.bytesReceived() + chl.bytesSent();

    if (verbose)
    {
        std::string typeStr = "be ";
        lout << tag <<
        " n:" << Color::Green << std::setw(6) << std::setfill(' ') << numOTs << Color::Default <<
        " type: " << Color::Green << typeStr << Color::Default <<
        "   ||   " << Color::Green <<
        std::setw(6) << std::setfill(' ') << milli << " ms   " <<
        std::setw(6) << std::setfill(' ') << com << " bytes" << std::endl << Color::Default;

        lout << gTimer << std::endl;
        lout << " **** sender ****\n" << timer << std::endl;
    }
}

void silent_rot_test(CLP& cmd)
{
    auto numOTs = cmd.isSet("nn")
        ? (1 << cmd.get<int>("nn"))
        : cmd.getOr("n", 0);
    auto numThreads = cmd.getOr("t", 4);
    auto ip = cmd.getOr<std::string>("ip", "localhost:1212");
    bool verbose = (cmd.getOr("v", 1) >= 1);

    if (numOTs == 0)
    numOTs = 1 << 20;

    PRNG prng(sysRandomSeed());

    std::vector<std::array<block, 2>> messages_s(numOTs);
    std::vector<block> messages_r(numOTs);
    BitVector choices(numOTs);
    choices.randomize(prng);
    choices[0] = 1;

    BitVector choices_old = choices;

    auto thrd = std::thread([&] {
        try { silent_rot_send(messages_s, ip, prng, numThreads, verbose); }
        catch (std::exception& e)
        {
            lout << e.what() << std::endl;
        }
        });

    try { silent_rot_recv(messages_r, choices, ip, prng, numThreads, verbose); }
    catch (std::exception& e)
    {
        lout << e.what() << std::endl;
    }
    thrd.join();

    std::cout << "After ROT:"<< std::endl;
    std::cout << "+ messages_s[0][0] -> "     <<  messages_s[0][0] << std::endl;
    std::cout << "+ messages_s[0][1] -> "     <<  messages_s[0][1] << std::endl;
    std::cout << "+ choices[0]       -> "     <<  choices[0]       << std::endl;
    std::cout << "+ messages_r[0]    -> "     <<  messages_r[0]    << std::endl;

    std::cout << "+ choices_old == choices? " << std ::endl;
    std::cout << "-+ " << std::boolalpha << (choices_old==choices) << std ::endl;
    std::cout << "-+ choices_old[:4] : "      << choices_old[0]    << choices_old[1] 
              << choices_old[2]               << choices_old[3]    << std ::endl;
    std::cout << "-+ choices[:4]     : "      << choices[0]        << choices[1]
              << choices[2]                   << choices[3]        << std ::endl;
    std::cout << std ::endl;
    
    checkRandom(messages_r, messages_s, choices, numOTs, verbose);
}

void silent_cot_recv(std::vector<block> &messages, BitVector &choices, 
    std::string ip, PRNG &prng, u64 numThreads, bool verbose = false)
{
    std::string tag = "silent_cot_recv";
    u64 numOTs = messages.size();

    auto malicious = SilentSecType::SemiHonest;
    auto multType  = MultType::slv5;
        
    // get up the networking
    auto chl = cp::asioConnect(ip, false);

    macoro::thread_pool threadPool;
    auto work = threadPool.make_work();
    if (numThreads > 1)
        threadPool.create_threads(numThreads);

    Timer timer;
    auto start = timer.setTimePoint("start");

    SilentOtExtReceiver recver;

    // optionally request the LPN encoding matrix.
    recver.mMultType = multType;

    // configure the sender. optional for semi honest security...
    recver.configure(numOTs, 2, numThreads, malicious);

    // optional. You can request that the base ot are generated either
    // using just base OTs (few rounds, more computation) or 128 base OTs and then extend those. 
    // The default is the latter, base + extension.
    // type == SilentBaseType::BaseExtend
    cp::sync_wait(recver.genSilentBaseOts(prng, chl, true));

    // std::vector<block> messages(numOTs);
    // BitVector choices(numOTs);

    // create the protocol object.
    auto protocol = recver.silentReceive(choices, messages, prng, chl, OTType::Correlated);

    // run the protocol
    if (numThreads <= 1)
        cp::sync_wait(protocol);
    else
        // launch the protocol on the thread pool.
        cp::sync_wait(std::move(protocol) | macoro::start_on(threadPool));

    // choices, messages has been populated with random OT messages.
    // messages[i] = sender.message[i][choices[i]]
    // See the header for other options.
    
    auto end = timer.setTimePoint("end");
    auto milli = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    u64 com = chl.bytesReceived() + chl.bytesSent();

    if (verbose)
    {
        std::string typeStr = "be ";
        lout << tag <<
        " n:" << Color::Green << std::setw(6) << std::setfill(' ') << numOTs << Color::Default <<
        " type: " << Color::Green << typeStr << Color::Default <<
        "   ||   " << Color::Green <<
        std::setw(6) << std::setfill(' ') << milli << " ms   " <<
        std::setw(6) << std::setfill(' ') << com << " bytes" << std::endl << Color::Default;

        lout << gTimer << std::endl;
        lout << " **** receiver ****\n" << timer << std::endl;
    }
}

void silent_cot_send(std::vector<block> &messages, block delta, 
    std::string ip, PRNG &prng, u64 numThreads, bool verbose = false)
{
    std::string tag = "silent_cot_send";
    u64 numOTs = messages.size();

    auto malicious = SilentSecType::SemiHonest;
    auto multType = MultType::slv5;

    std::vector<SilentBaseType> types;
    types.push_back(SilentBaseType::BaseExtend);
        
    // get up the networking
    auto chl = cp::asioConnect(ip, true);

    macoro::thread_pool threadPool;
    auto work = threadPool.make_work();
    if (numThreads > 1)
        threadPool.create_threads(numThreads);

    Timer timer;
    auto start = timer.setTimePoint("start");

    SilentOtExtSender sender;

    // optionally request the LPN encoding matrix.
    sender.mMultType = multType;

    // optionally configure the sender. default is semi honest security.
    sender.configure(numOTs, 2, numThreads, malicious);

    // optional. You can request that the base ot are generated either
    // using just base OTs (few rounds, more computation) or 128 base OTs and then extend those. 
    // The default is the latter, base + extension.
    // type == SilentBaseType::BaseExtend
    cp::sync_wait(sender.genSilentBaseOts(prng, chl, true));

    // std::vector<std::array<block, 2>> messages(numOTs);

    // create the protocol object.
    auto protocol = sender.silentSend(delta, messages, prng, chl);

    // run the protocol
    if (numThreads <= 1)
        cp::sync_wait(protocol);
    else
        // launch the protocol on the thread pool.
        cp::sync_wait(std::move(protocol) | macoro::start_on(threadPool));

    // messages has been populated with random OT messages.
    // See the header for other options.

    auto end = timer.setTimePoint("end");
    auto milli = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    u64 com = chl.bytesReceived() + chl.bytesSent();

    if (verbose)
    {
        std::string typeStr = "be ";
        lout << tag <<
        " n:" << Color::Green << std::setw(6) << std::setfill(' ') << numOTs << Color::Default <<
        " type: " << Color::Green << typeStr << Color::Default <<
        "   ||   " << Color::Green <<
        std::setw(6) << std::setfill(' ') << milli << " ms   " <<
        std::setw(6) << std::setfill(' ') << com << " bytes" << std::endl << Color::Default;

        lout << gTimer << std::endl;
        lout << " **** sender ****\n" << timer << std::endl;
    }
}

void silent_cot_test(CLP& cmd)
{
    auto numOTs = cmd.isSet("nn")
        ? (1 << cmd.get<int>("nn"))
        : cmd.getOr("n", 0);
    auto numThreads = cmd.getOr("t", 4);
    auto ip = cmd.getOr<std::string>("ip", "localhost:1212");
    bool verbose = (cmd.getOr("v", 1) >= 1);

    if (numOTs == 0)
    numOTs = 1 << 20;

    PRNG prng(sysRandomSeed());

    block delta = prng.get();

    auto ot_type = OTType::Correlated;

    std::vector<block> messages_s(numOTs);
    
    std::vector<block> messages_r(numOTs);
    BitVector choices(numOTs);
    choices.randomize(prng);
    choices[0] = 1;

    BitVector choices_old = choices;

    auto thrd = std::thread([&] {
        try { silent_cot_send(messages_s, delta, ip, prng, numThreads, verbose); }
        catch (std::exception& e)
        {
            lout << e.what() << std::endl;
        }
        });

    try { silent_cot_recv(messages_r, choices, ip, prng, numThreads, verbose); }
    catch (std::exception& e)
    {
        lout << e.what() << std::endl;
    }
    thrd.join();

    std::cout << "After COT:"<< std::endl;
    std::cout << "+ messages_s[0]       -> "  <<  messages_s[0]    << std::endl;
    std::cout << "+ messages_s[0]^delta -> "  <<  (messages_s[0] ^  delta)  << std::endl;
    std::cout << "+ choices[0]          -> "  <<  choices[0]       << std::endl;
    std::cout << "+ messages_r[0]       -> "  <<  messages_r[0]    << std::endl;

    std::cout << "+ choices_old == choices? " << std ::endl;
    std::cout << "-+ " << std::boolalpha << (choices_old==choices) << std ::endl;
    std::cout << "-+ choices_old[:4] : "      << choices_old[0]    << choices_old[1] 
              << choices_old[2]               << choices_old[3]    << std ::endl;
    std::cout << "-+ choices[:4]     : "      << choices[0]        << choices[1]
              << choices[2]                   << choices[3]        << std ::endl;
    std::cout << std ::endl;
    
    checkCorrelated(messages_r, messages_s, choices, delta, numOTs, verbose, ChoiceBitPacking::False);
}