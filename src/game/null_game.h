#pragma once

#include "game/game_access.h"
#include "game/ores.h"

#include <vector>

namespace neo {

// A harmless game facade used by the standalone UI preview and as a fallback
// while the JVM bridge is still warming up.
class DemoGame final : public GameAccess {
public:
    DemoGame();

    const GameSnapshot& snapshot() const override { return snapshot_; }
    bool keyDown(GameKey key) const override;
    void setKey(GameKey key, bool pressed) override;

    void jump() override;
    void setFallDistance(double value) override;
    bool onGround() const override { return snapshot_.onGround; }
    void setInvulnerableTime(int value) override { (void)value; }
    void setHurtTime(int value) override { (void)value; }
    void zeroServerFallDistance() override {}
    void setServerInvulnerableTime(int value) override { (void)value; }
    void setNoPhysics(bool value) override;

    Vec3 deltaMovement() const override { return {0.0, 0.0, 0.0}; }
    void setDeltaMovement(const Vec3& value) override {}
    Vec3 vehicleDeltaMovement() const override { return {0.0, 0.0, 0.0}; }
    void setVehicleDeltaMovement(const Vec3& value) override {}

    void setFlying(bool value) override;
    void setMayFly(bool value) override;
    void setFlyingSpeed(float value) override { flyingSpeed_ = value; }
    float flyingSpeed() const override { return flyingSpeed_; }

    void setGamma(double value) override { snapshot_.gamma = value; }
    double gamma() const override { return snapshot_.gamma; }
    void setFov(int value) override { fov_ = value; }
    int fov() const override { return fov_; }

    void respawn() override {}

    void setMenuCaptured(bool captured) override { (void)captured; }

    void requestOreScan(const OreScan& scan) override;
    const std::vector<OreHit>& oreMarkers() const override { return markers_; }

    void* sdlWindow() const override { return nullptr; }

    void setAvailable(bool value) { snapshot_.available = value; }
    void setFps(int value) { snapshot_.fps = value; }

private:
    GameSnapshot snapshot_;
    bool keys_[7]{};
    int fov_ = 70;
    float flyingSpeed_ = 0.05f;
    std::vector<OreHit> markers_;
};

} // namespace neo
