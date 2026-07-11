/*
 * jni_structures.h — Reconstructed from jvm.dll (x86-64 Windows, HotSpot JVM)
 * Source: Ghidra static analysis of /data/projects/client/bin/server/jvm.dll
 * Image base (analysis): 0x180000000
 *
 * All offsets from Ghidra stored as RVAs (relative to analysis base).
 * At runtime, relocate via: addr = GetModuleHandle("jvm.dll") + RVA
 * JNI_GetCreatedJavaVMs is resolved via GetProcAddress (not a hardcoded RVA)
 * so it survives Java updates. Internal global offsets are RVA-based and
 * may still shift across builds — update via Ghidra regen when needed.
 */
#pragma once

#include <cstdint>
#include <cstdarg>
#include <windows.h>

// ---------------------------------------------------------------------------
// JNI type aliases
// ---------------------------------------------------------------------------
typedef uint8_t  jboolean;
typedef int8_t   jbyte;
typedef uint16_t jchar;
typedef int16_t  jshort;
typedef int32_t  jint;
typedef int64_t  jlong;
typedef float    jfloat;
typedef double   jdouble;
typedef jint     jsize;

// --- JNI calling convention ---
#ifdef _WIN32
#define JNICALL __stdcall
#else
#define JNICALL
#endif

// --- Forward declares ---
struct JNIEnv_;
struct JavaVM_;
struct JNIInvokeInterface_;
struct JNINativeInterface_;

typedef JNIEnv_*  JNIEnv;
typedef JavaVM_*  JavaVM;

// --- Forward declares for opaque JNI reference types ---
struct _jobject;
struct _jclass;
struct _jmethodID;
struct _jfieldID;
struct _jthrowable;
struct _jstring;
struct _jarray;

typedef _jobject*    jobject;
typedef _jclass*     jclass;
typedef _jmethodID*  jmethodID;
typedef _jfieldID*   jfieldID;
typedef _jthrowable* jthrowable;
typedef _jstring*    jstring;
typedef _jarray*     jarray;

// --- JNI array wrappers ---
typedef jarray jobjectArray;
typedef jarray jbooleanArray;
typedef jarray jbyteArray;
typedef jarray jcharArray;
typedef jarray jshortArray;
typedef jarray jintArray;
typedef jarray jlongArray;
typedef jarray jfloatArray;
typedef jarray jdoubleArray;

// --- Other JNI types ---
typedef _jobject* jweak;
typedef jint      jobjectRefType;

typedef struct {
    const char* name;
    const char* signature;
    void*       fnPtr;
} JNINativeMethod;

typedef union jvalue {
    jboolean  z;
    jbyte     b;
    jchar     c;
    jshort    s;
    jint      i;
    jlong     j;
    jfloat    f;
    jdouble   d;
    jobject   l;
} jvalue;

// ---------------------------------------------------------------------------
// JNIInvokeInterface_ — what JavaVM->functions points to
//   Resides at RVA 0x974AD0 (.data section, populated at runtime)
//   Verified: offsets 0x00-0x38 match the JNI specification.
//   Static dump shows slots 0x00-0x18 are NULL before VM init.
//   Slots 0x20-0x38 hold the real implementations:
//     0x20 = RVA 0x3f57a0 — AttachCurrentThread
//     0x28 = RVA 0x3f5860 — DetachCurrentThread  (tail-calls 0x3f81f0, r9=0)
//     0x30 = RVA 0x3df990 — GetEnv
//     0x38 = RVA 0x3dfa40 — AttachCurrentThreadAsDaemon
//     0x40 = RVA 0x3dfb70 — EXTRA (beyond spec; tail-calls 0x3f81f0, r9=1)
// ---------------------------------------------------------------------------
struct JNIInvokeInterface_ {
    void* reserved0;                                    // 0x00
    void* reserved1;                                    // 0x08
    void* reserved2;                                    // 0x10

    jint (JNICALL *DestroyJavaVM)(JavaVM*);             // 0x18
    jint (JNICALL *AttachCurrentThread)(JavaVM*, JNIEnv*, void*);     // 0x20
    jint (JNICALL *DetachCurrentThread)(JavaVM*);       // 0x28
    jint (JNICALL *GetEnv)(JavaVM*, void**, jint);      // 0x30
    jint (JNICALL *AttachCurrentThreadAsDaemon)(JavaVM*, JNIEnv*, void*); // 0x38
};

// ---------------------------------------------------------------------------
// JavaVM_ — what g_vm_list[0] dereferences into
//   Standard JNI: JavaVM* is a pointer to JNIInvokeInterface_
// ---------------------------------------------------------------------------
struct JavaVM_ {
    const JNIInvokeInterface_* functions;               // 0x00
};

// ---------------------------------------------------------------------------
// JavaVMInitArgs — passed to JNI_CreateJavaVM
// ---------------------------------------------------------------------------
struct JavaVMInitArgs {
    jint        version;                                // 0x00
    jint        nOptions;                               // 0x04
    void*       options;                                // 0x08
    jboolean    ignoreUnrecognized;                     // 0x10
};

enum JNI_VERSION {
    JNI_VERSION_1_1 = 0x00010001,
    JNI_VERSION_1_2 = 0x00010002,
    JNI_VERSION_1_4 = 0x00010004,
    JNI_VERSION_1_6 = 0x00010006,
    JNI_VERSION_1_8 = 0x00010008,
    JNI_VERSION_9   = 0x00090000,
    JNI_VERSION_10  = 0x000a0000,
};

