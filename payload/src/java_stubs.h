#pragma once
#include "jni_structures.h"

// printf-style logging callback (stubs code formats + calls this)
typedef void (*JniLogFn)(const char* msg);

// Cached JNI class / field IDs — initialized once via init()
struct JniContext {
    jclass minecraftClass = nullptr;       // global ref
    jclass entityPlayerSPClass = nullptr;  // global ref, may be null

    jfieldID theMinecraft_fid = nullptr;  // static Minecraft singleton
    jfieldID thePlayer_fid   = nullptr;   // Minecraft.thePlayer (instance)

    jfieldID posX_fid = nullptr;
    jfieldID posY_fid = nullptr;
    jfieldID posZ_fid = nullptr;

    JniLogFn log = nullptr;

    bool init(JNIEnv env, JniLogFn log_fn);
    void cleanup(JNIEnv env);

    bool resolvePosFields(JNIEnv env, jobject playerObj);
};

// C++ stub for EntityPlayerSP — RAII wrapper over a jobject global ref.
// Do NOT construct/destruct inside SEH_TRY blocks (dtor fires DeleteGlobalRef).
class EntityPlayerSP {
    JNIEnv env_ = nullptr;
    jobject ref_ = nullptr;
public:
    EntityPlayerSP(JNIEnv env, jobject localRef);
    ~EntityPlayerSP();

    EntityPlayerSP(const EntityPlayerSP&) = delete;
    EntityPlayerSP& operator=(const EntityPlayerSP&) = delete;

    EntityPlayerSP(EntityPlayerSP&& other) noexcept;
    EntityPlayerSP& operator=(EntityPlayerSP&& other) noexcept;

    bool isValid() const { return ref_ != nullptr; }
    void getPos(double& x, double& y, double& z) const;
};

// C++ stub for Minecraft — RAII wrapper over a jobject global ref.
class Minecraft {
    JNIEnv env_ = nullptr;
    jobject ref_ = nullptr;
public:
    Minecraft(JNIEnv env, jobject localRef);
    ~Minecraft();

    Minecraft(const Minecraft&) = delete;
    Minecraft& operator=(const Minecraft&) = delete;

    Minecraft(Minecraft&& other) noexcept;
    Minecraft& operator=(Minecraft&& other) noexcept;

    bool isValid() const { return ref_ != nullptr; }
    EntityPlayerSP getThePlayer() const;
};

// Global JNI context — defined in java_stubs.cpp
extern JniContext g_jni;
