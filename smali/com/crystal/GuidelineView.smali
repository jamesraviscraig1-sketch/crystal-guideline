# ═══════════════════════════════════════════════════════════════
# com/crystal/GuidelineView.smali
# Transparent full-screen View.
# All drawing logic lives in CrystalGuideline.drawFrame (native).
# This smali just wires Canvas + MotionEvent to native.
# ═══════════════════════════════════════════════════════════════

.class public Lcom/crystal/GuidelineView;
.super Landroid/view/View;

.field private screenW:F
.field private screenH:F

# ── constructor ──────────────────────────────────────────────
.method public constructor <init>(Landroid/content/Context;)V
    .locals 0
    invoke-direct {p0, p1}, Landroid/view/View;-><init>(Landroid/content/Context;)V
    return-void
.end method

# ── onSizeChanged: track screen dimensions ───────────────────
.method protected onSizeChanged(IIII)V
    .locals 2

    int-to-float v0, p1
    iput v0, p0, Lcom/crystal/GuidelineView;->screenW:F

    int-to-float v1, p2
    iput v1, p0, Lcom/crystal/GuidelineView;->screenH:F

    invoke-super {p0, p1, p2, p3, p4}, Landroid/view/View;->onSizeChanged(IIII)V

    return-void
.end method

# ── onDraw: delegate all drawing to native ───────────────────
.method protected onDraw(Landroid/graphics/Canvas;)V
    .locals 2

    iget v0, p0, Lcom/crystal/GuidelineView;->screenW:F
    iget v1, p0, Lcom/crystal/GuidelineView;->screenH:F

    # native draws everything into canvas
    invoke-static {p1, v0, v1}, Lcom/crystal/CrystalGuideline;->drawFrame(Landroid/graphics/Canvas;FF)V

    invoke-super {p0, p1}, Landroid/view/View;->onDraw(Landroid/graphics/Canvas;)V

    return-void
.end method

# ── onTouchEvent: forward to native ─────────────────────────
.method public onTouchEvent(Landroid/view/MotionEvent;)Z
    .locals 4

    invoke-virtual {p1}, Landroid/view/MotionEvent;->getX()F
    move-result v0

    invoke-virtual {p1}, Landroid/view/MotionEvent;->getY()F
    move-result v1

    invoke-virtual {p1}, Landroid/view/MotionEvent;->getActionMasked()I
    move-result v2

    invoke-static {v0, v1, v2}, Lcom/crystal/CrystalGuideline;->onTouch(FFI)V

    # Redraw on every touch event
    invoke-virtual {p0}, Landroid/view/View;->invalidate()V

    const/4 v3, 0x1
    return v3
.end method
