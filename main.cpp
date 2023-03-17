
#include <silentOTutils.h>

using namespace osuCrypto;
using namespace std;
using namespace tests_libOTe;


int main(int argc, char** argv){
    CLP cmd;
	cmd.parse(argc, argv);
    silent_ex_test(cmd);
    return 0;
}