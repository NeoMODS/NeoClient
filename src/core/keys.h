#pragma once

#include <string>

namespace neo::keys {

std::string name(int virtualKey);
bool down(int virtualKey);
bool pressed(int virtualKey);
int fromImGuiKey(int imguiKey);

} // namespace neo::keys
