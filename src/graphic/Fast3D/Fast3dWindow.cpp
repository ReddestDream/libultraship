#include "Fast3dWindow.h"

#include "Context.h"
#include "public/bridge/consolevariablebridge.h"
#include "graphic/Fast3D/gfx_pc.h"
#include "graphic/Fast3D/gfx_sdl.h"
#include "graphic/Fast3D/gfx_dxgi.h"
#include "graphic/Fast3D/gfx_opengl.h"
#include "graphic/Fast3D/gfx_metal.h"
#include "graphic/Fast3D/gfx_direct3d11.h"
#include "graphic/Fast3D/gfx_direct3d12.h"
#include "graphic/Fast3D/gfx_pc.h"

#include <fstream>

namespace Fast {
Fast3dWindow::Fast3dWindow(std::shared_ptr<Ship::Gui> gui) : Ship::Window(gui) {
    mWindowManagerApi = nullptr;
    mRenderingApi = nullptr;

#ifdef _WIN32
    AddAvailableWindowBackend(Ship::WindowBackend::FAST3D_DXGI_DX11);
#endif
#ifdef __APPLE__
    if (Metal_IsSupported()) {
        AddAvailableWindowBackend(Ship::WindowBackend::FAST3D_SDL_METAL);
    }
#endif
    AddAvailableWindowBackend(Ship::WindowBackend::FAST3D_SDL_OPENGL);
}

Fast3dWindow::Fast3dWindow(std::vector<std::shared_ptr<Ship::GuiWindow>> guiWindows)
    : Fast3dWindow(std::make_shared<Ship::Gui>(guiWindows)) {
}

Fast3dWindow::Fast3dWindow() : Fast3dWindow(std::vector<std::shared_ptr<Ship::GuiWindow>>()) {
}

Fast3dWindow::~Fast3dWindow() {
    SPDLOG_DEBUG("destruct fast3dwindow");
    gfx_destroy();
}

void Fast3dWindow::Init() {
    bool gameMode = false;

#ifdef __linux__
    std::ifstream osReleaseFile("/etc/os-release");
    if (osReleaseFile.is_open()) {
        std::string line;
        while (std::getline(osReleaseFile, line)) {
            if (line.find("VARIANT_ID") != std::string::npos) {
                if (line.find("steamdeck") != std::string::npos) {
                    gameMode = std::getenv("XDG_CURRENT_DESKTOP") != nullptr &&
                               std::string(std::getenv("XDG_CURRENT_DESKTOP")) == "gamescope";
                }
                break;
            }
        }
    }
#elif defined(__ANDROID__) || defined(__IOS__)
    gameMode = true;
#endif

    bool isFullscreen;
    uint32_t width, height;
    int32_t posX, posY;

    isFullscreen = Ship::Context::GetInstance()->GetConfig()->GetBool("Window.Fullscreen.Enabled", false) || gameMode;
    posX = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.PositionX", 100);
    posY = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.PositionY", 100);

    if (isFullscreen) {
        width = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.Fullscreen.Width", gameMode ? 1280 : 1920);
        height = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.Fullscreen.Height", gameMode ? 800 : 1080);
    } else {
        width = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.Width", 640);
        height = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.Height", 480);
    }

    SetForceCursorVisibility(CVarGetInteger("gForceCursorVisibility", 0));

    InitWindowManager();

    gfx_init(mWindowManagerApi, mRenderingApi, Ship::Context::GetInstance()->GetName().c_str(), isFullscreen, width,
             height, posX, posY);
    mWindowManagerApi->set_fullscreen_changed_callback(OnFullscreenChanged);
    mWindowManagerApi->set_keyboard_callbacks(KeyDown, KeyUp, AllKeysUp);
    mWindowManagerApi->set_mouse_callbacks(MouseButtonDown, MouseButtonUp);

    SetTextureFilter((FilteringMode)CVarGetInteger(CVAR_TEXTURE_FILTER, FILTER_THREE_POINT));
}

void Fast3dWindow::SetTargetFps(int32_t fps) {
    gfx_set_target_fps(fps);
}

void Fast3dWindow::SetMaximumFrameLatency(int32_t latency) {
    gfx_set_maximum_frame_latency(latency);
}

void Fast3dWindow::GetPixelDepthPrepare(float x, float y) {
    gfx_get_pixel_depth_prepare(x, y);
}

uint16_t Fast3dWindow::GetPixelDepth(float x, float y) {
    return gfx_get_pixel_depth(x, y);
}

void Fast3dWindow::InitWindowManager() {
    SetWindowBackend(Ship::Context::GetInstance()->GetConfig()->GetWindowBackend());

    switch (GetWindowBackend()) {
#ifdef ENABLE_DX11
        case Ship::WindowBackend::FAST3D_DXGI_DX11:
            mRenderingApi = &gfx_direct3d11_api;
            mWindowManagerApi = &gfx_dxgi_api;
            break;
#endif
#ifdef ENABLE_OPENGL
        case Ship::WindowBackend::FAST3D_SDL_OPENGL:
            mRenderingApi = &gfx_opengl_api;
            mWindowManagerApi = &gfx_sdl;
            break;
#endif
#ifdef __APPLE__
        case Ship::WindowBackend::FAST3D_SDL_METAL:
            mRenderingApi = &gfx_metal_api;
            mWindowManagerApi = &gfx_sdl;
            break;
#endif
        default:
            SPDLOG_ERROR("Could not load the correct rendering backend");
            break;
    }
}

void Fast3dWindow::SetTextureFilter(FilteringMode filteringMode) {
    gfx_get_current_rendering_api()->set_texture_filter(filteringMode);
}

void Fast3dWindow::EnableSRGBMode() {
    gfx_get_current_rendering_api()->enable_srgb_mode();
}

void Fast3dWindow::SetRendererUCode(UcodeHandlers ucode) {
    gfx_set_target_ucode(ucode);
}

void Fast3dWindow::Close() {
    mWindowManagerApi->close();
}

void Fast3dWindow::StartFrame() {
    gfx_start_frame();
}

void Fast3dWindow::EndFrame() {
    gfx_end_frame();
}

bool Fast3dWindow::IsFrameReady() {
    return mWindowManagerApi->is_frame_ready();
}

bool Fast3dWindow::DrawAndRunGraphicsCommands(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtxReplacements) {
    std::shared_ptr<Window> wnd = Ship::Context::GetInstance()->GetWindow();

    // Skip dropped frames
    if (!wnd->IsFrameReady()) {
        return false;
    }

    auto gui = wnd->GetGui();
    // Setup of the backend frames and draw initial Window and GUI menus
    gui->StartDraw();
    // Setup game framebuffers to match available window space
    gfx_start_frame();
    // Execute the games gfx commands
    gfx_run(commands, mtxReplacements);
    // Renders the game frame buffer to the final window and finishes the GUI
    gui->EndDraw();
    // Finalize swap buffers
    gfx_end_frame();

    return true;
}

void Fast3dWindow::HandleEvents() {
    mWindowManagerApi->handle_events();
}

void Fast3dWindow::SetCursorVisibility(bool visible) {
    mWindowManagerApi->set_cursor_visibility(visible);
}

uint32_t Fast3dWindow::GetWidth() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->get_dimensions(&width, &height, &posX, &posY);
    return width;
}

