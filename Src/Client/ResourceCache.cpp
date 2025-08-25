#include "ResourceCache.hpp"
#include "sqlite3.h"
#include <fstream>


namespace LV::Client {


CacheDatabase::CacheDatabase(const fs::path &cachePath) 
    : Path(cachePath)
{
    int errc = sqlite3_open_v2((Path / "db.sqlite3").c_str(), &DB, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, nullptr);
    if(errc) {
        MAKE_ERROR("Не удалось открыть базу данных " << (Path / "db.sqlite3").c_str() << ": " << sqlite3_errmsg(DB));
    }

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS files(
        sha256          BLOB(32)    NOT NULL,   -- 
        last_used       INT         NOT NULL,   -- unix timestamp
        size            INT         NOT NULL,   -- file size
        UNIQUE (sha256));
    )";

    errc = sqlite3_exec(DB, sql, nullptr, nullptr, nullptr);
    if(errc != SQLITE_OK) {
        MAKE_ERROR("Не удалось подготовить таблицу базы: " << sqlite3_errmsg(DB));
    }

    sql = R"(
        INSERT OR REPLACE INTO files (sha256, last_used, size)
        VALUES (?, ?, ?);
    )";

    if(sqlite3_prepare_v2(DB, sql, -1, &STMT_INSERT, nullptr) != SQLITE_OK) {
        MAKE_ERROR("Не удалось подготовить запрос STMT_INSERT: " << sqlite3_errmsg(DB));
    }

    sql = R"(
        UPDATE files SET last_used = ? WHERE sha256 = ?;
    )";

    if(sqlite3_prepare_v2(DB, sql, -1, &STMT_UPDATE_TIME, nullptr) != SQLITE_OK) {
        MAKE_ERROR("Не удалось подготовить запрос STMT_UPDATE_TIME: " << sqlite3_errmsg(DB));
    }

    sql = R"(
        DELETE FROM files WHERE sha256=?;
    )";

    if(sqlite3_prepare_v2(DB, sql, -1, &STMT_REMOVE, nullptr) != SQLITE_OK) {
        MAKE_ERROR("Не удалось подготовить запрос STMT_REMOVE: " << sqlite3_errmsg(DB));
    }

    sql = R"(
        SELECT sha256 FROM files;
    )";

    if(sqlite3_prepare_v2(DB, sql, -1, &STMT_ALL_HASH, nullptr) != SQLITE_OK) {
        MAKE_ERROR("Не удалось подготовить запрос STMT_ALL_HASH: " << sqlite3_errmsg(DB));
    }

    sql = R"(
        SELECT SUM(size) FROM files;
    )";

    if(sqlite3_prepare_v2(DB, sql, -1, &STMT_SUM, nullptr) != SQLITE_OK) {
        MAKE_ERROR("Не удалось подготовить запрос STMT_SUM: " << sqlite3_errmsg(DB));
    }

    sql = R"(
        SELECT sha256, size FROM files WHERE last_used < ?;
    )";

    if(sqlite3_prepare_v2(DB, sql, -1, &STMT_OLD, nullptr) != SQLITE_OK) {
        MAKE_ERROR("Не удалось подготовить запрос STMT_OLD: " << sqlite3_errmsg(DB));
    }
    
    sql = R"(
        SELECT sha256
        FROM files
        ORDER BY last_used ASC, size ASC
        LIMIT (
            SELECT COUNT(*) FROM (
                SELECT SUM(size) OVER (ORDER BY last_used ASC, size ASC ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS running_total
                FROM files
                ORDER BY last_used ASC, size ASC
            ) sub
            WHERE running_total <= ?
        );
    )";

    if(sqlite3_prepare_v2(DB, sql, -1, &STMT_TO_FREE, nullptr) != SQLITE_OK) {
        MAKE_ERROR("Не удалось подготовить запрос STMT_TO_FREE: " << sqlite3_errmsg(DB));
    }

    sql = R"(
        SELECT COUNT(*) FROM files;
    )";

    if(sqlite3_prepare_v2(DB, sql, -1, &STMT_COUNT, nullptr) != SQLITE_OK) {
        MAKE_ERROR("Не удалось подготовить запрос STMT_COUNT: " << sqlite3_errmsg(DB));
    }
}

