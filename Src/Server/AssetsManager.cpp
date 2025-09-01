#include "AssetsManager.hpp"
#include "Common/Abstract.hpp"
#include "boost/json.hpp"
#include "png++/rgb_pixel.hpp"
#include <algorithm>
#include <exception>
#include <filesystem>
#include <png.h>
#include <pngconf.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "sol/sol.hpp"


namespace LV::Server {

PreparedModel::PreparedModel(const std::string& domain, const LV::PreparedModel& model) {
    Cuboids.reserve(model.Cuboids.size());

    for(auto& [key, cmd] : model.Textures) {
        PrecompiledTexturePipeline ptp = compileTexturePipeline(cmd, domain);
        for(auto& [domain, key] : ptp.Assets) {
            TextureDependencies[domain].push_back(key);
        }
    }

    for(auto& sub : model.SubModels) {
        ModelDependencies[sub.Domain].push_back(sub.Key);
    }

    // for(const PreparedModel::Cuboid& cuboid : model.Cuboids) {
    //     Cuboid result;
    //     result.From = cuboid.From;
    //     result.To = cuboid.To;
    //     result.Faces = 0;

    //     for(const auto& [key, _] : cuboid.Faces)
    //         result.Faces |= (1 << int(key));

    //     result.Transformations = cuboid.Transformations;
    // }
}

PreparedModel::PreparedModel(const std::string& domain, const js::object& glTF) {
    // gltf

    // Сцена по умолчанию
    // Сцены -> Ноды
    // Ноды -> Ноды, меши, матрицы, translation, rotation
    // Меши -> Примитивы
    // Примитивы -> Материал, вершинные данные
    // Материалы -> текстуры
    // Текстуры
    // Буферы
}

PreparedModel::PreparedModel(const std::string& domain, Resource glb) {

}

void AssetsManager::loadResourceFromFile(EnumAssets type, ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const {
    switch(type) {
    case EnumAssets::Nodestate:     loadResourceFromFile_Nodestate  (out, domain, key, path); return;
    case EnumAssets::Particle:      loadResourceFromFile_Particle   (out, domain, key, path); return;
    case EnumAssets::Animation:     loadResourceFromFile_Animation  (out, domain, key, path); return;
    case EnumAssets::Model:         loadResourceFromFile_Model      (out, domain, key, path); return;
    case EnumAssets::Texture:       loadResourceFromFile_Texture    (out, domain, key, path); return;
    case EnumAssets::Sound:         loadResourceFromFile_Sound      (out, domain, key, path); return;
    case EnumAssets::Font:          loadResourceFromFile_Font       (out, domain, key, path); return;
    default:
        std::unreachable();
    }
}

void AssetsManager::loadResourceFromLua(EnumAssets type, ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const {
    switch(type) {
    case EnumAssets::Nodestate:     loadResourceFromLua_Nodestate(out, domain, key, profile); return;
    case EnumAssets::Particle:      loadResourceFromLua_Particle(out, domain, key, profile);  return;
    case EnumAssets::Animation:     loadResourceFromLua_Animation(out, domain, key, profile); return;
    case EnumAssets::Model:         loadResourceFromLua_Model(out, domain, key, profile);     return;
    case EnumAssets::Texture:       loadResourceFromLua_Texture(out, domain, key, profile);   return;
    case EnumAssets::Sound:         loadResourceFromLua_Sound(out, domain, key, profile);     return;
    case EnumAssets::Font:          loadResourceFromLua_Font(out, domain, key, profile);      return;
    default:
        std::unreachable();
    }
}

void AssetsManager::loadResourceFromFile_Nodestate(ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const {
    Resource res(path);
    js::object obj = js::parse(std::string_view((const char*) res.data(), res.size())).as_object();
    PreparedNodeState pns(domain, obj);
    std::u8string data = pns.dump();
    Resource result((const uint8_t*) data.data(), data.size());
    out.Nodestates[domain].emplace_back(key, std::move(pns));
    out.NewOrChange[(int) EnumAssets::Nodestate][domain].emplace_back(key, result, fs::last_write_time(path));
}

void AssetsManager::loadResourceFromFile_Particle(ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const {
    std::unreachable();
}

void AssetsManager::loadResourceFromFile_Animation(ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const {
    std::unreachable();
}

void AssetsManager::loadResourceFromFile_Model(ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const {
    /*
        json, glTF, glB
    */

    // Либо это внутренний формат, либо glTF

    Resource res(path);
    std::filesystem::file_time_type ftt = fs::last_write_time(path);
    PreparedModel pmc;

    auto extension = path.extension();
    
    if(extension == ".json") {
        js::object obj = js::parse(std::string_view((const char*) res.data(), res.size())).as_object();
        LV::PreparedModel pm(domain, obj);
        std::u8string data = pm.dump();
        pmc = PreparedModel(domain, pm);
        out.NewOrChange[(int) EnumAssets::Model][domain].emplace_back(key, Resource((const uint8_t*) data.data(), data.size()), ftt);
    } else if(extension == ".gltf") {
        js::object obj = js::parse(std::string_view((const char*) res.data(), res.size())).as_object();
        pmc = PreparedModel(domain, obj);
        out.NewOrChange[(int) EnumAssets::Model][domain].emplace_back(key, res, ftt);
    } else if(extension == ".glb") {
        pmc = PreparedModel(domain, res);
        out.NewOrChange[(int) EnumAssets::Model][domain].emplace_back(key, res, ftt);
    } else {
        MAKE_ERROR("Не поддерживаемый формат файла");
    }

    out.Models[domain].emplace_back(key, pmc);
}

void AssetsManager::loadResourceFromFile_Texture(ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const {
    Resource res(path);

    if(res.size() < 8)
        MAKE_ERROR("Файл не является текстурой png или jpeg (недостаточный размер файла)");

    if(png_check_sig(reinterpret_cast<png_bytep>((unsigned char*) res.data()), 8)) {
        // Это png
        fs::file_time_type lwt = fs::last_write_time(path);
        out.NewOrChange[(int) EnumAssets::Texture][domain].emplace_back(key, res, lwt);
        return;
    } else if((int) res.data()[0] == 0xFF && (int) res.data()[1] == 0xD8) {
        // Это jpeg
        fs::file_time_type lwt = fs::last_write_time(path);
        out.NewOrChange[(int) EnumAssets::Texture][domain].emplace_back(key, res, lwt);
        return;
    } else {
        MAKE_ERROR("Файл не является текстурой png или jpeg");
    }

    std::unreachable();
}

void AssetsManager::loadResourceFromFile_Sound(ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const {
    std::unreachable();
}

void AssetsManager::loadResourceFromFile_Font(ResourceChangeObj& out, const std::string& domain, const std::string& key, fs::path path) const {
    std::unreachable();
}

void AssetsManager::loadResourceFromLua_Nodestate(ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        out.NewOrChange[(int) EnumAssets::Nodestate][domain].emplace_back(key, Resource(*path), fs::file_time_type::min());
        return;
    }

    std::unreachable();
}

void AssetsManager::loadResourceFromLua_Particle(ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        out.NewOrChange[(int) EnumAssets::Particle][domain].emplace_back(key, Resource(*path), fs::file_time_type::min());
        return;
    }
    
    std::unreachable();
}

void AssetsManager::loadResourceFromLua_Animation(ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        out.NewOrChange[(int) EnumAssets::Animation][domain].emplace_back(key, Resource(*path), fs::file_time_type::min());
        return;
    }
    
    std::unreachable();
}

void AssetsManager::loadResourceFromLua_Model(ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        out.NewOrChange[(int) EnumAssets::Model][domain].emplace_back(key, Resource(*path), fs::file_time_type::min());
        return;
    }
    
    std::unreachable();
}

void AssetsManager::loadResourceFromLua_Texture(ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        out.NewOrChange[(int) EnumAssets::Texture][domain].emplace_back(key, Resource(*path), fs::file_time_type::min());
        return;
    }
    
    std::unreachable();
}

void AssetsManager::loadResourceFromLua_Sound(ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        out.NewOrChange[(int) EnumAssets::Sound][domain].emplace_back(key, Resource(*path), fs::file_time_type::min());
        return;
    }
    
    std::unreachable();
}

void AssetsManager::loadResourceFromLua_Font(ResourceChangeObj& out, const std::string& domain, const std::string& key, const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        out.NewOrChange[(int) EnumAssets::Font][domain].emplace_back(key, Resource(*path), fs::file_time_type::min());
        return;
    }
    
    std::unreachable();
}

AssetsManager::AssetsManager(asio::io_context& ioc)
{

}

AssetsManager::~AssetsManager() = default;

std::tuple<ResourceId, std::optional<AssetsManager::DataEntry>&> AssetsManager::Local::nextId(EnumAssets type) {
    auto& table = Table[(int) type];
    ResourceId id = -1;
    std::optional<DataEntry> *data = nullptr;

    for(size_t index = 0; index < table.size(); index++) {
        auto& entry = *table[index];

        if(entry.IsFull)
            continue;

        uint32_t pos = entry.Empty._Find_first();
        entry.Empty.reset(pos);

        if(entry.Empty._Find_next(pos) == entry.Empty.size())
            entry.IsFull = true;

        id = index*TableEntry<DataEntry>::ChunkSize + pos;
        data = &entry.Entries[pos];
    }

    if(!data) {
        table.emplace_back(std::make_unique<TableEntry<DataEntry>>());
        id = (table.size()-1)*TableEntry<DataEntry>::ChunkSize;
        data = &table.back()->Entries[0];
        table.back()->Empty.reset(0);

        // Расширяем таблицу с ресурсами, если необходимо
        if(type == EnumAssets::Nodestate)
            Table_NodeState.emplace_back(std::make_unique<TableEntry<std::vector<AssetsModel>>>());
        else if(type == EnumAssets::Model)
            Table_Model.emplace_back(std::make_unique<TableEntry<ModelDependency>>());
    
    }

    return {id, *data};
}

AssetsManager::ResourceChangeObj AssetsManager::recheckResources(const AssetsRegister& info) {
    ResourceChangeObj result;

    // Найти пропавшие ресурсы
    for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++) {
        auto lock = LocalObj.lock();
        for(auto& [domain, resources] : lock->KeyToId[type]) {
            for(auto& [key, id] : resources) {
                if(!lock->Table[type][id / TableEntry<DataEntry>::ChunkSize]->Entries[id % TableEntry<DataEntry>::ChunkSize])
                    continue;

                bool exists = false;

                for(const fs::path& path : info.Assets) {
                    fs::path file = path / domain;

                    switch ((EnumAssets) type) {
                    case EnumAssets::Nodestate: file /= "nodestate";   break;
                    case EnumAssets::Particle:  file /= "particle";    break;
                    case EnumAssets::Animation: file /= "animation";   break;
                    case EnumAssets::Model:     file /= "model";       break;
                    case EnumAssets::Texture:   file /= "texture";     break;
                    case EnumAssets::Sound:     file /= "sound";       break;
                    case EnumAssets::Font:      file /= "font";        break;
                    default:
                        std::unreachable();
                    }

                    file /= key;

                    if(fs::exists(file) && !fs::is_directory(file)) {
                        exists = true;
                        break;
                    }
                }

                if(exists) continue;
                
                auto iterDomain = info.Custom[type].find(domain);
                if(iterDomain == info.Custom[type].end()) {
                    result.Lost[type][domain].push_back(key);
                } else {
                    auto iterData = iterDomain->second.find(key);
                    if(iterData == iterDomain->second.end()) {
                        result.Lost[type][domain].push_back(key);
                    }
                }
            }
        }
    }

    // Если ресурс уже был найден более приоритетными директориями, то пропускаем его
    std::unordered_map<std::string, std::unordered_set<std::string>> findedResources[(int) EnumAssets::MAX_ENUM];

    // Найти новые или изменённые ресурсы
    for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++) {
        for(auto& [domain, resources] : info.Custom[type]) {
            auto lock = LocalObj.lock();
            const auto& keyToId = lock->KeyToId[type];
            auto iterDomain = keyToId.find(domain);
            auto& findList = findedResources[type][domain];

            if(iterDomain == keyToId.end()) {
                // Ресурсы данного домена неизвестны
                auto& domainList = result.NewOrChange[type][domain];
                for(auto& [key, id] : resources) {
                    // Подобрать идентификатор
                    // TODO: реализовать регистрации ресурсов из lua
                    domainList.emplace_back(key, Resource("assets/null"), fs::file_time_type::min());
                    findList.insert(key);
                }
            } else {
                for(auto& [key, id] : resources) {
                    if(findList.contains(key))
                        // Ресурс уже был найден в вышестоящей директории
                        continue;
                    else if(iterDomain->second.contains(key)) {
                        // Ресурс уже есть, TODO: нужно проверить его изменение
                        loadResourceFromFile((EnumAssets) type, result, domain, key, "assets/null");
                    } else {
                        // Ресурс не был известен
                        loadResourceFromFile((EnumAssets) type, result, domain, key, "assets/null");
                    }

                    findList.insert(key);
                }
            }
        }
    }

