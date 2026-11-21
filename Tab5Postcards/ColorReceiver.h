#pragma once
#include <stdint.h>

// Streaming color receiver. A sample is signed, mono, 16 kHz PCM.
// Caller owns the 640 x 496 RGB888 output frame for the whole reception.
class ColorReceiver {
public:
    static constexpr int Rate = 16000, Width = 320, Height = 240;
    int width() const { return mode==95 ? 640 : 320; }
    int height() const { return mode==95 ? 496 : mode==99 ? 256 : 240; }
    enum Event { None, Locked, Complete, Lost, OtherMode };
    explicit ColorReceiver(uint8_t *image);
    Event push(int16_t sample);
    void reset();
    int rows() const { return row; }
    bool receiving() const { return phase==Picture; }
    static const char *modeName(int vis);
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
    // PD modes carry two luminance lines separated by shared chroma.
    int pdLine = 0, pdHeight = 0, scanUs = 0, nextPixel = 0;
    uint8_t channels[4][640]{};
    void colorPair();
    Event pushPd(float hz, bool sync);
    int pdSampleTime() const;
    float level(float sample);
};
