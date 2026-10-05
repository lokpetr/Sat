#include <Arduino.h>

#ifndef _SLIDING_AVERAGE_H_
#define _SLIDING_AVERAGE_H_

#include "xyz_data.h"

template <int N>
class SlidingAverage {
private:
	float samples[N];
	int index;
	int count;

public:
	SlidingAverage();
	void update(float _new);
	float get() const;
	float calculate(float _new);
};

template <int N>
class SlidingAverage3D {
private:
	SlidingAverage<N> xAverage;
	SlidingAverage<N> yAverage;
	SlidingAverage<N> zAverage;

public:
	SlidingAverage3D();
	void update(float newX, float newY, float newZ);
	void update(xyz_data<float> _new);
	float getX() const;
	float getY() const;
	float getZ() const;
	xyz_data<float> get() const;
	xyz_data<float> calculate(xyz_data<float> _new);
	xyz_data<float> calculate(float newX, float newY, float newZ);
};

template <int N, int D>
class SlidingAverageND {
private:
	SlidingAverage<N> averages[D];

public:
	SlidingAverageND();
	void update(float newValues[D]);
	void calculate(float newValues[D], float output[D]);
	void get(float output[D]) const;
	float get(uint8_t idx) const;
};

#endif
