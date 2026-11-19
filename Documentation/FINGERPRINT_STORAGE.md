# How fingerprints are stored

This explains where the access-control system keeps fingerprints, what is actually saved, and how enrolling, matching and deleting work. It describes the Adafruit R503 sensor driven by `Code/AccessControl/AccessControl.ino`.

## The short version

- **Where:** fingerprints are stored **inside the R503 sensor**, in its own flash memory. The UNO Q never holds them.
- **What:** the sensor saves a **template**, a set of features extracted from the print, not a picture of the finger.
- **How they're numbered:** each template sits in a numbered **slot**. The sketch only ever knows slot numbers, never whose finger is in a slot.
- **How long they last:** templates survive power cuts and re-uploading the sketch. They stay until they are deleted.

## Where things live

| Data | Stored in | Survives power off | Survives re-upload |
|---|---|---|---|
| Fingerprint templates | R503 flash memory, one per slot | Yes | Yes |
| Card IDs (`ADMIN_CARD`, `ALLOWED_CARDS`) | The sketch's code on the UNO Q | Yes | Only if they are still in the code you upload |
| Who owns each slot | Nowhere, so keep your own list | n/a | n/a |

Fingerprints and cards are managed differently. To add a card, you edit the sketch and upload it again. To add a fingerprint, you enroll it at the sensor and the code doesn't change.

## Inside the sensor

The R503 has two kinds of storage:

- **Two working buffers (buffer 1 and buffer 2).** These are temporary. Each one holds the features extracted from one image of a finger. They are cleared when the sensor loses power.
- **The template library.** This is permanent flash memory divided into numbered slots. Each slot holds one finished template.
  - The sketch reads the number of slots from the sensor at startup with `getParameters()`.
  - Adafruit lists the R503 at 200 fingerprints. The sketch doesn't print the exact figure.
  - The sketch uses slots from 1 upwards.

All matching happens inside the sensor. The UNO Q sends commands over the serial link (D0/D1) and gets back short answers, such as "matched slot 1 with confidence 133" or "no match".

## Enrolling a finger

Tap the admin card (the one set as `ADMIN_CARD` in the sketch) and the ring turns purple. Then:

1. **Pick a slot.** The sketch tries to load each slot from 1 upwards and uses the first one that is empty (`firstFreeSlot()`). Slots freed by a delete get reused, and existing prints are never overwritten.
2. **First image.** You place your finger. The sensor takes an image and extracts its features into **buffer 1** (`getImage()`, `image2Tz(1)`).
3. **Lift and replace.** You lift your finger and place it again. The second image goes into **buffer 2** (`image2Tz(2)`).
4. **Combine.** The sensor merges the two buffers into one template (`createModel()`). This fails if the two images look like different fingers.
5. **Save.** The template is written into the chosen slot (`storeModel(slot)`). The console prints `Enroll: saved`, and the ring flashes blue.

The images themselves are discarded. Only the combined template is kept.

If no finger arrives within 10 seconds at either step, or the two images don't match, the console prints `Enroll: failed` and nothing is saved.

## Recognising a finger

The sketch checks the sensor about every 150 ms. When a finger is present:

1. The sensor takes an image and extracts its features into buffer 1 (`getImage()`, `image2Tz()`).
2. It compares buffer 1 against every stored template (`fingerSearch()`).
3. It answers with the best matching slot and a **confidence** score. Higher is a closer match. A good match on this build scored 133.

The first image is often taken while the finger is still landing, so the sketch tries up to three times, 300 ms apart, before refusing the finger.

The sketch uses `fingerSearch()`, not the library's faster `fingerFastSearch()`. The R503 rejects the fast version with error code `0xFC`.

## Managing stored prints

Open App Lab's Serial Monitor, type a command into the message box and press Enter:

| Command | What it does |
|---|---|
| `list` | Shows how many prints are stored and which slots are in use |
| `delete 3` | Deletes the template in slot 3 |
| `clear` | Deletes every stored template. This can't be undone. |

To replace a finger, delete its slot, then enroll again. The new enrollment reuses the freed slot.

The sketch keeps no record of whose finger is in which slot. Note each slot number when you enroll someone, for example "slot 1: Alex, right index".

## Security notes

- **No pictures are stored.** A template is a set of measurements and points, not an image of the finger. It's still personal data, so treat the sensor as holding something private.
- **Physical access to the sensor:** the R503's protocol includes a command to download templates (the library's `getModel()`). The sketch never uses it, but anyone who unplugs the sensor and connects their own board could read the templates out. The sensor's password is the factory default.
- **Wipe before giving the sensor away:** run `clear` before reusing the sensor in another project or giving it away.
- **Cards are weaker than fingerprints:** card IDs are easy to read and copy. The RC522 reads only the card's ID number, which anyone with a reader can copy. That's fine for a demo; it isn't strong security.

## Moving to another sensor or board

- **New UNO Q, same R503:** the fingerprints come with the sensor. Upload the sketch and they work straight away.
- **New R503:** it starts empty. Everyone has to enroll again. The sketch doesn't copy templates between sensors.