// ---------------------------------------------------------------------------
// JNINativeInterface_ function pointer typedefs
// ---------------------------------------------------------------------------
typedef jint        (JNICALL *JNI_GetVersion_t)(JNIEnv);
typedef jclass      (JNICALL *JNI_DefineClass_t)(JNIEnv, const char*, jobject, const jbyte*, jsize);
typedef jclass      (JNICALL *JNI_FindClass_t)(JNIEnv, const char*);
typedef jmethodID   (JNICALL *JNI_FromReflectedMethod_t)(JNIEnv, jobject);
typedef jfieldID    (JNICALL *JNI_FromReflectedField_t)(JNIEnv, jobject);
typedef jobject     (JNICALL *JNI_ToReflectedMethod_t)(JNIEnv, jclass, jmethodID, jboolean);
typedef jclass      (JNICALL *JNI_GetSuperclass_t)(JNIEnv, jclass);
typedef jboolean    (JNICALL *JNI_IsAssignableFrom_t)(JNIEnv, jclass, jclass);
typedef jobject     (JNICALL *JNI_ToReflectedField_t)(JNIEnv, jclass, jfieldID, jboolean);
typedef jint        (JNICALL *JNI_Throw_t)(JNIEnv, jthrowable);
typedef jint        (JNICALL *JNI_ThrowNew_t)(JNIEnv, jclass, const char*);
typedef jthrowable  (JNICALL *JNI_ExceptionOccurred_t)(JNIEnv);
typedef void        (JNICALL *JNI_ExceptionDescribe_t)(JNIEnv);
typedef void        (JNICALL *JNI_ExceptionClear_t)(JNIEnv);
typedef void        (JNICALL *JNI_FatalError_t)(JNIEnv, const char*);
typedef jint        (JNICALL *JNI_PushLocalFrame_t)(JNIEnv, jint);
typedef jobject     (JNICALL *JNI_PopLocalFrame_t)(JNIEnv, jobject);
typedef jobject     (JNICALL *JNI_NewGlobalRef_t)(JNIEnv, jobject);
typedef void        (JNICALL *JNI_DeleteGlobalRef_t)(JNIEnv, jobject);
typedef void        (JNICALL *JNI_DeleteLocalRef_t)(JNIEnv, jobject);
typedef jboolean    (JNICALL *JNI_IsSameObject_t)(JNIEnv, jobject, jobject);
typedef jobject     (JNICALL *JNI_NewLocalRef_t)(JNIEnv, jobject);
typedef jint        (JNICALL *JNI_EnsureLocalCapacity_t)(JNIEnv, jint);
typedef jobject     (JNICALL *JNI_AllocObject_t)(JNIEnv, jclass);
typedef jobject     (JNICALL *JNI_NewObject_t)(JNIEnv, jclass, jmethodID, ...);
typedef jobject     (JNICALL *JNI_NewObjectV_t)(JNIEnv, jclass, jmethodID, va_list);
typedef jobject     (JNICALL *JNI_NewObjectA_t)(JNIEnv, jclass, jmethodID, const jvalue*);
typedef jclass      (JNICALL *JNI_GetObjectClass_t)(JNIEnv, jobject);
typedef jboolean    (JNICALL *JNI_IsInstanceOf_t)(JNIEnv, jobject, jclass);
typedef jmethodID   (JNICALL *JNI_GetMethodID_t)(JNIEnv, jclass, const char*, const char*);
typedef jobject     (JNICALL *JNI_CallObjectMethod_t)(JNIEnv, jobject, jmethodID, ...);
typedef jobject     (JNICALL *JNI_CallObjectMethodV_t)(JNIEnv, jobject, jmethodID, va_list);
typedef jobject     (JNICALL *JNI_CallObjectMethodA_t)(JNIEnv, jobject, jmethodID, const jvalue*);
typedef jboolean    (JNICALL *JNI_CallBooleanMethod_t)(JNIEnv, jobject, jmethodID, ...);
typedef jboolean    (JNICALL *JNI_CallBooleanMethodV_t)(JNIEnv, jobject, jmethodID, va_list);
typedef jboolean    (JNICALL *JNI_CallBooleanMethodA_t)(JNIEnv, jobject, jmethodID, const jvalue*);
typedef jbyte       (JNICALL *JNI_CallByteMethod_t)(JNIEnv, jobject, jmethodID, ...);
typedef jbyte       (JNICALL *JNI_CallByteMethodV_t)(JNIEnv, jobject, jmethodID, va_list);
typedef jbyte       (JNICALL *JNI_CallByteMethodA_t)(JNIEnv, jobject, jmethodID, const jvalue*);
typedef jchar       (JNICALL *JNI_CallCharMethod_t)(JNIEnv, jobject, jmethodID, ...);
typedef jchar       (JNICALL *JNI_CallCharMethodV_t)(JNIEnv, jobject, jmethodID, va_list);
typedef jchar       (JNICALL *JNI_CallCharMethodA_t)(JNIEnv, jobject, jmethodID, const jvalue*);
typedef jshort      (JNICALL *JNI_CallShortMethod_t)(JNIEnv, jobject, jmethodID, ...);
typedef jshort      (JNICALL *JNI_CallShortMethodV_t)(JNIEnv, jobject, jmethodID, va_list);
typedef jshort      (JNICALL *JNI_CallShortMethodA_t)(JNIEnv, jobject, jmethodID, const jvalue*);
typedef jint        (JNICALL *JNI_CallIntMethod_t)(JNIEnv, jobject, jmethodID, ...);
typedef jint        (JNICALL *JNI_CallIntMethodV_t)(JNIEnv, jobject, jmethodID, va_list);
typedef jint        (JNICALL *JNI_CallIntMethodA_t)(JNIEnv, jobject, jmethodID, const jvalue*);
typedef jlong       (JNICALL *JNI_CallLongMethod_t)(JNIEnv, jobject, jmethodID, ...);
typedef jlong       (JNICALL *JNI_CallLongMethodV_t)(JNIEnv, jobject, jmethodID, va_list);
typedef jlong       (JNICALL *JNI_CallLongMethodA_t)(JNIEnv, jobject, jmethodID, const jvalue*);
typedef jfloat      (JNICALL *JNI_CallFloatMethod_t)(JNIEnv, jobject, jmethodID, ...);
typedef jfloat      (JNICALL *JNI_CallFloatMethodV_t)(JNIEnv, jobject, jmethodID, va_list);
typedef jfloat      (JNICALL *JNI_CallFloatMethodA_t)(JNIEnv, jobject, jmethodID, const jvalue*);
typedef jdouble     (JNICALL *JNI_CallDoubleMethod_t)(JNIEnv, jobject, jmethodID, ...);
typedef jdouble     (JNICALL *JNI_CallDoubleMethodV_t)(JNIEnv, jobject, jmethodID, va_list);
typedef jdouble     (JNICALL *JNI_CallDoubleMethodA_t)(JNIEnv, jobject, jmethodID, const jvalue*);
typedef void        (JNICALL *JNI_CallVoidMethod_t)(JNIEnv, jobject, jmethodID, ...);
typedef void        (JNICALL *JNI_CallVoidMethodV_t)(JNIEnv, jobject, jmethodID, va_list);
typedef void        (JNICALL *JNI_CallVoidMethodA_t)(JNIEnv, jobject, jmethodID, const jvalue*);
typedef jobject     (JNICALL *JNI_CallNonvirtualObjectMethod_t)(JNIEnv, jobject, jclass, jmethodID, ...);
typedef jobject     (JNICALL *JNI_CallNonvirtualObjectMethodV_t)(JNIEnv, jobject, jclass, jmethodID, va_list);
typedef jobject     (JNICALL *JNI_CallNonvirtualObjectMethodA_t)(JNIEnv, jobject, jclass, jmethodID, const jvalue*);
typedef jboolean    (JNICALL *JNI_CallNonvirtualBooleanMethod_t)(JNIEnv, jobject, jclass, jmethodID, ...);
typedef jboolean    (JNICALL *JNI_CallNonvirtualBooleanMethodV_t)(JNIEnv, jobject, jclass, jmethodID, va_list);
typedef jboolean    (JNICALL *JNI_CallNonvirtualBooleanMethodA_t)(JNIEnv, jobject, jclass, jmethodID, const jvalue*);
typedef jbyte       (JNICALL *JNI_CallNonvirtualByteMethod_t)(JNIEnv, jobject, jclass, jmethodID, ...);
typedef jbyte       (JNICALL *JNI_CallNonvirtualByteMethodV_t)(JNIEnv, jobject, jclass, jmethodID, va_list);
typedef jbyte       (JNICALL *JNI_CallNonvirtualByteMethodA_t)(JNIEnv, jobject, jclass, jmethodID, const jvalue*);
typedef jchar       (JNICALL *JNI_CallNonvirtualCharMethod_t)(JNIEnv, jobject, jclass, jmethodID, ...);
typedef jchar       (JNICALL *JNI_CallNonvirtualCharMethodV_t)(JNIEnv, jobject, jclass, jmethodID, va_list);
typedef jchar       (JNICALL *JNI_CallNonvirtualCharMethodA_t)(JNIEnv, jobject, jclass, jmethodID, const jvalue*);
typedef jshort      (JNICALL *JNI_CallNonvirtualShortMethod_t)(JNIEnv, jobject, jclass, jmethodID, ...);
typedef jshort      (JNICALL *JNI_CallNonvirtualShortMethodV_t)(JNIEnv, jobject, jclass, jmethodID, va_list);
typedef jshort      (JNICALL *JNI_CallNonvirtualShortMethodA_t)(JNIEnv, jobject, jclass, jmethodID, const jvalue*);
typedef jint        (JNICALL *JNI_CallNonvirtualIntMethod_t)(JNIEnv, jobject, jclass, jmethodID, ...);
typedef jint        (JNICALL *JNI_CallNonvirtualIntMethodV_t)(JNIEnv, jobject, jclass, jmethodID, va_list);
typedef jint        (JNICALL *JNI_CallNonvirtualIntMethodA_t)(JNIEnv, jobject, jclass, jmethodID, const jvalue*);
typedef jlong       (JNICALL *JNI_CallNonvirtualLongMethod_t)(JNIEnv, jobject, jclass, jmethodID, ...);
typedef jlong       (JNICALL *JNI_CallNonvirtualLongMethodV_t)(JNIEnv, jobject, jclass, jmethodID, va_list);
typedef jlong       (JNICALL *JNI_CallNonvirtualLongMethodA_t)(JNIEnv, jobject, jclass, jmethodID, const jvalue*);
typedef jfloat      (JNICALL *JNI_CallNonvirtualFloatMethod_t)(JNIEnv, jobject, jclass, jmethodID, ...);
typedef jfloat      (JNICALL *JNI_CallNonvirtualFloatMethodV_t)(JNIEnv, jobject, jclass, jmethodID, va_list);
typedef jfloat      (JNICALL *JNI_CallNonvirtualFloatMethodA_t)(JNIEnv, jobject, jclass, jmethodID, const jvalue*);
typedef jdouble     (JNICALL *JNI_CallNonvirtualDoubleMethod_t)(JNIEnv, jobject, jclass, jmethodID, ...);
typedef jdouble     (JNICALL *JNI_CallNonvirtualDoubleMethodV_t)(JNIEnv, jobject, jclass, jmethodID, va_list);
typedef jdouble     (JNICALL *JNI_CallNonvirtualDoubleMethodA_t)(JNIEnv, jobject, jclass, jmethodID, const jvalue*);
typedef void        (JNICALL *JNI_CallNonvirtualVoidMethod_t)(JNIEnv, jobject, jclass, jmethodID, ...);
typedef void        (JNICALL *JNI_CallNonvirtualVoidMethodV_t)(JNIEnv, jobject, jclass, jmethodID, va_list);
typedef void        (JNICALL *JNI_CallNonvirtualVoidMethodA_t)(JNIEnv, jobject, jclass, jmethodID, const jvalue*);
typedef jfieldID    (JNICALL *JNI_GetFieldID_t)(JNIEnv, jclass, const char*, const char*);
typedef jobject     (JNICALL *JNI_GetObjectField_t)(JNIEnv, jobject, jfieldID);
typedef jboolean    (JNICALL *JNI_GetBooleanField_t)(JNIEnv, jobject, jfieldID);
typedef jbyte       (JNICALL *JNI_GetByteField_t)(JNIEnv, jobject, jfieldID);
typedef jchar       (JNICALL *JNI_GetCharField_t)(JNIEnv, jobject, jfieldID);
typedef jshort      (JNICALL *JNI_GetShortField_t)(JNIEnv, jobject, jfieldID);
typedef jint        (JNICALL *JNI_GetIntField_t)(JNIEnv, jobject, jfieldID);
typedef jlong       (JNICALL *JNI_GetLongField_t)(JNIEnv, jobject, jfieldID);
typedef jfloat      (JNICALL *JNI_GetFloatField_t)(JNIEnv, jobject, jfieldID);
typedef jdouble     (JNICALL *JNI_GetDoubleField_t)(JNIEnv, jobject, jfieldID);
typedef void        (JNICALL *JNI_SetObjectField_t)(JNIEnv, jobject, jfieldID, jobject);
typedef void        (JNICALL *JNI_SetBooleanField_t)(JNIEnv, jobject, jfieldID, jboolean);
typedef void        (JNICALL *JNI_SetByteField_t)(JNIEnv, jobject, jfieldID, jbyte);
typedef void        (JNICALL *JNI_SetCharField_t)(JNIEnv, jobject, jfieldID, jchar);
typedef void        (JNICALL *JNI_SetShortField_t)(JNIEnv, jobject, jfieldID, jshort);
typedef void        (JNICALL *JNI_SetIntField_t)(JNIEnv, jobject, jfieldID, jint);
typedef void        (JNICALL *JNI_SetLongField_t)(JNIEnv, jobject, jfieldID, jlong);
typedef void        (JNICALL *JNI_SetFloatField_t)(JNIEnv, jobject, jfieldID, jfloat);
typedef void        (JNICALL *JNI_SetDoubleField_t)(JNIEnv, jobject, jfieldID, jdouble);
typedef jmethodID   (JNICALL *JNI_GetStaticMethodID_t)(JNIEnv, jclass, const char*, const char*);
typedef jobject     (JNICALL *JNI_CallStaticObjectMethod_t)(JNIEnv, jclass, jmethodID, ...);
typedef jobject     (JNICALL *JNI_CallStaticObjectMethodV_t)(JNIEnv, jclass, jmethodID, va_list);
typedef jobject     (JNICALL *JNI_CallStaticObjectMethodA_t)(JNIEnv, jclass, jmethodID, const jvalue*);
typedef jboolean    (JNICALL *JNI_CallStaticBooleanMethod_t)(JNIEnv, jclass, jmethodID, ...);
typedef jboolean    (JNICALL *JNI_CallStaticBooleanMethodV_t)(JNIEnv, jclass, jmethodID, va_list);
typedef jboolean    (JNICALL *JNI_CallStaticBooleanMethodA_t)(JNIEnv, jclass, jmethodID, const jvalue*);
typedef jbyte       (JNICALL *JNI_CallStaticByteMethod_t)(JNIEnv, jclass, jmethodID, ...);
typedef jbyte       (JNICALL *JNI_CallStaticByteMethodV_t)(JNIEnv, jclass, jmethodID, va_list);
typedef jbyte       (JNICALL *JNI_CallStaticByteMethodA_t)(JNIEnv, jclass, jmethodID, const jvalue*);
typedef jchar       (JNICALL *JNI_CallStaticCharMethod_t)(JNIEnv, jclass, jmethodID, ...);
typedef jchar       (JNICALL *JNI_CallStaticCharMethodV_t)(JNIEnv, jclass, jmethodID, va_list);
typedef jchar       (JNICALL *JNI_CallStaticCharMethodA_t)(JNIEnv, jclass, jmethodID, const jvalue*);
typedef jshort      (JNICALL *JNI_CallStaticShortMethod_t)(JNIEnv, jclass, jmethodID, ...);
typedef jshort      (JNICALL *JNI_CallStaticShortMethodV_t)(JNIEnv, jclass, jmethodID, va_list);
typedef jshort      (JNICALL *JNI_CallStaticShortMethodA_t)(JNIEnv, jclass, jmethodID, const jvalue*);
typedef jint        (JNICALL *JNI_CallStaticIntMethod_t)(JNIEnv, jclass, jmethodID, ...);
typedef jint        (JNICALL *JNI_CallStaticIntMethodV_t)(JNIEnv, jclass, jmethodID, va_list);
typedef jint        (JNICALL *JNI_CallStaticIntMethodA_t)(JNIEnv, jclass, jmethodID, const jvalue*);
typedef jlong       (JNICALL *JNI_CallStaticLongMethod_t)(JNIEnv, jclass, jmethodID, ...);
typedef jlong       (JNICALL *JNI_CallStaticLongMethodV_t)(JNIEnv, jclass, jmethodID, va_list);
typedef jlong       (JNICALL *JNI_CallStaticLongMethodA_t)(JNIEnv, jclass, jmethodID, const jvalue*);
typedef jfloat      (JNICALL *JNI_CallStaticFloatMethod_t)(JNIEnv, jclass, jmethodID, ...);
typedef jfloat      (JNICALL *JNI_CallStaticFloatMethodV_t)(JNIEnv, jclass, jmethodID, va_list);
typedef jfloat      (JNICALL *JNI_CallStaticFloatMethodA_t)(JNIEnv, jclass, jmethodID, const jvalue*);
typedef jdouble     (JNICALL *JNI_CallStaticDoubleMethod_t)(JNIEnv, jclass, jmethodID, ...);
typedef jdouble     (JNICALL *JNI_CallStaticDoubleMethodV_t)(JNIEnv, jclass, jmethodID, va_list);
typedef jdouble     (JNICALL *JNI_CallStaticDoubleMethodA_t)(JNIEnv, jclass, jmethodID, const jvalue*);
typedef void        (JNICALL *JNI_CallStaticVoidMethod_t)(JNIEnv, jclass, jmethodID, ...);
typedef void        (JNICALL *JNI_CallStaticVoidMethodV_t)(JNIEnv, jclass, jmethodID, va_list);
typedef void        (JNICALL *JNI_CallStaticVoidMethodA_t)(JNIEnv, jclass, jmethodID, const jvalue*);
typedef jfieldID    (JNICALL *JNI_GetStaticFieldID_t)(JNIEnv, jclass, const char*, const char*);
typedef jobject     (JNICALL *JNI_GetStaticObjectField_t)(JNIEnv, jclass, jfieldID);
typedef jboolean    (JNICALL *JNI_GetStaticBooleanField_t)(JNIEnv, jclass, jfieldID);
typedef jbyte       (JNICALL *JNI_GetStaticByteField_t)(JNIEnv, jclass, jfieldID);
typedef jchar       (JNICALL *JNI_GetStaticCharField_t)(JNIEnv, jclass, jfieldID);
typedef jshort      (JNICALL *JNI_GetStaticShortField_t)(JNIEnv, jclass, jfieldID);
typedef jint        (JNICALL *JNI_GetStaticIntField_t)(JNIEnv, jclass, jfieldID);
typedef jlong       (JNICALL *JNI_GetStaticLongField_t)(JNIEnv, jclass, jfieldID);
typedef jfloat      (JNICALL *JNI_GetStaticFloatField_t)(JNIEnv, jclass, jfieldID);
typedef jdouble     (JNICALL *JNI_GetStaticDoubleField_t)(JNIEnv, jclass, jfieldID);
typedef void        (JNICALL *JNI_SetStaticObjectField_t)(JNIEnv, jclass, jfieldID, jobject);
typedef void        (JNICALL *JNI_SetStaticBooleanField_t)(JNIEnv, jclass, jfieldID, jboolean);
typedef void        (JNICALL *JNI_SetStaticByteField_t)(JNIEnv, jclass, jfieldID, jbyte);
typedef void        (JNICALL *JNI_SetStaticCharField_t)(JNIEnv, jclass, jfieldID, jchar);
typedef void        (JNICALL *JNI_SetStaticShortField_t)(JNIEnv, jclass, jfieldID, jshort);
typedef void        (JNICALL *JNI_SetStaticIntField_t)(JNIEnv, jclass, jfieldID, jint);
typedef void        (JNICALL *JNI_SetStaticLongField_t)(JNIEnv, jclass, jfieldID, jlong);
typedef void        (JNICALL *JNI_SetStaticFloatField_t)(JNIEnv, jclass, jfieldID, jfloat);
typedef void        (JNICALL *JNI_SetStaticDoubleField_t)(JNIEnv, jclass, jfieldID, jdouble);
typedef jstring     (JNICALL *JNI_NewString_t)(JNIEnv, const jchar*, jsize);
typedef jsize       (JNICALL *JNI_GetStringLength_t)(JNIEnv, jstring);
typedef const jchar*(JNICALL *JNI_GetStringChars_t)(JNIEnv, jstring, jboolean*);
typedef void        (JNICALL *JNI_ReleaseStringChars_t)(JNIEnv, jstring, const jchar*);
typedef jstring     (JNICALL *JNI_NewStringUTF_t)(JNIEnv, const char*);
typedef jsize       (JNICALL *JNI_GetStringUTFLength_t)(JNIEnv, jstring);
typedef const char* (JNICALL *JNI_GetStringUTFChars_t)(JNIEnv, jstring, jboolean*);
typedef void        (JNICALL *JNI_ReleaseStringUTFChars_t)(JNIEnv, jstring, const char*);
typedef jsize       (JNICALL *JNI_GetArrayLength_t)(JNIEnv, jarray);
typedef jobjectArray(JNICALL *JNI_NewObjectArray_t)(JNIEnv, jsize, jclass, jobject);
typedef jobject     (JNICALL *JNI_GetObjectArrayElement_t)(JNIEnv, jobjectArray, jsize);
typedef void        (JNICALL *JNI_SetObjectArrayElement_t)(JNIEnv, jobjectArray, jsize, jobject);
typedef jbooleanArray(JNICALL *JNI_NewBooleanArray_t)(JNIEnv, jsize);
typedef jbyteArray   (JNICALL *JNI_NewByteArray_t)(JNIEnv, jsize);
typedef jcharArray   (JNICALL *JNI_NewCharArray_t)(JNIEnv, jsize);
typedef jshortArray  (JNICALL *JNI_NewShortArray_t)(JNIEnv, jsize);
typedef jintArray    (JNICALL *JNI_NewIntArray_t)(JNIEnv, jsize);
typedef jlongArray   (JNICALL *JNI_NewLongArray_t)(JNIEnv, jsize);
typedef jfloatArray  (JNICALL *JNI_NewFloatArray_t)(JNIEnv, jsize);
typedef jdoubleArray (JNICALL *JNI_NewDoubleArray_t)(JNIEnv, jsize);
typedef jboolean*(JNICALL *JNI_GetBooleanArrayElements_t)(JNIEnv, jbooleanArray, jboolean*);
typedef jbyte*   (JNICALL *JNI_GetByteArrayElements_t)(JNIEnv, jbyteArray, jboolean*);
typedef jchar*   (JNICALL *JNI_GetCharArrayElements_t)(JNIEnv, jcharArray, jboolean*);
typedef jshort*  (JNICALL *JNI_GetShortArrayElements_t)(JNIEnv, jshortArray, jboolean*);
typedef jint*    (JNICALL *JNI_GetIntArrayElements_t)(JNIEnv, jintArray, jboolean*);
typedef jlong*   (JNICALL *JNI_GetLongArrayElements_t)(JNIEnv, jlongArray, jboolean*);
typedef jfloat*  (JNICALL *JNI_GetFloatArrayElements_t)(JNIEnv, jfloatArray, jboolean*);
typedef jdouble* (JNICALL *JNI_GetDoubleArrayElements_t)(JNIEnv, jdoubleArray, jboolean*);
typedef void     (JNICALL *JNI_ReleaseBooleanArrayElements_t)(JNIEnv, jbooleanArray, jboolean*, jint);
typedef void     (JNICALL *JNI_ReleaseByteArrayElements_t)(JNIEnv, jbyteArray, jbyte*, jint);
typedef void     (JNICALL *JNI_ReleaseCharArrayElements_t)(JNIEnv, jcharArray, jchar*, jint);
typedef void     (JNICALL *JNI_ReleaseShortArrayElements_t)(JNIEnv, jshortArray, jshort*, jint);
typedef void     (JNICALL *JNI_ReleaseIntArrayElements_t)(JNIEnv, jintArray, jint*, jint);
typedef void     (JNICALL *JNI_ReleaseLongArrayElements_t)(JNIEnv, jlongArray, jlong*, jint);
typedef void     (JNICALL *JNI_ReleaseFloatArrayElements_t)(JNIEnv, jfloatArray, jfloat*, jint);
typedef void     (JNICALL *JNI_ReleaseDoubleArrayElements_t)(JNIEnv, jdoubleArray, jdouble*, jint);
typedef void     (JNICALL *JNI_GetBooleanArrayRegion_t)(JNIEnv, jbooleanArray, jsize, jsize, jboolean*);
typedef void     (JNICALL *JNI_GetByteArrayRegion_t)(JNIEnv, jbyteArray, jsize, jsize, jbyte*);
typedef void     (JNICALL *JNI_GetCharArrayRegion_t)(JNIEnv, jcharArray, jsize, jsize, jchar*);
typedef void     (JNICALL *JNI_GetShortArrayRegion_t)(JNIEnv, jshortArray, jsize, jsize, jshort*);
typedef void     (JNICALL *JNI_GetIntArrayRegion_t)(JNIEnv, jintArray, jsize, jsize, jint*);
typedef void     (JNICALL *JNI_GetLongArrayRegion_t)(JNIEnv, jlongArray, jsize, jsize, jlong*);
typedef void     (JNICALL *JNI_GetFloatArrayRegion_t)(JNIEnv, jfloatArray, jsize, jsize, jfloat*);
typedef void     (JNICALL *JNI_GetDoubleArrayRegion_t)(JNIEnv, jdoubleArray, jsize, jsize, jdouble*);
typedef void     (JNICALL *JNI_SetBooleanArrayRegion_t)(JNIEnv, jbooleanArray, jsize, jsize, const jboolean*);
typedef void     (JNICALL *JNI_SetByteArrayRegion_t)(JNIEnv, jbyteArray, jsize, jsize, const jbyte*);
typedef void     (JNICALL *JNI_SetCharArrayRegion_t)(JNIEnv, jcharArray, jsize, jsize, const jchar*);
typedef void     (JNICALL *JNI_SetShortArrayRegion_t)(JNIEnv, jshortArray, jsize, jsize, const jshort*);
typedef void     (JNICALL *JNI_SetIntArrayRegion_t)(JNIEnv, jintArray, jsize, jsize, const jint*);
typedef void     (JNICALL *JNI_SetLongArrayRegion_t)(JNIEnv, jlongArray, jsize, jsize, const jlong*);
typedef void     (JNICALL *JNI_SetFloatArrayRegion_t)(JNIEnv, jfloatArray, jsize, jsize, const jfloat*);
typedef void     (JNICALL *JNI_SetDoubleArrayRegion_t)(JNIEnv, jdoubleArray, jsize, jsize, const jdouble*);
typedef jint     (JNICALL *JNI_RegisterNatives_t)(JNIEnv, jclass, const JNINativeMethod*, jint);
typedef jint     (JNICALL *JNI_UnregisterNatives_t)(JNIEnv, jclass);
typedef jint     (JNICALL *JNI_MonitorEnter_t)(JNIEnv, jobject);
typedef jint     (JNICALL *JNI_MonitorExit_t)(JNIEnv, jobject);
typedef jint     (JNICALL *JNI_GetJavaVM_t)(JNIEnv, JavaVM*);
typedef void     (JNICALL *JNI_GetStringRegion_t)(JNIEnv, jstring, jsize, jsize, jchar*);
typedef void     (JNICALL *JNI_GetStringUTFRegion_t)(JNIEnv, jstring, jsize, jsize, char*);
typedef void*    (JNICALL *JNI_GetPrimitiveArrayCritical_t)(JNIEnv, jarray, jboolean*);
typedef void     (JNICALL *JNI_ReleasePrimitiveArrayCritical_t)(JNIEnv, jarray, void*, jint);
typedef const jchar*(JNICALL *JNI_GetStringCritical_t)(JNIEnv, jstring, jboolean*);
typedef void     (JNICALL *JNI_ReleaseStringCritical_t)(JNIEnv, jstring, const jchar*);
typedef jweak    (JNICALL *JNI_NewWeakGlobalRef_t)(JNIEnv, jobject);
typedef void     (JNICALL *JNI_DeleteWeakGlobalRef_t)(JNIEnv, jweak);
typedef jboolean (JNICALL *JNI_ExceptionCheck_t)(JNIEnv);
typedef jobject  (JNICALL *JNI_NewDirectByteBuffer_t)(JNIEnv, void*, jlong);
typedef void*    (JNICALL *JNI_GetDirectBufferAddress_t)(JNIEnv, jobject);
typedef jlong    (JNICALL *JNI_GetDirectBufferCapacity_t)(JNIEnv, jobject);
typedef jobjectRefType (JNICALL *JNI_GetObjectRefType_t)(JNIEnv, jobject);
typedef jobject  (JNICALL *JNI_GetModule_t)(JNIEnv, jclass);

