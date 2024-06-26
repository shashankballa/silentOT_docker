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

class pprfOffline : public RegularPprfSender<block, block, CoeffCtxGF2> {
    public:
    /*
        Copies all the internal variables of the RegularPprfSender object
        to this pprfOffline object.
    */
    void copyPprfSender(RegularPprfSender<block, block, CoeffCtxGF2>& pprfSender) {
        mBaseOTs = pprfSender.mBaseOTs;
        mDomain = pprfSender.mDomain;
        mPntCount = pprfSender.mPntCount;
        mDepth = pprfSender.mDepth;
        mOutputFn = pprfSender.mOutputFn;
        mValue = pprfSender.mValue;
    }

    task<> expandOffline(
        const AlignedUnVector<block>& value,
        block seed,
        AlignedUnVector<block>& output,
        PprfOutputFormat oFormat,
        bool programPuncturedPoint,
        u64 numThreads,
        CoeffCtxGF2 ctx = {})
    {
        if(oFormat != PprfOutputFormat::Interleaved)
            throw std::runtime_error("Only interleaved format is supported in offline mode. " LOCATION);

        if (programPuncturedPoint)
            setValue(value);

        pprf::validateExpandFormat(oFormat, output, mDomain, mPntCount);

        MC_BEGIN(task<>, 
            this, 
            numThreads, 
            oFormat, 
            &output, 
            seed, 
            programPuncturedPoint, 
            ctx,
            treeIndex = u64{},
            tree = span<AlignedArray<block, 8>>{},
            levels = std::vector<span<AlignedArray<block, 8>> >{},
            leafIndex = u64{},
            leafLevelPtr = (AlignedUnVector<block>*)nullptr,
            leafLevel = AlignedUnVector<block>{},
            buff = std::vector<u8>{},
            encSums = span<std::array<block, 2>>{},
            leafMsgs = span<u8>{},
            mTreeAlloc = pprf::TreeAllocator{}
        );

        mTreeAlloc.reserve(numThreads, (1ull << mDepth) + 2);
        setTimePoint("SilentMultiPprfSender.reserve");

        levels.resize(mDepth);
        pprf::allocateExpandTree(mTreeAlloc, levels);

        for (treeIndex = 0; treeIndex < mPntCount; treeIndex += 8)
        {
            
            leafIndex = treeIndex * mDomain;
            leafLevelPtr = &output;

            // allocate the send buffer and partition it.
            pprf::allocateExpandBuffer<block>(
                mDepth - 1, std::min<u64>(8, mPntCount - treeIndex), programPuncturedPoint, buff, encSums, leafMsgs, ctx);

            // exapnd the tree
            expandOne(seed, treeIndex, programPuncturedPoint, levels, *leafLevelPtr, leafIndex, encSums, leafMsgs, ctx);
        }

        mBaseOTs = {};
        //mTreeAlloc.del(tree);
        mTreeAlloc.clear();

        setTimePoint("SilentMultiPprfSender.de-alloc");

        MC_END();
    }
};

class SilentOtExtSenderTest : public SilentOtExtSender {
    public:
        // The ggm tree thats used to generate the sparse vectors.
        pprfOffline mGenOffline;

        bool verbose = false; // verbose

        // sets the verbose flag
        void setVerbose(bool verbose) {
            this->verbose = verbose;
        }
            
