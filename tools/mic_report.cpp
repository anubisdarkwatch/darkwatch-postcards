// Run the same tone estimator and Robot 36 decoder on 16 kHz mono S16LE stdin.
#include "../DarkwatchPostcards/ToneCheck.h"
#include "../DarkwatchPostcards/Robot36.h"
#include <cstdio>
#include <algorithm>
int main() {
    ToneCheck tones;
    static uint8_t frame[320*240];
    Robot36 rx(frame);
    int16_t block[ToneCheck::Hop];
    int rows=0;
    bool complete=false,locked=false;
    size_t n;
    while ((n=fread(block,sizeof(int16_t),ToneCheck::Hop,stdin))>0) {
        if (n>=ToneCheck::Window) tones.observe(block);
        for (size_t i=0;i<n && !complete;++i) {
            auto e=rx.push(block[i]);
            locked|=e==Robot36::Locked;
            rows=std::max(rows,rx.rows());
            complete=e==Robot36::Complete;
        }
    }
    auto r=tones.result();
    printf("%d %u %u %d %d\n",r.hz,r.stableMs,r.pilotMs,locked,rows);
}
