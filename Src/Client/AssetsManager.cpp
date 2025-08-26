#include "AssetsManager.hpp"
#include "Common/Abstract.hpp"
#include "sqlite3.h"
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <utility>


namespace LV::Client {


AssetsManager::AssetsManager(boost::asio::io_context &ioc, const fs::path &cachePath,
        size_t maxCacheDirectorySize, size_t maxLifeTime)
    :   IAsyncDestructible(ioc), CachePath(cachePath)
{
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
            SELECT data inline_cache where sha256=?;
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
    })
        if(stmt)
            sqlite3_finalize(stmt);

    if(DB)
        sqlite3_close(DB);
}

coro<> AssetsManager::asyncDestructor() {
    assert(NeedShutdown); // Должен быть вызван нормальный shutdown
    co_await IAsyncDestructible::asyncDestructor();
}

void AssetsManager::readWriteThread(AsyncUseControl::Lock lock) {
    try {
        std::vector<fs::path> assets;
        size_t maxCacheDatabaseSize, maxLifeTime;

        while(!NeedShutdown && !WriteQueue.get_read().empty()) {
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
                        ReadyQueue.lock()->emplace_back(rk.Hash, res);
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
                        ReadyQueue.lock()->emplace_back(rk.Hash, res);
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
                        ReadyQueue.lock()->emplace_back(rk.Hash, PathFiles / hashKey.substr(0, 2) / hashKey.substr(2));
                    } else if(errc != SQLITE_DONE) {
                        sqlite3_reset(STMT_DISK_CONTAINS);
                        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_DISK_CONTAINS: " << sqlite3_errmsg(DB));
                    }

                    sqlite3_reset(STMT_DISK_CONTAINS);

                    if(finded) {
                        sqlite3_bind_blob(STMT_DISK_CONTAINS, 1, (const void*) rk.Hash.data(), 32, SQLITE_STATIC);
                        sqlite3_bind_int(STMT_DISK_CONTAINS, 2, time(nullptr));
                        if(sqlite3_step(STMT_DISK_CONTAINS) != SQLITE_DONE) {
                            sqlite3_reset(STMT_DISK_CONTAINS);
                            MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_DISK_CONTAINS: " << sqlite3_errmsg(DB));
                        }
                        
                        sqlite3_reset(STMT_DISK_CONTAINS);
                    }
                }

                if(!finded) {
                    // Не нашли
                    ReadyQueue.lock()->emplace_back(rk.Hash, std::nullopt);
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
                    sqlite3_bind_blob(STMT_INLINE_INSERT, 1, (const void*) res.hash().data(), 32, SQLITE_STATIC);
                    sqlite3_bind_int(STMT_INLINE_INSERT, 2, time(nullptr));
                    sqlite3_bind_blob(STMT_INLINE_INSERT, 3, res.data(), res.size(), SQLITE_STATIC);
                    if(sqlite3_step(STMT_INLINE_INSERT) != SQLITE_DONE) {
                        sqlite3_reset(STMT_INLINE_INSERT);
                        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_INLINE_INSERT: " << sqlite3_errmsg(DB));
                    }

                    sqlite3_reset(STMT_INLINE_INSERT);
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
                    std::ofstream fd(end, std::ios::binary);
                    fd.write((const char*) res.data(), res.size());

                    if(fd.fail())
                        MAKE_ERROR("Ошибка записи в файл: " << end.string());

                    fd.close();

                    sqlite3_bind_blob(STMT_DISK_INSERT, 1, (const void*) res.hash().data(), 32, SQLITE_STATIC);
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
        }
    } catch(const std::exception& exc) {
        LOG.warn() << "Ошибка в работе потока: " << exc.what();
        IssuedAnError = true;
    }
}

}