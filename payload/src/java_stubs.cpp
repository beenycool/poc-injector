#include "java_stubs.h"
#include "seh_compat.h"
#include <cstdio>
#include <cstring>

// Cross-platform SEH code type
#ifdef __APPLE__
typedef int seh_code_t;
#else
typedef DWORD seh_code_t;
#endif

// Global JNI context
JniContext g_jni;

// ---------------------------------------------------------------------------
// Logging helper
// ---------------------------------------------------------------------------
#define JNI_LOG(fmt, ...) do { \
    if (g_jni.log) { \
        char _buf[512]; \
        int _n = snprintf(_buf, sizeof(_buf), fmt, ##__VA_ARGS__); \
        if (_n > 0) g_jni.log(_buf); \
    } \
} while(0)

// ---------------------------------------------------------------------------
// Field resolution helpers (SEH-safe: POD only)
// ---------------------------------------------------------------------------

// Try GetFieldID with multiple names (instance fields). Returns nullptr if all fail.
static jfieldID findField(JNIEnv env, jclass cls, const char* sig,
                          const char** names, int count) {
    seh_code_t code;
    for (int i = 0; i < count; i++) {
        jfieldID fid = nullptr;
        SEH_TRY {
            fid = env->functions->GetFieldID(env, cls, names[i], sig);
        } SEH_EXCEPT(code) {
            JNI_LOG("  GetFieldID(%s) crashed (code=0x%08X)", names[i], code);
        }
        if (env->functions->ExceptionCheck(env))
            env->functions->ExceptionClear(env);
        if (fid) return fid;
    }
    return nullptr;
}

