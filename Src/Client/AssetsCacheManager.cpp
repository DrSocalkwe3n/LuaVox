#include "AssetsCacheManager.hpp"
#include "Common/Abstract.hpp"
#include "sqlite3.h"
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>
#include <utility>


namespace LV::Client {


AssetsCacheManager::AssetsCacheManager(boost::asio::io_context &ioc, const fs::path &cachePath,
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
            SELECT sha256, size FROM disk_cache ORDER BY last_used ASC;
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_DISK_OLDEST, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_DISK_OLDEST: " << sqlite3_errmsg(DB));
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

        sql = R"(
            DELETE FROM inline_cache WHERE sha256=?;
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_INLINE_REMOVE, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_INLINE_REMOVE: " << sqlite3_errmsg(DB));
        }

        sql = R"(
            SELECT sha256, LENGTH(data) FROM inline_cache ORDER BY last_used ASC;
        )";

        if(sqlite3_prepare_v2(DB, sql, -1, &STMT_INLINE_OLDEST, nullptr) != SQLITE_OK) {
            MAKE_ERROR("Не удалось подготовить запрос STMT_INLINE_OLDEST: " << sqlite3_errmsg(DB));
        }
    }

    LOG.debug() << "Успешно, запускаем поток обработки";
    OffThread = std::thread(&AssetsCacheManager::readWriteThread, this, AUC.use());
    LOG.info() << "Инициализировано хранилище кеша: " << CachePath.c_str();
}

AssetsCacheManager::~AssetsCacheManager() {
    for(sqlite3_stmt* stmt : {
        STMT_DISK_INSERT, STMT_DISK_UPDATE_TIME, STMT_DISK_REMOVE, STMT_DISK_CONTAINS,
        STMT_DISK_SUM, STMT_DISK_COUNT, STMT_DISK_OLDEST, STMT_INLINE_INSERT,
        STMT_INLINE_GET, STMT_INLINE_UPDATE_TIME, STMT_INLINE_SUM,
        STMT_INLINE_COUNT, STMT_INLINE_REMOVE, STMT_INLINE_OLDEST
    }) {
        if(stmt)
            sqlite3_finalize(stmt);
    }

    if(DB)
        sqlite3_close(DB);

    OffThread.join();

    LOG.info() << "Хранилище кеша закрыто";
}

coro<> AssetsCacheManager::asyncDestructor() {
    NeedShutdown = true;
    co_await IAsyncDestructible::asyncDestructor();
}

