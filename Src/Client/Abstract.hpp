#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <Common/Abstract.hpp>


namespace LV::Client {

struct VoxelCube {
    DefVoxelId_c VoxelId;
    Pos::Local256 Left, Right;
};

struct Node {
    DefNodeId_c NodeId;
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

class Entity {
public:
    // PosQuat
    DefWorldId_c WorldId;
    DefPortalId_c LastUsedPortal;
    Pos::Object Pos;
    glm::quat Quat;
    static constexpr uint16_t HP_BS = 4096, HP_BS_Bit = 12;
    uint32_t HP = 0;

    // State
    std::unordered_map<std::string, float> Tags;
    // m_attached_particle_spawners
    // states
};

/* Интерфейс рендера текущего подключения к серверу */
class IRenderSession {
public:
    virtual void onDefTexture(TextureId_c id, std::vector<std::byte> &&info) = 0;
    virtual void onDefTextureLost(const std::vector<TextureId_c> &&lost) = 0;
    virtual void onDefModel(ModelId_c id, std::vector<std::byte> &&info) = 0;
    virtual void onDefModelLost(const std::vector<ModelId_c> &&lost) = 0;

    virtual void onDefWorldUpdates(const std::vector<DefWorldId_c> &updates) = 0;
    virtual void onDefVoxelUpdates(const std::vector<DefVoxelId_c> &updates) = 0;
    virtual void onDefNodeUpdates(const std::vector<DefNodeId_c> &updates) = 0;
    virtual void onDefPortalUpdates(const std::vector<DefPortalId_c> &updates) = 0;
    virtual void onDefEntityUpdates(const std::vector<DefEntityId_c> &updates) = 0;

    // Сообщаем об изменившихся чанках
    virtual void onChunksChange(WorldId_c worldId, const std::vector<Pos::GlobalChunk> &changeOrAddList, const std::vector<Pos::GlobalChunk> &remove) = 0;
    // Установить позицию для камеры
    virtual void setCameraPos(WorldId_c worldId, Pos::Object pos, glm::quat quat) = 0;

    virtual ~IRenderSession();
};


struct Region {
    std::unordered_map<Pos::Local4_u::Key, Chunk[4][4][4]> Subs;
};


struct World {
    std::vector<EntityId_c> Entitys;
    std::unordered_map<Pos::GlobalRegion::Key, Region> Regions;
};


struct DefWorldInfo {

};

struct DefPortalInfo {

};

struct DefEntityInfo {

};

struct WorldInfo {

};

struct VoxelInfo {

};

struct NodeInfo {

};

struct PortalInfo {

};

struct EntityInfo {

};

/* Интерфейс обработчика сессии с сервером */
class IServerSession {
public:
    struct {
        std::unordered_map<DefWorldId_c, DefWorldInfo>      DefWorlds;
        std::unordered_map<DefVoxelId_c, VoxelInfo>            DefVoxels;
        std::unordered_map<DefNodeId_c, NodeInfo>              DefNodes;
        std::unordered_map<DefPortalId_c, DefPortalInfo>    DefPortals;
        std::unordered_map<DefEntityId_c, DefEntityInfo>    DefEntityes;

        std::unordered_map<WorldId_c, WorldInfo>            Worlds;
        std::unordered_map<PortalId_c, PortalInfo>          Portals;
        std::unordered_map<EntityId_c, EntityInfo>          Entityes;
    } Registry;

    virtual ~IServerSession();

    virtual void atFreeDrawTime() = 0;
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