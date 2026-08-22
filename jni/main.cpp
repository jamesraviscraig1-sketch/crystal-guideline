// ═══════════════════════════════════════════════════════════════
// main.cpp — Crystal 8 Ball Pool  |  Android JNI bridge
//
// JNI exports (called from smali injection):
//   init(Context)             — startup, resolve functions
//   drawFrame(Canvas, w, h)   — render guideline each frame
//   onTouch(x, y, action)     — touch routing
//   setPoint(id, x, y)        — tap-to-place point
//   cycleMode()               — double-tap to swap mode
//   cyclePoint()              — single-tap to change active point
//   getMode()                 — current mode string
//   setTableBounds(l,t,r,b)   — calibrate table boundary
//   setBallRadius(r)           — calibrate ball size
//   onMatchEnd()              — trigger auto-queue check
//
//   ── Config setters (called from settings UI) ──
//   setAimAction(int)         — 0=none 1=aim 2=auto_play
//   setHumanizedPower(bool, float)
//   setHumanizedAngle(bool, float)
//   setDelayMode(int, int, int, int)
//   setVisualPrediction(bool, float, float, int, bool)
//   setVisualShotState(bool, float, float, int, bool)
//   setPredState(int)         — 0=none 1=enemy 2=both
//   setWideGuideline(bool)
//   setColorStyle(int)
//   setProfileChanger(bool, long, long, int, String, String)
//   setAutoQueue(bool, int)
//   setAdBlock(bool)
// ═══════════════════════════════════════════════════════════════

#include "Guideline.hpp"
#include "ProfileChanger.hpp"
#include "Misc.hpp"
#include "Config.hpp"
#include <jni.h>
#include <android/log.h>
#include <string>

#define TAG "CrystalGuideline"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static JavaVM* gJvm     = nullptr;
static jobject gContext = nullptr;

// ── Canvas JNI cache ──────────────────────────────────────────
static bool      canvasResolved  = false;
static jclass    gCanvasClass    = nullptr;
static jclass    gPaintClass     = nullptr;
static jmethodID gDrawLine       = nullptr;
static jmethodID gDrawCircle     = nullptr;
static jmethodID gPaintNew       = nullptr;
static jmethodID gPaintSetColor  = nullptr;
static jmethodID gPaintSetStW    = nullptr;
static jmethodID gPaintSetStyle  = nullptr;
static jmethodID gPaintSetAA     = nullptr;
static jobject   gPaintInst      = nullptr;
static jobject   gStyleStroke    = nullptr;
static jobject   gStyleFill      = nullptr;

static bool ResolveCanvasIDs(JNIEnv* env) {
    if (canvasResolved) return true;
    gCanvasClass = (jclass)env->NewGlobalRef(env->FindClass("android/graphics/Canvas"));
    gPaintClass  = (jclass)env->NewGlobalRef(env->FindClass("android/graphics/Paint"));
    if (!gCanvasClass || !gPaintClass) { LOGE("Canvas/Paint class not found"); return false; }
    gDrawLine    = env->GetMethodID(gCanvasClass,"drawLine",   "(FFFFLandroid/graphics/Paint;)V");
    gDrawCircle  = env->GetMethodID(gCanvasClass,"drawCircle", "(FFFLandroid/graphics/Paint;)V");
    gPaintNew      = env->GetMethodID(gPaintClass,"<init>",        "()V");
    gPaintSetColor = env->GetMethodID(gPaintClass,"setColor",      "(I)V");
    gPaintSetStW   = env->GetMethodID(gPaintClass,"setStrokeWidth","(F)V");
    gPaintSetStyle = env->GetMethodID(gPaintClass,"setStyle",      "(Landroid/graphics/Paint$Style;)V");
    gPaintSetAA    = env->GetMethodID(gPaintClass,"setAntiAlias",  "(Z)V");
    jobject lp = env->NewObject(gPaintClass, gPaintNew);
    env->CallVoidMethod(lp, gPaintSetAA, (jboolean)JNI_TRUE);
    gPaintInst = env->NewGlobalRef(lp);
    env->DeleteLocalRef(lp);
    jclass sc = env->FindClass("android/graphics/Paint$Style");
    gStyleStroke = env->NewGlobalRef(env->GetStaticObjectField(sc,
        env->GetStaticFieldID(sc,"STROKE","Landroid/graphics/Paint$Style;")));
    gStyleFill   = env->NewGlobalRef(env->GetStaticObjectField(sc,
        env->GetStaticFieldID(sc,"FILL","Landroid/graphics/Paint$Style;")));
    canvasResolved = true;
    LOGI("Canvas JNI resolved");
    return true;
}

