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
    // Объект уведомления об изменениях
    struct TickSyncData {
        // Новые или изменённые используемые теперь двоичные ресурсы
        std::unordered_map<EnumAssets, std::vector<ResourceId>> Assets_ChangeOrAdd;
        // Более не используемые ресурсы
        std::unordered_map<EnumAssets, std::vector<ResourceId>> Assets_Lost;

        // Новые или изменённые профили контента 
        std::unordered_map<EnumDefContent, std::vector<ResourceId>> Profiles_ChangeOrAdd;
        // Более не используемые профили
        std::unordered_map<EnumDefContent, std::vector<ResourceId>> Profiles_Lost;

        // Новые или изменённые чанки
        std::unordered_map<WorldId_t, std::vector<Pos::GlobalChunk>> Chunks_ChangeOrAdd;
        // Более не отслеживаемые регионы
        std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> Chunks_Lost;
    };

public:
    // Серверная сессия собирается обработать данные такток сервера (изменение профилей, ресурсов, прочих игровых данных)
    virtual void prepareTickSync() = 0;
    // Началась стадия изменения данных IServerSession, все должны приостановить работу
    virtual void pushStageTickSync() = 0;
    // После изменения внутренних данных IServerSession, IRenderSession уведомляется об изменениях
    virtual void tickSync(const TickSyncData& data) = 0;

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
    DefEntityId DefId = 0;
    WorldId_t WorldId = 0;
    Pos::Object Pos = Pos::Object(0);
    glm::quat Quat = glm::quat(1.f, 0.f, 0.f, 0.f);
};

struct FuncEntityInfo {

};

struct DefItemInfo {

};

struct DefVoxel_t {};
struct DefNode_t {
    AssetsNodestate NodestateId = 0;
    AssetsTexture TexId = 0;

};

struct AssetEntry {
    EnumAssets Type;
    ResourceId Id;
    std::string Domain, Key;
    Resource Res;
    Hash_t Hash = {};
    std::vector<uint8_t> Dependencies;
};

/* 
    Интерфейс обработчика сессии с сервером.

    Данный здесь меняются только меж вызовами 
    IRenderSession::pushStageTickSync
    и
    IRenderSession::tickSync
*/
class IServerSession {
public:
    // Включить логирование входящих сетевых пакетов на клиенте.
    bool DebugLogPackets = false;

    // Используемые двоичные ресурсы
    std::unordered_map<EnumAssets, std::unordered_map<ResourceId, AssetEntry>> Assets;

    // Используемые профили контента
    struct {
        std::unordered_map<DefVoxelId, DefVoxel_t>                DefVoxel;
        std::unordered_map<DefNodeId, DefNode_t>                  DefNode;
        std::unordered_map<DefWorldId, DefWorldInfo>              DefWorld;
        std::unordered_map<DefPortalId, DefPortalInfo>            DefPortal;
        std::unordered_map<DefEntityId, DefEntityInfo>            DefEntity;
        std::unordered_map<DefItemId, DefItemInfo>                DefItem;
    } Profiles;

    // Видимый контент
    struct {
        std::unordered_map<WorldId_t, WorldInfo>                    Worlds;
        // std::unordered_map<PortalId_t, PortalInfo>                  Portals;
        std::unordered_map<EntityId_t, EntityInfo>                  Entityes;
    } Content;

    virtual ~IServerSession();

    // Обновление сессии с сервером, может начатся стадия IRenderSession::tickSync
    virtual void update(GlobalTime gTime, float dTime) = 0;
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
