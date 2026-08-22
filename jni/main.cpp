// ═══════════════════════════════════════════════════════════════
// main.cpp — Crystal Guideline  |  Android JNI bridge
//
// Native exports:
//   Java_com_crystal_CrystalGuideline_init(ctx)    ← called from Application.onCreate
//   Java_com_crystal_CrystalGuideline_drawFrame(canvas, w, h) ← called from View.onDraw
//   Java_com_crystal_CrystalGuideline_onTouch(x, y, action)  ← called from View.onTouchEvent
//   Java_com_crystal_CrystalGuideline_setPoint(id, x, y)     ← from tap-select mode
//   Java_com_crystal_CrystalGuideline_cycleMode()             ← double-tap
//   Java_com_crystal_CrystalGuideline_cyclePoint()            ← single-tap empty area
//   Java_com_crystal_CrystalGuideline_getMode()   → jstring
// ═══════════════════════════════════════════════════════════════

#include "Guideline.hpp"
#include "Memory.hpp"
#include <jni.h>
#include <android/log.h>
#include <string>

#define TAG "CrystalGuideline"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static JavaVM* gJvm     = nullptr;
static jobject gContext = nullptr;

// ─── Cached JNI IDs for Canvas (resolved once on first drawFrame) ──
static bool     canvasResolved  = false;
static jclass   gCanvasClass    = nullptr;
static jmethodID gDrawLine      = nullptr;
static jmethodID gDrawCircle    = nullptr;
static jclass   gPaintClass     = nullptr;
static jmethodID gPaintNew      = nullptr;
static jmethodID gPaintSetColor = nullptr;
static jmethodID gPaintSetStW   = nullptr;
static jmethodID gPaintSetStyle = nullptr;
static jmethodID gPaintSetAA    = nullptr;
static jobject   gPaintInst     = nullptr;   // single reusable Paint object
static jobject   gStyleStroke   = nullptr;   // Paint.Style.STROKE
static jobject   gStyleFill     = nullptr;   // Paint.Style.FILL

static bool ResolveCanvasIDs(JNIEnv* env) {
    if (canvasResolved) return true;

    gCanvasClass = (jclass)env->NewGlobalRef(env->FindClass("android/graphics/Canvas"));
    gPaintClass  = (jclass)env->NewGlobalRef(env->FindClass("android/graphics/Paint"));
    if (!gCanvasClass || !gPaintClass) { LOGE("Canvas/Paint class not found"); return false; }

    gDrawLine    = env->GetMethodID(gCanvasClass, "drawLine",   "(FFFFLandroid/graphics/Paint;)V");
    gDrawCircle  = env->GetMethodID(gCanvasClass, "drawCircle", "(FFFLandroid/graphics/Paint;)V");

    gPaintNew      = env->GetMethodID(gPaintClass, "<init>",       "()V");
    gPaintSetColor = env->GetMethodID(gPaintClass, "setColor",     "(I)V");
    gPaintSetStW   = env->GetMethodID(gPaintClass, "setStrokeWidth","(F)V");
    gPaintSetStyle = env->GetMethodID(gPaintClass, "setStyle",
                     "(Landroid/graphics/Paint$Style;)V");
    gPaintSetAA    = env->GetMethodID(gPaintClass, "setAntiAlias", "(Z)V");

    // Create a reusable Paint instance
    jobject localPaint = env->NewObject(gPaintClass, gPaintNew);
    env->CallVoidMethod(localPaint, gPaintSetAA, (jboolean)JNI_TRUE);
    gPaintInst = env->NewGlobalRef(localPaint);
    env->DeleteLocalRef(localPaint);

    // Resolve Paint.Style enum constants
    jclass styleClass = env->FindClass("android/graphics/Paint$Style");
    jfieldID strokeField = env->GetStaticFieldID(styleClass, "STROKE",
                           "Landroid/graphics/Paint$Style;");
    jfieldID fillField   = env->GetStaticFieldID(styleClass, "FILL",
                           "Landroid/graphics/Paint$Style;");
    gStyleStroke = env->NewGlobalRef(env->GetStaticObjectField(styleClass, strokeField));
    gStyleFill   = env->NewGlobalRef(env->GetStaticObjectField(styleClass, fillField));

    canvasResolved = true;
    LOGI("Canvas JNI IDs resolved");
    return true;
}

// ═══════════════════════════════════════════════════════════════
// JNI_OnLoad
// ═══════════════════════════════════════════════════════════════
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    gJvm = vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;

    // Init memory bases (libil2cpp.so should already be loaded by game)
    gMem.Init();

    LOGI("CrystalGuideline loaded | il2cpp@0x%lx", gMem.il2cppBase);
    return JNI_VERSION_1_6;
}

