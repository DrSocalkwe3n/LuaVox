#include "SaveBackend.hpp"

namespace AL::Server {

IWorldSaveBackend::~IWorldSaveBackend() = default;
IPlayerSaveBackend::~IPlayerSaveBackend() = default;
IAuthSaveBackend::~IAuthSaveBackend() = default;
IModStorageSaveBackend::~IModStorageSaveBackend() = default;
ISaveBackendProvider::~ISaveBackendProvider() = default;

}