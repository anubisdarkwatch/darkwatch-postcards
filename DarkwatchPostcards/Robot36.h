#pragma once
#include <stdint.h>

// Streaming luminance receiver. A sample is signed, mono, 16 kHz PCM.
// Caller owns the 320 x 240 byte output frame for the whole reception.
class Robot36 {
public:
    static constexpr int Rate = 16000, Width = 320, Height = 240;
    enum Event { None, Locked, Complete, Lost, OtherMode };
    explicit Robot36(uint8_t *image);
    Event push(int16_t sample);
    void reset();
    int rows() const { return row; }
    bool receiving() const { return phase==Picture; }
    int vis() const { return mode; }
private:
    enum Phase { Search, Header, Picture } phase = Search;
    static constexpr int Taps = 33;
    float kernelI[Taps], kernelQ[Taps], history[2*Taps]{};
    int head = 0;
    float dcEstimate = 0;
    float controlHz = 0;
    float prevI = 0, prevQ = 0;
    uint8_t *frame;
    int leader = 0, armed = 0, startRun = 0, elapsed = 0;
    int bitSum = 0, bitCount = 0, bits = 0, mode = 0;
    int row = 0, rowTime = 0, syncRun = 0, missed = 0, pixel = 0;
    bool aligned = false;
    float level(float sample);
};
