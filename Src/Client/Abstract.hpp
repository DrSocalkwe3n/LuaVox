#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <Common/Abstract.hpp>


namespace LV::Client {

using EntityId_t = uint16_t;
using FuncEntityId_t = uint16_t;

struct GlobalTime {
	uint32_t Seconds : 22 = 0, Sub : 10 = 0;

    GlobalTime() = default;
    GlobalTime(double gTime) {
		Seconds = int(gTime);
		Sub = (gTime-int(gTime))*1024;
    }

    GlobalTime& operator=(double gTime) {
		Seconds = int(gTime);
		Sub = (gTime-int(gTime))*1024;
        return *this;
    }

    operator double() const {
        return double(Seconds) + double(Sub)/1024.;
    }
};

struct VoxelCube {
    DefVoxelId_t VoxelId;
    Pos::bvec256u Left, Size;
};

struct Node {
    DefNodeId_t NodeId;
    uint8_t Rotate : 6;
};

// 16 метров ребро
// 256 вокселей ребро
struct Chunk {
    // Кубы вокселей в чанке
    std::vector<VoxelCube> Voxels;
    // Ноды
    std::unordered_map<Pos::bvec16u, Node> Nodes;
    // Ограничения прохождения света, идущего от солнца (от верха карты до верхней плоскости чанка)
    // LightPrism Lights[16][16];
};

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

/* Интерфейс рендера текущего подключения к серверу */
class IRenderSession {
public:
    // Подгрузка двоичных ресурсов
    virtual void onBinaryResourceAdd(std::unordered_map<EnumBinResource, std::unordered_map<ResourceId_t, BinaryResource>>) = 0;
    virtual void onBinaryResourceLost(std::unordered_map<EnumBinResource, std::vector<ResourceId_t>>) = 0;

    // Профили использования двоичных ресурсов
    // В этом месте нужно зарание распарсить
    virtual void onBinaryProfileAdd(std::unordered_map<EnumBinResource, std::unordered_map<ResourceId_t, std::u8string>>) = 0;
    virtual void onBinaryProfileLost(std::unordered_map<EnumBinResource, std::vector<ResourceId_t>>) = 0;

    virtual void onContentDefines(std::unordered_map<EnumDefContent, std::unordered_map<>>);
    EnumDefContent

    virtual void onDefWorldUpdates(const std::vector<DefWorldId_c> &updates) = 0;
    virtual void onDefVoxelUpdates(const std::vector<DefVoxelId_c> &updates) = 0;
    virtual void onDefNodeUpdates(const std::vector<DefNodeId_c> &updates) = 0;
    virtual void onDefPortalUpdates(const std::vector<DefPortalId_c> &updates) = 0;
    virtual void onDefEntityUpdates(const std::vector<DefEntityId_c> &updates) = 0;

    // Сообщаем об изменившихся чанках
    virtual void onChunksChange(WorldId_c worldId, const std::unordered_set<Pos::GlobalChunk> &changeOrAddList, const std::unordered_set<Pos::GlobalChunk> &remove) = 0;
    // Установить позицию для камеры
    virtual void setCameraPos(WorldId_c worldId, Pos::Object pos, glm::quat quat) = 0;

    virtual ~IRenderSession();
};


struct Region {
    std::unordered_map<Pos::Local16_u, Chunk> Chunks;
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
        std::unordered_map<DefVoxelId_c, VoxelInfo>         DefVoxels;
        std::unordered_map<DefNodeId_c, NodeInfo>           DefNodes;
        std::unordered_map<DefPortalId_c, DefPortalInfo>    DefPortals;
        std::unordered_map<DefEntityId_c, DefEntityInfo>    DefEntityes;

        std::unordered_map<WorldId_c, WorldInfo>            Worlds;
        std::unordered_map<PortalId_c, PortalInfo>          Portals;
        std::unordered_map<EntityId_c, EntityInfo>          Entityes;
    } Registry;

    struct {
        std::unordered_map<WorldId_c, World> Worlds;
    } External;

    virtual ~IServerSession();

    virtual void atFreeDrawTime(GlobalTime gTime, float dTime) = 0;
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