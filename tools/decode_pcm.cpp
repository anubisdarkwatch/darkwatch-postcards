// Feed mono signed 16-bit little-endian PCM at 16000 Hz on stdin (little-endian host).
#include "../DarkwatchPostcards/Robot36.h"
#include <cstdio>
int main() {
    static uint8_t image[Robot36::Width*Robot36::Height];
    Robot36 rx(image);
    int16_t sample;
    while (fread(&sample,sizeof(sample),1,stdin)==1) {
        auto event=rx.push(sample);
        if (event==Robot36::Locked) fprintf(stderr,"%s header locked\n",Robot36::modeName(rx.vis()));
        if (event==Robot36::Lost) fprintf(stderr,"Sync lost\n");
        if (event==Robot36::Complete) {
            printf("P5\n320 240\n255\n");
            return fwrite(image,1,sizeof(image),stdout)==sizeof(image) ? 0 : 2;
        }
    }
    fprintf(stderr,"No complete SSTV frame; rows=%d\n",rx.rows());
    return 1;
}
