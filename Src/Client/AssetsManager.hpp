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
#include <cstring>
#include "Client/AssetsCacheManager.hpp"
#include "Client/AssetsHeaderCodec.hpp"
#include "Common/Abstract.hpp"
#include "Common/IdProvider.hpp"
#include "Common/AssetsPreloader.hpp"
#include "Common/TexturePipelineProgram.hpp"
#include "TOSLib.hpp"
#include "assets.hpp"
#include "boost/asio/io_context.hpp"
#include "png++/image.hpp"
#include <fstream>
#include "Abstract.hpp"

namespace LV::Client {

namespace fs = std::filesystem;

class AssetsManager : public IdProvider<EnumAssets> {
public:
    struct ResourceUpdates {

        std::vector<AssetsModelUpdate> Models;
        std::vector<AssetsNodestateUpdate> Nodestates;
        std::vector<AssetsTextureUpdate> Textures;
        std::vector<AssetsBinaryUpdate> Particles;
        std::vector<AssetsBinaryUpdate> Animations;
        std::vector<AssetsBinaryUpdate> Sounds;
        std::vector<AssetsBinaryUpdate> Fonts;
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
            
        for(size_t type = 0; type < static_cast<size_t>(EnumAssets::MAX_ENUM); ++type) {
            for(ResourceId id : orr.RP.LostLinks[type]) {
                std::optional<AssetsPreloader::Out_Resource> res = ResourcePacks.getResource((EnumAssets) type, id);
                assert(res);

                auto hashIter = HashToPath.find(res->Hash);
                assert(hashIter != HashToPath.end());
                auto& entry = hashIter->second;
                auto iter = std::find(entry.begin(), entry.end(), res->Path);
                assert(iter != entry.end());
                entry.erase(iter);

                if(entry.empty())
                    HashToPath.erase(hashIter);
            }
        }

        ResourcePacks.applyResourcesUpdate(orr.RP);
        ExtraSource.applyResourcesUpdate(orr.ES);

        std::unordered_set<ResourceFile::Hash_t> needHashes;

