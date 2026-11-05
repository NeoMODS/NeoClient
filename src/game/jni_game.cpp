#include "game/jni_game.h"
#include "game/ores.h"

#include "core/logger.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace neo {
namespace {

constexpr auto kResolveRetry = std::chrono::seconds(2);

std::string binaryClassName(const char* name) {
    std::string binary(name);
    std::replace(binary.begin(), binary.end(), '/', '.');
    return binary;
}

// Class.forName is used instead of ClassLoader.loadClass because it accepts an
// explicit loader and always takes a dotted binary name. Trying multiple
// loaders is what makes the bridge work with launchers that keep the game
// classes outside the system class loader.
jclass forNameWithLoader(JNIEnv* env, jobject loader, const char* binaryName) {
    if (!env || !loader || !binaryName) {
        return nullptr;
    }

    jclass classClass = env->FindClass("java/lang/Class");
    if (!classClass) {
        env->ExceptionClear();
        return nullptr;
    }

    jmethodID forName = env->GetStaticMethodID(
        classClass, "forName", "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        forName = nullptr;
    }

    jclass result = nullptr;
    if (forName) {
        jstring name = env->NewStringUTF(binaryName);
        if (name) {
            result = static_cast<jclass>(env->CallStaticObjectMethod(
                classClass, forName, name, JNI_FALSE, loader));
            env->DeleteLocalRef(name);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                result = nullptr;
            }
        }
    }

    env->DeleteLocalRef(classClass);
    return result;
}

} // namespace

JniGame::JniGame() {
    nextResolveAttempt_ = std::chrono::steady_clock::now();
}

JniGame::~JniGame() {
    cleanupLocals();

    JNIEnv* env = jvm_.env();
    if (!env) {
        return;
    }

    auto release = [env](jclass& clazz) {
        if (clazz) {
            env->DeleteGlobalRef(clazz);
            clazz = nullptr;
        }
    };

    release(mcClass_);
    release(playerClass_);
    release(entityClass_);
    release(keyMappingClass_);
    release(optionsClass_);
    release(abilitiesClass_);
    release(windowClass_);
    release(optionClass_);
    release(boatClass_);
    release(doubleClass_);
    release(integerClass_);
    release(vecClass_);
    release(mouseHandlerClass_);

    if (gameLoader_) {
        env->DeleteGlobalRef(gameLoader_);
        gameLoader_ = nullptr;
    }

    release(blockPosClass_);
    release(mutablePosClass_);
    release(blockStateClass_);
    release(resourceLocationClass_);

    if (mutablePos_) {
        env->DeleteGlobalRef(mutablePos_);
        mutablePos_ = nullptr;
    }
    if (blockRegistry_) {
        env->DeleteGlobalRef(blockRegistry_);
        blockRegistry_ = nullptr;
    }

    for (jobject object : oreBlockObjects_) {
        if (object) {
            env->DeleteGlobalRef(object);
        }
    }
    oreBlockObjects_.clear();
    oreBlockGroups_.clear();
}

void JniGame::noteMissing(const char* kind, const char* name) {
    if (errorLogged_) {
        // Already reported this pass; avoid flooding the log on retries.
        return;
    }
    const std::string key = std::string(kind) + ":" + name;
    if (std::find(missingLogged_.begin(), missingLogged_.end(), key) != missingLogged_.end()) {
        return;
    }
    missingLogged_.push_back(key);
    NEO_LOG_ERROR("Missing %s: %s (check the Minecraft 26.3 mappings)", kind, name);
}

void JniGame::checkException(const char* context) {
    JNIEnv* env = jvm_.env();
    if (!env || !env->ExceptionCheck()) {
        return;
    }

    env->ExceptionClear();
    if (!errorLogged_) {
        NEO_LOG_ERROR("JNI exception in %s", context);
        errorLogged_ = true;
    }
}

jmethodID JniGame::method(jclass owner, const char* name, const char* signature) {
    JNIEnv* env = jvm_.env();
    if (!env || !owner) {
        return nullptr;
    }
    jmethodID id = env->GetMethodID(owner, name, signature);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (!id) {
        noteMissing("method", name);
    }
    return id;
}

jmethodID JniGame::staticMethod(jclass owner, const char* name, const char* signature) {
    JNIEnv* env = jvm_.env();
    if (!env || !owner) {
        return nullptr;
    }
    jmethodID id = env->GetStaticMethodID(owner, name, signature);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (!id) {
        noteMissing("static method", name);
    }
    return id;
}

jfieldID JniGame::field(jclass owner, const char* name, const char* signature) {
    JNIEnv* env = jvm_.env();
    if (!env || !owner) {
        return nullptr;
    }
    jfieldID id = env->GetFieldID(owner, name, signature);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (!id) {
        noteMissing("field", name);
    }
    return id;
}

