#include <GLFW/glfw3.h>
#include <boost/asio/buffer.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/detail/chrono.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/write.hpp>
#include <boost/chrono/duration.hpp>
#include <iostream>
#include <memory>
#include "Client/ServerSession.hpp"
#include "Common/Net.hpp"
#include "Common/Packets.hpp"
#include "Server/GameServer.hpp"
#include <Client/Vulkan/Vulkan.hpp>

namespace LV {

using namespace TOS;

std::unique_ptr<Client::ServerSession> session;

coro<> runClient(asio::io_context &ioc, uint16_t port) {
	try {
		tcp::socket sock = co_await Net::asyncConnectTo("localhost:"+std::to_string(port));
		co_await Client::ServerSession::asyncAuthorizeWithServer(sock, "DrSocalkwe3n", "1password2", 1);
		std::unique_ptr<Net::AsyncSocket> asock = co_await Client::ServerSession::asyncInitGameProtocol(ioc, std::move(sock));
		session = std::make_unique<Client::ServerSession>(ioc, std::move(asock), nullptr);
	} catch(const std::exception &exc) {
		std::cout << exc.what() << std::endl;
	}
}

int main() {

	// LuaVox

	asio::io_context ioc;

	LV::Client::VK::Vulkan vkInst(ioc);
	ioc.run();

	// Server::GameServer gs(ioc, "");

	// Net::Server server(ioc, [&](tcp::socket sock) -> coro<> {
	// 	server.stop();
	// 	co_await gs.pushSocketConnect(std::move(sock));
	// }, 6666);

	// std::cout << server.getPort() << std::endl;

	// asio::co_spawn(ioc, runClient(ioc, server.getPort()), asio::detached);


	// auto ot = std::async([&](){
	// 	VkInst.start([&](VK::Vulkan *instance, int subpass, VkCommandBuffer &renderCmd)
	// 	{
	// 		if(glfwWindowShouldClose(VkInst.Graphics.Window) || (session && !session->isConnected())) {
	// 			VkInst.shutdown();

	// 			if(glfwWindowShouldClose(VkInst.Graphics.Window) && session)
	// 				session->shutdown(EnumDisconnect::ByInterface);
	// 		}
	// 	});

	// 	session = nullptr;
	// });

	// ioc.run();
	// VkInst.shutdown();

	return 0;
}


}

int main() {
    TOS::Logger::addLogOutput(".*", TOS::EnumLogType::All);
	
	std::cout << "Hello world!" << std::endl;
	return LV::main();
}
