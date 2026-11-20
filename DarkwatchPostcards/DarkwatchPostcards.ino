#include <Arduino.h>
#include <bb_captouch.h>
#include <esp_heap_caps.h>
#include "AudioReceiver.h"
#include "ToneCheck.h"
#include <algorithm>
#include <SD.h>
#include <Wire.h>
#include <Seeed_GFX.h>
#include <bus/Bus_SPI.h>
#include "Robot36.h"

constexpr int okKey=4, upKey=5, downKey=6, sdCS=8, sdEnable=10, sdDetect=11;
constexpr size_t frameBytes = Robot36::Width * Robot36::Height;
const char *album = "/POSTCARDS";
Seeed_GFX screen(Seeed_Product::reTerminal_Sticky);
BBCapTouch touch;
bool touchReady=false;
uint8_t frame[frameBytes];
Robot36 decoder(frame);
AudioReceiver receiver(decoder);
RxStatus audioStatus;
int lastLevel=-999, lastGroup=-1;
RxState lastState=RxState::Waiting;
unsigned lastReport=0;
uint32_t lastPaint=0;
bool listening=false, ready=false, haveImage=false, clockBus=false;
int cardNumber=0, cardVis=8;
bool testPictureAvailable=false;
char received[32]="Clock unset";
char notice[80]="";