struct JNINativeInterface_ {
    void*           reserved0;                              // 0x000
    void*           reserved1;                              // 0x008
    void*           reserved2;                              // 0x010
    void*           reserved3;                              // 0x018

    JNI_GetVersion_t                    GetVersion;         // 0x020
    JNI_DefineClass_t                   DefineClass;        // 0x028
    JNI_FindClass_t                     FindClass;          // 0x030

    JNI_FromReflectedMethod_t           FromReflectedMethod;
    JNI_FromReflectedField_t            FromReflectedField;
    JNI_ToReflectedMethod_t             ToReflectedMethod;
    JNI_GetSuperclass_t                 GetSuperclass;
    JNI_IsAssignableFrom_t              IsAssignableFrom;
    JNI_ToReflectedField_t              ToReflectedField;

    JNI_Throw_t                         Throw;              // 0x060
    JNI_ThrowNew_t                      ThrowNew;
    JNI_ExceptionOccurred_t             ExceptionOccurred;
    JNI_ExceptionDescribe_t             ExceptionDescribe;
    JNI_ExceptionClear_t                ExceptionClear;
    JNI_FatalError_t                    FatalError;

    JNI_PushLocalFrame_t                PushLocalFrame;
    JNI_PopLocalFrame_t                 PopLocalFrame;
    JNI_NewGlobalRef_t                  NewGlobalRef;
    JNI_DeleteGlobalRef_t               DeleteGlobalRef;
    JNI_DeleteLocalRef_t                DeleteLocalRef;
    JNI_IsSameObject_t                  IsSameObject;       // 0x0A8
    JNI_NewLocalRef_t                   NewLocalRef;
    JNI_EnsureLocalCapacity_t           EnsureLocalCapacity;

