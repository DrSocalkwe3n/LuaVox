#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "Client/AssetsCacheManager.hpp"
#include "Client/AssetsHeaderCodec.hpp"
#include "Common/Abstract.hpp"
#include "Common/IdProvider.hpp"
#include "Common/AssetsPreloader.hpp"
#include "TOSLib.hpp"
#include "boost/asio/io_context.hpp"
#include <fstream>

namespace LV::Client {

namespace fs = std::filesystem;

class AssetsManager : public IdProvider<EnumAssets> {
public:
    struct ResourceUpdates {
        /// TODO: Добавить анимацию из меты
        std::vector<std::tuple<ResourceId, uint16_t, uint16_t, std::vector<uint32_t>>> Textures;
    };

public:
    AssetsManager(asio::io_context& ioc, fs::path cachePath)
    : Cache(AssetsCacheManager::Create(ioc, cachePath)) {
    }

// Ручные обновления
    struct Out_checkAndPrepareResourcesUpdate {
        AssetsPreloader::Out_checkAndPrepareResourcesUpdate RP, ES;

        std::unordered_map<ResourceFile::Hash_t, std::u8string> Files;
    };

    Out_checkAndPrepareResourcesUpdate checkAndPrepareResourcesUpdate(
        const std::vector<fs::path>& resourcePacks,
        const std::vector<fs::path>& extraSources
    ) {
        Out_checkAndPrepareResourcesUpdate result;

        result.RP = ResourcePacks.checkAndPrepareResourcesUpdate(
            AssetsPreloader::AssetsRegister{resourcePacks},
            [&](EnumAssets type, std::string_view domain, std::string_view key) -> ResourceId {
                return getId(type, domain, key);
            },
            [&](std::u8string&& data, ResourceFile::Hash_t hash, fs::path path) {
                result.Files.emplace(hash, std::move(data));
            }
        );

        result.ES = ExtraSource.checkAndPrepareResourcesUpdate(
            AssetsPreloader::AssetsRegister{resourcePacks},
            [&](EnumAssets type, std::string_view domain, std::string_view key) -> ResourceId {
                return getId(type, domain, key);
            }
        );

        return result;
    }

    struct Out_applyResourcesUpdate {
        
    };

    Out_applyResourcesUpdate applyResourcesUpdate(const Out_checkAndPrepareResourcesUpdate& orr) {
        Out_applyResourcesUpdate result;
            
        ResourcePacks.applyResourcesUpdate(orr.RP);
        ExtraSource.applyResourcesUpdate(orr.ES);

        std::unordered_set<ResourceFile::Hash_t> needHashes;

        for(size_t type = 0; type < static_cast<size_t>(EnumAssets::MAX_ENUM); ++type) {
            for(const auto& res : orr.RP.ResourceUpdates[type]) {
                // Помечаем ресурс для обновления
                PendingUpdateFromAsync[type].push_back(std::get<ResourceId>(res));
            }

            for(ResourceId id : orr.RP.LostLinks[type]) {
                // Помечаем ресурс для обновления
                PendingUpdateFromAsync[type].push_back(id);

                auto& hh = ServerIdToHH[type];
                if(id < hh.size())
                    needHashes.insert(std::get<ResourceFile::Hash_t>(hh[id]));
            }
        }

        {
            for(const auto& [hash, data] : orr.Files) {
                WaitingHashes.insert(hash);
            }

            for(const auto& hash : WaitingHashes)
                needHashes.erase(hash);

            std::vector<std::tuple<ResourceFile::Hash_t, fs::path>> toDisk;
            std::vector<ResourceFile::Hash_t> toCache;

            // Теперь раскидаем хеши по доступным источникам.
            for(const auto& hash : needHashes) {
                auto iter = HashToPath.find(hash);
                if(iter != HashToPath.end()) {
                    // Ставим задачу загрузить с диска.
                    toDisk.emplace_back(hash, iter->second.front());
                } else {
                    // Сделаем запрос в кеш.
                    toCache.push_back(hash);
                }
            }

            // Запоминаем, что эти ресурсы уже ожидаются.
            WaitingHashes.insert_range(needHashes);

            // Запрос в кеш (если там не найдётся, то запрос уйдёт на сервер).
            if(!toCache.empty())
                Cache->pushReads(std::move(toCache));

            // Запрос к диску.
            if(!toDisk.empty())
                NeedToReadFromDisk.append_range(std::move(toDisk));

            _onHashLoad(orr.Files);
        }

        return result;
    }

// ServerSession
    // Новые привязки ассетов к Домен+Ключ.
    void pushAssetsBindDK(
        const std::vector<std::string>& domains,
        const std::array<
            std::vector<std::vector<std::string>>,
            static_cast<size_t>(EnumAssets::MAX_ENUM)
        >& keys
    ) {
        for(size_t type = 0; type < static_cast<size_t>(EnumAssets::MAX_ENUM); ++type) {
            for(size_t forDomainIter = 0; forDomainIter < keys[type].size(); ++forDomainIter) {
                for(const std::string& key : keys[type][forDomainIter]) {
                    ServerToClientMap[type].push_back(getId((EnumAssets) type, domains[forDomainIter], key));
                }
            }
        }
    }

