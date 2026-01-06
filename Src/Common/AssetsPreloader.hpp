#pragma once

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
#include <vector>
#include "Abstract.hpp"
#include "Common/Abstract.hpp"

/*
    Класс отвечает за отслеживание изменений и подгрузки медиаресурсов в указанных директориях.
    Медиаресурсы, собранные из папки assets или зарегистрированные модами.
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

class AssetsPreloader {
public:
    using Ptr = std::shared_ptr<AssetsPreloader>;

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
        std::u8string Resource;
        // Его хеш
        ResourceFile::Hash_t Hash;
        // Заголовок
        std::u8string Header;
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
        idResolver -> функция получения идентификатора по Тип+Домен+Ключ
        onNewResourceParsed -> Callback на обработку распаршенных ресурсов без заголовков
        (на стороне сервера хранится в другой сущности, на стороне клиента игнорируется).
        status -> обратный отклик о процессе обновления ресурсов.
        ReloadStatus <- новые и потерянные ресурсы.
    */
    struct Out_checkAndPrepareResourcesUpdate {
        // Новые связки Id -> Hash + Header + Timestamp + Path (ресурс новый или изменён)
        std::array<
            std::vector<
                std::tuple<
                    ResourceId,             // Ресурс
                    ResourceFile::Hash_t,   // Хэш ресурса на диске
                    ResourceHeader,         // Хедер ресурса (со всеми зависимостями)
                    fs::file_time_type,     // Время изменения ресурса на диске
                    fs::path                // Путь до ресурса
                >
            >, 
            static_cast<size_t>(AssetType::MAX_ENUM)
        > ResourceUpdates;

        // Используется чтобы эффективно увеличить размер таблиц
        std::array<
            ResourceId,
            static_cast<size_t>(AssetType::MAX_ENUM)
        > MaxNewSize;

        // Потерянные связки Id (ресурс физически потерян)
        std::array<
            std::vector<ResourceId>,
            static_cast<size_t>(AssetType::MAX_ENUM)
        > LostLinks;

        /* 
            Новые пути предоставляющие хеш
            (по каким путям можно получить ресурс определённого хеша).
        */
        std::unordered_map<
            ResourceFile::Hash_t, 
            std::vector<fs::path>
        > HashToPathNew;

        /* 
            Потерянные пути, предоставлявшые ресурсы с данным хешем
            (пути по которым уже нельзя получить заданных хеш).
        */
        std::unordered_map<
            ResourceFile::Hash_t, 
            std::vector<fs::path>
        > HashToPathLost;
    };

    Out_checkAndPrepareResourcesUpdate checkAndPrepareResourcesUpdate(
        const AssetsRegister& instances,
        const std::function<ResourceId(EnumAssets type, std::string_view domain, std::string_view key)>& idResolver,
        const std::function<void(std::u8string&& resource, ResourceFile::Hash_t hash, fs::path resPath)>& onNewResourceParsed = nullptr,
        ReloadStatus* status = nullptr
    );

    /*
        Применяет расчитанные изменения.

        Out_applyResourceUpdate <- Нужно отправить клиентам новые привязки ресурсов
        id -> hash+header
    */
    struct BindHashHeaderInfo {
        ResourceId Id;
        ResourceFile::Hash_t Hash;
        ResourceHeader Header;
    };

    struct Out_applyResourcesUpdate {
        std::array<
            std::vector<BindHashHeaderInfo>, 
            static_cast<size_t>(EnumAssets::MAX_ENUM)
        > NewOrUpdates;
    };

    Out_applyResourcesUpdate applyResourcesUpdate(const Out_checkAndPrepareResourcesUpdate& orr);

    std::array< 
        std::vector<BindHashHeaderInfo>,
        static_cast<size_t>(EnumAssets::MAX_ENUM)
    > collectHashBindings() const 
    {
        std::array< 
            std::vector<BindHashHeaderInfo>,
            static_cast<size_t>(EnumAssets::MAX_ENUM)
        > result;

        for(size_t type = 0; type < static_cast<size_t>(EnumAssets::MAX_ENUM); ++type) {
            result[type].reserve(ResourceLinks[type].size());

            ResourceId counter = 0;
            for(const auto& [hash, header, _1, _2, _3] : ResourceLinks[type]) {
                ResourceId id = counter++;
                result[type].emplace_back(id, hash, header);
            }
        }

        return result;
    }

    const auto& getResourceLinks() const {
        return ResourceLinks;
    }

    struct Out_Resource {
        ResourceFile::Hash_t Hash;
        fs::path Path;
    };

    std::optional<Out_Resource> getResource(EnumAssets type, ResourceId id) const {
        const auto& rl = ResourceLinks[static_cast<size_t>(type)];
        if(id >= rl.size() || !rl[id].IsExist)
            return std::nullopt;

        return Out_Resource{rl[id].Hash, rl[id].Path};
    }

private:
    struct ResourceFindInfo {
        // Путь к архиву (если есть), и путь до ресурса
        fs::path ArchivePath, Path;
        // Время изменения файла
        fs::file_time_type Timestamp;
        // Идентификатор ресурса
        ResourceId Id;
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

    #ifndef NDEBUG
    // Текущее состояние reloadResources
    std::atomic<bool> _Reloading = false;
    #endif

    struct ResourceLink {
        ResourceFile::Hash_t Hash;      // Хэш ресурса на диске
        /// TODO: клиенту не нужны хедеры
        ResourceHeader Header;          // Хедер ресурса (со всеми зависимостями)
        fs::file_time_type LastWrite;   // Время изменения ресурса на диске
        fs::path Path;                  // Путь до ресурса
        bool IsExist;
    };

    Out_checkAndPrepareResourcesUpdate _checkAndPrepareResourcesUpdate(
        const AssetsRegister& instances,
        const std::function<ResourceId(EnumAssets type, std::string_view domain, std::string_view key)>& idResolver,
        const std::function<void(std::u8string&& resource, ResourceFile::Hash_t hash, fs::path resPath)>& onNewResourceParsed,
        ReloadStatus& status
    );

    // Привязка Id -> Hash + Header + Timestamp + Path
    std::array<
        std::vector<ResourceLink>, 
        static_cast<size_t>(AssetType::MAX_ENUM)
    > ResourceLinks;
};

}
