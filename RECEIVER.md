# Darkwatch Postcards 0.1.3
## Receiver fix

Version 0.1.3 removes a slowly changing microphone baseline before estimating SSTV tone frequency. On a real Sticky recording with about +1,355 counts of DC bias, the previous decoder measured the pilot correctly in its separate mic test but failed to acquire the header. Replaying the unmodified recording through this version recovered all 240 rows. The recovered grayscale frame had a pixel correlation of 0.954 against the source postcard. These are desktop replays of an actual board capture. Live reception on a reflashed Sticky still needs confirmation.

For a quick check after updating, **tap AI/OK** for normal listening and play DW-001 from the start. The microphone test below is available for diagnostics. It saves the original raw audio, including its baseline, so captures remain suitable for troubleshooting.

## Check the sound reaching the Sticky

On a Sticky already running Darkwatch Postcards, keep your existing factory backup. Write the v0.1.3 merged BIN at **0x0** with full-chip erase off. Reset and confirm **ROBOT 36 v0.1.3**.

1. Optionally insert a writable FAT32 microSD card. A test recording takes about 1.92 MB.
2. Hold **AI/OK** for two seconds and release. When **Play Speaker Check from the start** appears, start `postcards/DW-001-Speaker-Check.wav` immediately. Keep the lower microphone opening clear.
3. The test records 60 seconds from the same microphone input used for normal reception. The display holds still during recording. After capture it analyzes the samples and displays a persistent report.
4. A correctly received check track usually shows a longest tone near **1,900 Hz**, around **8 seconds** of pilot and **Header lock YES / 240 rows**. Press DOWN to view a fully recovered postcard. A short OK returns to normal listening.

The frequency estimator inspects 16 ms windows every 64 ms, looks for a concentrated spectral peak between about 375 Hz and 4.4 kHz, and tracks continuous runs. The 1,900 Hz pilot duration uses a ±60 Hz window. Quiet or heavily distorted audio may show no stable tone. The longest tone can be an unrelated sound. The header and row result comes from replaying the captured PCM through the actual C++ receiver on the Sticky.

When a card is available the test saves the original mono 16 kHz PCM WAV in its root as `MIC0001.WAV` or the next free number. The report shows the exact filename. It can still show the on-device result without a card. This explicit test records the room around the Sticky for one minute. Normal listening saves decoded postcards only.

The screen samples status every few seconds. Speak near the bottom microphone opening, look at the mic readings, then play the WAV from the beginning. A normal decode shows **Robot 36 locked**, a rising line count, and then the picture. The serial log on the CH343 port at **115200** reports levels about twice a second.

| During playback | Meaning and next check |
|---|---|
| **Flat mic input**, peak-to-peak 0–2 | The audio stream is returning nearly identical values. If the values stay flat through a few seconds of speech and playback, photograph the level, span and sample-rate readouts. |
| Level responds, **Waiting for Robot 36 header** | The microphone receives changing samples. Move the speaker a few inches from the mic opening, start at moderate volume, and replay from the very beginning. A signal that reaches 0 dBFS or repeatedly spans almost 65535 counts may be clipping. |
| **Robot 36 locked**, then **Sync lost** | The header decoded, but the picture lost line timing. Keep speaker distance fixed. The receiver is already listening for the next header. |
| **Mic read failed** or **Microphone start failed** | Press AI/OK once. If it recurs, report the exact status and serial output. |

The input rate should settle near **16,000 samples per second**. The displayed level is RMS dBFS with DC removed; the peak-to-peak span measures the signal swing in ADC counts. Changes between quiet and playback are more informative than a particular absolute reading.

## Receive your first postcard

DW-001 is a 320 × 240 Robot 36 picture made from the Anubis Darkwatch glyph. Play the entire `postcards/DW-001-Robot36.wav` through a phone or computer speaker. A conventional Robot 36 decoder can receive it directly. The Sticky firmware uses the built-in microphone and shows the finished picture on e-paper after about 37 seconds. Use `postcards/DW-001.png` as the reference.

For the first Sticky test, keep the device on USB power and start at moderate speaker volume, a few inches from the microphone opening. Wait until the listening screen has finished refreshing, then start the WAV from the beginning. Keep the player at normal speed and leave music or voiceovers off. After the picture arrives the microphone stops. Press **AI/OK** to listen again.

## Install on a Seeed reTerminal Sticky

The merged binary is for the **3.97-inch reTerminal Sticky, ESP32-S3**. Flashing replaces the current firmware and partition layout. For a first install, back up the factory unit so you can restore its device-specific Seeedash data. If you already made that backup for v0.1.0, retain it and proceed to step 3.

1. Connect the Sticky using a USB data cable. On Windows, locate its CH343 serial port in Device Manager. Close any serial monitor. The example below uses COM7; substitute your Sticky's port.
2. In PowerShell, install esptool 5 if needed. Read the full factory image to a new local filename. Keep the backup private.

       py -m pip install "esptool>=5,<6"
       py -m esptool --chip esp32s3 --port COM7 read-flash 0x0 0x2000000 sticky-original-32MB.bin
       (Get-Item .\sticky-original-32MB.bin).Length

   Verify the read finished successfully and the file contains **33,554,432 bytes**. Keep that file outside the project directory.