    // Новые привязки ассетов к Hash+Header.
    void pushAssetsBindHH(
        std::array<
            std::vector<std::tuple<ResourceId, ResourceFile::Hash_t, ResourceHeader>>,
            static_cast<size_t>(EnumAssets::MAX_ENUM)
        >&& hash_and_headers
    ) {
        std::array<
            std::vector<std::tuple<ResourceId, ResourceFile::Hash_t, ResourceHeader>>,
            static_cast<size_t>(EnumAssets::MAX_ENUM)
        > hah = std::move(hash_and_headers);

        std::unordered_set<ResourceFile::Hash_t> needHashes;

        for(size_t type = 0; type < static_cast<size_t>(EnumAssets::MAX_ENUM); ++type) {
            size_t maxSize = 0;

            for(auto& [id, hash, header] : hash_and_headers[type]) {
                assert(id < ServerToClientMap[type].size());
                id = ServerToClientMap[type][id];

                if(id > maxSize)
                    maxSize = id+1;

                // Добавляем идентификатор в таблицу ожидающих обновлений.
                PendingUpdateFromAsync[type].push_back(id);

                // Поискать есть ли ресурс в ресурспаках.
                std::optional<AssetsPreloader::Out_Resource> res = ResourcePacks.getResource((EnumAssets) type, id);
                if(res) {
                    needHashes.insert(res->Hash);
                } else {
                    needHashes.insert(hash);
                }
            }

            {
                // Уберём повторения в идентификаторах.
                auto& vec = PendingUpdateFromAsync[type];
                std::sort(vec.begin(), vec.end());
                vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
            }

            if(ServerIdToHH[type].size() < maxSize)
                ServerIdToHH[type].resize(maxSize);

            for(auto& [id, hash, header] : hash_and_headers[type]) {
                ServerIdToHH[type][id] = {hash, std::move(header)};
            }
        }

        // Нужно убрать хеши, которые уже запрошены
        // needHashes ^ WaitingHashes.

        for(const auto& hash : WaitingHashes)
            needHashes.erase(hash);

        std::vector<std::tuple<ResourceFile::Hash_t, fs::path>> toDisk;
        std::vector<ResourceFile::Hash_t> toCache;

        // Теперь раскидаем хеши по доступным источникам.
        for(const auto& hash : needHashes) {
            auto iter = HashToPath.find(hash);
            if(iter != HashToPath.end()) {
                // Ставим задачу загрузить с диска.
                toDisk.emplace_back(hash, iter->second.front());
            } else {
                // Сделаем запрос в кеш.
                toCache.push_back(hash);
            }
        }

        // Запоминаем, что эти ресурсы уже ожидаются.
        WaitingHashes.insert_range(needHashes);

        // Запрос к диску.
        if(!toDisk.empty())
            NeedToReadFromDisk.append_range(std::move(toDisk));

        // Запрос в кеш (если там не найдётся, то запрос уйдёт на сервер).
        if(!toCache.empty())
            Cache->pushReads(std::move(toCache));
    }

    // Новые ресурсы, полученные с сервера.
    void pushNewResources(
        std::vector<std::tuple<ResourceFile::Hash_t, std::u8string>> &&resources
    ) {
        std::unordered_map<ResourceFile::Hash_t, std::u8string> files;
        std::vector<Resource> vec;
        files.reserve(resources.size());
        vec.reserve(resources.size());

        for(auto& [hash, res] : resources) {
            vec.emplace_back(std::move(res));
            files.emplace(hash, res);
        }

        _onHashLoad(files);
        Cache->pushResources(std::move(vec));
    }

