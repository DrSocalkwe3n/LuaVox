#include "SaveBackend.hpp"

namespace LV::Server {

IWorldSaveBackend::~IWorldSaveBackend() = default;
IPlayerSaveBackend::~IPlayerSaveBackend() = default;
IAuthSaveBackend::~IAuthSaveBackend() = default;
IModStorageSaveBackend::~IModStorageSaveBackend() = default;
ISaveBackendProvider::~ISaveBackendProvider() = default;

}