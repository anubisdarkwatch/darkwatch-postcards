"""Run with Python 3, Pillow, NumPy, ffmpeg, and a C++ compiler on Linux/macOS.
Optional: --ariss DIR checks Slide01/02.wav and matching ARISS reference JPEGs."""
from pathlib import Path
import argparse
import io
import subprocess
import tempfile
import numpy as np
from PIL import Image

BASE = Path(__file__).resolve().parents[1]

def pcm(path):
    return subprocess.check_output(["ffmpeg","-v","error","-i",str(path),
        "-ar","16000","-ac","1","-f","s16le","-"])

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--ariss", type=Path)
    args=parser.parse_args()
    with tempfile.TemporaryDirectory() as d:
        exe=Path(d)/"decode"
        subprocess.run(["c++","-O2","-std=c++17",str(BASE/"tools/decode_pcm.cpp"),
            str(BASE/"DarkwatchPostcards/Robot36.cpp"),"-o",str(exe)],check=True)
        def check(name, data, source=None, minimum=0.0):
            p=subprocess.run([str(exe)], input=data, capture_output=True)
            if source is None:
                assert p.returncode==1 and not p.stdout,name
                print(name, "no complete frame"); return
            assert p.returncode==0,(name,p.stderr)
            image=Image.open(io.BytesIO(p.stdout))
            assert image.size==(320,240)
            ref=np.array(Image.open(source).convert("L"))
            score=np.corrcoef(np.asarray(image).ravel(),ref.ravel())[0,1]
            assert score>minimum,(name,score)
            print(f"{name}: full frame, reference correlation {score:.3f}")
        artwork=BASE/"postcards/DW-001.png"
        data=pcm(BASE/"postcards/DW-001-Robot36.wav")
        check("DW-001",data,artwork,0.95)
        rng=np.random.default_rng(36)
        samples=np.frombuffer(data,dtype="<i2")
        noisy=(samples*0.20+rng.normal(0,180,len(samples))).clip(-32768,32767).astype("<i2")
        check("Quiet playback + noise",noisy.tobytes(),artwork,.92)
        # Model low microphone level, one reflection, and additive room noise.
        # The v0.1.0 decoder missed the header for this exact sample sequence.
        gain=0.01
        delayed=59
        room=samples.astype(float)*gain
        room[delayed:]+=samples[:-delayed]*(gain*0.26)
        room+=np.random.default_rng(37).normal(0,30,len(samples))
        check("Speaker simulation (echo + noise)",room.astype("<i2").tobytes(),artwork,.90)
        noise=np.random.default_rng(41).normal(0,500,16000*40).astype("<i2")
        check("Noise only",noise.tobytes())
        # Field regression: onboard PCM had a mean near +1355 counts, while
        # the quiet SSTV signal was tens of counts. Cover either polarity.
        for bias in (1355, -1355):
            weak=samples.astype(float)*0.002+bias+np.random.default_rng(121).normal(0,3,len(samples))
            check(f"Quiet mic, DC offset {bias:+d}",np.round(weak).astype("<i2").tobytes(),artwork,.94)
        check("Constant DC only", np.full(16000*4,1355,dtype="<i2").tobytes())
        check("Silence",bytes(16000*2*5))
        check("Interrupted reception",data[:16000*2*7]+bytes(16000*2))
        check("Recovery after interruption",data[:16000*2*7]+bytes(16000*2)+data,artwork,.95)
        # Change VIS 8 to VIS 44, with even parity, while retaining Robot 36 line data.
        # A decoder which ignores VIS would incorrectly accept this fixture.
        changed=samples.copy()
        for i in range(8):
            n=round((0.640+0.030*i)*16000)
            bit=((44>>i)&1) if i<7 else (44).bit_count()%2
            f=1100 if bit else 1300
            changed[n:n+480]=14000*np.sin(np.arange(480)*2*np.pi*f/16000)
        check("Unsupported VIS",changed.tobytes())
        if args.ariss:
            for slide in ("Slide01","Slide02"):
                check(slide,pcm(args.ariss/(slide+".wav")),
                    args.ariss/(slide+"-Robot-36-Color.jpg"),.83)

if __name__=="__main__":
    main()
