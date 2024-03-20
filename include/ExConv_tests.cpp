#include "ExConv_tests.h"
#include "ExConvCodeTest/ExConvCodeTest.h"
#include <iomanip>
#include "libOTe/Tools/CoeffCtx.h"
#include "ExConvCodeTest/ExConvCheckerTest.h"

namespace osuCrypto
{
    // Overloads the '<<' operator for printing arrays of unsigned bytes to the output stream
    std::ostream& operator<<(std::ostream& o, const std::array<u8, 3>& a)
    {
        o << "{" << a[0] << " " << a[1] << " " << a[2] << "}";
        return o;
    }

    // A utility struct to facilitate formatted printing of a matrix
    struct mtxPrint
    {
        mtxPrint(AlignedUnVector<u8>& d, u64 r, u64 c)
            :mData(d)
            , mRows(r)
            , mCols(c)
        {}

        AlignedUnVector<u8>& mData; // Matrix data
        u64 mRows, mCols;           // Matrix dimensions
    };

    // Overloads the '<<' operator for printing the matrix in a formatted way
    std::ostream& operator<<(std::ostream& o, const mtxPrint& m)
    {
        for (u64 i = 0; i < m.mRows; ++i)
        {
            for (u64 j = 0; j < m.mCols; ++j)
            {
                bool color = (int)m.mData[i * m.mCols + j] && &o == &std::cout;
                if (color)
                    o << Color::Green;

                o << (int)m.mData[i * m.mCols + j] << " ";

                if (color)
                    o << Color::Default;
            }
            o << std::endl;
        }
        return o;
    }

