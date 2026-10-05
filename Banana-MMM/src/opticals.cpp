#include "opticals.h"

OpticalArray::OpticalArray() {}

OpticalArray::OpticalArray(uint8_t addr[]) {
	for (uint8_t i = 0; i < NUM_OPTICALS; i++)
		this->addr[i] = addr[i];
}
OpticalArray::OpticalArray(const uint8_t addr[]) {
	for (uint8_t i = 0; i < NUM_OPTICALS; i++)
		this->addr[i] = addr[i];
}
OpticalArray::OpticalArray(std::initializer_list<uint8_t> addr) {
	uint8_t i = 0;
	for (auto it = addr.begin(); it != addr.end() && i < NUM_OPTICALS; ++it, ++i)
		this->addr[i] = *it;
}

LightSensor OpticalArray::operator[](uint8_t idx) {
	if (idx >= NUM_OPTICALS)
		return LightSensor();

	return LightSensor(addr[idx]);
}

OpticalArray &OpticalArray::operator=(OpticalArray &other) {
	if (this != &other) {
		for (uint8_t i = 0; i < NUM_OPTICALS; i++)
			this->addr[i] = other.addr[i];
	}
	return *this;
}

OpticalArray &OpticalArray::operator=(std::initializer_list<uint8_t> addr) {
	uint8_t i = 0;
	for (auto it = addr.begin(); it != addr.end() && i < NUM_OPTICALS; ++it, ++i)
		this->addr[i] = *it;

	return *this;
}
OpticalArray &OpticalArray::operator=(uint8_t &addr) {
	for (uint8_t i = 0; i < NUM_OPTICALS; i++)
		this->addr[i] = addr;

	return *this;
}
OpticalArray &OpticalArray::operator=(const uint8_t &addr) {
	for (uint8_t i = 0; i < NUM_OPTICALS; i++)
		this->addr[i] = addr;

	return *this;
}

bool OpticalArray::Init() {
	for (uint8_t i = 0; i < NUM_OPTICALS; i++)
		if (!LightSensor(addr[i]).Init())
			return false;

	return true;
}

uint16_t *OpticalArray::GetRaw() {
	uint16_t *data = new uint16_t[NUM_OPTICALS];

	for (uint8_t i = 0; i < NUM_OPTICALS; i++)
		data[i] = GetRaw(i);

	return data;
}
uint16_t OpticalArray::GetRaw(uint8_t idx) {
	if (idx >= NUM_OPTICALS)
		return -1;

	return LightSensor(addr[idx]).GetLight();
}

float OpticalArray::GetLight(uint8_t idx) const {
	if (idx >= NUM_OPTICALS)
		return -1;

	return lightAverages.get(idx);
}

void OpticalArray::GetLight(float *output) const {
	for (uint8_t i = 0; i < NUM_OPTICALS; i++)
		output[i] = GetLight(i);
}
void OpticalArray::GetLight(uint16_t *output) const {
	for (uint8_t i = 0; i < NUM_OPTICALS; i++)
		output[i] = GetLight(i);
}
