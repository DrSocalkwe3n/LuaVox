#pragma once

#include "Abstract.hpp"
#include "Common/Abstract.hpp"
#include "TOSLib.hpp"
#include "boost/asio/io_context.hpp"
#include "sha2.hpp"
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <filesystem>
#include <optional>
#include <unordered_map>


namespace LV::Server {

namespace fs = std::filesystem;

struct DefModel {

};

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

    struct ResourceChangeObj {
        // Потерянные ресурсы
        std::unordered_map<std::string, std::vector<std::string>> Lost[(int) EnumAssets::MAX_ENUM];
        // Домен и ключ ресурса
        std::unordered_map<std::string, std::vector<std::tuple<std::string, Resource, fs::file_time_type>>> NewOrChange[(int) EnumAssets::MAX_ENUM];
    };

private:
    // Данные об отслеживаемых файлах
    struct DataEntry {
        // Время последнего изменения файла
        fs::file_time_type FileChangeTime;
        Resource Res;
        std::string Domain, Key;
    };

    template<typename T>
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
        std::vector<std::unique_ptr<TableEntry<DataEntry>>> Table[(int) EnumAssets::MAX_ENUM];

        // Распаршенные ресурсы, для использования сервером
        std::vector<std::unique_ptr<TableEntry<DefNodeState>>> Table_NodeState;
        std::vector<std::unique_ptr<TableEntry<DefModel>>> Table_Model;

        // Связь домены -> {ключ -> идентификатор}
        std::unordered_map<std::string, std::unordered_map<std::string, ResourceId>> KeyToId[(int) EnumAssets::MAX_ENUM];
        
        std::tuple<ResourceId, std::optional<DataEntry>&> nextId(EnumAssets type);
    };

    TOS::SpinlockObject<Local> LocalObj;

    /*
        Загрузка ресурса с файла. При необходимости приводится
        к внутреннему формату и сохраняется в кеше
    */
    void loadResourceFromFile   (EnumAssets type, ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const;
    void loadResourceFromLua    (EnumAssets type, ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const;

    void loadResourceFromFile_Nodestate (ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const;
    void loadResourceFromFile_Particle  (ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const;
    void loadResourceFromFile_Animation (ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const;
    void loadResourceFromFile_Model     (ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const;
    void loadResourceFromFile_Texture   (ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const;
    void loadResourceFromFile_Sound     (ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const;
    void loadResourceFromFile_Font      (ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const;

    void loadResourceFromLua_Nodestate  (ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const;
    void loadResourceFromLua_Particle   (ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const;
    void loadResourceFromLua_Animation  (ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const;
    void loadResourceFromLua_Model      (ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const;
    void loadResourceFromLua_Texture    (ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const;
    void loadResourceFromLua_Sound      (ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const;
    void loadResourceFromLua_Font       (ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const;

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

    ResourceChangeObj recheckResources(const AssetsRegister&);

    /*
        Применяет расчитанные изменения.
        Раздаёт идентификаторы ресурсам и записывает их в таблицу
    */
    struct Out_applyResourceChange {
        std::vector<ResourceId> Lost[(int) EnumAssets::MAX_ENUM];
        std::vector<std::pair<ResourceId, Resource>> NewOrChange[(int) EnumAssets::MAX_ENUM];
    };

    Out_applyResourceChange applyResourceChange(const ResourceChangeObj& orr);

    /*
        Выдаёт идентификатор ресурса, даже если он не существует или был удалён.
        resource должен содержать домен и путь
    */
    ResourceId getId(EnumAssets type, const std::string& domain, const std::string& key) {
        auto lock = LocalObj.lock();
        auto& keyToId = lock->KeyToId[(int) type];
        if(auto iterKTI = keyToId.find(domain); iterKTI != keyToId.end()) {
            if(auto iterKey = iterKTI->second.find(key); iterKey != iterKTI->second.end()) {
                return iterKey->second;
            }
        }

        auto [id, entry] = lock->nextId(type);
        keyToId[domain][key] = id;
        return id;
    }

    std::optional<std::tuple<Resource, const std::string&, const std::string&>> getResource(EnumAssets type, ResourceId id) {
        auto lock = LocalObj.lock();
        assert(id < lock->Table[(int) type].size()*TableEntry<DataEntry>::ChunkSize);
        auto& value = lock->Table[(int) type][id / TableEntry<DataEntry>::ChunkSize]->Entries[id % TableEntry<DataEntry>::ChunkSize];
        if(value)
            return {{value->Res, value->Domain, value->Key}};
        else
            return std::nullopt;
    }
};

}