    template<typename F, typename CoeffCtx>
    void exConvTest(u64 k, u64 n, u64 bw, u64 aw, bool sys, bool v = false)
    {
        // if verbose print the test and its parameters
        if(v){
            std::cout << "->-> exConvTest" << std::endl;
            std::cout << "     Params: k=" << k << "; n=" << n << "; bw=" << bw << "; aw=" << aw;
            // std::cout << "; sys=" << sys << "; F=" << typeid(F).name() ;
            // std::cout << "; CoeffCtx=" << typeid(CoeffCtx).name();
            std::cout << std::endl;
        }
        // The ExConv code instance
        ExConvCodeTest code;
        code.config(k, n, bw, aw, sys);
        
        // Offset for systematic coding
        auto accOffset = sys * k;

        // Psuedo-random number generator
        PRNG prng(CCBlock);
        
        std::vector<F> x_init(n);
        for (u64 i = 0; i < x_init.size(); ++i)
        {
            x_init[i] = prng.get();
        }

        // Coefficient context for operations
        CoeffCtx ctx;

        // Matrix coefficients for accumulation must have as many bytes required to fill the accumulator 
        // width. Generate code size many random Matrix coefficients for accumulation.
        std::vector<std::vector<u8>> matrixCoeffsVec(n, std::vector<u8> (divCeil(aw, 8)));
        for (u64 i = 0; i < n; ++i)
        {
            prng.get(matrixCoeffsVec[i].data(), matrixCoeffsVec[i].size());
        }
/*
=========================================================================================================
Part 1: Testing Accumulation
        Here, we test the accumulation process of the ExConv code. 
        x_acc and x_acc_b are initialized with the same random values from x_init
        The accumulation process is performed differently on each vector:
          x_acc using accOneGen
          x_acc_b using bitwise operations
        The accumulation results are compared to ensure they are equal.
=========================================================================================================
Part 2: Testing Accumulate-Fixed and Refill-Coefficients
        Here, we test the accumulateFixed method of the ExConvCodeTest class on x_acc and compare with
        the result of performing explicit bitwise operations on x_acc_b.
        x_accF is accumulated using an accumulator size of 0, 
        while x_accF_b is accumulated using explicit bitwise operations.
========================================================================================================
Part 3: LDPC-Expand
        Here, we test the expansion process, performed on x_accF, with two different methods:
          x_exp using expand function from ExpanderCodeTest
          x_exp_b using explicit bitwise operations
        The results are compared to ensure they are equal.
========================================================================================================
Part 4: Dual Encoding
        Here, we test the dual encoding process which performs accumulateFixed + expand.
        We perform the dual encoding on the result of the accumulation process and compare the result
        to x_exp, i.e.:
            x_dual = dualEncode(x_acc) == Expand(AccumulateFixed(x_acc)) = x_exp
        where x_acc is the result of the accumulation process on x_init.
========================================================================================================
*/
//======================================================================================================
// Computing with ExConvCodeTest functions to get the expected results that will be compared with the
// results from explicit bitwise operations later.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Part 1: Accumulation with ExConvCode functions
        // x_acc will hold the result of the accumulation process
        std::vector<F> x_acc = x_init;
        for (u64 i = 0; i < n; ++i)
        {   
            // Accumulate x_acc using random coefficients from matrixCoeffs
            code.accOneGen<F, CoeffCtx, true>(x_acc.data(), i, n,  matrixCoeffsVec[i].data(), ctx);
        }
        u64 size = n - accOffset;
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Part 2: Accumulate-Fixed with ExConvCode functions
        // x_accF will hold the result of the accumulate-fixed process
        std::vector<F> x_accF = x_acc;
        code.accumulateFixed<F, CoeffCtx, 0>(x_accF.data() + accOffset, size, ctx, code.mSeed);
        if (code.mAccTwice)
            code.accumulateFixed<F, CoeffCtx, 0>(x_accF.data() + accOffset, size, ctx, ~code.mSeed);
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Part 3: LDPC-Expand with ExConvCode functions
        // x_exp will hold the result of the LDPC-Expansion process
        std::vector<F> x_exp(k);
        if (sys)
        {
            std::copy(x_accF.data(), x_accF.data() + k, x_exp.data());
            code.mExpander.expand<F, CoeffCtx, true>(x_accF.data() + accOffset, x_exp.data());
        }
        else
        {
            code.mExpander.expand<F, CoeffCtx, false>(x_accF.data() + accOffset, x_exp.data());
        }

        u64 step, exSize, regCount = 0;;
        if (code.mExpander.mRegular)
        {
            regCount = divCeil(code.mExpander.mExpanderWeight, 2);
            exSize = step = code.mExpander.mCodeSize / regCount;
        }
        else
        {
            step = 0;
            exSize = n;
        }
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Part 4: Dual Encoding with ExConvCode functions
        // x_dual will hold the result of the dual encoding process
        std::vector<F> x_dual = x_acc;
        code.dualEncode<F, CoeffCtx>(x_dual.begin(), {});
        x_dual.resize(k);
//======================================================================================================
// Performing same computation with explicit bitwise operations
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Part 1: Accumulation with Bitwise Operations
        // x_acc_b will hold the result of the accumulation process
        std::vector<F> x_acc_b = x_init;
        for (u64 i = 0; i < n; ++i)
        {  
            // This code block is used to extract a u64 value from the matrixCoeffs vector.
            // It ensures that the number of bytes copied is not greater than 8, the size of u64.
            // The extracted value is stored in the bits variable.
            assert(aw <= 64); // Ensure that the accumulator width (aw) is not greater than 64 bits.
            u64 bits = 0; // Initialize the bits variable to store the extracted value.
            // Copy the bytes from matrixCoeffs to bits, up to a maximum of 8 bytes.
            // This ensures that only the first 8 bytes of matrixCoeffs are copied, even if it has 
            // more bytes.
            memcpy(&bits, matrixCoeffsVec[i].data(), std::min<u64>( matrixCoeffsVec[i].size(), 8));

            // Accumulate x_acc_b using bitwise operations
            // This loop runs 'aw' times. In each iteration, it increments 'j'.
            // The computation basically involves bitwise operations. Here it is written such that it 
            // can be performed with 64-bit integers. This computation can be significantly optimized 
            // for a verilog (hardware) implementation.
            u64 j = i + 1;
            for (u64 a = 0; a < aw; ++a, ++j)
            {
                // It checks the LSB of 'bits'. If it's 1, it performs an operation.
                if (bits & 1)
                {
                    // The operation is to add the 'i'-th element of 'x_acc_b' to the 'j'-th element.
                    // The result is stored back into the 'j'-th element.
                    // 'ctx.plus' is a method that presumably performs this addition operation.
                    // The '% n' operation is a modulo operation that ensures 'j' is within the bounds 
                    // of the array.
                    ctx.plus(x_acc_b[j % n], x_acc_b[j % n], x_acc_b[i]);
                }
                // It then shifts 'bits' one bit to the right, effectively discarding the LSB.
                bits >>= 1;
            }
            // x_acc_b[j] = x_acc_b[j] + x_acc_b[i]
            ctx.plus(x_acc_b[j % n], x_acc_b[j % n], x_acc_b[i]);
            // x_acc_b[j] = x_acc_b[j] * x_acc_b[j]
            ctx.mulConst(x_acc_b[j % n], x_acc_b[j % n]);
        }
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Part 2: Accumulate-Fixed with Bitwise Operations
        // x_accF_b will hold the result of the accumulate-fixed process
        std::vector<F> x_accF_b = x_acc_b;
        // Here, we perform the expansion process using explicit bitwise operations on x_acc_b and 
        // compare the result to x1 which was expanded using accumulateFixed from ExConvCodeTest class.
        for (auto r = 0; r < 1 + code.mAccTwice; ++r)
        {
            PRNG coeffGen(r ? ~code.mSeed : code.mSeed);
            u8* mtxCoeffIter = (u8*)coeffGen.mBuffer.data();
            auto mtxCoeffEnd = mtxCoeffIter + coeffGen.mBuffer.size() * sizeof(block) - divCeil(aw, 8);

            auto x = x_accF_b.data() + accOffset;
            u64 i = 0;
            while (i < size)
            {
                auto xi = x + i;

                if (mtxCoeffIter > mtxCoeffEnd)
                {
                    // generate more mtx coefficients
                    ExConvCodeTest::refill(coeffGen);
                    mtxCoeffIter = (u8*)coeffGen.mBuffer.data();
                }

                // add xi to the next positions
                auto j = (i + 1) % size;

                u64 bits = 0;
                memcpy(&bits, mtxCoeffIter, divCeil(aw, 8));
                for (u64 a = 0; a < aw; ++a)
                {

                    if (bits & 1)
                    {
                        auto xj = x + j;
                        ctx.plus(*xj, *xj, *xi);
                    }
                    bits >>= 1;
                    j = (j + 1) % size;
                }

                {
                    auto xj = x + j;
                    ctx.plus(*xj, *xj, *xi);
                    ctx.mulConst(*xj, *xj);
                }

                ++mtxCoeffIter;

                ++i;
            }
        }
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Part 3: LDPC-Expand with Bitwise Operations
        // x_exp_b will hold the result of the LDPC-Expansion process
        std::vector<F> x_exp_b(k);
        
        if (sys)
        {
            std::copy(x_accF_b.data(), x_accF_b.data() + k, x_exp_b.data());
        }

        detail::ExpanderModd regExp(code.mExpander.mSeed^ block(342342134, 23421341), exSize);
        detail::ExpanderModd fullExp(code.mExpander.mSeed, code.mExpander.mCodeSize);

        u64 i = 0;
        auto main = k / 8 * 8;
        for (; i < main; i += 8)
        {
            for (u64 j = 0; j < regCount; ++j)
            {
                for (u64 p = 0; p < 8; ++p)
                {
                    auto idx = regExp.get() + step * j;
                    ctx.plus(x_exp_b[i + p], x_exp_b[i + p], x_accF[idx + accOffset]);
                }
            }
            for (u64 j = 0; j < code.mExpander.mExpanderWeight - regCount; ++j)
            {
                for (u64 p = 0; p < 8; ++p)
                {
                    auto idx = fullExp.get();
                    ctx.plus(x_exp_b[i + p], x_exp_b[i + p], x_accF[idx + accOffset]);
                }
            }
        }

        for (; i < k; ++i)
        {
            for (u64 j = 0; j < regCount; ++j)
            {
                auto idx = regExp.get() + step * j;
                ctx.plus(x_exp_b[i], x_exp_b[i], x_accF[idx + accOffset]);
            }
            for (u64 j = 0; j < code.mExpander.mExpanderWeight - regCount; ++j)
            {
                auto idx = fullExp.get();
                ctx.plus(x_exp_b[i], x_exp_b[i], x_accF[idx + accOffset]);
            }
        }
//======================================================================================================
// Verifying the results from bitwise operations with the results from ExConvCodeTest functions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Verfication Code Block
        for (u64 i = 0; i < n; ++i)
        {  
            u64 j = i + 1;
            for (u64 a = 0; a <= aw; ++a, ++j)
            {
                // x_acc and x_acc_b must be equal!
                if (x_acc[j % n] != x_acc_b[j % n])
                {
                    std::cout << j % n << " " << ctx.str(x_acc[j % n]) << " " << ctx.str(x_acc_b[j % n]) << std::endl;
                    throw RTE_LOC;
                }
            }
        }
        // x_accF and x_accF_b must be equal!
        if (x_accF != x_accF_b)
        {
            for (u64 i = 0; i < x_accF.size(); ++i)
            {
                std::cout << i << " " << ctx.str(x_accF[i]) << " " << ctx.str(x_accF_b[i]) << std::endl;
            }
            throw RTE_LOC;
        }
        // x_exp and x_exp_b must be equal!
        if (x_exp != x_exp_b)
            throw RTE_LOC;
        // x_exp and x_exp must be equal!
        if (x_dual != x_exp_b)
            throw RTE_LOC;
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        if(v){
            std::cout << "exConvTest ->->" << std::endl;
        }
    }

