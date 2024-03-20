#include <libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h>
#include <libOTe/TwoChooseOne/Silent/SilentOtExtSender.h>
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtReceiver.h>
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtSender.h>
#include <libOTe/Tools/LDPC/Util.h>
#include <libOTe_Tests/Common.h>
#include <cryptoTools/Common/TestCollection.h>
#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Crypto/RandomOracle.h>
#include <coproto/Socket/AsioSocket.h>
#include <coproto/Socket/BufferingSocket.h>
#include "libOTe/Tools/QuasiCyclicCode.h"
#include "libOTe/Tools/TungstenCode/TungstenCode.h"

#include "ExConvCodeTest/ExConvCodeTest.h"

#include <iomanip>

#include <Millionaire/millionaire.h>

using namespace osuCrypto;
using namespace std;
using namespace tests_libOTe;

class SilentOtExtSenderTest : public SilentOtExtSender {
    public:
        
        bool verbose = false; // verbose

        // sets the verbose flag
        void setVerbose(bool verbose) {
            this->verbose = verbose;
        }
            
        void compressTest()
        {
            if (verbose) cout << "compressTest: starting..." << endl;
            switch (mMultType)
            {
            case osuCrypto::MultType::QuasiCyclic:
            {
                if (verbose) cout << "compressTest: MultType::QuasiCyclic" << endl;

#ifdef ENABLE_BITPOLYMUL
                QuasiCyclicCode code;
                code.init2(mRequestNumOts, mNoiseVecSize);
                code.dualEncode(mB.subspan(0, code.size()));
#else
                throw std::runtime_error("ENABLE_BITPOLYMUL");
#endif
            }
                break;
            case osuCrypto::MultType::ExAcc7:
            case osuCrypto::MultType::ExAcc11:
            case osuCrypto::MultType::ExAcc21:
            case osuCrypto::MultType::ExAcc40:
            {
                if (verbose) cout << "compressTest: MultType::ExAcc" << endl;

                EACode encoder;
                u64 expanderWeight = 0, _1;
                double _2;
                EAConfigure(mMultType, _1, expanderWeight, _2);
                encoder.config(mRequestNumOts, mNoiseVecSize, expanderWeight);

                AlignedUnVector<block> B2(encoder.mMessageSize);
                encoder.dualEncode<block, CoeffCtxGF2>(mB, B2, {});
                std::swap(mB, B2);
                break;
            }
            case osuCrypto::MultType::ExConv7x24:
            case osuCrypto::MultType::ExConv21x24:
            {
                if (verbose) cout << "compressTest: MultType::ExConv" << endl;

                u64 expanderWeight = 0, accWeight = 0, _1;
                double _2;
                ExConvConfigure(mMultType, _1, expanderWeight, accWeight, _2);

                if (verbose){
                    cout << "compressTest: Expander Weight   : " << expanderWeight << endl;
                    cout << "compressTest: Accumulator Weight: " << accWeight << endl;
                    cout << "compressTest: Scaler            : " << _1 << endl;
                    cout << "compressTest: MinDist           : " << _2 << endl;
                }

                ExConvCodeTest exConvEncoder;
                exConvEncoder.config(mRequestNumOts, mNoiseVecSize, expanderWeight, accWeight);

                if (verbose){
                    cout << "compressTest: ExConvCodeTest Message Size    : ";
                    cout << exConvEncoder.mMessageSize << endl;
                    cout << "compressTest: ExConvCodeTest Code Size       : ";
                    cout << exConvEncoder.mCodeSize << endl;
                    cout << "compressTest: ExConvCodeTest Accumulator Size: ";
                    cout << exConvEncoder.mAccumulatorSize << endl;
                    cout << "compressTest: ExConvCodeTest Systematic      : ";
                    cout << boolalpha << exConvEncoder.mSystematic << endl;
                }

                if (verbose) cout << "compressTest: ExConvCodeTest.dualEncode" << endl;
                exConvEncoder.dualEncode<block, CoeffCtxGF2>(mB.begin(), {});
                break;
            }
            case osuCrypto::MultType::Tungsten:
            {
                if (verbose) cout << "compressTest: MultType::Tungsten" << endl;

                experimental::TungstenCode encoder;
                encoder.config(oc::roundUpTo(mRequestNumOts, 8), mNoiseVecSize);
                encoder.dualEncode<block, CoeffCtxGF2>(mB.begin(), {});
                break;
            }
            default:
                throw RTE_LOC;
                break;
            }
            if (verbose) cout << "compressTest: exiting..." << endl;
        }
            
