#pragma once

#include "Abstract.hpp"
#include "Common/Abstract.hpp"
#include "boost/asio/io_context.hpp"
#include "sha2.hpp"
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <filesystem>
#include <unordered_map>


namespace LV::Server {

namespace fs = std::filesystem;

class AssetsManager {
public:
    struct Resource {
    private:
        struct Inline {
            boost::interprocess::file_mapping MMap;
            boost::interprocess::mapped_region Region;
            Hash_t Hash;

            Inline(fs::path path)
                : MMap(path.c_str(), boost::interprocess::read_only),
                  Region(MMap, boost::interprocess::read_only)
            {}
        };

        std::shared_ptr<Inline> In;

    public:
        Resource(fs::path path)
            : In(std::make_shared<Inline>(path))
        {
            In->Hash = sha2::sha256((const uint8_t*) In->Region.get_address(), In->Region.get_size());
        }

        Resource(const Resource&) = default;
        Resource(Resource&&) = default;
        Resource& operator=(const Resource&) = default;
        Resource& operator=(Resource&&) = default;
        bool operator<=>(const Resource&) const = default;

        const std::byte* data() const { return (const std::byte*) In->Region.get_address(); }
        size_t size() const { return In->Region.get_size(); }
        Hash_t hash() const { return In->Hash; }
    };

private:
    // Данные об отслеживаемых файлах
    struct DataEntry {
        // Время последнего изменения файла
        fs::file_time_type FileChangeTime;
        Resource Res;
    };

    struct TableEntry {
        static constexpr size_t ChunkSize = 4096;
        bool IsFull = false;
        std::bitset<ChunkSize> Empty;
        std::array<std::optional<DataEntry>, ChunkSize> Entries;

        TableEntry() {
            Empty.reset();
        }
    };

    // Данные не меняются в процессе работы сервера.
    // Изменения возможны при синхронизации всего сервера
    // и перехода в режим перезагрузки модов

    // Связь ресурсов по идентификаторам
    std::vector<std::unique_ptr<TableEntry>> Table[(int) EnumAssets::MAX_ENUM];
    // Связь домены -> {ключ -> идентификатор}
    std::unordered_map<std::string, std::unordered_map<std::string, ResourceId_t>> KeyToId[(int) EnumAssets::MAX_ENUM];

    DataEntry& getEntry(EnumAssets type, ResourceId_t id);


public:
    AssetsManager(asio::io_context& ioc);
    ~AssetsManager();

    /*
        Перепроверка изменений ресурсов по дате изменения, пересчёт хешей.
        Обнаруженные изменения должны быть отправлены всем клиентам.
        Ресурсы будут обработаны в подходящий формат и сохранены в кеше.
        Одновременно может выполнятся только одна такая функция
        Используется в GameServer
    */

    struct AssetsRegister {
        /*
            Пути до активных папок assets, соответствую порядку загруженным модам.
            От последнего мода к первому.
            Тот файл, что был загружен раньше и будет использоваться
        */
        std::vector<fs::path> Assets;
        /*
            У этих ресурсов приоритет выше, если их удастся получить,
            то использоваться будут именно они
            Domain -> {key + data}
        */
        std::unordered_map<std::string, std::unordered_map<std::string, void*>> Custom[(int) EnumAssets::MAX_ENUM];
    };

    struct Out_recheckResources {
        // Потерянные ресурсы
        std::unordered_map<std::string, std::vector<std::string>> Lost[(int) EnumAssets::MAX_ENUM];
        // Домен и ключ ресурса
        std::unordered_map<std::string, std::vector<std::tuple<std::string, Resource>>> NewOrChange[(int) EnumAssets::MAX_ENUM];
    };

    coro<Out_recheckResources> recheckResources(AssetsRegister) const;
};

}