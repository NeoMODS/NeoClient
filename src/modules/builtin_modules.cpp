#include "modules/builtin_modules.h"
#include "game/ores.h"

#include <windows.h>

#include <cmath>

namespace neo {
namespace {

bool anyMovementKey(const GameAccess& game) {
    return game.keyDown(GameKey::Forward) || game.keyDown(GameKey::Back) ||
           game.keyDown(GameKey::Left) || game.keyDown(GameKey::Right);
}

class AirJump final : public Module {
public:
    AirJump()
        : Module("air_jump", "Air Jump", "Jump again while airborne.", Tab::Exploit, Group::Movement, ui::Icon::AirJump) {
        addSetting(Setting::slider("charges", "Charges", 2.0f, 1.0f, 5.0f, 1.0f, "", "%.0f"));
        addSetting(Setting::slider("power", "Power", 0.42f, 0.2f, 0.9f, 0.01f));
    }

protected:
    void onDisable(GameAccess& game) override {
        chargesUsed_ = 0;
        wasJumpDown_ = false;
    }

    void onTick(GameAccess& game) override {
        const bool jumpDown = game.keyDown(GameKey::Jump);
        const bool pressed = jumpDown && !wasJumpDown_;
        wasJumpDown_ = jumpDown;

        if (game.snapshot().onGround) {
            chargesUsed_ = 0;
            return;
        }

        const int charges = static_cast<int>(number("charges"));
        if (pressed && !game.snapshot().inBoat && chargesUsed_ < charges) {
            const float power = number("power");
            Vec3 velocity = game.deltaMovement();
            velocity.y = power;
            game.setDeltaMovement(velocity);
            ++chargesUsed_;
        }
    }

private:
    int chargesUsed_ = 0;
    bool wasJumpDown_ = false;
};

class BoatFly final : public Module {
public:
    BoatFly()
        : Module("boat_fly", "Boat Fly", "Vertical and forward control while riding a boat.", Tab::Exploit, Group::Movement, ui::Icon::BoatFly) {
        addSetting(Setting::slider("up_speed", "Up Speed", 0.55f, 0.1f, 2.0f, 0.01f));
        addSetting(Setting::slider("down_speed", "Down Speed", 0.55f, 0.1f, 2.0f, 0.01f));
        addSetting(Setting::slider("forward_speed", "Forward Speed", 0.0f, 0.0f, 4.0f, 0.05f));
        addSetting(Setting::slider("stability", "Stability", 0.65f, 0.0f, 1.0f, 0.01f));
    }

protected:
    void onTick(GameAccess& game) override {
        if (!game.snapshot().inBoat) {
            return;
        }

        Vec3 velocity = game.vehicleDeltaMovement();
        const float up = number("up_speed");
        const float down = number("down_speed");
        const float forward = number("forward_speed");
        const float stability = number("stability");

        if (game.keyDown(GameKey::Jump)) {
            velocity.y = up;
        } else if (game.keyDown(GameKey::Shift)) {
            velocity.y = -down;
        } else if (stability > 0.0f) {
            velocity.y *= (1.0f - stability * 0.85f);
            if (std::abs(velocity.y) < 0.005) {
                velocity.y = 0.0;
            }
        }

        if (forward > 0.0f && game.keyDown(GameKey::Forward)) {
            const double yawRad = game.snapshot().yaw * 0.017453292519943295;
            velocity.x -= std::sin(yawRad) * forward;
            velocity.z += std::cos(yawRad) * forward;
        }

        game.setVehicleDeltaMovement(velocity);
    }
};

class Fly final : public Module {
public:
    Fly()
        : Module("fly", "Fly", "Creative-style flight in any game mode.", Tab::Exploit, Group::Movement, ui::Icon::Fly) {
        addSetting(Setting::slider("speed", "Fly Speed", 0.10f, 0.02f, 0.50f, 0.01f));
        addSetting(Setting::toggle("vertical", "Vertical Control", true));
    }

protected:
    void onEnable(GameAccess& game) override {
        // A module restored from the config is enabled before the JVM bridge
        // exists. Capture the original abilities on the first tick instead of
        // restoring fabricated defaults later.
        if (game.snapshot().available) {
            capture(game);
            apply(game);
        }
    }

    void onTick(GameAccess& game) override {
        if (!captured_) {
            capture(game);
        }
        apply(game);
    }

    void onDisable(GameAccess& game) override {
        // Always restore abilities on disable, even if capture() never ran
        // (e.g. module enabled before the JVM bridge was ready).
        game.setFlying(false);
        game.setMayFly(false);
        game.setFlyingSpeed(0.05f);
        captured_ = false;
    }

private:
    void capture(GameAccess& game) {
        oldMayFly_ = game.snapshot().mayFly;
        oldFlying_ = game.snapshot().flying;
        oldSpeed_ = game.flyingSpeed();
        captured_ = true;
    }

    void apply(GameAccess& game) {
        game.setMayFly(true);
        game.setFlying(true);
        game.setFlyingSpeed(number("speed"));
    }

