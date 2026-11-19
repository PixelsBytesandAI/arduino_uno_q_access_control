# Arduino UNO Q notes: MFRC522, serial ports and App Lab

What we learned getting the access-control sketch running on the Arduino UNO Q. Everything marked **confirmed** was seen working on our board; anything else says so.

Setup used: Arduino UNO Q, Arduino App Lab 0.6.0 (builds with platform `arduino:zephyr` 0.90.0), Arduino IDE 2 on Windows with `arduino:zephyr` 1.0.0, SunFounder RFID-RC522, Adafruit R503 fingerprint sensor, SG90 servo.

## Quick reference

| Problem | Cause | Fix |
|---|---|---|
| MFRC522 does not appear in App Lab's library search | The library's `architectures` list does not include `zephyr`, so App Lab hides it | Bundle the library into the sketch as one header (see below) |
| Nothing in the Arduino IDE Serial Monitor | `Monitor.print` goes to the Linux side, not the USB COM port | Read output in App Lab's Serial Monitor tab |
| Console goes silent once the sketch starts a serial device | The device was started on `Serial`, which is the Bridge link | Use `Serial1` for D0/D1 |
| App Lab builds an empty sketch | Code was pasted into the app's readme, not `sketch/sketch.ino` | Put code in `sketch/sketch.ino` and save |
| R503 search returns `0xFC` | The R503 rejects the high-speed search command | Use `fingerSearch()`, not `fingerFastSearch()` |

## MFRC522 library

### The problem

MFRC522 v1.4.12 (by GithubCommunity / miguelbalboa) lists `avr, megaavr, STM32F1, teensy, esp8266, esp32, samd, atmelsam, rp2040` in `library.properties`.

- **Arduino IDE 2:** shows a warning ("may be incompatible with your current board which runs on zephyr architecture(s)") but compiles and uploads.
- **App Lab:** the library does not appear in the "Add sketch library" search at all. Only unrelated libraries such as MFRC522_I2C and MFRC522_NTAG424DNA are listed.

### The workaround (confirmed)

The library's four source files (`require_cpp11.h`, `deprecated.h`, `MFRC522.h`, `MFRC522.cpp`) are merged into one header, `MFRC522_bundled.h`, which lives in the sketch folder:

```cpp
#include <SPI.h>
#include "MFRC522_bundled.h"   // instead of #include <MFRC522.h>
```

In App Lab, add it as a second file in the app's `sketch` folder, next to `sketch.ino`. The name must match exactly: a missing underscore gave `fatal error: MFRC522_bundled.h: No such file or directory`.

If adding a file is awkward, the header's contents can be pasted straight into `sketch.ino` in place of the `#include` line. Remove the `#pragma once` line if you do.

The library is public domain (Unlicense), so bundling it is allowed. Include the header from only one file, since it contains the function definitions as well as the declarations.

### What works (confirmed)

- `SPI.begin()` and `PCD_Init()` with NSS on D10 and RST on D9. SPI uses D11 (MOSI), D12 (MISO) and D13 (SCK).
- `PCD_ReadRegister(MFRC522::VersionReg)` returns `0x92`, a genuine MFRC522 v2.0.
- `PICC_IsNewCardPresent()`, `PICC_ReadCardSerial()` and `PICC_HaltA()`: both test cards returned 4-byte UIDs reliably.

### Not tested

- Reading or writing card memory (`MIFARE_Read`, `MIFARE_Write`, authentication).
- The library's debug helpers such as `PCD_DumpVersionToSerial()` and `PICC_DumpToSerial()`. They print to `Serial`, which on the UNO Q is the Bridge link (see below), so avoid them. Print with `Monitor` yourself instead.

## Serial ports on the UNO Q

### `Serial1` is D0/D1 (confirmed)

The R503 on D0 (RX) and D1 (TX) only worked as `Serial1`:

```cpp
Adafruit_Fingerprint finger(&Serial1);
```