    // Для запроса отсутствующих ресурсов с сервера на клиент.
    std::vector<ResourceFile::Hash_t> pollNeededResources() {
        return std::move(NeedToRequestFromServer);
    }
    
    // Получить изменённые ресурсы (для передачи другим модулям).
    ResourceUpdates pullResourceUpdates() {
        return std::move(RU);
    }

    void tick() {
        // Проверим кеш
        std::vector<std::pair<Hash_t, std::optional<Resource>>> resources = Cache->pullReads();
        if(!resources.empty()) {
            std::unordered_map<ResourceFile::Hash_t, std::u8string> needToProceed;
            needToProceed.reserve(resources.size());

            for(auto& [hash, res] : resources) {
                if(!res)
                    NeedToRequestFromServer.push_back(hash);
                else
                    needToProceed.emplace(hash, std::u8string{(const char8_t*) res->data(), res->size()});
            }

            if(!needToProceed.empty())
                _onHashLoad(needToProceed);
        }

        // Почитаем с диска
        if(!NeedToReadFromDisk.empty()) {
            std::unordered_map<ResourceFile::Hash_t, std::u8string> files;
            for(const auto& [hash, path] : NeedToReadFromDisk) {
                std::u8string data;
                std::ifstream file(path, std::ios::binary);
                if(file) {
                    file.seekg(0, std::ios::end);
                    std::streamoff size = file.tellg();
                    if(size < 0)
                        size = 0;
                    file.seekg(0, std::ios::beg);
                    data.resize(static_cast<size_t>(size));
                    if(size > 0) {
                        file.read(reinterpret_cast<char*>(data.data()), size);
                        if(!file)
                            data.clear();
                    }
                }
                files.emplace(hash, std::move(data));
            }

            NeedToReadFromDisk.clear();
            _onHashLoad(files);
        }
    }

private:
    // Менеджеры учёта дисковых ресурсов
    AssetsPreloader
        // В приоритете ищутся ресурсы из ресурспаков по Domain+Key.
        ResourcePacks,
        /*
            Дополнительные источники ресурсов.
            Используется для поиска ресурса по хешу от сервера (может стоит тот же мод с совпадающими ресурсами),
            или для временной подгрузки ресурса по Domain+Key пока ресурс не был получен с сервера.
        */
        ExtraSource;

    // Менеджер файлового кэша.
    AssetsCacheManager::Ptr Cache;

    // Указатели на доступные ресурсы
    std::unordered_map<ResourceFile::Hash_t, std::vector<fs::path>> HashToPath;

    // Таблица релинковки ассетов с идентификаторов сервера на клиентские.
    std::array<
        std::vector<ResourceId>,
        static_cast<size_t>(EnumAssets::MAX_ENUM)
    > ServerToClientMap;
    
    // Таблица серверных привязок HH (id клиентские)
    std::array<
        std::vector<std::tuple<ResourceFile::Hash_t, ResourceHeader>>,
        static_cast<size_t>(EnumAssets::MAX_ENUM)
    > ServerIdToHH;

    // Ресурсы в ожидании данных по хешу для обновления (с диска, кеша, сервера).
    std::array<
        std::vector<ResourceId>,
        static_cast<size_t>(EnumAssets::MAX_ENUM)
    > PendingUpdateFromAsync;

    // Хеши, для которых где-то висит задача на загрузку.
    std::unordered_set<ResourceFile::Hash_t> WaitingHashes;

    // Хеши, которые необходимо запросить с сервера.
    std::vector<ResourceFile::Hash_t> NeedToRequestFromServer;

    // Ресурсы, которые нужно считать с диска
    std::vector<std::tuple<ResourceFile::Hash_t, fs::path>> NeedToReadFromDisk;

    // Обновлённые ресурсы
    ResourceUpdates RU;

    // Когда данные были получены с диска, кеша или сервера
    void _onHashLoad(const std::unordered_map<ResourceFile::Hash_t, std::u8string>& files) {
        /// TODO: скомпилировать ресурсы

        for(const auto& [hash, res] : files)
            WaitingHashes.erase(hash);
    }
};

} // namespace LV::Client
