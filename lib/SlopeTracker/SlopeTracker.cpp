#include "SlopeTracker.h"

SlopeTracker::SlopeTracker(uint8_t _n, float s_m)
{
    this->step_minute = s_m;
    this->n = min(_n, (uint8_t)MAX_SAMPLE_SIZE);
    this->_r = false;
    this->sumX = 0;
    this->sumX2 = 0;
    for (int i = 0; i < this->n; i++)
    {
        this->x[i] = -step_minute * i;
        sumX = sumX + this->x[i];
        sumX2 = sumX2 + this->x[i] * this->x[i];
    }
}

void SlopeTracker::reset()
{
    this->_r = false;
    this->y.clear();
}

void SlopeTracker::addPoint(float y_new)
{
    this->y.unshift(y_new);
    this->_r = (this->y.available() < (MAX_SAMPLE_SIZE - this->n));
}

float SlopeTracker::getAvg()
{
    return this->getSumY() / this->n;
}

float SlopeTracker::getSumY()
{
    float sumY = 0;
    for (int i = 0; i < this->n; i++)
    {
        sumY = sumY + this->y[i];
    }
    return sumY;
}

float SlopeTracker::getSlope()
{
    float sumY = 0, sumXY = 0, b;
    for (int i = 0; i < this->n; i++)
    {
        sumXY = sumXY + this->x[i] * this->y[i];
    }
    b = (this->n * sumXY - this->sumX * this->getSumY()) / (this->n * this->sumX2 - this->sumX * this->sumX);
    return b;
}

bool SlopeTracker::ready()
{
    return (this->_r);
}