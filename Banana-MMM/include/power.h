#include <Arduino.h>

#ifndef _POWER_H_
#define _POWER_H_

#define adc_RESOLUTION 10  // max = 2^10 = 1024

enum PowerDevices {
	SOLAR,
	ACCUM
};

extern const uint8_t SOLAR_I_PIN;
extern const uint8_t SOLAR_V_PIN;
extern const uint8_t ACCUM_I_PIN;
extern const uint8_t ACCUM_V_PIN;

extern const double voltage_LIMIT;
extern const double current_LIMIT;

float readVoltage(uint8_t index) {
	switch (index) {
	case ACCUM:
		return 1000 * voltage_LIMIT * (float)analogRead(ACCUM_V_PIN) / pow(2, adc_RESOLUTION);
		break;

	case SOLAR:
		return 1000 * voltage_LIMIT * (float)analogRead(SOLAR_V_PIN) / pow(2, adc_RESOLUTION);
		break;
	}

	return 0.;
}
float readCurrent(uint8_t index) {
	switch (index) {
	case ACCUM:
		return 1000 * current_LIMIT * (float)analogRead(ACCUM_I_PIN) / pow(2, adc_RESOLUTION);
		break;

	case SOLAR:
		return 1000 * current_LIMIT * (float)analogRead(SOLAR_I_PIN) / pow(2, adc_RESOLUTION);
		break;
	}

	return 0.;
}

#endif // _POWER_H_
