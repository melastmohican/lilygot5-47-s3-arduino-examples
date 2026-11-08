// Quotables: a random quote on the LilyGo T5 4.7" ESP32-S3 (960x540, 16 grays).
//
// Ported from the Seeed reTerminal E1002 "Quotables" sketch (GxEPD2, 800x480 six-colour), which was
// ported from the Soldered Inkplate 10 "Quotables" example.
// Setup: copy secret.h.example to secret.h and set your WiFi credentials.
//
// Arduino Setting
//            Tools ->
//                  Board:"ESP32S3 Dev Module"
//                  USB CDC On Boot:"Enable"
//                  USB DFU On Boot:"Disable"
//                  Flash Size : "16MB(128Mb)"
//                  Flash Mode"QIO 80MHz
//                  Partition Scheme:"16M Flash(3M APP/9.9MB FATFS)"
//                  PSRAM:"OPI PSRAM"
//                  Upload Mode:"UART0/Hardware CDC"
//                  USB Mode:"Hardware CDC and JTAG"
//
// With a USB host connected the sketch stays awake and refreshes every DevRefreshMinutes;
// otherwise it deep-sleeps SleepMinutes between refreshes. DEV_NO_SLEEP forces staying awake.

//#define DEV_NO_SLEEP
#define DevRefreshMinutes 5
#define SleepMinutes      30

#include <Arduino.h>
#include "epd_driver.h"   // https://github.com/Xinyuan-LilyGO/LilyGo-EPD47
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secret.h"       // WIFI_SSID, WIFI_PASSWORD (gitignored)

#include "opensans12b.h"
#include "opensans18b.h"
#include "opensans24b.h"

#define MAX_QUOTE_LEN 160         // longer quotes are skipped (they won't fit)
#define MAX_TRIES 10

String quote;
String author;

// ===== Display helpers (LilyGo-EPD47 framebuffer, gray levels) ==============
#define White      0xFF
#define LightGrey  0xBB
#define Grey       0x88
#define DarkGrey   0x44
#define Black      0x00

uint8_t *framebuffer;
GFXfont currentFont;

static void setFont(GFXfont const &font) { currentFont = font; }
static void clearFrame() { memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2); }

// Draw text with its baseline at y.
static void drawText(int x, int y, const String &text) {
  int32_t cx = x, cy = y;
  write_string(&currentFont, const_cast<char *>(text.c_str()), &cx, &cy, framebuffer);
}

static int textWidth(const String &text) {
  int32_t x = 0, y = 0, x1, y1, w, h;
  get_text_bounds(&currentFont, const_cast<char *>(text.c_str()), &x, &y, &x1, &y1, &w, &h, NULL);
  return w;
}

static void drawTextRight(int xRight, int y, const String &text) { drawText(xRight - textWidth(text), y, text); }
static void drawTextCentered(int xCenter, int y, const String &text) { drawText(xCenter - textWidth(text) / 2, y, text); }

// GFX fonts are ASCII only: replace common UTF-8 punctuation, drop everything else non-ASCII.
static void asciiFold(String &s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length();) {
    uint8_t c = s[i];
    if (c < 0x80) { out += (char)c; i++; continue; }
    if (c == 0xE2 && i + 2 < s.length()) {
      uint8_t b = s[i + 2];
      if (s[i + 1] == (char)0x80) {
        if (b == 0x98 || b == 0x99) out += '\'';
        else if (b == 0x9C || b == 0x9D) out += '"';
        else if (b == 0x93 || b == 0x94) out += '-';
        else if (b == 0xA6) out += "...";
      }
      i += 3;
      continue;
    }
    i += (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : 2;   // skip the rest of a multi-byte sequence
  }
  s = out;
}

