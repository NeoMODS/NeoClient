#include "platform/overlay_gl.h"

#include "core/client.h"
#include "core/logger.h"
#include "game/jni_game.h"
#include "ui/fonts.h"
#include "ui/theme.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#include "MinHook.h"

#include <windows.h>
#include <GL/gl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <iterator>
#include <memory>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#ifndef GL_FRAMEBUFFER_SRGB
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#endif

namespace neo::platform {
namespace {

using PresentFn = BOOL(WINAPI*)(HDC);
using SteadyClock = std::chrono::steady_clock;

PresentFn g_originalSwapBuffers = nullptr;
PresentFn g_originalWglSwapBuffers = nullptr;
void* g_swapBuffersTarget = nullptr;
void* g_wglSwapBuffersTarget = nullptr;
std::atomic<int> g_activeHookCalls{0};
// Guards against the same frame being presented twice by the nested
// wglSwapBuffers -> SwapBuffers forwarding. A time-based guard would skip real
// presents at high (idle) frame rates and make the whole UI blink.
thread_local bool g_swapInProgress = false;

BOOL WINAPI swapBuffersHook(HDC hdc);
BOOL WINAPI wglSwapBuffersHook(HDC hdc);
LRESULT CALLBACK overlayWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

class Overlay {
public:
    bool install();
    void requestUnload();
    void finalize();

    bool unloadRequested() const { return unloadRequested_.load(); }
    bool safeToUnload() const { return safeToUnload_.load(); }
    bool installed() const { return installed_; }

    void present(HDC hdc);
    LRESULT windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

private:
    bool initialize(HWND hwnd);
    void rebindWindow(HWND hwnd);
    void renderFrame(SteadyClock::time_point now);
    void restoreWindowProc();
    void cleanupRenderThread();
    void updateCursor();
    void pollFallbackKeys();

    HWND hwnd_ = nullptr;
    WNDPROC originalWndProc_ = nullptr;
    bool windowProcInstalled_ = false;

    ImGuiContext* context_ = nullptr;
    std::unique_ptr<Client> client_;
    JniGame game_;

    bool installed_ = false;
    bool imguiReady_ = false;
    bool renderingDisabled_ = false;
    bool refreshedThisFrame_ = false;

    SteadyClock::time_point lastPresent_{};
    SteadyClock::time_point lastFrameTime_{};
    SteadyClock::time_point lastFallbackKey_{};
    int fallbackKeyState_ = 0;

    bool cursorReleased_ = false;

    std::atomic<bool> unloadRequested_{false};
    std::atomic<bool> safeToUnload_{false};
};

Overlay g_overlay;

HWND rootWindow(HDC dc) {
    HWND window = WindowFromDC(dc);
    if (!window) {
        return nullptr;
    }
    HWND root = GetAncestor(window, GA_ROOT);
    return root ? root : window;
}

HWND findMinecraftWindow() {
    struct Search {
        HWND result = nullptr;
    } search;

    EnumWindows([](HWND window, LPARAM parameter) -> BOOL {
        auto& search = *reinterpret_cast<Search*>(parameter);
        if (!IsWindowVisible(window)) {
            return TRUE;
        }

        wchar_t title[256]{};
        GetWindowTextW(window, title, static_cast<int>(std::size(title)));
        if (std::wcsstr(title, L"Minecraft") == nullptr) {
            return TRUE;
        }

        RECT rect{};
        GetClientRect(window, &rect);
        if (rect.right < 320 || rect.bottom < 240) {
            return TRUE;
        }

        search.result = window;
        return FALSE;
    }, reinterpret_cast<LPARAM>(&search));

    return search.result;
}

float windowScale(HWND hwnd) {
    RECT rect{};
    GetClientRect(hwnd, &rect);
    const float height = static_cast<float>(std::max<LONG>(rect.bottom, 1));
    return std::clamp(height / 1080.0f, 0.80f, 1.75f);
}

bool isMouseMessage(UINT message) {
    switch (message) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        return true;
    default:
        return false;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Overlay lifecycle
// ---------------------------------------------------------------------------

bool Overlay::install() {
    if (installed_) {
        return true;
    }

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        NEO_LOG_ERROR("MinHook initialization failed (%d)", static_cast<int>(status));
        return false;
    }

    HMODULE opengl = GetModuleHandleW(L"opengl32.dll");
    HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    if (!gdi) {
        gdi = LoadLibraryW(L"gdi32.dll");
    }

    int hooked = 0;

    if (gdi) {
        g_swapBuffersTarget = reinterpret_cast<void*>(GetProcAddress(gdi, "SwapBuffers"));
        if (g_swapBuffersTarget) {
            status = MH_CreateHook(g_swapBuffersTarget, reinterpret_cast<LPVOID>(&swapBuffersHook),
                reinterpret_cast<LPVOID*>(&g_originalSwapBuffers));
            if (status == MH_OK) {
                ++hooked;
            } else {
                NEO_LOG_WARN("Could not hook SwapBuffers (%d)", static_cast<int>(status));
            }
        }
    }

    if (opengl) {
        g_wglSwapBuffersTarget = reinterpret_cast<void*>(GetProcAddress(opengl, "wglSwapBuffers"));
        if (g_wglSwapBuffersTarget) {
            status = MH_CreateHook(g_wglSwapBuffersTarget, reinterpret_cast<LPVOID>(&wglSwapBuffersHook),
                reinterpret_cast<LPVOID*>(&g_originalWglSwapBuffers));
            if (status == MH_OK) {
                ++hooked;
            } else {
                NEO_LOG_WARN("Could not hook wglSwapBuffers (%d)", static_cast<int>(status));
            }
        }
    }

    if (hooked == 0) {
        NEO_LOG_ERROR("Neither SwapBuffers nor wglSwapBuffers could be hooked");
        MH_Uninitialize();
        return false;
    }

    status = MH_EnableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
        NEO_LOG_ERROR("Could not enable present hooks (%d)", static_cast<int>(status));
        MH_Uninitialize();
        return false;
    }

