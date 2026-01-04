#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include "Client/AssetsCacheManager.hpp"
#include "Client/AssetsHeaderCodec.hpp"
#include "Common/Abstract.hpp"
#include "TOSLib.hpp"

namespace LV::Client {

namespace fs = std::filesystem;

class AssetsManager {
public:
    using Ptr = std::shared_ptr<AssetsManager>;
    using AssetType = EnumAssets;
    using AssetId = ResourceId;

    // Ключ запроса ресурса (идентификация + хеш для поиска источника).
    struct ResourceKey {
        // Хеш ресурса, используемый для поиска в источниках и кэше.
        Hash_t Hash{};
        // Тип ресурса (модель, текстура и т.д.).
        AssetType Type{};
        // Домен ресурса.
        std::string Domain;
        // Ключ ресурса внутри домена.
        std::string Key;
        // Идентификатор ресурса на стороне клиента/локальный.
        AssetId Id = 0;
    };

    // Информация о биндинге серверного ресурса на локальный id.
    struct BindInfo {
        // Тип ресурса.
        AssetType Type{};
        // Локальный идентификатор.
        AssetId LocalId = 0;
        // Домен ресурса.
        std::string Domain;
        // Ключ ресурса.
        std::string Key;
        // Хеш ресурса.
        Hash_t Hash{};
        // Бинарный заголовок с зависимостями.
        std::vector<uint8_t> Header;
    };

    // Результат биндинга ресурса сервера.
    struct BindResult {
        // Итоговый локальный идентификатор.
        AssetId LocalId = 0;
        // Признак изменения бинда (хеш/заголовок).
        bool Changed = false;
        // Признак новой привязки.
        bool NewBinding = false;
        // Идентификатор, от которого произошёл ребинд (если был).
        std::optional<AssetId> ReboundFrom;
    };

    // Регистрация набора ресурспаков.
    struct PackRegister {
        // Пути до паков (директории/архивы).
        std::vector<fs::path> Packs;
    };

    // Ресурс, собранный из пака.
    struct PackResource {
        // Тип ресурса.
        AssetType Type{};
        // Локальный идентификатор.
        AssetId LocalId = 0;
        // Домен ресурса.
        std::string Domain;
        // Ключ ресурса.
        std::string Key;
        // Тело ресурса.
        Resource Res;
        // Хеш ресурса.
        Hash_t Hash{};
        // Заголовок ресурса (например, зависимости).
        std::u8string Header;
    };

    // Результат пересканирования паков.
    struct PackReloadResult {
        // Добавленные/изменённые ресурсы по типам.
        std::array<std::vector<AssetId>, static_cast<size_t>(AssetType::MAX_ENUM)> ChangeOrAdd;
        // Потерянные ресурсы по типам.
        std::array<std::vector<AssetId>, static_cast<size_t>(AssetType::MAX_ENUM)> Lost;
    };

    using ParsedHeader = AssetsHeaderCodec::ParsedHeader;

    // Фабрика с настройкой лимитов кэша.
    static Ptr Create(asio::io_context& ioc, const fs::path& cachePath,
        size_t maxCacheDirectorySize = 8 * 1024 * 1024 * 1024ULL,
        size_t maxLifeTime = 7 * 24 * 60 * 60) {
        return Ptr(new AssetsManager(ioc, cachePath, maxCacheDirectorySize, maxLifeTime));
    }

    // Пересканировать ресурспаки и вернуть изменившиеся/утраченные ресурсы.
    PackReloadResult reloadPacks(const PackRegister& reg);

    // Связать серверный ресурс с локальным id и записать метаданные.
    BindResult bindServerResource(AssetType type, AssetId serverId, std::string domain, std::string key,
        const Hash_t& hash, std::vector<uint8_t> header);
    // Отвязать серверный id и вернуть актуальный локальный id (если был).
    std::optional<AssetId> unbindServerResource(AssetType type, AssetId serverId);
    // Сбросить все серверные бинды.
    void clearServerBindings();

    // Получить данные бинда по локальному id.
    const BindInfo* getBind(AssetType type, AssetId localId) const;

    // Перебиндить хедер, заменив id зависимостей.
    std::vector<uint8_t> rebindHeader(AssetType type, const std::vector<uint8_t>& header, bool serverIds = true);
    // Распарсить хедер ресурса.
    static std::optional<ParsedHeader> parseHeader(AssetType type, const std::vector<uint8_t>& header);

    // Протолкнуть новые ресурсы в память и кэш.
    void pushResources(std::vector<Resource> resources);

    // Поставить запросы чтения ресурсов.
    void pushReads(std::vector<ResourceKey> reads);
    // Получить готовые результаты чтения.
    std::vector<std::pair<ResourceKey, std::optional<Resource>>> pullReads();
    // Продвинуть асинхронные источники (кэш).
    void tickSources();

    // Получить или создать локальный id по домену/ключу.
    AssetId getOrCreateLocalId(AssetType type, std::string_view domain, std::string_view key);
    // Получить локальный id по серверному id (если есть).
    std::optional<AssetId> getLocalIdFromServer(AssetType type, AssetId serverId) const;

private:
    // Связка домен/ключ для локального id.
    struct DomainKey {
        // Домен ресурса.
        std::string Domain;
        // Ключ ресурса.
        std::string Key;
        // Признак валидности записи.
        bool Known = false;
    };