        void compressExConv7x24()
        {   
            // Make sure that MultType is ExConv7x24
            if (mMultType != MultType::ExConv7x24)
                throw std::invalid_argument("mMultType != MultType::ExConv7x24 " LOCATION);

            if (verbose) cout << "compressExConv7x24: starting..." << endl;
            
            // The following performs: ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            //      ExConvConfigure(mMultType, _1, expanderWeight, accWeight, _2);
            // u64 expanderWeight = 7;
            // u64 accWeight = 24;
            // u64 scaler = 2;
            // double minDist = 0.15;
            // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            ExConvCodeTest xce; // Expand-Convolute Encoder
            // The following performs: ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            //      xce.config(mRequestNumOts, mNoiseVecSize, expanderWeight, accWeight);
            xce.mSeed            = block(9996754675674599, 56756745976768754);
            xce.mAccumulatorSize = 24;
            xce.mSystematic      = true;
            xce.mMessageSize     = mRequestNumOts;
            xce.mCodeSize        = mNoiseVecSize;
            //      The following performs: ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            //          xce.mExpander.config(mRequestNumOts, sysCodeSize, expanderWeight, regularExpander, _seed);
            xce.mExpander.mMessageSize = mRequestNumOts;
            u64 sysCodeSize = mNoiseVecSize - (mRequestNumOts * xce.mSystematic);
            xce.mExpander.mCodeSize = sysCodeSize;
            xce.mExpander.mExpanderWeight = 7;
            xce.mExpander.mRegular = true;
            block _ccblock = toBlock(0xcccccccccccccccc, 0xcccccccccccccccc);
            xce.mExpander.mSeed = xce.mSeed ^ _ccblock;
            //      ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

            if (verbose){
                cout << "compressExConv7x24: Expander Weight   : ";
                cout << xce.mExpander.mExpanderWeight << endl;
                cout << "compressExConv7x24: Accumulator Weight: ";
                cout << xce.mAccumulatorSize << endl;
                cout << "compressExConv7x24: ExConvCodeTest Message Size    : ";
                cout << xce.mMessageSize << endl;
                cout << "compressExConv7x24: ExConvCodeTest Code Size       : ";
                cout << xce.mCodeSize << endl;
                cout << "compressExConv7x24: ExConvCodeTest Accumulator Size: ";
                cout << xce.mAccumulatorSize << endl;
                cout << "compressExConv7x24: ExConvCodeTest Systematic      : ";
                cout << boolalpha << xce.mSystematic << endl;
            }

            if (verbose) cout << "compressExConv7x24: ExConvCodeTest.dualEncode" << endl;
            xce.dualEncode<block, CoeffCtxGF2>(mB.begin(), {});
            if (verbose) cout << "compressExConv7x24: exiting..." << endl;
        }
        
