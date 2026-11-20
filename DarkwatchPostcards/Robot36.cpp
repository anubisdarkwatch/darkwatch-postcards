#include "Robot36.h"
#include <math.h>
#include <string.h>

Robot36::Robot36(uint8_t *image) : frame(image) {
    // Analytic bandpass for the SSTV audio band. The phase change of successive
    // complex samples gives instantaneous frequency even when amplitude varies.
    constexpr double pi = 3.141592653589793;
    for (int k=0; k<Taps; ++k) {
        int t = k - Taps/2;
        double w = 0.54 - 0.46*cos(2*pi*k/(Taps-1));
        double lp = t ? sin(2*pi*1150*t/Rate)/(pi*t) : 2300.0/Rate;
        kernelI[k] = w*lp*cos(2*pi*1750*t/Rate);
        kernelQ[k] = -w*lp*sin(2*pi*1750*t/Rate);
    }
}
void Robot36::reset() {
    phase = Search;
    leader = armed = startRun = row = syncRun = missed = 0;
    memset(history, 0, sizeof(history));
    dcEstimate = prevI = prevQ = controlHz = 0;
}
float Robot36::level(float sample) {
    // Remove the microphone's DC bias before measuring phase. A low-level tone
    // can otherwise be smaller than the residual DC passed by the finite FIR.
    // This one-pole estimate tracks slow baseline drift (~81 Hz at 16 kHz).
    dcEstimate += (sample - dcEstimate) * (1.0f / 32);
    history[head] = history[head+Taps] = sample - dcEstimate;
    head = (head+1)%Taps;
    float i=0, q=0;
    for (int n=0; n<Taps; ++n) {
        i += history[head+n]*kernelI[n];
        q += history[head+n]*kernelQ[n];
    }
    float dot = prevI*i + prevQ*q, cross = prevI*q - prevQ*i;
    prevI=i; prevQ=q;
    if (dot < 2 || cross <= 0) return 0;
    return atanf(cross/dot)*(Rate/6.28318530718f);
}
Robot36::Event Robot36::push(int16_t sample) {
    float hz = level(sample);
    // VIS symbols are 30 ms long. Average their frequency over about 1 ms
    // so an isolated noisy sample cannot break an otherwise valid leader or sync.
    // Preserve the unsmoothed discriminator for the much shorter picture pixels.
    float bounded = hz < 600 ? 600 : hz > 2800 ? 2800 : hz;
    controlHz += (bounded-controlHz)*(1.0f/14);
    bool sync = fabsf(controlHz-1200)<85;
    if (phase == Search) {
        if (fabsf(controlHz-1900)<95) {
            if (leader < 3200) ++leader;
            if (leader >= 2400) armed=2000;
        } else if (leader > 0) leader = leader>12 ? leader-12 : 0;
        if (armed) {
            --armed;
            startRun = sync ? startRun+1 : 0;
            // Longer than the 10 ms calibration break between leaders.
            if (startRun==288) {
                elapsed=startRun; bits=bitCount=bitSum=0; phase=Header;
            }
        } else startRun=0;
        return None;
    }
    if (phase == Header) {
        ++elapsed;
        int slot=elapsed/480, position=elapsed%480;
        // Each 30 ms symbol is measured across its middle 12 ms.
        if (slot>=1 && slot<=8) {
            if (position>=144 && position<336 && (fabsf(controlHz-1100)<75 || fabsf(controlHz-1300)<75)) {
                bitSum += controlHz<1200 ? 1 : -1;
                ++bitCount;
            }
            if (position==336) {
                if (bitCount<120 || abs(bitSum)<bitCount/2) { reset(); return None; }
                if (bitSum>0) bits |= 1<<(slot-1);
                bitCount=bitSum=0;
            }
        }
        if (slot==9 && position==240) {
            mode=bits&127;
            if (!sync || __builtin_parity((unsigned)bits)) { reset(); return None; }
            if (mode!=8) { reset(); return OtherMode; }
            phase=Picture; row=0; missed=0; pixel=0; aligned=false;
            rowTime=-240; syncRun=0;
            return Locked;
        }
        return None;
    }
    // 150 ms per line. Re-lock at each sync trailing edge to absorb clock drift.
    // Coast over two damaged sync pulses, then abandon an interrupted picture.
    if (!aligned && rowTime>=-240 && rowTime<=240) {
        if (sync) ++syncRun;
        else {
            if (syncRun>=55 && rowTime>=64) {
                rowTime=151; aligned=true; missed=0;
            }
            syncRun=0;
        }
    }
    if (!aligned && rowTime==241 && ++missed>=3) { reset(); return Lost; }
    if (pixel<Width && rowTime>=192+(2*pixel+1)*1408/(2*Width)) {
        float value=(hz-1500)*(255.0f/800);
        frame[row*Width+pixel++] = (uint8_t)(value<0 ? 0 : value>255 ? 255 : value+0.5f);
    }
    if (pixel==Width && row==Height-1) { phase=Search; row=Height; return Complete; }
    if (++rowTime==2400) {
        rowTime=0; ++row; pixel=0; aligned=false; syncRun=0;
    }
    return None;
}