uint32_t Fast3dWindow::GetHeight() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->get_dimensions(&width, &height, &posX, &posY);
    return height;
}

int32_t Fast3dWindow::GetPosX() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->get_dimensions(&width, &height, &posX, &posY);
    return posX;
}

int32_t Fast3dWindow::GetPosY() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->get_dimensions(&width, &height, &posX, &posY);
    return posY;
}

void Fast3dWindow::SetMousePos(Ship::Coords pos) {
    mWindowManagerApi->set_mouse_pos(pos.x, pos.y);
}

Ship::Coords Fast3dWindow::GetMousePos() {
    int32_t x, y;
    mWindowManagerApi->get_mouse_pos(&x, &y);
    return { x, y };
}

Ship::Coords Fast3dWindow::GetMouseDelta() {
    int32_t x, y;
    mWindowManagerApi->get_mouse_delta(&x, &y);
    return { x, y };
}

Ship::CoordsF Fast3dWindow::GetMouseWheel() {
    float x, y;
    mWindowManagerApi->get_mouse_wheel(&x, &y);
    return { x, y };
}

bool Fast3dWindow::GetMouseState(Ship::MouseBtn btn) {
    return mWindowManagerApi->get_mouse_state(static_cast<uint32_t>(btn));
}

void Fast3dWindow::SetMouseCapture(bool capture) {
    mWindowManagerApi->set_mouse_capture(capture);
}

