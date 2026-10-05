#include <Arduino.h>
#include <SD.h>

#ifndef _SD_H
#define _SD_H

class CameraWriter {
private:
	byte packet[200];
	uint8_t pin;

public:
	uint16_t width, height, frame_num;
	CameraWriter(uint16_t width, uint16_t height);
	CameraWriter(uint8_t pin, uint16_t width, uint16_t height);
	String findNextFileName();
	bool appendImage(String filename, uint8_t *data);
	byte *toBytes(uint8_t *data);
};

#endif // _SD_H
