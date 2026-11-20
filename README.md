# Darkwatch Postcards

Receive Robot 36, PD90, or PD120 SSTV pictures through the built-in microphone of a Seeed Studio reTerminal Sticky and display it on e-paper. The flasher includes a DW-001 sample for each mode. A FAT32 microSD card lets the Sticky save received images.

[Open the USB installer](https://anubisdarkwatch.github.io/darkwatch-postcards/) · [Download v0.2.0 firmware](docs/firmware/Darkwatch-Postcards-Sticky-v0.2.0.bin) · [Receiver and backup guide](RECEIVER.md)

Connect the 3.97-inch Sticky with a USB data cable and open the installer in desktop Chrome or Edge. Installation replaces the current firmware and partition layout. Save your full factory backup before the first install.

## Try it

1. After flashing, confirm **Robot 36 / PD90 / PD120 v0.2.0** and tap **AI/OK** to listen.
2. Play a PD sample on the installer page, or play the original [DW-001 Robot 36 sample](postcards/DW-001-Robot36.wav) from the beginning near the lower microphone.
3. Watch for header lock, 240 rows, and the finished image. The installer also includes a speaker check for troubleshooting.

## Develop

The Arduino sketch is in `DarkwatchPostcards/`. See the [build instructions](RECEIVER.md#build-or-make-another-card) for pinned dependencies and board options. Run `python tools/verify.py` for the host decoder tests. The [original source kit](docs/downloads/Darkwatch-Postcards-v0.1.3.zip) includes the release checksums.

## Host the installer

In GitHub Settings → Pages, select **Deploy from a branch**, then **main** and **/docs**. For a local preview, run `python -m http.server --directory docs 8000` and open `http://localhost:8000`. USB installation uses ESP Web Tools 10.1.1 and requires access to unpkg.com.