void AssetsCacheManager::readWriteThread(AsyncUseControl::Lock lock) {
    try {
        [[maybe_unused]] size_t maxCacheDatabaseSize = 0;
        [[maybe_unused]] size_t maxLifeTime = 0;
        bool databaseSizeKnown = false;

        while(!NeedShutdown || !WriteQueue.get_read().empty()) {
            // Получить новые данные
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
                Hash_t hash;

                {
                    auto lock = ReadQueue.lock();
                    hash = lock->front();
                    lock->pop();
                }

                bool finded = false;
                // Поищем в малой базе
                sqlite3_bind_blob(STMT_INLINE_GET, 1, (const void*) hash.data(), 32, SQLITE_STATIC);
                int errc = sqlite3_step(STMT_INLINE_GET);
                if(errc == SQLITE_ROW) {
                    // Есть запись
                    const uint8_t *data = (const uint8_t*) sqlite3_column_blob(STMT_INLINE_GET, 0);
                    int size = sqlite3_column_bytes(STMT_INLINE_GET, 0);
                    Resource res(data, size);
                    finded = true;
                    ReadyQueue.lock()->emplace_back(hash, res);
                } else if(errc != SQLITE_DONE) {
                    sqlite3_reset(STMT_INLINE_GET);
                    MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_INLINE_GET: " << sqlite3_errmsg(DB));
                }

                sqlite3_reset(STMT_INLINE_GET);

                if(finded) {
                    sqlite3_bind_blob(STMT_INLINE_UPDATE_TIME, 1, (const void*) hash.data(), 32, SQLITE_STATIC);
                    sqlite3_bind_int(STMT_INLINE_UPDATE_TIME, 2, time(nullptr));
                    if(sqlite3_step(STMT_INLINE_UPDATE_TIME) != SQLITE_DONE) {
                        sqlite3_reset(STMT_INLINE_UPDATE_TIME);
                        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_INLINE_UPDATE_TIME: " << sqlite3_errmsg(DB));
                    }

                    sqlite3_reset(STMT_INLINE_UPDATE_TIME);
                }

                if(!finded) {
                    // Поищем на диске
                    sqlite3_bind_blob(STMT_DISK_CONTAINS, 1, (const void*) hash.data(), 32, SQLITE_STATIC);
                    errc = sqlite3_step(STMT_DISK_CONTAINS);
                    if(errc == SQLITE_ROW) {
                        // Есть запись
                        std::string hashKey;
                        {
                            std::stringstream ss;
                            ss << std::hex << std::setfill('0') << std::setw(2);
                            for (int i = 0; i < 32; ++i)
                                ss << static_cast<int>(hash[i]);

                            hashKey = ss.str();
                        }
                        
                        finded = true;
                        ReadyQueue.lock()->emplace_back(hash, PathFiles / hashKey.substr(0, 2) / hashKey.substr(2));
                    } else if(errc != SQLITE_DONE) {
                        sqlite3_reset(STMT_DISK_CONTAINS);
                        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_DISK_CONTAINS: " << sqlite3_errmsg(DB));
                    }

                    sqlite3_reset(STMT_DISK_CONTAINS);

                    if(finded) {
                        sqlite3_bind_int(STMT_DISK_UPDATE_TIME, 1, time(nullptr));
                        sqlite3_bind_blob(STMT_DISK_UPDATE_TIME, 2, (const void*) hash.data(), 32, SQLITE_STATIC);
                        if(sqlite3_step(STMT_DISK_UPDATE_TIME) != SQLITE_DONE) {
                            sqlite3_reset(STMT_DISK_UPDATE_TIME);
                            MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_DISK_UPDATE_TIME: " << sqlite3_errmsg(DB));
                        }

                        sqlite3_reset(STMT_DISK_UPDATE_TIME);
                    }
                }

                if(!finded) {
                    // Не нашли
                    ReadyQueue.lock()->emplace_back(hash, std::nullopt);
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

                if(!databaseSizeKnown) {
                    size_t diskSize = 0;
                    size_t inlineSize = 0;
                    int errc = sqlite3_step(STMT_DISK_SUM);
                    if(errc == SQLITE_ROW) {
                        if(sqlite3_column_type(STMT_DISK_SUM, 0) != SQLITE_NULL)
                            diskSize = static_cast<size_t>(sqlite3_column_int64(STMT_DISK_SUM, 0));
                    } else if(errc != SQLITE_DONE) {
                        sqlite3_reset(STMT_DISK_SUM);
                        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_DISK_SUM: " << sqlite3_errmsg(DB));
                    }
                    sqlite3_reset(STMT_DISK_SUM);

                    errc = sqlite3_step(STMT_INLINE_SUM);
                    if(errc == SQLITE_ROW) {
                        if(sqlite3_column_type(STMT_INLINE_SUM, 0) != SQLITE_NULL)
                            inlineSize = static_cast<size_t>(sqlite3_column_int64(STMT_INLINE_SUM, 0));
                    } else if(errc != SQLITE_DONE) {
                        sqlite3_reset(STMT_INLINE_SUM);
                        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_INLINE_SUM: " << sqlite3_errmsg(DB));
                    }
                    sqlite3_reset(STMT_INLINE_SUM);

                    DatabaseSize = diskSize + inlineSize;
                    databaseSizeKnown = true;
                }

                if(maxCacheDatabaseSize > 0 && DatabaseSize + res.size() > maxCacheDatabaseSize) {
                    size_t bytesToFree = DatabaseSize + res.size() - maxCacheDatabaseSize;

                    sqlite3_reset(STMT_DISK_OLDEST);
                    int errc = SQLITE_ROW;
                    while(bytesToFree > 0 && (errc = sqlite3_step(STMT_DISK_OLDEST)) == SQLITE_ROW) {
                        const void* data = sqlite3_column_blob(STMT_DISK_OLDEST, 0);
                        int dataSize = sqlite3_column_bytes(STMT_DISK_OLDEST, 0);
                        if(data && dataSize == 32) {
                            Hash_t hash;
                            std::memcpy(hash.data(), data, 32);
                            size_t entrySize = static_cast<size_t>(sqlite3_column_int64(STMT_DISK_OLDEST, 1));

                            std::string hashKey = hashToString(hash);
                            fs::path end = PathFiles / hashKey.substr(0, 2) / hashKey.substr(2);
                            std::error_code ec;
                            fs::remove(end, ec);

                            sqlite3_bind_blob(STMT_DISK_REMOVE, 1, (const void*) hash.data(), 32, SQLITE_STATIC);
                            if(sqlite3_step(STMT_DISK_REMOVE) != SQLITE_DONE) {
                                sqlite3_reset(STMT_DISK_REMOVE);
                                MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_DISK_REMOVE: " << sqlite3_errmsg(DB));
                            }

                            sqlite3_reset(STMT_DISK_REMOVE);

                            if(DatabaseSize >= entrySize)
                                DatabaseSize -= entrySize;
                            else
                                DatabaseSize = 0;

                            if(bytesToFree > entrySize)
                                bytesToFree -= entrySize;
                            else
                                bytesToFree = 0;
                        }
                    }
                    if(errc != SQLITE_DONE && errc != SQLITE_ROW) {
                        sqlite3_reset(STMT_DISK_OLDEST);
                        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_DISK_OLDEST: " << sqlite3_errmsg(DB));
                    }
                    sqlite3_reset(STMT_DISK_OLDEST);

                    sqlite3_reset(STMT_INLINE_OLDEST);
                    errc = SQLITE_ROW;
                    while(bytesToFree > 0 && (errc = sqlite3_step(STMT_INLINE_OLDEST)) == SQLITE_ROW) {
                        const void* data = sqlite3_column_blob(STMT_INLINE_OLDEST, 0);
                        int dataSize = sqlite3_column_bytes(STMT_INLINE_OLDEST, 0);
                        if(data && dataSize == 32) {
                            Hash_t hash;
                            std::memcpy(hash.data(), data, 32);
                            size_t entrySize = static_cast<size_t>(sqlite3_column_int64(STMT_INLINE_OLDEST, 1));

                            sqlite3_bind_blob(STMT_INLINE_REMOVE, 1, (const void*) hash.data(), 32, SQLITE_STATIC);
                            if(sqlite3_step(STMT_INLINE_REMOVE) != SQLITE_DONE) {
                                sqlite3_reset(STMT_INLINE_REMOVE);
                                MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_INLINE_REMOVE: " << sqlite3_errmsg(DB));
                            }

                            sqlite3_reset(STMT_INLINE_REMOVE);

                            if(DatabaseSize >= entrySize)
                                DatabaseSize -= entrySize;
                            else
                                DatabaseSize = 0;

                            if(bytesToFree > entrySize)
                                bytesToFree -= entrySize;
                            else
                                bytesToFree = 0;
                        }
                    }
                    if(errc != SQLITE_DONE && errc != SQLITE_ROW) {
                        sqlite3_reset(STMT_INLINE_OLDEST);
                        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_INLINE_OLDEST: " << sqlite3_errmsg(DB));
                    }
                    sqlite3_reset(STMT_INLINE_OLDEST);
                }

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
                        DatabaseSize += res.size();
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
                    DatabaseSize += res.size();
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

std::string AssetsCacheManager::hashToString(const Hash_t& hash) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto& byte : hash)
        ss << std::setw(2) << static_cast<int>(byte);
    
    return ss.str();
}

}