bool Fast3dWindow::IsMouseCaptured() {
    return mWindowManagerApi->is_mouse_captured();
}

uint32_t Fast3dWindow::GetCurrentRefreshRate() {
    uint32_t refreshRate;
    mWindowManagerApi->get_active_window_refresh_rate(&refreshRate);
    return refreshRate;
}

bool Fast3dWindow::SupportsWindowedFullscreen() {
    if (GetWindowBackend() == Ship::WindowBackend::FAST3D_SDL_OPENGL ||
        GetWindowBackend() == Ship::WindowBackend::FAST3D_SDL_METAL) {
        return true;
    }

    return false;
}

bool Fast3dWindow::CanDisableVerticalSync() {
    return mWindowManagerApi->can_disable_vsync();
}

void Fast3dWindow::SetResolutionMultiplier(float multiplier) {
    gfx_current_dimensions.internal_mul = multiplier;
}

void Fast3dWindow::SetMsaaLevel(uint32_t value) {
    gfx_msaa_level = value;
}

void Fast3dWindow::SetFullscreen(bool isFullscreen) {
    SaveWindowToConfig();
    mWindowManagerApi->set_fullscreen(isFullscreen);
}

bool Fast3dWindow::IsFullscreen() {
    return mWindowManagerApi->is_fullscreen();
}

bool Fast3dWindow::IsRunning() {
    return mWindowManagerApi->is_running();
}

const char* Fast3dWindow::GetKeyName(int32_t scancode) {
    return mWindowManagerApi->get_key_name(scancode);
}

bool Fast3dWindow::KeyUp(int32_t scancode) {
    if (scancode ==
        Ship::Context::GetInstance()->GetConfig()->GetInt("Shortcuts.Fullscreen", Ship::KbScancode::LUS_KB_F11)) {
        Ship::Context::GetInstance()->GetWindow()->ToggleFullscreen();
    }

    if (scancode ==
        Ship::Context::GetInstance()->GetConfig()->GetInt("Shortcuts.MouseCapture", Ship::KbScancode::LUS_KB_F2)) {
        bool captureState = Ship::Context::GetInstance()->GetWindow()->IsMouseCaptured();
        Ship::Context::GetInstance()->GetWindow()->SetMouseCapture(!captureState);
    }

    Ship::Context::GetInstance()->GetWindow()->SetLastScancode(-1);
    return Ship::Context::GetInstance()->GetControlDeck()->ProcessKeyboardEvent(
        Ship::KbEventType::LUS_KB_EVENT_KEY_UP, static_cast<Ship::KbScancode>(scancode));
}

bool Fast3dWindow::KeyDown(int32_t scancode) {
    bool isProcessed = Ship::Context::GetInstance()->GetControlDeck()->ProcessKeyboardEvent(
        Ship::KbEventType::LUS_KB_EVENT_KEY_DOWN, static_cast<Ship::KbScancode>(scancode));
    Ship::Context::GetInstance()->GetWindow()->SetLastScancode(scancode);

    return isProcessed;
}

void Fast3dWindow::AllKeysUp() {
    Ship::Context::GetInstance()->GetControlDeck()->ProcessKeyboardEvent(Ship::KbEventType::LUS_KB_EVENT_ALL_KEYS_UP,
                                                                         Ship::KbScancode::LUS_KB_UNKNOWN);
}

bool Fast3dWindow::MouseButtonUp(int button) {
    return Ship::Context::GetInstance()->GetControlDeck()->ProcessMouseButtonEvent(false,
                                                                                   static_cast<Ship::MouseBtn>(button));
}

bool Fast3dWindow::MouseButtonDown(int button) {
    bool isProcessed = Ship::Context::GetInstance()->GetControlDeck()->ProcessMouseButtonEvent(
        true, static_cast<Ship::MouseBtn>(button));
    return isProcessed;
}

void Fast3dWindow::OnFullscreenChanged(bool isNowFullscreen) {
    std::shared_ptr<Window> wnd = Ship::Context::GetInstance()->GetWindow();

    if (isNowFullscreen) {
        auto menuBar = wnd->GetGui()->GetMenuBar();
        wnd->SetMouseCapture(!(menuBar && menuBar->IsVisible() || wnd->ShouldForceCursorVisibility() ||
                               CVarGetInteger("gWindows.Menu", 0)));
    } else {
        wnd->SetMouseCapture(false);
    }
}
} // namespace Fast
