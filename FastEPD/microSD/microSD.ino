// microSD test for the LilyGo T5 4.7" ESP32-S3 (960x540, 16 grays) with FastEPD.
//
// Ported from the M5PaperDIY "microSD" sketch (M5Unified/M5GFX). Mounts the card, writes and reads back
// /LilyGo_Log.txt, then decodes /mocha800x480.png from the card and shows it centered as 16 grays.
// PNGs are decoded line by line with PNGdec, so the file never has to fit in RAM.
// Libraries: FastEPD, PNGdec.
//
// Card: FAT32, with mocha800x480.png in the root.
//
// Why FastEPD and not LilyGo-EPD47: LilyGo-EPD47 bundles its own zlib, which collides with PNGdec's
// (multiple definition of zError, zcalloc, ... at link time).
//
// Arduino Setting: Board "ESP32S3 Dev Module", USB CDC On Boot "Enable", Flash 16MB, PSRAM "OPI PSRAM",
// Partition Scheme "16M Flash (3M APP/9.9MB FATFS)".

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FastEPD.h>
#include "../lilygo_t5_47_s3.h"
#include <PNGdec.h>

// TF slot, from the LilyGo-EPD47 utilities.h (ESP32-S3 section)
#define SD_MISO  16
#define SD_MOSI  15
#define SD_SCLK  11
#define SD_CS    42

static const char *const imagePath = "/mocha800x480.png";

T5EPD epaper;
PNG *png;
File pngFile;
int imageX = 0, imageY = 0;   // top-left of the centered image

static void showStatus(const char *title, const char *detail) {
  epaper.fillScreen(0xF);
  epaper.setFont(FONT_12x16);
  epaper.setTextColor(0, 0xF);
  epaper.drawString(title, 60, 240);
  epaper.drawString(detail, 60, 280);
  epaper.fullUpdate(CLEAR_FAST, false);
}

// ---- PNGdec file callbacks (Arduino SD) ----
static void *pngOpen(const char *filename, int32_t *size) {
  pngFile = SD.open(filename);
  if (!pngFile) return nullptr;
  *size = pngFile.size();
  return &pngFile;
}
static void pngClose(void *) { if (pngFile) pngFile.close(); }
static int32_t pngRead(PNGFILE *, uint8_t *buf, int32_t len) { return pngFile ? pngFile.read(buf, len) : 0; }
static int32_t pngSeek(PNGFILE *, int32_t pos) { return pngFile ? pngFile.seek(pos) : 0; }

// One decoded line: RGB565 -> 4-bit gray (0 = black, 15 = white), written straight into the framebuffer.
static int pngDraw(PNGDRAW *d) {
  static uint16_t line[960];
  int y = imageY + d->y;
  if (y < 0 || y >= epaper.height()) return 1;
  png->getLineAsRGB565(d, line, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
  for (int x = 0; x < d->iWidth && imageX + x < epaper.width(); x++) {
    uint16_t p = line[x];
    int r = (p >> 11) << 3, g = ((p >> 5) & 0x3F) << 2, b = (p & 0x1F) << 3;
    int gray8 = (r * 77 + g * 150 + b * 29) >> 8;   // luma
    epaper.drawPixel(imageX + x, y, gray8 >> 4);
  }
  return 1;
}

static bool showImage(const char *path) {
  if (!SD.exists(path)) {
    showStatus("PNG not found", path);
    Serial.printf("Image not found: %s\n", path);
    return false;
  }
  png = (PNG *)malloc(sizeof(PNG));
  if (!png) return false;
  int rc = png->open(path, pngOpen, pngClose, pngRead, pngSeek, pngDraw);
  if (rc != PNG_SUCCESS) {
    free(png);
    showStatus("PNG load failed", path);
    Serial.printf("png.open failed: %d\n", rc);
    return false;
  }
  Serial.printf("%s: %d x %d, %d bpp\n", path, png->getWidth(), png->getHeight(), png->getBpp());
  imageX = (epaper.width() - png->getWidth()) / 2;
  imageY = (epaper.height() - png->getHeight()) / 2;
  epaper.fillScreen(0xF);
  rc = png->decode(NULL, 0);
  png->close();
  free(png);
  if (rc != PNG_SUCCESS) {
    showStatus("PNG decode failed", path);
    Serial.printf("png.decode failed: %d\n", rc);
    return false;
  }
  epaper.fullUpdate(CLEAR_FAST, false);
  Serial.printf("Displayed: %s\n", path);
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);   // USB-CDC: do not block forever without a monitor
  Serial.println("LilyGo T5 microSD test");

  epaper.initT5();
  epaper.setMode(BB_MODE_4BPP);
  epaper.fillScreen(0xF);
  epaper.fullUpdate(CLEAR_SLOW, false);

  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI, 10000000)) {
    showStatus("microSD mount failed", "Check card / FAT32");
    Serial.println("microSD initialization failed");
    return;
  }
  if (SD.cardType() == CARD_NONE) {
    showStatus("No microSD card", "Insert a FAT32 card");
    Serial.println("No microSD card detected");
    return;
  }
  Serial.printf("Card size: %llu MB\n", SD.cardSize() / (1024ULL * 1024ULL));

  File logFile = SD.open("/LilyGo_Log.txt", FILE_WRITE);
  if (logFile) {
    logFile.println("LilyGo T5 microSD test");
    logFile.close();
    Serial.println("Wrote /LilyGo_Log.txt");
  } else {
    Serial.println("Failed to open /LilyGo_Log.txt for writing");
  }
  File readFile = SD.open("/LilyGo_Log.txt", FILE_READ);
  if (readFile) {
    Serial.println("Contents of /LilyGo_Log.txt:");
    while (readFile.available()) Serial.write(readFile.read());
    readFile.close();
  }

  showImage(imagePath);
}

void loop() {
  delay(1000);
}