    for(const fs::path& path : info.Assets) {
        if(!fs::exists(path))
            continue;
        
        for(auto begin = fs::directory_iterator(path), end = fs::directory_iterator(); begin != end; begin++) {
            if(!begin->is_directory())
                continue;

            fs::path domainPath = begin->path();
            std::string domain = domainPath.filename();

            for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++) {
                fs::path resourcesPath = domainPath;

                switch ((EnumAssets) type) {
                case EnumAssets::Nodestate: resourcesPath /= "nodestate";   break;
                case EnumAssets::Particle:  resourcesPath /= "particle";    break;
                case EnumAssets::Animation: resourcesPath /= "animation";   break;
                case EnumAssets::Model:     resourcesPath /= "model";       break;
                case EnumAssets::Texture:   resourcesPath /= "texture";     break;
                case EnumAssets::Sound:     resourcesPath /= "sound";       break;
                case EnumAssets::Font:      resourcesPath /= "font";        break;
                default:
                    std::unreachable();
                }

                auto& findList = findedResources[type][domain];
                auto lock = LocalObj.lock();
                auto iterDomain = lock->KeyToId[type].find(domain);

                if(!fs::exists(resourcesPath) || !fs::is_directory(resourcesPath))
                    continue;

                // Рекурсивно загрузить ресурсы внутри папки resourcesPath
                for(auto begin = fs::recursive_directory_iterator(resourcesPath), end = fs::recursive_directory_iterator(); begin != end; begin++) {
                    if(begin->is_directory())
                        continue;

                    fs::path file = begin->path();
                    std::string key = fs::relative(begin->path(), resourcesPath).string();
                    if(findList.contains(key))
                        // Ресурс уже был найден в вышестоящей директории
                        continue;
                    else if(iterDomain != lock->KeyToId[type].end() && iterDomain->second.contains(key)) {
                        // Ресурс уже есть, TODO: нужно проверить его изменение
                        ResourceId id = iterDomain->second.at(key);
                        DataEntry& entry = *lock->Table[type][id / TableEntry<DataEntry>::ChunkSize]->Entries[id % TableEntry<DataEntry>::ChunkSize];
                        
                        fs::file_time_type lwt = fs::last_write_time(file);
                        if(lwt != entry.FileChangeTime)
                            // Будем считать что ресурс изменился
                            loadResourceFromFile((EnumAssets) type, result, domain, key, file);
                    } else {
                        // Ресурс не был известен
                        loadResourceFromFile((EnumAssets) type, result, domain, key, file);
                    }

                    findList.insert(key);
                }
            }
        }

    }

    return result;
}

