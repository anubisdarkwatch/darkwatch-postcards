#pragma once
#include <stdint.h>
#include <stddef.h>

struct ToneResult {
    int hz=0;
    unsigned stableMs=0, pilotMs=0;
};

// Analyze the first 256 samples of each 1024-sample interval at 16 kHz.
// longest and pilot are durations of uninterrupted, tone-dominated windows.
class ToneCheck {
public:
    static constexpr int Window=256, Hop=1024;
    ToneCheck();
    void observe(const int16_t *samples);
    ToneResult result() const { return best; }
private:
    static constexpr int First=5, Last=72;
    float window[Window], coefficient[Last+1];
    int run=0, pilot=0;
    float center=0;
    ToneResult best;
};
