#include <iostream>
#include <boost/asio.hpp>
#include <Client/Vulkan/Vulkan.hpp>

namespace LV {

using namespace TOS;

int main() {

	// LuaVox
	asio::io_context ioc;

	LV::Client::VK::Vulkan vkInst(ioc);
	ioc.run();

	return 0;
}


}

int main() {
    TOS::Logger::addLogOutput(".*", TOS::EnumLogType::All);
	TOS::Logger::addLogFile(".*", TOS::EnumLogType::All, "log.raw");
	
	std::cout << "Hello world!" << std::endl;
	return LV::main();
}
