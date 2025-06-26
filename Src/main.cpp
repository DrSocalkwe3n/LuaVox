#include <iostream>
#include <boost/asio.hpp>
#include <Client/Vulkan/Vulkan.hpp>

#include <Client/ResourceCache.hpp>

namespace LV {

/*
	База ресурсов на стороне клиента
	Протокол получения ресурсов, удаления, потом -> регулировки размера

*/


using namespace TOS;

int main() {

	// LuaVox
	asio::io_context ioc;

	{
		LV::Client::CacheHandlerBasic::Ptr handler = LV::Client::CacheHandlerBasic::Create(ioc, "cache");
	}
	//LV::Client::VK::Vulkan vkInst(ioc);
	ioc.run();

	return 0;
}


}

int main() {
    TOS::Logger::addLogOutput(".*", TOS::EnumLogType::All);
	TOS::Logger::addLogFile(".*", TOS::EnumLogType::All, "log.raw");
	
	std::cout << "Hello world!" << std::endl;
	return LV::main();

	return 0;
}
