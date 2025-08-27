#pragma once

#include <cstdint>
#include <glm/ext/quaternion_float.hpp>


namespace LV {

enum struct EnumDisconnect {
    ByInterface,
    CriticalError,
    ProtocolError
};

namespace ToServer {
    
struct PacketQuat {
    uint8_t Data[5];

    void fromQuat(const glm::quat &quat) {
        uint16_t 
            x = (quat.x+1)/2*0x3ff,
            y = (quat.y+1)/2*0x3ff,
            z = (quat.z+1)/2*0x3ff,
            w = (quat.w+1)/2*0x3ff;

        uint64_t value = 0;

        value |= x & 0x3ff;
        value |= uint64_t(y & 0x3ff) << 10;
        value |= uint64_t(z & 0x3ff) << 20;
        value |= uint64_t(w & 0x3ff) << 30;

        for(int iter = 0; iter < 5; iter++)
            Data[iter] = (value >> (iter*8)) & 0xff;
    }

    glm::quat toQuat() const {
        uint64_t value = 0;

        for(int iter = 0; iter < 5; iter++)
            value |= uint64_t(Data[iter]) << (iter*8);

        uint16_t
            x = value & 0x3ff,
            y = (value >> 10) & 0x3ff,
            z = (value >> 20) & 0x3ff,
            w = (value >> 30) & 0x3ff;

        float fx = (float(x)/0x3ff)*2-1;
        float fy = (float(y)/0x3ff)*2-1;
        float fz = (float(z)/0x3ff)*2-1;
        float fw = (float(w)/0x3ff)*2-1;

        return glm::quat(fw, fx, fy, fz);
    }
};

/*
    uint8_t+uint8_t
    0 - Системное
        0 -
        1 -
        2 - Новая позиция камеры WorldId_c+ObjectPos+PacketQuat
        3 - Изменение блока

*/

// Первый уровень
enum struct L1 : uint8_t {
    System,
};

// Второй уровень
enum struct L2System : uint8_t {
    InitEnd,
    Disconnect,
    Test_CAM_PYR_POS,
    BlockChange,
    ResourceRequest
};

}

namespace ToClient {

/*
    uint8_t+uint8_t
    0 - Системное
        0 - Инициализация           WorldId_c+ObjectPos
        1 - Отключение от сервера   String(Причина)
        2 - Привязка камеры к сущности  EntityId_c
        3 - Отвязка камеры
    1 - Оповещение о доступном ресурсе
        0 - Текстура                TextureId_c+Hash
        1 - Освобождение текстуры   TextureId_c
        2 - Звук                    SoundId_c+Hash
        3 - Освобождение звука      SoundId_c
        4 - Модель                  ModelId_c+Hash
        5 - Освобождение модели     ModelId_c
        253 - Инициирование передачи ресурса    StreamId+ResType+ResId+Size+Hash
        254 - Передача чанка данных             StreamId+Size+Data
        255 - Передача отменена                 StreamId
    2 - Новые определения
        0 - Мир                     DefWorldId_c+определение
        1 - Освобождение мира       DefWorldId_c
        2 - Воксель                 DefVoxelId_c+определение
        3 - Освобождение вокселя    DefVoxelId_c
        4 - Нода                    DefNodeId_c+определение
        5 - Освобождение ноды       DefNodeId_c
        6 - Портал                  DefPortalId_c+определение
        7 - Освобождение портала    DefPortalId_c
        8 - Сущность                DefEntityId_c+определение
        9 - Освобождение сущности   DefEntityId_c
    3 - Новый контент
        0 - Мир, новый/изменён      WorldId_c+...
        1 - Мир/Удалён              WorldId_c
        2 - Портал, новый/изменён   PortalId_c+...
        3 - Портал/Удалён           PortalId_c
        4 - Сущность, новый/изменён EntityId_c+...
        5 - Сущность/Удалёна        EntityId_c
        6 - Чанк/Воксели            WorldId_c+GlobalChunk+...
        7 - Чанк/Ноды               WorldId_c+GlobalChunk+...
        8 - Чанк/Призмы освещения   WorldId_c+GlobalChunk+...
        9 - Чанк/Удалён             WorldId_c+GlobalChunk



*/

// Первый уровень
enum struct L1 : uint8_t {
    System,
    Resource,
    Definition,
    Content
};

// Второй уровень
enum struct L2System : uint8_t {
    Init,
    Disconnect,
    LinkCameraToEntity,
    UnlinkCamera,
    SyncTick
};

enum struct L2Resource : uint8_t {
    Bind,   // Привязка идентификаторов ресурсов к хешам
    Lost,
    InitResSend = 253,
    ChunkSend
};

enum struct L2Definition : uint8_t {
    World,
    FreeWorld,
    Voxel,
    FreeVoxel,
    Node,
    FreeNode,
    Portal,
    FreePortal,
    Entity,
    FreeEntity,
    FuncEntity,
    FreeFuncEntity,
    Item,
    FreeItem
};

enum struct L2Content : uint8_t {
    World,
    RemoveWorld,
    Portal,
    RemovePortal,
    Entity,
    RemoveEntity,
    ChunkVoxels,
    ChunkNodes,
    ChunkLightPrism,
    RemoveRegion
};

}

}