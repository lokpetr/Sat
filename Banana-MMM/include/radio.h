#include <Arduino.h>

#ifndef _RADIO_H
#define _RADIO_H

#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <string.h>
#include <SPI.h>

class CustomRadio : public ELECHOUSE_CC1101 {
public:
	CustomRadio();
	CustomRadio(uint8_t csPin);
	void begin();
	void setBaudRate(uint32_t baudRate);
	void recieve(byte *buffer);

protected:
	const float freq = 433.92;
	const float deviation = 15.87;
};

#endif // _RADIO_H