        for(size_t type = 0; type < static_cast<size_t>(EnumAssets::MAX_ENUM); ++type) {
            for(const auto& res : orr.RP.ResourceUpdates[type]) {
                // Помечаем ресурс для обновления
                PendingUpdateFromAsync[type].push_back(std::get<ResourceId>(res));
                HashToPath[std::get<ResourceFile::Hash_t>(res)].push_back(std::get<fs::path>(res));
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
        LOG.debug() << "BindDK domains=" << domains.size();
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
        std::unordered_set<ResourceFile::Hash_t> needHashes;

        size_t totalBinds = 0;
        for(size_t type = 0; type < static_cast<size_t>(EnumAssets::MAX_ENUM); ++type) {
            size_t maxSize = 0;

            for(auto& [id, hash, header] : hash_and_headers[type]) {
                totalBinds++;
                assert(id < ServerToClientMap[type].size());
                id = ServerToClientMap[type][id];

                if(id >= maxSize)
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

        if(totalBinds)
            LOG.debug() << "BindHH total=" << totalBinds << " wait=" << WaitingHashes.size();

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
            vec.emplace_back(res);
            files.emplace(hash, std::move(res));
        }

        _onHashLoad(files);
        Cache->pushResources(std::move(vec));
    }

    // Для запроса отсутствующих ресурсов с сервера на клиент.
    std::vector<ResourceFile::Hash_t> pullNeededResources() {
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

            if(!NeedToRequestFromServer.empty())
                LOG.debug() << "CacheMiss count=" << NeedToRequestFromServer.size();

            if(!needToProceed.empty())
                _onHashLoad(needToProceed);
        }

        /// Читаем с диска TODO: получилась хрень с определением типа, чтобы получать headless ресурс
        if(!NeedToReadFromDisk.empty()) {
            std::unordered_map<ResourceFile::Hash_t, std::u8string> files;
            files.reserve(NeedToReadFromDisk.size());

            auto detectTypeDomainKey = [&](const fs::path& path, EnumAssets& typeOut, std::string& domainOut, std::string& keyOut) -> bool {
                fs::path cur = path.parent_path();
                for(; !cur.empty(); cur = cur.parent_path()) {
                    std::string name = cur.filename().string();
                    for(size_t typeIndex = 0; typeIndex < static_cast<size_t>(EnumAssets::MAX_ENUM); ++typeIndex) {
                        EnumAssets type = static_cast<EnumAssets>(typeIndex);
                        if(name == ::EnumAssetsToDirectory(type)) {
                            typeOut = type;
                            domainOut = cur.parent_path().filename().string();
                            keyOut = fs::relative(path, cur).generic_string();
                            return true;
                        }
                    }
                }
                return false;
            };

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
                } else {
                    LOG.warn() << "DiskReadFail " << path.string();
                }

                if(!data.empty()) {
                    EnumAssets type{};
                    std::string domain;
                    std::string key;
                    if(detectTypeDomainKey(path, type, domain, key)) {
                        if(type == EnumAssets::Nodestate) {
                            std::string_view view(reinterpret_cast<const char*>(data.data()), data.size());
                            js::object obj = js::parse(view).as_object();
                            HeadlessNodeState hns;
                            auto modelResolver = [&](const std::string_view model) -> AssetsModel {
                                auto [mDomain, mKey] = parseDomainKey(model, domain);
                                return getId(EnumAssets::Model, mDomain, mKey);
                            };
                            hns.parse(obj, modelResolver);
                            data = hns.dump();
                        } else if(type == EnumAssets::Model) {
                            std::string_view view(reinterpret_cast<const char*>(data.data()), data.size());
                            js::object obj = js::parse(view).as_object();
                            HeadlessModel hm;
                            auto modelResolver = [&](const std::string_view model) -> AssetsModel {
                                auto [mDomain, mKey] = parseDomainKey(model, domain);
                                return getId(EnumAssets::Model, mDomain, mKey);
                            };
                            auto textureIdResolver = [&](const std::string_view texture) -> std::optional<uint32_t> {
                                auto [tDomain, tKey] = parseDomainKey(texture, domain);
                                return getId(EnumAssets::Texture, tDomain, tKey);
                            };
                            auto textureResolver = [&](const std::string_view texturePipelineSrc) -> std::vector<uint8_t> {
                                TexturePipelineProgram tpp;
                                if(!tpp.compile(texturePipelineSrc))
                                    return {};
                                tpp.link(textureIdResolver);
                                return tpp.toBytes();
                            };
                            hm.parse(obj, modelResolver, textureResolver);
                            data = hm.dump();
                        }
                    }
                }

                files.emplace(hash, std::move(data));
            }

            NeedToReadFromDisk.clear();
            _onHashLoad(files);
        }
    }

private:
    Logger LOG = "Client>AssetsManager";

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
        const auto& rpLinks = ResourcePacks.getResourceLinks();
        const auto& esLinks = ExtraSource.getResourceLinks();

        auto mapModelId = [&](ResourceId id) -> ResourceId {
            const auto& map = ServerToClientMap[static_cast<size_t>(EnumAssets::Model)];
            if(id >= map.size())
                return 0;

            return map[id];
        };
        auto mapTextureId = [&](ResourceId id) -> ResourceId {
            const auto& map = ServerToClientMap[static_cast<size_t>(EnumAssets::Texture)];
            if(id >= map.size())
                return 0;

            return map[id];
        };
        auto rebindHeader = [&](EnumAssets type, const ResourceHeader& header) -> ResourceHeader {
            if(header.empty())
                return {};

            std::vector<uint8_t> bytes;
            bytes.resize(header.size());
            std::memcpy(bytes.data(), header.data(), header.size());
            std::vector<uint8_t> rebound = AssetsHeaderCodec::rebindHeader(
                type,
                bytes,
                mapModelId,
                mapTextureId,
                [](const std::string&) {}
            );

            return ResourceHeader(reinterpret_cast<const char8_t*>(rebound.data()), rebound.size());
        };

