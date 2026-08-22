# ═══════════════════════════════════════════════════════════════
# com/crystal/CrystalGuideline.smali — native bridge
# All heavy computation lives in libcrystal_guideline.so (main.cpp)
# ═══════════════════════════════════════════════════════════════

.class public Lcom/crystal/CrystalGuideline;
.super Ljava/lang/Object;

# init: called from Application.onCreate (injected smali)
# Stores context, starts GuidelineService
.method public static native init(Landroid/content/Context;)V
.end method

# drawFrame: called from GuidelineView.onDraw(Canvas)
# Computes guideline and draws directly to Canvas via JNI
.method public static native drawFrame(Landroid/graphics/Canvas;FF)V
.end method

# Touch events from GuidelineView.onTouchEvent
# action: 0=DOWN 1=UP 2=MOVE
.method public static native onTouch(FFI)V
.end method

# Tap-to-select: set specific point
# pointId: 1=CueBall 2=Target/LineTip 3=Hole
.method public static native setPoint(IFF)V
.end method

# Double-tap: cycle GHOST_BALL ↔ LINE_EXTENDER
.method public static native cycleMode()V
.end method

# Single-tap empty area: cycle selected point
.method public static native cyclePoint()V
.end method

# Returns "GHOST_BALL" or "LINE_EXTENDER"
.method public static native getMode()Ljava/lang/String;
.end method

# Table calibration (from calibration UI)
.method public static native setTableBounds(FFFF)V
.end method

# Adjust ball radius
.method public static native setBallRadius(F)V
.end method
