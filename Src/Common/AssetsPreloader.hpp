#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include "Common/TexturePipelineProgram.hpp"
#include "Common/Abstract.hpp"
#include "Common/Async.hpp"
#include "TOSAsync.hpp"
#include "TOSLib.hpp"
#include "sha2.hpp"

/*
    Класс отвечает за отслеживание изменений и подгрузки медиаресурсов в указанных директориях.
    Медиаресурсы, собранные из папки assets или зарегистрированные модами.
    Хранит все данные в оперативной памяти.
*/

static constexpr const char* EnumAssetsToDirectory(LV::EnumAssets value) {
    switch(value) {
    case LV::EnumAssets::Nodestate: return "nodestate";
    case LV::EnumAssets::Particle:  return "particle";
    case LV::EnumAssets::Animation: return "animation";
    case LV::EnumAssets::Model:     return "model";
    case LV::EnumAssets::Texture:   return "texture";
    case LV::EnumAssets::Sound:     return "sound";
    case LV::EnumAssets::Font:      return "font";
    default:
        break;
    }

    assert(!"Неизвестный тип медиаресурса");
    return "";
}

namespace LV {

namespace fs = std::filesystem;
using AssetType = EnumAssets;

struct ResourceFile {
    using Hash_t = sha2::sha256_hash; // boost::uuids::detail::sha1::digest_type;

    Hash_t Hash;
    std::u8string Data;

    void calcHash() {
        Hash = sha2::sha256((const uint8_t*) Data.data(), Data.size());
    }
};

class AssetsPreloader {
public:
    using Ptr = std::shared_ptr<AssetsPreloader>;
    using IdTable = 
        std::unordered_map<
            std::string, // Domain
            std::unordered_map<
                std::string, // Key
                uint32_t,    // ResourceId
                detail::TSVHash,
                detail::TSVEq
            >,
            detail::TSVHash,
            detail::TSVEq
        >;

    // 
    /*
        Ресурс имеет бинарную часть, из который вырезаны все зависимости.
        Вторая часть это заголовок, которые всегда динамично передаётся с сервера.
        В заголовке хранятся зависимости от ресурсов.
    */
    struct MediaResource {
        std::string Domain, Key;

        fs::file_time_type Timestamp;
        // Обезличенный ресурс
        std::shared_ptr<std::u8string> Resource;
        // Хэш ресурса
        ResourceFile::Hash_t Hash;

        // Скомпилированный заголовок
        std::u8string Header;
    };

    struct PendingResource {
        uint32_t Id;
        std::string Key;
        fs::file_time_type Timestamp;
        // Обезличенный ресурс
        std::shared_ptr<std::u8string> Resource;
        // Его хеш
        ResourceFile::Hash_t Hash;
        // Заголовок
        std::u8string Header;
    };

    struct BindDomainKeyInfo {
        std::string Domain;
        std::string Key;
    };

    struct BindHashHeaderInfo {
        ResourceId Id;
        Hash_t Hash;
        std::u8string Header;
    };

    struct Out_reloadResources {
        std::unordered_map<std::string, std::vector<PendingResource>> NewOrChange[(int) AssetType::MAX_ENUM];
        std::unordered_map<std::string, std::vector<std::string>> Lost[(int) AssetType::MAX_ENUM];
    };

    struct Out_applyResourceChange {
        std::array<
            std::vector<AssetsPreloader::BindHashHeaderInfo>,
            static_cast<size_t>(AssetType::MAX_ENUM)
        > NewOrChange;

        std::array<
            std::vector<ResourceId>, 
            static_cast<size_t>(AssetType::MAX_ENUM)
        > Lost;
    };

    struct Out_bakeId {
        // Новые привязки
        std::array<
            std::vector<BindDomainKeyInfo>, 
            static_cast<size_t>(AssetType::MAX_ENUM)
        > IdToDK;
    };

    struct ReloadStatus {
        /// TODO: callback'и для обновления статусов
        /// TODO: многоуровневый статус std::vector<std::string>. Этапы/Шаги/Объекты 
    };

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
        std::array<
            std::unordered_map<
                std::string,
                std::unordered_map<std::string, void*>
            >,
            static_cast<size_t>(AssetType::MAX_ENUM)
        > Custom;
    };

public:
    AssetsPreloader();
    ~AssetsPreloader() = default;

