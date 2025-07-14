#pragma once

#include "Common/Abstract.hpp"
#include "Common/Lockable.hpp"
#include "Server/RemoteClient.hpp"
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <filesystem>
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
    тогда обычным оповещениям клиентам дойдёт новая версия

    Подержать какое-то время ресурс в памяти

*/

class BinaryResourceManager : public AsyncObject {
public:

private:
    struct Resource {
        // Файл загруженный на диск
        std::shared_ptr<ResourceFile> Loaded;
        // Источник
        std::string Uri;
        bool IsLoading = false;

        std::string LastError;
    };

    struct UriParse {
        std::string Orig, Protocol, Path;
    };

    // Последовательная регистрация ресурсов
    BinTextureId_t NextIdTexture = 0, NextIdAnimation = 0, NextIdModel = 0,
        NextIdSound = 0, NextIdFont = 0;

    // Ресурсы - кешированные в оперативную память или в процессе загрузки
    std::map<BinTextureId_t, std::shared_ptr<Resource>>
    
    // Известные ресурсы
    std::map<std::string, ResourceId_t> KnownResource;
    std::map<ResourceId_t, std::shared_ptr<Resource>> ResourcesInfo;
    // Сюда 
    TOS::SpinlockObject<std::vector<ResourceId_t>> UpdatedResources;
    // Подготовленая таблица оповещения об изменениях ресурсов
    // Должна забираться сервером и отчищаться
    std::unordered_map<ResourceId_t, std::shared_ptr<ResourceFile>> PreparedInformation;

public:
    // Если ресурс будет обновлён или загружен будет вызвано onResourceUpdate
    BinaryResourceManager(asio::io_context &ioc);
    virtual ~BinaryResourceManager();

    // Перепроверка изменений ресурсов
    void recheckResources(std::vector<fs::path> assets /* Пути до активных папок assets */);
    // Выдаёт или назначает идентификатор для ресурса
    BinTextureId_t      getTexture  (const std::string& uri);
    BinAnimationId_t    getAnimation(const std::string& uri);
    BinModelId_t        getModel    (const std::string& uri);
    BinSoundId_t        getSound    (const std::string& uri);
    BinFontId_t         getFont     (const std::string& uri);

    // Запросить ресурсы через onResourceUpdate
    void needResourceResponse(const ResourceRequest &&resources);
    // Получение обновлений или оповещений ресурсов
    std::unordered_map<ResourceId_t, std::shared_ptr<ResourceFile>> takePreparedInformation() {
        return std::move(PreparedInformation);
    }

    // Серверный такт
    void update(float dtime);

protected:
    UriParse parseUri(const std::string &uri);
    ResourceId_t getResource_Assets(std::string path);
    coro<> checkResource_Assets(ResourceId_t id, fs::path path, std::shared_ptr<Resource> res);

};


}