        void compressExConvTest()
        {
            if (verbose) cout << "compressExConvTest: starting..." << endl;
            switch (mMultType)
            {
            case osuCrypto::MultType::ExConv7x24:
            case osuCrypto::MultType::ExConv21x24:
            {

                u64 expanderWeight = 0, accWeight = 0, _1;
                double _2;
                ExConvConfigure(mMultType, _1, expanderWeight, accWeight, _2);

                if (verbose){
                    cout << "compressExConvTest: Expander Weight   : " << expanderWeight << endl;
                    cout << "compressExConvTest: Accumulator Weight: " << accWeight << endl;
                    cout << "compressExConvTest: Scaler            : " << _1 << endl;
                    cout << "compressExConvTest: MinDist           : " << _2 << endl;
                }

                ExConvCodeTest exConvEncoder;
                exConvEncoder.config(mRequestNumOts, mNoiseVecSize, expanderWeight, accWeight);

                if (verbose){
                    cout << "compressExConvTest: ExConvCodeTest Message Size    : ";
                    cout << exConvEncoder.mMessageSize << endl;
                    cout << "compressExConvTest: ExConvCodeTest Code Size       : ";
                    cout << exConvEncoder.mCodeSize << endl;
                    cout << "compressExConvTest: ExConvCodeTest Accumulator Size: ";
                    cout << exConvEncoder.mAccumulatorSize << endl;
                    cout << "compressExConvTest: ExConvCodeTest Systematic      : ";
                    cout << boolalpha << exConvEncoder.mSystematic << endl;
                }

                if (verbose) cout << "compressExConvTest: ExConvCodeTest.dualEncode" << endl;
                exConvEncoder.dualEncode<block, CoeffCtxGF2>(mB.begin(), {});
                break;
            }
            default:
                throw RTE_LOC;
                break;
            }
            if (verbose) cout << "compressExConvTest: exiting..." << endl;
        }

        task<> silentSendInplaceTest(
            block d,
            u64 n,
            PRNG& prng,
            Socket& chl)
        {
            if (verbose) cout << "silentSendInplaceTest: MC_BEGIN" << endl;
            MC_BEGIN(task<>,this, d, n, &prng, &chl,
                i = u64{}, j = u64{},
                delta = AlignedUnVector<block>{}
            );

            gTimer.setTimePoint("sender.ot.enter");
            setTimePoint("sender.expand.enter");

            if (isConfigured() == false)
            {
                if (verbose) cout << "silentSendInplaceTest: configure" << endl;
                configure(n, 2, mNumThreads, mMalType);
            }

            if (n != mRequestNumOts)
                throw std::invalid_argument("n != mRequestNumOts " LOCATION);

            if (hasSilentBaseOts() == false)
            {
                if (verbose) cout << "silentSendInplaceTest: genSilentBaseOts" << endl;
                MC_AWAIT(genSilentBaseOts(prng, chl));
            }

            setTimePoint("sender.expand.start");
            gTimer.setTimePoint("sender.expand.start");

            mDelta = d;

            // allocate b
            mB.resize(mNoiseVecSize);
            
            delta.resize(1);
            delta[0] = mDelta;

            if (verbose) cout << "silentSendInplaceTest: mGen.expand" << endl;
            MC_AWAIT(mGen.expand(chl, delta, prng.get(), mB, PprfOutputFormat::Interleaved, true, mNumThreads));


            if (mMalType == SilentSecType::Malicious)
            {
                if (verbose) cout << "silentSendInplaceTest: ferretMalCheck" << endl;
                MC_AWAIT(ferretMalCheck(chl, prng));
            }

            setTimePoint("sender.expand.pprf_transpose");
            gTimer.setTimePoint("sender.expand.pprf_transpose");

            if (mDebug)
            {   
                if (verbose) cout << "silentSendInplaceTest: checkRT" << endl;
                MC_AWAIT(checkRT(chl));
            }

            if (verbose) cout << "silentSendInplaceTest: compressExConvTest" << endl;
            compressExConvTest();

            mB.resize(mRequestNumOts);

            if (verbose) cout << "silentSendInplaceTest: MC_END" << endl;
            MC_END();
        };

