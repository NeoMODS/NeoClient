#include "core/client.h"
#include "game/null_game.h"
#include "ui/fonts.h"
#include "ui/theme.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <windows.h>
#include <shellapi.h>
#include <GL/gl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

constexpr wchar_t kWindowClass[] = L"NeoClientPreviewWindow";
constexpr int kWindowWidth = 1200;
constexpr int kWindowHeight = 780;

HGLRC g_context = nullptr;
HDC g_deviceContext = nullptr;
neo::Client* g_client = nullptr;
bool g_running = true;

void drawMountain(ImDrawList* drawList, float baseY, float peakX, float peakY, float halfWidth, ImU32 color, ImU32 capColor) {
    drawList->AddTriangleFilled(
        ImVec2(peakX - halfWidth, baseY),
        ImVec2(peakX, peakY),
        ImVec2(peakX + halfWidth, baseY),
        color);

    const float capHeight = (baseY - peakY) * 0.22f;
    const float capWidth = halfWidth * 0.20f;
    drawList->AddTriangleFilled(
        ImVec2(peakX - capWidth, peakY + capHeight),
        ImVec2(peakX, peakY),
        ImVec2(peakX + capWidth, peakY + capHeight),
        capColor);
}

void drawPreviewScene(ImDrawList* drawList, ImVec2 size, float time) {
    const float horizon = size.y * 0.615f;

    drawList->AddRectFilledMultiColor(
        ImVec2(0.0f, 0.0f), ImVec2(size.x, horizon),
        IM_COL32(14, 18, 28, 255), IM_COL32(20, 25, 38, 255),
        IM_COL32(61, 62, 88, 255), IM_COL32(43, 48, 72, 255));

    // Soft late-afternoon glow.
    const ImVec2 sun(size.x * 0.775f, size.y * 0.265f);
    for (int i = 7; i >= 1; --i) {
        drawList->AddCircleFilled(sun, 26.0f + static_cast<float>(i) * 34.0f,
            IM_COL32(255, 207, 190, 2 + i), 48);
    }
    drawList->AddCircleFilled(sun, 26.0f, IM_COL32(255, 236, 220, 165), 48);
    drawList->AddCircleFilled(sun, 17.0f, IM_COL32(255, 247, 238, 205), 40);

    // Pastel aurora bands.
    for (int i = 0; i < 3; ++i) {
        const float y = size.y * (0.13f + i * 0.055f) + std::sin(time * 0.11f + i) * 6.0f;
        drawList->AddRectFilledMultiColor(
            ImVec2(0.0f, y), ImVec2(size.x, y + 34.0f + i * 10.0f),
            IM_COL32(187, 201, 255, 0), IM_COL32(246, 185, 211, 10 + i * 3),
            IM_COL32(167, 230, 205, 0), IM_COL32(187, 201, 255, 0));
    }

    // Distant stars.
    for (int i = 0; i < 36; ++i) {
        const float x = std::fmod(static_cast<float>(i) * 137.37f, size.x);
        const float y = std::fmod(static_cast<float>(i) * 61.71f, horizon * 0.72f);
        const float alpha = 24.0f + static_cast<float>((i * 37) % 46);
        drawList->AddCircleFilled(ImVec2(x, y), 1.0f + static_cast<float>(i % 2) * 0.5f,
            IM_COL32(230, 236, 255, static_cast<int>(alpha)), 6);
    }

    drawMountain(drawList, horizon + 6.0f, size.x * 0.12f, size.y * 0.38f, size.x * 0.24f,
        IM_COL32(31, 38, 55, 255), IM_COL32(66, 74, 96, 120));
    drawMountain(drawList, horizon + 6.0f, size.x * 0.36f, size.y * 0.31f, size.x * 0.28f,
        IM_COL32(26, 33, 49, 255), IM_COL32(70, 78, 101, 100));
    drawMountain(drawList, horizon + 6.0f, size.x * 0.63f, size.y * 0.44f, size.x * 0.30f,
        IM_COL32(22, 29, 44, 255), IM_COL32(60, 68, 90, 90));
    drawMountain(drawList, horizon + 6.0f, size.x * 0.88f, size.y * 0.34f, size.x * 0.30f,
        IM_COL32(28, 35, 52, 255), IM_COL32(64, 72, 94, 110));

    // Ground and horizon haze.
    drawList->AddRectFilledMultiColor(
        ImVec2(0.0f, horizon), ImVec2(size.x, size.y),
        IM_COL32(34, 47, 46, 255), IM_COL32(28, 40, 41, 255),
        IM_COL32(9, 13, 15, 255), IM_COL32(9, 13, 15, 255));

    drawList->AddRectFilledMultiColor(
        ImVec2(0.0f, horizon - 2.0f), ImVec2(size.x, horizon + 26.0f),
        IM_COL32(167, 230, 205, 0), IM_COL32(246, 185, 211, 0),
        IM_COL32(246, 185, 211, 20), IM_COL32(167, 230, 205, 20));

    // Soft light pools on the ground.
    for (int i = 0; i < 3; ++i) {
        const float x = size.x * (0.18f + 0.30f * i);
        const float y = size.y * (0.83f + 0.03f * i);
        for (int band = 3; band >= 1; --band) {
            drawList->AddEllipseFilled(ImVec2(x, y), ImVec2(80.0f * band + i * 24.0f, 10.0f * band + i * 3.0f),
                IM_COL32(167, 230, 205, 2 + band), 0.0f, 40);
        }
    }

    // A couple of isometric blocks resting near the edges.
    const auto block = [drawList](ImVec2 center, float radius, ImU32 top, ImU32 left, ImU32 right) {
        const ImVec2 topPoint(center.x, center.y - radius);
        const ImVec2 rightPoint(center.x + radius, center.y - radius * 0.5f);
        const ImVec2 bottomPoint(center.x, center.y);
        const ImVec2 leftPoint(center.x - radius, center.y - radius * 0.5f);
        const ImVec2 lowerLeft(center.x - radius, center.y + radius * 0.5f);
        const ImVec2 lowerBottom(center.x, center.y + radius);
        const ImVec2 lowerRight(center.x + radius, center.y + radius * 0.5f);

        drawList->AddQuadFilled(topPoint, rightPoint, bottomPoint, leftPoint, top);
        drawList->AddQuadFilled(leftPoint, bottomPoint, lowerBottom, lowerLeft, left);
        drawList->AddQuadFilled(bottomPoint, rightPoint, lowerRight, lowerBottom, right);
    };

    block(ImVec2(size.x * 0.075f, size.y * 0.86f), 26.0f,
        IM_COL32(77, 96, 92, 210), IM_COL32(49, 64, 62, 210), IM_COL32(37, 49, 48, 210));
    block(ImVec2(size.x * 0.925f, size.y * 0.79f), 20.0f,
        IM_COL32(83, 88, 116, 190), IM_COL32(55, 59, 80, 190), IM_COL32(41, 44, 61, 190));

    // Vignette strips.
    drawList->AddRectFilledMultiColor(
        ImVec2(0.0f, 0.0f), ImVec2(size.x, size.y * 0.14f),
        IM_COL32(4, 6, 9, 110), IM_COL32(4, 6, 9, 110), IM_COL32(4, 6, 9, 0), IM_COL32(4, 6, 9, 0));
    drawList->AddRectFilledMultiColor(
        ImVec2(0.0f, size.y * 0.86f), ImVec2(size.x, size.y),
        IM_COL32(4, 6, 9, 0), IM_COL32(4, 6, 9, 0), IM_COL32(4, 6, 9, 120), IM_COL32(4, 6, 9, 120));
    drawList->AddRectFilledMultiColor(
        ImVec2(0.0f, 0.0f), ImVec2(size.x * 0.10f, size.y),
        IM_COL32(4, 6, 9, 100), IM_COL32(4, 6, 9, 0), IM_COL32(4, 6, 9, 0), IM_COL32(4, 6, 9, 100));
    drawList->AddRectFilledMultiColor(
        ImVec2(size.x * 0.90f, 0.0f), ImVec2(size.x, size.y),
        IM_COL32(4, 6, 9, 0), IM_COL32(4, 6, 9, 100), IM_COL32(4, 6, 9, 100), IM_COL32(4, 6, 9, 0));
}

