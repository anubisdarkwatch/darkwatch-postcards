"""Usage: python encode.py source.png output.wav (pip install PySSTV==0.5.10)"""
import argparse
from PIL import Image, ImageOps
from pysstv.color import Robot36, PD90, PD120

def main():
    p=argparse.ArgumentParser(description="Create an SSTV postcard WAV.")
    p.add_argument("image")
    p.add_argument("wav")
    modes={"robot36": Robot36, "pd90": PD90, "pd120": PD120}
    p.add_argument("--mode", choices=modes, default="robot36")
    args=p.parse_args()
    encoder=modes[args.mode]
    with Image.open(args.image) as image:
        # Preserve the entire artwork and pad non-4:3 originals.
        canvas=ImageOps.pad(image.convert("RGB"), (encoder.WIDTH,encoder.HEIGHT), color="white")
        encoder(canvas,44100,16).write_wav(args.wav)

if __name__=="__main__":
    main()