    AssetsPreloader(const AssetsPreloader&) = delete;
    AssetsPreloader(AssetsPreloader&&) = delete;
    AssetsPreloader& operator=(const AssetsPreloader&) = delete;
    AssetsPreloader& operator=(AssetsPreloader&&) = delete;

    /*
        Перепроверка изменений ресурсов по дате изменения, пересчёт хешей.
        Обнаруженные изменения должны быть отправлены всем клиентам.
        Ресурсы будут обработаны в подходящий формат и сохранены в кеше.
        Используется в GameServer.
        ! Одновременно можно работать только один такой вызов.
        ! Бронирует идентификаторы используя getId();

        instances -> пути к директории с assets или архивы с assets внутри. От низшего приоритета к высшему.
        status -> обратный отклик о процессе обновления ресурсов.
        ReloadStatus <- новые и потерянные ресурсы.
    */
    Out_reloadResources reloadResources(const AssetsRegister& instances, ReloadStatus* status = nullptr);

    /*
        Применяет расчитанные изменения.

        Out_applyResourceChange <- Нужно отправить клиентам новые привязки ресурсов
        id -> hash+header
    */
    Out_applyResourceChange applyResourceChange(const Out_reloadResources& orr);

    /*
        Выдаёт идентификатор ресурса.
        Многопоточно.
        Иногда нужно вызывать bakeIdTables чтобы оптимизировать таблицы
        идентификаторов. При этом никто не должен использовать getId
    */
    ResourceId getId(AssetType type, std::string_view domain, std::string_view key);

    /*
        Оптимизирует таблицы идентификаторов.
        Нельзя использовать пока есть вероятность что кто-то использует getId().
        Такжке нельзя при выполнении reloadResources().

        Out_bakeId <- Нужно отправить подключенным клиентам новые привязки id -> домен+ключ
    */
    Out_bakeId bakeIdTables();

    /*
        Выдаёт пакет со всеми текущими привязками id -> домен+ключ.
        Используется при подключении новых клиентов.
    */
    void makeGlobalLinkagePacket() {
        /// TODO: Собрать пакет с IdToDK и сжать его домены и ключи и id -> hash+header

        // Тот же пакет для обновления идентификаторов 
        std::unreachable();
    }

    // Выдаёт ресурс по идентификатору
    const MediaResource* getResource(AssetType type, uint32_t id) const;

    // Выдаёт ресурс по хешу
    std::optional<std::tuple<AssetType, uint32_t, const MediaResource*>> getResource(const ResourceFile::Hash_t& hash);

    // Выдаёт зависимости к ресурсам профиля ноды
    std::tuple<AssetsNodestate, std::vector<AssetsModel>, std::vector<AssetsTexture>>
        getNodeDependency(const std::string& domain, const std::string& key);

private:
    struct ResourceFindInfo {
        // Путь к архиву (если есть), и путь до ресурса
        fs::path ArchivePath, Path;
        // Время изменения файла
        fs::file_time_type Timestamp;
    };

    struct HashHasher {
        std::size_t operator()(const ResourceFile::Hash_t& hash) const noexcept {
            std::size_t v = 14695981039346656037ULL;
            for (const auto& byte : hash) {
                v ^= static_cast<std::size_t>(byte);
                v *= 1099511628211ULL;
            }
            return v;
        }
    };

    // Текущее состояние reloadResources
    std::atomic<bool> _Reloading = false;

    // Если идентификатор не найден в асинхронной таблице, переходим к работе с синхронной
    ResourceId _getIdNew(AssetType type, std::string_view domain, std::string_view key);

    Out_reloadResources _reloadResources(const AssetsRegister& instances, ReloadStatus& status);

    #ifndef NDEBUG
    // Для контроля за режимом слияния ключей
    bool DKToIdInBakingMode = false;
    #endif

    /*
        Многопоточная таблица идентификаторов. Новые идентификаторы выделяются в NewDKToId,
        и далее вливаются в основную таблицу при вызове bakeIdTables()
    */
    std::array<IdTable, static_cast<size_t>(AssetType::MAX_ENUM)> DKToId;
    /*
        Многопоточная таблица обратного резолва.
        Идентификатор -> домен+ключ
    */
    std::array<std::vector<BindDomainKeyInfo>, static_cast<size_t>(AssetType::MAX_ENUM)> IdToDK;

