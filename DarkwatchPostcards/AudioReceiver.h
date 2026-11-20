#pragma once
#include <Arduino.h>
#include <ESP_I2S.h>
#include <atomic>
#include "Robot36.h"

enum class RxState { Waiting, Locked, Lost, Unsupported, Complete, InputError };
struct RxStatus {
    RxState state=RxState::Waiting;
    int peakToPeak=0, dbfs=-96, rows=0, vis=0, sampleRate=0;
    unsigned reports=0;
};

// The worker owns the decoder and frame until stop() has joined it.
// A one-element queue publishes snapshots without blocking the microphone on e-paper.
class AudioReceiver {
public:
    explicit AudioReceiver(Robot36 &r): decoder(r) {}
    bool start();
    // Called only by the explicit mic test. Normal reception never stores audio.
    size_t record(int16_t *dest,size_t capacity);
    void stop();
    bool latest(RxStatus &status) const;
private:
    static void entry(void *context);
    void capture();
    bool openMic();
    void closeMic();
    Robot36 &decoder;
    I2SClass mic;
    QueueHandle_t updates=nullptr;
    SemaphoreHandle_t finished=nullptr;
    std::atomic_bool stopping{false};
    bool running=false;
};
