# How the Access Control Project Works

This project is a working model of a door access system. You prove who you are with a fingerprint or an RFID card, an Arduino UNO Q decides whether to let you in, and a small printed door swings open if the answer is yes.

**Status:** the access control sketch runs on a real UNO Q. Card access, fingerprint enrollment and fingerprint access all work. The enclosure and mini door are designed and partly printed.

## The idea in one picture

```
  Finger on sensor ──┐
                     ├──>  UNO Q checks  ──┬── match ────>  door opens, ring turns blue
  Card on reader ────┘     who it is       │
                                           └── no match ─>  door stays shut, ring flashes red
```

Every access control system, from an office badge reader to a phone's lock screen, has the same three jobs:

1. **Identify:** read something that belongs to one person.
2. **Decide:** compare it against a list of who is allowed.
3. **Act:** unlock, or refuse.

This project does each job with a separate part, so you can see where one ends and the next begins.

## The parts and their jobs

| Part | Job | What it does |
|---|---|---|
| R503 fingerprint sensor | Identify | Reads a fingerprint and matches it against prints stored inside the sensor |
| RC522 RFID reader | Identify | Reads the ID number of a card or key fob held near it |
| Arduino UNO Q | Decide | Runs the sketch that checks the result and controls everything else |
| SG90 servo | Act | Swings the mini door open and closed |
| R503 light ring | Feedback | Shows the state of the system in colour |

The access box holds the UNO Q and both readers. The mini door stands beside it and connects by the servo's three-wire cable.

## Step by step

### 1. Waiting

The sketch checks both readers about seven times a second. The fingerprint sensor's ring breathes blue to show the system is ready.

### 2. Identifying by fingerprint

When a finger touches the sensor:

1. The sensor takes an image of the fingerprint.
2. It converts the image into a compact description of the print's features, called a template.
3. It searches its own memory for a stored template that matches.
4. It reports the result to the UNO Q: either the slot number of the matching print and a confidence score, or "not found".

The matching happens inside the sensor. The UNO Q never sees the fingerprint itself, only the answer.

### 3. Identifying by card

When a card or fob is held near the "tap here" mark:

1. The reader's antenna powers the chip in the card by radio. The card has no battery.
2. The card replies with its ID number.
3. The UNO Q turns that number into text, such as `A1B2C3D4`, and compares it against the list of allowed IDs written into the sketch.

Unlike the fingerprint sensor, the reader only reads. The UNO Q does the matching.

### 4. Deciding

The rule is simple: a known fingerprint **or** a known card is enough. Either one opens the door on its own.

### 5. Acting

**If access is granted:**

- The ring turns solid blue.
- The servo turns about 110° and the door swings open.
- After 4 seconds the door closes and the ring returns to breathing blue.

**If access is refused:**

- The ring flashes red three times.
- The door does not move.
- For an unknown card, its ID is printed to the console so you can add it to the allowed list if you want to.

## Adding people

**Cards** are added by editing the sketch. Tap a new card, copy the ID that appears in the console into the `ALLOWED_CARDS` list, and upload again.

**Fingerprints** are added without a computer, using one card set aside as the admin card:

1. Tap the admin card. The ring turns purple.
2. Place a finger on the sensor, lift it, and place it again.
3. The sensor combines the two readings into one template and stores it in its next free slot.

Two readings are used because a single press rarely captures the whole print cleanly.

## How the door moves

The servo sits in the base of the mini door with its shaft pointing up at the hinge corner. The door presses onto the servo's arm, so when the servo turns, the door turns with it. A pin through the top of the frame holds the other end of the hinge line.

A servo is told where to go by a stream of short electrical pulses. The length of each pulse sets the angle: a short pulse for one end of its travel, a long pulse for the other. The sketch moves the door by changing the pulse length a little at a time.

## Where the UNO Q fits

The UNO Q has two processors on one board:

- **A microcontroller**, which runs Arduino sketches and handles pins, timing and sensors. All of the access control logic in this project runs here.
- **A Linux computer**, which can run Python, cameras and AI models.

This project uses only the microcontroller so far. The Linux side is free for later additions, such as logging who entered and when, a web page showing the door's state, or a camera check alongside the fingerprint.

## What this demo does not do

This is a teaching model, and several things separate it from a system you could trust with a real door:

- **Card IDs can be copied.** The sketch checks only the card's ID number, which these cards send to any reader that asks. A cheap cloning tool can copy it onto a blank card. Secure systems use cards that prove their identity with encryption.
- **One factor is enough.** A fingerprint or a card opens the door. Requiring both would be stronger.
- **The allowed list lives in the sketch.** Changing who has card access means editing code and uploading again.
- **Nothing is logged.** The system does not record who opened the door or when.
- **The door is held by a hobby servo.** It resists a finger, not a determined push.
- **It fails shut with no backup.** If power is lost, nothing opens the door.

## Related files

| File | What it covers |
|---|---|
| `README.md` | The printed parts, hardware list, printing and assembly |
| `WIRING.md` | Pin-by-pin connections |
| `Code/AccessControl/` | The access control sketch and its bundled MFRC522 library |
| `UNO_Q_NOTES.md` | What we learned about the UNO Q, App Lab and the MFRC522 library |
| `FINGERPRINT_STORAGE.md` | How fingerprints are enrolled, stored and deleted |
