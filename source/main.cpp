#include <ExConv_tests.h>
#include <silentOTutils.h>
// using namespace tests_libOTe;

using namespace osuCrypto;
using namespace std;

int main(int argc, char** argv){
    CLP cmd;
	cmd.parse(argc, argv);

    // Tests ExConvCode
    // ExConvCode_encode_basic_bitwise_seq_test(cmd);
    
    // Tests silent Random OT
    silent_ot_test(cmd);

    return 0;
}