    void ExConvCode_tester(const oc::CLP& cmd)
    {   

        // k: message size k
        auto K = cmd.getManyOr<u64>("k", { 32ul, 333 });
        
        // r: code rate for the code size n = k * r   
        auto R = cmd.getManyOr<double>("R", { 2.0, 3.0 });   

        // bw: expander size, I think this is the number of bits in the expander matrix
        auto Bw = cmd.getManyOr<u64>("bw", { 7, 21 });
        auto Aw = cmd.getManyOr<u64>("aw", { 16, 24, 29 });

        // v: verbose mode
        bool v = cmd.isSet("v");

        // Print the test name
        if (v)
        {
            std::cout << "->-> ExConvCode_tester" << std::endl;
        }

        for (auto k : K) for (auto r : R) for (auto bw : Bw) for (auto aw : Aw) for (auto sys : { false, true })
        {

            auto n = k * r;

            if (v)
            {
                std::cout << "starting exConvTest with: F=block; CoeffCtx=CoeffCtxGF128" << std::endl;
            }
            exConvTest<block, CoeffCtxGF128>(k, n, bw, aw, sys, v);
        }
        if (v)
        {
            std::cout << "ExConvCode_tester ->->" << std::endl;
        }
    }

    Matrix<u8> getAccumulator(ExConvCodeTest& encoder)
    {
        auto k = encoder.mMessageSize;;
        auto n = encoder.mCodeSize;;
        if (encoder.mSystematic == false)
            throw RTE_LOC;//not impl

        auto d = n - k;
        Matrix<u8> g(d, d);
        for (u64 i = 0; i < d; ++i)
        {
            std::vector<u8> x(d);
            x[i] = 1;
            CoeffCtxGF2 ctx;
            encoder.accumulate<u8, CoeffCtxGF2>(x.data(), ctx);

            for (u64 j = 0; j < d; ++j)
            {
                g(j, i) = x[j];
            }
        }
        return g;
    }