// ═══════════════════════════════════════════════════════════════
// CrystalGuideline.init(Context)
// Called from Application.onCreate injection → main thread
// ═══════════════════════════════════════════════════════════════
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_init(JNIEnv* env, jclass, jobject ctx) {
    if (!gJvm) return;
    gContext = env->NewGlobalRef(ctx);

    // Try auto-memory (8 Ball Pool offsets)
    if (!gGuideline.InitMemory()) {
        LOGI("Memory init failed — manual mode active");
    }

    // startService → GuidelineService → creates GuidelineView overlay
    jclass intentClass = env->FindClass("android/content/Intent");
    jclass svcClass    = env->FindClass("com/crystal/GuidelineService");
    if (!intentClass || !svcClass) {
        LOGE("GuidelineService smali not injected?");
        return;
    }
    jmethodID intentInit = env->GetMethodID(intentClass, "<init>",
                           "(Landroid/content/Context;Ljava/lang/Class;)V");
    jobject intent = env->NewObject(intentClass, intentInit, ctx, svcClass);

    jclass  ctxClass   = env->GetObjectClass(ctx);
    jmethodID startSvc = env->GetMethodID(ctxClass, "startService",
                         "(Landroid/content/Intent;)Landroid/content/ComponentName;");
    env->CallObjectMethod(ctx, startSvc, intent);
    env->DeleteLocalRef(intent);

    LOGI("CrystalGuideline::init done");
}

// ═══════════════════════════════════════════════════════════════
// CrystalGuideline.drawFrame(Canvas, float w, float h)
// Called from GuidelineView.onDraw() — on the main/render thread.
// Computes all guidelines and draws to Android Canvas via JNI.
// ═══════════════════════════════════════════════════════════════
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_drawFrame(JNIEnv* env, jclass,
                                            jobject canvas,
                                            jfloat w, jfloat h)
{
    if (!ResolveCanvasIDs(env)) return;

    gGuideline.SetScreen(w, h);
    gGuideline.ComputeFrame();  // fills gDrawCmds

    for (const DrawCmd& cmd : gDrawCmds) {
        // Set color (ARGB → Android uses same format)
        env->CallVoidMethod(gPaintInst, gPaintSetColor, (jint)cmd.color);
        env->CallVoidMethod(gPaintInst, gPaintSetStW,   cmd.strokeWidth);

        switch (cmd.type) {
            case DrawCmd::LINE:
                env->CallVoidMethod(gPaintInst, gPaintSetStyle, gStyleStroke);
                env->CallVoidMethod(canvas, gDrawLine,
                    cmd.x1, cmd.y1, cmd.x2, cmd.y2, gPaintInst);
                break;
            case DrawCmd::CIRCLE_STROKE:
                env->CallVoidMethod(gPaintInst, gPaintSetStyle, gStyleStroke);
                env->CallVoidMethod(canvas, gDrawCircle,
                    cmd.x1, cmd.y1, cmd.radius, gPaintInst);
                break;
            case DrawCmd::CIRCLE_FILL:
                env->CallVoidMethod(gPaintInst, gPaintSetStyle, gStyleFill);
                env->CallVoidMethod(canvas, gDrawCircle,
                    cmd.x1, cmd.y1, cmd.radius, gPaintInst);
                break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Touch + control bridges
// ═══════════════════════════════════════════════════════════════
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_onTouch(JNIEnv*, jclass, jfloat x, jfloat y, jint action) {
    gGuideline.OnTouch(x, y, (int)action);
}

extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setPoint(JNIEnv*, jclass, jint pointId, jfloat x, jfloat y) {
    gGuideline.SetPoint((int)pointId, x, y);
}

extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_cycleMode(JNIEnv*, jclass) {
    gGuideline.CycleMode();
}

extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_cyclePoint(JNIEnv*, jclass) {
    gGuideline.CyclePoint();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_crystal_CrystalGuideline_getMode(JNIEnv* env, jclass) {
    const char* mode = (gGuideline.currentMode == GuidelineCore::GHOST_BALL)
        ? "GHOST_BALL" : "LINE_EXTENDER";
    return env->NewStringUTF(mode);
}

// Table calibration
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setTableBounds(JNIEnv*, jclass,
    jfloat left, jfloat top, jfloat right, jfloat bottom)
{
    gGuideline.tableLeft   = left;
    gGuideline.tableTop    = top;
    gGuideline.tableRight  = right;
    gGuideline.tableBottom = bottom;
}

extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setBallRadius(JNIEnv*, jclass, jfloat r) {
    gGuideline.ballRadius = r;
}