        /* 
            This function performs all the computations of the sender, offline.
        */
        void silentSendOffline(
            block d,
            u64 n,
            PRNG& prng)
        {
            // if (verbose) cout << "silentSendOffline: MC_BEGIN" << endl;
            // MC_BEGIN(task<>,this, d, n, &prng, &chl,
            //     i = u64{}, j = u64{},
            //     delta = AlignedUnVector<block>{}
            // );

            AlignedUnVector<block> delta;

            gTimer.setTimePoint("sender.ot.enter");
            setTimePoint("sender.expand.enter");

            if (isConfigured() == false)
                throw std::invalid_argument("Sender is not configured" LOCATION);
            if (hasSilentBaseOts() == false)
                throw std::invalid_argument("Sender doesn't have base OTs." LOCATION);
            if (n != mRequestNumOts)
                throw std::invalid_argument("n != mRequestNumOts " LOCATION);
            if (mMalType != SilentSecType::SemiHonest)
                throw std::invalid_argument("mMalType != SilentSecType::SemiHonest " LOCATION);

            setTimePoint("sender.expand.start");
            gTimer.setTimePoint("sender.expand.start");

            // Delta for correlated OTs: m[1] = m[0] ^ delta
            mDelta = d;

            // Allocate memory for the output of PPRF-Expand
            mB.resize(mNoiseVecSize);
            
            delta.resize(1);
            delta[0] = mDelta;

            if (verbose) cout << "silentSendOffline: mGen.expand" << endl;
            
            block _seed = prng.get();
            bool _programPuncturedPoint = true;
            PprfOutputFormat _oFormat = PprfOutputFormat::Interleaved;

            if (_programPuncturedPoint)
                mGen.setValue(delta);

            pprf::validateExpandFormat(_oFormat, mB, mGen.mDomain, mGen.mPntCount);
            
            u64 treeIndex;
            std::vector<span<AlignedArray<block, 8>> > _levels;
            u64 leafIndex;
            AlignedUnVector<block>* leafLevelPtr;
            std::vector<u8> _buff;
            span<std::array<block, 2>> _encSums;
            span<u8> _leafMsgs;
            pprf::TreeAllocator _mTreeAlloc;
            CoeffCtxGF128 _ctx;

            _mTreeAlloc.reserve(mNumThreads, (1ull << mGen.mDepth) + 2);
            _levels.resize(mGen.mDepth);
            pprf::allocateExpandTree(_mTreeAlloc, _levels);

            for (treeIndex = 0; treeIndex < mGen.mPntCount; treeIndex += 8)
            {
                leafIndex = treeIndex * mGen.mDomain;
                leafLevelPtr = &mB;

                // allocate the send buffer and partition it.
                pprf::allocateExpandBuffer<block>(
                    mGen.mDepth - 1, 
                    std::min<u64>(8, mGen.mPntCount - treeIndex), 
                    _programPuncturedPoint, _buff, _encSums, _leafMsgs, _ctx);

                // exapnd the tree
                mGen.expandOne(_seed, treeIndex, _programPuncturedPoint, _levels, *leafLevelPtr, leafIndex, _encSums, _leafMsgs, _ctx);

                // MC_AWAIT(chl.send(std::move(buff)));
                // if we aren't interleaved, we need to copy the
                // leaf layer to the output.
                // if (oFormat != PprfOutputFormat::Interleaved)
                //     pprf::copyOut<VecF, CoeffCtx>(leafLevel, output, mPntCount, treeIndex, oFormat, mOutputFn);

            }

            mGen.mBaseOTs = {};
            //mTreeAlloc.del(tree);
            _mTreeAlloc.clear();

            // MC_AWAIT(mGen.expand(chl, delta, _seed, mB, PprfOutputFormat::Interleaved, true, mNumThreads));

            setTimePoint("sender.expand.pprf_transpose");
            gTimer.setTimePoint("sender.expand.pprf_transpose");

            // if (mDebug)
            // {   
            //     if (verbose) cout << "silentSendInplaceTest: checkRT" << endl;
            //     MC_AWAIT(checkRT(chl));
            // }

            // Input for ExConv is mB. ExConv overwrites mB with the result of ExConv.
            // to check input, print/save mB.

            cout << "silentSendOffline: Size mB before compress (ExConv): " << mB.size() << endl;
            for(auto i = 0; i < mB.size(); i++){
                // cout << mB[i] << endl;
            }
            if (verbose) cout << "silentSendOffline: compressExConv7x24" << endl;
            compressExConv7x24();

            mB.resize(mRequestNumOts);
            cout << "silentSendOffline: Size mB after compress (ExConv): " << mB.size() << endl;

            // if (verbose) cout << "silentSendInplaceTest: MC_END" << endl;
            // MC_END();
        };
        
