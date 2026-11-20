"""End-to-end tests against the independent PySSTV 0.5.10 encoder."""
from pathlib import Path
from tempfile import TemporaryDirectory
import io
import subprocess
import numpy as np
from PIL import Image
from pysstv.color import PD90, PD120
from verify import BASE, pcm

with TemporaryDirectory() as temp:
    exe = Path(temp) / "decode"
    subprocess.run(["c++", "-O2", str(BASE / "tools/decode_pcm.cpp"),
                    str(BASE / "DarkwatchPostcards/Robot36.cpp"), "-o", exe], check=True)
    for mode in (PD90, PD120):
        y, x = np.indices((mode.HEIGHT, mode.WIDTH))
        # Distinct vertical/horizontal ramps and saturated colors exercise chroma
        # skipping as well as line order, aspect conversion and frame boundaries.
        rgb = np.stack(((x*2+y)%256, (y*3)%256, (x+y*2)%256), axis=-1)
        art = Image.fromarray(rgb.astype("uint8"))
        wav = Path(temp) / "test.wav"
        mode(art, 16000, 16).write_wav(str(wav))
        data = pcm(wav) + bytes(128)  # Drain the FIR after the final transmitted sample.
        result = subprocess.run([exe], input=data, capture_output=True)
        assert result.returncode == 0, result.stderr
        assert mode.__name__.encode() in result.stderr
        decoded = np.asarray(Image.open(io.BytesIO(result.stdout)))
        ref = np.asarray(art.convert("L").resize((320,240), Image.Resampling.NEAREST))
        score = np.corrcoef(decoded.ravel(), ref.ravel())[0,1]
        assert score > .98, (mode.__name__, score)
        broken = data[:12*16000*2] + bytes(4*16000*2)
        lost = subprocess.run([exe], input=broken, capture_output=True)
        assert lost.returncode == 1 and not lost.stdout and b"Sync lost" in lost.stderr
        print(f"{mode.__name__}: 320 x 240, reference correlation {score:.4f}, loss detected")