CacheDatabase::~CacheDatabase() {
    for(sqlite3_stmt* stmt : {STMT_INSERT, STMT_UPDATE_TIME, STMT_REMOVE, STMT_ALL_HASH, STMT_SUM, STMT_OLD, STMT_TO_FREE, STMT_COUNT})
        if(stmt)
            sqlite3_finalize(stmt);

    if(DB)
        sqlite3_close(DB);
}

size_t CacheDatabase::getCacheSize() {
    size_t Size;
    if(sqlite3_step(STMT_SUM) != SQLITE_ROW) {
        sqlite3_reset(STMT_SUM);
        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_SUM: " << sqlite3_errmsg(DB));
    }

    Size = sqlite3_column_int64(STMT_SUM, 0);
    sqlite3_reset(STMT_SUM);
    return Size;
}

std::pair<std::string, size_t> CacheDatabase::getAllHash() {
    if(sqlite3_step(STMT_COUNT) != SQLITE_ROW) {
        sqlite3_reset(STMT_COUNT);
        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_COUNT: " << sqlite3_errmsg(DB));
    }
    
    size_t count = sqlite3_column_int(STMT_COUNT, 0);
    sqlite3_reset(STMT_COUNT);

    std::string out;
    out.reserve(32*count);

    int errc;
    size_t readed = 0;
    while(true) {
        errc = sqlite3_step(STMT_ALL_HASH);
        if(errc == SQLITE_DONE)
            break;
        else if(errc != SQLITE_ROW) {
            sqlite3_reset(STMT_ALL_HASH);
            MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_ALL_HASH: " << sqlite3_errmsg(DB));
        }

        const char *hash = (const char*) sqlite3_column_blob(STMT_ALL_HASH, 0);
        readed++;
        out += std::string_view(hash, hash+32);
    }

    sqlite3_reset(STMT_ALL_HASH);
    return {out, readed};
}

void CacheDatabase::updateTimeFor(Hash_t hash) {
    sqlite3_bind_blob(STMT_UPDATE_TIME, 1, (const void*) hash.data(), 32, SQLITE_STATIC);
    sqlite3_bind_int(STMT_UPDATE_TIME, 2, time(nullptr));
    if(sqlite3_step(STMT_UPDATE_TIME) != SQLITE_DONE) {
        sqlite3_reset(STMT_UPDATE_TIME);
        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_UPDATE_TIME: " << sqlite3_errmsg(DB));
    }

    sqlite3_reset(STMT_UPDATE_TIME);
}

void CacheDatabase::insert(Hash_t hash, size_t size) {
    assert(size < (size_t(1) << 31)-1 && size > 0);

    sqlite3_bind_blob(STMT_INSERT, 1, (const void*) hash.data(), 32, SQLITE_STATIC);
    sqlite3_bind_int(STMT_INSERT, 2, time(nullptr));
    sqlite3_bind_int(STMT_INSERT, 3, (int) size);
    if(sqlite3_step(STMT_INSERT) != SQLITE_DONE) {
        sqlite3_reset(STMT_INSERT);
        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_INSERT: " << sqlite3_errmsg(DB));
    }

    sqlite3_reset(STMT_INSERT);
}

std::vector<Hash_t> CacheDatabase::findExcessHashes(size_t bytesToFree, int timeBefore = time(nullptr)-604800) {
    std::vector<Hash_t> out;
    size_t removed = 0;

    sqlite3_bind_int(STMT_OLD, 1, timeBefore);
    while(true) {
        int errc = sqlite3_step(STMT_OLD);
        if(errc == SQLITE_DONE)
            break;
        else if(errc != SQLITE_ROW) {
            sqlite3_reset(STMT_OLD);
            MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_OLD: " << sqlite3_errmsg(DB));
        }

        const uint8_t *hash = (const uint8_t*) sqlite3_column_blob(STMT_OLD, 0);
        removed += sqlite3_column_int(STMT_OLD, 1);
        Hash_t obj;
        for(int iter = 0; iter < 32; iter++)
            obj[iter] = hash[iter];

        out.push_back(obj);
    }

    sqlite3_reset(STMT_OLD);

    if(removed > bytesToFree)
        return out;

    sqlite3_bind_int(STMT_TO_FREE, 1, (int) bytesToFree);
    
    while(true) {
        int errc = sqlite3_step(STMT_TO_FREE);
        if(errc == SQLITE_DONE)
            break;
        else if(errc != SQLITE_ROW) {
            sqlite3_reset(STMT_TO_FREE);
            MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_TO_FREE: " << sqlite3_errmsg(DB));
        }

        const uint8_t *hash = (const uint8_t*) sqlite3_column_blob(STMT_TO_FREE, 0);
        Hash_t obj;
        for(int iter = 0; iter < 32; iter++)
            obj[iter] = hash[iter];

        out.push_back(obj);
    }

    sqlite3_reset(STMT_TO_FREE);
    return out;
}

