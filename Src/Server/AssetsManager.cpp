#include "AssetsManager.hpp"
#include "Common/Abstract.hpp"
#include <filesystem>
#include <unordered_map>
#include <unordered_set>


namespace LV::Server {


coro<AssetsManager::Out_recheckResources> AssetsManager::recheckResources(AssetsRegister info) const {
    Out_recheckResources result;

    // Найти пропавшие ресурсы
    for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++) {
        for(auto& [domain, resources] : KeyToId[type]) {
            for(auto& [key, id] : resources) {
                bool exists = false;

                for(const fs::path& path : info.Assets) {
                    fs::path file = path / domain;

                    switch ((EnumAssets) type) {
                    case EnumAssets::Nodestate: file /= "nodestate";   break;
                    case EnumAssets::Patricle:  file /= "particle";    break;
                    case EnumAssets::Animation: file /= "animation";   break;
                    case EnumAssets::Model:     file /= "model";       break;
                    case EnumAssets::Texture:   file /= "texture";     break;
                    case EnumAssets::Sound:     file /= "sound";       break;
                    case EnumAssets::Font:      file /= "font";        break;
                    default:
                        assert(false);
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
        const auto& keyToId = KeyToId[type];

        for(auto& [domain, resources] : info.Custom[type]) {
            auto iterDomain = keyToId.find(domain);
            auto& findList = findedResources[type][domain];

            if(iterDomain == keyToId.end()) {
                // Ресурсы данного домена неизвестны
                auto& domainList = result.NewOrChange[type][domain];
                for(auto& [key, id] : resources) {
                    // Подобрать идентификатор
                    // TODO: реализовать регистрации ресурсов из lua
                    domainList.emplace_back(key, Resource("assets/null"));
                    findList.insert(key);
                }
            } else {
                for(auto& [key, id] : resources) {
                    if(findList.contains(key))
                        // Ресурс уже был найден в вышестоящей директории
                        continue;
                    else if(iterDomain->second.contains(key)) {
                        // Ресурс уже есть, TODO: нужно проверить его изменение
                    } else {
                        // Ресурс не был известен
                        result.NewOrChange[type][domain].emplace_back(key, Resource("assets/null"));
                    }

                    findList.insert(key);
                }
            }
        }
    }

    for(const fs::path& path : info.Assets) {
        for(auto begin = fs::directory_iterator(path), end = fs::directory_iterator(); begin != end; begin++) {
            if(!begin->is_directory())
                continue;

            fs::path domainPath = begin->path();
            std::string domain = domainPath.filename();

            for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++) {
                fs::path resourcesPath = domainPath;

                switch ((EnumAssets) type) {
                case EnumAssets::Nodestate: resourcesPath /= "nodestate";   break;
                case EnumAssets::Patricle:  resourcesPath /= "particle";    break;
                case EnumAssets::Animation: resourcesPath /= "animation";   break;
                case EnumAssets::Model:     resourcesPath /= "model";       break;
                case EnumAssets::Texture:   resourcesPath /= "texture";     break;
                case EnumAssets::Sound:     resourcesPath /= "sound";       break;
                case EnumAssets::Font:      resourcesPath /= "font";        break;
                default:
                    assert(false);
                }

                auto& findList = findedResources[type][domain];
                auto iterDomain = KeyToId[type].find(domain);

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

                    else if(iterDomain != KeyToId[type].end() && iterDomain->second.contains(key)) {
                        // Ресурс уже есть, TODO: нужно проверить его изменение
                        ResourceId_t id = iterDomain->second.at(key);
                        DataEntry& entry = *Table[type][id / TableEntry::ChunkSize]->Entries[id % TableEntry::ChunkSize];
                        
                        fs::file_time_type lwt = fs::last_write_time(file);
                        if(lwt <= entry.FileChangeTime)
                            continue;

                        
                    } else {
                        // Ресурс не был известен
                        result.NewOrChange[type][domain].emplace_back(key, Resource("assets/null"));
                    }

                    findList.insert(key);
                }
            }
        }

    }

    co_return result;
}

}