#include "ExConv_tests.h"
#include "ExConvCode/ExConvCode.h"
#include <iomanip>
#include "libOTe/Tools/CoeffCtx.h"
#include "ExConvCode/ExConvChecker.h"

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

    //block mult2(block x, int imm8)
    //{
    //    assert(imm8 < 2);
    //    if (imm8)
    //    {
    //        // mult x[1] * 2
    // 
    //    }
    //    else
    //    {
    //        // x[0] * 2
    //        __m128i carry = _mm_slli_si128(x, 8); 
    //        carry = _mm_srli_epi64(carry, 63);  
    //        x = _mm_slli_epi64(x, 1);
    //        return _mm_or_si128(x, carry);
    //
    //        //return _mm_slli_si128(x, 8);
    //    }
    //    //TEMP[i] : = (TEMP1[0] and TEMP2[i])
    //    //    FOR a : = 1 to i
    //    //    TEMP[i] : = TEMP[i] XOR(TEMP1[a] AND TEMP2[i - a])
    //    //    ENDFOR
    //    //dst[i] : = TEMP[i]
    //}
    
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
        ExConvCode code;
        code.config(k, n, bw, aw, sys);
        
        // Offset for systematic coding
        auto accOffset = sys * k;

        // Vectors for testing the accumulation process of the ExConv code. 
        // x1, x2 and x3 are initialized with random values:
        //   x1 == x2 == x3, all same random values
        // The accumulation process is performed differently on each vector:
        //   x1 using accOneGen
        //   x2 using accOne
        //   x3 using bitwise operations)
        // The accumulation results are compared to ensure they are equal.
        std::vector<F> x1(n), x2(n), x3(n);

        // Psuedo-random number generator
        PRNG prng(CCBlock);

        // Initialize x1, x2 and x3 with random values
        // x1 == x2 == x3, all same random values
        for (u64 i = 0; i < x1.size(); ++i)
        {
            x1[i] = x2[i] = x3[i] = prng.get();
        }
        std::vector<F> x1_init = x1;

        // Coefficient context for operations
        CoeffCtx ctx;

        // Matrix coefficients for accumulation must have as 
        // many bytes required to fill the accumulator width
        std::vector<u8> matrixCoeffs(divCeil(aw, 8));

        // bools to save verification results for each loop
        bool x1_x2_nequal_acc  = true;
        bool x1_x3_nequal_acc  = true;
        bool x1_x2_nequal_accF = true;
        bool x1_x3_nequal_accF = true;
        bool y1_y2_nequal_exp  = true;
        bool x4_y1_nequal_dual = true;

//=====================================================================================================
// Component 1: Testing Accumulation
        for (u64 i = 0; i < n; ++i)
        {   
            // fill matrixCoeffs with random bytes
            prng.get(matrixCoeffs.data(), matrixCoeffs.size());
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x1 Code Block
            // Accumulate x1 using random coefficients from matrixCoeffs
            code.accOneGen<F, CoeffCtx, true>(x1.data(), i, n, matrixCoeffs.data(), ctx);
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x2 Code Block
            if (aw == 24)
                // Accumulate x2 using random coefficients
                code.accOne<F, CoeffCtx, true, 24>(x2.data(), i, n, matrixCoeffs.data(), ctx);
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x3 Code Block
            // This code block is used to extract a u64 value from the matrixCoeffs vector.
            // It ensures that the number of bytes copied is not greater than 8, which is the size of a u64.
            // The extracted value is stored in the bits variable.

            assert(aw <= 64); // Ensure that the accumulator width (aw) is not greater than 64 bits.

            u64 bits = 0; // Initialize the bits variable to store the extracted value.

            // Copy the bytes from matrixCoeffs to bits, up to a maximum of 8 bytes.
            // This ensures that only the first 8 bytes of matrixCoeffs are copied, even if it has more bytes.
            memcpy(&bits, matrixCoeffs.data(), std::min<u64>(matrixCoeffs.size(), 8));

            // Accumulate x3 using bitwise operations
            // This loop runs 'aw' times. In each iteration, it increments 'j'.
            // The computation basically involves bitwise operations. Here it is written such that it can be
            // performed with 64-bit integers. This computation can be significantly optimized for a verilog 
            // (hardware) implementation.
            u64 j = i + 1;
            for (u64 a = 0; a < aw; ++a, ++j)
            {
                // It checks the least significant bit of 'bits'. If it's 1, it performs an operation.
                if (bits & 1)
                {
                    // The operation is to add the 'i'-th element of 'x3' to the 'j'-th element.
                    // The result is stored back into the 'j'-th element.
                    // 'ctx.plus' is a method that presumably performs this addition operation.
                    // The '% n' operation is a modulo operation that ensures 'j' is within the bounds of the array.
                    ctx.plus(x3[j % n], x3[j % n], x3[i]);
                }
                // It then shifts 'bits' one bit to the right, effectively discarding the least significant bit.
                bits >>= 1;
            }
            // x3[j] = x3[j] + x3[i]
            ctx.plus(x3[j % n], x3[j % n], x3[i]);
            // x3[j] = x3[j] * x3[j]
            ctx.mulConst(x3[j % n], x3[j % n]);
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Verfication Code Block
            j = i + 1;
            for (u64 a = 0; a <= aw; ++a, ++j)
            {
                // Check if x1 and x2 are equal (only for aw = 24)
                if (aw == 24 && x1[j%n] != x2[j % n])
                {
                    std::cout << j % n << " " << ctx.str(x1[j % n]) << " " << ctx.str(x2[j % n]) << std::endl;
                    throw RTE_LOC;
                }

                // x1 and x3 must be equal!
                if (x1[j % n] != x3[j % n])
                {
                    std::cout << j % n << " " << ctx.str(x1[j % n]) << " " << ctx.str(x3[j % n]) << std::endl;
                    throw RTE_LOC;
                }
            }
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        }
        std::vector<F> x1_acc = x1;