// Try GetStaticFieldID with multiple names (static fields).
static jfieldID findStaticField(JNIEnv env, jclass cls, const char* sig,
                                const char** names, int count) {
    seh_code_t code;
    for (int i = 0; i < count; i++) {
        jfieldID fid = nullptr;
        SEH_TRY {
            fid = env->functions->GetStaticFieldID(env, cls, names[i], sig);
        } SEH_EXCEPT(code) {
            JNI_LOG("  GetStaticFieldID(%s) crashed (code=0x%08X)", names[i], code);
        }
        if (env->functions->ExceptionCheck(env))
            env->functions->ExceptionClear(env);
        if (fid) return fid;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Reflection fallback: scan static fields of Minecraft.class to find the
// singleton instance field (the one whose type == Minecraft class itself).
// ---------------------------------------------------------------------------
static jfieldID findMinecraftSingletonViaReflection(JNIEnv env, jclass mcClass) {
    seh_code_t code;

    jclass classClass = nullptr;
    jclass fieldClass = nullptr;
    jclass modClass = nullptr;
    jmethodID getDeclaredFieldsMid = nullptr;
    jmethodID setAccessibleMid = nullptr;
    jmethodID getModifiersMid = nullptr;
    jmethodID isStaticMid = nullptr;
    jmethodID getTypeMid = nullptr;
    jobjectArray fields = nullptr;
    jfieldID result = nullptr;

    SEH_TRY {
        classClass = env->functions->FindClass(env, "java/lang/Class");
        fieldClass = env->functions->FindClass(env, "java/lang/reflect/Field");
        modClass   = env->functions->FindClass(env, "java/lang/reflect/Modifier");
    } SEH_EXCEPT(code) {
        JNI_LOG("CRASH refl_fallback: FindClass (code=0x%08X)", code);
    }
    if (!classClass || !fieldClass || !modClass) goto cleanup;

    SEH_TRY {
        getDeclaredFieldsMid = env->functions->GetMethodID(env, classClass,
            "getDeclaredFields", "()[Ljava/lang/reflect/Field;");
        setAccessibleMid   = env->functions->GetMethodID(env, fieldClass,
            "setAccessible", "(Z)V");
        getModifiersMid    = env->functions->GetMethodID(env, fieldClass,
            "getModifiers", "()I");
        isStaticMid        = env->functions->GetStaticMethodID(env, modClass,
            "isStatic", "(I)Z");
        getTypeMid         = env->functions->GetMethodID(env, fieldClass,
            "getType", "()Ljava/lang/Class;");
    } SEH_EXCEPT(code) {
        JNI_LOG("CRASH refl_fallback: GetMethodID (code=0x%08X)", code);
    }
    if (!getDeclaredFieldsMid || !setAccessibleMid || !getModifiersMid ||
        !isStaticMid || !getTypeMid) {
        goto cleanup;
    }

    SEH_TRY {
        fields = (jobjectArray)env->functions->CallObjectMethod(env,
            (jobject)mcClass, getDeclaredFieldsMid);
    } SEH_EXCEPT(code) {
        JNI_LOG("CRASH refl_fallback: getDeclaredFields (code=0x%08X)", code);
    }
    if (!fields) goto cleanup;

    {
    jsize count = env->functions->GetArrayLength(env, fields);
    JNI_LOG("Reflection fallback: scanning %d static fields for singleton", (int)count);

    for (jsize i = 0; i < count && !result; i++) {
        jobject field = nullptr;
        SEH_TRY { field = env->functions->GetObjectArrayElement(env, fields, i); }
        SEH_EXCEPT(code) { continue; }
        if (!field) continue;

        SEH_TRY { env->functions->CallVoidMethod(env, field, setAccessibleMid, (jboolean)1); }
        SEH_EXCEPT(code) { env->functions->DeleteLocalRef(env, field); continue; }

        jint mods = 0;
        SEH_TRY { mods = env->functions->CallIntMethod(env, field, getModifiersMid); }
        SEH_EXCEPT(code) { env->functions->DeleteLocalRef(env, field); continue; }

        jboolean isStatic = 0;
        SEH_TRY { isStatic = env->functions->CallStaticBooleanMethod(env, modClass, isStaticMid, mods); }
        SEH_EXCEPT(code) { env->functions->DeleteLocalRef(env, field); continue; }
        if (!isStatic) { env->functions->DeleteLocalRef(env, field); continue; }

        jobject fieldType = nullptr;
        SEH_TRY { fieldType = env->functions->CallObjectMethod(env, field, getTypeMid); }
        SEH_EXCEPT(code) { env->functions->DeleteLocalRef(env, field); continue; }
        if (!fieldType) { env->functions->DeleteLocalRef(env, field); continue; }

        jboolean match = env->functions->IsSameObject(env, fieldType, (jobject)mcClass);

        if (match) {
            SEH_TRY { result = env->functions->FromReflectedField(env, field); }
            SEH_EXCEPT(code) { result = nullptr; }
            JNI_LOG("  Found singleton field via reflection (index %d)", (int)i);
        }

        env->functions->DeleteLocalRef(env, fieldType);
        env->functions->DeleteLocalRef(env, field);
    }
    }

cleanup:
    if (fields)      env->functions->DeleteLocalRef(env, (jobject)fields);
    if (classClass)  env->functions->DeleteLocalRef(env, (jobject)classClass);
    if (fieldClass)  env->functions->DeleteLocalRef(env, (jobject)fieldClass);
    if (modClass)    env->functions->DeleteLocalRef(env, (jobject)modClass);
    return result;
}

// ---------------------------------------------------------------------------
// JniContext::init — resolves all classes and field IDs.
// Called once during payload init.
// ---------------------------------------------------------------------------
bool JniContext::init(JNIEnv env, JniLogFn log_fn) {
    log = log_fn;
    seh_code_t code;

    // -- 1. Find Minecraft class (try various names) --
    const char* mcNames[] = {
        "net.minecraft.client.Minecraft",
        "net.minecraft.v1_8.Minecraft",
        "net.minecraft.v1_7.Minecraft"
    };
    const char* foundMcName = nullptr;

    for (auto name : mcNames) {
        SEH_TRY {
            minecraftClass = env->functions->FindClass(env, name);
        } SEH_EXCEPT(code) {
            JNI_LOG("CRASH FindClass(%s): code=0x%08X", name, code);
        }
        if (env->functions->ExceptionCheck(env))
            env->functions->ExceptionClear(env);
        if (minecraftClass) {
            foundMcName = name;
            JNI_LOG("Found Minecraft class: %s", name);
            break;
        }
    }

    if (!minecraftClass) {
        JNI_LOG("FATAL: Could not find Minecraft class");
        return false;
    }

    // Promote to global ref
    SEH_TRY { minecraftClass = (jclass)env->functions->NewGlobalRef(env, (jobject)minecraftClass); }
    SEH_EXCEPT(code) {
        JNI_LOG("CRASH NewGlobalRef(mcClass): code=0x%08X", code);
        minecraftClass = nullptr;
        return false;
    }

    // -- 2. Resolve theMinecraft static field (the singleton) --
    char mcSig[256];
    snprintf(mcSig, sizeof(mcSig), "L%s;", foundMcName);

    const char* mcSingletonNames[] = { "theMinecraft", "field_71432_P", "I" };
    theMinecraft_fid = findStaticField(env, minecraftClass, mcSig,
                                       mcSingletonNames, 3);

    if (!theMinecraft_fid) {
        JNI_LOG("Known names failed, trying reflection fallback...");
        theMinecraft_fid = findMinecraftSingletonViaReflection(env, minecraftClass);
    }

    if (!theMinecraft_fid) {
        JNI_LOG("FATAL: Could not resolve theMinecraft singleton field");
        return false;
    }
    JNI_LOG("theMinecraft_fid = 0x%p", theMinecraft_fid);

    // -- 3. Resolve thePlayer instance field on Minecraft --
    const char* playerSig = "Lnet/minecraft/client/entity/EntityPlayerSP;";
    const char* playerNames[] = { "thePlayer", "field_71439_g", "h" };
    thePlayer_fid = findField(env, minecraftClass, playerSig, playerNames, 3);

    if (!thePlayer_fid) {
        // Broader fallback: try Object signature
        const char* objSig = "Ljava/lang/Object;";
        thePlayer_fid = findField(env, minecraftClass, objSig, playerNames, 3);
    }

    if (!thePlayer_fid) {
        JNI_LOG("WARN: thePlayer field not resolved — will retry at runtime");
        // Not fatal; we retry in the main loop
    } else {
        JNI_LOG("thePlayer_fid = 0x%p", thePlayer_fid);
    }

    // -- 4. Find EntityPlayerSP class --
    const char* epspNames[] = {
        "net/minecraft/client/entity/EntityPlayerSP",
        "net/minecraft/entity/player/EntityPlayerSP"
    };
    for (auto name : epspNames) {
        SEH_TRY {
            entityPlayerSPClass = env->functions->FindClass(env, name);
        } SEH_EXCEPT(code) {}
        if (env->functions->ExceptionCheck(env))
            env->functions->ExceptionClear(env);
        if (entityPlayerSPClass) break;
    }

    if (entityPlayerSPClass) {
        SEH_TRY {
            entityPlayerSPClass = (jclass)env->functions->NewGlobalRef(env, (jobject)entityPlayerSPClass);
        } SEH_EXCEPT(code) {
            entityPlayerSPClass = nullptr;
        }
        JNI_LOG("EntityPlayerSP class resolved via FindClass");
    } else {
        JNI_LOG("EntityPlayerSP class not found via FindClass — deferred resolution");
    }

    // -- 5. Pre-resolve posX/Y/Z field IDs (if we have the class) --
    if (entityPlayerSPClass) {
        const char* dSig = "D";
        const char* posXNames[] = { "posX", "field_70165_t", "t" };
        const char* posYNames[] = { "posY", "field_70163_u", "u" };
        const char* posZNames[] = { "posZ", "field_70161_v", "v" };

        posX_fid = findField(env, entityPlayerSPClass, dSig, posXNames, 3);
        posY_fid = findField(env, entityPlayerSPClass, dSig, posYNames, 3);
        posZ_fid = findField(env, entityPlayerSPClass, dSig, posZNames, 3);

        JNI_LOG("Pre-resolved pos fields: X=0x%p Y=0x%p Z=0x%p",
                posX_fid, posY_fid, posZ_fid);
    }

    JNI_LOG("JNI stubs initialized successfully");
    return true;
}

// ---------------------------------------------------------------------------
// JniContext::resolvePosFields — deferred pos field resolution from a live
// player object.  Uses GetObjectClass so it works even when FindClass fails.
// ---------------------------------------------------------------------------
bool JniContext::resolvePosFields(JNIEnv env, jobject playerObj) {
    if (!playerObj) return false;
    seh_code_t code;

    jclass playerClass = nullptr;
    SEH_TRY { playerClass = env->functions->GetObjectClass(env, playerObj); }
    SEH_EXCEPT(code) {
        JNI_LOG("CRASH resolvePosFields: GetObjectClass (code=0x%08X)", code);
        return false;
    }
    if (!playerClass) return false;

    const char* dSig = "D";
    const char* posXNames[] = { "posX", "field_70165_t", "t" };
    const char* posYNames[] = { "posY", "field_70163_u", "u" };
    const char* posZNames[] = { "posZ", "field_70161_v", "v" };

    posX_fid = findField(env, playerClass, dSig, posXNames, 3);
    posY_fid = findField(env, playerClass, dSig, posYNames, 3);
    posZ_fid = findField(env, playerClass, dSig, posZNames, 3);

    env->functions->DeleteLocalRef(env, (jobject)playerClass);

    JNI_LOG("Deferred pos fields: X=0x%p Y=0x%p Z=0x%p",
            posX_fid, posY_fid, posZ_fid);

    return posX_fid && posY_fid && posZ_fid;
}

// ---------------------------------------------------------------------------
// JniContext::cleanup — release global class refs
// ---------------------------------------------------------------------------
void JniContext::cleanup(JNIEnv env) {
    if (minecraftClass) {
        env->functions->DeleteGlobalRef(env, (jobject)minecraftClass);
        minecraftClass = nullptr;
    }
    if (entityPlayerSPClass) {
        env->functions->DeleteGlobalRef(env, (jobject)entityPlayerSPClass);
        entityPlayerSPClass = nullptr;
    }
    theMinecraft_fid = nullptr;
    thePlayer_fid = nullptr;
    posX_fid = posY_fid = posZ_fid = nullptr;
    log = nullptr;
}

// ---------------------------------------------------------------------------
// EntityPlayerSP implementation
// ---------------------------------------------------------------------------
EntityPlayerSP::EntityPlayerSP(JNIEnv env, jobject localRef) {
    if (localRef) {
        seh_code_t code;
        SEH_TRY {
            ref_ = env->functions->NewGlobalRef(env, localRef);
            env_ = env;
        } SEH_EXCEPT(code) {
            ref_ = nullptr;
        }
        env->functions->DeleteLocalRef(env, localRef);
    }
}

EntityPlayerSP::~EntityPlayerSP() {
    if (ref_ && env_) {
        env_->functions->DeleteGlobalRef(env_, ref_);
    }
}

EntityPlayerSP::EntityPlayerSP(EntityPlayerSP&& other) noexcept
    : env_(other.env_), ref_(other.ref_) {
    other.env_ = nullptr;
    other.ref_ = nullptr;
}

EntityPlayerSP& EntityPlayerSP::operator=(EntityPlayerSP&& other) noexcept {
    if (this != &other) {
        if (ref_ && env_)
            env_->functions->DeleteGlobalRef(env_, ref_);
        env_ = other.env_;
        ref_ = other.ref_;
        other.env_ = nullptr;
        other.ref_ = nullptr;
    }
    return *this;
}

void EntityPlayerSP::getPos(double& x, double& y, double& z) const {
    if (!isValid()) return;
    seh_code_t code;
    SEH_TRY {
        x = env_->functions->GetDoubleField(env_, ref_, g_jni.posX_fid);
        y = env_->functions->GetDoubleField(env_, ref_, g_jni.posY_fid);
        z = env_->functions->GetDoubleField(env_, ref_, g_jni.posZ_fid);
    } SEH_EXCEPT(code) {
        x = y = z = 0.0;
    }
}

// ---------------------------------------------------------------------------
// Minecraft implementation
// ---------------------------------------------------------------------------
Minecraft::Minecraft(JNIEnv env, jobject localRef) {
    if (localRef) {
        seh_code_t code;
        SEH_TRY {
            ref_ = env->functions->NewGlobalRef(env, localRef);
            env_ = env;
        } SEH_EXCEPT(code) {
            ref_ = nullptr;
        }
        env->functions->DeleteLocalRef(env, localRef);
    }
}

Minecraft::~Minecraft() {
    if (ref_ && env_) {
        env_->functions->DeleteGlobalRef(env_, ref_);
    }
}

Minecraft::Minecraft(Minecraft&& other) noexcept
    : env_(other.env_), ref_(other.ref_) {
    other.env_ = nullptr;
    other.ref_ = nullptr;
}

Minecraft& Minecraft::operator=(Minecraft&& other) noexcept {
    if (this != &other) {
        if (ref_ && env_)
            env_->functions->DeleteGlobalRef(env_, ref_);
        env_ = other.env_;
        ref_ = other.ref_;
        other.env_ = nullptr;
        other.ref_ = nullptr;
    }
    return *this;
}

// SEH-safe raw GetObjectField helper — no C++ objects, safe for __try
static jobject safeGetObjectField(JNIEnv env, jobject obj, jfieldID fid) {
    seh_code_t code;
    jobject result = nullptr;
    SEH_TRY {
        result = env->functions->GetObjectField(env, obj, fid);
    } SEH_EXCEPT(code) {
        JNI_LOG("CRASH GetObjectField (code=0x%08X)", code);
    }
    return result;
}

EntityPlayerSP Minecraft::getThePlayer() const {
    if (!isValid()) return EntityPlayerSP(env_, nullptr);

    jobject playerLocal = safeGetObjectField(env_, ref_, g_jni.thePlayer_fid);
    if (env_->functions->ExceptionCheck(env_))
        env_->functions->ExceptionClear(env_);

    return EntityPlayerSP(env_, playerLocal);
}
