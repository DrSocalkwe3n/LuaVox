#pragma once

#include "Common/Abstract.hpp"

#include <ankerl/unordered_dense.h>

#include <array>
#include <atomic>
#include <cassert>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <algorithm>

namespace LV {

template<class Enum = EnumAssets, size_t ShardCount = 64>
class IdProvider {
public:
    static constexpr size_t MAX_ENUM = static_cast<size_t>(Enum::MAX_ENUM);

    struct BindDomainKeyInfo {
        std::string Domain, Key;
    };

public:
    explicit IdProvider() {
        for(size_t type = 0; type < MAX_ENUM; ++type) {
            _NextId[type].store(1, std::memory_order_relaxed);
            _Reverse[type].reserve(1024);

            IdToDK[type].push_back({"core", "none"});

            auto& sh = _shardFor(static_cast<Enum>(type), "core", "none");
            std::unique_lock lk(sh.mutex);
            sh.map.emplace(Key{"core", "none"}, 0);
        }
    }

    /*
        Находит или выдаёт идентификатор на запрошенный ресурс.
        Функция не требует внешней синхронизации.
    */
    inline ResourceId getId(Enum type, std::string_view domain, std::string_view key) {
#ifndef NDEBUG
        assert(!DKToIdInBakingMode);
#endif
        auto& sh = _shardFor(type, domain, key);

        // 1) Поиск в режиме для чтения
        {
            std::shared_lock lk(sh.mutex);
            if(auto it = sh.map.find(KeyView{domain, key}); it != sh.map.end()) {
                return it->second;
            }
        }

        // 2) Блокируем и повторно ищем запись (может кто уже успел её добавить)
        std::unique_lock lk(sh.mutex);
        if (auto it = sh.map.find(KeyView{domain, key}); it != sh.map.end()) {
            return it->second;
        }

        // Выделяем идентификатор
        ResourceId id = _NextId[static_cast<size_t>(type)].fetch_add(1, std::memory_order_relaxed);

        std::string d(domain);
        std::string k(key);

        sh.map.emplace(Key{d, k}, id);
        sh.newlyInserted.push_back(id);

        _storeReverse(type, id, std::move(d), std::move(k));

        return id;
    }

    /*
        Переносит все новые идентификаторы в основную таблицу.

        В этой реализации "основная таблица" уже основная (forward map обновляется сразу),
        а bake() собирает только новые привязки (domain,key) по логам вставок и дополняет IdToDK.

        Нельзя использовать пока есть вероятность что кто-то использует getId(), если ты хочешь
        строгий debug-контроль как раньше. В релизе это не требуется: bake читает только reverse,
        а forward не трогает.
    */
    std::array<std::vector<BindDomainKeyInfo>, MAX_ENUM> bake() {
#ifndef NDEBUG
        assert(!DKToIdInBakingMode);
        DKToIdInBakingMode = true;
        struct _tempStruct {
            IdProvider* handler;
            ~_tempStruct() { handler->DKToIdInBakingMode = false; }
        } _lock{this};
#endif

        std::array<std::vector<BindDomainKeyInfo>, MAX_ENUM> result;

        for(size_t t = 0; t < MAX_ENUM; ++t) {
            auto type = static_cast<Enum>(t);

            // 1) собрать новые id из всех шардов
            std::vector<ResourceId> new_ids;
            _drainNew(type, new_ids);

            if(new_ids.empty())
                continue;

            // 2) превратить id -> (domain,key) через reverse и вернуть наружу
            // + дописать в IdToDK[type] в порядке id (по желанию)
            std::sort(new_ids.begin(), new_ids.end());
            new_ids.erase(std::unique(new_ids.begin(), new_ids.end()), new_ids.end());

            result[t].reserve(new_ids.size());

            // reverse читаем под shared lock
            std::shared_lock rlk(_ReverseMutex[t]);
            for(ResourceId id : new_ids) {
                // id=0 не бывает в newlyInserted
                const std::size_t idx = static_cast<std::size_t>(id - 1);
                if(idx >= _Reverse[t].size()) {
                    // теоретически не должно случаться (мы пишем reverse до push в log)
                    continue;
                }

                const auto& e = _Reverse[t][idx];
                result[t].push_back({e.Domain, e.Key});
            }

            rlk.unlock();

            // 3) дописать в IdToDK (для новых клиентов)
            // Важно: IdToDK[0] уже содержит "core/none" как элемент 0.
            IdToDK[t].append_range(result[t]); // C++23
        }

        return result;
    }