AssetsManager::Out_applyResourceChange AssetsManager::applyResourceChange(const ResourceChangeObj& orr) {
    // Потерянные и обновлённые идентификаторы
    Out_applyResourceChange result;

    // Удаляем ресурсы
    /*
        Удаляются только ресурсы, при этом за ними остаётся бронь на идентификатор
        Уже скомпилированные зависимости к ресурсам не будут
        перекомпилироваться для смены идентификатора. Если нужный ресурс
        появится, то привязка останется. Новые клиенты не получат ресурс
        которого нет, но он может использоваться
    */
    for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++) {
        for(auto& [domain, resources] : orr.Lost[type]) {
            auto lock = LocalObj.lock();
            auto& keyToIdDomain = lock->KeyToId[type].at(domain);

            for(const std::string& key : resources) {
                auto iter = keyToIdDomain.find(key);
                assert(iter != keyToIdDomain.end());

                ResourceId resId = iter->second;

                if(type == (int) EnumAssets::Nodestate) {
                    if(resId / TableEntry<PreparedNodeState>::ChunkSize < lock->Table_NodeState.size()) {
                        lock->Table_NodeState[resId / TableEntry<PreparedNodeState>::ChunkSize]
                            ->Entries[resId % TableEntry<PreparedNodeState>::ChunkSize].reset();
                    }
                } else if(type == (int) EnumAssets::Model) {
                    if(resId / TableEntry<ModelDependency>::ChunkSize < lock->Table_Model.size()) {
                        lock->Table_Model[resId / TableEntry<ModelDependency>::ChunkSize]
                            ->Entries[resId % TableEntry<ModelDependency>::ChunkSize].reset();
                    }
                }

                auto& chunk = lock->Table[type][resId / TableEntry<DataEntry>::ChunkSize];
                chunk->Entries[resId % TableEntry<DataEntry>::ChunkSize].reset();
            }
        }
    }

    // Добавляем
    for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++) {
        for(auto& [domain, resources] : orr.NewOrChange[type]) {
            auto lock = LocalObj.lock();
            auto& keyToIdDomain = lock->KeyToId[type][domain];

            for(auto& [key, resource, lwt] : resources) {
                ResourceId id = -1;
                std::optional<DataEntry>* data = nullptr;

                if(auto iterId = keyToIdDomain.find(key); iterId != keyToIdDomain.end()) {
                    id = iterId->second;
                    data = &lock->Table[(int) type][id / TableEntry<DataEntry>::ChunkSize]->Entries[id % TableEntry<DataEntry>::ChunkSize];
                } else {
                    auto [_id, _data] = lock->nextId((EnumAssets) type);
                    id = _id;
                    data = &_data;
                }

                result.NewOrChange[type].push_back({id, resource});
                keyToIdDomain[key] = id;

                data->emplace(lwt, resource, domain, key);
            }
        }

        // Удалённые идентификаторы не считаются удалёнными, если были изменены
        std::unordered_set<ResourceId> noc;
        for(auto& [id, _] : result.NewOrChange[type])
            noc.insert(id);
        
        std::unordered_set<ResourceId> l(result.Lost[type].begin(), result.Lost[type].end());
        result.Lost[type].clear();
        std::set_difference(l.begin(), l.end(), noc.begin(), noc.end(), std::back_inserter(result.Lost[type]));
    }

    if(!orr.Nodestates.empty())
    {
        auto lock = LocalObj.lock();
        for(auto& [domain, table] : orr.Nodestates) {
            for(auto& [key, value] : table) {
                ResourceId resId = lock->getId(EnumAssets::Nodestate, domain, key);

                std::vector<AssetsModel> models;

                for(auto& [domain2, key2] : value.ModelToLocalId) {
                    models.push_back(lock->getId(EnumAssets::Model, domain2, key2));
                }

                {
                    std::sort(models.begin(), models.end());
                    auto iterErase = std::unique(models.begin(), models.end());
                    models.erase(iterErase, models.end());
                    models.shrink_to_fit();
                }

                lock->Table_NodeState[resId / TableEntry<DataEntry>::ChunkSize]
                    ->Entries[resId % TableEntry<DataEntry>::ChunkSize] = std::move(models);
            }
        }
    }

    if(!orr.Models.empty())
    {
        auto lock = LocalObj.lock();
        for(auto& [domain, table] : orr.Models) {
            for(auto& [key, value] : table) {
                ResourceId resId = lock->getId(EnumAssets::Model, domain, key);

                ModelDependency deps;
                for(auto& [domain2, list] : value.ModelDependencies) {
                    for(const std::string& key2 : list) {
                        ResourceId subResId = lock->getId(EnumAssets::Model, domain2, key2);
                        deps.ModelDeps.push_back(subResId);
                    }
                }

                for(auto& [domain2, list] : value.TextureDependencies) {
                    for(const std::string& key2 : list) {
                        ResourceId subResId = lock->getId(EnumAssets::Texture, domain2, key2);
                        deps.TextureDeps.push_back(subResId);
                    }
                }

                lock->Table_Model[resId / TableEntry<DataEntry>::ChunkSize]
                    ->Entries[resId % TableEntry<DataEntry>::ChunkSize] = std::move(deps);
            }
        }
    }

    // Вычислить зависимости моделей
    {
        // Затираем старые данные
        auto lock = LocalObj.lock();
        for(auto& entriesChunk : lock->Table_Model) {
            for(auto& entry : entriesChunk->Entries) {
                if(!entry)
                    continue;

                entry->Ready = false;
                entry->FullSubTextureDeps.clear();
                entry->FullSubModelDeps.clear();
            }
        }

        // Вычисляем зависимости
        std::function<void(AssetsModel resId, ModelDependency&)> calcDeps;
        calcDeps = [&](AssetsModel resId, ModelDependency& entry) {
            for(AssetsModel subResId : entry.ModelDeps) {
                auto& model = lock->Table_Model[subResId / TableEntry<ModelDependency>::ChunkSize]
                    ->Entries[subResId % TableEntry<ModelDependency>::ChunkSize];

                if(!model)
                    continue;

                if(resId == subResId) {
                    const auto object1 = lock->getResource(EnumAssets::Model, resId);
                    const auto object2 = lock->getResource(EnumAssets::Model, subResId);
                    LOG.warn() << "В моделе " << std::get<1>(*object1) << ':' << std::get<2>(*object1)
                        << " обнаружена циклическая зависимость с самой собою";
                    continue;
                }

                if(!model->Ready)
                    calcDeps(subResId, *model);

                if(std::binary_search(model->FullSubModelDeps.begin(), model->FullSubModelDeps.end(), resId)) {
                    // Циклическая зависимость
                    const auto object1 = lock->getResource(EnumAssets::Model, resId);
                    const auto object2 = lock->getResource(EnumAssets::Model, subResId);
                    assert(object1);

                    LOG.warn() << "В моделе " << std::get<1>(*object1) << ':' << std::get<2>(*object1)
                        << " обнаружена циклическая зависимость с " << std::get<1>(*object2) << ':' 
                        << std::get<2>(*object2);
                } else {
                    entry.FullSubTextureDeps.append_range(model->FullSubTextureDeps);
                    entry.FullSubModelDeps.push_back(subResId);
                    entry.FullSubModelDeps.append_range(model->FullSubModelDeps);
                }
            }

            {
                std::sort(entry.FullSubTextureDeps.begin(), entry.FullSubTextureDeps.end());
                auto eraseIter = std::unique(entry.FullSubTextureDeps.begin(), entry.FullSubTextureDeps.end());
                entry.FullSubTextureDeps.erase(eraseIter, entry.FullSubTextureDeps.end());
                entry.FullSubTextureDeps.shrink_to_fit();
            }

            {
                std::sort(entry.FullSubModelDeps.begin(), entry.FullSubModelDeps.end());
                auto eraseIter = std::unique(entry.FullSubModelDeps.begin(), entry.FullSubModelDeps.end());
                entry.FullSubModelDeps.erase(eraseIter, entry.FullSubModelDeps.end());
                entry.FullSubModelDeps.shrink_to_fit();
            }

            entry.Ready = true;
        };

        ssize_t iter = -1;
        for(auto& entriesChunk : lock->Table_Model) {
            for(auto& entry : entriesChunk->Entries) {
                iter++;

                if(!entry || entry->Ready)
                    continue;

                // Собираем зависимости
                calcDeps(iter, *entry);
            }
        }
    }

    return result;
}

}