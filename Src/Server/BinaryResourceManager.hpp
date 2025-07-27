#pragma once

#include "Common/Abstract.hpp"
#include "Common/Lockable.hpp"
#include "Server/RemoteClient.hpp"
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <variant>
#include <vector>
#include <Common/Async.hpp>
#include "Abstract.hpp"
#include "TOSLib.hpp"


namespace LV::Server {

namespace fs = std::filesystem;

/*
    Может прийти множество запросов на один не загруженный ресурс

    Чтение происходит отдельным потоком, переконвертацию пока предлагаю в realtime.
    Хэш вычисляется после чтения и может быть иным чем при прошлом чтении (ресурс изменили наживую)
    тогда обычным оповещением клиентам дойдёт новая версия

    Подержать какое-то время ресурс в памяти



    Ключи сопоставляются с идентификаторами и с хешеми. При появлении нового ключа, 
    ему выдаётся идентификатор и делается запрос на загрузку ресурса для вычисления хеша.
    recheckResources делает повторную загрузку всех ресурсов для проверки изменения хешей.


*/

class BinaryResourceManager : public AsyncObject {
private:

// Поток сервера
    // Последовательная регистрация ресурсов
    ResourceId_t NextId[(int) EnumBinResource::MAX_ENUM] = {0};
    // Известные ресурсы, им присвоен идентификатор
    // Нужно для потока загрузки
    std::unordered_map<std::string, ResourceId_t> KnownResource[(int) EnumBinResource::MAX_ENUM];

// Местные потоки
    struct LocalObj_t {
        // Трансляция идентификаторов в Uri (противоположность KnownResource)
        // Передаётся в отдельный поток
        std::vector<std::string>      ResIdToUri[(int) EnumBinResource::MAX_ENUM];
        // Кому-то нужно сопоставить идентификаторы с хешами
        std::vector<ResourceId_t>     BinToHash[(int) EnumBinResource::MAX_ENUM];
        // Запрос ресурсов, по которым потоки загружают ресурсы с диска
        std::vector<Hash_t>           Hashes;
        // Передача новых путей поиска ресурсов в другой поток
        std::vector<fs::path>         Assets;
    };

    TOS::SpinlockObject<LocalObj_t> Local;
public:
    // Подготовленные оповещения о ресурсах
    struct OutObj_t {
        std::unordered_map<ResourceId_t, ResourceFile::Hash_t>      BinToHash[(int) EnumBinResource::MAX_ENUM];
        std::vector<std::shared_ptr<ResourceFile>>                  HashToResource;
    };

private:
    TOS::SpinlockObject<OutObj_t> Out;
    volatile bool NeedShutdown = false;
    std::thread Thread;

    void run();
    std::variant<std::shared_ptr<ResourceFile>, std::string>
        loadFile(const std::vector<fs::path>& assets, const std::string& path, EnumBinResource type);

public:
    // Если ресурс будет обновлён или загружен будет вызвано onResourceUpdate
    BinaryResourceManager(asio::io_context &ioc);
    virtual ~BinaryResourceManager();

    // Перепроверка изменений ресурсов
    void recheckResources(std::vector<fs::path> assets /* Пути до активных папок assets */);
    // Выдаёт или назначает идентификатор для ресурса
    ResourceId_t getResource(const std::string& uri, EnumBinResource bin) {
        std::string fullUri;

        {
            size_t pos = uri.find("://");

            if(pos == std::string::npos)
                fullUri = "assets://" + uri;
            else
                fullUri = uri;
        }

        int index = (int) bin;

        auto &container = KnownResource[index];
        auto iter = container.find(fullUri);
        if(iter == container.end()) {
            assert(NextId[index] != ResourceId_t(-1));
            ResourceId_t nextId = NextId[index]++;
            container.insert(iter, {fullUri, nextId});

            auto lock = Local.lock();
            lock->ResIdToUri[index].push_back(uri);

            return nextId;
        }

        return iter->second;
    }

    BinTextureId_t getTexture(const std::string& uri) {
        return getResource(uri, EnumBinResource::Texture);
    }

    BinAnimationId_t getAnimation(const std::string& uri) {
        return getResource(uri, EnumBinResource::Animation);
    }

    BinModelId_t getModel(const std::string& uri) {
        return getResource(uri, EnumBinResource::Model);
    }

    BinSoundId_t getSound(const std::string& uri) {
        return getResource(uri, EnumBinResource::Sound);
    }

    BinFontId_t getFont(const std::string& uri) {
        return getResource(uri, EnumBinResource::Font);
    }

    // Запросить ресурсы через pushPreparedPackets
    void needResourceResponse(const ResourceRequest& resources);

    // Получение обновлений или оповещений ресурсов
    OutObj_t takePreparedInformation() {
        return std::move(*Out.lock());
    }

    // Серверный такт
    void update(float dtime);

};


}