#include "AssetsManager.hpp"
#include "Common/Abstract.hpp"
#include "png++/rgb_pixel.hpp"
#include <filesystem>
#include <png.h>
#include <pngconf.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>


namespace LV::Server {

AssetsManager::Resource AssetsManager::loadResourceFromFile(EnumAssets type, fs::path path) const {
    switch(type) {
    case EnumAssets::Nodestate: return loadResourceFromFile_Nodestate(path);
    case EnumAssets::Particle: return loadResourceFromFile_Particle(path);
    case EnumAssets::Animation: return loadResourceFromFile_Animation(path);
    case EnumAssets::Model: return loadResourceFromFile_Model(path);
    case EnumAssets::Texture: return loadResourceFromFile_Texture(path);
    case EnumAssets::Sound: return loadResourceFromFile_Sound(path);
    case EnumAssets::Font: return loadResourceFromFile_Font(path);
    default:
        std::unreachable();
    }
}

AssetsManager::Resource AssetsManager::loadResourceFromLua(EnumAssets type, const sol::table& profile) const {
    switch(type) {
    case EnumAssets::Nodestate: return loadResourceFromLua_Nodestate(profile);
    case EnumAssets::Particle: return loadResourceFromLua_Particle(profile);
    case EnumAssets::Animation: return loadResourceFromLua_Animation(profile);
    case EnumAssets::Model: return loadResourceFromLua_Model(profile);
    case EnumAssets::Texture: return loadResourceFromLua_Texture(profile);
    case EnumAssets::Sound: return loadResourceFromLua_Sound(profile);
    case EnumAssets::Font: return loadResourceFromLua_Font(profile);
    default:
        std::unreachable();
    }
}

AssetsManager::Resource AssetsManager::loadResourceFromFile_Nodestate(fs::path path) const {
    std::unreachable();
}

AssetsManager::Resource AssetsManager::loadResourceFromFile_Particle(fs::path path) const {
    std::unreachable();
}

AssetsManager::Resource AssetsManager::loadResourceFromFile_Animation(fs::path path) const {
    std::unreachable();
}

AssetsManager::Resource AssetsManager::loadResourceFromFile_Model(fs::path path) const {
    /*
        json, obj, glTF
    */
    std::unreachable();
}

AssetsManager::Resource AssetsManager::loadResourceFromFile_Texture(fs::path path) const {
    Resource res(path);

    if(res.size() < 8)
        MAKE_ERROR("Файл не является текстурой png или jpeg (недостаточный размер файла)");

    if(png_check_sig(reinterpret_cast<png_bytep>((unsigned char*) res.data()), 8)) {
        // Это png
        return res;
    } else if((int) res.data()[0] == 0xFF && (int) res.data()[1] == 0xD8) {
        // Это jpeg
        return res;
    } else {
        MAKE_ERROR("Файл не является текстурой png или jpeg");
    }

    std::unreachable();
}

AssetsManager::Resource AssetsManager::loadResourceFromFile_Sound(fs::path path) const {
    std::unreachable();
}

AssetsManager::Resource AssetsManager::loadResourceFromFile_Font(fs::path path) const {
    std::unreachable();
}

AssetsManager::Resource AssetsManager::loadResourceFromLua_Nodestate(const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        return AssetsManager::Resource(*path);
    }

    std::unreachable();
}

AssetsManager::Resource AssetsManager::loadResourceFromLua_Particle(const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        return AssetsManager::Resource(*path);
    }
    
    std::unreachable();
}

AssetsManager::Resource AssetsManager::loadResourceFromLua_Animation(const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        return AssetsManager::Resource(*path);
    }
    
    std::unreachable();
}

AssetsManager::Resource AssetsManager::loadResourceFromLua_Model(const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        return AssetsManager::Resource(*path);
    }
    
    std::unreachable();
}

AssetsManager::Resource AssetsManager::loadResourceFromLua_Texture(const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        return AssetsManager::Resource(*path);
    }
    
    std::unreachable();
}

AssetsManager::Resource AssetsManager::loadResourceFromLua_Sound(const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        return AssetsManager::Resource(*path);
    }
    
    std::unreachable();
}

AssetsManager::Resource AssetsManager::loadResourceFromLua_Font(const sol::table& profile) const {
    if(std::optional<std::string> path = profile.get<std::optional<std::string>>("path")) {
        return AssetsManager::Resource(*path);
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

        if(entry.Empty._Find_first() == entry.Empty.size())
            entry.IsFull = true;

        id = index*TableEntry::ChunkSize + pos;
        data = &entry.Entries[pos];
    }

    if(!data) {
        table.emplace_back(std::make_unique<TableEntry>());
        id = (table.size()-1)*TableEntry::ChunkSize;
        data = &table.back()->Entries[0];
    }

    return {id, *data};
}

AssetsManager::Out_recheckResources AssetsManager::recheckResources(const AssetsRegister& info) {
    Out_recheckResources result;

    // Найти пропавшие ресурсы
    for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++) {
        auto lock = LocalObj.lock();
        for(auto& [domain, resources] : lock->KeyToId[type]) {
            for(auto& [key, id] : resources) {
                if(!lock->Table[type][id / TableEntry::ChunkSize]->Entries[id % TableEntry::ChunkSize])
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
                        result.NewOrChange[type][domain].emplace_back(key, loadResourceFromFile((EnumAssets) type, "assets/null"), fs::file_time_type::min());
                    } else {
                        // Ресурс не был известен
                        result.NewOrChange[type][domain].emplace_back(key, loadResourceFromFile((EnumAssets) type, "assets/null"), fs::file_time_type::min());
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
                        DataEntry& entry = *lock->Table[type][id / TableEntry::ChunkSize]->Entries[id % TableEntry::ChunkSize];
                        
                        fs::file_time_type lwt = fs::last_write_time(file);
                        if(lwt != entry.FileChangeTime)
                            // Будем считать что ресурс изменился
                            result.NewOrChange[type][domain].emplace_back(key, loadResourceFromFile((EnumAssets) type, file), lwt);
                    } else {
                        // Ресурс не был известен
                        fs::file_time_type lwt = fs::last_write_time(file);
                        result.NewOrChange[type][domain].emplace_back(key, loadResourceFromFile((EnumAssets) type, file), lwt);
                    }

                    findList.insert(key);
                }
            }
        }

    }

    return result;
}

AssetsManager::Out_applyResourceChange AssetsManager::applyResourceChange(const Out_recheckResources& orr) {
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
                // keyToIdDomain.erase(iter);
                // lost[type].push_back(resId);

                uint32_t localId = resId % TableEntry::ChunkSize;
                auto& chunk = lock->Table[type][resId / TableEntry::ChunkSize];
                // chunk->IsFull = false;
                // chunk->Empty.set(localId);
                chunk->Entries[localId].reset();
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
                    data = &lock->Table[(int) type][id / TableEntry::ChunkSize]->Entries[id % TableEntry::ChunkSize];
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

    return result;
}

}