    using IdTable = std::unordered_map<
        std::string,
        std::unordered_map<std::string, AssetId, detail::TSVHash, detail::TSVEq>,
        detail::TSVHash,
        detail::TSVEq>;

    using PackTable = std::unordered_map<
        std::string,
        std::unordered_map<std::string, PackResource, detail::TSVHash, detail::TSVEq>,
        detail::TSVHash,
        detail::TSVEq>;

    struct PerType {
        // Таблица домен/ключ -> локальный id.
        IdTable DKToLocal;
        // Таблица локальный id -> домен/ключ.
        std::vector<DomainKey> LocalToDK;
        // Union-Find родительские ссылки для ребиндов.
        std::vector<AssetId> LocalParent;
        // Таблица серверный id -> локальный id.
        std::vector<AssetId> ServerToLocal;
        // Бинды с сервером по локальному id.
        std::vector<std::optional<BindInfo>> BindInfos;
        // Ресурсы, собранные из паков.
        PackTable PackResources;
        // Следующий локальный id.
        AssetId NextLocalId = 1;
    };

    enum class SourceStatus {
        Hit,
        Miss,
        Pending
    };

    struct SourceResult {
        // Статус ответа источника.
        SourceStatus Status = SourceStatus::Miss;
        // Значение ресурса, если найден.
        std::optional<Resource> Value;
        // Индекс источника.
        size_t SourceIndex = 0;
    };

    struct SourceReady {
        // Хеш готового ресурса.
        Hash_t Hash{};
        // Значение ресурса, если найден.
        std::optional<Resource> Value;
        // Индекс источника.
        size_t SourceIndex = 0;
    };

    class IResourceSource {
    public:
        virtual ~IResourceSource() = default;
        // Попытка получить ресурс синхронно.
        virtual SourceResult tryGet(const ResourceKey& key) = 0;
        // Забрать готовые результаты асинхронных запросов.
        virtual void collectReady(std::vector<SourceReady>& out) = 0;
        // Признак асинхронности источника.
        virtual bool isAsync() const = 0;
        // Запустить асинхронные запросы по хешам.
        virtual void startPending(std::vector<Hash_t> hashes) = 0;
    };

    struct SourceEntry {
        // Экземпляр источника.
        std::unique_ptr<IResourceSource> Source;
        // Поколение для инвалидирования кэша.
        size_t Generation = 0;
    };

    struct SourceCacheEntry {
        // Индекс источника, где был найден хеш.
        size_t SourceIndex = 0;
        // Поколение источника на момент кэширования.
        size_t Generation = 0;
    };

    // Конструктор с зависимостью от io_context и кэш-пути.
    AssetsManager(asio::io_context& ioc, const fs::path& cachePath,
        size_t maxCacheDirectorySize, size_t maxLifeTime);

    // Инициализация списка источников.
    void initSources();
    // Забрать готовые результаты из источников.
    void collectReadyFromSources();
    // Запросить ресурс в источниках, с учётом кэша.
    SourceResult querySources(const ResourceKey& key);
    // Запомнить успешный источник для хеша.
    void registerSourceHit(const Hash_t& hash, size_t sourceIndex);
    // Инвалидировать кэш по конкретному источнику.
    void invalidateSourceCache(size_t sourceIndex);
    // Инвалидировать весь кэш источников.
    void invalidateAllSourceCache();

    // Выделить новый локальный id.
    AssetId allocateLocalId(AssetType type);
    // Получить корневой локальный id с компрессией пути.
    AssetId resolveLocalIdMutable(AssetType type, AssetId localId);
    // Получить корневой локальный id без мутаций.
    AssetId resolveLocalId(AssetType type, AssetId localId) const;
    // Объединить два локальных id в один.
    void unionLocalIds(AssetType type, AssetId fromId, AssetId toId, std::optional<AssetId>* reboundFrom);

    // Найти ресурс в паке по домену/ключу.
    std::optional<PackResource> findPackResource(AssetType type, std::string_view domain, std::string_view key) const;

    // Логгер подсистемы.
    Logger LOG = "Client>AssetsManager";
    // Менеджер файлового кэша.
    AssetsCacheManager::Ptr Cache;

    // Таблицы данных по каждому типу ресурсов.
    std::array<PerType, static_cast<size_t>(AssetType::MAX_ENUM)> Types;

    // Список источников ресурсов.
    std::vector<SourceEntry> Sources;
    // Кэш попаданий по хешу.
    std::unordered_map<Hash_t, SourceCacheEntry> SourceCacheByHash;
    // Индекс источника паков.
    size_t PackSourceIndex = 0;
    // Индекс памяти (RAM) как источника.
    size_t MemorySourceIndex = 0;
    // Индекс файлового кэша.
    size_t CacheSourceIndex = 0;

    // Ресурсы в памяти по хешу.
    std::unordered_map<Hash_t, Resource> MemoryResourcesByHash;
    // Ожидающие запросы, сгруппированные по хешу.
    std::unordered_map<Hash_t, std::vector<ResourceKey>> PendingReadsByHash;
    // Готовые ответы на чтение.
    std::vector<std::pair<ResourceKey, std::optional<Resource>>> ReadyReads;
};

} // namespace LV::Client
