#include "platform/overlay_gl.h"

#include "core/logger.h"

#include <windows.h>

namespace {

HMODULE g_module = nullptr;

DWORD WINAPI controlThread(LPVOID) {
    // opengl32 is loaded as soon as Minecraft creates its context. Waiting a
    // little makes sure both present hooks can be installed in one pass.
    for (int i = 0; i < 100; ++i) {
        if (GetModuleHandleW(L"opengl32.dll") != nullptr) {
            break;
        }
        Sleep(50);
    }

    neo::Logger::instance().initialize();
    NEO_LOG_INFO("NeoClient DLL loaded");

    if (!neo::platform::installOverlay()) {
        NEO_LOG_ERROR("Overlay installation failed; unloading");
        Sleep(250);
        FreeLibraryAndExitThread(g_module, 0);
        return 0;
    }

    // The render thread performs the ImGui shutdown once the unload button is
    // pressed. Only then is it safe to restore the present prologues.
    while (!neo::platform::overlaySafeToUnload()) {
        Sleep(100);
    }

    neo::platform::finalizeOverlayUnload();
    NEO_LOG_INFO("Hooks removed; detaching");
    Sleep(100);

    FreeLibraryAndExitThread(g_module, 0);
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);

        HANDLE thread = CreateThread(nullptr, 0, &controlThread, nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }

    return TRUE;
}
