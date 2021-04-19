
#include <thread>
#include "../dedelib/toString.h"
#include "../dedelib/sjomn.h"
#include "../dedelib/cmdParser.h"
#include "../dedelib/fileNames.h"


int main(int argc, const char** argv) {
	std::cout << "Detected " << std::thread::hardware_concurrency() << " threads" << std::endl;

	ParsedArgs parsed(argc, argv);

	std::string dataDir = parsed.getOptional("dataDir");
	if(!dataDir.empty()) {
		FileName::setDataPath(dataDir);
	}

	sjomnumbertablesymmetric<2>([]() {});
}