// ═══════════════════════════════════════════════════════════════
// JNI_OnLoad
// ═══════════════════════════════════════════════════════════════
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    gJvm = vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
    gMem.Init();
    LOGI("Crystal loaded | il2cpp@0x%lx", gMem.il2cppBase);
    return JNI_VERSION_1_6;
}

// ═══════════════════════════════════════════════════════════════
// init(Context ctx)
// ═══════════════════════════════════════════════════════════════
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_init(JNIEnv* env, jclass, jobject ctx) {
    gContext = env->NewGlobalRef(ctx);
    gGuideline.InitMemory();

    // Start overlay service
    jclass intentClass = env->FindClass("android/content/Intent");
    jclass svcClass    = env->FindClass("com/crystal/GuidelineService");
    if (intentClass && svcClass) {
        jmethodID init = env->GetMethodID(intentClass,"<init>",
            "(Landroid/content/Context;Ljava/lang/Class;)V");
        jobject intent = env->NewObject(intentClass, init, ctx, svcClass);
        jclass  ctxCls = env->GetObjectClass(ctx);
        jmethodID ss   = env->GetMethodID(ctxCls,"startService",
            "(Landroid/content/Intent;)Landroid/content/ComponentName;");
        env->CallObjectMethod(ctx, ss, intent);
        env->DeleteLocalRef(intent);
    }
    LOGI("init done");
}

