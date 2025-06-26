#include <array>
#include <cassert>
#include <string>
#include <sqlite3.h>
#include <TOSLib.hpp>
#include <filesystem>
#include <string_view>


namespace LV::Client {

namespace fs = std::filesystem;

// NOT ThreadSafe
class ResourceCacheHandler {
    const fs::path Path;

    sqlite3 *DB = nullptr;
    sqlite3_stmt *STMT_INSERT = nullptr,
        *STMT_UPDATE_TIME = nullptr,
        *STMT_REMOVE = nullptr,
        *STMT_ALL_HASH = nullptr,
        *STMT_SUM = nullptr,
        *STMT_TO_FREE = nullptr,
        *STMT_COUNT = nullptr;

    size_t Size = -1;

public:
    ResourceCacheHandler(const std::string_view cache_path) 
        : Path(cache_path)
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
            SELECT sha256
            FROM files
            WHERE last_used < ?
            ORDER BY last_used ASC, size ASC
            LIMIT (
                SELECT COUNT(*) FROM (
                    SELECT SUM(size) OVER (ORDER BY last_used ASC, size ASC ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS running_total
                    FROM files
                    WHERE last_used < ?
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

    ~ResourceCacheHandler() {
        for(sqlite3_stmt* stmt : {STMT_INSERT, STMT_UPDATE_TIME, STMT_REMOVE, STMT_ALL_HASH, STMT_SUM, STMT_TO_FREE, STMT_COUNT})
            if(stmt)
                sqlite3_finalize(stmt);

        if(DB)
            sqlite3_close(DB);
    }

    ResourceCacheHandler(const ResourceCacheHandler&) = delete;
    ResourceCacheHandler(ResourceCacheHandler&&) = delete;
    ResourceCacheHandler& operator=(const ResourceCacheHandler&) = delete;
    ResourceCacheHandler& operator=(ResourceCacheHandler&&) = delete; 

    /*
        Выдаёт размер занимаемый всем хранимым кешем
    */
    size_t getCacheSize() {
        if(Size == -1) {
            if(sqlite3_step(STMT_SUM) != SQLITE_ROW) {
                sqlite3_reset(STMT_SUM);
                MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_SUM: " << sqlite3_errmsg(DB));
            }

            Size = sqlite3_column_int(STMT_SUM, 0);
            sqlite3_reset(STMT_SUM);
        }

        return Size;
    }

    // TODO: добавить ограничения на количество файлов

    /*
        Создаёт линейный массив в котором подряд указаны все хэш суммы в бинарном виде и возвращает их количество
    */
    std::pair<std::string, size_t> getAllHash() {
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

    using HASH = std::array<uint8_t, 32>;

    /*
        Обновляет время использования кеша
    */
    void updateTimeFor(HASH hash) {
        sqlite3_bind_blob(STMT_UPDATE_TIME, 0, (const void*) hash.data(), 32, SQLITE_STATIC);
        sqlite3_bind_int(STMT_UPDATE_TIME, 1, time(nullptr));
        if(sqlite3_step(STMT_UPDATE_TIME) != SQLITE_OK) {
            sqlite3_reset(STMT_UPDATE_TIME);
            MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_UPDATE_TIME: " << sqlite3_errmsg(DB));
        }

        sqlite3_reset(STMT_UPDATE_TIME);
    }

    /*
        Добавляет запись
    */
    void insert(HASH hash, size_t size) {
        assert(size < (size_t(1) << 31)-1 && size > 0);

        sqlite3_bind_blob(STMT_INSERT, 0, (const void*) hash.data(), 32, SQLITE_STATIC);
        sqlite3_bind_int(STMT_INSERT, 1, (int) size);
        sqlite3_bind_int(STMT_INSERT, 2, time(nullptr));
        if(sqlite3_step(STMT_INSERT) != SQLITE_OK) {
            sqlite3_reset(STMT_INSERT);
            MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_INSERT: " << sqlite3_errmsg(DB));
        }

        sqlite3_reset(STMT_INSERT);
    }

    /*
        Выдаёт хэши на удаление по размеру в сумме больше bytesToFree. В приоритете старые, потом мелкие
    */
    std::vector<HASH> findExcessHashes(size_t bytesToFree, int timeBefore = time(nullptr)-604800) {
        sqlite3_bind_int(STMT_TO_FREE, 0, timeBefore);
        sqlite3_bind_int(STMT_TO_FREE, 1, timeBefore);
        sqlite3_bind_int(STMT_TO_FREE, 2, (int) bytesToFree);
        
        std::vector<HASH> out;
        while(true) {
            int errc = sqlite3_step(STMT_TO_FREE);
            if(errc == SQLITE_DONE)
                break;
            else if(errc != SQLITE_ROW) {
                sqlite3_reset(STMT_TO_FREE);
                MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_TO_FREE: " << sqlite3_errmsg(DB));
            }

            const uint8_t *hash = (const uint8_t*) sqlite3_column_blob(STMT_TO_FREE, 0);
            HASH obj;
            for(int iter = 0; iter < 32; iter++)
                obj[iter] = hash[iter];

            out.push_back(obj);
        }

        sqlite3_reset(STMT_TO_FREE);
        return out;
    }

    /*
        Удаление записи
    */
    void remove(HASH hash) {
        sqlite3_bind_blob(STMT_REMOVE, 0, (const void*) hash.data(), 32, SQLITE_STATIC);
        if(sqlite3_step(STMT_REMOVE) != SQLITE_OK) {
            sqlite3_reset(STMT_REMOVE);
            MAKE_ERROR("Не удалось выполнить подготовленный запрос STMT_REMOVE: " << sqlite3_errmsg(DB));
        }

        sqlite3_reset(STMT_REMOVE);
    }

    static std::string hashToString(HASH hash) {
        std::string text;
        text.reserve(64);

        for(int iter = 0; iter < 32; iter++) {
            int val = hash[31-iter] & 0xf;
            if(val > 9)
                text += 'a'+val-10;
            else
                text += '0'+val;

            val = (hash[31-iter] >> 4) & 0xf;
            if(val > 9)
                text += 'a'+val-10;
            else
                text += '0'+val;
        }

        return text;
    }

    static int hexCharToInt(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        throw std::invalid_argument("Invalid hexadecimal character");
    }

    static HASH stringToHash(const std::string_view view) {
        if (view.size() != 64)
            throw std::invalid_argument("Hex string must be exactly 64 characters long");

        HASH hash;
    
        for (size_t i = 0; i < 32; ++i) {
            size_t offset = 62 - i * 2;
            int high = hexCharToInt(view[offset]);
            int low = hexCharToInt(view[offset + 1]);
            hash[i] = (high << 4) | low;
        }

        return hash;
    }
};



}