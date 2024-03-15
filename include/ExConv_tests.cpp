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

    /*
        Main function to test ExConv encoding and operations
        @param k: Message size
        @param n: Code size
        @param bw: Expander weight. The number of bits in the expander matrix
        @param aw: Accumulator size. The number of bits in the accumulator matrix
        @param sys: Whether the code is systematic or not

        F: Represents the type of the elements in the sequence that will be operated on. 
        This could be a simple type like u8 or u32 (representing unsigned integers of 8 
        or 32 bits, respectively), or a more complex type such as block (often used to 
        represent fixed-size blocks of binary data in cryptographic applications).

        CoeffCtx: Stands for the coefficient context, which is a type that encapsulates 
        the mathematical operations and rules applicable to the elements of type F. This 
        context is necessary for performing algebraic operations in a manner consistent 
        with the specific cryptographic protocol or algorithm being implemented.
    */
    template<typename F, typename CoeffCtx>
    void exConvTest(u64 k, u64 n, u64 bw, u64 aw, bool sys)
    {
        // The ExConv code instance
        ExConvCode code;
        code.config(k, n, bw, aw, sys);
        
        // Offset for systematic coding
        auto accOffset = sys * k;

        // Vectors for testing
        std::vector<F> x1(n), x2(n), x3(n), x4(n);

        // Psuedo-random number generator
        PRNG prng(CCBlock);

        // Initialize vectors with random values
        for (u64 i = 0; i < x1.size(); ++i)
        {
            x1[i] = x2[i] = x3[i] = prng.get();
        }

        // Coefficient context for operations
        CoeffCtx ctx;

        // Generate as many random bytes required to fill the accumulator size aw
        std::vector<u8> rand(divCeil(aw, 8));

        // Test accumulation, comparing methods and ensuring consistency
        for (u64 i = 0; i < n; ++i)
        {   
            // fill rand with random bytes
            prng.get(rand.data(), rand.size());

            /*
            F: The type of elements in the data sequence (e.g., bytes, integers, blocks).
            CoeffCtx: The context providing algebraic operations (e.g., addition, 
                multiplication) on elements of type F.
            rangeCheck: A boolean flag indicating whether to perform boundary checks on 
                indexes.
            
            x1.data(): A pointer to the beginning of the data sequence on which the 
                operation is performed.
            i: The index of the current element in the sequence to be used in the
                accumulation.
            n: The total number of elements in the data sequence.
            rand.data(): A pointer to the first coefficient in the sequence of accumulator 
                coefficients.
            ctx: An instance of CoeffCtx used for performing algebraic operations.

            The following code is a simplified version of the ExConvCode::accOneGen method

            OC_FORCEINLINE void ExConvCode::accOneGen(
                Iter X, u64 i, u64 size, u8* matrixCoeff, CoeffCtx& ctx){
                
                
                auto xi = X + i;      

                
                u64 j = i + 1;

                // If boundary checks are enabled and j exceeds the data sequence size,
                // wrap around to the beginning of the sequence.      
                if (rangeCheck && j >= size)
                    j -= size;
                
                // k: Counter for the number of bits processed in the accumulator coefficient.
                u64 k = 0; 

                // Process chunks of 8 coefficients at a time for efficiency until less 
                // than 8 remain.
                for (; k + 7 < mAccumulatorSize; k += 8)
                {
                    // Perform accumulation operation on a block of 8 elements.
                    accOne8<F, CoeffCtx, rangeCheck>(X, i, j, size, *matrixCoeff++, ctx);

                    // Move to the next block of 8 elements.
                    j += 8;

                    / Check boundaries and wrap around if necessary.
                    if (rangeCheck && j >= size)
                        j -= size;
                }

                // Process any remaining coefficients one at a time.
                for (; k < mAccumulatorSize; )
                {
                    // Get the next accumulator coefficient.
                    auto b = *matrixCoeff++; 

                    // For each bit in the coefficient, check if it's set and perform 
                    // accumulation if so.
                    for (u64 p = 0; p < 8 && k < mAccumulatorSize; ++p, ++k)
                    {
                        // If the current bit is set,
                        if (b & 1) 
                        {
                            // Get an iterator to the target element for accumulation.
                            auto xj = X + j;

                            // Add the value of xi to xj.
                            ctx.plus(*xj, *xj, *xi);
                        }

                        // Shift to the next bit in the coefficient.
                        b >>= 1;

                        // Move to the next element in the sequence.
                        ++j;

                        // Check boundaries and wrap around if necessary.
                        if (rangeCheck && j >= size)
                            j -= size;
                    }
                }

                // Final accumulation step for the last element.
                
                // Get an iterator to the target element for accumulation.
                auto xj = X + j;

                // Add the value of xi to xj.
                ctx.plus(*xj, *xj, *xi);

                // Multiply the result by a constant (likely for normalization or scaling).
                ctx.mulConst(*xj, *xj);
            }
            */
            code.accOneGen<F, CoeffCtx, true>(x1.data(), i, n, rand.data(), ctx);

            if (aw == 24)
                code.accOne<F, CoeffCtx, true, 24>(x2.data(), i, n, rand.data(), ctx);

            u64 j = i + 1;

            assert(aw <= 64);
            u64 bits = 0;
            memcpy(&bits, rand.data(), std::min<u64>(rand.size(), 8));
            for (u64 a = 0; a < aw; ++a, ++j)
            {
                if (bits & 1)
                {
                    ctx.plus(x3[j % n], x3[j % n], x3[i]);
                }
                bits >>= 1;
            }
            ctx.plus(x3[j % n], x3[j % n], x3[i]);
            ctx.mulConst(x3[j % n], x3[j % n]);

            j = i + 1;
            for (u64 a = 0; a <= aw; ++a, ++j)
            {
                //auto j = (i + a + 2) % n;

                if (aw == 24 && x1[j%n] != x2[j % n])
                {
                    std::cout << j % n << " " << ctx.str(x1[j % n]) << " " << ctx.str(x2[j % n]) << std::endl;
                    throw RTE_LOC;
                }

                if (x1[j % n] != x3[j % n])
                {
                    std::cout << j % n << " " << ctx.str(x1[j % n]) << " " << ctx.str(x3[j % n]) << std::endl;
                    throw RTE_LOC;
                }
            }
        }

        x4 = x1;
        u64 size = n - accOffset;

        code.accumulateFixed<F, CoeffCtx, 0>(x1.data() + accOffset, size, ctx, code.mSeed);
        if (code.mAccTwice)
            code.accumulateFixed<F, CoeffCtx, 0>(x1.data() + accOffset, size, ctx, ~code.mSeed);
        if (aw == 24)
        {
            code.accumulateFixed<F, CoeffCtx, 24>(x2.data() + accOffset, size, ctx, code.mSeed);

            if (code.mAccTwice)
                code.accumulateFixed<F, CoeffCtx, 24>(x2.data() + accOffset, size, ctx, ~code.mSeed);

            if (x1 != x2)
            {
                for (u64 i = 0; i < x1.size(); ++i)
                {
                    std::cout << i << " " << ctx.str(x1[i]) << " " << ctx.str(x2[i]) << std::endl;
                }
                throw RTE_LOC;
            }
        }

        {
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
        }

        if (x1 != x3)
        {
            for (u64 i = 0; i < x1.size(); ++i)
            {
                std::cout << i << " " << ctx.str(x1[i]) << " " << ctx.str(x3[i]) << std::endl;
            }
            throw RTE_LOC;
        }


        std::vector<F> y1(k), y2(k);

        if (sys)
        {
            std::copy(x1.data(), x1.data() + k, y1.data());
            y2 = y1;
            code.mExpander.expand<F, CoeffCtx, true>(x1.data() + accOffset, y1.data());
            //using P = std::pair<typename std::vector<F>::const_iterator, typename std::vector<F>::iterator>;
            //auto p = P{ x1.cbegin() + accOffset, y1.begin() };
            //code.mExpander.expandMany<true, CoeffCtx, F>(
            //    std::tuple<P>{ p }
            //);
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

        if (y1 != y2)
            throw RTE_LOC;

        code.dualEncode<F, CoeffCtx>(x4.begin(), {});

        x4.resize(k);
        if (x4 != y1)
            throw RTE_LOC;
    }

    //block mult2(block x, int imm8)
    //{
    //    assert(imm8 < 2);
    //    if (imm8)
    //    {
    //        // mult x[1] * 2

    //    }
    //    else
    //    {
    //        // x[0] * 2
    //        __m128i carry = _mm_slli_si128(x, 8); 
    //        carry = _mm_srli_epi64(carry, 63);  
    //        x = _mm_slli_epi64(x, 1);
    //        return _mm_or_si128(x, carry);

    //        //return _mm_slli_si128(x, 8);
    //    }
    //    //TEMP[i] : = (TEMP1[0] and TEMP2[i])
    //    //    FOR a : = 1 to i
    //    //    TEMP[i] : = TEMP[i] XOR(TEMP1[a] AND TEMP2[i - a])
    //    //    ENDFOR
    //    //dst[i] : = TEMP[i]
    //}


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
            std::cout << "ExConvCode_encode_basic_test" << std::endl;
        }

        for (auto k : K) for (auto r : R) for (auto bw : Bw) for (auto aw : Aw) for (auto sys : { false, true })
        {

            auto n = k * r;

            // Print the parameters
            if (v)
            {
                std::cout << "> k: " << k << " | n: " << n << " | bw: " << bw << " | aw: " << aw << " | sys: " << sys << std::endl;
            }

            // F = u32, CoeffCtx = CoeffCtxInteger
            exConvTest<u32, CoeffCtxInteger>(k, n, bw, aw, sys);
            // print F and CoeffCtx
            if (v)
            {
                std::cout << ">> F: u32 | CoeffCtx: CoeffCtxInteger" << std::endl;
            }
            
            // F = u8, CoeffCtx = CoeffCtxInteger
            exConvTest<u8, CoeffCtxInteger>(k, n, bw, aw, sys);
            // print F and CoeffCtx
            if (v)
            {
                std::cout << ">> F: u8 | CoeffCtx: CoeffCtxInteger" << std::endl;
            }

            // F = block, CoeffCtx = CoeffCtxGF128
            exConvTest<block, CoeffCtxGF128>(k, n, bw, aw, sys);
            // print F and CoeffCtx
            if (v)
            {
                std::cout << ">> F: block | CoeffCtx: CoeffCtxGF128" << std::endl;
            }

            // F = std::array<u8, 4>, CoeffCtx = CoeffCtxArray<u8, 4>
            exConvTest<std::array<u8, 4>, CoeffCtxArray<u8, 4>>(k, n, bw, aw, sys);
            // print F and CoeffCtx
            if (v)
            {
                std::cout << ">> F: std::array<u8, 4> | CoeffCtx: CoeffCtxArray<u8, 4>" << std::endl;
            }
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