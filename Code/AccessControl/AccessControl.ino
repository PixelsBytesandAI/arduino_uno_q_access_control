/*
  Access Control Box - Arduino UNO Q

  Unlocks the mini door when a known fingerprint (R503) or a known RFID
  card (RC522) is presented. See WIRING.md for the connections.

  Upload with Arduino App Lab and read messages in its Serial Monitor tab.
  Libraries to add to the App Lab app:
    - Adafruit Fingerprint Sensor Library
    - MFRC522: bundled as MFRC522_bundled.h in this folder (App Lab hides the library)
    - Servo                             (USE_SERVO_LIB is 1; add it to the App Lab app)

  Arduino_RouterBridge is built into the UNO Q platform.

  STATUS: runs on a UNO Q. Cards, fingerprint enrollment and fingerprint
  access are tested.

  First-time setup:
    1. Upload, open the console, and tap each card or fob. Unknown cards
       print their ID.
    2. Paste one ID into ADMIN_CARD and the others into ALLOWED_CARDS,
       then upload again.
    3. To enroll a fingerprint: tap the admin card, then place the same
       finger on the sensor twice when the ring turns purple.
    4. Type list, delete <slot> or clear into the Serial Monitor to manage
       stored fingerprints.
*/

#include <Arduino_RouterBridge.h>
#include <SPI.h>
#include "MFRC522_bundled.h"   // MFRC522 v1.4.12, bundled because App Lab hides the library
#include <Adafruit_Fingerprint.h>

// 1 = use the Servo library, which times the pulses with a hardware timer
//     so the readers cannot delay them (steady, quiet servo).
// 0 = generate the pulses in this sketch. No library needed, but the
//     timing is uneven while the readers are busy and the servo buzzes.
#define USE_SERVO_LIB 1
#if USE_SERVO_LIB
#include <Servo.h>
#endif

// ---------------- Pins ----------------
const int PIN_SERVO  = 6;    // D6
const int PIN_RC_RST = 9;    // D9
const int PIN_RC_SS  = 10;   // D10, RC522 "SDA" / "NSS"
// RC522 also uses D11 (MOSI), D12 (MISO), D13 (SCK).
// R503 uses the hardware UART: D0 (RX) and D1 (TX).

// On the UNO Q, D0/D1 is Serial1. Serial is the internal link the Bridge
// and Monitor use to reach the Linux side, so starting the sensor on Serial
// silences the console.
#define FINGER_SERIAL Serial1

// ---------------- Cards ----------------
// IDs are upper-case hex with no spaces, exactly as printed in the console.
// Replace these with your own IDs (see First-time setup above).
const char* ADMIN_CARD = "";              // tap to enroll a fingerprint (does not open the door), e.g. "A1B2C3D4"
const char* ALLOWED_CARDS[] = {
  "",                                     // cards that open the door, e.g. "11223344"
};
const int ALLOWED_COUNT = sizeof(ALLOWED_CARDS) / sizeof(ALLOWED_CARDS[0]);

// ---------------- Door ----------------
// If the door opens the wrong way, swap these two angles. Test the servo
// with the frame OFF: the frame's stop lip blocks the wrong direction.
const int CLOSED_ANGLE = 110;
const int OPEN_ANGLE   = 0;
const int STEP_DEG     = 3;               // degrees moved per 20 ms
const bool HOLD_CLOSED = false;           // true keeps driving the servo when shut (it buzzes)
const unsigned long OPEN_MS = 4000;       // how long the door stays open

// ---------------- Timing ----------------
const unsigned long POLL_MS  = 150;       // how often the readers are checked
const unsigned long SERVO_MS = 20;        // servo refresh period

MFRC522 rfid(PIN_RC_SS, PIN_RC_RST);
Adafruit_Fingerprint finger(&FINGER_SERIAL);
#if USE_SERVO_LIB
Servo doorServo;
#endif

bool fingerReady = false;
bool doorOpen = false;
int servoAngle = CLOSED_ANGLE;
int servoTarget = CLOSED_ANGLE;
unsigned long openedAt = 0;
unsigned long lastPoll = 0;
unsigned long lastServo = 0;
unsigned long settledAt = 0;

// ---------------- Servo ----------------
void writeServo(int angle) {
#if USE_SERVO_LIB
  doorServo.write(angle);
#else
  // 0 deg = 500 us, 180 deg = 2400 us (typical SG90 range)
  unsigned int us = 500 + (unsigned long)angle * 1900 / 180;
  digitalWrite(PIN_SERVO, HIGH);
  delayMicroseconds(us);
  digitalWrite(PIN_SERVO, LOW);
#endif
}

