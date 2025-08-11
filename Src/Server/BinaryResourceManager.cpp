#include "Common/Abstract.hpp"
#include "Server/Abstract.hpp"
#include <filesystem>
#include <unordered_map>
#define BOOST_ASIO_HAS_IO_URING 1
#include "BinaryResourceManager.hpp"
#include <memory>
#include <boost/system.hpp>
#include <boost/asio.hpp>
#include <boost/asio/stream_file.hpp>
#include <TOSLib.hpp>
#include <fstream>


namespace LV::Server {

struct UriParse {
    std::string Protocol, Path;

    std::string getFull() const {
        return Protocol + "://" + Path;
    }
};

UriParse parseUri(const std::string& uri) {
    size_t pos = uri.find("://");

    if(pos == std::string::npos)
        return {"assets", uri};
    else
        return {uri.substr(0, pos), uri.substr(pos+3)};
}

struct Resource {
    // Файл загруженный с диска
    std::shared_ptr<ResourceFile> Loaded;
    // Источник
    Hash_t Hash;
    size_t LastUsedTime = 0;
    EnumBinResource Type;
    ResourceId_t ResId;
    UriParse Uri;

    std::string LastError;
};

void BinaryResourceManager::run() {
    TOS::Logger LOG = "BinaryResourceManager::run";
    LOG.debug() << "Поток чтения двоичных ресурсов запущен";

    // Ресурсы - кешированные в оперативную память или в процессе загрузки
    std::unordered_map<ResourceId_t, std::shared_ptr<Resource>> knownResource[(int) EnumBinResource::MAX_ENUM];
    // Трансляция идентификаторов в Uri (противоположность KnownResource)
    std::vector<std::string> resIdToUri[(int) EnumBinResource::MAX_ENUM];
    // Новые полученные идентификаторы и те, чьи ресурсы нужно снова загрузить
    std::vector<ResourceId_t> newRes[(int) EnumBinResource::MAX_ENUM];
    // Пути поиска ресурсов
    std::vector<fs::path> assets;
    // Запросы хешей
    std::vector<ResourceId_t> binToHash[(int) EnumBinResource::MAX_ENUM];
    //
    std::unordered_map<Hash_t, std::shared_ptr<Resource>> hashToResource;
    std::vector<Hash_t> hashToLoad;

    auto lambdaLoadResource = [&](UriParse uri, int type) -> std::variant<std::shared_ptr<ResourceFile>, std::string>
    {
        // std::shared_ptr<Resource> resObj = std::make_shared<Resource>();
        // knownResource[type][resId] = resObj;

        if(uri.Protocol != "assets")
            return "Протокол не поддерживается";
        else {
            auto var = loadFile(assets, uri.Path, (EnumBinResource) type);

            if(var.index() == 0) {
                std::shared_ptr<ResourceFile> resource = std::get<0>(var);
                resource = convertFormate(resource, (EnumBinResource) type);
                resource->calcHash();
                return resource;
            } else {
                return std::get<1>(var);
            }
        }
    };

    try {
        while(!NeedShutdown) {
            bool hasWork = false;
            auto lock = Local.lock();

            for(int type = 0; type < (int) EnumBinResource::MAX_ENUM; type++) {
                for(ResourceId_t iter = 0; iter < lock->ResIdToUri[type].size(); iter++) {
                    newRes[type].push_back(resIdToUri[type].size()+iter);
                }

                resIdToUri[type].insert(resIdToUri[type].end(), lock->ResIdToUri[type].begin(), lock->ResIdToUri[type].end());
                resIdToUri[type].clear();

                binToHash[type].insert(binToHash[type].end(), lock->BinToHash[type].begin(), lock->BinToHash[type].end());
                lock->BinToHash[type].clear();
            }

            bool assetsUpdate = false;
            if(!lock->Assets.empty()) {
                // Требуется пересмотр всех ресурсов
                assets = std::move(lock->Assets);
                assetsUpdate = true;
            }

            std::vector<Hash_t> hashRequest = std::move(lock->Hashes);

            lock.unlock();

            if(!hashRequest.empty()) {
                std::vector<std::shared_ptr<ResourceFile>> hashToResourceOut;

                for(Hash_t hash : hashRequest) {
                    auto iter = hashToResource.find(hash);
                    
                    if(iter == hashToResource.end())
                        continue;

                    if(!iter->second->Loaded) {
                        hashToLoad.push_back(hash);
                        continue;
                    }

                    iter->second->LastUsedTime = TOS::Time::getSeconds();
                    hashToResourceOut.push_back(iter->second->Loaded);
                }

                auto outLock = Out.lock();
                outLock->HashToResource.insert(outLock->HashToResource.end(), hashToResourceOut.begin(), hashToResourceOut.end());
            }

            {
                std::unordered_map<ResourceId_t, ResourceFile::Hash_t> binToHashOut[(int) EnumBinResource::MAX_ENUM];

                for(int type = 0; type < (int) EnumBinResource::MAX_ENUM; type++) {
                    for(ResourceId_t resId : binToHash[type]) {
                        std::shared_ptr<Resource> resource = knownResource[type][resId];
                        if(!resource)
                            continue; // Идентификатор не известен

                        binToHashOut[type][resId] = resource->Hash;
                    }
                }
            }

            // Загрузка ресурсов по новым идентификаторам
            for(int type = 0; type < (int) EnumBinResource::MAX_ENUM; type++) {
                if(newRes[type].empty())
                    continue;

                hasWork = true;

                while(!newRes[type].empty()) {
                    ResourceId_t resId = newRes[type].back();
                    newRes[type].pop_back();

                    assert(resId < resIdToUri[type].size());
                    UriParse uri = parseUri(resIdToUri[type][resId]);
                    std::shared_ptr<Resource> resObj = std::make_shared<Resource>();
                    resObj->LastUsedTime = TOS::Time::getSeconds();
                    resObj->Type = (EnumBinResource) type;
                    resObj->ResId = resId;
                    resObj->Uri = uri;

                    auto var = lambdaLoadResource(uri, type);

                    if(var.index() == 0) {
                        resObj->Loaded = std::get<0>(var);
                        resObj->Hash = resObj->Loaded->Hash;
                        hashToResource[resObj->Hash] = resObj;
                    } else {
                        std::fill(resObj->Hash.begin(), resObj->Hash.end(), 0);
                        resObj->LastError = std::get<1>(var);
                    }
                    
                    knownResource[type][resId] = resObj;
                }
            }

            while(!hashToLoad.empty()) {
                Hash_t hash = hashToLoad.back();
                hashToLoad.pop_back();

                auto iter = hashToResource.find(hash);
                if(iter == hashToResource.end())
                    continue;

                std::shared_ptr<Resource> &res = iter->second;

                if(res->Loaded) {
                    Out.lock()->HashToResource.push_back(res->Loaded);
                } else {
                    if(!res->LastError.empty())
                        continue;

                    auto var = lambdaLoadResource(res->Uri, (int) res->Type);
                    if(var.index() == 0) {
                        hasWork = true;
                        res->Loaded = std::get<0>(var);
                        res->LastUsedTime = TOS::Time::getSeconds();
                        res->LastError.clear();

                        if(res->Hash != res->Loaded->Hash) {
                            // Хеш изменился
                            Out.lock()->BinToHash[(int) res->Type][res->ResId] = res->Loaded->Hash;
                            res->Hash = res->Loaded->Hash;
                            std::shared_ptr<Resource> resObj = res;
                            hashToResource.erase(iter);
                            hashToResource[hash] = resObj;
                        } else {
                            Out.lock()->HashToResource.push_back(res->Loaded);
                        }
                    } else {
                        res->LastError = std::get<1>(var);
                    }
                }
            }

            // Удаляем долго не используемые ресурсы
            {
                size_t now = TOS::Time::getSeconds();
                for(int type = 0; type < (int) EnumBinResource::MAX_ENUM; type++) {
                    for(auto& resObj : knownResource[type]) {
                        if(now - resObj.second->LastUsedTime > 30)
                            resObj.second->Loaded = nullptr;
                    }
                }
            }

            if(assetsUpdate) {
                hashToLoad.clear();
                hashToResource.clear();

                for(int type = 0; type < (int) EnumBinResource::MAX_ENUM; type++) {
                    for(auto& [resId, resObj] : knownResource[type]) {
                        auto var = lambdaLoadResource(resObj->Uri, type);

                        if(var.index() == 0) {
                            hasWork = true;
                            resObj->Loaded = std::get<0>(var);
                            resObj->LastUsedTime = TOS::Time::getSeconds();
                            resObj->LastError.clear();

                            if(resObj->Hash != resObj->Loaded->Hash) {
                                // Хеш изменился
                                Out.lock()->BinToHash[type][resId] = resObj->Loaded->Hash;
                                resObj->Hash = resObj->Loaded->Hash;
                            }

                            hashToResource[resObj->Hash] = resObj;
                        } else {
                            resObj->LastError = std::get<1>(var);
                        }
                    }
                }
            }

            if(!hasWork)
                TOS::Time::sleep3(10);
        }
    } catch(const std::exception& exc) {
        LOG.error() << exc.what();
    }

    NeedShutdown = true;
    LOG.debug() << "Поток чтения двоичных ресурсов остановлен";
}

std::variant<std::shared_ptr<ResourceFile>, std::string>
BinaryResourceManager::loadFile(const std::vector<fs::path>& assets, const std::string& path, EnumBinResource type)
{
    try {
        std::shared_ptr<ResourceFile> file = std::make_shared<ResourceFile>();
        
        std::string firstPath;

        switch(type) {
        case EnumBinResource::Texture:      firstPath = "texture"; break;
        case EnumBinResource::Animation:    firstPath = "animation"; break;
        case EnumBinResource::Model:        firstPath = "model"; break;
        case EnumBinResource::Sound:        firstPath = "sound"; break;
        case EnumBinResource::Font:         firstPath = "font"; break;
        default: assert(false);
        }

        for(fs::path assetsPath : assets) {
            fs::path p = assetsPath / firstPath / path;
            if(!fs::exists(p))
                continue;

            std::ifstream fd(p);

            if(!fd) 
                MAKE_ERROR("Не удалось открыть файл: " << p.string());
            
            fd.seekg(0, std::ios::end);
            std::streamsize size = fd.tellg();
            fd.seekg(0, std::ios::beg);
            file->Data.resize(size);
            fd.read((char*) file->Data.data(), size);

            return file;
        }

        MAKE_ERROR("Файл не найден");
    } catch(const std::exception& exc) {
        return exc.what();
    }
}

std::shared_ptr<ResourceFile> convertFormate(std::shared_ptr<ResourceFile> file, EnumBinResource type) {
    return file;
}

BinaryResourceManager::BinaryResourceManager(asio::io_context &ioc)
    : AsyncObject(ioc), Thread(&BinaryResourceManager::run, this)
{
}

BinaryResourceManager::~BinaryResourceManager() {
    NeedShutdown = true;
    Thread.join();
}

void BinaryResourceManager::recheckResources(std::vector<fs::path> assets /* Пути до активных папок assets */) {

}

void BinaryResourceManager::needResourceResponse(const ResourceRequest& resources) {
    auto lock = Local.lock();
    for(int iter = 0; iter < (int) EnumBinResource::MAX_ENUM; iter++) {
        lock->BinToHash[iter].insert(lock->BinToHash[iter].end(), resources.BinToHash[iter].begin(), resources.BinToHash[iter].end());
    }

    lock->Hashes.insert(lock->Hashes.end(), resources.Hashes.begin(), resources.Hashes.end());
}

void BinaryResourceManager::update(float dtime) {
    // if(UpdatedResources.no_lock_readable().empty())
    //     return;

    // auto lock = UpdatedResources.lock_write();
    // for(ResourceId_t resId : *lock) {
    //     auto iterPI = PreparedInformation.find(resId);
    //     if(iterPI != PreparedInformation.end())
    //         continue;

    //     auto iterRI = ResourcesInfo.find(resId);
    //     if(iterRI != ResourcesInfo.end()) {
    //         PreparedInformation[resId] = iterRI->second->Loaded;
    //     }
    // }
}

// coro<> BinaryResourceManager::checkResource_Assets(ResourceId_t id, fs::path path, std::shared_ptr<Resource> res) {
//     try {
//         asio::stream_file fd(IOC, path, asio::stream_file::flags::read_only);

//         if(fd.size() > 1024*1024*16)
//             MAKE_ERROR("Превышен лимит размера файла: " << fd.size() << " > " << 1024*1024*16);

//         std::shared_ptr<ResourceFile> file = std::make_shared<ResourceFile>();
//         file->Data.resize(fd.size());
//         co_await asio::async_read(fd, asio::mutable_buffer(file->Data.data(), file->Data.size()));
//         file->calcHash();
//         res->LastError.clear();
//     } catch(const std::exception &exc) {
//         res->LastError = exc.what();
//         res->IsLoading = false;

//         if(const boost::system::system_error *errc = dynamic_cast<const boost::system::system_error*>(&exc); errc && errc->code() == asio::error::operation_aborted)
//             co_return;
//     }

//     res->IsLoading = false;
//     UpdatedResources.lock_write()->push_back(id);
// }

}