    // id to DK
    std::optional<BindDomainKeyInfo> getDK(Enum type, ResourceId id) {
        auto& vec = _Reverse[static_cast<size_t>(type)];
        auto& mtx = _ReverseMutex[static_cast<size_t>(type)];

        std::unique_lock lk(mtx);
        if(id >= vec.size())
            return std::nullopt;
        
        return vec[id];
    }

    // Для отправки новым подключенным клиентам
    const std::array<std::vector<BindDomainKeyInfo>, MAX_ENUM>& idToDK() const {
        return IdToDK;
    }

private:
    // ---- key types for unordered_dense ----
    struct Key {
        std::string domain;
        std::string key;
    };

    struct KeyView {
        std::string_view domain;
        std::string_view key;
    };

    struct KeyHash {
        using is_transparent = void;

        static inline std::size_t h(std::string_view sv) noexcept {
            // если у тебя есть detail::TSVHash под string_view — можно подставить
            return std::hash<std::string_view>{}(sv);
        }

        static inline std::size_t mix(std::size_t a, std::size_t b) noexcept {
            a ^= b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2);
            return a;
        }

        std::size_t operator()(const Key& k) const noexcept {
            return mix(h(k.domain), h(k.key));
        }

        std::size_t operator()(const KeyView& kv) const noexcept {
            return mix(h(kv.domain), h(kv.key));
        }
    };

    struct KeyEq {
        using is_transparent = void;

        bool operator()(const Key& a, const Key& b) const noexcept {
            return a.domain == b.domain && a.key == b.key;
        }

        bool operator()(const Key& a, const KeyView& b) const noexcept {
            return a.domain == b.domain && a.key == b.key;
        }

        bool operator()(const KeyView& a, const Key& b) const noexcept {
            return a.domain == b.domain && a.key == b.key;
        }
    };

    using Map = ankerl::unordered_dense::map<Key, ResourceId, KeyHash, KeyEq>;

    struct Shard {
        mutable std::shared_mutex mutex;
        Map map;
        std::vector<ResourceId> newlyInserted;
    };

private:
    // Кластер таблиц идентификаторов
    std::array<
        std::array<Shard, ShardCount>, MAX_ENUM
    > _Shards;

    // Счётчики идентификаторов
    std::array<std::atomic<ResourceId>, MAX_ENUM> _NextId;

    // Таблица обратных связок (Id to DK)
    std::array<std::vector<BindDomainKeyInfo>, MAX_ENUM> _Reverse;
    mutable std::array<std::shared_mutex, MAX_ENUM> _ReverseMutex;

#ifndef NDEBUG
    bool DKToIdInBakingMode = false;
#endif

    // stable "full sync" table for new clients:
    std::array<std::vector<BindDomainKeyInfo>, MAX_ENUM> IdToDK;

private:
    Shard& _shardFor(Enum type, std::string_view domain, std::string_view key) {
        const std::size_t idx = KeyHash{}(KeyView{domain, key}) % ShardCount;
        return _Shards[static_cast<size_t>(type)][idx];
    }

    const Shard& _shardFor(Enum type, std::string_view domain, std::string_view key) const {
        const std::size_t idx = KeyHash{}(KeyView{domain, key}) % ShardCount;
        return _Shards[static_cast<size_t>(type)][idx];
    }

    void _storeReverse(Enum type, ResourceId id, std::string&& domain, std::string&& key) {
        auto& vec = _Reverse[static_cast<size_t>(type)];
        auto& mtx = _ReverseMutex[static_cast<size_t>(type)];
        const std::size_t idx = static_cast<std::size_t>(id);

        std::unique_lock lk(mtx);
        if(idx >= vec.size())
            vec.resize(idx + 1);

        vec[idx] = BindDomainKeyInfo{std::move(domain), std::move(key)};
    }

    void _drainNew(Enum type, std::vector<ResourceId>& out) {
        out.clear();
        auto& shards = _Shards[static_cast<size_t>(type)];

        // Можно добавить reserve по эвристике
        for (auto& sh : shards) {
            std::unique_lock lk(sh.mutex);
            if (sh.newlyInserted.empty()) continue;

            const auto old = out.size();
            out.resize(old + sh.newlyInserted.size());
            std::copy(sh.newlyInserted.begin(), sh.newlyInserted.end(), out.begin() + old);
            sh.newlyInserted.clear();
        }
    }
};

} // namespace LV
