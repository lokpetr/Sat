#include "telemetry.h"
#include "xyz_data.h"
#include <SD.h>

extern const double voltage_LIMIT;
extern const double current_LIMIT;

String telemetry::toString(String delimiter) {
	String packet;

	uint32_t _time = millis();

	_time %= 1000 * 3600 * 24;

	// uint8_t _hour = _time / 1000 / 3600;
	// uint8_t _minutes = _time / 1000 / 60;
	// uint8_t _seconds = _time / 1000 / 60 % 60;
	// uint16_t _millis = _time % 1000;

	packet += String(_time);

	for (uint8_t i = 0; i < 4; i++)
		packet += delimiter + this->light_sensors[i];

	packet += delimiter + this->accum_voltage + delimiter + this->accum_current;
	packet += delimiter + this->solar_voltage + delimiter + this->solar_current;

	packet += delimiter + this->g.x + delimiter + this->g.y + delimiter + this->g.z;
	packet += delimiter + this->m.x + delimiter + this->m.y + delimiter + this->m.z;

	return packet;
}

uint8_t *telemetry::toBytes() {
	uint8_t *packet = new uint8_t[21];

	uint8_t accum_voltage_converted = map(this->accum_voltage, 0, voltage_LIMIT, 0, 255);
	uint8_t accum_current_converted = map(this->accum_current, 0, current_LIMIT, 0, 255);
	uint8_t solar_voltage_converted = map(this->solar_voltage, 0, voltage_LIMIT, 0, 255);
	uint8_t solar_current_converted = map(this->solar_current, 0, current_LIMIT, 0, 255);

	uint8_t gx = map(this->g.x, -32768, +32767, 0, 255);
	uint8_t gy = map(this->g.y, -32768, +32767, 0, 255);
	uint8_t gz = map(this->g.z, -32768, +32767, 0, 255);

	uint8_t mx = map(this->m.x, -32768, +32767, 0, 255);
	uint8_t my = map(this->m.y, -32768, +32767, 0, 255);
	uint8_t mz = map(this->m.z, -32768, +32767, 0, 255);

	uint8_t flywheel_speed = map(this->flywheel_speed, -32768, +32767, 0, 255);
	uint8_t coil_rate = map(this->coil_rate, -32768, +32767, 0, 255);

	packet[0] = (0b01000000) | (this->light_sensors[0] >> 13);
	packet[1] = ((this->light_sensors[0] >> 2) & 0xff);
	packet[2] = ((this->light_sensors[0] << 6) & 0xff) | (this->light_sensors[1] >> 13);
	packet[3] = ((this->light_sensors[1] >> 2) & 0xff);
	packet[4] = ((this->light_sensors[1] << 6) & 0xff) | (this->light_sensors[2] >> 13);
	packet[5] = ((this->light_sensors[2] >> 2) & 0xff);
	packet[6] = ((this->light_sensors[2] << 6) & 0xff) | (this->light_sensors[3] >> 13);
	packet[7] = ((this->light_sensors[3] >> 2) & 0xff);
	packet[8] = ((this->light_sensors[3] << 6) & 0xff) | (accum_voltage_converted >> 2);
	packet[9] = ((accum_voltage_converted << 6) & 0xff) | (accum_current_converted >> 2);
	packet[10] = ((accum_current_converted << 6) & 0xff) | (solar_voltage_converted >> 2);
	packet[11] = ((solar_voltage_converted << 6) & 0xff) | (solar_current_converted >> 2);
	packet[12] = ((solar_current_converted << 6) & 0xff) | (gx >> 2);
	packet[13] = ((gx << 6) & 0xff) | (gy >> 2);
	packet[14] = ((gy << 6) & 0xff) | (gz >> 2);
	packet[15] = ((gz << 6) & 0xff) | (mx >> 2);
	packet[16] = ((mx << 6) & 0xff) | (my >> 2);
	packet[17] = ((my << 6) & 0xff) | (mz >> 2);
	packet[18] = ((mz << 6) & 0xff) | (flywheel_speed >> 2);
	packet[19] = ((flywheel_speed << 6) & 0xff) | (coil_rate >> 2);
	packet[20] = ((coil_rate << 6) & 0xff);

	return packet;

	/*

	fucking fuck
		---
	   / | \
	   |---|
	   |   |
	   |   |
	   |   |
	 ---   ---
	|   | |   |
	 ---   ---
	*/
}
