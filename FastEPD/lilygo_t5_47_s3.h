// LilyGo T5 4.7" ESP32-S3 (V2.4, ED047TC1, 960x540) support for stock FastEPD 2.2.0.
// No library patch needed. Usage in a sketch:
//
//   #include <FastEPD.h>
//   #include "../lilygo_t5_47_s3.h"
//   T5EPD epaper;                 // instead of FASTEPD epaper;
//   epaper.initT5();              // instead of epaper.initPanel(...)
//
// Approach taken from vulnersCom/VulnCast (src/board_v24.*): build a custom panel
// definition and reuse FastEPD's own LilyGo shift-register callbacks.
//
// - The V2.4 board uses the same 74HC595 control register and bit order as the
//   T5 S3-Pro, so FastEPD's LilyGoEinkPower / LilyGoIOInit / LilyGoRowControl are reused.
//   They have external linkage in FastEPD.cpp, so we only declare them here.
// - initCustomPanel() skips bbepIOInit() (the ESP32-S3 LCD i80 bus setup), so we call it
//   ourselves; it is also external, but _state is protected, hence the subclass.
// - The gray matrix is a copy of FastEPD's u8M5Matrix, which has internal linkage.
// - Pins are from LilyGo-EPD47 ed047tc1.h: data D0..D7 = 8,1,2,3,4,5,6,7; CKV 38;
//   STH 40; CKH 41; shift register DATA 13, CLK 12, STR 0; GPIO46 is the unused
//   dummy DC pin for the LCD peripheral.
#pragma once
#include <FastEPD.h>

extern int bbepIOInit(FASTEPDSTATE *pState);
extern int LilyGoEinkPower(void *pBBEP, int bOn);
extern int LilyGoIOInit(void *pBBEP);
extern void LilyGoRowControl(void *pBBEP, int iMode);

static const uint8_t t5MatrixV24[] = {
    1, 1, 1, 1, 1, 1, 1, 1,  2, 2, 1, 1, 2, 1, 1, 1,  2, 2, 1, 1, 1, 1, 2, 1,  2, 2, 1, 1, 2, 2, 1, 1,
    2, 2, 2, 2, 1, 1, 2, 1,  2, 2, 1, 1, 1, 2, 2, 1,  2, 2, 1, 1, 2, 1, 1, 2,  2, 2, 2, 1, 2, 1, 1, 2,
    2, 2, 2, 2, 2, 1, 2, 1,  1, 1, 1, 1, 1, 1, 2, 2,  2, 2, 1, 1, 1, 1, 2, 2,  1, 1, 1, 1, 2, 1, 2, 2,
    2, 2, 1, 1, 2, 1, 2, 2,  2, 1, 1, 2, 2, 1, 2, 2,  2, 2, 1, 2, 2, 1, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2,
};

static BBPANELDEF t5PanelDef = {
    .width = 960,
    .height = 540,
    .bus_speed = 10000000,
    .flags = BB_PANEL_FLAG_SLOW_SPH,
    .data = {8, 1, 2, 3, 4, 5, 6, 7, BB_NOT_USED, BB_NOT_USED, BB_NOT_USED, BB_NOT_USED,
             BB_NOT_USED, BB_NOT_USED, BB_NOT_USED, BB_NOT_USED},
    .bus_width = 8,
    .ioPWR = BB_NOT_USED,
    .ioSPV = BB_NOT_USED,
    .ioCKV = 38,
    .ioSPH = 40,
    .ioOE = BB_NOT_USED,
    .ioLE = BB_NOT_USED,
    .ioCL = 41,
    .ioPWR_Good = BB_NOT_USED,
    .ioSDA = 13,
    .ioSCL = 12,
    .ioShiftSTR = 0,
    .ioShiftMask = 0,
    .ioDCDummy = 46,
    .pGrayMatrix = t5MatrixV24,
    .iMatrixSize = sizeof(t5MatrixV24),
    .iLinePadding = 16,
    .iVCOM = -1600,
};

static BBPANELPROCS t5PanelProcs = {LilyGoEinkPower, LilyGoIOInit, LilyGoRowControl, nullptr, nullptr};

class T5EPD : public FASTEPD {
  public:
    // Same result as initPanel(BB_PANEL_LILYGO_T5_47_S3) with the patched library:
    // 1-bit mode, buffers allocated, panel ready for fullUpdate().
    int initT5() {
        int rc = initCustomPanel(&t5PanelDef, &t5PanelProcs);
        if (rc != BBEP_SUCCESS) return rc;
        rc = bbepIOInit(&_state);  // ESP32-S3 LCD i80 bus
        if (rc != BBEP_SUCCESS) return rc;
        rc = setPanelSize(t5PanelDef.width, t5PanelDef.height, t5PanelDef.flags, t5PanelDef.iVCOM);
        if (rc != BBEP_SUCCESS) return rc;
        return setMode(BB_MODE_1BPP);  // sets the pixel functions; initPanel() does this too
    }
};