// ═══════════════════════════════════════════════════════════════
// drawFrame(Canvas, float w, float h)
// ═══════════════════════════════════════════════════════════════
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_drawFrame(JNIEnv* env, jclass,
                                            jobject canvas, jfloat w, jfloat h)
{
    if (!ResolveCanvasIDs(env)) return;
    gGuideline.SetScreen(w, h);
    gGuideline.ComputeFrame();

    for (const DrawCmd& cmd : gDrawCmds) {
        env->CallVoidMethod(gPaintInst, gPaintSetColor, (jint)cmd.color);
        env->CallVoidMethod(gPaintInst, gPaintSetStW,   cmd.strokeWidth);
        switch (cmd.type) {
            case DrawCmd::LINE:
                env->CallVoidMethod(gPaintInst, gPaintSetStyle, gStyleStroke);
                env->CallVoidMethod(canvas, gDrawLine,
                    cmd.x1,cmd.y1,cmd.x2,cmd.y2,gPaintInst);
                break;
            case DrawCmd::CIRCLE_STROKE:
                env->CallVoidMethod(gPaintInst, gPaintSetStyle, gStyleStroke);
                env->CallVoidMethod(canvas, gDrawCircle,
                    cmd.x1,cmd.y1,cmd.radius,gPaintInst);
                break;
            case DrawCmd::CIRCLE_FILL:
                env->CallVoidMethod(gPaintInst, gPaintSetStyle, gStyleFill);
                env->CallVoidMethod(canvas, gDrawCircle,
                    cmd.x1,cmd.y1,cmd.radius,gPaintInst);
                break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Touch + controls
// ═══════════════════════════════════════════════════════════════
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_onTouch(JNIEnv*, jclass, jfloat x, jfloat y, jint action) {
    gGuideline.OnTouch(x, y, (int)action);
}
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setPoint(JNIEnv*, jclass, jint id, jfloat x, jfloat y) {
    gGuideline.SetPoint((int)id, x, y);
}
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_cycleMode(JNIEnv*, jclass) { gGuideline.CycleMode(); }
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_cyclePoint(JNIEnv*, jclass) { gGuideline.CyclePoint(); }
extern "C" JNIEXPORT jstring JNICALL
Java_com_crystal_CrystalGuideline_getMode(JNIEnv* env, jclass) {
    return env->NewStringUTF(gGuideline.currentMode==GuidelineCore::GHOST_BALL
        ? "GHOST_BALL" : "LINE_EXTENDER");
}
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setTableBounds(JNIEnv*, jclass,
    jfloat l, jfloat t, jfloat r, jfloat b)
{
    gGuideline.tableLeft=l; gGuideline.tableTop=t;
    gGuideline.tableRight=r; gGuideline.tableBottom=b;
}
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setBallRadius(JNIEnv*, jclass, jfloat r) {
    gGuideline.ballRadius = r;
}

// ─── Match lifecycle ──────────────────────────────────────────
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_onMatchEnd(JNIEnv*, jclass) {
    gMisc.OnMatchEnd();
    gAimbot.OnTurnChange();
    gProfile.Reset();
}

// ─── Poll auto-queue tap request ─────────────────────────────
extern "C" JNIEXPORT jboolean JNICALL
Java_com_crystal_CrystalGuideline_needsQueueTap(JNIEnv*, jclass) {
    return gMisc.ConsumeQueueTap() ? JNI_TRUE : JNI_FALSE;
}

// ═══════════════════════════════════════════════════════════════
// CONFIG SETTERS — called from Java settings UI
// ═══════════════════════════════════════════════════════════════

// ── Aimbot ────────────────────────────────────────────────────
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setAimAction(JNIEnv*, jclass, jint mode) {
    gCfg.aimAction = (AimAction)mode;
    LOGI("AimAction = %d", mode);
}

extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setHumanizedPower(JNIEnv*, jclass,
    jboolean enabled, jfloat variance)
{
    gCfg.humanizedPower = (enabled == JNI_TRUE);
    gCfg.humanPowerVar  = variance;
}

extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setHumanizedAngle(JNIEnv*, jclass,
    jboolean enabled, jfloat speed)
{
    gCfg.humanizedAngle = (enabled == JNI_TRUE);
    gCfg.humanAngleSpeed = speed;
}

extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setDelayMode(JNIEnv*, jclass,
    jint mode, jint fixedMs, jint minMs, jint maxMs)
{
    gCfg.delayMode    = (DelayMode)mode;
    gCfg.aimDelayMs   = fixedMs;
    gCfg.aimDelayMin  = minMs;
    gCfg.aimDelayMax  = maxMs;
}

// ── Visual ────────────────────────────────────────────────────
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setVisualPrediction(JNIEnv*, jclass,
    jboolean draw, jfloat lineThick, jfloat ballRad, jint alpha, jboolean filled)
{
    gCfg.drawPrediction    = (draw   == JNI_TRUE);
    gCfg.predLineThickness = lineThick;
    gCfg.predBallRadius    = ballRad;
    gCfg.predAlpha         = (uint8_t)alpha;
    gCfg.predBallFilled    = (filled == JNI_TRUE);
    gGuideline.ballRadius  = ballRad;
}

extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setVisualShotState(JNIEnv*, jclass,
    jboolean draw, jfloat thick, jfloat rad, jint alpha, jboolean filled)
{
    gCfg.drawShotState        = (draw   == JNI_TRUE);
    gCfg.shotCircleThickness  = thick;
    gCfg.shotCircleRadius     = rad;
    gCfg.shotCircleAlpha      = (uint8_t)alpha;
    gCfg.shotCircleFilled     = (filled == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setPredState(JNIEnv*, jclass, jint state) {
    gCfg.predState = (PredictionState)state;
}

extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setWideGuideline(JNIEnv*, jclass, jboolean enabled) {
    gCfg.wideGuideline = (enabled == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setColorStyle(JNIEnv*, jclass, jint style) {
    gCfg.colorStyle = (ColorStyle)style;
}

// ── Profile Changer ───────────────────────────────────────────
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setProfileChanger(JNIEnv* env, jclass,
    jboolean enabled, jlong coins, jlong cash, jint level,
    jstring displayName, jstring userId)
{
    gCfg.profileChanger = (enabled == JNI_TRUE);
    gCfg.fakeCoins      = (uint64_t)coins;
    gCfg.fakeCash       = (uint64_t)cash;
    gCfg.fakeLevel      = (int32_t)level;

    if (displayName) {
        const char* dn = env->GetStringUTFChars(displayName, nullptr);
        strncpy(gCfg.fakeDisplayName, dn, sizeof(gCfg.fakeDisplayName)-1);
        env->ReleaseStringUTFChars(displayName, dn);
    }
    if (userId) {
        const char* uid = env->GetStringUTFChars(userId, nullptr);
        strncpy(gCfg.fakeUserId, uid, sizeof(gCfg.fakeUserId)-1);
        env->ReleaseStringUTFChars(userId, uid);
    }

    if (gCfg.profileChanger) gProfile.Apply();
}

// ── Miscellaneous ─────────────────────────────────────────────
extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setAutoQueue(JNIEnv*, jclass,
    jboolean enabled, jint delayMs)
{
    gCfg.autoQueue         = (enabled == JNI_TRUE);
    gCfg.autoQueueDelayMs  = delayMs;
}

extern "C" JNIEXPORT void JNICALL
Java_com_crystal_CrystalGuideline_setAdBlock(JNIEnv*, jclass, jboolean enabled) {
    gCfg.adBlock = (enabled == JNI_TRUE);
    gMisc.SetAdBlock(gCfg.adBlock);
}