        task<> silentSendOffline2(
            block d,
            u64 n,
            PRNG& prng)
        {
            mGenOffline.copyPprfSender(mGen);

            if (verbose) cout << "silentSendInplaceTest: MC_BEGIN" << endl;
            MC_BEGIN(task<>,this, d, n, &prng,
                i = u64{}, j = u64{},
                delta = AlignedUnVector<block>{}
            );

            gTimer.setTimePoint("sender.ot.enter");
            setTimePoint("sender.expand.enter");

            if (isConfigured() == false)
                throw std::invalid_argument("Sender is not configured" LOCATION);
            if (hasSilentBaseOts() == false)
                throw std::invalid_argument("Sender doesn't have base OTs." LOCATION);
            if (n != mRequestNumOts)
                throw std::invalid_argument("n != mRequestNumOts " LOCATION);
            if (mMalType != SilentSecType::SemiHonest)
                throw std::invalid_argument("mMalType != SilentSecType::SemiHonest " LOCATION);
            if (mDebug != false)
                throw std::invalid_argument("mDebug != false " LOCATION);

            setTimePoint("sender.expand.start");
            gTimer.setTimePoint("sender.expand.start");

            // Delta for correlated OTs: m[1] = m[0] ^ delta
            mDelta = d;

            // Allocate memory for the output of PPRF-Expand
            mB.resize(mNoiseVecSize);
            
            delta.resize(1);
            delta[0] = mDelta;

            if (verbose) cout << "silentSendInplaceTest: mGen.expand" << endl;
            // MC_AWAIT(mGen.expand(chl, delta, prng.get(), mB, PprfOutputFormat::Interleaved, true, mNumThreads));
            
            MC_AWAIT(mGenOffline.expandOffline(delta, prng.get(), mB, PprfOutputFormat::Interleaved, true, mNumThreads));

            setTimePoint("sender.expand.pprf_transpose");
            gTimer.setTimePoint("sender.expand.pprf_transpose");

            // Input for ExConv is mB. ExConv overwrites mB with the result of ExConv.
            // to check input, print/save mB.
            cout << "silentSendInplaceTest: Size mB before compress (ExConv): " << mB.size() << endl;
            for(auto i = 0; i < mB.size(); i++){
                // cout << mB[i] << endl;
            }
            if (verbose) cout << "silentSendInplaceTest: compressExConv7x24" << endl;
            compressExConv7x24();

            mB.resize(mRequestNumOts);
            cout << "silentSendInplaceTest: Size mB after compress (ExConv): " << mB.size() << endl;

            if (verbose) cout << "silentSendInplaceTest: MC_END" << endl;
            MC_END();
        };

        /* 
            This function is used to test the silent OT protocol.
            It is a wrapper around the silentSendInplaceTest function.
            arguments:
                messages: the messages to be sent
                prng: the PRNG
                chl: the socket
        */
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

            if (isConfigured() == false || hasSilentBaseOts() == false)
                throw std::invalid_argument("Sender is not configured or doesn't have base OTs." LOCATION);

            if (n != mRequestNumOts)
                throw std::invalid_argument("n != mRequestNumOts " LOCATION);

            setTimePoint("sender.expand.start");
            gTimer.setTimePoint("sender.expand.start");

            // Delta for correlated OTs: m[1] = m[0] ^ delta
            mDelta = d;

            // Allocate memory for the output of PPRF-Expand
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


            // Input for ExConv is mB. ExConv overwrites mB with the result of ExConv.
            // to check input, print/save mB.
            cout << "silentSendInplaceTest: Size mB before compress (ExConv): " << mB.size() << endl;
            for(auto i = 0; i < mB.size(); i++){
                // cout << mB[i] << endl;
            }
            if (verbose) cout << "silentSendInplaceTest: compressExConv7x24" << endl;
            compressExConv7x24();