// Word-wrap text into a box of width w starting at baseline y with the current font.
// Draws at most maxLines lines ("..." on overflow). Returns the baseline for the next line.
static int drawWrapped(int x, int y, int w, int maxLines, const String &text, int lineHeight) {
  int line = 0, pos = 0, n = text.length();
  String cur;
  while (pos < n && line < maxLines) {
    int sp = text.indexOf(' ', pos);
    String word = (sp < 0) ? text.substring(pos) : text.substring(pos, sp);
    pos = (sp < 0) ? n : sp + 1;
    String trial = cur.length() ? cur + " " + word : word;
    if (textWidth(trial) > w && cur.length()) {
      drawText(x, y + line * lineHeight, cur);
      line++;
      cur = word;
    } else {
      cur = trial;
    }
  }
  if (line < maxLines && cur.length()) {
    drawText(x, y + line * lineHeight, cur);
    line++;
  } else if (pos < n && line >= maxLines) {
    drawText(x + w - 30, y + (maxLines - 1) * lineHeight, "...");
  }
  return y + line * lineHeight;
}

static void pushFrame() {
  epd_poweron();
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff_all();
}

static void drawMessage(const char *msg) {
  clearFrame();
  setFont(OpenSans18B);
  drawText(60, 280, msg);
  pushFrame();
}

static bool connectWiFi(unsigned long timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

// ===== Power =================================================================
static bool StayAwake() {
#ifdef DEV_NO_SLEEP
  return true;
#elif ARDUINO_USB_MODE == 1 && ARDUINO_USB_CDC_ON_BOOT
  return Serial.isConnected();
#else
  return false;
#endif
}

static void BeginSleep() {
  epd_poweroff_all();
  if (StayAwake()) {
    Serial.println("USB connected: staying awake, next refresh in " + String(DevRefreshMinutes) + " min");
    return;
  }
  esp_sleep_enable_timer_wakeup((uint64_t)SleepMinutes * 60ULL * 1000000ULL);
  Serial.println("Deep sleep for " + String(SleepMinutes) + " min");
  Serial.flush();
  esp_deep_sleep_start();
}

// ===== Network ================================================================
static bool fetchQuote() {
  WiFiClientSecure client;
  client.setInsecure();   // public quotes only; no certificate pinning
  HTTPClient http;
  if (!http.begin(client, "https://api.quotable.kurokeita.dev/api/quotes/random")) return false;
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[quote] HTTP %d\n", code);
    http.end();
    return false;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    Serial.printf("[quote] JSON error: %s\n", err.c_str());
    return false;
  }
  const char *content = doc["quote"]["content"];
  const char *name = doc["quote"]["author"]["name"];
  if (!content || !name || strlen(content) > MAX_QUOTE_LEN) return false;
  quote = content;
  author = name;
  asciiFold(quote);
  asciiFold(author);
  return true;
}

// ===== Drawing ================================================================
static void drawQuote() {
  const int W = EPD_WIDTH, H = EPD_HEIGHT;
  clearFrame();

  // Big opening quote mark
  setFont(OpenSans24B);
  drawText(40, 100, "\"");

  setFont(OpenSans24B);
  drawWrapped(80, 130, W - 160, 6, quote, 60);

  setFont(OpenSans12B);
  drawTextRight(W - 60, H - 40, String("- ") + author);
  pushFrame();
}

// ===== Main ===================================================================
static void RunUpdate() {
  if (!connectWiFi(30000)) {
    Serial.println("WiFi failed");
    drawMessage("WiFi connection failed.");
  } else {
    bool ok = false;
    for (int i = 0; i < MAX_TRIES && !(ok = fetchQuote()); i++) delay(1000);
    if (ok) drawQuote();
    else drawMessage("Could not fetch a quote.");
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  BeginSleep();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);   // USB-CDC: do not block forever without a monitor
  Serial.println("Quotables, starting...");
  epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) Serial.println("Memory alloc failed!");
  clearFrame();
  RunUpdate();
}

void loop() {   // only reached when BeginSleep() returned, i.e. USB connected
  delay(DevRefreshMinutes * 60UL * 1000UL);
  RunUpdate();
}
