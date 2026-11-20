#include "ToneCheck.h"
#include <cmath>
#include <algorithm>

ToneCheck::ToneCheck() {
    constexpr float circle=6.28318530718f;
    for (int i=0;i<Window;++i) window[i]=0.5f-0.5f*cosf(circle*i/Window);
    for (int k=First;k<=Last;++k) coefficient[k]=2*cosf(circle*k/Window);
}
void ToneCheck::observe(const int16_t *s) {
    int sum=0;
    for (int i=0;i<Window;++i) sum+=s[i];
    float centered[Window], energy=0, mean=float(sum)/Window;
    for (int i=0;i<Window;++i) {
        centered[i]=(s[i]-mean)*window[i];
        energy+=centered[i]*centered[i];
    }
    float power[Last+1]{};
    int peak=First;
    for (int k=First;k<=Last;++k) {
        float a=0,b=0;
        for (float sample:centered) {
            float next=sample+coefficient[k]*a-b;
            b=a; a=next;
        }
        power[k]=std::max(0.0f,a*a+b*b-coefficient[k]*a*b);
        if (power[k]>power[peak]) peak=k;
    }
    int hz=0;
    if (energy>Window && peak>First && peak<Last) {
        float l=power[peak-1], m=power[peak], r=power[peak+1];
        // Fraction of total non-DC energy in the three bins around the peak.
        if (2*(l+m+r)>0.55f*Window*energy) {
            l=logf(l+1e-12f); m=logf(m+1e-12f); r=logf(r+1e-12f);
            float curve=l-2*m+r;
            float offset=(curve<0)?0.5f*(l-r)/curve:0;
            hz=lroundf((peak+offset)*16000/Window);
        }
    }
    if (hz) {
        if (!run || fabsf(hz-center)>45) { center=hz; run=0; }
        center+=(hz-center)/(++run);
        unsigned duration=(run-1)*64+16;
        if (duration>best.stableMs) { best.stableMs=duration; best.hz=lroundf(center); }
    } else run=0;
    if (hz>=1840 && hz<=1960) {
        ++pilot;
        best.pilotMs=std::max(best.pilotMs,unsigned((pilot-1)*64+16));
    } else pilot=0;
}