    installed_ = true;
    NEO_LOG_INFO("Present hooks installed (%d)", hooked);
    return true;
}

void Overlay::requestUnload() {
    unloadRequested_.store(true);
}

void Overlay::finalize() {
    if (!installed_) {
        return;
    }

    // Stop calling into the hook functions before restoring the prologues.
    MH_DisableHook(MH_ALL_HOOKS);

    for (int i = 0; i < 200 && g_activeHookCalls.load() > 0; ++i) {
        Sleep(5);
    }

    if (g_swapBuffersTarget) {
        MH_RemoveHook(g_swapBuffersTarget);
        g_swapBuffersTarget = nullptr;
    }
    if (g_wglSwapBuffersTarget) {
        MH_RemoveHook(g_wglSwapBuffersTarget);
        g_wglSwapBuffersTarget = nullptr;
    }

    MH_Uninitialize();
    installed_ = false;
}

bool Overlay::initialize(HWND hwnd) {
    if (!hwnd || !wglGetCurrentContext()) {
        return false;
    }

    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    if (!context_) {
        NEO_LOG_ERROR("Could not create the ImGui context");
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    const float scale = windowScale(hwnd);
    ui::applyStyle(scale);
    ui::loadFonts(scale);

    if (!ImGui_ImplWin32_InitForOpenGL(hwnd)) {
        NEO_LOG_ERROR("ImGui Win32 backend initialization failed");
        ImGui::DestroyContext(context_);
        context_ = nullptr;
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 150")) {
        NEO_LOG_ERROR("ImGui OpenGL backend initialization failed");
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(context_);
        context_ = nullptr;
        return false;
    }

    // Enable VSync so the overlay caps at the monitor refresh rate.
    using SwapIntervalFn = BOOL(WINAPI*)(int);
    auto swapInterval = reinterpret_cast<SwapIntervalFn>(wglGetProcAddress("wglSwapIntervalEXT"));
    if (swapInterval) {
        swapInterval(1);
        NEO_LOG_INFO("VSync enabled");
    } else {
        NEO_LOG_WARN("wglSwapIntervalEXT not available; overlay will run unlocked");
    }

    hwnd_ = hwnd;
    SetLastError(0);
    const LONG_PTR previous = SetWindowLongPtrW(hwnd_, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&overlayWindowProc));
    if (previous != 0 || GetLastError() == 0) {
        originalWndProc_ = reinterpret_cast<WNDPROC>(previous);
        windowProcInstalled_ = true;
    } else {
        NEO_LOG_WARN("Could not subclass the Minecraft window; menu key polling will be used");
    }

    client_ = std::make_unique<Client>(game_);
    client_->initialize();
    client_->setUnloadCallback([]() {
        g_overlay.requestUnload();
    });

    imguiReady_ = true;
    lastPresent_ = SteadyClock::now();
    lastFrameTime_ = SteadyClock::now();

    NEO_LOG_INFO("Overlay ready on window 0x%p (scale %.2f)", hwnd_, scale);
    return true;
}

void Overlay::rebindWindow(HWND hwnd) {
    if (!hwnd || hwnd == hwnd_) {
        return;
    }

    restoreWindowProc();
    ImGui_ImplWin32_Shutdown();
    if (!ImGui_ImplWin32_InitForOpenGL(hwnd)) {
        NEO_LOG_ERROR("Could not reinitialize the ImGui window backend");
        return;
    }

    hwnd_ = hwnd;
    SetLastError(0);
    const LONG_PTR previous = SetWindowLongPtrW(hwnd_, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&overlayWindowProc));
    if (previous != 0 || GetLastError() == 0) {
        originalWndProc_ = reinterpret_cast<WNDPROC>(previous);
        windowProcInstalled_ = true;
    }

