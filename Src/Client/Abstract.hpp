#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <Common/Abstract.hpp>


namespace AL::Client {

using VoxelId_t = uint16_t;

struct VoxelCube {
    Pos::Local256 Left, Right;
    VoxelId_t Material;
};

using NodeId_t = uint16_t;

struct Node {
    NodeId_t NodeId;
    uint8_t Rotate : 6;
};

// 16 метров ребро
// 256 вокселей ребро
struct Chunk {
    // Кубы вокселей в чанке
    std::vector<VoxelCube> Voxels;
    // Ноды
    std::unordered_map<Pos::Local16_u, Node> Nodes;
    // Ограничения прохождения света, идущего от солнца (от верха карты до верхней плоскости чанка)
    LightPrism Lights[16][16];
};

using WorldId_t = uint8_t;
using PortalId_t = uint16_t;
using EntityId_t = uint16_t;

class Entity {
public:
    // PosQuat
    WorldId_t WorldId;
    PortalId_t LastUsedPortal;
    Pos::Object Pos;
    glm::quat Quat;
    static constexpr uint16_t HP_BS = 4096, HP_BS_Bit = 12;
    uint32_t HP = 0;

    // State
    std::unordered_map<std::string, float> Tags;
    // m_attached_particle_spawners
    // states
};

using TextureId_t = uint16_t;

struct TextureInfo {

};

using ModelId_t = uint16_t;

struct ModelInfo {

};

/* Интерфейс рендера текущего подключения к серверу */
class IRenderSession {
public:
    // Сообщаем об изменившихся чанках
    virtual void onChunksChange(WorldId_t worldId, const std::vector<Pos::GlobalChunk> &changeOrAddList, const std::vector<Pos::GlobalChunk> &remove);
    // Подключаем камеру к сущности
    virtual void attachCameraToEntity(EntityId_t id);

    // 

    // Мир уже есть в глобальном списке
    virtual void onWorldAdd(WorldId_t id);
    // Мира уже нет в списке
    virtual void onWorldRemove(WorldId_t id);
    // Изменение состояния мира
    virtual void onWorldChange(WorldId_t id);

    // Сущности уже есть в глобальном списке
    virtual void onEntitysAdd(const std::vector<EntityId_t> &list);
    //
    virtual void onEntitysRemove(const std::vector<EntityId_t> &list);
    //
    virtual void onEntitysPosQuatChanges(const std::vector<EntityId_t> &list);
    //
    virtual void onEntitysStateChanges(const std::vector<EntityId_t> &list);

    virtual TextureId_t allocateTexture();
    virtual void freeTexture(TextureId_t id);
    virtual void setTexture(TextureId_t id, TextureInfo info);

    virtual ModelId_t allocateModel();
    virtual void freeModel(ModelId_t id);
    virtual void setModel(ModelId_t id, ModelInfo info);

    virtual ~IRenderSession();
};


struct Region {
    std::unordered_map<Pos::Local4_u::Key, Chunk[4][4][4]> Subs;
};


struct World {
    std::vector<EntityId_t> Entitys;
    std::unordered_map<Pos::GlobalRegion::Key, Region> Regions;
};


class ChunksIterator {
public:
};

struct VoxelInfo {

};

struct NodeInfo {

};

/* Интерфейс обработчика сессии с сервером */
class IServerSession {
public:
    std::unordered_map<EntityId_t, Entity> Entitys;
    std::unordered_map<WorldId_t, World> Worlds;
    std::unordered_map<VoxelId_t, VoxelInfo> VoxelRegistry;
    std::unordered_map<NodeId_t, NodeInfo> NodeRegistry;

    virtual ~IServerSession();
};


/* Интерфейс получателя событий от модуля вывода графики в ОС */
class ISurfaceEventListener {
public:
    enum struct EnumCursorMoveMode {
        Default, MoveAndHidden
    } CursorMode = EnumCursorMoveMode::Default;

    enum struct EnumCursorBtn {
        Left, Middle, Right, One, Two
    };

public:
    // Изменение размера окна вывода графики
    virtual void onResize(uint32_t width, uint32_t height);
    // Приобретение или потеря фокуса приложением
    virtual void onChangeFocusState(bool isFocused);
    // Абсолютное изменение позиции курсора (вызывается только если CursorMode == EnumCursorMoveMode::Default)
    virtual void onCursorPosChange(int32_t width, int32_t height);
    // Относительное перемещение курсора (вызывается только если CursorMode == EnumCursorMoveMode::MoveAndHidden)
    virtual void onCursorMove(float xMove, float yMove);
    // Когда на GPU отправлены команды на отрисовку мира и мир рисуется
    virtual void onFrameRendering(); // Здесь пока неизвестно что можно делать, но тут есть свободное время
    // Когда GPU завершил кадр
    virtual void onFrameRenderEnd(); // Изменять игровые данные можно только здесь

    virtual void onCursorBtn(EnumCursorBtn btn, bool state);
    virtual void onKeyboardBtn(int btn, int state);
    virtual void onJoystick();

    virtual ~ISurfaceEventListener();
};

}