LRESULT CALLBACK previewWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
        const bool repeat = (lparam & 0x40000000) != 0;
        if (!repeat && g_client) {
            g_client->keyDown(static_cast<int>(wparam));
        }
    }

    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam)) {
        return 0;
    }

    switch (message) {
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize.x = 1000;
        info->ptMinTrackSize.y = 700;
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool createWindow(HINSTANCE instance, HWND& window, bool visible) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &previewWindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kWindowClass;

    if (!RegisterClassExW(&windowClass)) {
        return false;
    }

    RECT rect{0, 0, kWindowWidth, kWindowHeight};
    AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

    window = CreateWindowExW(
        0, kWindowClass, L"NeoClient Preview",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, instance, nullptr);

    if (!window) {
        return false;
    }

    if (visible) {
        ShowWindow(window, SW_SHOW);
        UpdateWindow(window);
    }
    return true;
}

bool g_vsync = false;

std::wstring argumentValue(const wchar_t* flag) {
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (!arguments) {
        return {};
    }

    std::wstring value;
    const std::wstring target(flag);
    for (int i = 1; i + 1 < argumentCount; ++i) {
        if (std::wstring(arguments[i]) == target) {
            value = arguments[i + 1];
            break;
        }
    }

    LocalFree(arguments);
    return value;
}

