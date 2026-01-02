#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <unordered_map>
#include "Common/Async.hpp"
#include "TOSAsync.hpp"
#include "boost/asio/executor.hpp"
#include "boost/asio/experimental/channel.hpp"
#include "boost/asio/this_coro.hpp"
#include "sha2.hpp"

/*
    Класс отвечает за отслеживание изменений и подгрузки медиаресурсов в указанных директориях.
    Медиаресурсы, собранные из папки assets или зарегистрированные модами.
    Хранит все данные в оперативной памяти.
*/


enum class EnumAssets : int {
   Nodestate, Particle, Animation, Model, Texture, Sound, Font, MAX_ENUM
};

using AssetsNodestate   = uint32_t;
using AssetsParticle    = uint32_t;
using AssetsAnimation   = uint32_t;
using AssetsModel       = uint32_t;
using AssetsTexture     = uint32_t;
using AssetsSound       = uint32_t;
using AssetsFont        = uint32_t;

static constexpr const char* EnumAssetsToDirectory(EnumAssets value) {
    switch(value) {
    case EnumAssets::Nodestate: return "nodestate";
    case EnumAssets::Particle:  return "particles";
    case EnumAssets::Animation: return "animations";
    case EnumAssets::Model:     return "models";
    case EnumAssets::Texture:   return "textures";
    case EnumAssets::Sound:     return "sounds";
    case EnumAssets::Font:      return "fonts";
    default:
    }

    assert(!"Неизвестный тип медиаресурса");
}

namespace LV {

namespace fs = std::filesystem;

struct ResourceFile {
    using Hash_t = sha2::sha256_hash; // boost::uuids::detail::sha1::digest_type;

    Hash_t Hash;
    std::vector<std::byte> Data;

    void calcHash() {
        Hash = sha2::sha256((const uint8_t*) Data.data(), Data.size());
    }
};

class AssetsPreloader : public TOS::IAsyncDestructible {
public:
    using Ptr = std::shared_ptr<AssetsPreloader>;

    // 
    struct ReloadResult {
    };

    struct ReloadStatus {
        /// TODO: callback'и для обновления статусов
        /// TODO: многоуровневый статус std::vector<std::string>. Этапы/Шаги/Объекты 
    };

public:
    static coro<Ptr> Create(asio::io_context& ioc);
    ~AssetsPreloader() = default;

    AssetsPreloader(const AssetsPreloader&) = delete;
    AssetsPreloader(AssetsPreloader&&) = delete;
    AssetsPreloader& operator=(const AssetsPreloader&) = delete;
    AssetsPreloader& operator=(AssetsPreloader&&) = delete;

    // Пересматривает ресурсы и выдаёт изменения.
    // Одновременно можно работать только один такой вызов.
    // instances -> пути к директории с assets или архивы с assets внутри. От низшего приоритета к высшему.
    // status -> обратный отклик о процессе обновления ресурсов.
    // ReloadStatus <- новые и потерянные ресурсы.
    coro<ReloadResult> reloadResources(const std::vector<fs::path>& instances, ReloadStatus* status = nullptr) {
        bool expected = false;
        assert(Reloading_.compare_exchange_strong(expected, true) && "Двойной вызов reloadResources");

        try {
            ReloadStatus secondStatus;
            co_return _reloadResources(instances, status ? *status : secondStatus);
        } catch(...) {
            assert(!"reloadResources: здесь не должно быть ошибок");
        }

        Reloading_.exchange(false);
    }

private:
    struct ResourceFirstStageInfo {
        // Путь к архиву (если есть), и путь до ресурса
        fs::path ArchivePath, Path;
        // Время изменения файла
        fs::file_time_type Timestamp;
    };

    struct ResourceSecondStageInfo : public ResourceFirstStageInfo {
        // Обезличенный ресурс
        std::shared_ptr<std::vector<uint8_t>> Resource;
        ResourceFile::Hash_t Hash;
        // Сырой заголовок
        std::vector<std::string> Dependencies;
    };

    /*
        Ресурс имеет бинарную часть, из который вырезаны все зависимости.
        Вторая часть это заголовок, которые всегда динамично передаётся с сервера.
        В заголовке хранятся зависимости от ресурсов.
    */
    struct MediaResource {
        std::string Domain, Key;