3. From the extracted project folder, write the merged image at **0x0**.

       py -m esptool --chip esp32s3 --port COM7 write-flash 0x0 firmware/Darkwatch-Postcards-Sticky-v0.1.3.bin

   In a graphical ESP flasher, select ESP32-S3, choose the CH343 port and load only the merged BIN at 0x0. Leave full-chip erase disabled. The bundle contains a bootloader, partition table, boot selection data and application. This build uses Seeed's XIAO_ESP32S3 board profile and its 8 MB partition layout on the Sticky's 32 MB flash.
4. Wait for the write verification and reset. If the unit stays in download mode, briefly press Reset.

To restore your original setup, write your full verified backup to 0x0 using the same esptool command with the backup filename.

## Your album

- Insert a compatible FAT32 microSD card (MBR recommended) for persistent storage. The firmware checks the card for each save or browse action.
- Captures are 8-bit grayscale PGM files in `/POSTCARDS/RX00001.PGM`, `RX00002.PGM`, and so on. The e-paper view uses dithering. The file header contains the board clock time if its date appears valid. The firmware does not set or synchronize the clock.
- **UP** and **DOWN** open the latest saved card from the listening screen and then cycle through the album. You can also swipe across a displayed card. Browsing while listening cancels that reception.
- **RAM only** means the image is visible but the save failed or no card was inserted. Starting another capture, opening another saved card or resetting discards an unsaved image.
- Reception begins with the Robot 36 identification header (VIS 8). Start listening before the sender starts. Other SSTV modes, including PD120, need their own decoder. The first version draws the complete frame in one e-paper refresh.
- For a live ham transmission, tune a separate receiver to the announced frequency and modulation, and play its speaker near the Sticky. The same receiver firmware handles both recorded and live Robot 36 audio.
- For diagnostics, open the CH343 serial port at **115200** after flashing. The repeating line reports microphone level, span, measured input rate, completed rows and state (0 waiting, 1 locked, 2 sync lost, 3 unsupported mode). A completed frame prints its album index. A signal lost mid-frame is discarded after repeated missed line syncs; the receiver listens for the next header. For a blank screen, press Reset and watch for `Display init failed`.

## Verification

The v0.1.2 desktop test processed the complete 50.91-second speaker-check file with the same tone estimator and image decoder used by the on-device mic test. It measured 1,901 Hz, an 8.59-second pilot run including adjacent header tones, and recovered 240 rows. The outcome was the same with added noise and reduced level (simulated quiet ~−66 dBFS, pilot ~−54 dBFS). Separate 950 Hz and 3,800 Hz fixtures produced estimates of 951 Hz and 3,799 Hz. Broadband noise produced no stable tone. The diagnostic mode's recording, SD write and end-to-end operation still require a physical Sticky test.


The ESP32-S3 firmware build uses the CH343 UART for logs and leaves native USB CDC off. On this board GPIO19/20 carry PDM audio. The old profile also enabled native USB CDC at boot. The decoder now averages short-term frequency estimates for header and sync detection while retaining the original picture-sample bandwidth.

Desktop tests exercise the same C++ decoder linked into the firmware. The earlier decoder missed the header on a DW-001 fixture with reduced level, additive noise and a 3.69 ms echo. Version 0.1.1 recovered all 240 lines from the same fixture with a reference correlation of 0.933. It also recovered DW-001 and both ARISS Fram2 practice images, and rejected noise, silence, a truncated image and an unsupported identifier. See `tools/verification.txt` for measurements.

The tests replay digital samples. Acoustic capture and the end-to-end update on a physical Sticky remain unverified. A PC decode confirms the source WAV; the new level and line counters let you check the two stages on the board itself.

## Build or make another card

Install Arduino-ESP32 **3.3.7**, **bb_captouch 1.3.1**, and Seeed_GFX2 from commit `c891be8dd26ffd9d720f40d4445d135a4fc41dcf`. Open `DarkwatchPostcards/DarkwatchPostcards.ino` in Arduino IDE. Select **XIAO_ESP32S3**, **OPI PSRAM**, **USB CDC On Boot: Disabled**, and the default 8 MB partition scheme. With the dependencies already installed, the CLI equivalent is:

    arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi,CDCOnBoot=cdc --export-binaries DarkwatchPostcards

For standalone artwork, use a 4:3 source with bold strokes. The encoder fits and pads other aspect ratios. With Python 3.12:

    python -m pip install PySSTV==0.5.10
    python tools/encode.py postcard.png postcard.wav

To repeat the host tests, install NumPy, Pillow and ffmpeg and run `python tools/verify.py` on Linux or macOS with a C++ compiler. For the optional reference tests, download Slide01.wav, Slide02.wav and their two matching SlideNN-Robot-36-Color.jpg files from ARISS, then pass `--ariss DIRECTORY`. Reference recordings are linked below.

## Project references

- [ARISS Fram2 practice WAVs and reference images](https://ariss-usa.org/ARISS_SSTV/Fram2Test/)
- [Seeed Sticky display and board setup](https://www.seeedstudio.com/sticky/docs/en/device-guide/arduino-basics/)
- [Seeed microphone, touch and shared-SPI examples](https://www.seeedstudio.com/sticky/docs/en/device-guide/arduino-interaction/)
- [Seeed Sticky backup guidance](https://www.seeedstudio.com/sticky/playground/sticky-lab/)
- [Seeed_GFX2 pinned source](https://github.com/Seeed-Studio/Seeed_GFX2/tree/c891be8dd26ffd9d720f40d4445d135a4fc41dcf)
- [Arduino-ESP32 3.3.7 source and licenses](https://github.com/espressif/arduino-esp32/tree/3.3.7)
- [PySSTV encoder](https://github.com/dnet/pySSTV)