void serviceServo() {
  unsigned long now = millis();
  if (now - lastServo < SERVO_MS) return;
  lastServo = now;

  if (servoAngle != servoTarget) {
    int diff = servoTarget - servoAngle;
    if (abs(diff) <= STEP_DEG) servoAngle = servoTarget;
    else servoAngle += (diff > 0 ? STEP_DEG : -STEP_DEG);
    settledAt = now;
  }
  // Keep pulsing while moving, while open, for 1 s after closing,
  // and always if HOLD_CLOSED is set.
  bool active = doorOpen || HOLD_CLOSED || (now - settledAt < 1000);
#if USE_SERVO_LIB
  // The library pulses on its own; detach when idle so the servo goes quiet.
  if (active) {
    if (!doorServo.attached()) doorServo.attach(PIN_SERVO, 500, 2400);
    writeServo(servoAngle);
  } else if (doorServo.attached()) {
    doorServo.detach();
    digitalWrite(PIN_SERVO, LOW);   // in case it detached mid-pulse
  }
#else
  if (active) writeServo(servoAngle);
#endif
}

// Run the servo for a while without checking the readers.
void waitWithServo(unsigned long ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    serviceServo();
    delay(2);
  }
}

// ---------------- Ring light ----------------
void ring(uint8_t mode, uint8_t colour, uint8_t speed = 0, uint8_t count = 0) {
  if (fingerReady) finger.LEDcontrol(mode, speed, colour, count);
}
void ringIdle()    { ring(FINGERPRINT_LED_BREATHING, FINGERPRINT_LED_BLUE, 150); }
void ringGranted() { ring(FINGERPRINT_LED_ON, FINGERPRINT_LED_BLUE); }
void ringDenied()  { ring(FINGERPRINT_LED_FLASHING, FINGERPRINT_LED_RED, 25, 3); }
void ringEnroll()  { ring(FINGERPRINT_LED_ON, FINGERPRINT_LED_PURPLE); }

// ---------------- Access ----------------
void grant(const char* how) {
  Monitor.print("Access granted: ");
  Monitor.println(how);
  ringGranted();
  servoTarget = OPEN_ANGLE;
  doorOpen = true;
  openedAt = millis();
}

void closeDoor() {
  servoTarget = CLOSED_ANGLE;
  doorOpen = false;
  ringIdle();
  Monitor.println("Door closed");
}

void deny(const char* how) {
  Monitor.print("Access denied: ");
  Monitor.println(how);
  ringDenied();
  waitWithServo(1200);
  ringIdle();
}

// ---------------- Fingerprint ----------------
// A slot is in use if its template loads. Returns 0 if every slot is full.
uint16_t firstFreeSlot() {
  for (uint16_t id = 1; id < finger.capacity; id++) {
    if (finger.loadModel(id) != FINGERPRINT_OK) return id;
  }
  return 0;
}

bool captureTo(uint8_t slot, unsigned long timeoutMs) {
  unsigned long t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (finger.getImage() == FINGERPRINT_OK) {
      return finger.image2Tz(slot) == FINGERPRINT_OK;
    }
    delay(50);
  }
  return false;
}

void enrollFinger() {
  if (!fingerReady) {
    Monitor.println("Enroll: fingerprint sensor not found");
    return;
  }
  uint16_t id = firstFreeSlot();
  if (id == 0) {
    Monitor.println("Enroll: sensor is full");
    return;
  }
  Monitor.print("Enroll: place finger for slot ");
  Monitor.println(id);
  ringEnroll();

  bool ok = captureTo(1, 10000);
  if (ok) {
    Monitor.println("Enroll: lift finger");
    unsigned long t0 = millis();
    while (finger.getImage() != FINGERPRINT_NOFINGER && millis() - t0 < 5000) delay(50);
    Monitor.println("Enroll: place the same finger again");
    ok = captureTo(2, 10000);
  }
  if (ok) ok = finger.createModel() == FINGERPRINT_OK;
  if (ok) ok = finger.storeModel(id) == FINGERPRINT_OK;

  if (ok) {
    Monitor.println("Enroll: saved");
    ring(FINGERPRINT_LED_FLASHING, FINGERPRINT_LED_BLUE, 25, 3);
  } else {
    Monitor.println("Enroll: failed, tap the admin card to try again");
    ringDenied();
  }
  delay(1500);
  ringIdle();
}