jclass JniGame::loadClass(const char* name) {
    JNIEnv* env = jvm_.env();
    if (!env) {
        return nullptr;
    }

    jclass direct = env->FindClass(name);
    if (direct) {
        return direct;
    }
    env->ExceptionClear();

    const std::string binary = binaryClassName(name);

    // The loader that worked for an earlier game class is by far the fastest
    // path. It is cached as a global reference because resolve() is retried.
    if (gameLoader_) {
        if (jclass result = forNameWithLoader(env, gameLoader_, binary.c_str())) {
            return result;
        }
    }

    jclass threadClass = env->FindClass("java/lang/Thread");
    jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
    if (!threadClass || !classLoaderClass) {
        if (threadClass) env->DeleteLocalRef(threadClass);
        if (classLoaderClass) env->DeleteLocalRef(classLoaderClass);
        env->ExceptionClear();
        return nullptr;
    }

    jmethodID currentThread = env->GetStaticMethodID(threadClass, "currentThread", "()Ljava/lang/Thread;");
    jmethodID getContextLoader = env->GetMethodID(threadClass, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
    jmethodID getSystemLoader = env->GetStaticMethodID(classLoaderClass, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    const auto accept = [&](jobject loader) -> jclass {
        if (!loader) {
            return nullptr;
        }
        jclass result = forNameWithLoader(env, loader, binary.c_str());
        if (result && !gameLoader_) {
            gameLoader_ = env->NewGlobalRef(loader);
            if (gameLoader_) {
                NEO_LOG_INFO("JNI class loader resolved");
            }
        }
        return result;
    };

    jobject loaders[2] = {nullptr, nullptr};
    int loaderCount = 0;

    if (currentThread && getContextLoader) {
        jobject thread = env->CallStaticObjectMethod(threadClass, currentThread);
        if (thread) {
            loaders[loaderCount] = env->CallObjectMethod(thread, getContextLoader);
            env->DeleteLocalRef(thread);
            if (loaders[loaderCount]) {
                ++loaderCount;
            }
        }
    }
    if (getSystemLoader) {
        loaders[loaderCount] = env->CallStaticObjectMethod(classLoaderClass, getSystemLoader);
        if (loaders[loaderCount]) {
            ++loaderCount;
        }
    }

    for (int i = 0; i < loaderCount; ++i) {
        jclass result = accept(loaders[i]);
        if (result) {
            for (int j = 0; j < loaderCount; ++j) {
                env->DeleteLocalRef(loaders[j]);
            }
            env->DeleteLocalRef(threadClass);
            env->DeleteLocalRef(classLoaderClass);
            return result;
        }
    }
    for (int j = 0; j < loaderCount; ++j) {
        env->DeleteLocalRef(loaders[j]);
    }

    // Some launchers load Minecraft from a child loader that is only visible
    // through the context class loader of a game thread. Try every thread.
    jmethodID getAllStackTraces = getContextLoader
        ? env->GetStaticMethodID(threadClass, "getAllStackTraces", "()Ljava/util/Map;")
        : nullptr;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        getAllStackTraces = nullptr;
    }

    jobject map = getAllStackTraces ? env->CallStaticObjectMethod(threadClass, getAllStackTraces) : nullptr;
    if (map) {
        jclass mapClass = env->GetObjectClass(map);
        jmethodID keySetMethod = env->GetMethodID(mapClass, "keySet", "()Ljava/util/Set;");
        jobject set = keySetMethod ? env->CallObjectMethod(map, keySetMethod) : nullptr;
        if (set) {
            jclass setClass = env->GetObjectClass(set);
            jmethodID toArray = env->GetMethodID(setClass, "toArray", "()[Ljava/lang/Object;");
            jobjectArray threads = toArray
                ? static_cast<jobjectArray>(env->CallObjectMethod(set, toArray))
                : nullptr;
            if (threads) {
                const jsize count = env->GetArrayLength(threads);
                for (jsize i = 0; i < count; ++i) {
                    jobject thread = env->GetObjectArrayElement(threads, i);
                    if (!thread) {
                        continue;
                    }
                    jobject loader = env->CallObjectMethod(thread, getContextLoader);
                    env->DeleteLocalRef(thread);
                    if (!loader) {
                        continue;
                    }
                    jclass result = accept(loader);
                    env->DeleteLocalRef(loader);
                    if (result) {
                        env->DeleteLocalRef(threads);
                        env->DeleteLocalRef(setClass);
                        env->DeleteLocalRef(set);
                        env->DeleteLocalRef(mapClass);
                        env->DeleteLocalRef(map);
                        env->DeleteLocalRef(threadClass);
                        env->DeleteLocalRef(classLoaderClass);
                        return result;
                    }
                }
                env->DeleteLocalRef(threads);
            }
            env->DeleteLocalRef(setClass);
            env->DeleteLocalRef(set);
        }
        env->DeleteLocalRef(mapClass);
        env->DeleteLocalRef(map);
    }

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    env->DeleteLocalRef(threadClass);
    env->DeleteLocalRef(classLoaderClass);
    return nullptr;
}

bool JniGame::resolve() {
    if (!attachAttempted_) {
        attachAttempted_ = true;
        if (!jvm_.attach()) {
            NEO_LOG_WARN("Waiting for the Minecraft JVM...");
            return false;
        }
    }

    JNIEnv* env = jvm_.env();
    if (!env) {
        return false;
    }

    const auto globalClass = [this, env](const char* name) -> jclass {
        jclass local = loadClass(name);
        if (!local) {
            noteMissing("class", name);
            return nullptr;
        }
        jclass global = static_cast<jclass>(env->NewGlobalRef(local));
        env->DeleteLocalRef(local);
        return global;
    };

    missingLogged_.clear();
    errorLogged_ = false;

    mcClass_ = globalClass("net/minecraft/client/Minecraft");
    playerClass_ = globalClass("net/minecraft/client/player/LocalPlayer");
    entityClass_ = globalClass("net/minecraft/world/entity/Entity");
    keyMappingClass_ = globalClass("net/minecraft/client/KeyMapping");
    optionsClass_ = globalClass("net/minecraft/client/Options");
    abilitiesClass_ = globalClass("net/minecraft/world/entity/player/Abilities");
    windowClass_ = globalClass("com/mojang/blaze3d/platform/Window");
    optionClass_ = globalClass("net/minecraft/client/OptionInstance");
    boatClass_ = globalClass("net/minecraft/world/entity/vehicle/boat/AbstractBoat");
    doubleClass_ = globalClass("java/lang/Double");
    integerClass_ = globalClass("java/lang/Integer");
    vecClass_ = globalClass("net/minecraft/world/phys/Vec3");
    mouseHandlerClass_ = globalClass("net/minecraft/client/MouseHandler");

    if (!mcClass_ || !playerClass_ || !entityClass_ || !keyMappingClass_ || !optionsClass_ ||
        !abilitiesClass_ || !windowClass_ || !optionClass_ || !doubleClass_ || !integerClass_ ||
        !vecClass_ || !mouseHandlerClass_) {
        return false;
    }

    getInstance_ = staticMethod(mcClass_, "getInstance", "()Lnet/minecraft/client/Minecraft;");
    getFps_ = method(mcClass_, "getFps", "()I");
    getWindow_ = method(mcClass_, "getWindow", "()Lcom/mojang/blaze3d/platform/Window;");
    getSingleplayerServer_ = method(mcClass_, "getSingleplayerServer", "()Lnet/minecraft/client/server/IntegratedServer;");

    mcPlayer_ = field(mcClass_, "player", "Lnet/minecraft/client/player/LocalPlayer;");
    mcLevel_ = field(mcClass_, "level", "Lnet/minecraft/client/multiplayer/ClientLevel;");
    mcOptions_ = field(mcClass_, "options", "Lnet/minecraft/client/Options;");
    mcMouseHandler_ = field(mcClass_, "mouseHandler", "Lnet/minecraft/client/MouseHandler;");
    // Camera is the interpolated render pose + true FOV; optional (X-Ray only).
    mcGameRenderer_ = field(mcClass_, "gameRenderer", "Lnet/minecraft/client/renderer/GameRenderer;");
    {
        jclass rendererClass = loadClass("net/minecraft/client/renderer/GameRenderer");
        if (rendererClass) {
            getMainCamera_ = method(rendererClass, "mainCamera", "()Lnet/minecraft/client/Camera;");
            env->DeleteLocalRef(rendererClass);
        }
        jclass cameraClass = loadClass("net/minecraft/client/Camera");
        if (cameraClass) {
            cameraPosition_ = method(cameraClass, "position", "()Lnet/minecraft/world/phys/Vec3;");
            cameraXRot_ = method(cameraClass, "xRot", "()F");
            cameraYRot_ = method(cameraClass, "yRot", "()F");
            cameraGetFov_ = method(cameraClass, "getFov", "()F");
            cameraInitialized_ = method(cameraClass, "isInitialized", "()Z");
            env->DeleteLocalRef(cameraClass);
        }
    }

    keyUp_ = field(optionsClass_, "keyUp", "Lnet/minecraft/client/KeyMapping;");
    keyBack_ = field(optionsClass_, "keyDown", "Lnet/minecraft/client/KeyMapping;");
    keyLeft_ = field(optionsClass_, "keyLeft", "Lnet/minecraft/client/KeyMapping;");
    keyRight_ = field(optionsClass_, "keyRight", "Lnet/minecraft/client/KeyMapping;");
    keyJump_ = field(optionsClass_, "keyJump", "Lnet/minecraft/client/KeyMapping;");
    keyShift_ = field(optionsClass_, "keyShift", "Lnet/minecraft/client/KeyMapping;");
    keySprint_ = field(optionsClass_, "keySprint", "Lnet/minecraft/client/KeyMapping;");

    fallDistance_ = field(entityClass_, "fallDistance", "D");
    invulnerableTime_ = field(entityClass_, "invulnerableTime", "I");
    // hurtTime is declared on LivingEntity, not Entity; resolving from the
    // player class walks the hierarchy and finds it.
    hurtTime_ = field(playerClass_, "hurtTime", "I");
    noPhysics_ = field(entityClass_, "noPhysics", "Z");
    // OptionInstance#value is the backing field; writing it directly bypasses
    // validateValue() which clamps gamma to its slider range (breaks Fullbright).
    optionValue_ = field(optionClass_, "value", "Ljava/lang/Object;");
    getUuid_ = method(entityClass_, "getUUID", "()Ljava/util/UUID;");
    flying_ = field(abilitiesClass_, "flying", "Z");
    mayFly_ = field(abilitiesClass_, "mayfly", "Z");
    vecX_ = field(vecClass_, "x", "D");
    vecY_ = field(vecClass_, "y", "D");
    vecZ_ = field(vecClass_, "z", "D");

    onGround_ = method(entityClass_, "onGround", "()Z");
    isPassenger_ = method(entityClass_, "isPassenger", "()Z");
    getVehicle_ = method(entityClass_, "getVehicle", "()Lnet/minecraft/world/entity/Entity;");
    getDeltaMovement_ = method(entityClass_, "getDeltaMovement", "()Lnet/minecraft/world/phys/Vec3;");
    setDeltaMovement_ = method(entityClass_, "setDeltaMovement", "(DDD)V");
    getX_ = method(entityClass_, "getX", "()D");
    getY_ = method(entityClass_, "getY", "()D");
    getZ_ = method(entityClass_, "getZ", "()D");
    // Camera rotation + eye height are optional: X-Ray needs them, the rest
    // of the client keeps working when they move between versions.
    getYRot_ = method(entityClass_, "getYRot", "()F");
    if (!getYRot_) {
        getYRot_ = method(playerClass_, "getYRot", "()F");
    }
    getXRot_ = method(entityClass_, "getXRot", "()F");
    if (!getXRot_) {
        getXRot_ = method(playerClass_, "getXRot", "()F");
    }
    getEyeHeight_ = method(entityClass_, "getEyeHeight", "()F");
    jumpFromGround_ = method(playerClass_, "jumpFromGround", "()V");
    setSprinting_ = method(playerClass_, "setSprinting", "(Z)V");
    isDeadOrDying_ = method(playerClass_, "isDeadOrDying", "()Z");
    getAbilities_ = method(playerClass_, "getAbilities", "()Lnet/minecraft/world/entity/player/Abilities;");
    respawn_ = method(playerClass_, "respawn", "()V");
    onUpdateAbilities_ = method(playerClass_, "onUpdateAbilities", "()V");
    setFlyingSpeed_ = method(abilitiesClass_, "setFlyingSpeed", "(F)V");
    getFlyingSpeed_ = method(abilitiesClass_, "getFlyingSpeed", "()F");
    keySetDown_ = method(keyMappingClass_, "setDown", "(Z)V");
    keyIsDown_ = method(keyMappingClass_, "isDown", "()Z");
    keySetAll_ = staticMethod(keyMappingClass_, "setAll", "()V");
    keyReleaseAll_ = staticMethod(keyMappingClass_, "releaseAll", "()V");
    releaseMouse_ = method(mouseHandlerClass_, "releaseMouse", "()V");
    grabMouse_ = method(mouseHandlerClass_, "grabMouse", "()V");
    isMouseGrabbed_ = method(mouseHandlerClass_, "isMouseGrabbed", "()Z");

    getGamma_ = method(optionsClass_, "gamma", "()Lnet/minecraft/client/OptionInstance;");
    getFov_ = method(optionsClass_, "fov", "()Lnet/minecraft/client/OptionInstance;");
    optionSet_ = method(optionClass_, "set", "(Ljava/lang/Object;)V");
    optionGet_ = method(optionClass_, "get", "()Ljava/lang/Object;");
    doubleValueOf_ = staticMethod(doubleClass_, "valueOf", "(D)Ljava/lang/Double;");
    doubleValue_ = method(doubleClass_, "doubleValue", "()D");
    integerValueOf_ = staticMethod(integerClass_, "valueOf", "(I)Ljava/lang/Integer;");
    integerValue_ = method(integerClass_, "intValue", "()I");

    getGuiScaledWidth_ = method(windowClass_, "getGuiScaledWidth", "()I");
    getGuiScaledHeight_ = method(windowClass_, "getGuiScaledHeight", "()I");
    windowHandle_ = method(windowClass_, "handle", "()J");

    // Integrated-server access for a real NoFall. In singleplayer the server is
    // authoritative for fall damage, so the client-side fallDistance alone
    // cannot prevent it. Optional: if these do not resolve, NoFall keeps the
    // client-side attempt and everything else continues to work.
    {
        jclass serverClass = loadClass("net/minecraft/server/MinecraftServer");
        if (serverClass) {
            serverGetPlayerList_ = method(serverClass, "getPlayerList", "()Lnet/minecraft/server/players/PlayerList;");
            env->DeleteLocalRef(serverClass);
        }
        jclass listClass = loadClass("net/minecraft/server/players/PlayerList");
        if (listClass) {
            playerListGetPlayer_ = method(listClass, "getPlayer", "(Ljava/util/UUID;)Lnet/minecraft/server/level/ServerPlayer;");
            env->DeleteLocalRef(listClass);
        }
    }

    checkException("resolve");

    const bool complete =
        getInstance_ && getFps_ && getWindow_ && getSingleplayerServer_ &&
        mcPlayer_ && mcLevel_ && mcOptions_ && mcMouseHandler_ &&
        keyUp_ && keyBack_ && keyLeft_ && keyRight_ && keyJump_ && keyShift_ && keySprint_ &&
        fallDistance_ && noPhysics_ && flying_ && mayFly_ && vecX_ && vecY_ && vecZ_ &&
        onGround_ && isPassenger_ && getVehicle_ && getDeltaMovement_ && setDeltaMovement_ &&
        getX_ && getY_ && getZ_ && jumpFromGround_ && setSprinting_ && isDeadOrDying_ &&
        getAbilities_ && respawn_ && setFlyingSpeed_ && getFlyingSpeed_ &&
        keySetDown_ && keyIsDown_ && keySetAll_ && getGamma_ && getFov_ && optionSet_ && optionGet_ &&
        doubleValueOf_ && doubleValue_ && integerValueOf_ && integerValue_ && windowHandle_ &&
        releaseMouse_ && grabMouse_ && isMouseGrabbed_;

    if (!complete) {
        NEO_LOG_ERROR("Minecraft 26.3 JNI layout did not match the expected 1.0.0 bridge");
        return false;
    }

    resolved_ = true;
    blockRegistry_ = nullptr; // ore scan may re-resolve lazily
    oreScanReady_ = false;
    oreBlockCacheTried_ = false;
    NEO_LOG_INFO("JNI bridge ready (Minecraft %s)", NEO_MC_VERSION);
    return true;
}

jobject JniGame::callGetInstance() {
    JNIEnv* env = jvm_.env();
    if (!env || !getInstance_) {
        return nullptr;
    }
    return env->CallStaticObjectMethod(mcClass_, getInstance_);
}

void JniGame::cleanupLocals() {
    JNIEnv* env = jvm_.env();
    if (!env) {
        mc_ = player_ = level_ = options_ = vehicle_ = nullptr;
        return;
    }

    auto release = [env](jobject& object) {
        if (object) {
            env->DeleteLocalRef(object);
            object = nullptr;
        }
    };

    release(vehicle_);
    release(options_);
    release(level_);
    release(player_);
    release(mc_);
}

void JniGame::refresh() {
    cleanupLocals();
    snapshot_ = GameSnapshot{};

    if (!resolved_) {
        const auto now = std::chrono::steady_clock::now();
        if (now < nextResolveAttempt_) {
            return;
        }
        if (!resolve()) {
            nextResolveAttempt_ = now + kResolveRetry;
            return;
        }
    }

    JNIEnv* env = jvm_.env();
    if (!env) {
        return;
    }

    mc_ = callGetInstance();
    checkException("Minecraft.getInstance");
    if (!mc_) {
        cleanupLocals();
        return;
    }

    snapshot_.available = true;
    options_ = env->GetObjectField(mc_, mcOptions_);
    player_ = env->GetObjectField(mc_, mcPlayer_);
    level_ = env->GetObjectField(mc_, mcLevel_);

    snapshot_.fps = env->CallIntMethod(mc_, getFps_);

    jobject server = env->CallObjectMethod(mc_, getSingleplayerServer_);
    snapshot_.singleplayer = server != nullptr;
    if (server) env->DeleteLocalRef(server);

    if (!player_ || !level_) {
        checkException("Minecraft fields");
        cleanupLocals();
        return;
    }

    snapshot_.inGame = true;
    snapshot_.x = env->CallDoubleMethod(player_, getX_);
    snapshot_.y = env->CallDoubleMethod(player_, getY_);
    snapshot_.z = env->CallDoubleMethod(player_, getZ_);
    snapshot_.onGround = env->CallBooleanMethod(player_, onGround_) == JNI_TRUE;
    snapshot_.dead = env->CallBooleanMethod(player_, isDeadOrDying_) == JNI_TRUE;
    captureRotation();

    // Prefer the render camera's interpolated pose/FOV for X-Ray so markers do
    // not drift or blink while the player moves (the player pose is tick-based).
    snapshot_.hasCamera = false;
    if (mcGameRenderer_ && getMainCamera_ && cameraPosition_) {
        jobject renderer = env->GetObjectField(mc_, mcGameRenderer_);
        if (renderer) {
            jobject camera = env->CallObjectMethod(renderer, getMainCamera_);
            env->DeleteLocalRef(renderer);
            if (camera) {
                const bool initialized = !cameraInitialized_ ||
                    env->CallBooleanMethod(camera, cameraInitialized_) == JNI_TRUE;
                if (initialized) {
                    jobject camPos = env->CallObjectMethod(camera, cameraPosition_);
                    if (camPos) {
                        snapshot_.camX = env->GetDoubleField(camPos, vecX_);
                        snapshot_.camY = env->GetDoubleField(camPos, vecY_);
                        snapshot_.camZ = env->GetDoubleField(camPos, vecZ_);
                        env->DeleteLocalRef(camPos);
                        snapshot_.camYaw = cameraYRot_ ? env->CallFloatMethod(camera, cameraYRot_) : snapshot_.yaw;
                        snapshot_.camPitch = cameraXRot_ ? env->CallFloatMethod(camera, cameraXRot_) : snapshot_.pitch;
                        snapshot_.camFov = cameraGetFov_
                            ? env->CallFloatMethod(camera, cameraGetFov_)
                            : static_cast<float>(snapshot_.fov);
                        snapshot_.hasCamera = true;
                    }
                }
                env->DeleteLocalRef(camera);
            }
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    vehicle_ = env->CallObjectMethod(player_, getVehicle_);
    if (vehicle_) {
        snapshot_.inBoat = boatClass_ && env->IsInstanceOf(vehicle_, boatClass_) == JNI_TRUE;
    }

    jobject playerAbilities = env->CallObjectMethod(player_, getAbilities_);
    if (playerAbilities) {
        snapshot_.flying = env->GetBooleanField(playerAbilities, flying_) == JNI_TRUE;
        snapshot_.mayFly = env->GetBooleanField(playerAbilities, mayFly_) == JNI_TRUE;
        env->DeleteLocalRef(playerAbilities);
    }

    snapshot_.gamma = gamma();
    snapshot_.fov = fov();

    checkException("refresh");
}

void JniGame::endFrame() {
    cleanupLocals();
}

jobject JniGame::keyMapping(GameKey key) const {
    JNIEnv* env = jvm_.env();
    if (!env || !options_) {
        return nullptr;
    }

    jfieldID field = nullptr;
    switch (key) {
    case GameKey::Jump: field = keyJump_; break;
    case GameKey::Shift: field = keyShift_; break;
    case GameKey::Sprint: field = keySprint_; break;
    case GameKey::Forward: field = keyUp_; break;
    case GameKey::Back: field = keyBack_; break;
    case GameKey::Left: field = keyLeft_; break;
    case GameKey::Right: field = keyRight_; break;
    }

    return field ? env->GetObjectField(options_, field) : nullptr;
}

bool JniGame::keyDown(GameKey key) const {
    JNIEnv* env = jvm_.env();
    if (!env) {
        return false;
    }

    jobject mapping = keyMapping(key);
    if (!mapping) {
        return false;
    }

    const bool down = env->CallBooleanMethod(mapping, keyIsDown_) == JNI_TRUE;
    env->DeleteLocalRef(mapping);
    return down;
}

void JniGame::setKey(GameKey key, bool pressed) {
    JNIEnv* env = jvm_.env();
    if (!env) {
        return;
    }

    jobject mapping = keyMapping(key);
    if (!mapping) {
        return;
    }

    env->CallVoidMethod(mapping, keySetDown_, pressed ? JNI_TRUE : JNI_FALSE);
    env->DeleteLocalRef(mapping);
}

void JniGame::jump() {
    JNIEnv* env = jvm_.env();
    if (env && player_) {
        env->CallVoidMethod(player_, jumpFromGround_);
    }
}

void JniGame::setFallDistance(double value) {
    JNIEnv* env = jvm_.env();
    if (env && player_) {
        env->SetDoubleField(player_, fallDistance_, value);
    }
}

bool JniGame::onGround() const {
    JNIEnv* env = jvm_.env();
    if (!env || !player_ || !onGround_) {
        return false;
    }
    return env->CallBooleanMethod(player_, onGround_) == JNI_TRUE;
}

void JniGame::setInvulnerableTime(int value) {
    JNIEnv* env = jvm_.env();
    if (env && player_ && invulnerableTime_) {
        env->SetIntField(player_, invulnerableTime_, value);
    }
}

void JniGame::setHurtTime(int value) {
    JNIEnv* env = jvm_.env();
    if (env && player_ && hurtTime_) {
        env->SetIntField(player_, hurtTime_, value);
    }
}

// Resolves the authoritative ServerPlayer for this LocalPlayer in singleplayer.
// Returns a local reference the caller must delete, or nullptr when not in a
// singleplayer world / symbols unavailable.
jobject JniGame::serverPlayer() {
    JNIEnv* env = jvm_.env();
    if (!env || !player_ || !serverGetPlayerList_ || !playerListGetPlayer_ || !getUuid_) {
        return nullptr;
    }
    jobject mc = callGetInstance();
    if (!mc) {
        return nullptr;
    }
    jobject server = env->CallObjectMethod(mc, getSingleplayerServer_);
    env->DeleteLocalRef(mc);
    if (!server) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return nullptr;
    }
    jobject list = env->CallObjectMethod(server, serverGetPlayerList_);
    env->DeleteLocalRef(server);
    if (!list) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return nullptr;
    }
    jobject uuid = env->CallObjectMethod(player_, getUuid_);
    if (!uuid) {
        env->DeleteLocalRef(list);
        if (env->ExceptionCheck()) env->ExceptionClear();
        return nullptr;
    }
    jobject sp = env->CallObjectMethod(list, playerListGetPlayer_, uuid);
    env->DeleteLocalRef(uuid);
    env->DeleteLocalRef(list);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return sp;
}

void JniGame::zeroServerFallDistance() {
    JNIEnv* env = jvm_.env();
    if (!env || !fallDistance_) {
        return;
    }
    jobject sp = serverPlayer();
    if (sp) {
        env->SetDoubleField(sp, fallDistance_, 0.0);
        env->DeleteLocalRef(sp);
    }
}

void JniGame::setServerInvulnerableTime(int value) {
    JNIEnv* env = jvm_.env();
    if (!env || !invulnerableTime_) {
        return;
    }
    jobject sp = serverPlayer();
    if (sp) {
        env->SetIntField(sp, invulnerableTime_, value);
        env->DeleteLocalRef(sp);
    }
}

void JniGame::setNoPhysics(bool value) {
    JNIEnv* env = jvm_.env();
    if (env && player_) {
        env->SetBooleanField(player_, noPhysics_, value ? JNI_TRUE : JNI_FALSE);
    }
}

Vec3 JniGame::deltaMovement() const {
    JNIEnv* env = jvm_.env();
    if (!env || !player_) {
        return {};
    }

    jobject vector = env->CallObjectMethod(player_, getDeltaMovement_);
    if (!vector) {
        return {};
    }

    Vec3 result{
        env->GetDoubleField(vector, vecX_),
        env->GetDoubleField(vector, vecY_),
        env->GetDoubleField(vector, vecZ_)};
    env->DeleteLocalRef(vector);
    return result;
}

void JniGame::setDeltaMovement(const Vec3& value) {
    JNIEnv* env = jvm_.env();
    if (env && player_) {
        env->CallVoidMethod(player_, setDeltaMovement_, value.x, value.y, value.z);
    }
}

Vec3 JniGame::vehicleDeltaMovement() const {
    JNIEnv* env = jvm_.env();
    if (!env || !vehicle_) {
        return {};
    }

    jobject vector = env->CallObjectMethod(vehicle_, getDeltaMovement_);
    if (!vector) {
        return {};
    }

    Vec3 result{
        env->GetDoubleField(vector, vecX_),
        env->GetDoubleField(vector, vecY_),
        env->GetDoubleField(vector, vecZ_)};
    env->DeleteLocalRef(vector);
    return result;
}

void JniGame::setVehicleDeltaMovement(const Vec3& value) {
    JNIEnv* env = jvm_.env();
    if (env && vehicle_) {
        env->CallVoidMethod(vehicle_, setDeltaMovement_, value.x, value.y, value.z);
    }
}

jobject JniGame::abilities() const {
    JNIEnv* env = jvm_.env();
    if (!env || !player_) {
        return nullptr;
    }
    return env->CallObjectMethod(player_, getAbilities_);
}

void JniGame::setFlying(bool value) {
    JNIEnv* env = jvm_.env();
    if (!env) {
        return;
    }

    jobject valueObject = abilities();
    if (valueObject) {
        const jboolean target = value ? JNI_TRUE : JNI_FALSE;
        if (env->GetBooleanField(valueObject, flying_) == target) {
            env->DeleteLocalRef(valueObject);
            return;
        }
        env->SetBooleanField(valueObject, flying_, target);
        env->DeleteLocalRef(valueObject);
        if (onUpdateAbilities_ && player_) {
            env->CallVoidMethod(player_, onUpdateAbilities_);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }
}

void JniGame::setMayFly(bool value) {
    JNIEnv* env = jvm_.env();
    if (!env) {
        return;
    }

    jobject valueObject = abilities();
    if (valueObject) {
        const jboolean target = value ? JNI_TRUE : JNI_FALSE;
        if (env->GetBooleanField(valueObject, mayFly_) == target) {
            env->DeleteLocalRef(valueObject);
            return;
        }
        env->SetBooleanField(valueObject, mayFly_, target);
        env->DeleteLocalRef(valueObject);
        if (onUpdateAbilities_ && player_) {
            env->CallVoidMethod(player_, onUpdateAbilities_);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }
}

void JniGame::setFlyingSpeed(float value) {
    JNIEnv* env = jvm_.env();
    if (!env) {
        return;
    }

    jobject valueObject = abilities();
    if (valueObject) {
        const float current = getFlyingSpeed_
            ? env->CallFloatMethod(valueObject, getFlyingSpeed_)
            : -1.0f;
        if (std::fabs(current - value) < 0.0001f) {
            env->DeleteLocalRef(valueObject);
            return;
        }
        env->CallVoidMethod(valueObject, setFlyingSpeed_, value);
        env->DeleteLocalRef(valueObject);
        if (onUpdateAbilities_ && player_) {
            env->CallVoidMethod(player_, onUpdateAbilities_);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }
}

float JniGame::flyingSpeed() const {
    JNIEnv* env = jvm_.env();
    if (!env) {
        return 0.05f;
    }

    jobject valueObject = abilities();
    if (!valueObject) {
        return 0.05f;
    }

    const float result = env->CallFloatMethod(valueObject, getFlyingSpeed_);
    env->DeleteLocalRef(valueObject);
    return result;
}

double JniGame::optionNumber(const char* methodName, const char* methodSignature, bool integer) const {
    JNIEnv* env = jvm_.env();
    if (!env || !options_) {
        return 0.0;
    }

    jmethodID method = env->GetMethodID(optionsClass_, methodName, methodSignature);
    if (!method) {
        return 0.0;
    }

    jobject option = env->CallObjectMethod(options_, method);
    if (!option) {
        return 0.0;
    }

    jobject valueObject = env->CallObjectMethod(option, optionGet_);
    env->DeleteLocalRef(option);
    if (!valueObject) {
        return 0.0;
    }

    double result = 0.0;
    if (integer) {
        result = env->CallIntMethod(valueObject, integerValue_);
    } else {
        result = env->CallDoubleMethod(valueObject, doubleValue_);
    }

    env->DeleteLocalRef(valueObject);
    return result;
}

void JniGame::setOptionNumber(const char* methodName, const char* methodSignature, bool integer, double value) {
    JNIEnv* env = jvm_.env();
    if (!env || !options_) {
        return;
    }

    jmethodID method = env->GetMethodID(optionsClass_, methodName, methodSignature);
    if (!method) {
        return;
    }

    jobject option = env->CallObjectMethod(options_, method);
    if (!option) {
        return;
    }

    jobject boxed = integer
        ? env->CallStaticObjectMethod(integerClass_, integerValueOf_, static_cast<jint>(value))
        : env->CallStaticObjectMethod(doubleClass_, doubleValueOf_, value);

    if (boxed) {
        env->CallVoidMethod(option, optionSet_, boxed);
        env->DeleteLocalRef(boxed);
    }
    env->DeleteLocalRef(option);
}

double JniGame::gamma() const {
    return optionNumber("gamma", "()Lnet/minecraft/client/OptionInstance;", false);
}

void JniGame::setGamma(double value) {
    JNIEnv* env = jvm_.env();
    if (!env || !options_ || !getGamma_) {
        return;
    }
    jobject option = env->CallObjectMethod(options_, getGamma_);
    if (!option) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }
    jobject boxed = env->CallStaticObjectMethod(doubleClass_, doubleValueOf_, value);
    if (boxed) {
        if (optionValue_) {
            // Write the backing value field directly: OptionInstance#set runs
            // validateValue(), which clamps gamma to the 0..1 slider range and
            // would cap Fullbright.
            env->SetObjectField(option, optionValue_, boxed);
        } else {
            env->CallVoidMethod(option, optionSet_, boxed);
        }
        env->DeleteLocalRef(boxed);
    }
    env->DeleteLocalRef(option);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

int JniGame::fov() const {
    return static_cast<int>(optionNumber("fov", "()Lnet/minecraft/client/OptionInstance;", true));
}

void JniGame::setFov(int value) {
    setOptionNumber("fov", "()Lnet/minecraft/client/OptionInstance;", true, value);
}

void JniGame::respawn() {
    JNIEnv* env = jvm_.env();
    if (env && player_) {
        env->CallVoidMethod(player_, respawn_);
    }
}

void JniGame::setMenuCaptured(bool captured) {
    JNIEnv* env = jvm_.env();
    if (!env || !resolved_ || !mcMouseHandler_) {
        return;
    }

    // Fetch the instance for the current thread instead of reusing the cached
    // local reference from refresh(): the key handler and the render hook can
    // run on different threads, and JNI local refs are thread bound.
    jobject minecraft = callGetInstance();
    if (!minecraft) {
        return;
    }

    jobject mouse = env->GetObjectField(minecraft, mcMouseHandler_);
    env->DeleteLocalRef(minecraft);
    if (!mouse) {
        return;
    }

    if (captured) {
        if (!menuCaptured_) {
            menuCaptured_ = true;
            resumeMouseGrab_ = isMouseGrabbed_
                ? env->CallBooleanMethod(mouse, isMouseGrabbed_) == JNI_TRUE
                : true;
        }

        // releaseMouse() only touches mouseGrabbed and the cursor; it is safe
        // to call every frame and also handles focus changes that re-grab.
        env->CallVoidMethod(mouse, releaseMouse_);

        // Release every key mapping so a key held while the menu opened does
        // not keep moving the player behind the overlay.
        if (keyReleaseAll_) {
            env->CallStaticVoidMethod(keyMappingClass_, keyReleaseAll_);
        }
    } else if (menuCaptured_) {
        menuCaptured_ = false;
        if (resumeMouseGrab_) {
            env->CallVoidMethod(mouse, grabMouse_);
            resumeMouseGrab_ = false;
        }
        // Re-read the physical keyboard so keys held while the menu was open
        // resume working immediately after closing it.
        if (keySetAll_) {
            env->CallStaticVoidMethod(keyMappingClass_, keySetAll_);
        }
    }

    env->DeleteLocalRef(mouse);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

void* JniGame::sdlWindow() const {
    JNIEnv* env = jvm_.env();
    if (!env || !mc_) {
        return nullptr;
    }

    jobject window = env->CallObjectMethod(mc_, getWindow_);
    if (!window) {
        return nullptr;
    }

    const jlong handle = env->CallLongMethod(window, windowHandle_);
    env->DeleteLocalRef(window);
    return reinterpret_cast<void*>(static_cast<intptr_t>(handle));
}

void JniGame::captureRotation() {
    JNIEnv* env = jvm_.env();
    if (!env || !player_) {
        return;
    }

    if (getYRot_) {
        snapshot_.yaw = env->CallFloatMethod(player_, getYRot_);
    }
    if (getXRot_) {
        snapshot_.pitch = env->CallFloatMethod(player_, getXRot_);
    }
    if (getEyeHeight_) {
        const float eyeHeight = env->CallFloatMethod(player_, getEyeHeight_);
        if (eyeHeight > 0.1f && eyeHeight < 8.0f) {
            snapshot_.eyeHeight = eyeHeight;
        }
    }
    checkException("captureRotation");
}

jobject JniGame::findBlockObject(const char* name) {
    JNIEnv* env = jvm_.env();
    if (!env || !blockRegistry_ || !registryGet_) {
        return nullptr;
    }

    jobject location = nullptr;

    // 26.3 calls this Identifier; older versions used ResourceLocation. The
    // factory pair below covers both while still accepting a bare block id.
    if (resourceLocationFromNamespace_) {
        jstring namespaceText = env->NewStringUTF("minecraft");
        jstring pathText = env->NewStringUTF(name);
        location = env->CallStaticObjectMethod(
            resourceLocationClass_, resourceLocationFromNamespace_, namespaceText, pathText);
        if (namespaceText) env->DeleteLocalRef(namespaceText);
        if (pathText) env->DeleteLocalRef(pathText);
        checkException("Identifier.fromNamespaceAndPath");
    }
    if (!location && resourceLocationParse_) {
        const std::string fullName = std::string("minecraft:") + name;
        jstring text = env->NewStringUTF(fullName.c_str());
        location = env->CallStaticObjectMethod(resourceLocationClass_, resourceLocationParse_, text);
        if (text) env->DeleteLocalRef(text);
        checkException("Identifier.parse");
    }
    if (!location && resourceLocationCtor_) {
        jstring text = env->NewStringUTF(name);
        location = env->NewObject(resourceLocationClass_, resourceLocationCtor_, text);
        if (text) env->DeleteLocalRef(text);
        checkException("ResourceLocation(String)");
    }
    if (!location && resourceLocationCtor2_) {
        jstring namespaceText = env->NewStringUTF("minecraft");
        jstring pathText = env->NewStringUTF(name);
        location = env->NewObject(resourceLocationClass_, resourceLocationCtor2_, namespaceText, pathText);
        if (namespaceText) env->DeleteLocalRef(namespaceText);
        if (pathText) env->DeleteLocalRef(pathText);
        checkException("ResourceLocation(String, String)");
    }
    if (!location) {
        return nullptr;
    }

    jobject block = env->CallObjectMethod(blockRegistry_, registryGet_, location);
    env->DeleteLocalRef(location);
    checkException("Registry.getValue");
    return block;
}

bool JniGame::prepareOreScan() {
    if (oreScanBroken_ || oreScanReady_) {
        return oreScanReady_;
    }

    JNIEnv* env = jvm_.env();
    if (!env || !level_) {
        return false;
    }

    // --- BlockPos / MutableBlockPos ------------------------------------------------
    if (!blockPosClass_) {
        constexpr const char* kBlockPosNames[] = {
            "net/minecraft/core/BlockPos",
            "net/minecraft/util/math/BlockPos",
        };
        for (const char* name : kBlockPosNames) {
            blockPosClass_ = static_cast<jclass>(env->NewGlobalRef(loadClass(name)));
            if (blockPosClass_) {
                break;
            }
        }
        if (!blockPosClass_) {
            NEO_LOG_ERROR("X-Ray: BlockPos class not found; ore scan disabled");
            oreScanBroken_ = true;
            return false;
        }
        blockPosCtor_ = method(blockPosClass_, "<init>", "(III)V");
        if (!blockPosCtor_) {
            NEO_LOG_ERROR("X-Ray: BlockPos constructor (III) not found; ore scan disabled");
            oreScanBroken_ = true;
            return false;
        }

        constexpr const char* kMutablePosNames[] = {
            "net/minecraft/core/BlockPos$MutableBlockPos",
            "net/minecraft/util/math/BlockPos$MutableBlockPos",
        };
        for (const char* name : kMutablePosNames) {
            jclass local = loadClass(name);
            if (!local) {
                continue;
            }
            mutablePosClass_ = static_cast<jclass>(env->NewGlobalRef(local));
            env->DeleteLocalRef(local);
            break;
        }
        if (mutablePosClass_) {
            jmethodID declared = nullptr;
            bool voidSignature = false;
            declared = method(mutablePosClass_, "set", "(III)Lnet/minecraft/core/BlockPos;");
            if (!declared) {
                declared = method(mutablePosClass_, "set", "(III)Lnet/minecraft/core/BlockPos$MutableBlockPos;");
            }
            if (!declared) {
                declared = method(mutablePosClass_, "set", "(III)V");
                voidSignature = declared != nullptr;
            }
            mutablePosSet_ = declared;
            mutablePosSetVoid_ = voidSignature;
            if (mutablePosSet_ && !mutablePos_) {
                jmethodID mutableCtor = env->GetMethodID(mutablePosClass_, "<init>", "()V");
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                }
                if (mutableCtor) {
                    jobject instance = env->NewObject(mutablePosClass_, mutableCtor);
                    checkException("MutableBlockPos()");
                    if (instance) {
                        mutablePos_ = env->NewGlobalRef(instance);
                        env->DeleteLocalRef(instance);
                    } else {
                        checkException("MutableBlockPos instance");
                    }
                }
            }
        }
        if (!mutablePosSet_ || !mutablePos_) {
            NEO_LOG_WARN("X-Ray: MutableBlockPos unavailable, falling back to per-probe allocation");
            mutablePosSet_ = nullptr;
            mutablePos_ = nullptr;
        }
    }

    // --- level.getBlockState -------------------------------------------------------
    if (!levelGetBlockState_ && level_) {
        jclass local = env->GetObjectClass(level_);
        constexpr const char* kStateSigs[] = {
            "(Lnet/minecraft/core/BlockPos;)Lnet/minecraft/world/level/block/state/BlockState;",
            "(Lnet/minecraft/util/math/BlockPos;)Lnet/minecraft/world/level/block/state/BlockState;",
        };
        for (const char* signature : kStateSigs) {
            levelGetBlockState_ = method(local, "getBlockState", signature);
            if (levelGetBlockState_) {
                break;
            }
        }
        env->DeleteLocalRef(local);
        if (!levelGetBlockState_) {
            NEO_LOG_ERROR("X-Ray: Level.getBlockState not found; ore scan disabled");
            oreScanBroken_ = true;
            return false;
        }
    }

    // --- BlockState.getBlock -------------------------------------------------------
    if (!stateGetBlock_) {
        if (!blockStateClass_) {
            constexpr const char* kStateClassNames[] = {
                "net/minecraft/world/level/block/state/BlockBehaviour$BlockStateBase",
                "net/minecraft/world/level/block/state/BlockState",
            };
            for (const char* name : kStateClassNames) {
                jclass local = loadClass(name);
                if (!local) {
                    continue;
                }
                blockStateClass_ = static_cast<jclass>(env->NewGlobalRef(local));
                env->DeleteLocalRef(local);
                break;
            }
        }
        if (!blockStateClass_) {
            NEO_LOG_ERROR("X-Ray: BlockState class not found; ore scan disabled");
            oreScanBroken_ = true;
            return false;
        }
        stateGetBlock_ = method(blockStateClass_, "getBlock", "()Lnet/minecraft/world/level/block/Block;");
        if (!stateGetBlock_) {
            NEO_LOG_ERROR("X-Ray: BlockState.getBlock not found; ore scan disabled");
            oreScanBroken_ = true;
            return false;
        }
    }

    // --- Identifier + block registry ------------------------------------------------
    if (!resourceLocationClass_) {
        constexpr const char* kIdentifierNames[] = {
            "net/minecraft/resources/Identifier",
            "net/minecraft/resources/ResourceLocation",
        };
        bool isIdentifier = false;
        for (const char* name : kIdentifierNames) {
            jclass local = loadClass(name);
            if (!local) {
                continue;
            }
            resourceLocationClass_ = static_cast<jclass>(env->NewGlobalRef(local));
            env->DeleteLocalRef(local);
            isIdentifier = std::string(name).find("Identifier") != std::string::npos;
            break;
        }
        if (!resourceLocationClass_) {
            NEO_LOG_ERROR("X-Ray: Identifier class not found; ore scan disabled");
            oreScanBroken_ = true;
            return false;
        }

        if (isIdentifier) {
            resourceLocationFromNamespace_ = staticMethod(resourceLocationClass_, "fromNamespaceAndPath",
                "(Ljava/lang/String;Ljava/lang/String;)Lnet/minecraft/resources/Identifier;");
            resourceLocationParse_ = staticMethod(resourceLocationClass_, "parse",
                "(Ljava/lang/String;)Lnet/minecraft/resources/Identifier;");
        } else {
            resourceLocationParse_ = staticMethod(resourceLocationClass_, "parse",
                "(Ljava/lang/String;)Lnet/minecraft/resources/ResourceLocation;");
            resourceLocationCtor_ = method(resourceLocationClass_, "<init>", "(Ljava/lang/String;)V");
            resourceLocationCtor2_ = method(resourceLocationClass_, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
        }

        if (!resourceLocationFromNamespace_ && !resourceLocationParse_ &&
            !resourceLocationCtor_ && !resourceLocationCtor2_) {
            NEO_LOG_ERROR("X-Ray: no Identifier factory available; ore scan disabled");
            oreScanBroken_ = true;
            return false;
        }
    }

    if (!blockRegistry_) {
        jclass registriesClass = loadClass("net/minecraft/core/registries/BuiltInRegistries");
        jfieldID registryField = nullptr;
        if (registriesClass) {
            constexpr const char* kRegistryFieldSigs[] = {
                "Lnet/minecraft/core/DefaultedRegistry;",
                "Lnet/minecraft/core/Registry;",
            };
            for (const char* signature : kRegistryFieldSigs) {
                registryField = env->GetStaticFieldID(registriesClass, "BLOCK", signature);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                }
                if (registryField) {
                    break;
                }
            }
        }
        if (!registriesClass || !registryField) {
            if (registriesClass) env->DeleteLocalRef(registriesClass);
            NEO_LOG_ERROR("X-Ray: block registry field not found; ore scan disabled");
            oreScanBroken_ = true;
            return false;
        }

        jobject local = env->GetStaticObjectField(registriesClass, registryField);
        env->DeleteLocalRef(registriesClass);
        checkException("BuiltInRegistries.BLOCK");
        if (!local) {
            NEO_LOG_ERROR("X-Ray: block registry is null; ore scan disabled");
            oreScanBroken_ = true;
            return false;
        }
        blockRegistry_ = env->NewGlobalRef(local);
        env->DeleteLocalRef(local);

        jclass registryClass = loadClass("net/minecraft/core/Registry");
        if (!registryClass) {
            registryClass = env->GetObjectClass(blockRegistry_);
        }
        registryGet_ = method(registryClass, "getValue",
            "(Lnet/minecraft/resources/Identifier;)Ljava/lang/Object;");
        if (!registryGet_) {
            registryGet_ = method(registryClass, "getValue",
                "(Lnet/minecraft/resources/ResourceLocation;)Ljava/lang/Object;");
        }
        if (!registryGet_) {
            registryGet_ = method(registryClass, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
        }
        env->DeleteLocalRef(registryClass);
        if (!registryGet_) {
            NEO_LOG_ERROR("X-Ray: Registry.getValue not found; ore scan disabled");
            oreScanBroken_ = true;
            return false;
        }
    }

    oreScanReady_ = true;
    oreBlockCacheTried_ = false;
    return resolveOreBlocks();
}

bool JniGame::resolveOreBlocks() {
    if (oreBlockCacheTried_ || !oreScanReady_) {
        return !oreBlockObjects_.empty();
    }
    oreBlockCacheTried_ = true;

    JNIEnv* env = jvm_.env();
    if (!env || !blockRegistry_ || !registryGet_) {
        return false;
    }

    int resolved = 0;
    for (int group = 0; group < ores::groupCount; ++group) {
        for (int nameIndex = 0; ores::kGroups[group].blocks[nameIndex] != nullptr; ++nameIndex) {
            const char* name = ores::kGroups[group].blocks[nameIndex];
            jobject block = findBlockObject(name);
            if (!block) {
                checkException("ore block lookup");
                noteMissing("ore block", name);
                continue;
            }

            jobject global = env->NewGlobalRef(block);
            env->DeleteLocalRef(block);
            if (!global) {
                continue;
            }
            oreBlockObjects_.push_back(global);
            oreBlockGroups_.push_back(group);
            ++resolved;
        }
    }

    NEO_LOG_INFO("X-Ray: cached %d ore blocks from the registry", resolved);
    if (resolved == 0) {
        // The registry may not be fully populated on the first ticks; retry.
        oreBlockCacheTried_ = false;
    }
    return resolved > 0;
}

void JniGame::runOreSweep(const OreScan& scan) {
    JNIEnv* env = jvm_.env();
    if (!env || !level_ || !levelGetBlockState_) {
        return;
    }

    const int radius = std::clamp(scan.radius, 4, 32);
    const int edge = radius * 2 + 1;
    const int layer = edge * edge;
    const int volume = layer * edge;

    const int originX = static_cast<int>(std::floor(scan.x)) - radius;
    const int originY = static_cast<int>(std::floor(scan.y)) - radius;
    const int originZ = static_cast<int>(std::floor(scan.z)) - radius;

    const auto packKey = [](int x, int y, int z) -> std::uint64_t {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x) & 0x1FFFFFu) << 42) |
               (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y) & 0x1FFFFFu) << 21) |
               (static_cast<std::uint64_t>(static_cast<std::uint32_t>(z) & 0x1FFFFFu));
    };

    const bool maskChanged = scan.mask != oreScanMask_;
    const bool volumeChanged = volume != oreScanVolume_;

    // Start (or restart) a pass. Markers persist between passes: the sweep only
    // adds/removes individual entries, so nothing ever flickers or pops while
    // walking, and partial progress is always visible.
    if (!oreScanInProgress_ || maskChanged || volumeChanged) {
        oreScanVolume_ = volume;
        oreScanOrigin_[0] = originX;
        oreScanOrigin_[1] = originY;
        oreScanOrigin_[2] = originZ;
        oreScanMask_ = scan.mask;
        oreScanIndex_ = 0;
        oreScanInProgress_ = true;

        // Drop markers that are deselected or now far away (bounds memory and
        // keeps the render loop cheap) while keeping everything the new pass
        // will cover (radius + margin).
        const double prune = static_cast<double>(radius + 16);
        const double prune2 = prune * prune;
        std::size_t w = 0;
        for (std::size_t i = 0; i < oreMarkersResolved_.size(); ++i) {
            const OreHit& h = oreMarkersResolved_[i];
            const bool selected = h.group >= 0 && h.group < ores::groupCount &&
                                  (scan.mask & (1ull << h.group)) != 0;
            if (!selected) {
                continue;
            }
            const double dx = h.x + 0.5 - scan.x;
            const double dy = h.y + 0.5 - scan.y;
            const double dz = h.z + 0.5 - scan.z;
            if (dx * dx + dy * dy + dz * dz > prune2) {
                continue;
            }
            oreMarkersResolved_[w++] = h;
        }
        oreMarkersResolved_.resize(w);
        oreIndex_.clear();
        oreIndex_.reserve(oreMarkersResolved_.size() * 2 + 16);
        for (std::size_t i = 0; i < oreMarkersResolved_.size(); ++i) {
            const OreHit& h = oreMarkersResolved_[i];
            oreIndex_[packKey(h.x, h.y, h.z)] = i;
        }
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(4);
    while (oreScanIndex_ < volume) {
        const int index = oreScanIndex_;
        const int wy = originY + index / layer;
        const int rest = index % layer;
        const int wz = originZ + rest / edge;
        const int wx = originX + rest % edge;
        ++oreScanIndex_;

        jobject pos = nullptr;
        if (mutablePos_ && mutablePosSet_) {
            pos = mutablePos_;
            if (mutablePosSetVoid_) {
                env->CallVoidMethod(mutablePos_, mutablePosSet_, wx, wy, wz);
            } else {
                jobject result = env->CallObjectMethod(mutablePos_, mutablePosSet_, wx, wy, wz);
                if (result) env->DeleteLocalRef(result);
            }
        } else {
            pos = env->NewObject(blockPosClass_, blockPosCtor_, wx, wy, wz);
        }

        jobject state = env->CallObjectMethod(level_, levelGetBlockState_, pos);
        if (!mutablePos_ && pos) {
            env->DeleteLocalRef(pos);
        }
        if (!state) {
            checkException("ore sweep");
            continue;
        }

        const std::uint64_t key = packKey(wx, wy, wz);
        int matchedGroup = -1;
        jobject block = stateGetBlock_ ? env->CallObjectMethod(state, stateGetBlock_) : nullptr;
        if (block) {
            for (std::size_t entry = 0; entry < oreBlockObjects_.size(); ++entry) {
                if ((oreScanMask_ & (1u << oreBlockGroups_[entry])) &&
                    env->IsSameObject(block, oreBlockObjects_[entry])) {
                    matchedGroup = oreBlockGroups_[entry];
                    break;
                }
            }
            env->DeleteLocalRef(block);
        }
        env->DeleteLocalRef(state);

        if (matchedGroup >= 0) {
            if (oreIndex_.find(key) == oreIndex_.end()) {
                OreHit hit;
                hit.x = wx;
                hit.y = wy;
                hit.z = wz;
                hit.group = matchedGroup;
                oreIndex_.emplace(key, oreMarkersResolved_.size());
                oreMarkersResolved_.push_back(hit);
            }
        } else if (!oreIndex_.empty()) {
            auto it = oreIndex_.find(key);
            if (it != oreIndex_.end()) {
                const std::size_t i = it->second;
                const std::size_t last = oreMarkersResolved_.size() - 1;
                if (i != last) {
                    oreMarkersResolved_[i] = oreMarkersResolved_[last];
                    const OreHit& moved = oreMarkersResolved_[i];
                    oreIndex_[packKey(moved.x, moved.y, moved.z)] = i;
                }
                oreMarkersResolved_.pop_back();
                oreIndex_.erase(it);
            }
        }

        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }
    }

    if (oreScanIndex_ >= volume) {
        oreScanIndex_ = 0;
        oreScanInProgress_ = false;  // next tick recenters a fresh pass
    }
}

void JniGame::requestOreScan(const OreScan& scan) {
    if (!resolved_) {
        return;
    }

    if (!scan.active) {
        if (!oreMarkersResolved_.empty() || oreScanInProgress_) {
            oreMarkersResolved_.clear();
            oreIndex_.clear();
            oreScanIndex_ = 0;
            oreScanInProgress_ = false;
            NEO_LOG_DEBUG("Ore scan stopped");
        }
        return;
    }

    if (!prepareOreScan()) {
        return;
    }
    if (!resolveOreBlocks()) {
        return;
    }
    runOreSweep(scan);
}

} // namespace neo