//=====================================================================================================
// Component 2: Testing Accumulate-Fixed and Refill-Coefficients
        // Here, we test the accumulateFixed method of the ExConvCode class on x1 and x2.
        // x1 is accumulated using an accumulator size of 0, 
        // while x2 is accumulated using an accumulator size of 24.
        u64 size = n - accOffset;
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x1 Code Block
        code.accumulateFixed<F, CoeffCtx, 0>(x1.data() + accOffset, size, ctx, code.mSeed);
        if (code.mAccTwice)
            code.accumulateFixed<F, CoeffCtx, 0>(x1.data() + accOffset, size, ctx, ~code.mSeed);
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x2 Code Block
        if (aw == 24)
        {
            code.accumulateFixed<F, CoeffCtx, 24>(x2.data() + accOffset, size, ctx, code.mSeed);

            if (code.mAccTwice)
                code.accumulateFixed<F, CoeffCtx, 24>(x2.data() + accOffset, size, ctx, ~code.mSeed);
        }
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x3 Code Block
        // Here, we perform the expansion process using explicit bitwise operations on x3 and compare the 
        // result to x1 which was expanded using accumulateFixed from ExConvCode class.
        for (auto r = 0; r < 1 + code.mAccTwice; ++r)
        {
            PRNG coeffGen(r ? ~code.mSeed : code.mSeed);
            u8* mtxCoeffIter = (u8*)coeffGen.mBuffer.data();
            auto mtxCoeffEnd = mtxCoeffIter + coeffGen.mBuffer.size() * sizeof(block) - divCeil(aw, 8);

            auto x = x3.data() + accOffset;
            u64 i = 0;
            while (i < size)
            {
                auto xi = x + i;

                if (mtxCoeffIter > mtxCoeffEnd)
                {
                    // generate more mtx coefficients
                    ExConvCode::refill(coeffGen);
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
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Verfication Code Block
        // If the accumulator width is 24, the results x1 and x2 are compared to ensure they are equal.
        if (aw == 24)
        {
            // x1 and x2 must be equal!
            if (x1 != x2)
            {
                for (u64 i = 0; i < x1.size(); ++i)
                {
                    std::cout << i << " " << ctx.str(x1[i]) << " " << ctx.str(x2[i]) << std::endl;
                }
                throw RTE_LOC;
            }
        }
        // x1 and x3 must be equal!
        if (x1 != x3)
        {
            for (u64 i = 0; i < x1.size(); ++i)
            {
                std::cout << i << " " << ctx.str(x1[i]) << " " << ctx.str(x3[i]) << std::endl;
            }
            throw RTE_LOC;
        }
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        std::vector<F> x1_accF = x1;
//====================================================================================================
// Component 3: Expand
        // Here, we test the expansion process, performed on x1, with two different methods:
        //   y1 using expand function from ExpanderCode
        //   y2 using explicit bitwise operations
        // The results are compared to ensure they are equal.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// y1 Code Block
        std::vector<F> y1(k), y2(k);
        if (sys)
        {
            std::copy(x1.data(), x1.data() + k, y1.data());
            y2 = y1;
            code.mExpander.expand<F, CoeffCtx, true>(x1.data() + accOffset, y1.data());
        }
        else
        {
            code.mExpander.expand<F, CoeffCtx, false>(x1.data() + accOffset, y1.data());
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
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// y2 Code Block

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
                    ctx.plus(y2[i + p], y2[i + p], x1[idx + accOffset]);
                }
            }
            for (u64 j = 0; j < code.mExpander.mExpanderWeight - regCount; ++j)
            {
                for (u64 p = 0; p < 8; ++p)
                {
                    auto idx = fullExp.get();
                    ctx.plus(y2[i + p], y2[i + p], x1[idx + accOffset]);
                }
            }
        }

        for (; i < k; ++i)
        {
            for (u64 j = 0; j < regCount; ++j)
            {
                auto idx = regExp.get() + step * j;
                ctx.plus(y2[i], y2[i], x1[idx + accOffset]);
            }
            for (u64 j = 0; j < code.mExpander.mExpanderWeight - regCount; ++j)
            {
                auto idx = fullExp.get();
                ctx.plus(y2[i], y2[i], x1[idx + accOffset]);
            }
        }
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Verfication Code Block
        // y1 and y2 must be equal!
        if (y1 != y2)
            throw RTE_LOC;
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        std::vector<F> x1_exp = y1;
//====================================================================================================
// Component 4: Dual Encoding
        // Here, we test the dual encoding process which includes accumulateFixed and expand.
        // x4 is used to hold a copy of x1 after the accumulation process. We perform the dual encoding
        // process on x4 and compare the result to y1, i.e.:
        //     dualEncode(x1_acc) == Expand(AccumulateFixed(x1_acc))
        // where x1_acc is the value of x1 after the accumulation process.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x4 Code Block
        // x4 is used to hold a copy of x1 after the accumulation process. 
        // It is used to test the dual encoding process of the ExConv code.
        std::vector<F> x4 = x1_acc;
        code.dualEncode<F, CoeffCtx>(x4.begin(), {});
        x4.resize(k);
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Verfication Code Block
        // x4 and y1 must be equal!
        if (x4 != y1)
            throw RTE_LOC;
//====================================================================================================
        if(v){
            std::cout << "exConvTest ->->" << std::endl;
        }
    }

    template<typename F, typename CoeffCtx>
    void exConvTestBitWise(u64 k, u64 n, u64 bw, u64 aw, bool sys, bool v = false)
    {
        // if verbose print the test and its parameters
        if(v){
            std::cout << "->-> exConvTestBitWise" << std::endl;
            std::cout << "     Params: k=" << k << "; n=" << n << "; bw=" << bw << "; aw=" << aw;
            // std::cout << "; sys=" << sys << "; F=" << typeid(F).name() ;
            // std::cout << "; CoeffCtx=" << typeid(CoeffCtx).name();
            std::cout << std::endl;
        }
        // The ExConv code instance
        ExConvCode code;
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

//=====================================================================================================
// Component 1: Testing Accumulation
        // Here, we test the accumulation process of the ExConv code. 
        // x_acc and x_acc_b are initialized with the same random values from x_init
        // The accumulation process is performed differently on each vector:
        //   x_acc using accOneGen
        //   x_acc_b using bitwise operations
        // The accumulation results are compared to ensure they are equal.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x_acc Code Block
        std::vector<F> x_acc = x_init;
        for (u64 i = 0; i < n; ++i)
        {   
            // Accumulate x_acc using random coefficients from matrixCoeffs
            code.accOneGen<F, CoeffCtx, true>(x_acc.data(), i, n,  matrixCoeffsVec[i].data(), ctx);
        }
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x_acc_b Code Block
        std::vector<F> x_acc_b = x_init;
        for (u64 i = 0; i < n; ++i)
        {  
            // This code block is used to extract a u64 value from the matrixCoeffs vector.
            // It ensures that the number of bytes copied is not greater than 8, which is the size of a u64.
            // The extracted value is stored in the bits variable.
            assert(aw <= 64); // Ensure that the accumulator width (aw) is not greater than 64 bits.
            u64 bits = 0; // Initialize the bits variable to store the extracted value.
            // Copy the bytes from matrixCoeffs to bits, up to a maximum of 8 bytes.
            // This ensures that only the first 8 bytes of matrixCoeffs are copied, even if it has more bytes.
            memcpy(&bits, matrixCoeffsVec[i].data(), std::min<u64>( matrixCoeffsVec[i].size(), 8));

            // Accumulate x_acc_b using bitwise operations
            // This loop runs 'aw' times. In each iteration, it increments 'j'.
            // The computation basically involves bitwise operations. Here it is written such that it can be
            // performed with 64-bit integers. This computation can be significantly optimized for a verilog 
            // (hardware) implementation.
            u64 j = i + 1;
            for (u64 a = 0; a < aw; ++a, ++j)
            {
                // It checks the least significant bit of 'bits'. If it's 1, it performs an operation.
                if (bits & 1)
                {
                    // The operation is to add the 'i'-th element of 'x_acc_b' to the 'j'-th element.
                    // The result is stored back into the 'j'-th element.
                    // 'ctx.plus' is a method that presumably performs this addition operation.
                    // The '% n' operation is a modulo operation that ensures 'j' is within the bounds of the array.
                    ctx.plus(x_acc_b[j % n], x_acc_b[j % n], x_acc_b[i]);
                }
                // It then shifts 'bits' one bit to the right, effectively discarding the least significant bit.
                bits >>= 1;
            }
            // x_acc_b[j] = x_acc_b[j] + x_acc_b[i]
            ctx.plus(x_acc_b[j % n], x_acc_b[j % n], x_acc_b[i]);
            // x_acc_b[j] = x_acc_b[j] * x_acc_b[j]
            ctx.mulConst(x_acc_b[j % n], x_acc_b[j % n]);
        }
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
//=====================================================================================================
// Component 2: Testing Accumulate-Fixed and Refill-Coefficients
        // Here, we test the accumulateFixed method of the ExConvCode class on x_acc and compare with
        // the result of performing explicit bitwise operations on x_acc_b.
        // x_accF is accumulated using an accumulator size of 0, 
        // while x_accF_b is accumulated using explicit bitwise operations.
        u64 size = n - accOffset;
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x_accF Code Block
        std::vector<F> x_accF = x_acc;
        code.accumulateFixed<F, CoeffCtx, 0>(x_accF.data() + accOffset, size, ctx, code.mSeed);
        if (code.mAccTwice)
            code.accumulateFixed<F, CoeffCtx, 0>(x_accF.data() + accOffset, size, ctx, ~code.mSeed);
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x_accF_b Code Block
        std::vector<F> x_accF_b = x_acc_b;
        // Here, we perform the expansion process using explicit bitwise operations on x_acc_b and compare the 
        // result to x1 which was expanded using accumulateFixed from ExConvCode class.
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
                    ExConvCode::refill(coeffGen);
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
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Verfication Code Block
        // x_accF and x_accF_b must be equal!
        if (x_accF != x_accF_b)
        {
            for (u64 i = 0; i < x_accF.size(); ++i)
            {
                std::cout << i << " " << ctx.str(x_accF[i]) << " " << ctx.str(x_accF_b[i]) << std::endl;
            }
            throw RTE_LOC;
        }
//====================================================================================================
// Component 3: Expand
        // Here, we test the expansion process, performed on x_accF, with two different methods:
        //   x_exp using expand function from ExpanderCode
        //   x_exp_b using explicit bitwise operations
        // The results are compared to ensure they are equal.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x_exp Code Block
        std::vector<F> x_exp(k), x_exp_b(k);
        if (sys)
        {
            std::copy(x_accF.data(), x_accF.data() + k, x_exp.data());
            x_exp_b = x_exp;
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
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x_exp_b Code Block

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
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Verfication Code Block
        // x_exp and x_exp_b must be equal!
        if (x_exp != x_exp_b)
            throw RTE_LOC;
//====================================================================================================
// Component 4: Dual Encoding
        // Here, we test the dual encoding process which includes accumulateFixed and expand.
        // x4 is used to hold a copy of x1 after the accumulation process. We perform the dual encoding
        // process on x4 and compare the result to x_exp, i.e.:
        //     dualEncode(x1_acc) == Expand(AccumulateFixed(x1_acc))
        // where x1_acc is the value of x1 after the accumulation process.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x_dual Code Block
        std::vector<F> x_dual = x_acc;
        code.dualEncode<F, CoeffCtx>(x_dual.begin(), {});
        x_dual.resize(k);
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Verfication Code Block
        // x_exp and x_exp must be equal!
        if (x_dual != x_exp_b)
            throw RTE_LOC;
//====================================================================================================
        if(v){
            std::cout << "exConvTestBitWise ->->" << std::endl;
        }
    }

    template<typename F, typename CoeffCtx>
    void exConvTestBitWiseSeq(u64 k, u64 n, u64 bw, u64 aw, bool sys, bool v = false)
    {
        // if verbose print the test and its parameters
        if(v){
            std::cout << "->-> exConvTestBitWiseSeq" << std::endl;
            std::cout << "     Params: k=" << k << "; n=" << n << "; bw=" << bw << "; aw=" << aw;
            // std::cout << "; sys=" << sys << "; F=" << typeid(F).name() ;
            // std::cout << "; CoeffCtx=" << typeid(CoeffCtx).name();
            std::cout << std::endl;
        }
        // The ExConv code instance
        ExConvCode code;
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
Component 1: Testing Accumulation
        Here, we test the accumulation process of the ExConv code. 
        x_acc and x_acc_b are initialized with the same random values from x_init
        The accumulation process is performed differently on each vector:
          x_acc using accOneGen
          x_acc_b using bitwise operations
        The accumulation results are compared to ensure they are equal.
=========================================================================================================
Component 2: Testing Accumulate-Fixed and Refill-Coefficients
        Here, we test the accumulateFixed method of the ExConvCode class on x_acc and compare with
        the result of performing explicit bitwise operations on x_acc_b.
        x_accF is accumulated using an accumulator size of 0, 
        while x_accF_b is accumulated using explicit bitwise operations.
========================================================================================================
Component 3: Expand
        Here, we test the expansion process, performed on x_accF, with two different methods:
          x_exp using expand function from ExpanderCode
          x_exp_b using explicit bitwise operations
        The results are compared to ensure they are equal.
========================================================================================================
Component 4: Dual Encoding
        Here, we test the dual encoding process which performs accumulateFixed + expand.
        We perform the dual encoding on the result of the accumulation process and compare the result
        to x_exp, i.e.:
            x_dual = dualEncode(x_acc) == Expand(AccumulateFixed(x_acc)) = x_exp
        where x_acc is the result of the accumulation process on x_init.
========================================================================================================
*/
//======================================================================================================
// Computing with ExConvCode functions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x_acc Code Block
        std::vector<F> x_acc = x_init;
        for (u64 i = 0; i < n; ++i)
        {   
            // Accumulate x_acc using random coefficients from matrixCoeffs
            code.accOneGen<F, CoeffCtx, true>(x_acc.data(), i, n,  matrixCoeffsVec[i].data(), ctx);
        }
        u64 size = n - accOffset;
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x_accF Code Block
        std::vector<F> x_accF = x_acc;
        code.accumulateFixed<F, CoeffCtx, 0>(x_accF.data() + accOffset, size, ctx, code.mSeed);
        if (code.mAccTwice)
            code.accumulateFixed<F, CoeffCtx, 0>(x_accF.data() + accOffset, size, ctx, ~code.mSeed);
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x_exp Code Block
        std::vector<F> x_exp(k), x_exp_b(k);
        if (sys)
        {
            std::copy(x_accF.data(), x_accF.data() + k, x_exp.data());
            x_exp_b = x_exp;
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
// x_dual Code Block
        std::vector<F> x_dual = x_acc;
        code.dualEncode<F, CoeffCtx>(x_dual.begin(), {});
        x_dual.resize(k);
//======================================================================================================
// Performing same computation with explicit bitwise operations
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// x_acc_b Code Block
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
// x_accF_b Code Block
        std::vector<F> x_accF_b = x_acc_b;
        // Here, we perform the expansion process using explicit bitwise operations on x_acc_b and 
        // compare the result to x1 which was expanded using accumulateFixed from ExConvCode class.
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
                    ExConvCode::refill(coeffGen);
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
// x_exp_b Code Block

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
// Verifying the results from bitwise operations with the results from ExConvCode functions
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
            std::cout << "exConvTestBitWiseSeq ->->" << std::endl;
        }
    }


    void ExConvCode_encode_basic_test(const oc::CLP& cmd)
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
            std::cout << "->-> ExConvCode_encode_basic_test" << std::endl;
        }

        for (auto k : K) for (auto r : R) for (auto bw : Bw) for (auto aw : Aw) for (auto sys : { false, true })
        {

            auto n = k * r;

            if (v)
            {
                std::cout << "starting exConvTest with: F=u32; CoeffCtx=CoeffCtxInteger" << std::endl;
            }
            exConvTest<u32, CoeffCtxInteger>(k, n, bw, aw, sys, v);
            
            if (v)
            {
                std::cout << "starting exConvTest with: F=u8; CoeffCtx=CoeffCtxInteger" << std::endl;
            }
            exConvTest<u8, CoeffCtxInteger>(k, n, bw, aw, sys, v);

            if (v)
            {
                std::cout << "starting exConvTest with: F=block; CoeffCtx=CoeffCtxGF128" << std::endl;
            }
            exConvTest<block, CoeffCtxGF128>(k, n, bw, aw, sys, v);

            if (v)
            {
                std::cout << "starting exConvTest with: F=std::array<u8, 4>; CoeffCtx=CoeffCtxArray<u8, 4>" << std::endl;
            }
            exConvTest<std::array<u8, 4>, CoeffCtxArray<u8, 4>>(k, n, bw, aw, sys, v);
        }
        if (v)
        {
            std::cout << "ExConvCode_encode_basic_test ->->" << std::endl;
        }
    }

    
    void ExConvCode_encode_basic_bitwise_test(const oc::CLP& cmd)
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
            std::cout << "->-> ExConvCode_encode_basic_bitwise_test" << std::endl;
        }

        for (auto k : K) for (auto r : R) for (auto bw : Bw) for (auto aw : Aw) for (auto sys : { false, true })
        {

            auto n = k * r;

            if (v)
            {
                std::cout << "starting exConvTestBitWise with: F=u32; CoeffCtx=CoeffCtxInteger" << std::endl;
            }
            exConvTestBitWise<u32, CoeffCtxInteger>(k, n, bw, aw, sys, v);
            
            if (v)
            {
                std::cout << "starting exConvTestBitWise with: F=u8; CoeffCtx=CoeffCtxInteger" << std::endl;
            }
            exConvTestBitWise<u8, CoeffCtxInteger>(k, n, bw, aw, sys, v);

            if (v)
            {
                std::cout << "starting exConvTestBitWise with: F=block; CoeffCtx=CoeffCtxGF128" << std::endl;
            }
            exConvTestBitWise<block, CoeffCtxGF128>(k, n, bw, aw, sys, v);

            if (v)
            {
                std::cout << "starting exConvTestBitWise with: F=std::array<u8, 4>; CoeffCtx=CoeffCtxArray<u8, 4>" << std::endl;
            }
            exConvTestBitWise<std::array<u8, 4>, CoeffCtxArray<u8, 4>>(k, n, bw, aw, sys, v);
        }
        if (v)
        {
            std::cout << "ExConvCode_encode_basic_bitwise_test ->->" << std::endl;
        }
    }


    void ExConvCode_encode_basic_bitwise_seq_test(const oc::CLP& cmd)
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
            std::cout << "->-> ExConvCode_encode_basic_bitwise_seq_test" << std::endl;
        }

        for (auto k : K) for (auto r : R) for (auto bw : Bw) for (auto aw : Aw) for (auto sys : { false, true })
        {

            auto n = k * r;

            if (v)
            {
                std::cout << "starting exConvTestBitWiseSeq with: F=u32; CoeffCtx=CoeffCtxInteger" << std::endl;
            }
            exConvTestBitWiseSeq<u32, CoeffCtxInteger>(k, n, bw, aw, sys, v);
            
            if (v)
            {
                std::cout << "starting exConvTestBitWiseSeq with: F=u8; CoeffCtx=CoeffCtxInteger" << std::endl;
            }
            exConvTestBitWiseSeq<u8, CoeffCtxInteger>(k, n, bw, aw, sys, v);

            if (v)
            {
                std::cout << "starting exConvTestBitWiseSeq with: F=block; CoeffCtx=CoeffCtxGF128" << std::endl;
            }
            exConvTestBitWiseSeq<block, CoeffCtxGF128>(k, n, bw, aw, sys, v);

            if (v)
            {
                std::cout << "starting exConvTestBitWiseSeq with: F=std::array<u8, 4>; CoeffCtx=CoeffCtxArray<u8, 4>" << std::endl;
            }
            exConvTestBitWiseSeq<std::array<u8, 4>, CoeffCtxArray<u8, 4>>(k, n, bw, aw, sys, v);
        }
        if (v)
        {
            std::cout << "ExConvCode_encode_basic_bitwise_seq_test ->->" << std::endl;
        }
    }

    Matrix<u8> getAccumulator(ExConvCode& encoder)
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


    u64 getGeneratorWeight_(ExConvCode& encoder, bool verbose)
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

                ExConvCode encoder;
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