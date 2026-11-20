#include "AudioReceiver.h"
#include <cmath>
#include <algorithm>

namespace {
constexpr int MicPower=38, MicClock=19, MicData=20;
}
bool AudioReceiver::openMic() {
    pinMode(MicPower,OUTPUT);
    digitalWrite(MicPower,HIGH);
    delay(80);
    mic.setPinsPdmRx(MicClock,MicData);
    mic.setTimeout(250);
    if (!mic.begin(I2S_MODE_PDM_RX,Robot36::Rate,I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_MONO)) {
        digitalWrite(MicPower,LOW); return false;
    }
    return true;
}
void AudioReceiver::closeMic() {
    mic.end();
    digitalWrite(MicPower,LOW);
}
size_t AudioReceiver::record(int16_t *dest,size_t capacity) {
    stop();
    if (!openMic()) return 0;
    size_t count=0;
    while (count<capacity) {
        size_t n=std::min(size_t(512),capacity-count);
        size_t bytes=mic.readBytes(reinterpret_cast<char*>(dest+count),n*sizeof(int16_t));
        if (!bytes || bytes%sizeof(int16_t)) break;
        count+=bytes/sizeof(int16_t);
    }
    closeMic();
    return count;
}
bool AudioReceiver::start() {
    stop();
    if (!updates) updates=xQueueCreate(1,sizeof(RxStatus));
    if (!finished) finished=xSemaphoreCreateBinary();
    if (!updates || !finished) return false;
    xQueueReset(updates);
    if (!openMic()) return false;
    decoder.reset();
    stopping=false;
    running=xTaskCreatePinnedToCore(entry,"postcard_rx",6144,this,3,nullptr,0)==pdPASS;
    if (!running) closeMic();
    return running;
}
void AudioReceiver::stop() {
    if (!running) return;
    stopping=true;
    xSemaphoreTake(finished,portMAX_DELAY);
    closeMic();
    running=false;
}
bool AudioReceiver::latest(RxStatus &s) const {
    return updates && xQueuePeek(updates,&s,0)==pdTRUE;
}
void AudioReceiver::entry(void *p) {
    auto *self=static_cast<AudioReceiver*>(p);
    self->capture();
    xSemaphoreGive(self->finished);
    vTaskDelete(nullptr);
}
void AudioReceiver::capture() {
    RxStatus report;
    int16_t samples[256];
    int64_t sum=0, squares=0;
    int low=32767, high=-32768, count=0;
    uint32_t previous=millis();
    while (!stopping.load()) {
        size_t bytes=mic.readBytes(reinterpret_cast<char*>(samples),sizeof(samples));
        if (!bytes || bytes%2) {
            report.state=RxState::InputError;
            xQueueOverwrite(updates,&report);
            return;
        }
        bool changed=false;
        for (size_t i=0;i<bytes/sizeof(samples[0]);++i) {
            int v=samples[i];
            low=std::min(low,v); high=std::max(high,v);
            sum+=v; squares+=v*v; ++count;
            auto event=decoder.push(samples[i]);
            if (event==Robot36::None) continue;
            changed=true;
            if (event==Robot36::Locked) report.state=RxState::Locked;
            if (event==Robot36::Lost) report.state=RxState::Lost;
            if (event==Robot36::OtherMode) { report.state=RxState::Unsupported; report.vis=decoder.vis(); }
            if (event==Robot36::Complete) {
                report.state=RxState::Complete;
                report.rows=Robot36::Height;
                xQueueOverwrite(updates,&report);
                return;
            }
        }
        if (count>=Robot36::Rate/2) {
            // Remove DC when measuring level. An input stuck at a rail is flat.
            double mean=double(sum)/count;
            double variance=std::max(0.0,double(squares)/count-mean*mean);
            report.dbfs=std::max(-96,(int)lround(10*log10(std::max(0.27,variance)/(32768.0*32768.0))));
            report.peakToPeak=high-low;
            uint32_t now=millis();
            if (now!=previous) report.sampleRate=(1000UL*count)/(now-previous);
            previous=now;
            ++report.reports;
            sum=squares=0; low=32767; high=-32768; count=0;
            changed=true;
        }
        if (changed) {
            report.rows=decoder.rows();
            xQueueOverwrite(updates,&report);
        }
    }
}