Sources disagree on this: some forum posts say `Serial` is D0/D1. On our board, starting the sensor with `Serial` at 57600 baud stopped all console output. Moving it to `Serial1` brought the console back and the sensor was found straight away.

### `Serial` belongs to the Bridge

Our reading, from a source-code write-up and the behaviour above, is that `Serial` is the internal UART the Bridge and `Monitor` use to reach the Linux side. Do not call `Serial.begin()` or pass `&Serial` to a library.

## Console output (`Monitor`)

- **Where it shows (confirmed):** `Monitor.print` output appears only in App Lab's **Serial Monitor** tab. The Arduino IDE's Serial Monitor on the board's COM port stays blank. The Arduino forum suggests `arduino-app-cli monitor`, run in a shell on the board, as a command-line alternative; we did not try it.
- **Startup (confirmed):** this sequence works:
  ```cpp
  Bridge.begin();
  Monitor.begin();
  delay(1000);   // gives the console time to attach before the first messages
  ```
  We added `Bridge.begin()` and the delay together while fixing the `Serial` problem, so we do not know whether either is strictly needed.
- **Input (confirmed):** `Monitor.available()` and `Monitor.read()` receive text typed into App Lab's Serial Monitor message box. The sketch uses this for the `list`, `delete <slot>` and `clear` fingerprint commands.
- **Baud setting:** the 9600 baud selector in App Lab's Serial Monitor does not matter for `Monitor`.

## Arduino App Lab

- **Where code goes:** code goes in `sketch/sketch.ino`. An App Lab app also has a readme and a `python/main.py`. Code pasted into the readme is not compiled, and App Lab silently builds its empty template instead. The clue was in the build log: the "Used library" lines listed only Arduino_RouterBridge and its helpers, and the sketch was 64 KB instead of about 100 KB.
- **Libraries are per app:** libraries installed in the Arduino IDE are not available in App Lab. Add each one to the app from the Libraries panel; they are recorded in `sketch/sketch.yaml`.
- **Arduino_RouterBridge:** it is part of the platform. App Lab removed it from our app's library list on its own ("Applied app migration for platform arduino:zephyr >0.54.1").
- **Platform versions differ:** App Lab compiled with `arduino:zephyr` 0.90.0, while the Arduino IDE on the same PC had 1.0.0. Something that builds in one may not build in the other.
- **A Python container starts with every app.** A sketch-only app can ignore it.

## Other libraries

### Adafruit Fingerprint Sensor Library 2.1.4

- **Visible in App Lab:** it appears in App Lab's search because it lists `architectures=*`.
- **Search command (confirmed):** `fingerFastSearch()` returns `0xFC` from the R503 on every attempt. `fingerSearch()` matches correctly, for example "slot 1, confidence 133".
- **Capacity:** call `getParameters()` after `verifyPassword()` to read the sensor's real `capacity`. It defaults to 64 otherwise.
- **Where prints are stored:** enrolled prints live in the R503, not the UNO Q. They survive re-uploads and need `deleteModel()` or `emptyDatabase()` to remove.
- **Partial first image:** the first image is often taken while the finger is still landing. Retrying `getImage()` / `image2Tz()` / `fingerSearch()` two or three times, 300 ms apart, avoids false rejections.

### Servo 1.3.0

- **Support (confirmed):** it lists `zephyr` and has a UNO Q implementation that times pulses with a hardware counter (`counter_servo` in the board's devicetree). It installed and compiled in App Lab.
- **Servo noise:** it did not reduce our SG90's noise compared with pulses generated in the sketch, so the noise was not a timing problem.
- **Detaching:** after `detach()`, set the pin LOW yourself in case it stopped mid-pulse.

## Arduino IDE

- **Sketch folder:** a sketch must sit in a folder with the same name as its `.ino` file (`AccessControl/AccessControl.ino`), or the IDE refuses to open it.
- **Extra `.ino` files:** do not keep a second `.ino` in the same folder. The IDE compiles every `.ino` in a folder as one sketch, which gives duplicate definitions.
