#pragma once

#include "game/game_access.h"
#include "game/jvm.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace neo {

class JniGame final : public GameAccess {
public:
    JniGame();
    ~JniGame() override;

    void refresh();
    void endFrame();

    const GameSnapshot& snapshot() const override { return snapshot_; }
    bool keyDown(GameKey key) const override;
    void setKey(GameKey key, bool pressed) override;

    void jump() override;
    void setFallDistance(double value) override;
    bool onGround() const override;
    void setInvulnerableTime(int value) override;
    void setHurtTime(int value) override;
    void zeroServerFallDistance() override;
    void setServerInvulnerableTime(int value) override;
    void setNoPhysics(bool value) override;

    Vec3 deltaMovement() const override;
    void setDeltaMovement(const Vec3& value) override;
    Vec3 vehicleDeltaMovement() const override;
    void setVehicleDeltaMovement(const Vec3& value) override;

    void setFlying(bool value) override;
    void setMayFly(bool value) override;
    void setFlyingSpeed(float value) override;
    float flyingSpeed() const override;

    void setGamma(double value) override;
    double gamma() const override;
    void setFov(int value) override;
    int fov() const override;

    void respawn() override;
    void setMenuCaptured(bool captured) override;

    void requestOreScan(const OreScan& scan) override;
    const std::vector<OreHit>& oreMarkers() const override { return oreMarkersResolved_; }

    void* sdlWindow() const override;

private:
    bool resolve();
    void checkException(const char* context);
    jclass loadClass(const char* name);
    jobject callGetInstance();
    jobject serverPlayer();
    jobject optionsObject() const;
    jobject keyMapping(GameKey key) const;
    jobject abilities() const;
    jobject currentVehicle() const;
    double optionNumber(const char* methodName, const char* methodSignature, bool integer) const;
    void setOptionNumber(const char* methodName, const char* methodSignature, bool integer, double value);
    void captureRotation();
    void cleanupLocals();

    // X-Ray support: resolve classes reflexively with fallbacks (Minecraft
    // 26.3 names may drift), then run a throttled cube sweep.
    bool prepareOreScan();
    bool resolveOreBlocks();
    void runOreSweep(const OreScan& scan);
    void noteMissing(const char* kind, const char* name);
    jmethodID method(jclass owner, const char* name, const char* signature);
    jmethodID staticMethod(jclass owner, const char* name, const char* signature);
    jfieldID field(jclass owner, const char* name, const char* signature);
    jobject findBlockObject(const char* name);

    Jvm jvm_;
    bool resolved_ = false;
    bool attachAttempted_ = false;
    bool errorLogged_ = false;
    std::vector<std::string> missingLogged_;
    std::chrono::steady_clock::time_point nextResolveAttempt_{};

    jclass mcClass_ = nullptr;
    jclass playerClass_ = nullptr;
    jclass entityClass_ = nullptr;
    jclass keyMappingClass_ = nullptr;
    jclass optionsClass_ = nullptr;
    jclass abilitiesClass_ = nullptr;
    jclass windowClass_ = nullptr;
    jclass optionClass_ = nullptr;
    jclass boatClass_ = nullptr;
    jclass doubleClass_ = nullptr;
    jclass integerClass_ = nullptr;
    jclass vecClass_ = nullptr;
    jclass mouseHandlerClass_ = nullptr;
    jobject gameLoader_ = nullptr; // Fabric/launcher class loader (global ref)

    jmethodID getInstance_ = nullptr;
    jmethodID getFps_ = nullptr;
    jmethodID getWindow_ = nullptr;
    jmethodID getSingleplayerServer_ = nullptr;
    jmethodID getGuiScaledWidth_ = nullptr;
    jmethodID getGuiScaledHeight_ = nullptr;
    jmethodID windowHandle_ = nullptr;

    jfieldID mcPlayer_ = nullptr;
    jfieldID mcLevel_ = nullptr;
    jfieldID mcOptions_ = nullptr;
    jfieldID mcMouseHandler_ = nullptr;
    jfieldID mcGameRenderer_ = nullptr;
    jfieldID keyUp_ = nullptr;
    jfieldID keyBack_ = nullptr;
    jfieldID keyLeft_ = nullptr;
    jfieldID keyRight_ = nullptr;
    jfieldID keyJump_ = nullptr;
    jfieldID keyShift_ = nullptr;
    jfieldID keySprint_ = nullptr;
    jfieldID fallDistance_ = nullptr;
    jfieldID invulnerableTime_ = nullptr;
    jfieldID hurtTime_ = nullptr;
    jfieldID noPhysics_ = nullptr;
    jfieldID optionValue_ = nullptr;
    jfieldID flying_ = nullptr;
    jfieldID mayFly_ = nullptr;
    jfieldID vecX_ = nullptr;
    jfieldID vecY_ = nullptr;
    jfieldID vecZ_ = nullptr;