    bool captured_ = false;
    bool oldMayFly_ = false;
    bool oldFlying_ = false;
    float oldSpeed_ = 0.05f;
};

class Speed final : public Module {
public:
    Speed()
        : Module("speed", "Speed", "Gently increases ground movement speed.", Tab::Exploit, Group::Movement, ui::Icon::Speed) {
        addSetting(Setting::slider("multiplier", "Multiplier", 1.45f, 1.0f, 3.0f, 0.05f));
        addSetting(Setting::toggle("ground_only", "Ground Only", true));
    }

protected:
    void onTick(GameAccess& game) override {
        if (flag("ground_only") && !game.snapshot().onGround) {
            return;
        }
        if (game.snapshot().inBoat) {
            return;
        }

        Vec3 velocity = game.deltaMovement();
        const double horizontal = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
        if (horizontal < 0.001) {
            return;
        }

        const double target = 0.18 + 0.10 * static_cast<double>(number("multiplier"));
        if (horizontal >= target) {
            return;
        }

        double scale = target / horizontal;
        scale = std::fmin(scale, 1.20);
        velocity.x *= scale;
        velocity.z *= scale;
        game.setDeltaMovement(velocity);
    }
};

class NoClip final : public Module {
public:
    NoClip()
        : Module("no_clip", "No Clip", "Pass through blocks while moving.", Tab::Exploit, Group::Movement, ui::Icon::NoClip) {
        addSetting(Setting::toggle("restore", "Restore On Disable", true));
    }

protected:
    void onEnable(GameAccess& game) override {
        game.setNoPhysics(true);
    }

    void onTick(GameAccess& game) override {
        game.setNoPhysics(true);
    }

    void onDisable(GameAccess& game) override {
        if (flag("restore")) {
            game.setNoPhysics(false);
        }
    }
};

class AutoJump final : public Module {
public:
    AutoJump()
        : Module("auto_jump", "Auto Jump", "Hops automatically while you move.", Tab::Misc, Group::Movement, ui::Icon::AutoJump) {
        addSetting(Setting::slider("interval", "Interval", 0.30f, 0.10f, 1.00f, 0.05f, "s"));
    }

protected:
    void onTick(GameAccess& game) override {
        cooldown_ -= 1.0f / 20.0f;
        if (cooldown_ > 0.0f || !game.snapshot().onGround || game.snapshot().inBoat) {
            return;
        }
        if (!anyMovementKey(game)) {
            return;
        }

        game.jump();
        cooldown_ = number("interval");
    }

private:
    float cooldown_ = 0.0f;
};

class AutoWalk final : public Module {
public:
    AutoWalk()
        : Module("auto_walk", "Auto Walk", "Keeps the forward key held.", Tab::Misc, Group::Movement, ui::Icon::AutoWalk) {
    }

protected:
    void onEnable(GameAccess& game) override {
        game.setKey(GameKey::Forward, true);
    }

    void onTick(GameAccess& game) override {
        game.setKey(GameKey::Forward, true);
    }

    void onDisable(GameAccess& game) override {
        game.setKey(GameKey::Forward, false);
    }
};

class Sprint final : public Module {
public:
    Sprint()
        : Module("sprint", "Sprint", "Always keeps sprint active.", Tab::Misc, Group::Movement, ui::Icon::Sprint) {
    }

protected:
    void onEnable(GameAccess& game) override {
        game.setKey(GameKey::Sprint, true);
    }

    void onTick(GameAccess& game) override {
        game.setKey(GameKey::Sprint, true);
    }

    void onDisable(GameAccess& game) override {
        game.setKey(GameKey::Sprint, false);
    }
};

class Fullbright final : public Module {
public:
    Fullbright()
        : Module("fullbright", "Fullbright", "Lifts the world out of the dark.", Tab::Misc, Group::Visuals, ui::Icon::Fullbright) {
        addSetting(Setting::slider("brightness", "Brightness", 8.0f, 1.0f, 15.0f, 0.5f, "", "%.1f"));
    }

protected:
    void onEnable(GameAccess& game) override {
        if (game.snapshot().available) {
            oldGamma_ = game.gamma();
            captured_ = true;
            game.setGamma(number("brightness"));
        }
    }

    void onTick(GameAccess& game) override {
        if (!captured_) {
            oldGamma_ = game.gamma();
            captured_ = true;
        }
        game.setGamma(number("brightness"));
    }