    /*
        Таблица в которой выделяются новые идентификаторы, которых не нашлось в DKToId.
        Данный объект одновременно может работать только с одним потоком.
    */
    std::array<TOS::SpinlockObject<IdTable>, static_cast<size_t>(AssetType::MAX_ENUM)> NewDKToId;
    /*
        Конец поля идентификаторов, известный клиентам.
        Если NextId продвинулся дальше, нужно уведомить клиентов о новых привязках.
    */
    std::array<ResourceId, static_cast<size_t>(AssetType::MAX_ENUM)> LastSendId;
    /*
        Списки в которых пишутся новые привязки. Начала спиской исходят из LastSendId.
        Id + LastSendId -> домен+ключ
    */
    std::array<TOS::SpinlockObject<std::vector<BindDomainKeyInfo>>, static_cast<size_t>(AssetType::MAX_ENUM)> NewIdToDK;

    // Загруженные ресурсы
    std::array<std::unordered_map<ResourceId, MediaResource>, static_cast<size_t>(AssetType::MAX_ENUM)> MediaResources;
    // Hash -> ресурс
    std::unordered_map<ResourceFile::Hash_t, std::pair<AssetType, ResourceId>, HashHasher> HashToId;
    // Для последовательного выделения идентификаторов
    std::array<ResourceId, static_cast<size_t>(AssetType::MAX_ENUM)> NextId;
};

inline ResourceId AssetsPreloader::getId(AssetType type, std::string_view domain, std::string_view key) {
    #ifndef NDEBUG
    assert(!DKToIdInBakingMode);
    #endif

    const auto& typeTable = DKToId[static_cast<size_t>(type)];
    auto domainTable = typeTable.find(domain);

    #ifndef NDEBUG
    assert(!DKToIdInBakingMode);
    #endif

    if(domainTable == typeTable.end())
        return _getIdNew(type, domain, key);

    auto keyTable = domainTable->second.find(key);

    if (keyTable == domainTable->second.end())
        return _getIdNew(type, domain, key);

    return keyTable->second;

    return 0;
}

inline ResourceId AssetsPreloader::_getIdNew(AssetType type, std::string_view domain, std::string_view key) {
    auto lock = NewDKToId[static_cast<size_t>(type)].lock();

    auto iterDomainNewTable = lock->find(domain);
    if(iterDomainNewTable == lock->end()) {
        iterDomainNewTable = lock->emplace_hint(
            iterDomainNewTable,
            (std::string) domain,
            std::unordered_map<std::string, uint32_t, detail::TSVHash, detail::TSVEq>{}
        );
    }

    auto& domainNewTable = iterDomainNewTable->second;

    if(auto iter = domainNewTable.find(key); iter != domainNewTable.end())
        return iter->second;

    uint32_t id = domainNewTable[(std::string) key] = NextId[static_cast<size_t>(type)]++;

    auto lock2 = NewIdToDK[static_cast<size_t>(type)].lock();
    lock.unlock();

    lock2->emplace_back((std::string) domain, (std::string) key);

    return id;
}

inline const AssetsPreloader::MediaResource* AssetsPreloader::getResource(AssetType type, uint32_t id) const {
    auto& iterType = MediaResources[static_cast<size_t>(type)];

    auto iterRes = iterType.find(id);
    if(iterRes == iterType.end())
        return nullptr;

    return &iterRes->second;
}

inline std::optional<std::tuple<AssetType, uint32_t, const AssetsPreloader::MediaResource*>> 
    AssetsPreloader::getResource(const ResourceFile::Hash_t& hash)
{
    auto iter = HashToId.find(hash);
    if(iter == HashToId.end())
        return std::nullopt;

    auto [type, id] = iter->second;
    const MediaResource* res = getResource(type, id);
    if(!res) {
        HashToId.erase(iter);
        return std::nullopt;
    }

    if(res->Hash != hash) {
        HashToId.erase(iter);
        return std::nullopt;
    }

    return std::tuple<AssetType, uint32_t, const MediaResource*>{type, id, res};
}

}