// The first image is often taken while the finger is still landing, so a
// failed match is retried a few times before the finger is refused.
void checkFinger() {
  if (!fingerReady) return;
  if (finger.getImage() != FINGERPRINT_OK) return;      // no finger present

  uint8_t result = 0;
  for (int attempt = 1; attempt <= 3; attempt++) {
    if (attempt > 1) {
      delay(300);
      if (finger.getImage() != FINGERPRINT_OK) continue;
    }
    result = finger.image2Tz();
    if (result != FINGERPRINT_OK) continue;             // unreadable image
    result = finger.fingerSearch();      // fingerFastSearch returns 0xFC on this R503
    if (result == FINGERPRINT_OK) {
      Monitor.print("Fingerprint slot ");
      Monitor.print(finger.fingerID);
      Monitor.print(", confidence ");
      Monitor.println(finger.confidence);
      grant("fingerprint");
      return;
    }
  }
  Monitor.print("Fingerprint not matched (sensor code 0x");
  Monitor.print(result, HEX);
  Monitor.println(")");
  deny("unknown fingerprint");
}

// ---------------- Console commands ----------------
// Type into App Lab's Serial Monitor:  list   delete <slot>   clear
char cmdBuf[24];
int cmdLen = 0;

void runCommand(char* cmd) {
  if (!fingerReady) {
    Monitor.println("Fingerprint sensor not found");
    return;
  }
  if (strcmp(cmd, "list") == 0) {
    finger.getTemplateCount();
    Monitor.print("Stored prints: ");
    Monitor.println(finger.templateCount);
    for (uint16_t id = 1; id < finger.capacity; id++) {
      if (finger.loadModel(id) == FINGERPRINT_OK) {
        Monitor.print("  slot ");
        Monitor.println(id);
      }
    }
  } else if (strncmp(cmd, "delete ", 7) == 0) {
    int id = atoi(cmd + 7);
    if (id < 1) {
      Monitor.println("Usage: delete <slot>");
    } else if (finger.deleteModel(id) == FINGERPRINT_OK) {
      Monitor.print("Deleted slot ");
      Monitor.println(id);
    } else {
      Monitor.print("Could not delete slot ");
      Monitor.println(id);
    }
  } else if (strcmp(cmd, "clear") == 0) {
    if (finger.emptyDatabase() == FINGERPRINT_OK) Monitor.println("All fingerprints deleted");
    else Monitor.println("Could not clear the fingerprint database");
  } else if (cmd[0] != '\0') {
    Monitor.println("Commands: list, delete <slot>, clear");
  }
}

void checkConsole() {
  while (Monitor.available()) {
    char c = Monitor.read();
    if (c == '\n' || c == '\r') {
      cmdBuf[cmdLen] = '\0';
      runCommand(cmdBuf);
      cmdLen = 0;
    } else if (cmdLen < (int)sizeof(cmdBuf) - 1) {
      cmdBuf[cmdLen++] = c;
    }
  }
}

// ---------------- RFID ----------------
void checkCard() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  char id[2 * 10 + 1];
  int n = 0;
  for (byte i = 0; i < rfid.uid.size && i < 10; i++) {
    n += snprintf(id + n, sizeof(id) - n, "%02X", rfid.uid.uidByte[i]);
  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  if (ADMIN_CARD[0] != '\0' && strcmp(id, ADMIN_CARD) == 0) {
    enrollFinger();
    return;
  }
  for (int i = 0; i < ALLOWED_COUNT; i++) {
    if (strcmp(id, ALLOWED_CARDS[i]) == 0) {
      grant("card");
      return;
    }
  }
  Monitor.print("Unknown card ID: ");
  Monitor.println(id);
  deny("unknown card");
}

// ---------------- Setup / loop ----------------
void setup() {
  Bridge.begin();
  Monitor.begin();
  delay(1000);   // give the console time to attach before the first messages

#if USE_SERVO_LIB
  doorServo.attach(PIN_SERVO, 500, 2400);   // same 500-2400 us range as the manual pulses
#else
  pinMode(PIN_SERVO, OUTPUT);
  digitalWrite(PIN_SERVO, LOW);
#endif

  SPI.begin();
  rfid.PCD_Init();

  finger.begin(57600);
  delay(100);
  fingerReady = finger.verifyPassword();
  if (fingerReady) {
    finger.getParameters();    // reads finger.capacity
    finger.getTemplateCount();
    Monitor.print("Fingerprint sensor ready, stored prints: ");
    Monitor.println(finger.templateCount);
    Monitor.println("Console commands: list, delete <slot>, clear");
  } else {
    Monitor.println("Fingerprint sensor not found (try swapping the D0 and D1 wires)");
  }

  ringIdle();
  settledAt = millis();
  Monitor.println("Access control ready");
}

void loop() {
  serviceServo();
  checkConsole();

  unsigned long now = millis();
  if (doorOpen && now - openedAt >= OPEN_MS) closeDoor();

  if (!doorOpen && servoAngle == servoTarget && now - lastPoll >= POLL_MS) {
    lastPoll = now;
    checkCard();
    if (!doorOpen) checkFinger();
  }
}