    u64 getGeneratorWeight_(ExConvCodeTest& encoder, bool verbose)
    {
        auto k = encoder.mMessageSize;
        auto n = encoder.mCodeSize;
        auto g = getGenerator(encoder);
        //bool failed = false;
        u64 min = n;
        u64 iMin = 0;;
        for (u64 i = 0; i < k; ++i)
        {
            u64 weight = 0;
            for (u64 j = 0; j < n; ++j)
            {
                //if (verbose)
                //{
                //    if (g(i, j))
                //        std::cout << Color::Green << "1" << Color::Default;
                //    else
                //        std::cout << "0";
                //}
                assert(g(i, j) < 2);
                weight += g(i, j);
            }
            //if (verbose)
            //    std::cout << std::endl;

            if (weight < min)
                iMin = i;
            min = std::min<u64>(min, weight);
        }

        if (verbose)
        {
            auto Ex = encoder.mExpander.getMatrix();
            //for (u64 i = 0; i < Ex.cols(); ++i)
            //{
            //    std::cout << Ex(985, i) << " ";

            //}
            //std::cout << " ~ " << k << std::endl;

            std::cout << "i " << iMin << " " << min << " / " << n << " = " << double(min) / n << std::endl;
            for (u64 j = 0; j < n; ++j)
            {
                auto ei = Ex[iMin];
                bool found = std::find(ei.begin(), ei.end(), j - k) != ei.end();
                if (j == iMin + k)
                {
                    std::cout << Color::Blue << int(g(iMin, j)) << Color::Default;

                }
                else if (found)
                {
                    std::cout << Color::Red << int(g(iMin, j)) << Color::Default;

                }
                else if (g(iMin, j))
                    std::cout << Color::Green << "1" << Color::Default;
                else
                    std::cout << "0";

                if (j == k - 1)
                {
                    std::cout << "\n";
                }
            }
            std::cout << std::endl << "--------------------------------\n";

            auto a = getAccumulator(encoder);

            auto ei = Ex[iMin];
            for (auto i : ei)
            {
                for (u64 j = 0; j < k; ++j)
                {
                    if (a(i, j))
                        std::cout << Color::Green << "1" << Color::Default;
                    else
                        std::cout << "0";
                }
                std::cout << std::endl;
            }
        }

        return min;
    }


