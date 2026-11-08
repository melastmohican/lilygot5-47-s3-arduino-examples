# FastEPD on the LilyGo T5 4.7" ESP32-S3

FastEPD (bitbank2/FastEPD) has no preset for the original **LilyGo T5-4.7-S3**
(ED047TC1, 960x540, 74HC595 control shift register). `BB_PANEL_LILYGO_T5PRO` is
the newer S3-Pro and uses different pins.

**Sketches no longer need the patch.** They include `lilygo_t5_47_s3.h` (in this folder), which builds
a custom panel definition and reuses FastEPD's own LilyGo shift-register callbacks, so stock FastEPD
2.2.0 works unmodified. The approach comes from [vulnersCom/VulnCast](https://github.com/vulnersCom/VulnCast)
(`src/board_v24.*`). The patch below is optional and kept for reference; it gives the same result as
`initPanel(BB_PANEL_LILYGO_T5_47_S3)`.

```cpp
#include <FastEPD.h>
#include "../lilygo_t5_47_s3.h"
T5EPD epaper;          // subclass of FASTEPD, instead of FASTEPD epaper;

void setup() {
  epaper.initT5();     // 960x540, gray matrix, LCD bus, 1-bit mode (setMode() afterwards as usual)
}
```

How it works: FastEPD's `LilyGoEinkPower/IOInit/RowControl` (external linkage) drive the same 74HC595
as this board; `initCustomPanel()` registers the pins; it skips `bbepIOInit()` (the S3 LCD i80 bus
setup), so `T5EPD` calls it itself (needs the subclass because `_state` is protected). The gray matrix
is a copy of `u8M5Matrix` (internal linkage in FastEPD). Risk: it relies on FastEPD internals, so a
future FastEPD release could rename them.

---

`lilygo-t5-47-s3.patch` (optional) adds `BB_PANEL_LILYGO_T5_47_S3` to the library.
Written against FastEPD **2.2.0**; touches only `src/FastEPD.h` and `src/FastEPD.inl`.

## Apply the optional patch (also after every FastEPD update)

```sh
cd ~/Src/Arduino/libraries/FastEPD
patch -p1 --dry-run < ~/Src/Arduino/LilyGoT5ePaper/FastEPD/lilygo-t5-47-s3.patch   # check first
patch -p1            < ~/Src/Arduino/LilyGoT5ePaper/FastEPD/lilygo-t5-47-s3.patch
```

Remove it: same command with `patch -p1 -R`.

If the dry run fails, upstream changed those files. Re-add the three pieces by
hand (see "What the patch does") and regenerate the patch.

## Use in a sketch (with the patch)

```cpp
#include <FastEPD.h>
FASTEPD epaper;

void setup() {
  epaper.initPanel(BB_PANEL_LILYGO_T5_47_S3);   // sets size (960x540) and gray matrix
  epaper.clearWhite();
  epaper.fullUpdate(CLEAR_NONE, true);          // REQUIRED once before any partialUpdate()
  // ... draw ...
  epaper.partialUpdate(true);
}
```

Arduino IDE settings: ESP32S3 Dev Module, PSRAM **OPI**, 16MB flash.

## Gotchas

- **`partialUpdate()` needs one `fullUpdate()` first.** `clearWhite()` does not
  set the previous-frame mode, so `partialUpdate()` returns
  `BBEP_ERROR_BAD_PARAMETER` (1) and nothing is drawn. Call
  `fullUpdate(CLEAR_NONE, true)` once after clearing.
- This applies to every FastEPD example that uses `partialUpdate()`.
- Other examples hardcode `BB_PANEL_V7_RAW` / `setPanelSize(...)` for EPDiy
  boards. Replace both with the `initPanel(BB_PANEL_LILYGO_T5_47_S3)` line above.

- **Do not pass `iStartRow`/`iEndRow` to `partialUpdate()`.** In 2.2.0 the skipped rows alternate between
  two DMA buffers and only one is zeroed, so the other keeps the last data row and draws gray vertical
  streaks down the whole panel (seen in `macpaint_hello`). Call `partialUpdate(true)` with no range.

## What the patch does

1. `FastEPD.h`: new enum value `BB_PANEL_LILYGO_T5_47_S3`, placed after
   `BB_PANEL_SENSORIA_C5` so existing values keep their numbers.
2. `FastEPD.inl` `panelDefs[]`: pin map and settings.
3. `FastEPD.inl` `panelProcs[]`: reuses the S3-Pro `LilyGoEinkPower / LilyGoIOInit /
   LilyGoRowControl`. The shift-register bit layout is identical (latch 0x01,
   power_disable 0x02, pos 0x04, neg 0x08, stv 0x10, scan_dir 0x20, mode 0x40,
   output_enable 0x80).

Both tables are indexed by the enum, so entries must stay in the same order.

| Signal | GPIO |
|---|---|
| Data D0..D7 | 8, 1, 2, 3, 4, 5, 6, 7 |
| CKV | 38 |
| STH (FastEPD `ioSPH`) | 40 |
| CKH (FastEPD `ioCL`) | 41 |
| Shift register data / clock / strobe | 13 / 12 / 0 |
| LCD driver dummy DC pin | 46 (unused on this board) |

Source of the pins: LilyGo-EPD47 `src/ed047tc1.h` (ESP32-S3 section).

## Settings that were not verified

- Bus speed is 10 MHz (what LilyGo-EPD47 uses). The S3-Pro preset uses 20 MHz;
  try that if updates feel slow.
- `BB_PANEL_FLAG_SLOW_SPH` is copied from the S3-Pro preset. It works on this
  board; the alternative (flag removed) was not tried.

## Verified

`animated_smiles` (8 bouncing smileys, `partialUpdate` ~153 ms/frame) runs on
the board with ESP32 Arduino core 4.0.0-alpha1.

## Upstream

Not in FastEPD `main` as of 2026-11-06 (latest release 2.2.0). A PR could
carry this as-is.
