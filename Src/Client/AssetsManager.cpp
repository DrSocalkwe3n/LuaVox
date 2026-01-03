#include "AssetsManager.hpp"
#include "Common/Abstract.hpp"
#include "sqlite3.h"
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>
#include <utility>


namespace LV::Client {


AssetsManager::AssetsManager(boost::asio::io_context &ioc, const fs::path &cachePath,
        size_t maxCacheDirectorySize, size_t maxLifeTime)
    :   IAsyncDestructible(ioc), CachePath(cachePath)
{
    NextId.fill(0);
    {
        auto lock = Changes.lock();
        lock->MaxCacheDatabaseSize = maxCacheDirectorySize;
        lock->MaxLifeTime = maxLifeTime;
        lock->MaxChange = true;
    }

    if(!fs::exists(PathFiles)) {
        LOG.debug() << "Директория для хранения кеша отсутствует, создаём новую '" << CachePath << '\'';
        fs::create_directories(PathFiles);
    }

    LOG.debug() << "Открываем базу данных кеша... (инициализация sqlite3)";
    {
        int errc = sqlite3_open_v2(PathDatabase.c_str(), &DB, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, nullptr);
        if(errc)
            MAKE_ERROR("Не удалось открыть базу данных " << PathDatabase.c_str() << ": " << sqlite3_errmsg(DB));


        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS disk_cache(
            sha256          BLOB(32)    NOT NULL,   -- 
            last_used       INT         NOT NULL,   -- unix timestamp
            size            INT         NOT NULL,   -- file size
            UNIQUE (sha256));
            CREATE INDEX IF NOT EXISTS idx__disk_cache__sha256 ON disk_cache(sha256);

            CREATE TABLE IF NOT EXISTS inline_cache(
            sha256          BLOB(32)    NOT NULL,   -- 
            last_used       INT         NOT NULL,   -- unix timestamp
            data            BLOB        NOT NULL,   -- file data
            UNIQUE (sha256));
            CREATE INDEX IF NOT EXISTS idx__inline_cache__sha256 ON inline_cache(sha256);
        )";

        errc = sqlite3_exec(DB, sql, nullptr, nullptr, nullptr);
        if(errc != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить таблицы: " << sqlite3_errmsg(DB));
        }

        sql = R"(
            INSERT OR REPLACE INTO disk_cache (sha256, last_used, size)
            VALUES (?, ?, ?);
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_DISK_INSERT, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_DISK_INSERT: " << sqlite3_errmsg(DB));
        }

        sql = R"(
            UPDATE disk_cache SET last_used = ? WHERE sha256 = ?;
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_DISK_UPDATE_TIME, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_DISK_UPDATE_TIME: " << sqlite3_errmsg(DB));
        }

        sql = R"(
            DELETE FROM disk_cache WHERE sha256=?;
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_DISK_REMOVE, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_DISK_REMOVE: " << sqlite3_errmsg(DB));
        }

        sql = R"(
            SELECT 1 FROM disk_cache where sha256=?;
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_DISK_CONTAINS, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_DISK_CONTAINS: " << sqlite3_errmsg(DB));
        }

        sql = R"(
            SELECT SUM(size) FROM disk_cache;
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_DISK_SUM, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_DISK_SUM: " << sqlite3_errmsg(DB));
        }

        sql = R"(
            SELECT COUNT(*) FROM disk_cache;
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_DISK_COUNT, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_DISK_COUNT: " << sqlite3_errmsg(DB));
        }

        sql = R"(
            INSERT OR REPLACE INTO inline_cache (sha256, last_used, data)
            VALUES (?, ?, ?);
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_INLINE_INSERT, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_INLINE_INSERT: " << sqlite3_errmsg(DB));
        }

        sql = R"(
            SELECT data FROM inline_cache where sha256=?;
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_INLINE_GET, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_INLINE_GET: " << sqlite3_errmsg(DB));
        }

        sql = R"(
            UPDATE inline_cache SET last_used = ? WHERE sha256 = ?;
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_INLINE_UPDATE_TIME, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_INLINE_UPDATE_TIME: " << sqlite3_errmsg(DB));
        }

        sql = R"(
            SELECT SUM(LENGTH(data)) from inline_cache;
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_INLINE_SUM, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_INLINE_SUM: " << sqlite3_errmsg(DB));
        }


        sql = R"(
            SELECT COUNT(*) FROM inline_cache;
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_INLINE_COUNT, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_INLINE_COUNT: " << sqlite3_errmsg(DB));
        }
    }

    LOG.debug() << "Успешно, запускаем поток обработки";
    OffThread = std::thread(&AssetsManager::readWriteThread, this, AUC.use());
    LOG.info() << "Инициализировано хранилище кеша: " << CachePath.c_str();
}

