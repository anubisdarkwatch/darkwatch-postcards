"""Usage: python encode.py source.png output.wav (pip install PySSTV==0.5.10)"""
import argparse
from PIL import Image, ImageOps
from pysstv.color import Robot36

def main():
    p=argparse.ArgumentParser(description="Create a broadcast-compatible Robot 36 postcard WAV.")
    p.add_argument("image")
    p.add_argument("wav")
    args=p.parse_args()
    with Image.open(args.image) as image:
        # Preserve the entire artwork and pad non-4:3 originals.
        canvas=ImageOps.pad(image.convert("RGB"), (320,240), color="white")
        Robot36(canvas,44100,16).write_wav(args.wav)

if __name__=="__main__":
    main()
