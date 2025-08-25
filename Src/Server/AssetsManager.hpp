#pragma once

#include "Common/Abstract.hpp"
#include "TOSLib.hpp"
#include "Common/Net.hpp"
#include "sha2.hpp"
#include <bitset>
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
        uint8_t Faces;
    
        std::vector<PreparedModel::Cuboid::Transformation> Transformations;
    };

    std::vector<Cuboid> Cuboids;
    std::vector<PreparedModel::SubModel> SubModels;

    PreparedModelCollision(const PreparedModel& model);
    PreparedModelCollision(const std::string& domain, const js::object& glTF);
    PreparedModelCollision(const std::string& domain, Resource glb);

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