    void onDisable(GameAccess& game) override {
        if (captured_) {
            game.setGamma(oldGamma_);
            captured_ = false;
        }
    }

private:
    bool captured_ = false;
    double oldGamma_ = 1.0;
};

class Hud final : public Module {
public:
    Hud()
        : Module("hud", "HUD", "A soft, minimal in-game overlay.", Tab::Misc, Group::Visuals, ui::Icon::Hud) {
        addSetting(Setting::toggle("watermark", "Watermark", true));
        addSetting(Setting::toggle("modules", "Active Modules", true));
        addSetting(Setting::toggle("coords", "Coordinates", true));
        addSetting(Setting::toggle("fps", "FPS", true));
    }
};

class NoFall final : public Module {
public:
    NoFall()
        : Module("no_fall", "No Fall", "Prevents fall damage, including singleplayer's authoritative server.", Tab::Player, Group::Movement, ui::Icon::NoFall) {
    }

protected:
    void onTick(GameAccess& game) override {
        // Client-side mirror.
        game.setFallDistance(0.0);
        // In singleplayer the integrated server owns fall damage, so zero the
        // ServerPlayer's fall distance as well.
        game.zeroServerFallDistance();
        const bool onGround = game.onGround();
        if (onGround && !wasOnGround_) {
            // Landing tick: brief invulnerability catches the impact frame.
            game.setServerInvulnerableTime(10);
            game.setInvulnerableTime(10);
        }
        wasOnGround_ = onGround;
    }

private:
    bool wasOnGround_ = false;
};

class AutoRespawn final : public Module {
public:
    AutoRespawn()
        : Module("auto_respawn", "Auto Respawn", "Respawns as soon as you die.", Tab::Player, Group::Utility, ui::Icon::Respawn) {
        addSetting(Setting::slider("delay", "Delay", 0.6f, 0.1f, 3.0f, 0.1f, "s"));
    }

protected:
    void onTick(GameAccess& game) override {
        if (!game.snapshot().dead) {
            cooldown_ = 0.0f;
            return;
        }

        cooldown_ += 1.0f / 20.0f;
        if (cooldown_ >= number("delay")) {
            game.respawn();
            cooldown_ = 0.0f;
        }
    }

private:
    float cooldown_ = 0.0f;
};

class Zoom final : public Module {
public:
    Zoom()
        : Module("zoom", "Zoom", "Smooth hold-to-zoom with a custom key.", Tab::World, Group::Visuals, ui::Icon::Zoom) {
        addSetting(Setting::keybind("zoom_key", "Zoom Key", 'C'));
        addSetting(Setting::slider("amount", "Amount", 3.0f, 1.5f, 8.0f, 0.1f, "x"));
    }

protected:
    void onEnable(GameAccess& game) override {
        oldFov_ = game.snapshot().available ? game.fov() : 0;
    }

    void onTick(GameAccess& game) override {
        if (oldFov_ <= 0) {
            oldFov_ = game.fov();
        }

        const Setting* setting = findSetting("zoom_key");
        const int key = setting ? setting->key : 'C';

        if (key > 0 && (GetAsyncKeyState(key) & 0x8000)) {
            game.setFov(static_cast<int>(70.0f / number("amount")));
        } else {
            game.setFov(oldFov_);
        }
    }

    void onDisable(GameAccess& game) override {
        if (oldFov_ > 0) {
            game.setFov(oldFov_);
        }
    }

private:
    int oldFov_ = 0;
};

class Xray final : public Module {
public:
    Xray()
        : Module("xray", "X-Ray", "Spot the exact ores you pick, even through stone.", Tab::World, Group::Visuals, ui::Icon::Spark) {
        for (const auto& ore : ores::kGroups) {
            addSetting(Setting::toggle(ore.settingId, ore.label, ore.defaultOn));
        }
        addSetting(Setting::slider("radius", "Radius", 8.0f, 4.0f, 32.0f, 1.0f, "", "%.0f"));
        addSetting(Setting::toggle("labels", "Show Labels", true));
        addSetting(Setting::keybind("xray_key", "X-Ray Key", 'X'));
    }

protected:
    void onEnable(GameAccess& game) override {
        // The next tick starts a fresh sweep.
        game.requestOreScan(query(game));
    }

    void onTick(GameAccess& game) override {
        game.requestOreScan(query(game));
    }

    void onDisable(GameAccess& game) override {
        OreScan stop;
        stop.active = false;
        game.requestOreScan(stop);
    }

private:
    OreScan query(GameAccess& game) const {
        OreScan scan;
        scan.active = true;
        const GameSnapshot& snapshot = game.snapshot();
        scan.x = snapshot.x;
        scan.y = snapshot.y;
        scan.z = snapshot.z;
        scan.radius = static_cast<int>(number("radius"));
        for (int i = 0; i < ores::groupCount; ++i) {
            if (flag(ores::kGroups[i].settingId)) {
                scan.mask |= (1u << i);
            }
        }
        return scan;
    }
};

} // namespace

void registerBuiltinModules(ModuleManager& manager) {
    manager.add(std::make_unique<AirJump>());
    manager.add(std::make_unique<BoatFly>());
    manager.add(std::make_unique<Fly>());
    manager.add(std::make_unique<Speed>());
    manager.add(std::make_unique<NoClip>());

    manager.add(std::make_unique<AutoJump>());
    manager.add(std::make_unique<AutoWalk>());
    manager.add(std::make_unique<Sprint>());
    manager.add(std::make_unique<Fullbright>());
    manager.add(std::make_unique<Hud>());

    manager.add(std::make_unique<NoFall>());
    manager.add(std::make_unique<AutoRespawn>());

    manager.add(std::make_unique<Zoom>());
    manager.add(std::make_unique<Xray>());

    if (Module* hud = manager.find("hud")) {
        hud->setDefaultEnabled(true);
    }
}

} // namespace neo