    cursorReleased_ = false;

    NEO_LOG_INFO("Overlay attached to a new game window 0x%p", hwnd_);
}

void Overlay::restoreWindowProc() {
    if (windowProcInstalled_ && hwnd_ && originalWndProc_) {
        SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWndProc_));
    }
    windowProcInstalled_ = false;
    originalWndProc_ = nullptr;
}

void Overlay::updateCursor() {
    if (!client_) {
        return;
    }

    // The actual mouse grab is released through JNI in Client::frame, using
    // Minecraft's own MouseHandler. Here we only make sure the window does not
    // keep a clip rectangle that would trap the cursor behind the menu.
    const bool open = client_->menuOpen();
    if (open) {
        ClipCursor(nullptr);
        if (!cursorReleased_) {
            cursorReleased_ = true;
            NEO_LOG_DEBUG("Cursor released for the menu");
        }
    } else if (cursorReleased_) {
        ClipCursor(nullptr);
        cursorReleased_ = false;
        NEO_LOG_DEBUG("Cursor returned to the game");
    }
}

void Overlay::pollFallbackKeys() {
    if (!client_) {
        return;
    }

    const auto now = SteadyClock::now();
    if (lastFallbackKey_ != SteadyClock::time_point{} &&
        now - lastFallbackKey_ < std::chrono::milliseconds(16)) {
        return;
    }
    lastFallbackKey_ = now;

    const bool down = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0 ||
                      (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (down && fallbackKeyState_ == 0) {
        const HWND foreground = GetForegroundWindow();
        if (foreground == hwnd_ || (foreground && IsChild(hwnd_, foreground))) {
            client_->toggleMenu();
        }
    }
    fallbackKeyState_ = down ? 1 : 0;
}

void Overlay::renderFrame(SteadyClock::time_point now) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();

    float dt = std::chrono::duration<float>(now - lastFrameTime_).count();
    dt = std::clamp(dt, 0.0f, 0.1f);
    lastFrameTime_ = now;

    // Local JNI references only live for the current native call, so the game
    // state has to be refreshed every frame before modules run. Throttling
    // this left Player/Level references dangling on skipped frames.
    game_.refresh();
    refreshedThisFrame_ = true;

    updateCursor();
    if (!windowProcInstalled_) {
        pollFallbackKeys();
    }

    ImGuiIO& io = ImGui::GetIO();
    // Always draw the software cursor while the menu is open. Some focus and
    // fullscreen transitions hide the OS cursor again, and the menu must stay
    // usable through them.
    io.MouseDrawCursor = client_->menuOpen();

    ImGui::NewFrame();
    client_->setGameWindow(hwnd_);
    client_->frame(dt);
    ImGui::Render();

    ImDrawData* data = ImGui::GetDrawData();
    if (data && data->DisplaySize.x > 0.0f && data->DisplaySize.y > 0.0f) {
        const GLboolean srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
        if (srgb) {
            glDisable(GL_FRAMEBUFFER_SRGB);
        }

        glViewport(0, 0, static_cast<GLsizei>(data->DisplaySize.x), static_cast<GLsizei>(data->DisplaySize.y));
        ImGui_ImplOpenGL3_RenderDrawData(data);

        if (srgb) {
            glEnable(GL_FRAMEBUFFER_SRGB);
        }
    }

    if (refreshedThisFrame_) {
        game_.endFrame();
    }

    if (unloadRequested_.load()) {
        cleanupRenderThread();
    }
}

