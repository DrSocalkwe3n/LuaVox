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

// 16 метров ребро
// 256 вокселей ребро
struct Chunk {
    // Кубы вокселей в чанке
    std::vector<VoxelCube> Voxels;
    // Ноды
    std::array<Node, 16*16*16> Nodes;
    // Ограничения прохождения света, идущего от солнца (от верха карты до верхней плоскости чанка)
    // LightPrism Lights[16][16];
};

class Entity {
public:
    // PosQuat
    WorldId_t WorldId;
    // PortalId LastUsedPortal;
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
    virtual void onBinaryResourceAdd(std::vector<Hash_t>) = 0;

    virtual void onContentDefinesAdd(std::unordered_map<EnumDefContent, std::vector<ResourceId>>) = 0;
    virtual void onContentDefinesLost(std::unordered_map<EnumDefContent, std::vector<ResourceId>>) = 0;

    // Сообщаем об изменившихся чанках
    virtual void onChunksChange(WorldId_t worldId, const std::unordered_set<Pos::GlobalChunk> &changeOrAddList, const std::unordered_set<Pos::GlobalRegion> &remove) = 0;
    // Установить позицию для камеры
    virtual void setCameraPos(WorldId_t worldId, Pos::Object pos, glm::quat quat) = 0;

    virtual ~IRenderSession();
};

struct Region {
    std::array<Chunk, 4*4*4> Chunks;
};

struct World {
    
};

struct DefWorldInfo {

};

struct DefPortalInfo {

};

struct DefEntityInfo {

};

struct DefFuncEntityInfo {

};

struct WorldInfo {
    std::vector<EntityId_t> Entitys;
    std::vector<FuncEntityId_t> FuncEntitys;
    std::unordered_map<Pos::GlobalRegion, Region> Regions;
};

struct VoxelInfo {

};

struct NodeInfo {

};

struct PortalInfo {

};

struct EntityInfo {

};

struct FuncEntityInfo {

};

struct DefItemInfo {

};

struct DefVoxel_t {};
struct DefNode_t {};

/* Интерфейс обработчика сессии с сервером */
class IServerSession {
    struct ArrayHasher {
        std::size_t operator()(const Hash_t& a) const {
            std::size_t h = 0;
            for (auto e : a)
                h ^= std::hash<int>{}(e)  + 0x9e3779b9 + (h << 6) + (h >> 2);
            
            return h;
        }
    };

public:
    struct {
        std::unordered_map<Hash_t, BinaryResource, ArrayHasher>     Resources;
    } Binary;

    struct {
        std::unordered_map<DefVoxelId, DefVoxel_t>              DefVoxel;
        std::unordered_map<DefNodeId, DefNode_t>                  DefNode;
        std::unordered_map<DefWorldId, DefWorldInfo>              DefWorld;
        std::unordered_map<DefPortalId, DefPortalInfo>            DefPortal;
        std::unordered_map<DefEntityId, DefEntityInfo>            DefEntity;
        std::unordered_map<DefItemId, DefItemInfo>                DefItem;
    } Registry;

    struct {
        std::unordered_map<WorldId_t, WorldInfo>                    Worlds;
        // std::unordered_map<PortalId_t, PortalInfo>                  Portals;
        std::unordered_map<EntityId_t, EntityInfo>                  Entityes;
    } Data;

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
        Left, Right, Middle, One, Two
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