        task<> silentSendTest(
            span<std::array<block, 2>> messages,
            PRNG& prng,
            Socket& chl)
        {
            if (verbose) cout << "silentSendTest: MC_BEGIN" << endl;
            MC_BEGIN(task<>,this, messages, &prng, &chl,
                type = ChoiceBitPacking::True
            );

            if (verbose) cout << "silentSendTest: silentSendInplaceTest" << endl;
            MC_AWAIT(silentSendInplaceTest(prng.get(), messages.size(), prng, chl));
            
            if (verbose) cout << "silentSendTest: hash" << endl;
            hash(messages, type);

            if (verbose) cout << "silentSendTest: clear" << endl;
            clear();

            if (verbose) cout << "silentSendTest: MC_END" << endl;
            MC_END();
        }
};

void fakeBase(u64 n,
    u64 s,
    u64 threads,
    PRNG& prng,
    SilentOtExtReceiver& recver, SilentOtExtSenderTest& sender)
{
    sender.configure(n, s, threads);
    auto count = sender.silentBaseOtCount();
    std::vector<std::array<block, 2>> msg2(count);
    for (u64 i = 0; i < msg2.size(); ++i)
    {
        msg2[i][0] = prng.get();
        msg2[i][1] = prng.get();
    }
    sender.setSilentBaseOts(msg2);

    // fake base OTs.
    {
        recver.configure(n, s, threads);
        BitVector choices = recver.sampleBaseChoiceBits(prng);
        std::vector<block> msg(choices.size());
        for (u64 i = 0; i < msg.size(); ++i)
            msg[i] = msg2[i][choices[i]];
        recver.setSilentBaseOts(msg);
    }
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

/*
    Tests the silent Random OT protocol.
*/
void silent_ot_test(CLP& cmd)
{
    auto sockets = cp::LocalAsyncSocket::makePair();

    auto numOTs = cmd.isSet("nn")
        ? (1 << cmd.get<int>("nn"))
        : cmd.getOr("n", 0);
    
    if (numOTs == 0) numOTs = 1 << 20;

    auto numThreads = cmd.getOr("t", 4);
    bool verbose = (cmd.getOr("v", 0) >= 1);
    u64 scaler = cmd.getOr("s", 2);

    PRNG prng(toBlock(cmd.getOr("seed", 0)));
    PRNG prng1(toBlock(cmd.getOr("seed1", 1)));

    SilentOtExtSenderTest sender;
    SilentOtExtReceiver recver;

    sender.setVerbose(verbose);

    sender.mMultType = recver.mMultType = MultType::ExConv7x24;

    // We need delta only for COT.
    // block delta = prng.get();
    
    // The sender supplies (gets) 2 messages for every COT (ROT).
    std::vector<std::array<block, 2>> msg_s(numOTs);

    // The receiver gets 1 message for every OT.
    std::vector<block> msg_r(numOTs);
    BitVector choice(numOTs);

    fakeBase(numOTs, scaler, numThreads, prng, recver, sender);

    Timer timer;
    auto start = timer.setTimePoint("start");
    
    auto p0 = sender.silentSendTest(msg_s, prng, sockets[0]);
    auto p1 = recver.silentReceive(choice, msg_r, prng, sockets[1]);

    eval(p0, p1);
    
    auto end = timer.setTimePoint("end");
    auto milli = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    checkRandom(msg_r, msg_s, choice, numOTs, true);

    u64 com = sockets[0].bytesReceived() + sockets[0].bytesSent();

    if (verbose)
    {
        lout << "silent_ot_test" <<
        " n:" << Color::Green << std::setw(6) << std::setfill(' ') << numOTs << Color::Default <<
        "   ||   " << Color::Green <<
        std::setw(6) << std::setfill(' ') << milli << " ms   " <<
        std::setw(6) << std::setfill(' ') << com << " bytes" << std::endl << Color::Default;

        lout << gTimer << std::endl;
    }
}