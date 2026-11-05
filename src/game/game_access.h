#pragma once

#include "core/math.h"

#include <cstdint>
#include <string>
#include <vector>

namespace neo {

enum class GameKey {
    Jump,
    Shift,
    Sprint,
    Forward,
    Back,
    Left,
    Right,
};

struct GameSnapshot {
    bool available = false;
    bool inGame = false;
    bool singleplayer = false;
    bool onGround = false;
    bool inBoat = false;
    bool dead = false;
    bool flying = false;
    bool mayFly = false;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double gamma = 1.0;
    float yaw = 0.0f;          // degrees, Minecraft camera convention
    float pitch = 0.0f;        // degrees
    float eyeHeight = 1.62f;   // blocks above the feet position
    int fov = 70;              // vertical field of view option in degrees
    int fps = 0;

    // Interpolated camera pose + effective (sprint/zoom-adjusted) FOV. When
    // present these are used for X-Ray projection so markers stay locked to the
    // world while moving; otherwise the player pose above is used.
    bool hasCamera = false;
    double camX = 0.0;
    double camY = 0.0;
    double camZ = 0.0;
    float camYaw = 0.0f;
    float camPitch = 0.0f;
    float camFov = 70.0f;
};

// One request of the X-Ray scanner. The game access implementation turns it
// into a incremental, budgeted sweep and keeps the resulting markers around so
// the UI can project them without touching the JVM every frame.
struct OreScan {
    bool active = false;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    int radius = 16;
    std::uint32_t mask = 0; // ore::kGroups bit field; bit i selects group i
};

// A highlighted block. `group` indexes into ore::kGroups for color + label.
struct OreHit {
    int x = 0;
    int y = 0;
    int z = 0;
    int group = 0;
};

class GameAccess {
public:
    virtual ~GameAccess() = default;

    virtual const GameSnapshot& snapshot() const = 0;
    virtual bool keyDown(GameKey key) const = 0;
    virtual void setKey(GameKey key, bool pressed) = 0;

    virtual void jump() = 0;
    virtual void setFallDistance(double value) = 0;
    virtual bool onGround() const = 0;
    virtual void setInvulnerableTime(int value) = 0;
    virtual void setHurtTime(int value) = 0;
    virtual void zeroServerFallDistance() = 0;
    virtual void setServerInvulnerableTime(int value) = 0;
    virtual void setNoPhysics(bool value) = 0;

    virtual Vec3 deltaMovement() const = 0;
    virtual void setDeltaMovement(const Vec3& value) = 0;
    virtual Vec3 vehicleDeltaMovement() const = 0;
    virtual void setVehicleDeltaMovement(const Vec3& value) = 0;

    virtual void setFlying(bool value) = 0;
    virtual void setMayFly(bool value) = 0;
    virtual void setFlyingSpeed(float value) = 0;
    virtual float flyingSpeed() const = 0;

    virtual void setGamma(double value) = 0;
    virtual double gamma() const = 0;
    virtual void setFov(int value) = 0;
    virtual int fov() const = 0;

    virtual void respawn() = 0;

    // Releases the game's mouse grab and clears its pressed keys while the
    // NeoClient menu is visible. Must be called every frame the menu is open
    // so focus changes cannot re-grab the cursor behind our back.
    virtual void setMenuCaptured(bool captured) = 0;

    // X-Ray ore scanning. Call requestOreScan once per module tick with the
    // current query; the sweep is spread over several calls. oreMarkers()
    // always returns the last completed pass so drawing stays allocation free.
    virtual void requestOreScan(const OreScan& scan) = 0;
    virtual const std::vector<OreHit>& oreMarkers() const = 0;

    virtual void* sdlWindow() const = 0;
};

} // namespace neo
