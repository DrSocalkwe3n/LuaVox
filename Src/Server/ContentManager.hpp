#pragma once

#include "Server/Abstract.hpp"


namespace LV::Server {

struct DefVoxel {

};

struct DefNode {

};

struct DefWorld {

};

struct DefPortal {

};

struct DefEntity {

};

struct DefItem {

};

class ContentManager {
    // Профили зарегистрированные модами

    // Изменения, накладываемые на профили

    // Следующие идентификаторы регистрации контента
    ResourceId_t NextId[(int) EnumDefContent::MAX_ENUM] = {0};
    // Домен -> {ключ -> идентификатор}
    std::unordered_map<std::string, std::unordered_map<std::string, ResourceId_t>> ContentKeyToId[(int) EnumDefContent::MAX_ENUM];

    template<typename T>
    struct TableEntry {
        static constexpr size_t ChunkSize = 4096;
        std::array<std::optional<T>, ChunkSize> Entries;
    };

    // Конечные профили контента
    std::vector<std::unique_ptr<TableEntry<DefVoxel>>>     Profiles_Voxel;
    std::vector<std::unique_ptr<TableEntry<DefNode>>>       Profiles_Node;
    std::vector<std::unique_ptr<TableEntry<DefWorld>>>     Profiles_World;
    std::vector<std::unique_ptr<TableEntry<DefPortal>>>   Profiles_Portal;
    std::vector<std::unique_ptr<TableEntry<DefEntity>>>   Profiles_Entity;
    std::vector<std::unique_ptr<TableEntry<DefItem>>>       Profiles_Item;

public:
    ContentManager(asio::io_context& ioc);
    ~ContentManager();

};

}