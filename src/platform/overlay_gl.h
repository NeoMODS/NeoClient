#pragma once

#include <windows.h>

namespace neo::platform {

// Installs the OpenGL / GDI present hooks and prepares the overlay.
// Safe to call from a worker thread; the ImGui context itself is created on
// the game's render thread the first time a swap is observed.
bool installOverlay();

// Requests a clean shutdown. The render thread tears down ImGui and the
// control thread removes the MinHook hooks before freeing the module.
void requestOverlayUnload();

// Called from the control thread after the render thread has finished its
// last frame. Returns once the hooks have been removed.
void finalizeOverlayUnload();

bool overlayUnloadRequested();
bool overlaySafeToUnload();
bool overlayInstalled();

} // namespace neo::platform
