# Crystal Guideline — 8 Ball Pool Android

Aim assistant mod for 8 Ball Pool mobile (arm64-v8a).  
Ported from the [Windows/DirectX version](https://www.unknowncheats.me).

## Modes
- **Ghost Ball** — ghost ball + tangent predictor
- **Line Extender** — extends shot direction with wall bounces (3×)

## Build

```bash
# Local (requires Android NDK)
export NDK_PATH=/path/to/ndk-26.x.x
./build.sh

# GitHub Actions — push to main, artifact auto-uploads
```

## Patch APK

```bash
# After build.sh finishes:
./patch.sh 8ballpool.apk
# Output: dist/crystal_patched_signed.apk
```

## Project Structure

```
jni/
├── MathUtils.hpp       ← Vector2, reflect math (unchanged from PC)
├── Memory.hpp          ← Android in-process memory reader
├── PatternScanner.hpp  ← ARM64 /proc/self/maps pattern scanner
├── Guideline.hpp       ← Core aim logic → DrawCmd output
└── main.cpp            ← JNI bridge + Canvas rendering

smali/com/crystal/
├── CrystalGuideline.smali  ← native method declarations
├── GuidelineView.smali     ← transparent overlay View
├── GuidelineService.smali  ← Android Service (manages overlay)
└── ShowGuidelineRunnable.smali

.github/workflows/build.yml ← GitHub Actions CI
```

## Touch Controls

| Gesture | Action |
|---------|--------|
| Drag | Move selected point |
| Single tap | Cycle selected point (Cue → Target → Hole) |
| Double tap | Toggle mode (Ghost Ball ↔ Line Extender) |