            mB.resize(mRequestNumOts);
            cout << "silentSendInplaceTest: Size mB after compress (ExConv): " << mB.size() << endl;

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

/*
    This is a tesing function that sets the base OTs for sender and corresponding 
    base OTs for the receiver.
    arguments:
        numOTs: number of OTs
        scaler: scaler
        threads: number of threads
        prng: the PRNG
        recver: the receiver
        sender: the sender

*/
void fakeBaseExConv7x24(u64 numOTs,
    u64 threads,
    PRNG& prng,
    SilentOtExtReceiver& recver, SilentOtExtSenderTest& sender,
    bool verbose = false)
{

    u64 scaler = 2;
    
    // fake base OTs for receiver.
    if (recver.mMultType != MultType::ExConv7x24)
        throw std::invalid_argument("recver.mMultType != MultType::ExConv7x24 " LOCATION);
    recver.configure(numOTs, scaler, threads);
    BitVector choices = recver.sampleBaseChoiceBits(prng);
    std::vector<block> msg(choices.size());
    for (u64 i = 0; i < msg.size(); ++i)
        msg[i] = prng.get();
    recver.setSilentBaseOts(msg);

    // Make sure that MultType is ExConv7x24
    if (sender.mMultType != MultType::ExConv7x24)
        throw std::invalid_argument("sender.mMultType != MultType::ExConv7x24 " LOCATION);

    if (verbose) {
        cout << "fakeBase: starting..." << endl;
        cout << "fakeBase: numOTs : " << numOTs << endl;
        cout << "fakeBase: threads: " << threads << endl;
    }
    
    // The following performs: ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //      sender.configure(numOTs, scaler, threads);
    sender.mRequestNumOts = numOTs;
    sender.mNumThreads    = threads;
    sender.mMalType = SilentSecType::SemiHonest;
    u64 secParam    = 128;
    double minDist  = 0.15;
    sender.mNumPartitions = getRegNoiseWeight(minDist, numOTs * scaler, secParam);
    sender.mSizePer = \
        std::max<u64>(
            4, 
            roundUpTo(
                divCeil(
                    numOTs * scaler, 
                    sender.mNumPartitions
                ), 
                2
            )
        );
    
    sender.mNoiseVecSize = sender.mSizePer * sender.mNumPartitions;
    //      The following performs: ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //          sender.mGen.configure(sender.mSizePer, sender.mNumPartitions);
    if (sender.mSizePer & 1)
        throw std::runtime_error("Pprf domain (mSizePer) must be even. " LOCATION);
    if (sender.mSizePer < 2)
        throw std::runtime_error("Pprf domain (mSizePer) must must be at least 2. " LOCATION);
    sender.mGen.mDomain = sender.mSizePer;
    sender.mGen.mDepth = log2ceil(sender.mSizePer);
    sender.mGen.mPntCount = sender.mNumPartitions;
    sender.mGen.mBaseOTs.resize(0, 0);
    //      ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if (verbose) {
        cout << "fakeBase: sender.mNumPartitions: " << sender.mNumPartitions << endl;
        cout << "fakeBase: sender.mSizePer      : " << sender.mSizePer << endl;
        cout << "fakeBase: sender.mNoiseVecSize : " << sender.mNoiseVecSize << endl;
    }

    auto numBaseOTs = sender.mGen.mDepth * sender.mGen.mPntCount;
    std::vector<std::array<block, 2>> msg2(numBaseOTs);
    for (u64 i = 0; i < msg2.size(); ++i)
    {
        msg2[i][choices[i]] = msg[i];
        msg2[i][1 - choices[i]] = prng.get();
    }
    sender.setSilentBaseOts(msg2);

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
    u64 scaler = 2;

    PRNG prng(toBlock(cmd.getOr("seed", 0)));
    PRNG prng1(toBlock(cmd.getOr("seed1", 1)));

    u64 pprf_ggm_depth = cmd.getOr("d", 15);

    bool run_d3_nn5 = cmd.isSet("d3_nn5");
    if (run_d3_nn5){
        cout << "Running d3_nn5 | PPRF GGM-tree depth = 3 | PPRF-Expand output size = 64" << endl;
        verbose = true;
        numOTs = 1 << 5;
        pprf_ggm_depth = 3;
    }

    u64 pprf_num_partitions = (numOTs * scaler) / (1 << pprf_ggm_depth);

    /*  
    The code in:
        1. Silent OT: Receiver Setup -> Base OT
        2. Silent OT: Sender Setup -> Base OT

    Performs the following function call: 
        fakeBaseExConv7x24(numOTs, numThreads, prng, recver, sender, verbose);
    */

    // ========================================================
    // Silent OT: Receiver Setup -> Base OT
    // ========================================================
    
    SilentOtExtReceiver recver;

    // Expand-Convolute LDPC Compression
    recver.mMultType = MultType::ExConv7x24;

    // Receiver's Configuration:
    // recver.configure(numOTs, scaler, numThreads);

    // Semi-Honest Threat Model
    recver.mMalType = SilentSecType::SemiHonest;
    recver.mNumThreads = numThreads;

    // Total number of OTs to be performed
    recver.mRequestNumOts = numOTs;

