#pragma once

#include "Abstract.hpp"
#include "Common/Abstract.hpp"
#include "TOSLib.hpp"
#include "assets.hpp"
#include "boost/asio/io_context.hpp"
#include "sha2.hpp"
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <variant>


namespace LV::Server {

namespace fs = std::filesystem;


/*
    Используется для расчёта коллизии,
    если это необходимо.

    glTF конвертируется в кубы
*/
struct PreparedModelCollision {
    struct Cuboid {
        glm::vec3 From, To;

        enum EnumFace {
            Down, Up, North, South, West, East
        };

        struct Face {
            std::optional<EnumFace> Cullface;
            uint8_t Rotation = 0;
        };

        std::unordered_map<EnumFace, Face> Faces;

        struct Transformation {
            enum EnumTransform {
                MoveX, MoveY, MoveZ,
                RotateX, RotateY, RotateZ,
                MAX_ENUM
            } Op;

            float Value;
        };
    
        std::vector<Transformation> Transformations;
    };

    std::vector<Cuboid> Cuboids;

    PreparedModelCollision(const PreparedModel& model);
    PreparedModelCollision(const std::string& domain, const js::object& glTF);
    PreparedModelCollision(const std::string& domain, Resource res);

    PreparedModelCollision() = default;
    PreparedModelCollision(const PreparedModelCollision&) = default;
    PreparedModelCollision(PreparedModelCollision&&) = default;

    PreparedModelCollision& operator=(const PreparedModelCollision&) = default;
    PreparedModelCollision& operator=(PreparedModelCollision&&) = default;
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
        struct InlineMMap {
            boost::interprocess::file_mapping MMap;
            boost::interprocess::mapped_region Region;
            Hash_t Hash;

            InlineMMap(fs::path path)
                : MMap(path.c_str(), boost::interprocess::read_only),
                  Region(MMap, boost::interprocess::read_only)
            {
                Hash = sha2::sha256((const uint8_t*) Region.get_address(), Region.get_size());
            }

            const std::byte* data() const { return (const std::byte*) Region.get_address(); }
            size_t size() const { return Region.get_size(); }
        };

        struct InlinePtr {
            std::vector<uint8_t> Data;
            Hash_t Hash;

            InlinePtr(const uint8_t* data, size_t size) {
                Data.resize(size);
                std::copy(data, data+size, Data.data());
                Hash = sha2::sha256(data, size);
            }

            const std::byte* data() const { return (const std::byte*) Data.data(); }
            size_t size() const { return Data.size(); }
        };

        std::shared_ptr<std::variant<InlineMMap, InlinePtr>> In;

    public:
        Resource(fs::path path)
            : In(std::make_shared<std::variant<InlineMMap, InlinePtr>>(InlineMMap(path)))
        {}

        Resource(const uint8_t* data, size_t size)
            : In(std::make_shared<std::variant<InlineMMap, InlinePtr>>(InlinePtr(data, size)))
        {}

        Resource(const Resource&) = default;
        Resource(Resource&&) = default;
        Resource& operator=(const Resource&) = default;
        Resource& operator=(Resource&&) = default;
        bool operator<=>(const Resource&) const = default;

        const std::byte* data() const { return std::visit<const std::byte*>([](auto& obj){ return obj.data(); }, *In); }
        size_t size() const { return std::visit<size_t>([](auto& obj){ return obj.size(); }, *In); }
        Hash_t hash() const { return std::visit<Hash_t>([](auto& obj){ return obj.Hash; }, *In); }
    };

    struct ResourceChangeObj {
        // Потерянные ресурсы
        std::unordered_map<std::string, std::vector<std::string>> Lost[(int) EnumAssets::MAX_ENUM];
        // Домен и ключ ресурса
        std::unordered_map<std::string, std::vector<std::tuple<std::string, Resource, fs::file_time_type>>> NewOrChange[(int) EnumAssets::MAX_ENUM];
    
        
        std::unordered_map<std::string, std::vector<std::pair<std::string, PreparedNodeState>>> Nodestates;
        std::unordered_map<std::string, std::vector<std::pair<std::string, PreparedModelCollision>>> Models;
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
        std::vector<std::unique_ptr<TableEntry<PreparedNodeState>>> Table_NodeState;
        std::vector<std::unique_ptr<TableEntry<PreparedModelCollision>>> Table_Model;

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