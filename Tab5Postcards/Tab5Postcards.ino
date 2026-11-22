#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include "ColorReceiver.h"

constexpr uint32_t black=0x080d0b, panel=0x15211c, amber=0xffb000, green=0x86f79a, ivory=0xe8eee7;
uint8_t *frame;
ColorReceiver *rx;
// Each queued buffer covers 128 ms of audio at the decoder rate.
constexpr int blockSamples=2048;
int16_t pcm[2][blockSamples];
unsigned active=0;
bool listening=false;
int rendered=0;
char saved[40];

void label(int x,int y,int size,uint32_t color,const char *s) {
    auto &d=M5.Display;
    d.setTextSize(size); d.setTextColor(color); d.setCursor(x,y); d.print(s);
}
void screen(const char *state,const char *detail,bool card=false) {
    auto &d=M5.Display;
    int w=d.width(), h=d.height();
    d.fillScreen(black);
    label(32,20,2,amber,"ANUBIS DARKWATCH");
    label(32,51,4,ivory,"Darkwatch Postcards");
    d.drawFastHLine(32,102,w-64,amber);
    d.drawRoundRect(32,125,672,526,12,panel);
    label(740,150,3,green,state);
    label(740,210,2,ivory,detail);
    if (!card) {
        label(90,315,4,amber,"SSTV receiver");
        label(90,375,3,ivory,"Robot 36 / PD90 / PD120");
    }
    if (!listening) {
        d.fillRoundRect(740,h-122,w-780,72,12,amber);
        label(766,h-102,3,black,"Listen");
    }
}
bool writeBmp() {
    if (!SD.begin(42,SPI,25000000)) return false;
    for(int i=1;;++i) {
        if(i>99999) return false;
        snprintf(saved,sizeof(saved),"/RX%05d.BMP",i);
        if(!SD.exists(saved)) break;
    }
    const char *tmp="/postcard.tmp";
    if(SD.exists(tmp) && !SD.remove(tmp)) return false;
    File f=SD.open(tmp,FILE_WRITE);
    if(!f) return false;
    int w=rx->width(),h=rx->height();
    uint32_t bytes=3*w*h;
    uint8_t header[54]={'B','M'};
    auto put=[&](int at,uint32_t v){memcpy(header+at,&v,4);};
    put(2,54+bytes);put(10,54);put(14,40);put(18,w);put(22,h);
    header[26]=1;header[28]=24;put(34,bytes);
    bool ok=f.write(header,54)==54;
    static uint8_t row[640*3];
    for(int y=h-1;ok && y>=0;--y) {
        for(int x=0;x<w;++x) {
            const uint8_t *p=frame+3*(y*w+x);
            row[3*x]=p[2];row[3*x+1]=p[1];row[3*x+2]=p[0];
        }
        ok=f.write(row,w*3)==size_t(w*3);
    }
    f.close();
    return ok && SD.rename(tmp,saved);
}
void drawRows(int end) {
    static uint16_t line[640];
    int w=rx->width(),h=rx->height();
    for(;rendered<end;++rendered) {
        const uint8_t *p=frame+3*rendered*w;
        for(int x=0;x<w;++x,p+=3) line[x]=M5.Display.color565(p[0],p[1],p[2]);
        M5.Display.pushImage(48+(640-w)/2,140+(496-h)/2+rendered,w,1,line);
    }
    M5.Display.fillRect(740,255,360*end/h,8,green);
}
void setup() {
    M5.begin(); if(M5.Display.width()<M5.Display.height()) M5.Display.setRotation(1);
    SPI.begin(43,39,44,42);
    frame=(uint8_t*)ps_malloc(640*496*3);
    if(frame) rx=new ColorReceiver(frame);
    screen(rx?"Ready":"Memory error",rx?"Tap Listen to receive":"PSRAM unavailable");
}
void loop() {
    if (!listening) M5.update();
    if(!rx) {delay(10);return;}
    if(!listening) {
        auto t=M5.Touch.getDetail();
        if(!t.wasPressed() || t.x<740 || t.x>M5.Display.width()-40 ||
           t.y<M5.Display.height()-122 || t.y>M5.Display.height()-50) {delay(4);return;}
        rx->reset();active=0;
        listening=true;
        screen("Listening","Microphone starting");
        M5.Display.fillRect(48,140,640,496,black);
        if(!M5.Mic.begin() || !M5.Mic.record(pcm[0],blockSamples,16000)) {
            listening=false;screen("Mic error","Tap Listen to retry");return;
        }
        M5.Display.fillRect(740,198,510,44,black);
        label(740,210,2,ivory,"Play audio from the beginning");
    }
    if(!M5.Mic.record(pcm[1-active],blockSamples,16000)) {
        M5.Mic.end();listening=false;screen("Mic error","Tap Listen to retry");return;
    }
    while(M5.Mic.isRecording()>1) delay(1);
    for(auto sample:pcm[active]) {
        auto ev=rx->push(sample);
        if(ev==ColorReceiver::Locked) {
            rendered=0;

            M5.Display.fillRect(740,140,500,150,black);
            label(740,150,3,green,ColorReceiver::modeName(rx->vis()));
            label(740,210,2,ivory,"Receiving");
            M5.Display.fillRect(740,255,360,8,panel);
        } else if(ev==ColorReceiver::Lost) {
            char detail[64];
            snprintf(detail,sizeof(detail),"%s lost at row %d; replay from start",ColorReceiver::modeName(rx->vis()),rendered);
            rendered=0;
            M5.Display.fillRect(740,140,525,150,black);
            label(740,150,3,amber,"Listening");
            label(740,210,2,ivory,detail);
        } else if(ev==ColorReceiver::Complete) {
            M5.Mic.end();listening=false;
            bool ok=writeBmp();
            screen(ColorReceiver::modeName(rx->vis()),ok?saved:"RAM only - SD save failed",true);
            rendered=0;drawRows(rx->height());break;
        }
    }
    if(listening && rx->receiving() && (rx->rows() & ~1)>rendered) drawRows(rx->rows() & ~1);
    active=1-active;
}