    // Number of GGM-trees in PPRF-Expand (numOTs * scaler / 2 ^ pprf_ggm_depth)
    // u64 secParam    = 128;
    // double minDist  = 0.15;
    // auto t = max<u64>(40, -double(secParam) / log2(1 - 2 * minDist));
    // if(numOTs * scaler < 512) t = max<u64>(t, 64);
    // recver.mNumPartitions = roundUpTo(t, 8); // this is 248
    recver.mNumPartitions = pprf_num_partitions;

    // Output Size per GGM-tree in PPRF-Expand (2 ^ pprf_ggm_depth)
    recver.mSizePer = \
        std::max<u64>(
            4, 
            roundUpTo(
                divCeil(
                    numOTs * scaler, 
                    recver.mNumPartitions
                ), 
                2
            )
        );

    // Total Output Size of PPRF-Expand (numOTs * scaler)
    recver.mNoiseVecSize = recver.mSizePer * recver.mNumPartitions;
    recver.mS.resize(recver.mNumPartitions);

    // Set up parameters for Receiver's PPRF-Expand:
    // recver.mGen.configure(recver.mSizePer, recver.mNumPartitions);
    if (recver.mSizePer & 1)
        throw std::runtime_error("Pprf domain (mSizePer) must be even. " LOCATION);
    if (recver.mSizePer < 2)
        throw std::runtime_error("Pprf domain (mSizePer) must must be at least 2. " LOCATION);
    
    // Output Size of each GGM-tree (2 ^ pprf_ggm_depth)
    recver.mGen.mDomain = recver.mSizePer;

    // Depth of each GGM-tree (pprf_ggm_depth)
    recver.mGen.mDepth = log2ceil(recver.mSizePer);

    // Number of GGM-trees (numOTs * scaler / 2 ^ pprf_ggm_depth)
    recver.mGen.mPntCount = recver.mNumPartitions;

    if (verbose){
        // Print Receiver's Configuration
        cout << "recver.mGen.mDomain  : " << recver.mGen.mDomain   << endl;
        cout << "recver.mGen.mDepth   : " << recver.mGen.mDepth    << endl;
        cout << "recver.mGen.mPntCount: " << recver.mGen.mPntCount << endl;
    }

    // Receiver's choices for Base OTs
    BitVector baseChoices = recver.sampleBaseChoiceBits(prng);

    // Receiver's messages for Base OTs
    std::vector<block> baseOTs_r(baseChoices.size());
    for (u64 i = 0; i < baseOTs_r.size(); ++i)
        baseOTs_r[i] = prng.get();
    recver.setSilentBaseOts(baseOTs_r);
    
    // Receiver's choices for Silent OTs
    BitVector choice(numOTs);
    
    // Receiver's messages for Silent OTs. 
    // The receiver gets 1 message (block) for every OT.
    std::vector<block> OTs_r(numOTs);
    // ========================================================


    // ========================================================
    // Silent OT: Sender Setup -> Base OT
    // ========================================================
    
    SilentOtExtSenderTest sender;

    // Expand-Convolute LDPC Compression
    sender.mMultType = MultType::ExConv7x24;
    sender.setVerbose(verbose);

    // Sender's Configuration:
    // sender.configure(numOTs, scaler, threads);

    // Semi-Honest Threat Model
    sender.mMalType = SilentSecType::SemiHonest;
    sender.mNumThreads    = numThreads;

    // Total number of OTs to be performed
    sender.mRequestNumOts = numOTs;

    // Number of GGM-trees in PPRF-Expand (numOTs * scaler / 2 ^ pprf_ggm_depth)
    // u64 secParam    = 128;
    // double minDist  = 0.15;
    // auto t = max<u64>(40, -double(secParam) / log2(1 - 2 * minDist));
    // if(numOTs * scaler < 512) t = max<u64>(t, 64);
    // sender.mNumPartitions = roundUpTo(t, 8); // this is 248
    sender.mNumPartitions = pprf_num_partitions;

