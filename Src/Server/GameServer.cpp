#include "GameServer.hpp"
#include "Common/Abstract.hpp"
#include "Common/Net.hpp"
#include "Common/Packets.hpp"
#include "Server/Abstract.hpp"
#include "Server/ContentManager.hpp"
#include "Server/RemoteClient.hpp"
#include <algorithm>
#include <array>
#include <boost/json/parse.hpp>
#include <chrono>
#include <filesystem>
#include <functional>
#include <glm/geometric.hpp>
#include <glm/gtc/noise.hpp>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <sol/forward.hpp>
#include <sol/protected_function_result.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "SaveBackends/Filesystem.hpp"
#include "Server/SaveBackend.hpp"
#include "Server/World.hpp"
#include "TOSLib.hpp"
#include "boost/json.hpp"
#include "boost/json/array.hpp"
#include "boost/json/object.hpp"
#include "boost/json/parse_into.hpp"
#include "boost/json/serialize.hpp"
#include "glm/gtc/noise.hpp"
#include <fstream>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace js = boost::json;

namespace LV::Server {

template <typename T, size_t N>
bool hasAnyBindings(const std::array<std::vector<T>, N>& data) {
    for(const auto& list : data) {
        if(!list.empty())
            return true;
    }
    return false;
}

std::string ModInfo::dump() const {
    js::object obj;

    obj["id"] = Id;
    obj["name"] = Name;
    obj["description"] = Description;
    obj["author"] = Author;
    obj["version"] = {Version[0], Version[1], Version[2], Version[3]};
    obj["hasLiveReload"] = HasLiveReload;
    
    {
        js::array arr;
        for(const auto& depend : Dependencies) {
            js::object obj;
            obj["id"] = depend.Id;
            obj["version_min"] = {depend.MinVersion[0], depend.MinVersion[1], depend.MinVersion[2], depend.MinVersion[3]};
            obj["version_max"] = {depend.MaxVersion[0], depend.MaxVersion[1], depend.MaxVersion[2], depend.MaxVersion[3]};
            arr.push_back(obj);
        }

        obj["depend"] = arr;
    }

    {
        js::array arr;
        for(const auto& depend : Optional) {
            js::object obj;
            obj["id"] = depend.Id;
            obj["version_min"] = {depend.MinVersion[0], depend.MinVersion[1], depend.MinVersion[2], depend.MinVersion[3]};
            obj["version_max"] = {depend.MaxVersion[0], depend.MaxVersion[1], depend.MaxVersion[2], depend.MaxVersion[3]};
            arr.push_back(obj);
        }

        obj["optional_depend"] = arr;
    }

    obj["load_priority"] = LoadPriority;
    obj["path"] = Path.string();

    return js::serialize(obj);
}

struct ModPreloadInfo {
    std::vector<ModInfo> Mods;
    std::vector<std::string> Errors;
};

ModPreloadInfo preLoadMods(const std::vector<fs::path>& dirs) {
    std::vector<ModInfo> mods;
    std::vector<std::string> errors;

    for(const fs::path& p : dirs) {
        try {
            if(!fs::is_directory(p))
                errors.push_back("Объект не является директорией: " + p.string());
            else {
                fs::directory_iterator begin(p), end;
                for(; begin != end; begin++) {
                    if(!begin->is_directory())
                        continue;

                    fs::path modPath = begin->path();
                    fs::path modJson = modPath / "mod.json";

                    if(!fs::exists(modJson)) {
                        errors.push_back("В директории мода отсутствует файл mod.json: " + modJson.string());
                    } else {
                        std::string data;
                        try {
                            std::ifstream fd(modJson);
                            fd.exceptions(std::ifstream::failbit | std::ifstream::badbit);
                            
                            fd.seekg(0, std::ios::end);
                            std::streamsize size = fd.tellg();
                            fd.seekg(0, std::ios::beg);

                            if(size > 1024*1024)
                                MAKE_ERROR("Превышен размер файла (1 мб)");
                            
                            data.resize(size);
                            fd.read((char*) data.data(), size);
                        } catch (const std::exception& exc) {
                            errors.push_back("Не удалось считать mod.json '" + modPath.string() + "': " + exc.what());
                            goto skip;
                        }

                        try {
                            js::object obj = js::parse(data).as_object();

                            ModInfo info;
                            info.Id = obj.at("id").as_string();
                            info.Name = obj.contains("title") ? obj["title"].as_string() : "";
                            info.Description = obj.contains("description") ? obj["description"].as_string() : "";
                            info.Author = obj.contains("author") ? obj["author"].as_string() : "";
                            info.HasLiveReload = obj.contains("hasLiveReload") ? obj["hasLiveReload"].as_bool() : false;

                            {
                                js::array version = obj.at("version").as_array();
                                for(int iter = 0; iter < 4; iter++)
                                    info.Version[iter] = version.at(iter).as_int64();
                            }

                            if(obj.contains("depends")) {
                                js::array arr = obj["depends"].as_array();
                                for(auto& iter : arr) {
                                    ModRequest depend;

                                    if(iter.is_string()) {
                                        depend.Id = iter.as_string();
                                        std::fill(depend.MinVersion.begin(), depend.MinVersion.end(), 0);
                                        std::fill(depend.MaxVersion.begin(), depend.MaxVersion.end(), uint32_t(-1));
                                    } else if(iter.is_object()) {
                                        js::object d = iter.as_object();
                                        depend.Id = d.at("id").as_string();

                                        if(d.contains("version_min")) {
                                            js::array v = d.at("version_min").as_array();
                                            for(int iter = 0; iter < 4; iter++) {
                                                depend.MinVersion[iter] = v.at(iter).as_int64();
                                            }
                                        } else
                                            std::fill(depend.MinVersion.begin(), depend.MinVersion.end(), 0);

                                        if(d.contains("version_max")) {
                                            js::array v = d.at("version_max").as_array();
                                            for(int iter = 0; iter < 4; iter++) {
                                                depend.MaxVersion[iter] = v.at(iter).as_int64();
                                            }
                                        } else
                                            std::fill(depend.MaxVersion.begin(), depend.MaxVersion.end(), uint32_t(-1));
                                    }

                                    info.Dependencies.push_back(depend);
                                }
                            }

                            
                            if(obj.contains("optional_depends")) {
                                js::array arr = obj["optional_depends"].as_array();
                                for(auto& iter : arr) {
                                    ModRequest depend;

                                    if(iter.is_string()) {
                                        depend.Id = iter.as_string();
                                        std::fill(depend.MinVersion.begin(), depend.MinVersion.end(), 0);
                                        std::fill(depend.MaxVersion.begin(), depend.MaxVersion.end(), uint32_t(-1));
                                    } else if(iter.is_object()) {
                                        js::object d = iter.as_object();
                                        depend.Id = d.at("id").as_string();

                                        if(d.contains("version_min")) {
                                            js::array v = d.at("version_min").as_array();
                                            for(int iter = 0; iter < 4; iter++) {
                                                depend.MinVersion[iter] = v.at(iter).as_int64();
                                            }
                                        } else
                                            std::fill(depend.MinVersion.begin(), depend.MinVersion.end(), 0);

                                        if(d.contains("version_max")) {
                                            js::array v = d.at("version_max").as_array();
                                            for(int iter = 0; iter < 4; iter++) {
                                                depend.MaxVersion[iter] = v.at(iter).as_int64();
                                            }
                                        } else
                                            std::fill(depend.MaxVersion.begin(), depend.MaxVersion.end(), uint32_t(-1));
                                    }

                                    info.Optional.push_back(depend);
                                }
                            }

                            if(obj.contains("load_priority")) {
                                info.LoadPriority = obj.at("load_priority").as_double();
                            } else {
                                info.LoadPriority = 0.5;
                            }

                            info.Path = modPath;
                            mods.push_back(info);

                        } catch (const std::exception& exc) {
                            errors.push_back("Не удалось распарсить mod.json '" + modPath.string() + "': " + exc.what());
                            goto skip;
                        }
                    }

                    skip:
                }
            }
        } catch(const std::exception& exc) {
            errors.push_back("Неопределённая ошибка при работе с директорией: " + p.string());
        }
    }

    return {mods, errors};
}

std::vector<std::vector<ModInfo>> rangDepends(const std::vector<ModInfo>& mods) {
    std::vector<std::vector<ModInfo>> ranging;

    std::vector<ModInfo> state, next = mods;
    while(!next.empty()) {
        state = std::move(next);

        for(size_t index = 0; index < state.size(); index++) {
            ModInfo &mod = state[index];
            std::vector<ModRequest> depends = mod.Dependencies;
            depends.insert(depends.end(), mod.Optional.begin(), mod.Optional.end());

            for(ModRequest &depend : depends) {
                for(size_t index2 = 0; index2 < state.size(); index2++) {
                    ModInfo &mod2 = state[index];
                    if(depend.Id == mod2.Id) {
                        next.push_back(mod2);
                        state.erase(state.begin()+index2);
                        if(index2 <= index)
                            index--;
                        index2--;
                        break;
                    }
                }
            }
        }

        if(state.empty()) {
            // Циклическая зависимость
            ranging.push_back(std::move(next));
            break;
        } 

        ranging.push_back(std::move(state));
    }

    for(auto& list : ranging)
        std::sort(list.begin(), list.end(), [](const ModInfo& left, const ModInfo& right){ return left.LoadPriority < right.LoadPriority; });

    return ranging;
}

struct ModLoadTree {
    std::vector<ModInfo> UnloadChain, LoadChain;
};

std::variant<ModLoadTree, std::vector<std::string>> buildLoadChain(const std::vector<ModInfo>& loaded, const std::vector<ModInfo>& toUnload, const std::vector<ModInfo>& toLoad) {
    // Проверить обязательные зависимости в конечном состоянии
    {
        std::vector<std::string> errors;
        std::vector<ModInfo> endState;
        for(const ModInfo& lmod : loaded) {
            bool contains = false;
            
            for(const ModInfo& umod : toUnload) {
                if(lmod.Id == umod.Id) {
                    contains = true;
                    break;
                }
            }

            if(!contains)
                endState.push_back(lmod);
        }

        endState.insert(endState.end(), toLoad.begin(), toLoad.end());

        for(const ModInfo& mmod : endState) {
            for(const ModRequest& depend : mmod.Dependencies) {
                std::vector<std::string> lerrors;
                bool contains = false;

                for(const ModInfo& mmod2 : endState) {
                    if(depend.Id != mmod2.Id)
                        continue;

                    if(depend.MinVersion > mmod2.Version) {
                        goto versionMismatch;
                    }

                    if(depend.MaxVersion[0] != uint32_t(-1)) {
                        if(depend.MaxVersion < mmod2.Version
                            || depend.MaxVersion[1] < mmod2.Version[1]
                            || depend.MaxVersion[2] < mmod2.Version[2]
                            || depend.MaxVersion[3] < mmod2.Version[3]
                        ) {
                            goto versionMismatch;
                        }
                    }

                    contains = true;
                    continue;

                    versionMismatch:
                    std::stringstream ss;
                    ss << depend.MinVersion[0] << '.';
                    ss << depend.MinVersion[1] << '.';
                    ss << depend.MinVersion[2] << '.';
                    ss << depend.MinVersion[3];
                    std::string verMin = ss.str();
                    ss.str("");

                    if(depend.MaxVersion[0] != uint32_t(-1)) {
                        ss << depend.MaxVersion[0] << '.';
                    ss << (depend.MaxVersion[1] == uint32_t(-1) ? "*" : std::to_string(depend.MaxVersion[1])) << '.';
                    ss << (depend.MaxVersion[2] == uint32_t(-1) ? "*" : std::to_string(depend.MaxVersion[2])) << '.';
                    ss << (depend.MaxVersion[3] == uint32_t(-1) ? "*" : std::to_string(depend.MaxVersion[3])) << '.';
                        verMin += " -> " + ss.str();
                        ss.str("");
                    }

                    ss << mmod2.Version[0] << '.';
                    ss << mmod2.Version[1] << '.';
                    ss << mmod2.Version[2] << '.';
                    ss << mmod2.Version[3];
                    std::string ver = ss.str();
                    ss.str("");

                    lerrors.push_back("Мод " + mmod.Name+"("+mmod.Id+") требует "+mmod2.Name+"("+mmod2.Id+", "+verMin+"), найдена версия " + ver);
                }

                if(!contains)
                    errors.insert(errors.end(), lerrors.begin(), lerrors.end());
            }
        }

        if(!errors.empty())
            return errors;
    }
    
    std::vector<ModInfo> unloadChain;

    {
        std::vector<std::vector<ModInfo>> rangeUnload = rangDepends(toUnload);
        for(auto begin = rangeUnload.begin(), end = rangeUnload.end(); begin != end; begin++) {
            unloadChain.insert(unloadChain.end(), begin->rbegin(), begin->rend());
        }
    }

    std::vector<ModInfo> loadChain;

    {
        std::vector<std::vector<ModInfo>> rangeLoad = rangDepends(toLoad);
        for(auto begin = rangeLoad.rbegin(), end = rangeLoad.rend(); begin != end; begin++) {
            loadChain.insert(loadChain.end(), begin->begin(), begin->end());
        }
    }

    return ModLoadTree{unloadChain, loadChain};
}

/*
    Находит необходимые моды в доступных загруженных и зависимости к ним
*/

std::variant<std::vector<ModInfo>, std::vector<std::string>> resolveDepends(const std::vector<ModRequest>& requests, const std::vector<ModInfo>& mods) {
    std::vector<ModInfo> toLoad;
    std::vector<std::string> errors;

    std::vector<ModRequest> next;

    // Найти те, что имеют чёткую версию загрузки
    // Собрать с не чёткой версией загрузки

    for(const ModRequest& request : requests) {
        if(request.MinVersion != request.MaxVersion) {
            next.push_back(request);
            continue;
        }

        bool find = false, versionMismatch = false;

        for(const ModInfo& mod : mods) {
            if(request.Id != mod.Id)
                continue;

            if(request.MaxVersion != mod.Version) {
                versionMismatch = true;
                continue;
            }

            find = true;
            toLoad.push_back(mod);
            next.insert(next.end(), mod.Dependencies.begin(), mod.Dependencies.end());
            break;
        }

        if(!find) {
            if(versionMismatch)
                errors.push_back("Не найден мод " + request.Id + " соответствующей версии");
            else
                errors.push_back("Не найден мод " + request.Id);
        }
    }

    // assert(next.empty());
    // :(

    for(const ModRequest& request : next) {
        bool find = false, versionMismatch = false;

        for(const ModInfo& mod : mods) {
            if(request.Id != mod.Id)
                continue;

            if(request.MaxVersion < mod.Version) {
                versionMismatch = true;
                continue;
            }

            if(request.MinVersion > mod.Version) {
                versionMismatch = true;
                continue;
            }

            find = true;
            toLoad.push_back(mod);
            break;
        }

        if(!find) {
            if(versionMismatch)
                errors.push_back("Не найден мод " + request.Id + " соответствующей версии");
            else
                errors.push_back("Не найден мод " + request.Id);
        }
    }


    if(!errors.empty())
        return errors;

    return toLoad;
}

GameServer::GameServer(asio::io_context &ioc, fs::path worldPath)
    : AsyncObject(ioc),
        Content(ioc)
{
    init(worldPath);
}
    
GameServer::~GameServer() {
    shutdown("on ~GameServer");
    BackingChunkPressure.NeedShutdown = true;
    BackingChunkPressure.Symaphore.notify_all();
    BackingNoiseGenerator.NeedShutdown = true;
    BackingAsyncLua.NeedShutdown = true;

    RunThread.join();
    WorkDeadline.cancel();
    UseLock.wait_no_use();

    BackingChunkPressure.stop();
    BackingNoiseGenerator.stop();
    BackingAsyncLua.stop();

    LOG.info() << "Сервер уничтожен";
}

void GameServer::BackingChunkPressure_t::run(int id) {
    // static thread_local int local_counter = -1;
    int iteration = 0;
    LOG.debug() << "Старт потока " << id;

    try {
        while(true) {
            // local_counter++;
            // LOG.debug() << "Ожидаю начала " << id << ' ' << local_counter;
            {
                std::unique_lock<std::mutex> lock(Mutex);
                Symaphore.wait(lock, [&](){ return iteration != Iteration || NeedShutdown; });
                if(NeedShutdown) {
                    LOG.debug() << "Завершение выполнения потока " << id;
                    break;
                }

                iteration = Iteration;
            }

            assert(RunCollect > 0);
            assert(RunCompress > 0);

            // Сбор данных
            size_t pullSize = Threads.size();
            size_t counter = 0;

            struct Dump {
                std::vector<std::shared_ptr<RemoteClient>> CECs, NewCECs;
                std::unordered_map<Pos::bvec4u, std::vector<VoxelCube>> Voxels;
                std::unordered_map<Pos::bvec4u, std::array<Node, 16*16*16>> Nodes;
                uint64_t IsChunkChanged_Nodes, IsChunkChanged_Voxels;
            };

            std::vector<std::pair<WorldId_t, std::vector<std::pair<Pos::GlobalRegion, Dump>>>> dump;

            for(const auto& [worldId, world] : *Worlds) {
                const auto &worldObj = *world;
                std::vector<std::pair<Pos::GlobalRegion, Dump>> dumpWorld;

                for(const auto& [regionPos, region] : worldObj.Regions) {
                    auto& regionObj = *region;
                    if(counter++ % pullSize != id) {
                        continue;
                    }

                    Dump dumpRegion;

                    dumpRegion.CECs = regionObj.RMs;
                    dumpRegion.IsChunkChanged_Voxels = regionObj.IsChunkChanged_Voxels;
                    regionObj.IsChunkChanged_Voxels = 0;
                    dumpRegion.IsChunkChanged_Nodes = regionObj.IsChunkChanged_Nodes;
                    regionObj.IsChunkChanged_Nodes = 0;
                    
                    if(!regionObj.NewRMs.empty()) {
                        dumpRegion.NewCECs = std::move(regionObj.NewRMs);
                        dumpRegion.Voxels = regionObj.Voxels;

                        for(int z = 0; z < 4; z++)
                            for(int y = 0; y < 4; y++)
                                for(int x = 0; x < 4; x++) 
                            {
                                auto &toPtr = dumpRegion.Nodes[Pos::bvec4u(x, y, z)];
                                const Node *fromPtr = regionObj.Nodes[Pos::bvec4u(x, y, z).pack()].data();
                                std::copy(fromPtr, fromPtr+16*16*16, toPtr.data());
                            }
                    } else {
                        if(dumpRegion.IsChunkChanged_Voxels) {
                            for(int index = 0; index < 64; index++) {
                                if(((dumpRegion.IsChunkChanged_Voxels >> index) & 0x1) == 0)
                                    continue;

                                Pos::bvec4u chunkPos;
                                chunkPos.unpack(index);

                                auto voxelIter = regionObj.Voxels.find(chunkPos);
                                if(voxelIter != regionObj.Voxels.end()) {
                                    dumpRegion.Voxels[chunkPos] = voxelIter->second;
                                } else {
                                    dumpRegion.Voxels[chunkPos] = {};
                                }
                            }
                        }

                        if(dumpRegion.IsChunkChanged_Nodes) {
                            for(int index = 0; index < 64; index++) {
                                if(((dumpRegion.IsChunkChanged_Nodes >> index) & 0x1) == 0)
                                    continue;

                                Pos::bvec4u chunkPos;
                                chunkPos.unpack(index);

                                auto &toPtr = dumpRegion.Nodes[chunkPos];
                                const Node *fromPtr = regionObj.Nodes[chunkPos.pack()].data();
                                std::copy(fromPtr, fromPtr+16*16*16, toPtr.data());
                            }
                        }
                    }

                    if(!dumpRegion.CECs.empty()) {
                        dumpWorld.push_back({regionPos, std::move(dumpRegion)});
                    }
                }

                if(!dumpWorld.empty()) {
                    dump.push_back({worldId, std::move(dumpWorld)});
                }
            }

            // Синхронизация
            // LOG.debug() << "Синхронизирую " << id << ' ' << local_counter;
            {
                std::unique_lock<std::mutex> lock(Mutex);
                RunCollect -= 1;
                Symaphore.notify_all();
            }

            // Сжатие и отправка игрокам
            for(auto& [worldId, world] : dump) {
                for(auto& [regionPos, region] : world) {
                    for(auto& [chunkPos, chunk] : region.Voxels) {
                        std::u8string cmp = compressVoxels(chunk);

                        for(auto& ptr : region.NewCECs) {
                            ptr->prepareChunkUpdate_Voxels(worldId, regionPos, chunkPos, cmp);
                        }

                        if((region.IsChunkChanged_Voxels >> chunkPos.pack()) & 0x1) {
                            for(auto& ptr : region.CECs) {
                                bool skip = false;
                                for(auto& ptr2 : region.NewCECs) {
                                    if(ptr == ptr2) {
                                        skip = true;
                                        break;
                                    }
                                }

                                if(skip)
                                    continue;

                                ptr->prepareChunkUpdate_Voxels(worldId, regionPos, chunkPos, cmp);
                            }
                        }
                    }

                    for(auto& [chunkPos, chunk] : region.Nodes) {
                        std::u8string cmp = compressNodes(chunk.data());

                        for(auto& ptr : region.NewCECs) {
                            ptr->prepareChunkUpdate_Nodes(worldId, regionPos, chunkPos, cmp);
                        }

                        if((region.IsChunkChanged_Nodes >> chunkPos.pack()) & 0x1) {
                            for(auto& ptr : region.CECs) {
                                bool skip = false;
                                for(auto& ptr2 : region.NewCECs) {
                                    if(ptr == ptr2) {
                                        skip = true;
                                        break;
                                    }
                                }

                                if(skip)
                                    continue;

                                ptr->prepareChunkUpdate_Nodes(worldId, regionPos, chunkPos, cmp);
                            }
                        }
                    }
                }
            }

            // Синхронизация
            // LOG.debug() << "Конец " << id << ' ' << local_counter;
            {
                std::unique_lock<std::mutex> lock(Mutex);
                RunCompress -= 1;
                Symaphore.notify_all();
            }
        }
    } catch(const std::exception& exc) {
        std::unique_lock<std::mutex> lock(Mutex);
        NeedShutdown = true;
        LOG.error() << "Ошибка выполнения потока " << id << ":\n" << exc.what();
    }

    Symaphore.notify_all();
}

void GameServer::BackingNoiseGenerator_t::run(int id) {

    LOG.debug() << "Старт потока " << id;

    try {
        while(true) {
            if(NeedShutdown) {
                LOG.debug() << "Завершение выполнения потока " << id;
                break;
            }

            if(Input.get_read().empty())
                TOS::Time::sleep3(50);

            NoiseKey key;

            {
                auto lock = Input.lock();
                if(lock->empty())
                    continue;

                key = lock->front();
                lock->pop();
            }

            Pos::GlobalNode posNode = key.RegionPos;
            posNode <<= 6;

            std::array<float, 64*64*64> data;
            float *ptr = &data[0];
            std::fill(ptr, ptr+64*64*64, 0);

            // for(int z = 0; z < 64; z++)
            //     for(int y = 0; y < 64; y++)
            //     for(int x = 0; x < 64; x++, ptr++) {
            //         // *ptr = TOS::genRand();
            //         *ptr = glm::perlin(glm::vec3(posNode.x+x, posNode.y+y, posNode.z+z) / 16.13f);
            //         //*ptr = std::pow(*ptr, 0.75f)*1.5f;
            //     }

            Output.lock()->push_back({key, std::move(data)});
        }
    } catch(const std::exception& exc) {
        NeedShutdown = true;
        LOG.error() << "Ошибка выполнения потока " << id << ":\n" << exc.what();
    }

}

void GameServer::BackingAsyncLua_t::run(int id) {
    LOG.debug() << "Старт потока " << id;

    BackingNoiseGenerator_t::NoiseKey key;
    std::array<float, 64*64*64> noise;
    World::RegionIn out;

    try {
        while(true) {
            if(NeedShutdown) {
                LOG.debug() << "Завершение выполнения потока " << id;
                break;
            }

            if(NoiseIn.get_read().empty())
                TOS::Time::sleep3(50);

            {
                auto lock = NoiseIn.lock();
                if(lock->empty())
                    continue;

                key = lock->front().first;
                noise = lock->front().second;
                lock->pop();
            }

            out.Voxels.clear();
            out.Entityes.clear();

            {
                constexpr DefNodeId kNodeAir = 0;
                constexpr DefNodeId kNodeGrass = 2;
                constexpr uint8_t kMetaGrass = 1;
                constexpr DefNodeId kNodeDirt = 3;
                constexpr DefNodeId kNodeStone = 4;
                constexpr DefNodeId kNodeWood = 1;
                constexpr DefNodeId kNodeLeaves = 5;
                constexpr DefNodeId kNodeLava = 7;
                constexpr DefNodeId kNodeWater = 8;
                constexpr DefNodeId kNodeFire = 9;

                auto hash32 = [](uint32_t x) {
                    x ^= x >> 16;
                    x *= 0x7feb352dU;
                    x ^= x >> 15;
                    x *= 0x846ca68bU;
                    x ^= x >> 16;
                    return x;
                };

                Pos::GlobalNode regionBase = key.RegionPos;
                regionBase <<= 6;

                std::array<int, 64*64> heights;
                for(int z = 0; z < 64; z++) {
                    for(int x = 0; x < 64; x++) {
                        int32_t gx = regionBase.x + x;
                        int32_t gz = regionBase.z + z;
                        float fx = float(gx);
                        float fz = float(gz);

                        float base = glm::perlin(glm::vec2(fx * 0.005f, fz * 0.005f));
                        float detail = glm::perlin(glm::vec2(fx * 0.02f, fz * 0.02f)) * 0.35f;
                        float ridge = glm::perlin(glm::vec2(fx * 0.0015f, fz * 0.0015f));
                        float ridged = 1.f - std::abs(ridge);
                        float mountains = ridged * ridged;
                        float noiseDetail = noise[(z * 64) + x];

                        float height = 18.f + (base + detail) * 8.f + mountains * 32.f + noiseDetail * 3.f;
                        int h = std::clamp<int>(int(height + 0.5f), -256, 256);
                        heights[z * 64 + x] = h;
                    }
                }

                for(int z = 0; z < 64; z++) {
                    for(int x = 0; x < 64; x++) {
                        int surface = heights[z * 64 + x];
                        int32_t gx = regionBase.x + x;
                        int32_t gz = regionBase.z + z;
                        uint32_t seed = hash32(uint32_t(gx) * 73856093u ^ uint32_t(gz) * 19349663u);

                        for(int y = 0; y < 64; y++) {
                            int32_t gy = regionBase.y + y;
                            Pos::bvec64u nodePos(x, y, z);
                            auto &node = out.Nodes[Pos::bvec4u(nodePos >> 4).pack()][Pos::bvec16u(nodePos & 0xf).pack()];

                            if(gy <= surface) {
                                if(gy == surface) {
                                    node.NodeId = kNodeGrass;
                                    node.Meta = kMetaGrass;
                                } else if(gy >= surface - 3) {
                                    node.NodeId = kNodeDirt;
                                    node.Meta = uint8_t((seed + gy) & 0x3);
                                } else {
                                    node.NodeId = kNodeStone;
                                    node.Meta = uint8_t((seed + gy + 1) & 0x3);
                                }
                            } else {
                                node.Data = kNodeAir;
                            }
                        }
                    }
                }

                auto setNode = [&](int x, int y, int z, DefNodeId id, uint8_t meta, bool onlyAir) {
                    if(x < 0 || x >= 64 || y < 0 || y >= 64 || z < 0 || z >= 64)
                        return;

                    Pos::bvec64u nodePos(x, y, z);
                    auto &node = out.Nodes[Pos::bvec4u(nodePos >> 4).pack()][Pos::bvec16u(nodePos & 0xf).pack()];
                    if(onlyAir && node.Data != 0)
                        return;

                    node.NodeId = id;
                    node.Meta = meta;
                };

                for(int z = 1; z < 63; z++) {
                    for(int x = 1; x < 63; x++) {
                        int surface = heights[z * 64 + x];
                        int localY = surface - regionBase.y;
                        if(localY < 1 || localY >= 63)
                            continue;

                        int32_t gx = regionBase.x + x;
                        int32_t gz = regionBase.z + z;
                        uint32_t seed = hash32(uint32_t(gx) * 83492791u ^ uint32_t(gz) * 2971215073u);

                        int treeHeight = 4 + int(seed % 3);
                        if(localY + treeHeight + 2 >= 64)
                            continue;

                        if((seed % 97) >= 2)
                            continue;

                        int diff = surface - heights[z * 64 + (x - 1)];
                        if(diff > 2 || diff < -2)
                            continue;
                        diff = surface - heights[z * 64 + (x + 1)];
                        if(diff > 2 || diff < -2)
                            continue;
                        diff = surface - heights[(z - 1) * 64 + x];
                        if(diff > 2 || diff < -2)
                            continue;
                        diff = surface - heights[(z + 1) * 64 + x];
                        if(diff > 2 || diff < -2)
                            continue;

                        uint8_t woodMeta = uint8_t((seed >> 2) & 0x3);
                        uint8_t leafMeta = uint8_t((seed >> 4) & 0x3);

                        for(int i = 1; i <= treeHeight; i++) {
                            setNode(x, localY + i, z, kNodeWood, woodMeta, false);
                        }

                        int topY = localY + treeHeight;
                        for(int dy = -2; dy <= 2; dy++) {
                            for(int dz = -2; dz <= 2; dz++) {
                                for(int dx = -2; dx <= 2; dx++) {
                                    int dist2 = dx * dx + dz * dz + dy * dy;
                                    if(dist2 > 5)
                                        continue;

                                    setNode(x + dx, topY + dy, z + dz, kNodeLeaves, leafMeta, true);
                                }
                            }
                        }
                    }
                }

                if(regionBase.x == 0 && regionBase.z == 0) {
                    constexpr int kTestGlobalY = 64;
                    if(regionBase.y <= kTestGlobalY && (regionBase.y + 63) >= kTestGlobalY) {
                        int localY = kTestGlobalY - regionBase.y;
                        setNode(2, localY, 2, kNodeLava, 0, false);
                        setNode(4, localY, 2, kNodeWater, 0, false);
                        setNode(6, localY, 2, kNodeFire, 0, false);
                    }
                }
            } 
            // else {
            //     Node *ptr = (Node*) &out.Nodes[0][0];
            //     Node node;
            //     node.Data = 0;
            //     std::fill(ptr, ptr+64*64*64, node);
            // }

            RegionOut.lock()->push_back({key, out});
        }
    } catch(const std::exception& exc) {
        NeedShutdown = true;
        LOG.error() << "Ошибка выполнения потока " << id << ":\n" << exc.what();
    }
}

static thread_local std::vector<ContentViewCircle> TL_Circles;

std::vector<ContentViewCircle> GameServer::Expanse_t::accumulateContentViewCircles(ContentViewCircle circle, int depth)
{
    TL_Circles.clear();
    TL_Circles.reserve(64);
    TL_Circles.push_back(circle);
    _accumulateContentViewCircles(circle, depth);
    return TL_Circles;
}

void GameServer::Expanse_t::_accumulateContentViewCircles(ContentViewCircle circle, int depth) {
    for(const auto &pair : ContentBridges) {
        auto &br = pair.second;
        if(br.LeftWorld == circle.WorldId) {
            glm::i32vec3 vec = circle.Pos-br.LeftPos;
            ContentViewCircle circleNew = {br.RightWorld, br.RightPos, static_cast<int16_t>(circle.Range-int16_t(vec.x*vec.x+vec.y*vec.y+vec.z*vec.z))};

            if(circleNew.Range >= 0) {
                bool isIn = false;

                for(ContentViewCircle &exCircle : TL_Circles) {
                    if(exCircle.WorldId != circleNew.WorldId)
                        continue;

                    vec = exCircle.Pos-circleNew.Pos;
                    if(exCircle.Range >= vec.x*vec.x+vec.y*vec.y+vec.z*vec.z+circleNew.Range) {
                        isIn = true;
                        break;
                    }
                }

                if(isIn)
                    continue;

                TL_Circles.push_back(circleNew);
                if(depth > 1)
                    _accumulateContentViewCircles(circleNew, depth-1);
            }
        }

        if(br.IsTwoWay && br.RightWorld == circle.WorldId) {
            glm::i32vec3 vec = circle.Pos-br.RightPos;
            ContentViewCircle circleNew = {br.LeftWorld, br.LeftPos, static_cast<int16_t>(circle.Range-int16_t(vec.x*vec.x+vec.y*vec.y+vec.z*vec.z))};

            if(circleNew.Range >= 0) {
                bool isIn = false;

                for(ContentViewCircle &exCircle : TL_Circles) {
                    if(exCircle.WorldId != circleNew.WorldId)
                        continue;

                    vec = exCircle.Pos-circleNew.Pos;
                    if(exCircle.Range >= vec.x*vec.x+vec.y*vec.y+vec.z*vec.z+circleNew.Range) {
                        isIn = true;
                        break;
                    }
                }

                if(isIn)
                    continue;

                TL_Circles.push_back(circleNew);
                if(depth > 1)
                    _accumulateContentViewCircles(circleNew, depth-1);
            }
        }
    }
}

// std::unordered_map<WorldId_t, std::vector<ContentViewCircle>> GameServer::WorldObj::remapCVCsByWorld(const std::vector<ContentViewCircle> &list) {
//     std::unordered_map<WorldId_t, std::vector<ContentViewCircle>> out;

//     for(const ContentViewCircle &circle : list) {
//         out[circle.WorldId].push_back(circle);
//     }

//     return out;
// }


ContentViewInfo GameServer::Expanse_t::makeContentViewInfo(const std::vector<ContentViewCircle> &views) {
    ContentViewInfo cvi;

    for(const ContentViewCircle &circle : views) {
        std::vector<Pos::GlobalRegion> &cvw = cvi.Regions[circle.WorldId];
        int32_t regionRange = std::sqrt(circle.Range);

        cvw.reserve(cvw.size()+std::pow(regionRange*2+1, 3));

        for(int32_t z = -regionRange; z <= regionRange; z++)
            for(int32_t y = -regionRange; y <= regionRange; y++)
                for(int32_t x = -regionRange; x <= regionRange; x++)
                    cvw.push_back(Pos::GlobalRegion(x, y, z)+circle.Pos);
    }

    for(auto& [worldId, regions] : cvi.Regions) {
        std::sort(regions.begin(), regions.end());
        auto eraseIter = std::unique(regions.begin(), regions.end());
        regions.erase(eraseIter, regions.end());
        regions.shrink_to_fit();
    }

    return cvi;
}

coro<> GameServer::pushSocketConnect(tcp::socket socket) {
    auto useLock = UseLock.lock();

    try {
        std::string magic = "AlterLuanti";
        co_await Net::AsyncSocket::read(socket, (std::byte*) magic.data(), magic.size());
        
        if(magic != "AlterLuanti") {
            co_return;
        }

        uint8_t mver = co_await Net::AsyncSocket::read<uint8_t>(socket);
        
        if(mver != 0) {
            co_return;
        }

        uint8_t a_ar_r = co_await Net::AsyncSocket::read<uint8_t>(socket);
        
        std::string username = co_await Net::AsyncSocket::read<std::string>(socket);
        if(username.size() > 255)
            co_return;


        std::string token = co_await Net::AsyncSocket::read<std::string>(socket);
        if(token.size() > 255)
            co_return;

        uint8_t response_code;
        std::string response_message;

        if(a_ar_r < 0 || a_ar_r > 2)
            co_return;

        bool authorized = false;
        // Авторизация
        if (a_ar_r == 0 || a_ar_r == 1) {
            authorized = true;
            response_code = 0;
            // Авторизация
        }

        bool justRegistered = false;

        if (!authorized && (a_ar_r == 1 || a_ar_r == 2)) {
            // Регистрация

            response_code = 1;
        }

        co_await Net::AsyncSocket::write<uint8_t>(socket, response_code);
        
        if(response_code > 1) {
            co_await Net::AsyncSocket::write(socket, "Неизвестный протокол");
        } else
            co_await pushSocketAuthorized(std::move(socket), username);
        
    } catch (const std::exception& e) {
    }
}

coro<> GameServer::pushSocketAuthorized(tcp::socket socket, const std::string username) {
    auto useLock = UseLock.lock();
    uint8_t code = co_await Net::AsyncSocket::read<uint8_t>(socket);
        
    if(code == 0) {
        co_await pushSocketGameProtocol(std::move(socket), username);
    } else {
        co_await Net::AsyncSocket::write<uint8_t>(socket, 1);
        co_await Net::AsyncSocket::write(socket, "Неизвестный протокол");
    }
}

coro<> GameServer::pushSocketGameProtocol(tcp::socket socket, const std::string username) {
    auto useLock = UseLock.lock();
    // Проверить не подключен ли уже игрок
    std::string ep = socket.remote_endpoint().address().to_string() + ':' + std::to_string(socket.remote_endpoint().port());

    bool isConnected = External.ConnectedPlayersSet.lock_read()->contains(username);

    if(isConnected) {
        LOG.info() << "Игрок не смог подключится (уже в игре) " << username;
        co_await Net::AsyncSocket::write<uint8_t>(socket, 1);
        co_await Net::AsyncSocket::write(socket, "Вы уже подключены к игре");
    } else if(IsGoingShutdown) {
        LOG.info() << "Игрок не смог подключится (сервер завершает работу) " << username;
        co_await Net::AsyncSocket::write<uint8_t>(socket, 1);
        if(ShutdownReason.empty())
            co_await Net::AsyncSocket::write(socket, "Сервер завершает работу");
        else
            co_await Net::AsyncSocket::write(socket, "Сервер завершает работу, причина: "+ShutdownReason);
    } else {
        auto lock = External.ConnectedPlayersSet.lock_write();
        isConnected = lock->contains(username);

        if(isConnected) {
            lock.unlock();
            LOG.info() << "Игрок не смог подключится (уже в игре) " << username;
            co_await Net::AsyncSocket::write<uint8_t>(socket, 1);
            co_await Net::AsyncSocket::write(socket, "Вы уже подключены к игре");
        } else {
            LOG.info() << "Подключился к игре " << username;
            lock->insert(username);
            lock.unlock();

            co_await Net::AsyncSocket::write<uint8_t>(socket, 0);

            External.NewConnectedPlayers.lock_write()
               ->push_back(std::make_shared<RemoteClient>(IOC, std::move(socket), username, this));
        }
    }
}

int my_exception_handler(lua_State* lua, sol::optional<const std::exception&> maybe_exception, sol::string_view description) {
	std::cout << "An exception occurred in a function, here's what it says ";
	if (maybe_exception) {
		std::cout << "(straight from the exception): ";
		const std::exception& ex = *maybe_exception;
		std::cout << ex.what() << std::endl;
	}
	else {
		std::cout << "(from the description parameter): ";
		std::cout.write(description.data(), static_cast<std::streamsize>(description.size()));
		std::cout << std::endl;
	}

	return sol::stack::push(lua, description);
}

void GameServer::init(fs::path worldPath) {
    // world.json

    fs::create_directories(worldPath);
    fs::path worldJson = worldPath / "world.json";

    LOG.info() << "Обработка файла " << worldJson.string();

    js::object sbWorld, sbPlayer, sbAuth, sbModStorage;
    std::vector<ModRequest> modsToLoad;

    if(!fs::exists(worldJson)) {
        MAKE_ERROR("Файл отсутствует");
    } else {
        std::string data;
        try {
            std::ifstream fd(worldJson);
            fd.exceptions(std::ifstream::failbit | std::ifstream::badbit);
            
            fd.seekg(0, std::ios::end);
            std::streamsize size = fd.tellg();
            fd.seekg(0, std::ios::beg);

            if(size > 16*1024*1024)
                MAKE_ERROR("Превышен размер файла (16 мб)");
            
            data.resize(size);
            fd.read((char*) data.data(), size);
        } catch (const std::exception& exc) {
            MAKE_ERROR("Не удалось считать: " << exc.what());
        }

        // void checkJson()

        try {
            js::object obj = js::parse(data).as_object();

            {
                js::object sb = obj.at("save_backends").as_object();
                sbWorld = sb.at("world").as_object();
                sbPlayer = sb.at("player").as_object();
                sbAuth = sb.at("auth").as_object();
                sbModStorage = sb.at("mod_storage").as_object();
            }

            {
                js::array arr = obj.at("mods").as_array();
                for(const js::value& v : arr) {
                    ModRequest mi;

                    if(v.is_string()) {
                        mi.Id = v.as_string();
                        std::fill(mi.MinVersion.begin(), mi.MinVersion.end(), 0);
                        std::fill(mi.MaxVersion.begin(), mi.MaxVersion.end(), uint32_t(-1));
                    } else {
                        js::object value = v.as_object();
                        mi.Id = value.at("id").as_string();
                        if(value.contains("version")) {
                            js::array version = value.at("version").as_array();
                            for(int iter = 0; iter < 4; iter++)
                                mi.MaxVersion[iter] = version.at(iter).as_int64();
                            mi.MinVersion = mi.MaxVersion;
                        } else {
                            if(value.contains("max_version")) {
                                js::array version = value.at("max_version").as_array();
                                for(int iter = 0; iter < 4; iter++)
                                    mi.MaxVersion[iter] = version.at(iter).as_int64();
                            } else {
                                std::fill(mi.MaxVersion.begin(), mi.MaxVersion.end(), uint32_t(-1));
                            }

                            if(value.contains("min_version")) {
                                js::array version = value.at("min_version").as_array();
                                for(int iter = 0; iter < 4; iter++)
                                    mi.MinVersion[iter] = version.at(iter).as_int64();
                            } else {
                                std::fill(mi.MinVersion.begin(), mi.MinVersion.end(), uint32_t(0));
                            }
                        }
                    }

                    modsToLoad.push_back(mi);
                }
            }

        } catch(const std::exception& exc) {
            MAKE_ERROR("Ошибка структуры параметров: " << exc.what());
        }
    }


    SaveBackends::Filesystem fsbc;

    LOG.info() << "Запуск базы хранения миров";
    SaveBackend.World = fsbc.createWorld(sbWorld);
    LOG.info() << "Запуск базы хранения игроков";
    SaveBackend.Player = fsbc.createPlayer(sbPlayer);
    LOG.info() << "Запуск базы хранения аутентификаций";
    SaveBackend.Auth = fsbc.createAuth(sbAuth);
    LOG.info() << "Запуск базы хранения данных модов";
    SaveBackend.ModStorage = fsbc.createModStorage(sbModStorage);

    LOG.info() << "Инициализация модов";

    ModPreloadInfo mpi = preLoadMods({"mods"});
    for(const std::string& error : mpi.Errors) {
        LOG.warn() << error;
    }

    LOG.info() << "Выборка модов";

    std::variant<std::vector<ModInfo>, std::vector<std::string>> resolveDependsResult = resolveDepends(modsToLoad, mpi.Mods);
    if(resolveDependsResult.index() == 1) {
        for(const std::string& error : std::get<1>(resolveDependsResult))
            LOG.warn() << error;

        MAKE_ERROR("Не удалось удовлетворить зависимости модов");
    }

    LOG.info() << "Построение этапов загрузки модов";

    std::variant<ModLoadTree, std::vector<std::string>> buildLoadChainResult = buildLoadChain({}, {}, std::get<0>(resolveDependsResult));
    assert(buildLoadChainResult.index() == 0);

    ModLoadTree mlt = std::get<0>(buildLoadChainResult);
    assert(mlt.UnloadChain.empty());

    LOG.info() << "Загрузка инстансов модов";

    LoadedMods = mlt.LoadChain;

    LuaMainState.open_libraries();
	LuaMainState.set_exception_handler(&my_exception_handler);

    for(const ModInfo& info : mlt.LoadChain) {
        LOG.info() << info.Id;
        CurrentModId = info.Id;
        sol::load_result res = LuaMainState.load_file(info.Path / "init.lua");
        ModInstances.emplace_back(info.Id, res.call<sol::table>());
    }

    std::function<void(const std::string&)> pushEvent = [&](const std::string& function) {
        for(auto& [id, core] : ModInstances) {
            std::optional<sol::protected_function> func = core.get<std::optional<sol::protected_function>>(function);
            if(func) {
                sol::protected_function_result result;
                try {
                    result = func->operator()();
                } catch(const std::exception &exc) {
                    MAKE_ERROR("Ошибка инициализации мода " << id << ":\n" << exc.what());
                }

                if(!result.valid()) {
                    sol::error err = result;
                    MAKE_ERROR("Ошибка инициализации мода " << id << ":\n" << err.what());
                }
            }
        }
    };

    initLuaAssets();
    pushEvent("initAssets");
    for(ssize_t index = mlt.LoadChain.size()-1; index >= 0; index--) {
        AssetsInit.Assets.push_back(mlt.LoadChain[index].Path / "assets");
    }

    auto capru = Content.AM.checkAndPrepareResourcesUpdate(AssetsInit);
    Content.AM.applyResourcesUpdate(capru);

    LOG.info() << "Пре Инициализация";

    {
        sol::table t = LuaMainState.create_table();
        // Content.CM.registerBase(EnumDefContent::Node, "core", "none", t);
        Content.CM.registerBase(EnumDefContent::World, "test", "devel_world", t);
        Content.CM.registerBase(EnumDefContent::Entity, "core", "player", t);
        PlayerEntityDefId = Content.CM.getContentId(EnumDefContent::Entity, "core", "player");
    }

    initLuaPre();
    pushEvent("lowPreInit");

    // TODO: регистрация контента из mod/content/*

    pushEvent("preInit");
    pushEvent("highPreInit");

    Content.CM.buildEndProfiles();


    LOG.info() << "Инициализация";
    initLua();
    pushEvent("init");

    LOG.info() << "Пост Инициализация";
    initLuaPost();
    pushEvent("postInit");


    // Загрузить миры с существующими профилями
    LOG.info() << "Загрузка существующих миров...";

    Expanse.Worlds[0] = std::make_unique<World>(0);

    LOG.info() << "Оповещаем моды о завершении загрузки";
    pushEvent("serverReady");

    LOG.info() << "Загрузка существующих миров...";
    BackingChunkPressure.Threads.resize(4);
    BackingChunkPressure.Worlds = &Expanse.Worlds;
    for(size_t iter = 0; iter < BackingChunkPressure.Threads.size(); iter++) {
        BackingChunkPressure.Threads[iter] = std::thread(&BackingChunkPressure_t::run, &BackingChunkPressure, iter);
    }

    BackingNoiseGenerator.Threads.resize(4);
    for(size_t iter = 0; iter < BackingNoiseGenerator.Threads.size(); iter++) {
        BackingNoiseGenerator.Threads[iter] = std::thread(&BackingNoiseGenerator_t::run, &BackingNoiseGenerator, iter);
    }

    BackingAsyncLua.Threads.resize(1);
    for(size_t iter = 0; iter < BackingAsyncLua.Threads.size(); iter++) {
        BackingAsyncLua.Threads[iter] = std::thread(&BackingAsyncLua_t::run, &BackingAsyncLua, iter);
    }

    RunThread = std::thread(&GameServer::prerun, this);
}

void GameServer::prerun() {
    try {
        auto useLock = UseLock.lock();
        run();

    } catch(...) {
    }

    IsAlive = false;
}

void GameServer::run() {
    // {
    //     IWorldSaveBackend::TickSyncInfo_In in;
    //     for(int x = -1; x <= 1; x++)
    //         for(int y = -1; y <= 1; y++)
    //             for(int z = -1; z <= 1; z++)
    //                 in.Load[0].push_back(Pos::GlobalChunk(x, y, z));

    //     stepGeneratorAndLuaAsync(SaveBackend.World->tickSync(std::move(in)));
    // }

    while(true) {
        ((uint32_t&) Game.AfterStartTime) += (uint32_t) (CurrentTickDuration*256);

        std::chrono::steady_clock::time_point atTickStart = std::chrono::steady_clock::now();

        if(IsGoingShutdown) {
            // Отключить игроков
            for(std::shared_ptr<RemoteClient> &remoteClient : Game.RemoteClients) {
                remoteClient->shutdown(EnumDisconnect::ByInterface, ShutdownReason);
            }

            {
                // Отключить вновь подключившихся
                auto lock = External.NewConnectedPlayers.lock_write();

                for(std::shared_ptr<RemoteClient> &client : *lock) {
                    client->shutdown(EnumDisconnect::ByInterface, ShutdownReason);
                }

                bool hasNewConnected = !lock->empty();
                lock.unlock();

                // Если были ещё подключившиеся сделать паузу на 1 секунду
                if(hasNewConnected)
                    std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Конец
            break;
        }

        stepConnections();
        stepModInitializations();
        IWorldSaveBackend::TickSyncInfo_Out dat1 = stepDatabaseSync();
        stepGeneratorAndLuaAsync(std::move(dat1));
        stepPlayerProceed();
        stepWorldPhysic();
        stepGlobalStep();
        stepSyncContent();

        // Прочие моменты
        if(!IsGoingShutdown) {
            if(BackingChunkPressure.NeedShutdown
                || BackingNoiseGenerator.NeedShutdown)
            {
                LOG.error() << "Ошибка работы одного из модулей";
                IsGoingShutdown = true;
            }
        }

        // Сон или подгонка длительности такта при высоких нагрузках
        std::chrono::steady_clock::time_point atTickEnd = std::chrono::steady_clock::now();
        float currentWastedTime = double((atTickEnd-atTickStart).count() * std::chrono::steady_clock::duration::period::num) / std::chrono::steady_clock::duration::period::den;
        GlobalTickLagTime += CurrentTickDuration-currentWastedTime;

        if(GlobalTickLagTime > 0) {
            CurrentTickDuration -= PerTickAdjustment;
            if(CurrentTickDuration < PerTickDuration)
                CurrentTickDuration = PerTickDuration;

            std::this_thread::sleep_for(std::chrono::milliseconds(uint32_t(1000*GlobalTickLagTime)));
            GlobalTickLagTime = 0;
        } else {
            CurrentTickDuration += PerTickAdjustment;
        }
    }

    LOG.info() << "Сервер завершил работу";
}

void GameServer::initLuaAssets() {
    auto &lua = LuaMainState;
    std::optional<sol::table> core = lua.get<std::optional<sol::table>>("core");
    if(!core)
        core = lua.create_named_table("core");

    std::function<void(EnumAssets, const std::string&, const sol::table&)> reg
        = [this](EnumAssets type, const std::string& key, const sol::table& profile)
    {
        std::optional<std::vector<std::optional<std::string>>> result_o = TOS::Str::match(key, "^(?:([\\w\\d_]+):)?([\\w\\d_]+)$");

        if(!result_o) {
            MAKE_ERROR("Недействительный идентификатор: " << key);
        }
        
        auto &result = *result_o;
        if(result[1])
            AssetsInit.Custom[(int) type][*result[1]][*result[2]] = nullptr;
        else
            AssetsInit.Custom[(int) type][CurrentModId][*result[2]] = nullptr;
    };

    core->set_function("register_nodestate",    [&](const std::string& key, const sol::table& profile) { reg(EnumAssets::Nodestate, key, profile); });
    core->set_function("register_particle",     [&](const std::string& key, const sol::table& profile) { reg(EnumAssets::Particle, key, profile); });
    core->set_function("register_animation",    [&](const std::string& key, const sol::table& profile) { reg(EnumAssets::Animation, key, profile); });
    core->set_function("register_model",        [&](const std::string& key, const sol::table& profile) { reg(EnumAssets::Model, key, profile); });
    core->set_function("register_texture",      [&](const std::string& key, const sol::table& profile) { reg(EnumAssets::Texture, key, profile); });
    core->set_function("register_sound",        [&](const std::string& key, const sol::table& profile) { reg(EnumAssets::Sound, key, profile); });
    core->set_function("register_font",         [&](const std::string& key, const sol::table& profile) { reg(EnumAssets::Font, key, profile); });
}

void GameServer::initLuaPre() {
    auto &lua = LuaMainState;
    sol::table core = lua["core"];

    auto lambdaError = [](sol::this_state L) {
        luaL_error(L.lua_state(), "Данная функция может использоваться только в стадии [assetsInit]");
    };

    for(const char* name : {"register_nodestate", "register_particle", "register_animation", 
            "register_model", "register_texture", "register_sound", "register_font"})
        core.set_function(name, lambdaError);

    std::function<void(EnumDefContent, const std::string&, const sol::table&)> reg
        = [this](EnumDefContent type, const std::string& key, const sol::table& profile)
    {
        std::optional<std::vector<std::optional<std::string>>> result_o = TOS::Str::match(key, "^(?:([\\w\\d_]+):)?([\\w\\d_]+)$");

        if(!result_o) {
            MAKE_ERROR("Недействительный идентификатор: " << key);
        }
        
        auto &result = *result_o;
        if(result[1])
            Content.CM.registerBase(type, *result[1], *result[2], profile);
        else
            Content.CM.registerBase(type, CurrentModId, *result[2], profile);
    };

    core.set_function("register_voxel",    [reg](const std::string& key, const sol::table& profile) { reg(EnumDefContent::Voxel, key, profile); });
    core.set_function("register_node",     [reg](const std::string& key, const sol::table& profile) { reg(EnumDefContent::Node, key, profile); });
    core.set_function("register_world",    [reg](const std::string& key, const sol::table& profile) { reg(EnumDefContent::World, key, profile); });
    core.set_function("register_portal",   [reg](const std::string& key, const sol::table& profile) { reg(EnumDefContent::Portal, key, profile); });
    core.set_function("register_entity",   [reg](const std::string& key, const sol::table& profile) { reg(EnumDefContent::Entity, key, profile); });
    core.set_function("register_item",     [reg](const std::string& key, const sol::table& profile) { reg(EnumDefContent::Item, key, profile); });
}

void GameServer::initLua() {
    auto &lua = LuaMainState;

    sol::table core = lua["core"];

    auto lambdaError = [](sol::this_state L) {
        luaL_error(L.lua_state(), "Данная функция может использоваться только в стадии [preInit]");
    };

    for(const char* name : {"register_voxel", "register_node", "register_world", "register_portal", "register_entity", "register_item"})
        core.set_function(name, lambdaError);

    
}

void GameServer::initLuaPost() {
    
}

void GameServer::requestModsReload() {
    bool expected = false;
    if(ModsReloadRequested.compare_exchange_strong(expected, true)) {
        LOG.info() << "Запрошена перезагрузка модов";
    }
}

void GameServer::stepConnections() {
    std::vector<std::shared_ptr<RemoteClient>> newClients;
    // Подключить новых игроков
    if(!External.NewConnectedPlayers.no_lock_readable().empty()) {
        auto lock = External.NewConnectedPlayers.lock_write();

        for(std::shared_ptr<RemoteClient>& client : *lock) {
            co_spawn(client->run());
            Game.RemoteClients.push_back(client);
            newClients.push_back(client);
        }

        lock->clear();
    }

    if(!newClients.empty()) {
        std::array<std::vector<ResourceId>, static_cast<size_t>(EnumAssets::MAX_ENUM)> lost{};

        std::vector<Net::Packet> packets;
        packets.push_back(RemoteClient::makePacket_informateAssets_DK(Content.AM.idToDK()));
        packets.push_back(RemoteClient::makePacket_informateAssets_HH(Content.AM.collectHashBindings(), lost));

        for(const std::shared_ptr<RemoteClient>& client : newClients) {
            if(!packets.empty()) {
                auto copy = packets;
                client->pushPackets(&copy);
            }
        }
    }

    BackingChunkPressure.endCollectChanges();

    // Отключение игроков
    for(std::shared_ptr<RemoteClient>& cec : Game.RemoteClients) {
        // Убрать отключившихся
        if(!cec->isConnected()) {
            // Отписываем наблюдателя от миров
            for(auto wPair : cec->ContentViewState.Regions) {
                auto wIter = Expanse.Worlds.find(wPair.first);
                assert(wIter != Expanse.Worlds.end());

                wIter->second->onRemoteClient_RegionsLost(wPair.first, cec, wPair.second);
            }

            if(cec->PlayerEntity) {
                ServerEntityId_t entityId = *cec->PlayerEntity;
                auto [worldId, regionPos, entityIndex] = entityId;
                auto iterWorld = Expanse.Worlds.find(worldId);
                if(iterWorld != Expanse.Worlds.end()) {
                    auto iterRegion = iterWorld->second->Regions.find(regionPos);
                    if(iterRegion != iterWorld->second->Regions.end()) {
                        Region& region = *iterRegion->second;
                        if(entityIndex < region.Entityes.size())
                            region.Entityes[entityIndex].IsRemoved = true;

                        std::vector<ServerEntityId_t> removed = {entityId};
                        for(const std::shared_ptr<RemoteClient>& observer : region.RMs) {
                            observer->prepareEntitiesRemove(removed);
                        }
                    }
                }
                cec->clearPlayerEntity();
            }

            std::string username = cec->Username;
            External.ConnectedPlayersSet.lock_write()->erase(username);
            
            cec = nullptr;
        }
    }

    // Вычистить невалидные ссылки на игроков
    Game.RemoteClients.erase(std::remove_if(Game.RemoteClients.begin(), Game.RemoteClients.end(),
            [](const std::shared_ptr<RemoteClient>& ptr) { return !ptr; }), 
        Game.RemoteClients.end());
}

void GameServer::stepModInitializations() {
    if(ModsReloadRequested.exchange(false)) {
        reloadMods();
    }
    BackingChunkPressure.endWithResults();
}

void GameServer::reloadMods() {
    std::vector<Net::Packet> packetsToSend;

    LOG.info() << "Перезагрузка модов";
    {
        // TODO: перезагрузка модов

        Content.CM.buildEndProfiles();
    }

    LOG.info() << "Перезагрузка ассетов";
    {
        {
            AssetsManager::Out_checkAndPrepareResourcesUpdate capru = Content.AM.checkAndPrepareResourcesUpdate(AssetsInit);
            AssetsManager::Out_applyResourcesUpdate aru = Content.AM.applyResourcesUpdate(capru);

            if(!capru.ResourceUpdates.empty() || !capru.LostLinks.empty())
                packetsToSend.push_back(
                    RemoteClient::makePacket_informateAssets_HH(
                        aru.NewOrUpdates,
                        capru.LostLinks
                    )
                );
        }
    

        {
            std::array<
                std::vector<AssetsManager::BindDomainKeyInfo>, 
                static_cast<size_t>(EnumAssets::MAX_ENUM)
            > baked = Content.AM.bake();

            if(hasAnyBindings(baked)) {
                packetsToSend.push_back(RemoteClient::makePacket_informateAssets_DK(baked));
            }
        }
    }

    // Отправка пакетов
    for(std::shared_ptr<RemoteClient>& cec : Game.RemoteClients) {
        auto copy = packetsToSend;
        cec->pushPackets(&copy);
    }
}

IWorldSaveBackend::TickSyncInfo_Out GameServer::stepDatabaseSync() {
    IWorldSaveBackend::TickSyncInfo_In toDB;
    
    for(std::shared_ptr<RemoteClient>& remoteClient : Game.RemoteClients) {
        assert(remoteClient);
        // Пересчитать зоны наблюдения
        if(remoteClient->CrossedRegion) {
            remoteClient->CrossedRegion = false;
            
            // Пересчёт зон наблюдения
            std::vector<ContentViewCircle> newCVCs;

            {
                std::vector<std::tuple<WorldId_t, Pos::Object, uint8_t>> points = remoteClient->getViewPoints();
                for(auto& [wId, pos, radius] : points) {
                    assert(radius < 5);
                    ContentViewCircle cvc;
                    cvc.WorldId = wId;
                    cvc.Pos = Pos::Object_t::asRegionsPos(pos);
                    cvc.Range = radius*radius;

                    std::vector<ContentViewCircle> list = Expanse.accumulateContentViewCircles(cvc);
                    newCVCs.insert(newCVCs.end(), list.begin(), list.end());
                }
            }

            ContentViewInfo newCbg = Expanse_t::makeContentViewInfo(newCVCs);

            ContentViewInfo_Diff diff = newCbg.diffWith(remoteClient->ContentViewState);
            if(!diff.WorldsNew.empty()) {
                // Сообщить о новых мирах
                for(const WorldId_t id : diff.WorldsNew) {
                    auto iter = Expanse.Worlds.find(id);
                    assert(iter != Expanse.Worlds.end());

                    remoteClient->prepareWorldUpdate(id, iter->second.get());
                }
            }

            remoteClient->ContentViewState = newCbg;
            // Вычистка не наблюдаемых регионов
            for(const auto& [worldId, regions] : diff.RegionsLost)
                remoteClient->prepareRegionsRemove(worldId, regions);
            // и миров
            for(const WorldId_t worldId : diff.WorldsLost)
                remoteClient->prepareWorldRemove(worldId);

            // Подписываем игрока на наблюдение за регионами
            for(const auto& [worldId, regions] : diff.RegionsNew) {
                auto iterWorld = Expanse.Worlds.find(worldId);
                assert(iterWorld != Expanse.Worlds.end());

                std::vector<Pos::GlobalRegion> notLoaded = iterWorld->second->onRemoteClient_RegionsEnter(worldId, remoteClient, regions);
                if(!notLoaded.empty()) {
                    // Добавляем к списку на загрузку
                    std::vector<Pos::GlobalRegion> &tl = toDB.Load[worldId];
                    tl.insert(tl.end(), notLoaded.begin(), notLoaded.end());
                }
            }

            // Отписываем то, что игрок больше не наблюдает
            for(const auto& [worldId, regions] : diff.RegionsLost) {
                auto iterWorld = Expanse.Worlds.find(worldId);
                assert(iterWorld != Expanse.Worlds.end());

                iterWorld->second->onRemoteClient_RegionsLost(worldId, remoteClient, regions);
            }
        }
    }

    for(auto& [worldId, regions] : toDB.Load) {
        std::sort(regions.begin(), regions.end());
        auto eraseIter = std::unique(regions.begin(), regions.end());
        regions.erase(eraseIter, regions.end());
    }

    // Обзавелись списком на прогрузку регионов
    // Теперь узнаем что нужно сохранить и что из регионов было выгружено
    for(auto& [worldId, world] : Expanse.Worlds) {
        World::SaveUnloadInfo info = world->onStepDatabaseSync();
        
        if(!info.ToSave.empty()) {
            auto &obj = toDB.ToSave[worldId];
            obj.insert(obj.end(), std::make_move_iterator(info.ToSave.begin()), std::make_move_iterator(info.ToSave.end()));
        }

        if(!info.ToUnload.empty()) {
            auto &obj = toDB.Unload[worldId];
            obj.insert(obj.end(), info.ToUnload.begin(), info.ToUnload.end());
        }
    }

    // Синхронизируемся с базой
    return SaveBackend.World->tickSync(std::move(toDB));
}

void GameServer::stepGeneratorAndLuaAsync(IWorldSaveBackend::TickSyncInfo_Out db) {
    // 1. Получили сырые регионы и те регионы, что не существуют
    // 2.1 Те регионы, что не существуют отправляются на расчёт шума
    // 2.2 Далее в луа для обработки шума
    // 3.1 Нужно прогнать идентификаторы через обработчики lua
    // 3.2 Полученный регион связать с существующими профилями сервера
    // 4. Полученные регионы раздать мирам и попробовать по новой подписать к ним игроков, если они всё ещё должны наблюдать эти регионы


    // Синхронизация с генератором шума
    
    std::unordered_map<WorldId_t, std::vector<std::pair<Pos::GlobalRegion, World::RegionIn>>> toLoadRegions;

    // Синхронизация с контроллером асинхронных обработчиков луа
    // 2.2 и 3.1
    // Обработка шума на стороне луа
    {
        std::vector<std::pair<BackingNoiseGenerator_t::NoiseKey, std::array<float, 64*64*64>>> calculatedNoise = BackingNoiseGenerator.tickSync(std::move(db.NotExisten));
        if(!calculatedNoise.empty())
            BackingAsyncLua.NoiseIn.lock()->push_range(calculatedNoise);

        calculatedNoise.clear();

        if(!BackingAsyncLua.RegionOut.get_read().empty()) {
            std::vector<
                std::pair<BackingNoiseGenerator_t::NoiseKey, World::RegionIn>
            > toLoad = std::move(*BackingAsyncLua.RegionOut.lock());
            
            for(auto& [key, region] : toLoad) {
                toLoadRegions[key.WId].push_back({key.RegionPos, region});
            }
        }
    }

    // Обработка идентификаторов на стороне луа

    // Трансформация полученных ключей в профили сервера
    for(auto& [WorldId_t, regions] : db.LoadedRegions) {
        auto &list = toLoadRegions[WorldId_t];

        for(auto& [pos, region] : regions) {
            auto &obj = list.emplace_back(pos, World::RegionIn()).second;
            convertRegionVoxelsToChunks(region.Voxels, obj.Voxels);
            obj.Nodes = std::move(region.Nodes);
            obj.Entityes = std::move(region.Entityes);
        }
    }

    // Раздадим полученные регионы мирам и попробуем подписать на них наблюдателей
    for(auto& [worldId, regions] : toLoadRegions) {
        auto iterWorld = Expanse.Worlds.find(worldId);
        assert(iterWorld != Expanse.Worlds.end());

        std::vector<Pos::GlobalRegion> newRegions;
        newRegions.reserve(regions.size());
        for(auto& [pos, _] : regions)
            newRegions.push_back(pos);
        std::sort(newRegions.begin(), newRegions.end());

        std::unordered_map<std::shared_ptr<RemoteClient>, std::vector<Pos::GlobalRegion>> toSubscribe;
        
        for(auto& remoteClient : Game.RemoteClients) {
            auto iterViewWorld = remoteClient->ContentViewState.Regions.find(worldId);
            if(iterViewWorld == remoteClient->ContentViewState.Regions.end())
                continue;

            for(auto& pos : iterViewWorld->second) {
                if(std::binary_search(newRegions.begin(), newRegions.end(), pos))
                    toSubscribe[remoteClient].push_back(pos);
            }
        }

        iterWorld->second->pushRegions(std::move(regions));
        for(auto& [cec, poses] : toSubscribe) {
            iterWorld->second->onRemoteClient_RegionsEnter(worldId, cec, poses);
        }
    }
}

void GameServer::stepPlayerProceed() {
    auto iterWorld = Expanse.Worlds.find(0);
    if(iterWorld == Expanse.Worlds.end())
        return;

    World& world = *iterWorld->second;

    for(std::shared_ptr<RemoteClient>& remoteClient : Game.RemoteClients) {
        if(!remoteClient)
            continue;

        Pos::Object pos = remoteClient->CameraPos;
        Pos::GlobalRegion regionPos = Pos::Object_t::asRegionsPos(pos);
        glm::quat quat = remoteClient->CameraQuat.toQuat();

        if(!remoteClient->PlayerEntity) {
            auto iterRegion = world.Regions.find(regionPos);
            if(iterRegion == world.Regions.end())
                continue;

            Entity entity(PlayerEntityDefId);
            entity.WorldId = iterWorld->first;
            entity.Pos = pos;
            entity.Quat = quat;
            entity.InRegionPos = regionPos;

            Region& region = *iterRegion->second;
            RegionEntityId_t entityIndex = region.pushEntity(entity);
            if(entityIndex == RegionEntityId_t(-1))
                continue;

            ServerEntityId_t entityId = {iterWorld->first, regionPos, entityIndex};
            remoteClient->setPlayerEntity(entityId);

            std::vector<std::tuple<ServerEntityId_t, const Entity*>> updates;
            updates.emplace_back(entityId, &region.Entityes[entityIndex]);
            for(const std::shared_ptr<RemoteClient>& observer : region.RMs) {
                observer->prepareEntitiesUpdate(updates);
            }

            continue;
        }

        ServerEntityId_t entityId = *remoteClient->PlayerEntity;
        auto [worldId, prevRegion, entityIndex] = entityId;
        auto iterRegion = world.Regions.find(prevRegion);
        if(iterRegion == world.Regions.end()) {
            remoteClient->clearPlayerEntity();
            continue;
        }

        Region& region = *iterRegion->second;
        if(entityIndex >= region.Entityes.size() || region.Entityes[entityIndex].IsRemoved) {
            remoteClient->clearPlayerEntity();
            continue;
        }

        Entity& entity = region.Entityes[entityIndex];
        Pos::GlobalRegion nextRegion = Pos::Object_t::asRegionsPos(pos);
        if(nextRegion != prevRegion) {
            entity.IsRemoved = true;
            std::vector<ServerEntityId_t> removed = {entityId};
            for(const std::shared_ptr<RemoteClient>& observer : region.RMs) {
                observer->prepareEntitiesRemove(removed);
            }

            remoteClient->clearPlayerEntity();

            auto iterNewRegion = world.Regions.find(nextRegion);
            if(iterNewRegion == world.Regions.end())
                continue;

            Entity nextEntity(PlayerEntityDefId);
            nextEntity.WorldId = iterWorld->first;
            nextEntity.Pos = pos;
            nextEntity.Quat = quat;
            nextEntity.InRegionPos = nextRegion;

            Region& newRegion = *iterNewRegion->second;
            RegionEntityId_t nextIndex = newRegion.pushEntity(nextEntity);
            if(nextIndex == RegionEntityId_t(-1))
                continue;

            ServerEntityId_t nextId = {iterWorld->first, nextRegion, nextIndex};
            remoteClient->setPlayerEntity(nextId);

            std::vector<std::tuple<ServerEntityId_t, const Entity*>> updates;
            updates.emplace_back(nextId, &newRegion.Entityes[nextIndex]);
            for(const std::shared_ptr<RemoteClient>& observer : newRegion.RMs) {
                observer->prepareEntitiesUpdate(updates);
            }
            continue;
        }

        entity.Pos = pos;
        entity.Quat = quat;
        entity.WorldId = iterWorld->first;
        entity.InRegionPos = prevRegion;

        std::vector<std::tuple<ServerEntityId_t, const Entity*>> updates;
        updates.emplace_back(entityId, &entity);
        for(const std::shared_ptr<RemoteClient>& observer : region.RMs) {
            observer->prepareEntitiesUpdate(updates);
        }
    }
}

void GameServer::stepWorldPhysic() {
    // Максимальная скорость в обсчёте за такт половина максимального размера объекта
    // По всем объектам в регионе расчитывается максимальный размео по оси, делённый на линейную скорость
    // Выбирается наибольшая скорость. Если скорость превышает максимальную за раз, 
    // то физика в текущем такте рассчитывается в несколько проходов

    
    // for(auto &pWorld : Expanse.Worlds) {
    //     World &wobj = *pWorld.second;
       
    //     assert(pWorld.first == 0 && "Требуется WorldManager");

    //     std::string worldStringId = "unexisten";

    //     std::vector<Pos::GlobalRegion> regionsToRemove;
    //     for(auto &pRegion : wobj.Regions) {
    //         Region &region = *pRegion.second;

    //         // Позиции исчисляются в целых числах
    //         // Вместо умножения на dtime, используется *dTimeMul/dTimeDiv
    //         int32_t dTimeDiv = Pos::Object_t::BS;
    //         int32_t dTimeMul = dTimeDiv*CurrentTickDuration;

    //         // Обновить сущностей
    //         for(size_t entityIndex = 0; entityIndex < region.Entityes.size(); entityIndex++) {
    //             Entity &entity = region.Entityes[entityIndex];

    //             if(entity.IsRemoved)
    //                 continue;

    //             // Если нет ни скорости, ни ускорения, то пропускаем расчёт
    //             if((entity.Speed.x != 0 || entity.Speed.y != 0 || entity.Speed.z != 0)
    //                     || (entity.Acceleration.x != 0 || entity.Acceleration.y != 0 || entity.Acceleration.z != 0))
    //             {
    //                 Pos::Object &eSpeed = entity.Speed;

    //                 // Ограничение на 256 м/с
    //                 static constexpr int32_t MAX_SPEED_PER_SECOND = 256*Pos::Object_t::BS;
    //                 {
    //                     uint32_t linearSpeed = std::sqrt(eSpeed.x*eSpeed.x + eSpeed.y*eSpeed.y + eSpeed.z*eSpeed.z);

    //                     if(linearSpeed > MAX_SPEED_PER_SECOND) {
    //                         eSpeed *= MAX_SPEED_PER_SECOND;
    //                         eSpeed /= linearSpeed;
    //                     }

    //                     Pos::Object &eAcc = entity.Acceleration;
    //                     linearSpeed = std::sqrt(eAcc.x*eAcc.x + eAcc.y*eAcc.y + eAcc.z*eAcc.z);

    //                     if(linearSpeed > MAX_SPEED_PER_SECOND/2) {
    //                         eAcc *= MAX_SPEED_PER_SECOND/2;
    //                         eAcc /= linearSpeed;
    //                     }
    //                 }

    //                 // Потенциальное изменение позиции сущности в пустой области
    //                 // vt+(at^2)/2 = (v+at/2)*t = (Скорость + Ускорение/2*dtime)*dtime
    //                 Pos::Object dpos = (eSpeed + entity.Acceleration/2*dTimeMul/dTimeDiv)*dTimeMul/dTimeDiv;
    //                 // Стартовая и конечная позиции
    //                 Pos::Object &startPos = entity.Pos, endPos = entity.Pos + dpos;
    //                 // Новая скорость
    //                 Pos::Object nSpeed = entity.Speed + entity.Acceleration*dTimeMul/dTimeDiv;

    //                 // Зона расчёта коллизии
    //                 AABB collideZone = {startPos, endPos};
    //                 collideZone.sortMinMax();
    //                 collideZone.VecMin -= Pos::Object(entity.ABBOX.x, entity.ABBOX.y, entity.ABBOX.z)/2+Pos::Object(1);
    //                 collideZone.VecMax += Pos::Object(entity.ABBOX.x, entity.ABBOX.y, entity.ABBOX.z)/2+Pos::Object(1);

    //                 // Сбор ближайших коробок
    //                 std::vector<CollisionAABB> Boxes;

    //                 {
    //                     glm::ivec3 beg = collideZone.VecMin >> 20, end = (collideZone.VecMax + 0xfffff) >> 20;

    //                     for(; beg.z <= end.z; beg.z++)
    //                     for(; beg.y <= end.y; beg.y++)
    //                     for(; beg.x <= end.x; beg.x++) {
    //                         Pos::GlobalRegion rPos(beg.x, beg.y, beg.z);
    //                         auto iterChunk = wobj.Regions.find(rPos);
    //                         if(iterChunk == wobj.Regions.end())
    //                             continue;

    //                         iterChunk->second->getCollideBoxes(rPos, collideZone, Boxes);
    //                     }
    //                 }

    //                 // Коробка сущности
    //                 AABB entityAABB = entity.aabbAtPos();

    //                 // Симулируем физику
    //                 // Оставшееся время симуляции
    //                 int32_t remainingSimulationTime = dTimeMul;
    //                 // Оси, по которым было пересечение
    //                 bool axis[3]; // x y z

    //                 // Симулируем пока не будет просчитано выделенное время
    //                 while(remainingSimulationTime > 0) {
    //                     if(nSpeed.x == 0 && nSpeed.y == 0 && nSpeed.z == 0)
    //                         break; // Скорости больше нет

    //                     entityAABB = entity.aabbAtPos();

    //                     // Ближайшее время пересечения с боксом
    //                     int32_t minSimulationTime = remainingSimulationTime;
    //                     // Ближайший бокс в пересечении
    //                     int nearest_boxindex = -1;

    //                     for(size_t index = 0; index < Boxes.size(); index++) {
    //                         CollisionAABB &caabb = Boxes[index];

    //                         if(caabb.Skip)
    //                             continue;

    //                         int32_t delta;
    //                         if(!entityAABB.collideWithDelta(caabb, nSpeed, delta, axis))
    //                             continue;

    //                         if(delta > remainingSimulationTime)
    //                             continue;

    //                         nearest_boxindex = index;
    //                         minSimulationTime = delta;
    //                     }

    //                     if(nearest_boxindex == -1) {
    //                         // Свободный ход
    //                         startPos += nSpeed*dTimeDiv/minSimulationTime;
    //                         remainingSimulationTime = 0;
    //                         break;
    //                     } else {
    //                         if(minSimulationTime == 0) {
    //                             // Уже где-то застряли
    //                             // Да и хрен бы с этим
    //                         } else {
    //                             // Где-то встрянем через minSimulationTime
    //                             startPos += nSpeed*dTimeDiv/minSimulationTime;
    //                             remainingSimulationTime -= minSimulationTime;

    //                             nSpeed.x = nSpeed.y = nSpeed.z = 0;
    //                             break;
    //                         }

    //                         if(axis[0] == 0) {
    //                             nSpeed.x = 0;
    //                         }
                            
    //                         if(axis[1] == 0) {
    //                             nSpeed.y = 0;
    //                         } 
                            
    //                         if(axis[2] == 0) {
    //                             nSpeed.z = 0;
    //                         }

    //                         CollisionAABB &caabb = Boxes[nearest_boxindex];
    //                         caabb.Skip = true;
    //                     }
    //                 }

    //                 // Симуляция завершена
    //             }

    //             // Сущность будет вычищена
    //             if(entity.NeedRemove) {
    //                 entity.NeedRemove = false;
    //                 entity.IsRemoved = true;
    //             }

    //             // Проверим необходимость перемещения сущности в другой регион
    //             // Вынести в отдельный этап обновления сервера, иначе будут происходить двойные симуляции
    //             // сущностей при пересечении регионов/миров
    //             {
    //                 Pos::Object temp = entity.Pos >> 20;
    //                 Pos::GlobalRegion rPos(temp.x, temp.y, temp.z);

    //                 if(rPos != pRegion.first || pWorld.first != entity.WorldId) {

    //                     Region *toRegion = forceGetRegion(entity.WorldId, rPos);
    //                     RegionEntityId_t newId = toRegion->pushEntity(entity);
    //                     // toRegion->Entityes[newId].WorldId = Если мир изменился

    //                     if(newId == RegionEntityId_t(-1)) {
    //                         // В другом регионе нет места
    //                     } else {
    //                         entity.IsRemoved = true;
    //                         // Сообщаем о перемещении сущности
    //                         for(RemoteClient *cec : region.CECs) {
    //                             cec->onEntitySwap(pWorld.first, pRegion.first, entityIndex, entity.WorldId, rPos, newId);
    //                         }
    //                     }
    //                 }
    //             }
    //         }

    //         // Проверить необходимость перерасчёта вертикальной проходимости света 
    //         // std::unordered_map<Pos::bvec4u, const LightPrism*> ChangedLightPrism;
    //         // {
    //         //     for(int big = 0; big < 64; big++) {
    //         //         uint64_t bits = region.IsChunkChanged_Voxels[big] | region.IsChunkChanged_Nodes[big];

    //         //         if(!bits)
    //         //             continue;

    //         //         for(int little = 0; little < 64; little++) {
    //         //             if(((bits >> little) & 1) == 0)
    //         //                 continue;

                        
    //         //         }
    //         //     }
    //         // }

    //         // Сбор данных об изменившихся чанках
    //         std::unordered_map<Pos::bvec4u, const std::vector<VoxelCube>*> ChangedVoxels;
    //         std::unordered_map<Pos::bvec4u, const Node*> ChangedNodes;
    //         {
    //             if(!region.IsChunkChanged_Voxels && !region.IsChunkChanged_Nodes)
    //                 continue;

    //             for(int index = 0; index < 64; index++) {
    //                 Pos::bvec4u pos;
    //                 pos.unpack(index);

    //                 if(((region.IsChunkChanged_Voxels >> index) & 1) == 1) {
    //                     auto iter = region.Voxels.find(pos);
    //                     assert(iter != region.Voxels.end());
    //                     ChangedVoxels[pos] = &iter->second;
    //                 }

    //                 if(((region.IsChunkChanged_Nodes >> index) & 1) == 1) {
    //                     ChangedNodes[pos] = (Node*) &region.Nodes[0][0][0][pos.x][pos.y][pos.z];
    //                 }
    //             }
    //         }

    //         // Об изменившихся сущностях
    //         {

    //         }

    //         if(++region.CEC_NextChunkAndEntityesViewCheck >= region.CECs.size())
    //             region.CEC_NextChunkAndEntityesViewCheck = 0;

    //         // Пробегаемся по всем наблюдателям
    //         {
    //             size_t cecIndex = 0;
    //             for(RemoteClient *cec : region.CECs) {
    //                 cecIndex++;

    //                 auto cvwIter = cec->ContentViewState.find(pWorld.first);
    //                 if(cvwIter == cec->ContentViewState.end())
    //                     // Мир не отслеживается
    //                     continue;


    //                 const ContentViewWorld &cvw = cvwIter->second;
    //                 auto chunkBitsetIter = cvw.find(pRegion.first);
    //                 if(chunkBitsetIter == cvw.end())
    //                     // Регион не отслеживается
    //                     continue;

    //                 // Наблюдаемые чанки
    //                 const std::bitset<64> &chunkBitset = chunkBitsetIter->second;

    //                 // Пересылка изменений в регионе
    //                 // if(!ChangedLightPrism.empty())
    //                 //     cec->onChunksUpdate_LightPrism(pWorld.first, pRegion.first, ChangedLightPrism);

    //                 if(!ChangedVoxels.empty())
    //                     cec->onChunksUpdate_Voxels(pWorld.first, pRegion.first, ChangedVoxels);

    //                 if(!ChangedNodes.empty())
    //                     cec->onChunksUpdate_Nodes(pWorld.first, pRegion.first, ChangedNodes);

    //                 // Отправка полной информации о новых наблюдаемых чанках
    //                 {
    //                         //std::unordered_map<Pos::bvec4u, const LightPrism*> newLightPrism;
    //                         std::unordered_map<Pos::bvec4u, const std::vector<VoxelCube>*> newVoxels;
    //                         std::unordered_map<Pos::bvec4u, const Node*> newNodes;

    //                         //newLightPrism.reserve(new_chunkBitset->count());
    //                         newVoxels.reserve(new_chunkBitset->count());
    //                         newNodes.reserve(new_chunkBitset->count());

    //                         size_t bitPos = new_chunkBitset->_Find_first();
    //                         while(bitPos != new_chunkBitset->size()) {
    //                             Pos::bvec4u chunkPos;
    //                             chunkPos = bitPos;

    //                             //newLightPrism.insert({chunkPos, &region.Lights[0][0][chunkPos.X][chunkPos.Y][chunkPos.Z]});
    //                             newVoxels.insert({chunkPos, &region.Voxels[chunkPos]});
    //                             newNodes.insert({chunkPos, &region.Nodes[0][0][0][chunkPos.x][chunkPos.y][chunkPos.z]});

    //                             bitPos = new_chunkBitset->_Find_next(bitPos);
    //                         }

    //                         //cec->onChunksUpdate_LightPrism(pWorld.first, pRegion.first, newLightPrism);
    //                         cec->onChunksUpdate_Voxels(pWorld.first, pRegion.first, newVoxels);
    //                         cec->onChunksUpdate_Nodes(pWorld.first, pRegion.first, newNodes);
                        
    //                 }

    //                 // То, что уже отслеживает наблюдатель
    //                 const auto &subs = cec->getSubscribed();

    //                 // // Проверка отслеживания сущностей
    //                 // if(cecIndex-1 == region.CEC_NextChunkAndEntityesViewCheck) {
    //                 //     std::vector<LocalEntityId_t> newEntityes, lostEntityes;
    //                 //     for(size_t iter = 0; iter < region.Entityes.size(); iter++) {
    //                 //         Entity &entity = region.Entityes[iter];

    //                 //         if(entity.IsRemoved)
    //                 //             continue;

    //                 //         for(const ContentViewCircle &circle : cvc->second) {
    //                 //             int x = entity.ABBOX.x >> 17;
    //                 //             int y = entity.ABBOX.y >> 17;
    //                 //             int z = entity.ABBOX.z >> 17;

    //                 //             uint32_t size = 0;
    //                 //             if(circle.isIn(entity.Pos, x*x+y*y+z*z))
    //                 //                 newEntityes.push_back(iter);
    //                 //         }
    //                 //     }

    //                 //     std::unordered_set<LocalEntityId_t> newEntityesSet(newEntityes.begin(), newEntityes.end());

    //                 //     {
    //                 //         auto iterR_W = subs.Entities.find(pWorld.first);
    //                 //         if(iterR_W == subs.Entities.end())
    //                 //             // Если мир не отслеживается наблюдателем
    //                 //             goto doesNotObserveEntityes;

    //                 //         auto iterR_W_R = iterR_W->second.find(pRegion.first);
    //                 //         if(iterR_W_R == iterR_W->second.end())
    //                 //             // Если регион не отслеживается наблюдателем
    //                 //             goto doesNotObserveEntityes;

    //                 //         // Подходят ли уже наблюдаемые сущности под наблюдательные области
    //                 //         for(LocalEntityId_t eId : iterR_W_R->second) {
    //                 //             if(eId >= region.Entityes.size()) {
    //                 //                 lostEntityes.push_back(eId);
    //                 //                 break;
    //                 //             }

    //                 //             Entity &entity = region.Entityes[eId];

    //                 //             if(entity.IsRemoved) {
    //                 //                 lostEntityes.push_back(eId);
    //                 //                 break;
    //                 //             }

    //                 //             int x = entity.ABBOX.x >> 17;
    //                 //             int y = entity.ABBOX.y >> 17;
    //                 //             int z = entity.ABBOX.z >> 17;

    //                 //             for(const ContentViewCircle &circle : cvc->second) {
    //                 //                 if(!circle.isIn(entity.Pos, x*x+y*y+z*z))
    //                 //                     lostEntityes.push_back(eId);
    //                 //             }
    //                 //         }

    //                 //         // Удалим чанки которые наблюдатель уже видит
    //                 //         for(LocalEntityId_t eId : iterR_W_R->second)
    //                 //             newEntityesSet.erase(eId);
    //                 //     }

    //                 //     doesNotObserveEntityes:

    //                 //     cec->onEntityEnterLost(pWorld.first, pRegion.first, newEntityesSet, std::unordered_set<LocalEntityId_t>(lostEntityes.begin(), lostEntityes.end()));
    //                 //     // Отправить полную информацию о новых наблюдаемых сущностях наблюдателю
    //                 // }

    //                 if(!region.Entityes.empty()) {
    //                     std::unordered_map<RegionEntityId_t, Entity*> entities;
    //                     for(size_t iter = 0; iter < region.Entityes.size(); iter++)
    //                         entities[iter] = &region.Entityes[iter];
    //                     cec->onEntityUpdates(pWorld.first, pRegion.first, entities);
    //                 }
    //             }
    //         }


    //         // Сохраняем регионы
    //         region.LastSaveTime += CurrentTickDuration;

    //         bool needToUnload = region.CECs.empty() && region.LastSaveTime > 60;
    //         bool needToSave = region.IsChanged && region.LastSaveTime > 15;

    //         if(needToUnload || needToSave) {
    //             region.LastSaveTime = 0;
    //             region.IsChanged = false;

    //             SB_Region data;
    //             convertChunkVoxelsToRegion(region.Voxels, data.Voxels);
    //             SaveBackend.World->save(worldStringId, pRegion.first, &data);
    //         }

    //         // Выгрузим регионы
    //         if(needToUnload) {
    //             regionsToRemove.push_back(pRegion.first);
    //         }

    //         // Сброс информации об изменившихся данных
    //         region.IsChunkChanged_Voxels = 0;
    //         region.IsChunkChanged_Nodes = 0;
    //     }

    //     for(Pos::GlobalRegion regionPos : regionsToRemove) {
    //         auto iter = wobj.Regions.find(regionPos);
    //         if(iter == wobj.Regions.end())
    //             continue;

    //         wobj.Regions.erase(iter);
    //     }

    //     // Загрузить регионы
    //     if(wobj.NeedToLoad.empty())
    //         continue;
 
    //     for(Pos::GlobalRegion &regionPos : wobj.NeedToLoad) {
    //         if(!SaveBackend.World->isExist(worldStringId, regionPos)) {
    //             wobj.Regions[regionPos]->IsLoaded = true;
    //         } else {
    //             SB_Region data;
    //             SaveBackend.World->load(worldStringId, regionPos, &data);
    //             Region &robj = *wobj.Regions[regionPos];

    //             // TODO: Передефайнить идентификаторы нод

    //             convertRegionVoxelsToChunks(data.Voxels, robj.Voxels);
    //         }
    //     }

    //     wobj.NeedToLoad.clear();
    // }
    
    // // Проверить отслеживание порталов
}

void GameServer::stepGlobalStep() {
    for(auto &pair : Expanse.Worlds)
        pair.second->onUpdate(this, CurrentTickDuration);
}

void GameServer::stepSyncContent() {
    for(std::shared_ptr<RemoteClient>& remoteClient : Game.RemoteClients) {
        remoteClient->onUpdate();

        // Это для пробы строительства и ломания нод
        while(!remoteClient->Build.empty()) {
            Pos::GlobalNode node = remoteClient->Build.front();
            remoteClient->Build.pop();

            Pos::GlobalRegion rPos = node >> 6;
            Pos::bvec4u cPos = (node >> 4) & 0x3;
            Pos::bvec16u nPos = node & 0xf;

            auto region = Expanse.Worlds[0]->Regions.find(rPos);
            if(region != Expanse.Worlds[0]->Regions.end()) {
                Node& n = region->second->Nodes[cPos.pack()][nPos.pack()];
                n.NodeId = 4;
                n.Meta = uint8_t((int(nPos.x) + int(nPos.y) + int(nPos.z)) & 0x3);
                region->second->IsChunkChanged_Nodes |= 1ull << cPos.pack();
            }
        }

        while(!remoteClient->Break.empty()) {
            Pos::GlobalNode node = remoteClient->Break.front();
            remoteClient->Break.pop();

            Pos::GlobalRegion rPos = node >> 6;
            Pos::bvec4u cPos = (node >> 4) & 0x3;
            Pos::bvec16u nPos = node & 0xf;

            auto region = Expanse.Worlds[0]->Regions.find(rPos);
            if(region != Expanse.Worlds[0]->Regions.end()) {
                Node& n = region->second->Nodes[cPos.pack()][nPos.pack()];
                n.NodeId = 0;
                n.Meta = 0;
                region->second->IsChunkChanged_Nodes |= 1ull << cPos.pack();
            }
        }
    }


    // Сбор запросов на ресурсы + отправка пакетов игрокам
    ResourceRequest full = std::move(Content.OnContentChanges);
    for(std::shared_ptr<RemoteClient>& cec : Game.RemoteClients) {
        full.merge(cec->pushPreparedPackets());
    }

    full.uniq();

    std::vector<Net::Packet> packetsToAll;
    {
        std::array<
            std::vector<AssetsManager::BindDomainKeyInfo>, 
            static_cast<size_t>(EnumAssets::MAX_ENUM)
        > baked = Content.AM.bake();
        
        if(hasAnyBindings(baked)) {
            packetsToAll.push_back(RemoteClient::makePacket_informateAssets_DK(baked));
        }
    }

    // Оповещаем о двоичных ресурсах по запросу
    auto assetTypeName = [](EnumAssets type) {
        switch(type) {
        case EnumAssets::Nodestate: return "nodestate";
        case EnumAssets::Model: return "model";
        case EnumAssets::Texture: return "texture";
        case EnumAssets::Particle: return "particle";
        case EnumAssets::Animation: return "animation";
        case EnumAssets::Sound: return "sound";
        case EnumAssets::Font: return "font";
        default: return "unknown";
        }
    };

    std::vector<std::tuple<ResourceFile::Hash_t, std::shared_ptr<const std::u8string>>> binaryResources
        = Content.AM.getResources(full.Hashes);

    for(std::shared_ptr<RemoteClient>& remoteClient : Game.RemoteClients) {
        if(!binaryResources.empty())
            remoteClient->informateBinaryAssets(binaryResources);

        if(!packetsToAll.empty()) {
            auto copy = packetsToAll;
            remoteClient->pushPackets(&copy);
        }
    }


    BackingChunkPressure.startCollectChanges();
}


}
