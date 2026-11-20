# Darkwatch Postcards

Receive a Robot 36 SSTV picture through the built-in microphone of a Seeed Studio reTerminal Sticky and display it on e-paper. The included DW-001 transmission takes about 37 seconds. A FAT32 microSD card lets the Sticky save received images.

[Open the USB installer](https://anubisdarkwatch.github.io/darkwatch-postcards/) · [Download v0.1.3 firmware](docs/firmware/Darkwatch-Postcards-Sticky-v0.1.3.bin) · [Receiver and backup guide](RECEIVER.md)

Connect the 3.97-inch Sticky with a USB data cable and open the installer in desktop Chrome or Edge. Installation replaces the current firmware and partition layout. Save your full factory backup before the first install.

## Try it

1. After flashing, confirm **ROBOT 36 v0.1.3** and tap **AI/OK** to listen.
2. Play [DW-001](postcards/DW-001-Robot36.wav) from the beginning near the lower microphone.
3. Watch for header lock, 240 rows, and the finished image. The installer also includes a speaker check for troubleshooting.

## Develop

The Arduino sketch is in `DarkwatchPostcards/`. See the [build instructions](RECEIVER.md#build-or-make-another-card) for pinned dependencies and board options. Run `python tools/verify.py` for the host decoder tests. The [original source kit](docs/downloads/Darkwatch-Postcards-v0.1.3.zip) includes the release checksums.

## Host the installer

In GitHub Settings → Pages, select **Deploy from a branch**, then **main** and **/docs**. For a local preview, run `python -m http.server --directory docs 8000` and open `http://localhost:8000`. USB installation uses ESP Web Tools 10.1.1 and requires access to unpkg.com.
