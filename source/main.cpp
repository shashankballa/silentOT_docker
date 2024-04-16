#include <ExConv_tests.h>
#include <silentOTutils.h>
// using namespace tests_libOTe;

using namespace osuCrypto;
using namespace std;

int main(int argc, char** argv){
    CLP cmd;
	cmd.parse(argc, argv);

    // Tests ExConvCode
    //ExConvCode_tester(cmd);
    
    // Tests silent Random OT
    silent_ot_test(cmd);

    return 0;
}