bool captureScreenshot(HWND window, const std::wstring& path) {
    RECT rect{};
    if (!GetClientRect(window, &rect)) {
        return false;
    }

    const int width = static_cast<int>(rect.right - rect.left);
    const int height = static_cast<int>(rect.bottom - rect.top);
    if (width <= 0 || height <= 0) {
        return false;
    }

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    const size_t rowBytes = static_cast<size_t>(width) * 4u;
    for (int y = 0; y < height / 2; ++y) {
        unsigned char* top = pixels.data() + static_cast<size_t>(y) * rowBytes;
        unsigned char* bottom = pixels.data() + static_cast<size_t>(height - 1 - y) * rowBytes;
        for (size_t x = 0; x < rowBytes; ++x) {
            std::swap(top[x], bottom[x]);
        }
    }

    const std::filesystem::path destination(path);
    if (destination.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(destination.parent_path(), error);
    }

    const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, destination.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Length <= 0) {
        return false;
    }

    std::string utf8Path(static_cast<size_t>(utf8Length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, destination.c_str(), -1, utf8Path.data(), utf8Length, nullptr, nullptr);
    if (!utf8Path.empty() && utf8Path.back() == '\0') {
        utf8Path.pop_back();
    }

    return stbi_write_png(utf8Path.c_str(), width, height, 4, pixels.data(), width * 4) != 0;
}

