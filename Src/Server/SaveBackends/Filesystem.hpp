#pragma once

#include <Server/SaveBackend.hpp>


namespace AL::Server::SaveBackends {

class Filesystem : public ISaveBackendProvider {
public:
    virtual ~Filesystem();

    virtual bool isAvailable() override;
    virtual std::string getName() override;
    virtual std::unique_ptr<IWorldSaveBackend> createWorld(boost::json::object data) override;
    virtual std::unique_ptr<IPlayerSaveBackend> createPlayer(boost::json::object data) override;
    virtual std::unique_ptr<IAuthSaveBackend> createAuth(boost::json::object data) override;
    virtual std::unique_ptr<IModStorageSaveBackend> createModStorage(boost::json::object data) override;
};

}