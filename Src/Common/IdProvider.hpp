#pragma once

#include "Common/Abstract.hpp"


namespace LV {

template<class Enum = EnumAssets>
class IdProvider {
public:
    static constexpr size_t MAX_ENUM = static_cast<size_t>(Enum::MAX_ENUM);
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

    struct BindDomainKeyInfo {
        std::string Domain, Key;
    };

public:
    IdProvider() {
        std::fill(NextId.begin(), NextId.end(), 1);
        for(size_t type = 0; type < static_cast<size_t>(Enum::MAX_ENUM); ++type) {
            DKToId[type]["core"]["none"] = 0;
            IdToDK[type].emplace_back("core", "none");
        }
    }
    /*
        Находит или выдаёт идентификатор на запрошенный ресурс.
        Функция не требует внешней синхронизации.
        Требуется периодически вызывать bake().
    */
    inline ResourceId getId(EnumAssets type, std::string_view domain, std::string_view key) {
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

    /*
        Переносит все новые идентификаторы в основную таблицу.
        Нельзя использовать пока есть вероятность что кто-то использует getId().

        Out_bakeId <- Возвращает все новые привязки.
    */
    std::array<
        std::vector<BindDomainKeyInfo>, 
        MAX_ENUM
    > bake() {
        #ifndef NDEBUG

        assert(!DKToIdInBakingMode);
        DKToIdInBakingMode = true;
        struct _tempStruct {
            IdProvider* handler;
            ~_tempStruct() { handler->DKToIdInBakingMode = false; }
        } _lock{this};

        #endif

        std::array<
            std::vector<BindDomainKeyInfo>, 
            MAX_ENUM
        > result;

        for(size_t type = 0; type < MAX_ENUM; ++type) {
            // Домен+Ключ -> Id
            {
                auto lock = NewDKToId[type].lock();
                auto& dkToId = DKToId[type];
                for(auto& [domain, keys] : *lock) {
                    // Если домен не существует, просто воткнёт новые ключи
                    auto [iterDomain, inserted] = dkToId.try_emplace(domain, std::move(keys));
                    if(!inserted) {
                        // Домен уже существует, сливаем новые ключи
                        iterDomain->second.merge(keys);
                    }
                }

                lock->clear();
            }

            // Id -> Домен+Ключ
            {
                auto lock = NewIdToDK[type].lock();

                auto& idToDK = IdToDK[type];
                result[type] = std::move(*lock);
                lock->clear();
                idToDK.append_range(result[type]);
            }
        }

        return result;
    }

    // Для отправки новым подключенным клиентам 
    const std::array<
        std::vector<BindDomainKeyInfo>,
        static_cast<size_t>(EnumAssets::MAX_ENUM)
    >& idToDK() const {
        return IdToDK;
    }

protected:
    #ifndef NDEBUG
    // Для контроля за режимом слияния ключей
    bool DKToIdInBakingMode = false;
    #endif

    /*
        Работает с таблицами для новых идентификаторов, в синхронном режиме.
        Используется когда в основных таблицах не нашлось привязки,
        она будет найдена или создана здесь синхронно.
    */
    inline ResourceId _getIdNew(EnumAssets type, std::string_view domain, std::string_view key) {
        // Блокировка по нужному типу ресурса
        auto lock = NewDKToId[static_cast<size_t>(type)].lock();

        auto iterDomainNewTable = lock->find(domain);
        // Если домена не нашлось, сразу вставляем его на подходящее место
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
        else {
            uint32_t id = NextId[static_cast<size_t>(type)]++;
            domainNewTable.emplace_hint(iter, (std::string) key, id);

            // Добавился новый идентификатор, теперь добавим обратную связку
            auto lock2 = NewIdToDK[static_cast<size_t>(type)].lock();
            lock.unlock();

            lock2->emplace_back((std::string) domain, (std::string) key);
            return id;
        }
    }

// Условно многопоточные объекты
    /*
        Таблица идентификаторов. Новые идентификаторы выделяются в NewDKToId,
        и далее вливаются в основную таблицу при вызове bakeIdTables().

        Домен+Ключ -> Id
    */
    std::array<IdTable, MAX_ENUM> DKToId;

    /*
        Таблица обратного резолва.
        Id -> Домен+Ключ.
    */
    std::array<std::vector<BindDomainKeyInfo>, MAX_ENUM> IdToDK;

// Требующие синхронизации
    /*
        Таблица в которой выделяются новые идентификаторы, перед вливанием в DKToId.
        Домен+Ключ -> Id.
    */
    std::array<TOS::SpinlockObject<IdTable>, MAX_ENUM> NewDKToId;

    /*
        Списки в которых пишутся новые привязки.
        Id + LastMaxId -> Домен+Ключ.
    */
    std::array<TOS::SpinlockObject<std::vector<BindDomainKeyInfo>>, MAX_ENUM> NewIdToDK;

    // Для последовательного выделения идентификаторов
    std::array<ResourceId, MAX_ENUM> NextId;
};

}