void Overlay::cleanupRenderThread() {
    if (renderingDisabled_) {
        return;
    }
    renderingDisabled_ = true;

    restoreWindowProc();
    if (client_) {
        client_->setMenuOpen(false);
    }
    client_.reset();

    if (imguiReady_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(context_);
        context_ = nullptr;
        imguiReady_ = false;
    }

    safeToUnload_.store(true);
    NEO_LOG_INFO("Overlay stopped");
}

void Overlay::present(HDC hdc) {
    // A nested present (wglSwapBuffers forwarding into SwapBuffers) must not
    // draw the UI a second time. Non-nested presents always draw, so the
    // overlay is composited on every frame even at very high frame rates.
    if (g_swapInProgress) {
        return;
    }
    g_swapInProgress = true;
    struct SwapScopeGuard {
        ~SwapScopeGuard() { g_swapInProgress = false; }
    } swapScopeGuard;

    if (renderingDisabled_) {
        return;
    }

    if (unloadRequested_.load()) {
        cleanupRenderThread();
        return;
    }

    if (!imguiReady_) {
        HWND hwnd = rootWindow(hdc);
        if (!hwnd) {
            hwnd = findMinecraftWindow();
        }
        if (!hwnd) {
            return;
        }
        if (!initialize(hwnd)) {
            return;
        }
    }

    HWND current = rootWindow(hdc);
    if (current && current != hwnd_) {
        rebindWindow(current);
    }

    RECT rect{};
    GetClientRect(hwnd_, &rect);
    if (rect.right <= 0 || rect.bottom <= 0 || IsIconic(hwnd_)) {
        return;
    }

    const auto now = SteadyClock::now();
    lastPresent_ = now;

    renderFrame(now);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

LRESULT Overlay::windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    const bool menuOpen = client_ && client_->menuOpen();

    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
        const bool repeat = (lparam & 0x40000000) != 0;
        bool consumed = false;
        if (!repeat && client_) {
            consumed = client_->keyDown(static_cast<int>(wparam));
        }

        if (client_ && client_->menuOpen()) {
            ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam);
            return 0;
        }
        if (consumed) {
            return 0;
        }
    } else if (message == WM_KEYUP || message == WM_SYSKEYUP) {
        if (menuOpen) {
            ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam);
            return 0;
        }
    } else if (message == WM_CHAR || message == WM_SYSCHAR || message == WM_UNICHAR) {
        if (menuOpen) {
            ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam);
            return 0;
        }
    } else if (isMouseMessage(message)) {
        if (menuOpen) {
            ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam);
            return 0;
        }
    } else if (message == WM_INPUT) {
        if (menuOpen) {
            // Raw mouse input would still move the camera behind the menu.
            return 0;
        }
    } else if (message == WM_SETCURSOR) {
        if (menuOpen) {
            return TRUE;
        }
    }

    if (originalWndProc_) {
        return CallWindowProcW(originalWndProc_, hwnd, message, wparam, lparam);
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

// ---------------------------------------------------------------------------
// Hook entry points
// ---------------------------------------------------------------------------

namespace {

BOOL WINAPI swapBuffersHook(HDC hdc) {
    g_activeHookCalls.fetch_add(1, std::memory_order_relaxed);
    g_overlay.present(hdc);
    const BOOL result = g_originalSwapBuffers ? g_originalSwapBuffers(hdc) : FALSE;
    g_activeHookCalls.fetch_sub(1, std::memory_order_relaxed);
    return result;
}

BOOL WINAPI wglSwapBuffersHook(HDC hdc) {
    g_activeHookCalls.fetch_add(1, std::memory_order_relaxed);
    g_overlay.present(hdc);
    const BOOL result = g_originalWglSwapBuffers ? g_originalWglSwapBuffers(hdc) : FALSE;
    g_activeHookCalls.fetch_sub(1, std::memory_order_relaxed);
    return result;
}

LRESULT CALLBACK overlayWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    return g_overlay.windowProc(hwnd, message, wparam, lparam);
}

} // namespace

// ---------------------------------------------------------------------------
// Public helpers
// ---------------------------------------------------------------------------

bool installOverlay() {
    return g_overlay.install();
}

void requestOverlayUnload() {
    g_overlay.requestUnload();
}

void finalizeOverlayUnload() {
    g_overlay.finalize();
}

bool overlayUnloadRequested() {
    return g_overlay.unloadRequested();
}

bool overlaySafeToUnload() {
    return g_overlay.safeToUnload();
}

bool overlayInstalled() {
    return g_overlay.installed();
}

} // namespace neo::platform