bool createContext(HWND window) {
    g_deviceContext = GetDC(window);

    PIXELFORMATDESCRIPTOR descriptor{};
    descriptor.nSize = sizeof(descriptor);
    descriptor.nVersion = 1;
    descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    descriptor.iPixelType = PFD_TYPE_RGBA;
    descriptor.cColorBits = 32;
    descriptor.cDepthBits = 24;
    descriptor.cStencilBits = 8;
    descriptor.iLayerType = PFD_MAIN_PLANE;

    const int format = ChoosePixelFormat(g_deviceContext, &descriptor);
    if (format == 0 || !SetPixelFormat(g_deviceContext, format, &descriptor)) {
        return false;
    }

    g_context = wglCreateContext(g_deviceContext);
    if (!g_context || !wglMakeCurrent(g_deviceContext, g_context)) {
        return false;
    }

    using SwapIntervalFn = BOOL(WINAPI*)(int);
    auto swapInterval = reinterpret_cast<SwapIntervalFn>(wglGetProcAddress("wglSwapIntervalEXT"));
    if (swapInterval) {
        g_vsync = swapInterval(1) != FALSE;
    }

    return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    ImGui_ImplWin32_EnableDpiAwareness();

    const std::wstring screenshotPath = argumentValue(L"--screenshot");
    const bool screenshotMode = !screenshotPath.empty();

    HWND window = nullptr;
    if (!createWindow(instance, window, !screenshotMode) || !createContext(window)) {
        MessageBoxW(nullptr, L"Could not create the preview window.", L"NeoClient", MB_ICONERROR);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    neo::ui::applyStyle(1.0f);
    neo::ui::loadFonts(1.0f);

    if (!ImGui_ImplWin32_InitForOpenGL(window) || !ImGui_ImplOpenGL3_Init("#version 150")) {
        MessageBoxW(nullptr, L"Could not initialize the ImGui backends.", L"NeoClient", MB_ICONERROR);
        return 1;
    }

    neo::DemoGame game;
    neo::Client client(game);
    client.initialize();
    client.setMenuOpen(true);
    client.gui().state().menuAnim = 1.0f;
    client.gui().state().tab = neo::Tab::Exploit;
    client.gui().state().group = neo::Group::Movement;
    client.gui().state().selectedModule = "speed";

    if (argumentValue(L"--tab") == L"menu") {
        client.gui().state().tab = neo::Tab::Menu;
        client.gui().state().selectedModule.clear();
    }

    client.modules().setEnabled("fly", true, game);
    client.modules().setEnabled("speed", true, game);
    client.modules().setEnabled("fullbright", true, game);
    client.modules().setEnabled("no_fall", true, game);
    client.modules().setEnabled("hud", true, game);

    g_client = &client;

    const auto renderFrame = [&](float dt, float elapsed) {
        game.setFps(196 + static_cast<int>(std::sin(elapsed * 0.85f) * 13.0f +
                                            std::sin(elapsed * 0.27f) * 7.0f));

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        io.DeltaTime = dt;

        drawPreviewScene(ImGui::GetBackgroundDrawList(), io.DisplaySize, elapsed);
        client.frame(dt);
        ImGui::Render();

        RECT rect{};
        GetClientRect(window, &rect);
        glViewport(0, 0, std::max<LONG>(rect.right, 1), std::max<LONG>(rect.bottom, 1));
        glClearColor(0.025f, 0.031f, 0.043f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    };

    int exitCode = 0;
    if (screenshotMode) {
        constexpr float fixedDt = 1.0f / 60.0f;
        constexpr int warmupFrames = 620;
        for (int i = 0; i < warmupFrames; ++i) {
            renderFrame(fixedDt, static_cast<float>(i) * fixedDt);
        }

        if (!captureScreenshot(window, screenshotPath)) {
            exitCode = 2;
        }
    } else {
        auto lastFrame = std::chrono::steady_clock::now();
        const auto start = lastFrame;
        MSG message{};

        while (g_running) {
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (!g_running) {
                break;
            }

            const auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - lastFrame).count();
            lastFrame = now;
            dt = std::clamp(dt, 0.0f, 0.1f);
            if (dt <= 0.0f) {
                dt = 1.0f / 60.0f;
            }

            const float elapsed = std::chrono::duration<float>(now - start).count();
            renderFrame(dt, elapsed);
            SwapBuffers(g_deviceContext);

            if (!g_vsync) {
                Sleep(8);
            }
        }
    }

    g_client = nullptr;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_context) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(g_context);
        g_context = nullptr;
    }
    if (g_deviceContext) {
        if (IsWindow(window)) {
            ReleaseDC(window, g_deviceContext);
        }
        g_deviceContext = nullptr;
    }
    if (IsWindow(window)) {
        DestroyWindow(window);
    }
    UnregisterClassW(kWindowClass, instance);

    return exitCode;
}
