#include <Arduino.h>

#ifndef _TELEMETRY_WRITER_H
#define _TELEMETRY_WRITER_H

#include "xyz_data.h"
#include <SD.h>

struct telemetry {
	uint16_t light_sensors[4]; // light_AMOUNT = 4
	float accum_voltage, accum_current;
	float solar_voltage, solar_current;
	xyz_data<float> g; // gyroscope
	xyz_data<float> m; // compas
	float flywheel_speed;
	float coil_rate;

	String toString(String delimiter);
	uint8_t *toBytes();
};

#endif