void CacheDatabase::remove(Hash_t hash) {
    sqlite3_bind_blob(STMT_REMOVE, 1, (const void*) hash.data(), 32, SQLITE_STATIC);
    if(sqlite3_step(STMT_REMOVE) != SQLITE_DONE) {
        sqlite3_reset(STMT_REMOVE);
        MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_REMOVE: " << sqlite3_errmsg(DB));
    }

    sqlite3_reset(STMT_REMOVE);
}

std::string CacheDatabase::hashToString(Hash_t hash) {
    std::string text;
    text.reserve(64);

    for(int iter = 0; iter < 32; iter++) {
        int val = (hash[31-iter] >> 4) & 0xf;
        if(val > 9)
            text += 'a'+val-10;
        else
            text += '0'+val;

        val = hash[31-iter] & 0xf;
        if(val > 9)
            text += 'a'+val-10;
        else
            text += '0'+val;
    }

    return text;
}

// int CacheDatabase::hexCharToInt(char c) {
//     if (c >= '0' && c <= '9') return c - '0';
//     if (c >= 'a' && c <= 'f') return c - 'a' + 10;
//     throw std::invalid_argument("Invalid hexadecimal character");
// }

// Hash_t CacheDatabase::stringToHash(const std::string_view view) {
//     if (view.size() != 64)
//         throw std::invalid_argument("Hex string must be exactly 64 characters long");

//     Hash_t hash;

//     for (size_t i = 0; i < 32; ++i) {
//         size_t offset = 62 - i * 2;
//         int high = hexCharToInt(view[offset]);
//         int low = hexCharToInt(view[offset + 1]);
//         hash[i] = (high << 4) | low;
//     }

//     return hash;
// }

coro<> ResourceHandler::asyncDestructor() {
    assert(NeedShutdown); // Нормальный shutdown должен быть вызван
    co_await IAsyncDestructible::asyncDestructor();
}