AssetsManager::~AssetsManager() {
    for(sqlite3_stmt* stmt : {
        STMT_DISK_INSERT, STMT_DISK_UPDATE_TIME, STMT_DISK_REMOVE, STMT_DISK_CONTAINS,
        STMT_DISK_SUM, STMT_DISK_COUNT, STMT_INLINE_INSERT, STMT_INLINE_GET,
        STMT_INLINE_UPDATE_TIME, STMT_INLINE_SUM, STMT_INLINE_COUNT
    }) {
        if(stmt)
            sqlite3_finalize(stmt);
    }

    if(DB)
        sqlite3_close(DB);

    OffThread.join();

    LOG.info() << "Хранилище кеша закрыто";
}

ResourceId AssetsManager::getId(EnumAssets type, const std::string& domain, const std::string& key) {
    std::lock_guard lock(MapMutex);
    auto& typeTable = DKToId[type];
    auto& domainTable = typeTable[domain];
    if(auto iter = domainTable.find(key); iter != domainTable.end())
        return iter->second;

    ResourceId id = NextId[(int) type]++;
    domainTable[key] = id;
    return id;
}

std::optional<ResourceId> AssetsManager::getLocalIdFromServer(EnumAssets type, ResourceId serverId) const {
    std::lock_guard lock(MapMutex);
    auto iterType = ServerToLocal.find(type);
    if(iterType == ServerToLocal.end())
        return std::nullopt;
    auto iter = iterType->second.find(serverId);
    if(iter == iterType->second.end())
        return std::nullopt;
    return iter->second;
}

const AssetsManager::BindInfo* AssetsManager::getBind(EnumAssets type, ResourceId localId) const {
    std::lock_guard lock(MapMutex);
    auto iterType = LocalBinds.find(type);
    if(iterType == LocalBinds.end())
        return nullptr;
    auto iter = iterType->second.find(localId);
    if(iter == iterType->second.end())
        return nullptr;
    return &iter->second;
}

AssetsManager::BindResult AssetsManager::bindServerResource(EnumAssets type, ResourceId serverId, const std::string& domain,
    const std::string& key, const Hash_t& hash, std::vector<uint8_t> header)
{
    BindResult result;
    result.LocalId = getId(type, domain, key);

    std::lock_guard lock(MapMutex);
    ServerToLocal[type][serverId] = result.LocalId;

    auto& binds = LocalBinds[type];
    auto iter = binds.find(result.LocalId);
    if(iter == binds.end()) {
        result.Changed = true;
        binds.emplace(result.LocalId, BindInfo{
            .LocalId = result.LocalId,
            .ServerId = serverId,
            .Domain = domain,
            .Key = key,
            .Hash = hash,
            .Header = std::move(header)
        });
        return result;
    }

    BindInfo& info = iter->second;
    bool hashChanged = info.Hash != hash;
    bool headerChanged = info.Header != header;
    result.Changed = hashChanged || headerChanged || info.ServerId != serverId;
    info.ServerId = serverId;
    info.Domain = domain;
    info.Key = key;
    info.Hash = hash;
    info.Header = std::move(header);
    return result;
}

std::optional<ResourceId> AssetsManager::unbindServerResource(EnumAssets type, ResourceId serverId) {
    std::lock_guard lock(MapMutex);
    auto iterType = ServerToLocal.find(type);
    if(iterType == ServerToLocal.end())
        return std::nullopt;
    auto iter = iterType->second.find(serverId);
    if(iter == iterType->second.end())
        return std::nullopt;

    ResourceId localId = iter->second;
    iterType->second.erase(iter);

    auto iterBindType = LocalBinds.find(type);
    if(iterBindType != LocalBinds.end())
        iterBindType->second.erase(localId);

    return localId;
}

void AssetsManager::clearServerBindings() {
    std::lock_guard lock(MapMutex);
    ServerToLocal.clear();
    LocalBinds.clear();
}

