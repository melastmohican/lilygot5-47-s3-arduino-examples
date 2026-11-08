# LilyGoT5ePaper — agent notes

Arduino sketches for the LilyGo T5 4.7" ESP32-S3 e-paper board (ED047TC1,
960x540). No build system in the repo: sketches are compiled and flashed from
Arduino IDE or `arduino-cli`. Libraries live outside the repo in
`~/Src/Arduino/libraries/`.

Git: this directory is untracked inside the parent `~/Src` repo. Do not
`git add` from the parent root. Do not commit unless asked.

## Layout

- `LilyGo/<sketch>/` — vendor examples built on LilyGo-EPD47 (`epd_driver.h`,
  `utilities.h`, `firasans.h`). Keep close to upstream; don't refactor.
  `demo` and `wifi_sync` deviate (secret.h, no `esp_adc_cal`); listed in the
  README. Add any new deviation there.
- `OWM-Weather/` — port of David Bird's OpenWeatherMap sketch (LilyGo-EPD47
  based). No license upstream: keep its copyright header, don't add a license.
  Deviations are listed in the README.
- `OpenMeteo-Weather/` — port of the reTerminal E1002 WeatherStation (Open-Meteo, no API
  key) on LilyGo-EPD47. Own code plus Lucide icons (ISC) in `icons/`; changes are listed in
  the README.
- `News/`, `Quotables/` — ports of the reTerminal E1002 sketches on LilyGo-EPD47. Each has its
  own copy of the helper code and the Open Sans fonts; keep them in sync by hand. Changes are
  listed in the README.
- `FastEPD/<sketch>/` — our sketches built on bitbank2/FastEPD.
- `FastEPD/lilygo_t5_47_s3.h` — shared header: `T5EPD` class + `initT5()`. Makes stock
  FastEPD 2.2.0 drive this board with no library patch. Every FastEPD sketch includes it
  as `"../lilygo_t5_47_s3.h"`.
- `FastEPD/lilygo-t5-47-s3.patch` — optional; adds `BB_PANEL_LILYGO_T5_47_S3` to FastEPD
  2.2.0. No sketch needs it any more.
- `FastEPD/README.md` — patch details, pin table, gotchas. Read before editing
  the patch or any FastEPD sketch.

## Rules

- **Hardware is the V2.4 board** (Screen-4.7-S3, 2024-12-03). LilyGo's product page and wiki show
  other revisions; take pinouts from the V2.4 schematic linked in the README, not the wiki.
- **Never commit or print `secret.h`** (WiFi credentials, gitignored). Edit
  `secret.h.example` for shape changes.
- **Do not mix the two libraries** in one sketch. Both drive the same parallel
  bus and define conflicting symbols.
- FastEPD sketches must declare `T5EPD epaper;` and call `epaper.initT5()`, not
  `BB_PANEL_V7_RAW` / `setPanelSize` (EPDiy defaults in upstream examples).
- `partialUpdate()` fails silently (returns `BBEP_ERROR_BAD_PARAMETER`) unless
  `fullUpdate(CLEAR_NONE, true)` ran once after `clearWhite()`.
- The two `*.patch` files under `LilyGo/` (`arduino_change_idf.patch`) are
  quilt patches for an ESP-IDF variant; `quilt push -a` generates the project.
  Not needed for Arduino builds.
- `lilygo_t5_47_s3.h` depends on FastEPD internals (`bbepIOInit`, `LilyGoEinkPower`,
  `LilyGoIOInit`, `LilyGoRowControl`, protected `_state`). If a FastEPD update breaks the
  build, check those names first. If the optional patch dry-run fails, upstream changed;
  re-add the three pieces by hand (steps in `FastEPD/README.md`).
- Both enum and `panelDefs[]`/`panelProcs[]` tables in FastEPD are index-matched:
  keep entries in the same order.

## Build / flash

Board: `ESP32S3 Dev Module`, PSRAM `OPI`, 16MB flash, USB CDC On Boot enabled.
Example (FQBN must match your installed core):

```sh
arduino-cli compile --fqbn esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,CDCOnBoot=cdc FastEPD/animated_smiles
arduino-cli upload  --fqbn esp32:esp32:esp32s3 -p <port> FastEPD/animated_smiles
```

`FastEPD/antialias_font` includes fonts via a relative path
(`../../../libraries/FastEPD/Fonts/`), so it only compiles from this location.

Hardware verification: use the `capture-display-image` skill to look at the
panel; use `arduino-serial-monitor` for serial output.
