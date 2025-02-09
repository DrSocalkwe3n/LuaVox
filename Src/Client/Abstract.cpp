#include "Abstract.hpp"
#include "Common/Abstract.hpp"


namespace LV::Client {

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