#include "Abstract.hpp"


namespace LV::Client {

    void IRenderSession::onChunksChange(WorldId_t worldId, const std::vector<Pos::GlobalChunk> &changeOrAddList, const std::vector<Pos::GlobalChunk> &remove) {}
    void IRenderSession::attachCameraToEntity(EntityId_t id) {}
    void IRenderSession::onWorldAdd(WorldId_t id) {}
    void IRenderSession::onWorldRemove(WorldId_t id) {}
    void IRenderSession::onWorldChange(WorldId_t id) {}
    void IRenderSession::onEntitysAdd(const std::vector<EntityId_t> &list) {}
    void IRenderSession::onEntitysRemove(const std::vector<EntityId_t> &list) {}
    void IRenderSession::onEntitysPosQuatChanges(const std::vector<EntityId_t> &list) {}
    void IRenderSession::onEntitysStateChanges(const std::vector<EntityId_t> &list) {}
    TextureId_t IRenderSession::allocateTexture() { return 0; }
    void IRenderSession::freeTexture(TextureId_t id) {}
    void IRenderSession::setTexture(TextureId_t id, TextureInfo info) {}
    ModelId_t IRenderSession::allocateModel() { return 0; }
    void IRenderSession::freeModel(ModelId_t id) {}
    void IRenderSession::setModel(ModelId_t id, ModelInfo info) {}
    IRenderSession::~IRenderSession() = default;

    IServerSession::~IServerSession() = default;

    void ISurfaceEventListener::onResize(uint32_t width, uint32_t height) {}
    void ISurfaceEventListener::onChangeFocusState(bool isFocused) {}
    void ISurfaceEventListener::onCursorPosChange(int32_t width, int32_t height) {}
    void ISurfaceEventListener::onCursorMove(float xMove, float yMove) {}
    void ISurfaceEventListener::onFrameRendering() {}
    void ISurfaceEventListener::onFrameRenderEnd() {}
    void ISurfaceEventListener::onCursorBtn(EnumCursorBtn btn, bool state) {}
    void ISurfaceEventListener::onKeyboardBtn(int btn, int state) {}
    void ISurfaceEventListener::onJoystick() {}
    ISurfaceEventListener::~ISurfaceEventListener() = default;

}