    jmethodID onGround_ = nullptr;
    jmethodID isPassenger_ = nullptr;
    jmethodID getVehicle_ = nullptr;
    jmethodID getDeltaMovement_ = nullptr;
    jmethodID setDeltaMovement_ = nullptr;
    jmethodID getX_ = nullptr;
    jmethodID getY_ = nullptr;
    jmethodID getZ_ = nullptr;
    jmethodID getYRot_ = nullptr;
    jmethodID getXRot_ = nullptr;
    jmethodID getEyeHeight_ = nullptr;
    jmethodID jumpFromGround_ = nullptr;
    jmethodID setSprinting_ = nullptr;
    jmethodID isDeadOrDying_ = nullptr;
    jmethodID getAbilities_ = nullptr;
    jmethodID respawn_ = nullptr;
    jmethodID getUuid_ = nullptr;
    jmethodID serverGetPlayerList_ = nullptr;
    jmethodID playerListGetPlayer_ = nullptr;
    jmethodID getMainCamera_ = nullptr;
    jmethodID cameraPosition_ = nullptr;
    jmethodID cameraXRot_ = nullptr;
    jmethodID cameraYRot_ = nullptr;
    jmethodID cameraGetFov_ = nullptr;
    jmethodID cameraInitialized_ = nullptr;
    jmethodID setFlyingSpeed_ = nullptr;
    jmethodID getFlyingSpeed_ = nullptr;
    jmethodID keySetDown_ = nullptr;
    jmethodID keyIsDown_ = nullptr;
    jmethodID keySetAll_ = nullptr;
    jmethodID keyReleaseAll_ = nullptr;
    jmethodID onUpdateAbilities_ = nullptr;
    jmethodID releaseMouse_ = nullptr;
    jmethodID grabMouse_ = nullptr;
    jmethodID isMouseGrabbed_ = nullptr;

    jmethodID getGamma_ = nullptr;
    jmethodID getFov_ = nullptr;
    jmethodID optionSet_ = nullptr;
    jmethodID optionGet_ = nullptr;
    jmethodID doubleValueOf_ = nullptr;
    jmethodID doubleValue_ = nullptr;
    jmethodID integerValueOf_ = nullptr;
    jmethodID integerValue_ = nullptr;

    jobject mc_ = nullptr;
    jobject player_ = nullptr;
    jobject level_ = nullptr;
    jobject options_ = nullptr;
    jobject vehicle_ = nullptr;

    bool menuCaptured_ = false;
    bool resumeMouseGrab_ = false;

    // Ore scan (resolved lazily; every symbol is optional and failures only
    // disable X-Ray, never the rest of the client).
    bool oreScanReady_ = false;
    bool oreScanBroken_ = false;
    bool oreBlockCacheTried_ = false;
    bool oreScanInProgress_ = false;
    std::uint32_t oreScanMask_ = 0;
    int oreScanIndex_ = 0;
    int oreScanVolume_ = 0;
    int oreScanOrigin_[3] = {0, 0, 0};
    std::vector<OreHit> oreMarkersResolved_;  // persistent, updated incrementally
    std::unordered_map<std::uint64_t, std::size_t> oreIndex_;  // packed pos -> index

    jclass blockPosClass_ = nullptr;
    jclass mutablePosClass_ = nullptr;
    jobject mutablePos_ = nullptr;
    jmethodID mutablePosSet_ = nullptr;
    bool mutablePosSetVoid_ = false;
    jmethodID blockPosCtor_ = nullptr;
    jmethodID levelGetBlockState_ = nullptr;
    jmethodID stateGetBlock_ = nullptr;
    jclass resourceLocationClass_ = nullptr;
    jmethodID resourceLocationCtor_ = nullptr;
    jmethodID resourceLocationCtor2_ = nullptr;
    jmethodID resourceLocationParse_ = nullptr;
    jmethodID resourceLocationFromNamespace_ = nullptr;
    jclass blockStateClass_ = nullptr;
    jobject blockRegistry_ = nullptr;
    jmethodID registryGet_ = nullptr;
    std::vector<jobject> oreBlockObjects_;   // global refs, resolved from ores.h
    std::vector<int> oreBlockGroups_;        // group index for each entry

    GameSnapshot snapshot_;
};

} // namespace neo