    JNI_AllocObject_t                   AllocObject;
    JNI_NewObject_t                     NewObject;
    JNI_NewObjectV_t                    NewObjectV;
    JNI_NewObjectA_t                    NewObjectA;

    JNI_GetObjectClass_t                GetObjectClass;
    JNI_IsInstanceOf_t                  IsInstanceOf;
    JNI_GetMethodID_t                   GetMethodID;        // 0x0F8

    JNI_CallObjectMethod_t              CallObjectMethod;
    JNI_CallObjectMethodV_t             CallObjectMethodV;
    JNI_CallObjectMethodA_t             CallObjectMethodA;
    JNI_CallBooleanMethod_t             CallBooleanMethod;
    JNI_CallBooleanMethodV_t            CallBooleanMethodV;
    JNI_CallBooleanMethodA_t            CallBooleanMethodA;
    JNI_CallByteMethod_t                CallByteMethod;
    JNI_CallByteMethodV_t               CallByteMethodV;
    JNI_CallByteMethodA_t               CallByteMethodA;
    JNI_CallCharMethod_t                CallCharMethod;
    JNI_CallCharMethodV_t               CallCharMethodV;
    JNI_CallCharMethodA_t               CallCharMethodA;
    JNI_CallShortMethod_t               CallShortMethod;
    JNI_CallShortMethodV_t              CallShortMethodV;
    JNI_CallShortMethodA_t              CallShortMethodA;
    JNI_CallIntMethod_t                 CallIntMethod;
    JNI_CallIntMethodV_t                CallIntMethodV;
    JNI_CallIntMethodA_t                CallIntMethodA;
    JNI_CallLongMethod_t                CallLongMethod;
    JNI_CallLongMethodV_t               CallLongMethodV;
    JNI_CallLongMethodA_t               CallLongMethodA;
    JNI_CallFloatMethod_t               CallFloatMethod;
    JNI_CallFloatMethodV_t              CallFloatMethodV;
    JNI_CallFloatMethodA_t              CallFloatMethodA;
    JNI_CallDoubleMethod_t              CallDoubleMethod;
    JNI_CallDoubleMethodV_t             CallDoubleMethodV;
    JNI_CallDoubleMethodA_t             CallDoubleMethodA;
    JNI_CallVoidMethod_t                CallVoidMethod;
    JNI_CallVoidMethodV_t               CallVoidMethodV;
    JNI_CallVoidMethodA_t               CallVoidMethodA;