    // Output Size per GGM-tree in PPRF-Expand (2 ^ pprf_ggm_depth)
    sender.mSizePer = \
        std::max<u64>(
            4, 
            roundUpTo(
                divCeil(
                    numOTs * scaler, 
                    sender.mNumPartitions
                ), 
                2
            )
        );
    
    // Total Output Size of PPRF-Expand (numOTs * scaler)
    sender.mNoiseVecSize = sender.mSizePer * sender.mNumPartitions;

    // Set up parameters for Sender's PPRF-Expand:
    // sender.mGen.configure(sender.mSizePer, sender.mNumPartitions);
    if (sender.mSizePer & 1)
        throw std::runtime_error("Pprf domain (mSizePer) must be even. " LOCATION);
    if (sender.mSizePer < 2)
        throw std::runtime_error("Pprf domain (mSizePer) must must be at least 2. " LOCATION);
    
    // Output Size of each GGM-tree (2 ^ pprf_ggm_depth)
    sender.mGen.mDomain = sender.mSizePer;

    // Depth of each GGM-tree (pprf_ggm_depth)
    sender.mGen.mDepth = log2ceil(sender.mSizePer);

    // Number of GGM-trees (numOTs * scaler / 2 ^ pprf_ggm_depth)
    sender.mGen.mPntCount = sender.mNumPartitions;
    if (verbose){
        // Print Sender's Configuration
        cout << "sender.mGen.mDomain  : " << sender.mGen.mDomain << endl;
        cout << "sender.mGen.mDepth   : " << sender.mGen.mDepth << endl;
        cout << "sender.mGen.mPntCount: " << sender.mGen.mPntCount << endl;
    }

    // Sender Base OTs
    sender.mGen.mBaseOTs.resize(0, 0);

    // Number of Base OTs to be performed =  1 per level per GGM-tree
    auto numBaseOTs = sender.mGen.mDepth * sender.mGen.mPntCount;

    // Sender's messages for Base OTs
    std::vector<std::array<block, 2>> baseOTs_s(numBaseOTs);
    for (u64 i = 0; i < baseOTs_s.size(); ++i)
    {
        baseOTs_s[i][  baseChoices[i]  ] = baseOTs_r[i];
        baseOTs_s[i][1 - baseChoices[i]] = prng.get();
    }
    sender.setSilentBaseOts(baseOTs_s);
    
    // Sender's messages for Silent OTs
    // The sender supplies (gets) 2 messages for every COT (ROT).
    std::vector<std::array<block, 2>> OTs_s(numOTs);
    // ========================================================


    // ========================================================
    // Silent OT: PPRF-Expand -> ExConv-Compress -> hash
    // ========================================================
    Timer timer;
    auto start = timer.setTimePoint("start");
    
    auto p0 = sender.silentSendTest(OTs_s, prng, sockets[0]);
    auto p1 = recver.silentReceive(choice, OTs_r, prng, sockets[1]);

