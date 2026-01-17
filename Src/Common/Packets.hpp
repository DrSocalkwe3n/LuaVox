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
    ResourceRequest,
    ReloadMods
};

}

enum struct ToClient : uint8_t {
    Init,               // Первый пакет от сервера
    Disconnect,         // Отключаем клиента

    AssetsBindDK,       // Привязка AssetsId к домен+ключ
    AssetsBindHH,       // Привязка AssetsId к hash+header
    AssetsInitSend,     // Начало отправки запрошенного клиентом ресурса
    AssetsNextSend,     // Продолжение отправки ресурса

    DefinitionsFull,    // Полная информация о профилях контента
    DefinitionsUpdate,  // Обновление и потеря профилей контента (воксели, ноды, сущности, миры, ...)

    ChunkVoxels,        // Обновление вокселей чанка
    ChunkNodes,         // Обновление нод чанка
    ChunkLightPrism,    // 
    RemoveRegion,       // Удаление региона из зоны видимости

    Tick,               // Новые или потерянные игровые объекты (миры, сущности), динамичные данные такта (положение сущностей)

    TestLinkCameraToEntity, // Привязываем камеру к сущности
    TestUnlinkCamera,       // Отвязываем от сущности
};

}