    JNI_CallNonvirtualObjectMethod_t    CallNonvirtualObjectMethod;
    JNI_CallNonvirtualObjectMethodV_t   CallNonvirtualObjectMethodV;
    JNI_CallNonvirtualObjectMethodA_t   CallNonvirtualObjectMethodA;
    JNI_CallNonvirtualBooleanMethod_t   CallNonvirtualBooleanMethod;
    JNI_CallNonvirtualBooleanMethodV_t  CallNonvirtualBooleanMethodV;
    JNI_CallNonvirtualBooleanMethodA_t  CallNonvirtualBooleanMethodA;
    JNI_CallNonvirtualByteMethod_t      CallNonvirtualByteMethod;
    JNI_CallNonvirtualByteMethodV_t     CallNonvirtualByteMethodV;
    JNI_CallNonvirtualByteMethodA_t     CallNonvirtualByteMethodA;
    JNI_CallNonvirtualCharMethod_t      CallNonvirtualCharMethod;
    JNI_CallNonvirtualCharMethodV_t     CallNonvirtualCharMethodV;
    JNI_CallNonvirtualCharMethodA_t     CallNonvirtualCharMethodA;
    JNI_CallNonvirtualShortMethod_t     CallNonvirtualShortMethod;
    JNI_CallNonvirtualShortMethodV_t    CallNonvirtualShortMethodV;
    JNI_CallNonvirtualShortMethodA_t    CallNonvirtualShortMethodA;
    JNI_CallNonvirtualIntMethod_t       CallNonvirtualIntMethod;
    JNI_CallNonvirtualIntMethodV_t      CallNonvirtualIntMethodV;
    JNI_CallNonvirtualIntMethodA_t      CallNonvirtualIntMethodA;
    JNI_CallNonvirtualLongMethod_t      CallNonvirtualLongMethod;
    JNI_CallNonvirtualLongMethodV_t     CallNonvirtualLongMethodV;
    JNI_CallNonvirtualLongMethodA_t     CallNonvirtualLongMethodA;
    JNI_CallNonvirtualFloatMethod_t     CallNonvirtualFloatMethod;
    JNI_CallNonvirtualFloatMethodV_t    CallNonvirtualFloatMethodV;
    JNI_CallNonvirtualFloatMethodA_t    CallNonvirtualFloatMethodA;
    JNI_CallNonvirtualDoubleMethod_t    CallNonvirtualDoubleMethod;
    JNI_CallNonvirtualDoubleMethodV_t   CallNonvirtualDoubleMethodV;
    JNI_CallNonvirtualDoubleMethodA_t   CallNonvirtualDoubleMethodA;
    JNI_CallNonvirtualVoidMethod_t      CallNonvirtualVoidMethod;
    JNI_CallNonvirtualVoidMethodV_t     CallNonvirtualVoidMethodV;
    JNI_CallNonvirtualVoidMethodA_t     CallNonvirtualVoidMethodA;