        fs::file_time_type Timestamp;
        // Обезличенный ресурс
        std::shared_ptr<std::vector<uint8_t>> Resource;
        // Хэш ресурса
        ResourceFile::Hash_t Hash;

        // Скомпилированный заголовок
        std::vector<uint8_t> Dependencies;
    };

    AssetsPreloader(asio::io_context& ioc)
    : TOS::IAsyncDestructible(ioc)
    {

    } 

    // Текущее состояние reloadResources
    std::atomic<bool> Reloading_ = false;

    // Пересмотр ресурсов
    coro<ReloadResult> _reloadResources(const std::vector<fs::path>& instances, ReloadStatus& status) const {
        // 1) Поиск всех ресурсов и построение конечной карты ресурсов (timestamps, path, name, size)
        // Карта найденных ресурсов
        std::unordered_map<
            EnumAssets, // Тип ресурса
            std::unordered_map<
                std::string, // Domain
                std::unordered_map<
                    std::string, // Key
                    ResourceFirstStageInfo // ResourceInfo
                >
            >
        > resourcesFirstStage;

        for (const fs::path& instance : instances) {
            try {
                if (fs::is_regular_file(instance)) {
                    // Может архив
                    /// TODO: пока не поддерживается
                } else if (fs::is_directory(instance)) {
                    // Директория
                    fs::path assets = instance / "assets";
                    if (fs::exists(assets) && fs::is_directory(assets)) {
                        // Директорию assets существует, перебираем домены в ней 
                        for (auto begin = fs::directory_iterator(assets), end = fs::directory_iterator(); begin != end; begin++) {
                            if (!begin->is_directory())
                                continue;

                            /// TODO: выглядит всё не очень асинхронно
                            co_await asio::post(co_await asio::this_coro::executor);

                            fs::path domainPath = begin->path();
                            std::string domain = domainPath.filename();
                            
                            // Перебираем по типу ресурса
                            for (EnumAssets assetType = EnumAssets(0); assetType < EnumAssets::MAX_ENUM; ((int&) assetType)++) {
                                fs::path assetPath = domainPath / EnumAssetsToDirectory(assetType);

                                std::unordered_map<
                                    std::string, // Key
                                    ResourceFirstStageInfo // ResourceInfo
                                >& firstStage = resourcesFirstStage[assetType][domain];

                                // Исследуем все ресурсы одного типа
                                for (auto begin = fs::recursive_directory_iterator(assetPath), end = fs::recursive_directory_iterator(); begin != end; begin++) {
                                    if (begin->is_directory())
                                        continue;

                                    fs::path file = begin->path();
                                    std::string key = fs::relative(file, domainPath).string();

                                    // Работаем с ресурсом
                                    firstStage[key] = ResourceFirstStageInfo{
                                        .Path = file,
                                        .Timestamp = fs::last_write_time(file)
                                    };
                                }
                            }
                        }
                    }
                } else {
                    throw std::runtime_error("Неизвестный тип инстанса медиаресурсов");
                }
            } catch (const std::exception& exc) {
                /// TODO: Логгировать в статусе

            }
        }

        // 2) Обрабатываться будут только изменённые (новый timestamp) или новые ресурсы
        // .meta

        // Текстуры, шрифты, звуки хранить как есть
        // У моделей, состояний нод, анимации, частиц обналичить зависимости
        // Мета влияет только на хедер

        /// TODO: реализовать реформатирование новых и изменённых ресурсов во внутренний обезличенный формат

        co_await asio::post(co_await asio::this_coro::executor);

        asio::experimental::channel<void()> ch(IOC, 8);

        co_return ReloadResult{};
    }

    std::unordered_map<
        EnumAssets, // Тип ресурса
        std::unordered_map<
            std::string, // Domain
            std::unordered_map<
                std::string, // Key
                uint32_t     // ResourceId
            >
        >
    > DKToId;

    std::unordered_map<
        EnumAssets, // Тип ресурса
        std::unordered_map<
            uint32_t, 
            MediaResource // ResourceInfo
        >
    > MediaResources;
};

}