std::optional<AssetsManager::ParsedHeader> AssetsManager::parseHeader(const std::vector<uint8_t>& data) {
    size_t pos = 0;
    auto readU8 = [&](uint8_t& out) -> bool {
        if(pos + 1 > data.size())
            return false;
        out = data[pos++];
        return true;
    };
    auto readU32 = [&](uint32_t& out) -> bool {
        if(pos + 4 > data.size())
            return false;
        out = uint32_t(data[pos]) |
              (uint32_t(data[pos + 1]) << 8) |
              (uint32_t(data[pos + 2]) << 16) |
              (uint32_t(data[pos + 3]) << 24);
        pos += 4;
        return true;
    };

    ParsedHeader out;
    uint8_t c0, c1, version, type;
    if(!readU8(c0) || !readU8(c1) || !readU8(version) || !readU8(type))
        return std::nullopt;
    if(c0 != 'a' || c1 != 'h' || version != 1)
        return std::nullopt;
    out.Type = static_cast<EnumAssets>(type);

    uint32_t count = 0;
    if(!readU32(count))
        return std::nullopt;
    out.ModelDeps.reserve(count);
    for(uint32_t i = 0; i < count; i++) {
        uint32_t id;
        if(!readU32(id))
            return std::nullopt;
        out.ModelDeps.push_back(id);
    }

    if(!readU32(count))
        return std::nullopt;
    out.TextureDeps.reserve(count);
    for(uint32_t i = 0; i < count; i++) {
        uint32_t id;
        if(!readU32(id))
            return std::nullopt;
        out.TextureDeps.push_back(id);
    }

    uint32_t extraSize = 0;
    if(!readU32(extraSize))
        return std::nullopt;
    if(pos + extraSize > data.size())
        return std::nullopt;
    out.Extra.assign(data.begin() + pos, data.begin() + pos + extraSize);
    return out;
}

std::vector<uint8_t> AssetsManager::buildHeader(EnumAssets type, const std::vector<uint32_t>& modelDeps,
    const std::vector<uint32_t>& textureDeps, const std::vector<uint8_t>& extra)
{
    std::vector<uint8_t> data;
    data.reserve(4 + 4 + modelDeps.size() * 4 + 4 + textureDeps.size() * 4 + 4 + extra.size());
    data.push_back('a');
    data.push_back('h');
    data.push_back(1);
    data.push_back(static_cast<uint8_t>(type));

    auto writeU32 = [&](uint32_t value) {
        data.push_back(uint8_t(value & 0xff));
        data.push_back(uint8_t((value >> 8) & 0xff));
        data.push_back(uint8_t((value >> 16) & 0xff));
        data.push_back(uint8_t((value >> 24) & 0xff));
    };

    writeU32(static_cast<uint32_t>(modelDeps.size()));
    for(uint32_t id : modelDeps)
        writeU32(id);

    writeU32(static_cast<uint32_t>(textureDeps.size()));
    for(uint32_t id : textureDeps)
        writeU32(id);

    writeU32(static_cast<uint32_t>(extra.size()));
    if(!extra.empty())
        data.insert(data.end(), extra.begin(), extra.end());

    return data;
}

std::vector<uint8_t> AssetsManager::rebindHeader(const std::vector<uint8_t>& header) const {
    auto parsed = parseHeader(header);
    if(!parsed)
        return header;

    std::vector<uint32_t> modelDeps;
    modelDeps.reserve(parsed->ModelDeps.size());
    for(uint32_t serverId : parsed->ModelDeps) {
        auto localId = getLocalIdFromServer(EnumAssets::Model, serverId);
        modelDeps.push_back(localId.value_or(0));
    }

    std::vector<uint32_t> textureDeps;
    textureDeps.reserve(parsed->TextureDeps.size());
    for(uint32_t serverId : parsed->TextureDeps) {
        auto localId = getLocalIdFromServer(EnumAssets::Texture, serverId);
        textureDeps.push_back(localId.value_or(0));
    }

    return buildHeader(parsed->Type, modelDeps, textureDeps, parsed->Extra);
}

coro<> AssetsManager::asyncDestructor() {
    NeedShutdown = true;
    co_await IAsyncDestructible::asyncDestructor();
}

