#pragma once

#include "Abstract.hpp"
#include "Common/Abstract.hpp"
#include "TOSLib.hpp"
#include "boost/asio/io_context.hpp"
#include "sha2.hpp"
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <filesystem>
#include <unordered_map>


namespace LV::Server {

namespace fs = std::filesystem;

/*
    Работает с ресурсами из папок assets.
    Использует папку server_cache/assets для хранения
    преобразованных ресурсов
*/
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
            Empty.set();
        }
    };

    struct Local {
        // Связь ресурсов по идентификаторам
        std::vector<std::unique_ptr<TableEntry>> Table[(int) EnumAssets::MAX_ENUM];
        // Связь домены -> {ключ -> идентификатор}
        std::unordered_map<std::string, std::unordered_map<std::string, ResourceId_t>> KeyToId[(int) EnumAssets::MAX_ENUM];
        
        std::tuple<ResourceId_t, std::optional<DataEntry>&> nextId(EnumAssets type);
    };

    TOS::SpinlockObject<Local> LocalObj;

    /*
        Загрузка ресурса с файла. При необходимости приводится
        к внутреннему формату и сохраняется в кеше
    */
    Resource loadResourceFromFile(EnumAssets type, fs::path path) const;
    Resource loadResourceFromLua(EnumAssets type, void*) const;

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
        std::unordered_map<std::string, std::vector<std::tuple<std::string, Resource, fs::file_time_type>>> NewOrChange[(int) EnumAssets::MAX_ENUM];
    };

    Out_recheckResources recheckResources(const AssetsRegister&);

    /*
        Применяет расчитанные изменения.
        Раздаёт идентификаторы ресурсам и записывает их в таблицу
    */
    struct Out_applyResourceChange {
        std::vector<ResourceId_t> Lost[(int) EnumAssets::MAX_ENUM];
        std::vector<std::pair<ResourceId_t, Resource>> NewOrChange[(int) EnumAssets::MAX_ENUM];
    };

    Out_applyResourceChange applyResourceChange(const Out_recheckResources& orr);

    /*
        Выдаёт идентификатор ресурса, даже если он не существует или был удалён.
        resource должен содержать домен и путь
    */
    ResourceId_t getId(EnumAssets type, const std::string& domain, const std::string& key);
};

}