void writePin(int pin, bool high) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, high ? HIGH : LOW);
}
void label(const char *s,int x,int y,int size) {
    screen.setTextSize(size);
    screen.drawString(s,x,y);
}
void showPage(bool picture) {
    screen.fillScreen(TFT_WHITE);
    screen.setTextColor(TFT_BLACK);
    if (picture) {
        // Ordered dithering preserves photo midtones on a one-bit panel.
        const uint8_t bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
        for (int y=0;y<240;++y) for (int x=0;x<320;++x) {
            if (frame[y*320+x] < 8+16*bayer[(y%4)*4+x%4])
                screen.fillRect(x*2,y*2,2,2,TFT_BLACK);
        }
        screen.drawFastVLine(648,8,464,TFT_BLACK);
        label("DARKWATCH",660,20,2); label("POSTCARDS",660,43,2);
        char number[22];
        snprintf(number,sizeof(number),"RX %05d",cardNumber);
        label(cardNumber?number:"PREVIEW",660,96,2);
        label(Robot36::modeName(cardVis),660,132,2);
        if (strlen(received)==19) {
            char date[11]; memcpy(date,received,10); date[10]=0;
            label(date,660,188,2); label(received+11,660,212,2);
        } else label("Clock unset",660,194,2);
        label(cardNumber?"SD saved":"RAM only",660,270,2);
        label("UP / DOWN",660,355,2); label(touchReady?"or swipe":"Browse",660,378,2);
        label("OK Listen",660,436,2);
    } else {
        screen.fillRect(0,0,800,80,TFT_BLACK);
        screen.setTextColor(TFT_WHITE);
        label("DARKWATCH POSTCARDS",28,20,5);
        screen.setTextColor(TFT_BLACK);
        label("Robot 36 / PD90 / PD120   v0.2.0",30,111,3);
        label(notice,30,190,3);
        if (listening && audioStatus.reports) {
            char line[80];
            snprintf(line,sizeof(line),"Mic level: %d dBFS    Peak-to-peak: %d",audioStatus.dbfs,audioStatus.peakToPeak);
            label(line,30,252,2);
            screen.drawRect(30,284,404,20,TFT_BLACK);
            int width=constrain(audioStatus.dbfs+90,0,90)*400/90;
            if (width>0) screen.fillRect(32,286,width,16,TFT_BLACK);
            snprintf(line,sizeof(line),"Input: %d samples/sec",audioStatus.sampleRate);
            label(line,30,327,2);
        } else if (listening) {
            label("Play the WAV from the beginning.",30,259,3);
        }
        label("OK  Listen   |   Hold OK  Mic test   |   UP / DOWN  Album",30,423,2);
    }
    screen.update();
}
void stopListening() {
    receiver.stop();
    listening=false;
}
void startListening() {
    stopListening();
    haveImage=false; testPictureAvailable=false; cardNumber=0;
    audioStatus=RxStatus{};
    lastLevel=-999; lastGroup=-1; lastReport=0; lastState=RxState::Waiting;
    listening=receiver.start();
    strcpy(notice,listening?"Listening for SSTV":"Microphone start failed. Press OK.");
    showPage(false);
    lastPaint=millis();
}
int bcd(uint8_t v) { return (v>>4)*10+(v&15); }
void readClock() {
    strcpy(received,"Clock unset");
    if (!clockBus) return;
    Wire1.beginTransmission(0x51); Wire1.write(2);
    if (Wire1.endTransmission(false)!=0 || Wire1.requestFrom((uint8_t)0x51,(size_t)7)!=7) return;
    uint8_t t[7];
    for (auto &v:t) v=Wire1.read();
    int day=bcd(t[3]&63), month=bcd(t[5]&31), yy=bcd(t[6]), h=bcd(t[2]&63);
    int m=bcd(t[1]&127), s=bcd(t[0]&127);
    if ((t[0]&128) || yy<24 || yy>99 || day<1 || day>31 || month<1 || month>12 || h>23 || m>59 || s>59) return;
    snprintf(received,sizeof(received),"20%02d-%02d-%02d %02d:%02d:%02d",yy,month,day,h,m,s);
}
bool openSD() {
    if (digitalRead(sdDetect)!=LOW) return false;
    writePin(15,true); writePin(sdCS,true); writePin(sdEnable,true); delay(100);
    auto *bus=static_cast<Bus_SPI&>(screen.panel().driver().bus()).spiInstance();
    if (bus && SD.begin(sdCS,*bus) && SD.cardType()!=CARD_NONE) return true;
    SD.end(); writePin(sdCS,true); writePin(sdEnable,false); return false;
}
void closeSD() { SD.end(); writePin(sdCS,true); writePin(sdEnable,false); writePin(15,true); }
void pathFor(int n,char *path) { snprintf(path,40,"%s/RX%05d.PGM",album,n); }
// Directory order is filesystem-dependent. Return the nearest existing neighbor,
// wrapping at the ends, or the latest capture if the album is opened from home.
int findCard(int from,int direction) {
    File dir=SD.open(album);
    if (!dir || !dir.isDirectory()) return 0;
    int first=100000,last=0,best=0;
    for (File f=dir.openNextFile(); f; f=dir.openNextFile()) {
        const char *name=f.name();
        int n=0;
        if (strlen(name)==11 && strncmp(name,"RX",2)==0 && strcmp(name+7,".PGM")==0) {
            bool digits=true;
            for (int i=2;i<7;++i) { if (!isdigit((unsigned char)name[i])) digits=false; }
            if (digits) n=atoi(name+2);
        }
        f.close();
        if (!n) continue;
        first=min(first,n); last=max(last,n);
        if (direction>0 && n>from && (!best || n<best)) best=n;
        if (direction<0 && n<from && n>best) best=n;
    }
    if (!from) return last;
    return best?best:(direction>0 && last ? first:last);
}
int saveCard() {
    if (!openSD()) return 0;
    int n=0;
    if (SD.exists(album) || SD.mkdir(album)) {
        n=findCard(0,0)+1;
        char path[40]; pathFor(n,path);
        const char *temp="/POSTCARDS/CAPTURE.TMP";
        if (SD.exists(temp)) SD.remove(temp);
        File f;
        if (n<=99999 && !SD.exists(path)) f=SD.open(temp,"w");
        char header[72];
        int len=snprintf(header,sizeof(header),"P5\n# %s\n# VIS %d\n320 240\n255\n",received,cardVis);
        bool ok=f && f.write((const uint8_t*)header,len)==(size_t)len && f.write(frame,frameBytes)==frameBytes;
        if (f) { f.flush(); ok=ok && !f.getWriteError() && f.size()==frameBytes+len; f.close(); }
        if (!ok || !SD.rename(temp,path)) { SD.remove(temp); n=0; }
    }
    closeSD(); return n;
}
void browse(int direction) {
    stopListening();
    bool loaded=false;
    if (openSD()) {
        int n=findCard(cardNumber,direction);
        if (n) {
            char path[40]; pathFor(n,path);
            File f=SD.open(path);
            if (f && f.readStringUntil('\n')=="P5") {
                String stamp=f.readStringUntil('\n');
                String dimensions=f.readStringUntil('\n');
                int savedVis=8; // Original single-mode album files omit VIS.
                if (dimensions.startsWith("# VIS ")) {
                    savedVis=dimensions.substring(6).toInt();
                    dimensions=f.readStringUntil('\n');
                }
                if (stamp.startsWith("# ") && dimensions=="320 240" && f.readStringUntil('\n')=="255" &&
                    f.size()-f.position()==frameBytes) {
                    // Keep the displayed image intact if the card is removed mid-read.
                    uint8_t *next=(uint8_t*)heap_caps_malloc(frameBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
                    if (next && f.read(next,frameBytes)==frameBytes) {
                        memcpy(frame,next,frameBytes);
                        stamp.substring(2).toCharArray(received,sizeof(received));
                        loaded=true; cardNumber=n; cardVis=savedVis;
                    }
                    free(next);
                }
            }
        }
        closeSD();
    }
    if (loaded) { haveImage=true; showPage(true); }
    else if (haveImage) showPage(true);
    else { strcpy(notice,"No readable cards. Press OK to listen."); showPage(false); }
}
int swipeDirection() {
    static int origin=0;
    static uint32_t lastContact=0;
    static bool tracking=false, acted=false;
    if (!touchReady) return 0;
    TOUCHINFO points={};
    uint32_t now=millis();
    if (now-lastContact>150) { tracking=false; acted=false; }
    if (touch.getSamples(&points)!=1 || !points.count) return 0;
    int x=799-points.y[0];
    lastContact=now;
    if (!tracking) { origin=x; tracking=true; }
    if (!acted && abs(x-origin)>75) {
        acted=true;
        return x<origin?1:-1;
    }
    return 0;
}
// RIFF/WAVE header, little-endian PCM16 mono. The payload contains precisely
// the samples from the microphone path (no gain, normalization or filtering).
static void le32(uint8_t *p,uint32_t n) {
    for (int i=0;i<4;++i) p[i]=(n>>(8*i))&255;
}
bool saveMicWav(const int16_t *pcm,size_t count,char *path,size_t length) {
    if (!openSD()) return false;
    const char *temp="/DWMICTST.TMP";
    bool saved=false;
    // Use a private staging name. A power interruption cannot leave a final
    // filename pointing to a partly written WAV.
    if (SD.exists(temp)) SD.remove(temp);
    int n=1;
    for (;n<=9999;++n) {
        snprintf(path,length,"/MIC%04d.WAV",n);
        if (!SD.exists(path)) break;
    }
    if (n<=9999 && !SD.exists(temp)) {
        File f=SD.open(temp,"w");
        uint8_t h[44]={};
        memcpy(h,"RIFF",4); le32(h+4,36+count*2); memcpy(h+8,"WAVEfmt ",8);
        le32(h+16,16); h[20]=1; h[22]=1; le32(h+24,Robot36::Rate);
        le32(h+28,Robot36::Rate*2); h[32]=2; h[34]=16;
        memcpy(h+36,"data",4); le32(h+40,count*2);
        bool ok=f && f.write(h,sizeof(h))==sizeof(h);
        for (size_t at=0;ok && at<count;at+=4096) {
            size_t size=std::min(size_t(4096),count-at)*2;
            ok=f.write(reinterpret_cast<const uint8_t*>(pcm+at),size)==size;
        }
        if (f) { f.flush(); ok=ok && !f.getWriteError() && f.size()==44+count*2; f.close(); }
        saved=ok && SD.rename(temp,path);
        if (!saved) SD.remove(temp);
    }
    closeSD(); return saved;
}
void micTestPage(const char *heading) {
    screen.fillScreen(TFT_WHITE);
    screen.setTextColor(TFT_BLACK);
    label("DARKWATCH  /  MIC TEST",25,25,4);
    label(heading,25,105,3);
}
void runMicTest() {
    stopListening();
    haveImage=false; testPictureAvailable=false; cardNumber=0;
    constexpr size_t target=60*Robot36::Rate;
    auto *pcm=static_cast<int16_t*>(heap_caps_malloc(target*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if (!pcm) {
        strcpy(notice,"Mic test needs PSRAM. Press OK."); showPage(false); return;
    }
    micTestPage("Play Speaker Check from the start.");
    label("Recording 60 seconds of microphone audio.",25,200,3);
    label("Leave the buttons alone until the report appears.",25,286,2);
    screen.update();
    size_t count=receiver.record(pcm,target);
    if (!count) {
        free(pcm); strcpy(notice,"Mic test read failed. Press OK."); showPage(false); return;
    }
    micTestPage("Analyzing the microphone recording...");
    screen.update();
    char wav[20]{};
    bool saved=saveMicWav(pcm,count,wav,sizeof(wav));
    decoder.reset();
    ToneCheck tones;
    bool locked=false,complete=false;
    int rows=0;
    for (size_t i=0;i<count;++i) {
        if (i%ToneCheck::Hop==0 && count-i>=ToneCheck::Window) tones.observe(pcm+i);
        if (!complete) {
            auto event=decoder.push(pcm[i]);
            locked|=event==Robot36::Locked;
            rows=std::max(rows,decoder.rows());
            complete=event==Robot36::Complete;
        }
        if ((i&4095)==0) delay(1);
    }
    free(pcm);
    if (complete) { cardVis=decoder.vis(); readClock(); cardNumber=saveCard(); haveImage=true; testPictureAvailable=true; }
    auto result=tones.result();
    char line[80];
    micTestPage("Microphone test result");
    if (result.stableMs>=250)
        snprintf(line,sizeof(line),"Longest tone: %d Hz for %.1f s",result.hz,result.stableMs/1000.0);
    else strcpy(line,"Longest tone: none stable for 0.25 s");
    label(line,25,180,3);
    snprintf(line,sizeof(line),"1900 Hz pilot: %.1f s   Header lock: %s",result.pilotMs/1000.0,locked?"YES":"NO");
    label(line,25,240,2);
    snprintf(line,sizeof(line),"Decoded: %d / 240 rows   Recorded: %.1f s",rows,double(count)/Robot36::Rate);
    label(line,25,285,2);
    snprintf(line,sizeof(line),"WAV: %s",saved?wav:"No SD file saved");
    label(line,25,335,2);
    label(complete?"DOWN  View postcard     OK  Listen":"OK  Listen     UP / DOWN  Album",25,430,2);
    screen.update();
    Serial1.printf("Mic test: %u samples; longest %d Hz (%u ms); pilot %u ms; lock %d; rows %d; WAV %s\n",
        (unsigned)count,result.hz,result.stableMs,result.pilotMs,locked,rows,saved?wav:"unsaved");
}

void setup() {
    writePin(45,true); writePin(46,true); writePin(39,false);
    writePin(sdCS,true); writePin(sdEnable,false);
    pinMode(sdDetect,INPUT_PULLUP);
    for (int k:{okKey,upKey,downKey}) pinMode(k,INPUT_PULLUP);
    Serial1.begin(115200,SERIAL_8N1,44,43);
    if (!(ready=screen.begin())) { Serial1.println("Display init failed"); return; }
    writePin(42,true); delay(250);
    touchReady=(touch.init(3,2,41,21,400000,&Wire)==CT_SUCCESS && touch.sensorType()==CT_TYPE_GT911);
    clockBus=Wire1.begin(1,0,100000);
    startListening();
}
void loop() {
    if (!ready) { delay(100); return; }
    static int held=0;
    int key=digitalRead(okKey)==LOW ? okKey : digitalRead(upKey)==LOW ? upKey : digitalRead(downKey)==LOW ? downKey : 0;
    if (key && !held) {
        if (key==okKey) {
            uint32_t pressed=millis();
            while (digitalRead(okKey)==LOW) delay(10);
            if (millis()-pressed>=1200) runMicTest();
            else startListening();
        } else if (key==downKey && testPictureAvailable) {
            testPictureAvailable=false; showPage(true);
        } else { testPictureAvailable=false; browse(key==upKey?-1:1); }
    }
    held=key;
    if (!listening) {
        if (haveImage && !key) {
            int swipe=swipeDirection();
            if (swipe) browse(swipe);
        }
        delay(20); return;
    }
    if (!receiver.latest(audioStatus)) { delay(15); return; }
    if (audioStatus.state==RxState::Complete) {
        stopListening(); cardVis=audioStatus.vis; readClock(); cardNumber=saveCard(); haveImage=true;
        Serial1.printf("Frame complete; album index %d\n",cardNumber);
        showPage(true); return;
    }
    if (audioStatus.state==RxState::InputError) {
        stopListening(); strcpy(notice,"Mic read failed. Press OK to retry.");
        showPage(false); return;
    }
    if (audioStatus.reports!=lastReport) {
        Serial1.printf("Mic %d dBFS  span=%d  rate=%d  rows=%d  state=%u\n",
            audioStatus.dbfs,audioStatus.peakToPeak,audioStatus.sampleRate,audioStatus.rows,(unsigned)audioStatus.state);
        lastReport=audioStatus.reports;
    }
    int group=audioStatus.rows/40;
    bool changed=audioStatus.state!=lastState || abs(audioStatus.dbfs-lastLevel)>=3 || group!=lastGroup;
    if (audioStatus.reports && changed && millis()-lastPaint>=3000) {
        switch (audioStatus.state) {
            case RxState::Locked:
                snprintf(notice,sizeof(notice),"%s locked   %d / 240 rows",Robot36::modeName(audioStatus.vis),audioStatus.rows); break;
            case RxState::Lost: strcpy(notice,"Sync lost. Replay from the start."); break;
            case RxState::Unsupported:
                snprintf(notice,sizeof(notice),"Other SSTV mode (VIS %d)",audioStatus.vis); break;
            default: strcpy(notice,audioStatus.peakToPeak<=2?"Flat mic input":"Waiting for SSTV header"); break;
        }
        lastLevel=audioStatus.dbfs; lastGroup=group; lastState=audioStatus.state;
        showPage(false);
        lastPaint=millis();
    }
    delay(15);
}
