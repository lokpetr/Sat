#include <ELECHOUSE_CC1101_SRC_DRV.h>

#include "radio.h"

CustomRadio::CustomRadio() {}

CustomRadio::CustomRadio(uint8_t csPin) {
	this->setSSPin(csPin);
}

void CustomRadio::begin() {
	this->Init();
	this->setCCMode(1);
	this->setModulation(0);
	this->setMHZ(freq);
	this->setSyncMode(2);
	this->setCrc(true);
};

void CustomRadio::setBaudRate(uint32_t baudRate) {
	this->setDRate(baudRate / 1000.);
}

void CustomRadio::recieve(byte *buffer) {
	if (this->CheckRxFifo(0))
		if (this->CheckCRC()) {
			uint8_t len = this->ReceiveData(buffer);
			buffer[len] = '\0';
		}
}
