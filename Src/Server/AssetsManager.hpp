#pragma once

#include "Common/Abstract.hpp"
#include "TOSLib.hpp"
#include "Common/Net.hpp"
#include "sha2.hpp"
#include <bitset>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <variant>


namespace LV::Server {

namespace fs = std::filesystem;

/*
    Используется для расчёта коллизии,
    если это необходимо, а также зависимостей к ассетам.
*/
struct PreparedModel {
    // Упрощённая коллизия
    std::vector<std::pair<glm::vec3, glm::vec3>> Cuboids;
    // Зависимости от текстур, которые нужно сообщить клиенту
    std::unordered_map<std::string, std::vector<std::string>> TextureDependencies;
    // Зависимости от моделей
    std::unordered_map<std::string, std::vector<std::string>> ModelDependencies;

    PreparedModel(const std::string& domain, const LV::PreparedModel& model);
    PreparedModel(const std::string& domain, const js::object& glTF);
    PreparedModel(const std::string& domain, Resource glb);

    PreparedModel() = default;
    PreparedModel(const PreparedModel&) = default;
    PreparedModel(PreparedModel&&) = default;

    PreparedModel& operator=(const PreparedModel&) = default;
    PreparedModel& operator=(PreparedModel&&) = default;
};

struct ModelDependency {
    // Прямые зависимости к тестурам и моделям
    std::vector<AssetsTexture> TextureDeps;
    std::vector<AssetsModel> ModelDeps;
    // Коллизия
    std::vector<std::pair<glm::vec3, glm::vec3>> Cuboids;

    // 
    bool Ready = false;
    // Полный список зависимостей рекурсивно
    std::vector<AssetsTexture> FullSubTextureDeps;
    std::vector<AssetsModel> FullSubModelDeps;
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
        std::unordered_map<std::string, std::vector<std::pair<std::string, PreparedModel>>> Models;
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
        std::array<std::optional<T>, ChunkSize> Entries;

        TableEntry() {
            Empty.set();
        }
    };

    struct Local {
        // Связь ресурсов по идентификаторам
        std::vector<std::unique_ptr<TableEntry<DataEntry>>> Table[(int) EnumAssets::MAX_ENUM];

        // Распаршенные ресурсы, для использования сервером (сбор зависимостей профиля нод и расчёт коллизии если нужно)
        // Первичные зависимости Nodestate к моделям
        std::vector<std::unique_ptr<TableEntry<std::vector<AssetsModel>>>> Table_NodeState;
        // Упрощённые модели для коллизии
        std::vector<std::unique_ptr<TableEntry<ModelDependency>>> Table_Model;

        // Связь домены -> {ключ -> идентификатор}
        std::unordered_map<std::string, std::unordered_map<std::string, ResourceId>> KeyToId[(int) EnumAssets::MAX_ENUM];
        std::unordered_map<Hash_t, std::tuple<EnumAssets, ResourceId>> HashToId;
        
        std::tuple<ResourceId, std::optional<DataEntry>&> nextId(EnumAssets type);


        ResourceId getId(EnumAssets type, const std::string& domain, const std::string& key) {
            auto& keyToId = KeyToId[(int) type];
            if(auto iterKTI = keyToId.find(domain); iterKTI != keyToId.end()) {
                if(auto iterKey = iterKTI->second.find(key); iterKey != iterKTI->second.end()) {
                    return iterKey->second;
                }
            }

            auto [id, entry] = nextId(type);
            keyToId[domain][key] = id;

            return id;
        }

        std::optional<std::tuple<Resource, const std::string&, const std::string&>> getResource(EnumAssets type, ResourceId id) {
            assert(id < Table[(int) type].size()*TableEntry<DataEntry>::ChunkSize);
            auto& value = Table[(int) type][id / TableEntry<DataEntry>::ChunkSize]->Entries[id % TableEntry<DataEntry>::ChunkSize];
            if(value)
                return {{value->Res, value->Domain, value->Key}};
            else
                return std::nullopt;
        }

        std::optional<std::tuple<Resource, const std::string&, const std::string&, EnumAssets, ResourceId>> getResource(const Hash_t& hash) {
            auto iter = HashToId.find(hash);
            if(iter == HashToId.end())
                return std::nullopt;

            auto [type, id] = iter->second;
            std::optional<std::tuple<Resource, const std::string&, const std::string&>> res = getResource(type, id);
            if(!res) {
                HashToId.erase(iter);
                return std::nullopt;
            }

            if(std::get<Resource>(*res).hash() == hash) {
                auto& [resource, domain, key] = *res;
                return std::tuple<Resource, const std::string&, const std::string&, EnumAssets, ResourceId>{resource, domain, key, type, id};
            }


            HashToId.erase(iter);
            return std::nullopt;
        }

        const std::optional<std::vector<AssetsModel>>& getResourceNodestate(ResourceId id) {
            assert(id < Table_NodeState.size()*TableEntry<DataEntry>::ChunkSize);
            return Table_NodeState[id / TableEntry<DataEntry>::ChunkSize]
                ->Entries[id % TableEntry<DataEntry>::ChunkSize];
        }


        const std::optional<ModelDependency>& getResourceModel(ResourceId id) {
            assert(id < Table_Model.size()*TableEntry<DataEntry>::ChunkSize);
            return Table_Model[id / TableEntry<DataEntry>::ChunkSize]
                ->Entries[id % TableEntry<DataEntry>::ChunkSize];
        }
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
        return LocalObj.lock()->getId(type, domain, key);
    }

    // Выдаёт ресурс по идентификатору
    std::optional<std::tuple<Resource, const std::string&, const std::string&>> getResource(EnumAssets type, ResourceId id) {
       return LocalObj.lock()->getResource(type, id);
    }

    // Выдаёт ресурс по хешу
    std::optional<std::tuple<Resource, const std::string&, const std::string&, EnumAssets, ResourceId>> getResource(const Hash_t& hash) {
        return LocalObj.lock()->getResource(hash);
    }

    // Выдаёт зависимости к ресурсам профиля ноды
    std::tuple<AssetsNodestate, std::vector<AssetsModel>, std::vector<AssetsTexture>>
        getNodeDependency(const std::string& domain, const std::string& key)
    {
        auto lock = LocalObj.lock();
        AssetsNodestate nodestateId = lock->getId(EnumAssets::Nodestate, domain, key+".json");

        std::vector<AssetsModel> models;
        std::vector<AssetsTexture> textures;

        if(auto subModelsPtr = lock->getResourceNodestate(nodestateId)) {
            for(AssetsModel resId : *subModelsPtr) {
                const auto& subModel = lock->getResourceModel(resId);

                if(!subModel)
                    continue;

                models.push_back(resId);
                models.append_range(subModel->FullSubModelDeps);
                textures.append_range(subModel->FullSubTextureDeps);
            }
        } else {
            LOG.debug() << "Для ноды " << domain << ':' << key << " отсутствует описание Nodestate";
        }

        {
            std::sort(models.begin(), models.end());
            auto eraseIter = std::unique(models.begin(), models.end());
            models.erase(eraseIter, models.end());
            models.shrink_to_fit();
        }

        {
            std::sort(textures.begin(), textures.end());
            auto eraseIter = std::unique(textures.begin(), textures.end());
            textures.erase(eraseIter, textures.end());
            textures.shrink_to_fit();
        }
        
        return {nodestateId, std::move(models), std::move(textures)};
    }

private:
    TOS::Logger LOG = "Server>AssetsManager";

};

}