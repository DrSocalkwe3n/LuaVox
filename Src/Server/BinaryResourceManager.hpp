#pragma once

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


namespace AL::Server {

namespace fs = std::filesystem;

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
    
    // Нулевой ресурс
    std::shared_ptr<ResourceFile> ZeroResource;
    // Домены поиска ресурсов
    std::unordered_map<std::string, fs::path> Domains;
    // Известные ресурсы
    std::map<std::string, ResourceId_t> KnownResource;
    std::map<ResourceId_t, std::shared_ptr<Resource>> ResourcesInfo;
    // Последовательная регистрация ресурсов
    ResourceId_t NextId = 1;
    // Накапливаем идентификаторы готовых ресурсов
    Lockable<std::vector<ResourceId_t>> UpdatedResources;
    // Подготовленая таблица оповещения об изменениях ресурсов
    // Должна забираться сервером и отчищаться
    std::unordered_map<ResourceId_t, std::shared_ptr<ResourceFile>> PreparedInformation;

public:
    // Если ресурс будет обновлён или загружен будет вызвано onResourceUpdate
    BinaryResourceManager(asio::io_context &ioc, std::shared_ptr<ResourceFile> zeroResource);
    virtual ~BinaryResourceManager();

    // Перепроверка изменений ресурсов
    void recheckResources();
    // Домен мода -> путь к папке с ресурсами
    void setAssetsDomain(std::unordered_map<std::string, fs::path> &&domains) { Domains = std::move(domains); }
    // Идентификатор ресурса по его uri
    ResourceId_t mapUriToId(const std::string &uri);
    // Запросить ресурсы через onResourceUpdate
    void needResourceResponse(const std::vector<ResourceId_t> &resources);
    // Серверный такт
    void update(float dtime);
    bool hasPreparedInformation() { return !PreparedInformation.empty(); }
    
    std::unordered_map<ResourceId_t, std::shared_ptr<ResourceFile>> takePreparedInformation() {
        return std::move(PreparedInformation);
    }

protected:
    UriParse parseUri(const std::string &uri);
    ResourceId_t getResource_Assets(std::string path);
    coro<> checkResource_Assets(ResourceId_t id, fs::path path, std::shared_ptr<Resource> res);

};


}