    JNI_GetFieldID_t                    GetFieldID;
    JNI_GetObjectField_t                GetObjectField;
    JNI_GetBooleanField_t               GetBooleanField;
    JNI_GetByteField_t                  GetByteField;
    JNI_GetCharField_t                  GetCharField;
    JNI_GetShortField_t                 GetShortField;
    JNI_GetIntField_t                   GetIntField;
    JNI_GetLongField_t                  GetLongField;
    JNI_GetFloatField_t                 GetFloatField;
    JNI_GetDoubleField_t                GetDoubleField;
    JNI_SetObjectField_t                SetObjectField;
    JNI_SetBooleanField_t               SetBooleanField;
    JNI_SetByteField_t                  SetByteField;
    JNI_SetCharField_t                  SetCharField;
    JNI_SetShortField_t                 SetShortField;
    JNI_SetIntField_t                   SetIntField;
    JNI_SetLongField_t                  SetLongField;
    JNI_SetFloatField_t                 SetFloatField;
    JNI_SetDoubleField_t                SetDoubleField;

    JNI_GetStaticMethodID_t             GetStaticMethodID;

    JNI_CallStaticObjectMethod_t        CallStaticObjectMethod;
    JNI_CallStaticObjectMethodV_t       CallStaticObjectMethodV;
    JNI_CallStaticObjectMethodA_t       CallStaticObjectMethodA;
    JNI_CallStaticBooleanMethod_t       CallStaticBooleanMethod;
    JNI_CallStaticBooleanMethodV_t      CallStaticBooleanMethodV;
    JNI_CallStaticBooleanMethodA_t      CallStaticBooleanMethodA;
    JNI_CallStaticByteMethod_t          CallStaticByteMethod;
    JNI_CallStaticByteMethodV_t         CallStaticByteMethodV;
    JNI_CallStaticByteMethodA_t         CallStaticByteMethodA;
    JNI_CallStaticCharMethod_t          CallStaticCharMethod;
    JNI_CallStaticCharMethodV_t         CallStaticCharMethodV;
    JNI_CallStaticCharMethodA_t         CallStaticCharMethodA;
    JNI_CallStaticShortMethod_t         CallStaticShortMethod;
    JNI_CallStaticShortMethodV_t        CallStaticShortMethodV;
    JNI_CallStaticShortMethodA_t        CallStaticShortMethodA;
    JNI_CallStaticIntMethod_t           CallStaticIntMethod;
    JNI_CallStaticIntMethodV_t          CallStaticIntMethodV;
    JNI_CallStaticIntMethodA_t          CallStaticIntMethodA;
    JNI_CallStaticLongMethod_t          CallStaticLongMethod;
    JNI_CallStaticLongMethodV_t         CallStaticLongMethodV;
    JNI_CallStaticLongMethodA_t         CallStaticLongMethodA;
    JNI_CallStaticFloatMethod_t         CallStaticFloatMethod;
    JNI_CallStaticFloatMethodV_t        CallStaticFloatMethodV;
    JNI_CallStaticFloatMethodA_t        CallStaticFloatMethodA;
    JNI_CallStaticDoubleMethod_t        CallStaticDoubleMethod;
    JNI_CallStaticDoubleMethodV_t       CallStaticDoubleMethodV;
    JNI_CallStaticDoubleMethodA_t       CallStaticDoubleMethodA;
    JNI_CallStaticVoidMethod_t          CallStaticVoidMethod;
    JNI_CallStaticVoidMethodV_t         CallStaticVoidMethodV;
    JNI_CallStaticVoidMethodA_t         CallStaticVoidMethodA;

