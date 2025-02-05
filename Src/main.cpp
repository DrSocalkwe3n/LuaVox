#include <boost/asio/buffer.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/write.hpp>
#include <iostream>
#include <memory>
#include "Client/ServerSession.hpp"
#include "Common/Net.hpp"
#include "Server/GameServer.hpp"
#include <Client/Vulkan/Vulkan.hpp>

namespace LV {

using namespace TOS;

coro<> runClient(asio::io_context &ioc, uint16_t port) {
	try {
		tcp::socket sock = co_await Net::asyncConnectTo("localhost:"+std::to_string(port));
		co_await Client::ServerSession::asyncAuthorizeWithServer(sock, "DrSocalkwe3n", "1password2", 1);
		std::unique_ptr<Net::AsyncSocket> asock = co_await Client::ServerSession::asyncInitGameProtocol(ioc, std::move(sock));
	} catch(const std::exception &exc) {
		std::cout << exc.what() << std::endl;
	}
}

int main() {

	VK::Vulkan VkInst;
	VkInst.getSettingsNext() = VkInst.getBestSettings();
	VkInst.reInit();

	auto ot = std::async([&](){
		VkInst.start([&](VK::Vulkan *instance, int subpass, VkCommandBuffer &renderCmd)
		{
		});
	});

	// LuaVox

	asio::io_context ioc;

	Server::GameServer gs(ioc, "");

	Net::Server server(ioc, [&](tcp::socket sock) -> coro<> {
		server.stop();
		co_await gs.pushSocketConnect(std::move(sock));
	}, 6666);

	std::cout << server.getPort() << std::endl;

	asio::co_spawn(ioc, runClient(ioc, server.getPort()), asio::detached);

	ioc.run();
	VkInst.shutdown();

	return 0;
}


}

int main() {
    TOS::Logger::addLogOutput(".*", TOS::EnumLogType::All);
	
	std::cout << "Hello world!" << std::endl;
	return LV::main();
}