    eval(p0, p1);
    // ========================================================


    
    auto end = timer.setTimePoint("end");
    auto milli = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    checkRandom(OTs_r, OTs_s, choice, numOTs, true);

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


/*
    Tests the sender's computation in silent Random OT protocol.
*/
void silent_ot_sender_offline_test(CLP& cmd)
{
    auto numOTs = cmd.isSet("nn")
        ? (1 << cmd.get<int>("nn"))
        : cmd.getOr("n", 0);

    if (numOTs == 0) numOTs = 1 << 20;

    auto numThreads = cmd.getOr("t", 4);
    bool verbose = (cmd.getOr("v", 0) >= 1);
    u64 scaler = 2;

    PRNG prng(toBlock(cmd.getOr("seed", 0)));
    PRNG prng1(toBlock(cmd.getOr("seed1", 1)));

    u64 pprf_ggm_depth = cmd.getOr("d", 15);

    bool run_d3_nn5 = cmd.isSet("d3_nn5");
    if (run_d3_nn5){
        cout << "Running d3_nn5 | PPRF GGM-tree depth = 3 | PPRF-Expand output size = 64" << endl;
        verbose = true;
        numOTs = 1 << 5;
        pprf_ggm_depth = 3;
    }

    u64 pprf_num_partitions = (numOTs * scaler) / (1 << pprf_ggm_depth);

    // ========================================================
    // Silent OT: Sender Setup -> Base OT
    // ========================================================
    
    SilentOtExtSenderTest sender;

    // Expand-Convolute LDPC Compression
    sender.mMultType = MultType::ExConv7x24;
    sender.setVerbose(verbose);

    // Sender's Configuration:
    // sender.configure(numOTs, scaler, threads);

    // Semi-Honest Threat Model
    sender.mMalType = SilentSecType::SemiHonest;
    sender.mNumThreads    = numThreads;

    // Total number of OTs to be performed
    sender.mRequestNumOts = numOTs;

    // Number of GGM-trees in PPRF-Expand (numOTs * scaler / 2 ^ pprf_ggm_depth)
    // u64 secParam    = 128;
    // double minDist  = 0.15;
    // auto t = max<u64>(40, -double(secParam) / log2(1 - 2 * minDist));
    // if(numOTs * scaler < 512) t = max<u64>(t, 64);
    // sender.mNumPartitions = roundUpTo(t, 8); // this is 248
    sender.mNumPartitions = pprf_num_partitions;

    // Output Size per GGM-tree in PPRF-Expand (2 ^ pprf_ggm_depth)
    sender.mSizePer = \
        std::max<u64>(
            4, 
            roundUpTo(
                divCeil(
                    numOTs * scaler, 
                    sender.mNumPartitions
                ), 
                2
            )
        );
    
    // Total Output Size of PPRF-Expand (numOTs * scaler)
    sender.mNoiseVecSize = sender.mSizePer * sender.mNumPartitions;

    // Set up parameters for Sender's PPRF-Expand:
    // sender.mGen.configure(sender.mSizePer, sender.mNumPartitions);
    if (sender.mSizePer & 1)
        throw std::runtime_error("Pprf domain (mSizePer) must be even. " LOCATION);
    if (sender.mSizePer < 2)
        throw std::runtime_error("Pprf domain (mSizePer) must must be at least 2. " LOCATION);
    
    // Output Size of each GGM-tree (2 ^ pprf_ggm_depth)
    sender.mGen.mDomain = sender.mSizePer;

    // Depth of each GGM-tree (pprf_ggm_depth)
    sender.mGen.mDepth = log2ceil(sender.mSizePer);

    // Number of GGM-trees (numOTs * scaler / 2 ^ pprf_ggm_depth)
    sender.mGen.mPntCount = sender.mNumPartitions;
    if (verbose){
        // Print Sender's Configuration
        cout << "sender.mGen.mDomain  : " << sender.mGen.mDomain << endl;
        cout << "sender.mGen.mDepth   : " << sender.mGen.mDepth << endl;
        cout << "sender.mGen.mPntCount: " << sender.mGen.mPntCount << endl;
    }

    // Sender Base OTs
    sender.mGen.mBaseOTs.resize(0, 0);

    // Number of Base OTs to be performed =  1 per level per GGM-tree
    auto numBaseOTs = sender.mGen.mDepth * sender.mGen.mPntCount;

    // Sender's messages for Base OTs
    std::vector<std::array<block, 2>> baseOTs_s(numBaseOTs);
    for (u64 i = 0; i < baseOTs_s.size(); ++i)
    {
        baseOTs_s[i][0] = prng.get();
        baseOTs_s[i][1] = prng.get();
    }
    sender.setSilentBaseOts(baseOTs_s);
    
    // Sender's messages for Silent OTs
    // The sender supplies (gets) 2 messages for every COT (ROT).
    std::vector<std::array<block, 2>> OTs_s(numOTs);


    // ========================================================
    // Silent OT: PPRF-Expand -> ExConv-Compress -> hash
    // ========================================================
    sender.silentSendOffline(prng.get(), numOTs, prng);
    // ========================================================
}