    JNI_GetStaticFieldID_t              GetStaticFieldID;
    JNI_GetStaticObjectField_t          GetStaticObjectField;
    JNI_GetStaticBooleanField_t         GetStaticBooleanField;
    JNI_GetStaticByteField_t            GetStaticByteField;
    JNI_GetStaticCharField_t            GetStaticCharField;
    JNI_GetStaticShortField_t           GetStaticShortField;
    JNI_GetStaticIntField_t             GetStaticIntField;
    JNI_GetStaticLongField_t            GetStaticLongField;
    JNI_GetStaticFloatField_t           GetStaticFloatField;
    JNI_GetStaticDoubleField_t          GetStaticDoubleField;
    JNI_SetStaticObjectField_t          SetStaticObjectField;
    JNI_SetStaticBooleanField_t         SetStaticBooleanField;
    JNI_SetStaticByteField_t            SetStaticByteField;
    JNI_SetStaticCharField_t            SetStaticCharField;
    JNI_SetStaticShortField_t           SetStaticShortField;
    JNI_SetStaticIntField_t             SetStaticIntField;
    JNI_SetStaticLongField_t            SetStaticLongField;
    JNI_SetStaticFloatField_t           SetStaticFloatField;
    JNI_SetStaticDoubleField_t          SetStaticDoubleField;

    JNI_NewString_t                     NewString;
    JNI_GetStringLength_t               GetStringLength;
    JNI_GetStringChars_t                GetStringChars;
    JNI_ReleaseStringChars_t            ReleaseStringChars;
    JNI_NewStringUTF_t                  NewStringUTF;
    JNI_GetStringUTFLength_t            GetStringUTFLength;
    JNI_GetStringUTFChars_t             GetStringUTFChars;
    JNI_ReleaseStringUTFChars_t         ReleaseStringUTFChars;
    JNI_GetArrayLength_t                GetArrayLength;
    JNI_NewObjectArray_t                NewObjectArray;
    JNI_GetObjectArrayElement_t         GetObjectArrayElement;
    JNI_SetObjectArrayElement_t         SetObjectArrayElement;
    JNI_NewBooleanArray_t               NewBooleanArray;
    JNI_NewByteArray_t                  NewByteArray;
    JNI_NewCharArray_t                  NewCharArray;
    JNI_NewShortArray_t                 NewShortArray;
    JNI_NewIntArray_t                   NewIntArray;
    JNI_NewLongArray_t                  NewLongArray;
    JNI_NewFloatArray_t                 NewFloatArray;
    JNI_NewDoubleArray_t                NewDoubleArray;
    JNI_GetBooleanArrayElements_t       GetBooleanArrayElements;
    JNI_GetByteArrayElements_t          GetByteArrayElements;
    JNI_GetCharArrayElements_t          GetCharArrayElements;
    JNI_GetShortArrayElements_t         GetShortArrayElements;
    JNI_GetIntArrayElements_t           GetIntArrayElements;
    JNI_GetLongArrayElements_t          GetLongArrayElements;
    JNI_GetFloatArrayElements_t         GetFloatArrayElements;
    JNI_GetDoubleArrayElements_t        GetDoubleArrayElements;
    JNI_ReleaseBooleanArrayElements_t   ReleaseBooleanArrayElements;
    JNI_ReleaseByteArrayElements_t      ReleaseByteArrayElements;
    JNI_ReleaseCharArrayElements_t      ReleaseCharArrayElements;
    JNI_ReleaseShortArrayElements_t     ReleaseShortArrayElements;
    JNI_ReleaseIntArrayElements_t       ReleaseIntArrayElements;
    JNI_ReleaseLongArrayElements_t      ReleaseLongArrayElements;
    JNI_ReleaseFloatArrayElements_t     ReleaseFloatArrayElements;
    JNI_ReleaseDoubleArrayElements_t    ReleaseDoubleArrayElements;
    JNI_GetBooleanArrayRegion_t         GetBooleanArrayRegion;
    JNI_GetByteArrayRegion_t            GetByteArrayRegion;
    JNI_GetCharArrayRegion_t            GetCharArrayRegion;
    JNI_GetShortArrayRegion_t           GetShortArrayRegion;
    JNI_GetIntArrayRegion_t             GetIntArrayRegion;
    JNI_GetLongArrayRegion_t            GetLongArrayRegion;
    JNI_GetFloatArrayRegion_t           GetFloatArrayRegion;
    JNI_GetDoubleArrayRegion_t          GetDoubleArrayRegion;
    JNI_SetBooleanArrayRegion_t         SetBooleanArrayRegion;
    JNI_SetByteArrayRegion_t            SetByteArrayRegion;
    JNI_SetCharArrayRegion_t            SetCharArrayRegion;
    JNI_SetShortArrayRegion_t           SetShortArrayRegion;
    JNI_SetIntArrayRegion_t             SetIntArrayRegion;
    JNI_SetLongArrayRegion_t            SetLongArrayRegion;
    JNI_SetFloatArrayRegion_t           SetFloatArrayRegion;
    JNI_SetDoubleArrayRegion_t          SetDoubleArrayRegion;