void AssetsManager::readWriteThread(AsyncUseControl::Lock lock) {
    try {
        std::vector<fs::path> assets;
        size_t maxCacheDatabaseSize, maxLifeTime;

        while(!NeedShutdown || !WriteQueue.get_read().empty()) {
            // Получить новые данные
            if(Changes.get_read().AssetsChange) {
                auto lock = Changes.lock();
                assets = std::move(lock->Assets);
                lock->AssetsChange = false;
            }

            if(Changes.get_read().MaxChange) {
                auto lock = Changes.lock();
                maxCacheDatabaseSize = lock->MaxCacheDatabaseSize;
                maxLifeTime = lock->MaxLifeTime;
                lock->MaxChange = false;
            }

            if(Changes.get_read().FullRecheck) {
                std::move_only_function<void(std::string)> onRecheckEnd;

                {
                    auto lock = Changes.lock();
                    onRecheckEnd = std::move(*lock->OnRecheckEnd);
                    lock->FullRecheck = false;
                }

                LOG.info() << "Начата проверка консистентности кеша ассетов";



                LOG.info() << "Завершена проверка консистентности кеша ассетов";
            }

            // Чтение
            if(!ReadQueue.get_read().empty()) {
                ResourceKey rk;

                {
                    auto lock = ReadQueue.lock();
                    rk = lock->front();
                    lock->pop();
                }

                bool finded = false;
                // Сначала пробежимся по ресурспакам
                {
                    std::string_view type;

                    switch(rk.Type) {
                        case EnumAssets::Nodestate:     type = "nodestate"; break;
                        case EnumAssets::Particle:      type = "particle"; break;
                        case EnumAssets::Animation:     type = "animation"; break;
                        case EnumAssets::Model:         type = "model"; break;
                        case EnumAssets::Texture:       type = "texture"; break;
                        case EnumAssets::Sound:         type = "sound"; break;
                        case EnumAssets::Font:          type = "font"; break;
                        default:
                            std::unreachable();
                    }
                    
                    for(const fs::path& path : assets) {
                        fs::path end = path / rk.Domain / type / rk.Key;

                        if(!fs::exists(end))
                            continue;

                        // Нашли
                        finded = true;
                        Resource res = Resource(end).convertToMem();
                        ReadyQueue.lock()->emplace_back(rk, res);
                        break;
                    }
                }

                if(!finded) {
                    // Поищем в малой базе
                    sqlite3_bind_blob(STMT_INLINE_GET, 1, (const void*) rk.Hash.data(), 32, SQLITE_STATIC);
                    int errc = sqlite3_step(STMT_INLINE_GET);
                    if(errc == SQLITE_ROW) {
                        // Есть запись
                        const uint8_t *hash = (const uint8_t*) sqlite3_column_blob(STMT_INLINE_GET, 0);
                        int size = sqlite3_column_bytes(STMT_INLINE_GET, 0);
                        Resource res(hash, size);
                        finded = true;
                        ReadyQueue.lock()->emplace_back(rk, res);
                    } else if(errc != SQLITE_DONE) {
                        sqlite3_reset(STMT_INLINE_GET);
                        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_INLINE_GET: " << sqlite3_errmsg(DB));
                    }

                    sqlite3_reset(STMT_INLINE_GET);

                    if(finded) {
                        sqlite3_bind_blob(STMT_INLINE_UPDATE_TIME, 1, (const void*) rk.Hash.data(), 32, SQLITE_STATIC);
                        sqlite3_bind_int(STMT_INLINE_UPDATE_TIME, 2, time(nullptr));
                        if(sqlite3_step(STMT_INLINE_UPDATE_TIME) != SQLITE_DONE) {
                            sqlite3_reset(STMT_INLINE_UPDATE_TIME);
                            MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_INLINE_UPDATE_TIME: " << sqlite3_errmsg(DB));
                        }

                        sqlite3_reset(STMT_INLINE_UPDATE_TIME);
                    }
                }

                if(!finded) {
                    // Поищем на диске
                    sqlite3_bind_blob(STMT_DISK_CONTAINS, 1, (const void*) rk.Hash.data(), 32, SQLITE_STATIC);
                    int errc = sqlite3_step(STMT_DISK_CONTAINS);
                    if(errc == SQLITE_ROW) {
                        // Есть запись
                        std::string hashKey;
                        {
                            std::stringstream ss;
                            ss << std::hex << std::setfill('0') << std::setw(2);
                            for (int i = 0; i < 32; ++i)
                                ss << static_cast<int>(rk.Hash[i]);

                            hashKey = ss.str();
                        }
                        
                        finded = true;
                        ReadyQueue.lock()->emplace_back(rk, PathFiles / hashKey.substr(0, 2) / hashKey.substr(2));
                    } else if(errc != SQLITE_DONE) {
                        sqlite3_reset(STMT_DISK_CONTAINS);
                        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_DISK_CONTAINS: " << sqlite3_errmsg(DB));
                    }

                    sqlite3_reset(STMT_DISK_CONTAINS);

                    if(finded) {
                        sqlite3_bind_int(STMT_DISK_UPDATE_TIME, 1, time(nullptr));
                        sqlite3_bind_blob(STMT_DISK_UPDATE_TIME, 2, (const void*) rk.Hash.data(), 32, SQLITE_STATIC);
                        if(sqlite3_step(STMT_DISK_UPDATE_TIME) != SQLITE_DONE) {
                            sqlite3_reset(STMT_DISK_UPDATE_TIME);
                            MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_DISK_UPDATE_TIME: " << sqlite3_errmsg(DB));
                        }

                        sqlite3_reset(STMT_DISK_UPDATE_TIME);
                    }
                }

                if(!finded) {
                    // Не нашли
                    ReadyQueue.lock()->emplace_back(rk, std::nullopt);
                }

                continue;
            }

            // Запись
            if(!WriteQueue.get_read().empty()) {
                Resource res;

                {
                    auto lock = WriteQueue.lock();
                    res = lock->front();
                    lock->pop();
                }

                // TODO: добавить вычистку места при нехватке

                if(res.size() <= SMALL_RESOURCE) {
                        Hash_t hash = res.hash();
                    LOG.debug() << "Сохраняем ресурс " << hashToString(hash);

                    try {
                        sqlite3_bind_blob(STMT_INLINE_INSERT, 1, (const void*) hash.data(), 32, SQLITE_STATIC);
                        sqlite3_bind_int(STMT_INLINE_INSERT, 2, time(nullptr));
                        sqlite3_bind_blob(STMT_INLINE_INSERT, 3, res.data(), res.size(), SQLITE_STATIC);
                        if(sqlite3_step(STMT_INLINE_INSERT) != SQLITE_DONE) {
                            sqlite3_reset(STMT_INLINE_INSERT);
                            MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_INLINE_INSERT: " << sqlite3_errmsg(DB));
                        }

                        sqlite3_reset(STMT_INLINE_INSERT);
                    } catch(const std::exception& exc) {
                        LOG.error() << "Произошла ошибка при сохранении " << hashToString(hash);
                        throw;
                    }

                } else {
                    std::string hashKey;
                    {
                        std::stringstream ss;
                        ss << std::hex << std::setfill('0') << std::setw(2);
                        for (int i = 0; i < 32; ++i)
                            ss << static_cast<int>(res.hash()[i]);

                        hashKey = ss.str();
                    }
                    
                    fs::path end = PathFiles / hashKey.substr(0, 2) / hashKey.substr(2);
                    fs::create_directories(end.parent_path());
                    std::ofstream fd(end, std::ios::binary);
                    fd.write((const char*) res.data(), res.size());

                    if(fd.fail())
                        MAKE_ERROR("Ошибка записи в файл: " << end.string());

                    fd.close();

                    Hash_t hash = res.hash();
                    sqlite3_bind_blob(STMT_DISK_INSERT, 1, (const void*) hash.data(), 32, SQLITE_STATIC);
                    sqlite3_bind_int(STMT_DISK_INSERT, 2, time(nullptr));
                    sqlite3_bind_int(STMT_DISK_INSERT, 3, res.size());
                    if(sqlite3_step(STMT_DISK_INSERT) != SQLITE_DONE) {
                        sqlite3_reset(STMT_DISK_INSERT);
                        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_DISK_INSERT: " << sqlite3_errmsg(DB));
                    }

                    sqlite3_reset(STMT_DISK_INSERT);
                }

                continue;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } catch(const std::exception& exc) {
        LOG.warn() << "Ошибка в работе потока:\n" << exc.what();
        IssuedAnError = true;
    }
}

std::string AssetsManager::hashToString(const Hash_t& hash) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto& byte : hash)
        ss << std::setw(2) << static_cast<int>(byte);
    
    return ss.str();
}

}
