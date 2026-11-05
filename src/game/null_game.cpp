#include "game/null_game.h"

#include <cmath>

namespace neo {
namespace {

// Hand-placed fake ore field so the preview can show the X-Ray pass without a
// JVM. Positions are relative to the demo player spawn.
struct FakeOre {
    double dx, dy, dz;
    int group;
};

constexpr FakeOre kFakeOres[] = {
    {7.0, -6.0, 4.0, 0},    // diamond
    {-9.0, -10.0, 12.0, 0},
    {13.5, -14.0, -8.5, 0},
    {4.0, -3.0, -13.0, 1},  // iron
    {-6.0, -8.0, -6.5, 1},
    {15.0, -12.0, 9.0, 1},
    {9.5, -4.5, 15.0, 1},
    {-13.0, -16.0, 3.0, 2}, // gold
    {6.0, -13.0, 12.0, 2},
    {-3.5, -6.5, 9.0, 3},   // copper
    {11.0, -2.0, 6.0, 3},
    {-11.5, -20.0, -11.0, 3},
    {-8.0, -1.5, -15.0, 4}, // emerald
    {12.5, -9.5, -3.5, 4},
    {2.0, -5.0, 17.0, 5},   // redstone
    {-15.0, -11.0, 7.5, 5},
    {-5.0, -12.0, 18.0, 6}, // lapis
    {14.0, -7.0, -14.5, 6},
    {-2.0, -2.0, -19.0, 7}, // coal
    {8.5, -1.0, 9.5, 7},
    {-16.5, -4.0, -4.0, 7},
    {5.5, -9.0, -6.0, 7},
    {17.0, -18.0, 2.5, 8},  // ancient debris
    {-7.5, -19.0, 15.5, 8},
};

} // namespace

DemoGame::DemoGame() {
    snapshot_.available = true;
    snapshot_.inGame = true;
    snapshot_.singleplayer = true;
    snapshot_.onGround = true;
    snapshot_.x = 128.42;
    snapshot_.y = 71.00;
    snapshot_.z = -342.81;
    snapshot_.gamma = 1.0;
    snapshot_.yaw = 198.0f;
    snapshot_.pitch = -11.0f;
    snapshot_.fov = 70;
    snapshot_.fps = 240;

    for (const FakeOre& fake : kFakeOres) {
        OreHit hit;
        hit.x = static_cast<int>(std::floor(snapshot_.x + fake.dx));
        hit.y = static_cast<int>(std::floor(snapshot_.y + fake.dy));
        hit.z = static_cast<int>(std::floor(snapshot_.z + fake.dz));
        hit.group = fake.group;
        markers_.push_back(hit);
    }
}

bool DemoGame::keyDown(GameKey key) const {
    return keys_[static_cast<int>(key)];
}

void DemoGame::setKey(GameKey key, bool pressed) {
    keys_[static_cast<int>(key)] = pressed;
}

void DemoGame::jump() {}
void DemoGame::setFallDistance(double value) {}
void DemoGame::setNoPhysics(bool value) {}
void DemoGame::setFlying(bool value) { snapshot_.flying = value; }
void DemoGame::setMayFly(bool value) { snapshot_.mayFly = value; }

void DemoGame::requestOreScan(const OreScan& scan) {
    (void)scan; // the demo markers never change
}

} // namespace neo