    JNI_RegisterNatives_t               RegisterNatives;
    JNI_UnregisterNatives_t             UnregisterNatives;
    JNI_MonitorEnter_t                  MonitorEnter;
    JNI_MonitorExit_t                   MonitorExit;
    JNI_GetJavaVM_t                     GetJavaVM;

    JNI_GetStringRegion_t               GetStringRegion;
    JNI_GetStringUTFRegion_t            GetStringUTFRegion;
    JNI_GetPrimitiveArrayCritical_t     GetPrimitiveArrayCritical;
    JNI_ReleasePrimitiveArrayCritical_t ReleasePrimitiveArrayCritical;
    JNI_GetStringCritical_t             GetStringCritical;
    JNI_ReleaseStringCritical_t         ReleaseStringCritical;

    JNI_NewWeakGlobalRef_t              NewWeakGlobalRef;
    JNI_DeleteWeakGlobalRef_t           DeleteWeakGlobalRef;
    JNI_ExceptionCheck_t                ExceptionCheck;

    JNI_NewDirectByteBuffer_t           NewDirectByteBuffer;
    JNI_GetDirectBufferAddress_t        GetDirectBufferAddress;
    JNI_GetDirectBufferCapacity_t       GetDirectBufferCapacity;
    JNI_GetObjectRefType_t              GetObjectRefType;
    JNI_GetModule_t                     GetModule;
};

// JNIEnv_ — HotSpot stores a pointer to JNINativeInterface_ at offset 0x00.
// AttachCurrentThread returns env = JavaThread + 0x2C0, where *env = fn table ptr.
struct JNIEnv_ {
    const JNINativeInterface_* functions;                    // 0x00
};

// ---------------------------------------------------------------------------
// Known internal globals (RVAs relative to image base 0x180000000)
//
// At runtime, relocate each via:
//   uintptr_t jvmBase = (uintptr_t)GetModuleHandleA("jvm.dll");
//   auto* real_ptr   = (T*)(jvmBase + RVA);
//
// CAVEAT: these internal data-structure offsets are build-specific.
//         They WILL shift across HotSpot versions. Re-reverse via
//         Ghidra if targeting a different jvm.dll build.
// ---------------------------------------------------------------------------
struct Offsets {
    static constexpr uint64_t ANALYSIS_BASE = 0x180000000;

    // g_vm_init_state — dword: 0=uninit, 2=running
    //   MOV EAX,[0x180b97148] → RVA = 0xb97148
    static constexpr uint64_t RVA_VM_INIT_STATE = 0xb97148;

    // g_vm_list — JavaVM*[] (one entry, populated by JNI_CreateJavaVM)
    //   LEA RAX,[0x180b29840] → RVA = 0xb29840
    static constexpr uint64_t RVA_VM_LIST = 0xb29840;

    // JNIInvokeInterface_ instance
    //   Referenced at RVA 0x974ad0
    static constexpr uint64_t RVA_INVOKE_IFACE = 0x974ad0;

    // g_tls_thread_index — dword TLS slot index
    //   Accessed via [0x180bd3d38] → RVA = 0xbd3d38
    static constexpr uint64_t RVA_TLS_SLOT_IDX = 0xbd3d38;

    // thread_ptr offsets within HotSpot's JavaThread structure (reached via TLS)
    static constexpr uint32_t THREAD_JNIENV_OFF = 0x20; // JNIEnv*
    static constexpr uint32_t THREAD_INIT_FLAG   = 0x50; // byte, checked before lazy init
};

// ---------------------------------------------------------------------------
// JvmDll — runtime helper: resolves JNI_GetCreatedJavaVMs via GetProcAddress
//          and relocates internal RVAs to the module's runtime base.
//
// Usage (payload DllMain):
//   JvmDll jvm;
//   if (!jvm.init()) return FALSE;
//
//   JavaVM* vm = nullptr;
//   jsize   numVMs = 0;
//   jvm.JNI_GetCreatedJavaVMs(&vm, 1, &numVMs);
//   if (numVMs == 0) { /* JVM not ready yet */ }
//
//   // Attach current thread and get JNIEnv:
//   JNIEnv* env = nullptr;
//   vm->functions->AttachCurrentThread(vm, &env, nullptr);
//
//   TLS shortcut (use only from an already-attached thread):
//     // uintptr_t jvmBase = (uintptr_t)GetModuleHandleA("jvm.dll");
//     // DWORD slot = *(DWORD*)(jvmBase + Offsets::RVA_TLS_SLOT_IDX);
//     // auto* thread_ptr = *(uintptr_t*)(__readgsqword(0x58) + slot * 8);
//     // JNIEnv* fast_env = *(JNIEnv**)(thread_ptr + Offsets::THREAD_JNIENV_OFF);
// ---------------------------------------------------------------------------
struct JvmDll {
    // Resolved at init — GetProcAddress
    jint (JNICALL *JNI_GetCreatedJavaVMs)(JavaVM*, jsize, jsize*) = nullptr;

    uintptr_t base = 0;        // runtime base address of jvm.dll
    bool      ok   = false;

    bool init() {
        HMODULE hJvm = GetModuleHandleA("jvm.dll");
        if (!hJvm) {
            // jvm.dll may not be loaded yet — caller should retry.
            // In an injected DLL, jvm.dll is already mapped; this should succeed.
            return false;
        }
        base = (uintptr_t)hJvm;

        // Resolve the public export — works across versions
        FARPROC p = GetProcAddress(hJvm, "JNI_GetCreatedJavaVMs");
        if (!p) return false;
        JNI_GetCreatedJavaVMs = (decltype(JNI_GetCreatedJavaVMs))p;

        ok = true;
        return true;
    }

    // Relocate an RVA (from Offsets) to a runtime pointer.
    template <typename T = void*>
    T* addr(uint32_t rva) const {
        return (T*)(base + rva);
    }
};
