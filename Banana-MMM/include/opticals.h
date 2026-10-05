#include <Arduino.h>
#include <LightSensor.h>

#ifndef _OPTICALS_H
#define _OPTICALS_H

#include "slidingAverage.h"

using namespace IntroSatLib;

class OpticalArray {
public:
	static const uint8_t NUM_OPTICALS = 4;
	OpticalArray();
	OpticalArray(uint8_t addr[]);
	OpticalArray(const uint8_t addr[]);
	OpticalArray(std::initializer_list<uint8_t> addr);
	LightSensor operator[](uint8_t idx);
	OpticalArray &operator=(OpticalArray &other);
	OpticalArray &operator=(std::initializer_list<uint8_t> addr);
	OpticalArray &operator=(uint8_t &addr);
	OpticalArray &operator=(const uint8_t &addr);
	bool Init();
	float GetLight(uint8_t idx) const;
	void GetLight(float *output) const;
	void GetLight(uint16_t *output) const;

private:
	uint16_t *GetRaw();
	uint16_t GetRaw(uint8_t idx);
	SlidingAverageND<5, NUM_OPTICALS> lightAverages;

	uint8_t addr[NUM_OPTICALS] = {0x50, 0x51, 0x52, 0x53};
};

#endif // _OPTICALS_H