void ResourceHandler::readWriteThread(AsyncUseControl::Lock lock) {
    LOG.info() << "Поток чтения/записи запущен";
    
    while(!NeedShutdown || !WriteQueue.get_read().empty()) {
        if(!ReadQueue.get_read().empty()) {
            auto lock = ReadQueue.lock();
            if(!lock->empty()) {
                Hash_t hash = lock->front();
                lock->pop();
                lock.unlock();

                std::string name = CacheDatabase::hashToString(hash);
                fs::path path = Path / name.substr(0, 2) / name.substr(2, 2) / name.substr(4);

                std::shared_ptr<std::string> data;

                {
                    auto lock_wc = WriteCache.lock();
                    auto iter = lock_wc->begin();
                    while(iter != lock_wc->end()) {
                        if(iter->first == hash) {
                            // Копируем
                            data = std::make_shared<std::string>(*iter->second);
                            break;
                        }
                    }
                }
                
                if(!data) {
                    data = std::make_shared<std::string>();

                    try {
                        std::ifstream fd(path, std::ios::binary | std::ios::ate);
                        if (!fd.is_open())
                            MAKE_ERROR("!is_open(): " << fd.exceptions());
                        
                        if (fd.fail())
                            MAKE_ERROR("fail(): " << fd.exceptions());
                        
                        std::ifstream::pos_type size = fd.tellg();
                        fd.seekg(0, std::ios::beg);
                        data->resize(size);
                        fd.read(data->data(), size);

                        if (!fd.good()) 
                            MAKE_ERROR("!good(): " << fd.exceptions());

                        DB.updateTimeFor(hash);
                    } catch(const std::exception &exc) {
                        LOG.error() << "Не удалось считать ресурс " << path.c_str() << ": " << exc.what();
                    }   
                }

                ReadedQueue.lock()->emplace_back(hash, std::move(data));
                continue;
            }
        }

        if(!WriteQueue.get_read().empty()) {
            auto lock = WriteQueue.lock();
            if(!lock->empty()) {
                DataTask task = lock->front();
                lock->pop();
                lock.unlock();

                std::string name = CacheDatabase::hashToString(task.Hash);
                fs::path path = Path / name.substr(0, 2) / name.substr(2, 2) / name.substr(4);


                try {
                    // Проверка на наличие свободного места (виртуально)
                    if(ssize_t free = ssize_t(MaxCacheDirectorySize)-DB.getCacheSize(); free < task.Data->size()) {
                        // Недостаточно места, сколько необходимо освободить с запасом
                        ssize_t need = task.Data->size()-free + 64*1024*1024;
                        std::vector<Hash_t> hashes = DB.findExcessHashes(need, time(nullptr)-MaxLifeTime);
                        
                        LOG.warn() << "Удаление устаревшего кеша в количестве " << hashes.size() << "...";

                        for(Hash_t hash : hashes) {
                            std::string name = CacheDatabase::hashToString(hash);
                            fs::path path = Path / name.substr(0, 2) / name.substr(2, 2) / name.substr(4);
                            DB.remove(hash);
                            fs::remove(path);
                            
                            fs::path up1 = path.parent_path();
                            LOG.info() << "В директории " << up1.c_str() << " не осталось файлов, удаляем...";
                            size_t count = std::distance(fs::directory_iterator(up1), fs::directory_iterator());
                            if(count == 0)
                                fs::remove(up1);
                        }
                    }

                    fs::create_directories(path.parent_path());

                    std::ofstream fd(path, std::ios::binary | std::ios::ate);
                    fd.write(task.Data->data(), task.Data->size());

                    DB.insert(task.Hash, task.Data->size());
                } catch(const std::exception &exc) {
                    LOG.error() << "Не удалось сохранить ресурс " << path.c_str() << ": " << exc.what();
                }

                auto lock = WriteCache.lock();
                auto iter = lock->begin();
                while(iter != lock->end()) {
                    if(iter->first == task.Hash)
                        break;
                    iter++;
                }

                assert(iter != lock->end());
                lock->erase(iter);
            }
        }

        TOS::Time::sleep3(20);
    }

    LOG.info() << "Поток чтения/записи остановлен";
    lock.unlock();
}

ResourceHandler::ResourceHandler(boost::asio::io_context &ioc, const fs::path &cachePath,
        size_t maxCacheDirectorySize, size_t maxLifeTime)
    :   IAsyncDestructible(ioc),
        OffThread(&ResourceHandler::readWriteThread, this, AUC.use())
{
    LOG.info() << "Инициализировано хранилище кеша: " << cachePath.c_str();
}


// void ResourceHandler::updateParams(size_t maxLifeTime, size_t maxCacheDirectorySize) {
//     MaxLifeTime = maxLifeTime;
    
//     if(MaxCacheDirectorySize != maxCacheDirectorySize) {
//         MaxCacheDirectorySize = maxCacheDirectorySize;

//         size_t size = DB.getCacheSize();
//         if(size > maxCacheDirectorySize) {
//             size_t needToFree = size-maxCacheDirectorySize+64*1024*1024;
//             try {
//                 LOG.info() << "Начата вычистка кеша на сумму  " << needToFree/1024/1024 << " Мб";
//                 std::vector<Hash_t> hashes = DB.findExcessHashes(needToFree, time(nullptr)-MaxLifeTime);
//                 LOG.warn() << "Удаление кеша в количестве " << hashes.size() << "...";

//                 for(Hash_t hash : hashes) {
//                     std::string name = CacheDatabase::hashToString(hash);
//                     fs::path path = Path / name.substr(0, 2) / name.substr(2, 2) / name.substr(4);
//                     DB.remove(hash);
//                     fs::remove(path);
                    
//                     fs::path up1 = path.parent_path();
//                     LOG.info() << "В директории " << up1.c_str() << " не осталось файлов, удаляем...";
//                     size_t count = std::distance(fs::directory_iterator(up1), fs::directory_iterator());
//                     if(count == 0)
//                         fs::remove(up1);
//                 }
//             } catch(const std::exception &exc) {
//                 LOG.error() << "Не удалось очистить кеш до новой границы: " << exc.what();
//             }
//         }
//     }
// }

}