        for(size_t typeIndex = 0; typeIndex < static_cast<size_t>(EnumAssets::MAX_ENUM); ++typeIndex) {
            auto& pending = PendingUpdateFromAsync[typeIndex];
            if(pending.empty())
                continue;

            std::vector<ResourceId> stillPending;
            stillPending.reserve(pending.size());
            size_t updated = 0;
            size_t missingSource = 0;
            size_t missingData = 0;

            for(ResourceId id : pending) {
                ResourceFile::Hash_t hash{};
                ResourceHeader header;
                bool hasSource = false;
                bool localHeader = false;

                if(id < rpLinks[typeIndex].size() && rpLinks[typeIndex][id].IsExist) {
                    hash = rpLinks[typeIndex][id].Hash;
                    header = rpLinks[typeIndex][id].Header;
                    hasSource = true;
                    localHeader = true;
                } else if(id < ServerIdToHH[typeIndex].size()) {
                    std::tie(hash, header) = ServerIdToHH[typeIndex][id];
                    hasSource = true;
                }

                if(!hasSource) {
                    missingSource++;
                    stillPending.push_back(id);
                    continue;
                }

                auto dataIter = files.find(hash);
                if(dataIter == files.end()) {
                    missingData++;
                    stillPending.push_back(id);
                    continue;
                }

                const auto& dkTable = IdToDK[typeIndex];
                std::string domain = "core";
                std::string key;
                if(id < dkTable.size()) {
                    domain = dkTable[id].Domain;
                    key = dkTable[id].Key;
                }

                std::u8string data = dataIter->second;
                EnumAssets type = static_cast<EnumAssets>(typeIndex);
                ResourceHeader finalHeader = localHeader ? header : rebindHeader(type, header);

                if(id == 0)
                    continue;

                if(type == EnumAssets::Nodestate) {
                    HeadlessNodeState ns;
                    ns.load(data);
                    HeadlessNodeState::Header headerParsed;
                    headerParsed.load(finalHeader);
                    RU.Nodestates.push_back({id, std::move(ns), std::move(headerParsed)});
                    updated++;
                } else if(type == EnumAssets::Model) {
                    HeadlessModel hm;
                    hm.load(data);
                    HeadlessModel::Header headerParsed;
                    headerParsed.load(finalHeader);
                    RU.Models.push_back({id, std::move(hm), std::move(headerParsed)});
                    updated++;
                } else if(type == EnumAssets::Texture) {
                    AssetsTextureUpdate entry;
                    entry.Id = id;
                    entry.Header = std::move(finalHeader);
                    if(!data.empty()) {
                        iResource sres(reinterpret_cast<const uint8_t*>(data.data()), data.size());
                        iBinaryStream stream = sres.makeStream();
                        png::image<png::rgba_pixel> img(stream.Stream);
                        entry.Width = static_cast<uint16_t>(img.get_width());
                        entry.Height = static_cast<uint16_t>(img.get_height());
                        entry.Pixels.resize(static_cast<size_t>(entry.Width) * entry.Height);
                        for(uint32_t y = 0; y < entry.Height; ++y) {
                            const auto& row = img.get_pixbuf().operator[](y);
                            for(uint32_t x = 0; x < entry.Width; ++x) {
                                const auto& px = row[x];
                                uint32_t rgba = (uint32_t(px.alpha) << 24)
                                    | (uint32_t(px.red) << 16)
                                    | (uint32_t(px.green) << 8)
                                    | uint32_t(px.blue);
                                entry.Pixels[x + y * entry.Width] = rgba;
                            }
                        }
                    }
                    
                    RU.Textures.push_back(std::move(entry));
                    updated++;
                } else if(type == EnumAssets::Particle) {
                    RU.Particles.push_back({id, std::move(data)});
                    updated++;
                } else if(type == EnumAssets::Animation) {
                    RU.Animations.push_back({id, std::move(data)});
                    updated++;
                } else if(type == EnumAssets::Sound) {
                    RU.Sounds.push_back({id, std::move(data)});
                    updated++;
                } else if(type == EnumAssets::Font) {
                    RU.Fonts.push_back({id, std::move(data)});
                    updated++;
                }
            }

            if(updated || missingSource || missingData) {
                LOG.debug() << "HashLoad type=" << int(typeIndex)
                    << " updated=" << updated
                    << " missingSource=" << missingSource
                    << " missingData=" << missingData;
            }

            pending = std::move(stillPending);
        }

        for(const auto& [hash, res] : files)
            WaitingHashes.erase(hash);
    }
};

} // namespace LV::Client
