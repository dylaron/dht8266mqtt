#ifndef SLOPETRACKER_H
#define SLOPETRACKER_H

#define MAX_SAMPLE_SIZE 96

#include <Arduino.h>
#include <CircularBuffer.h>

class SlopeTracker
{
private:
    float step_minute, x[MAX_SAMPLE_SIZE];
    CircularBuffer<float, MAX_SAMPLE_SIZE> y;
    uint8_t n;
    bool _r;
    float sumX, sumX2;

public:
    SlopeTracker(uint8_t _n, float s_m);
    void reset();
    void addPoint(float y_new);
    float getAvg();
    float getSumY();
    float getSlope();
    bool ready();
};

#endif