    void ExConvCode_weight_test(const oc::CLP& cmd)
    {
        u64  k = cmd.getOr("k", 1ull << cmd.getOr("kk", 6));
        u64 n = k * 2;
        //u64 aw = cmd.getOr("aw", 24);
        //u64 bw = cmd.getOr("bw", 7);
        bool verbose = cmd.isSet("v");
        bool accTwice = cmd.getOr("accTwice", 1);
        for (u64 aw = 4; aw < 16; aw += 2)
        {
            for (u64 bw = 3; bw < 11; bw += 2)
            {

                ExConvCodeTest encoder;
                encoder.config(k, n, bw, aw);
                encoder.mAccTwice = accTwice;

                auto threshold = n / 6 - 2 * std::sqrt(n);
                u64 min = 0;
                //if (cmd.isSet("x2"))
                //    min = getGeneratorWeightx2(encoder, verbose);
                //else
                min = getGeneratorWeight_(encoder, verbose);

                if (cmd.isSet("acc"))
                {
                    auto g = getAccumulator(encoder);

                    for (u64 i = 0; i < k; ++i)
                    {
                        u64 w = 0;
                        for (u64 j = 0; j < k; ++j)
                        {
                            w += g(i, j);
                        }
                        //if (w < k / 2.2)
                        {
                            //std::cout << i << " " << w << std::endl;
                            for (u64 j = 0; j < k; ++j)
                            {
                                if (g(i, j))
                                    std::cout << Color::Green << "1" << Color::Default;
                                else
                                    std::cout << "0";
                            }
                            std::cout << std::endl;
                        }
                    }
                    std::cout << std::endl;
                }

                if (verbose)
                    std::cout << "aw " << aw << " bw " << bw << ": " << min << " / " << n << " = " << double(min) / n << " < threshold " << double(threshold) / n << std::endl;

                if (min < threshold)
                {
                    throw RTE_LOC;
                }

            }
        }

    }

}