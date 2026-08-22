# ═══════════════════════════════════════════════════════════════
# com/crystal/ShowGuidelineRunnable.smali
# ═══════════════════════════════════════════════════════════════

.class public Lcom/crystal/ShowGuidelineRunnable;
.super Ljava/lang/Object;
.implements Ljava/lang/Runnable;

.field private service:Lcom/crystal/GuidelineService;

.method public constructor <init>(Lcom/crystal/GuidelineService;)V
    .locals 0
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    iput-object p1, p0, Lcom/crystal/ShowGuidelineRunnable;->service:Lcom/crystal/GuidelineService;
    return-void
.end method

.method public run()V
    .locals 1
    iget-object v0, p0, Lcom/crystal/ShowGuidelineRunnable;->service:Lcom/crystal/GuidelineService;
    invoke-virtual {v0}, Lcom/crystal/GuidelineService;->createOverlay()V
    return-void
.end method
