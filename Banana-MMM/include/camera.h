#include <Arduino.h>

#ifndef _CAMERA_H
#define _CAMERA_H

#define CAMERA_WIDTH 160
#define CAMERA_HEIGHT 120

template <uint16_t width, uint16_t height>
class CustomCamera {
public:
	static constexpr int frame_size = width * height;
	CustomCamera(HardwareSerial ser, uint32_t baud_rate = 115200);
	void waitFrame();
	void readLine(byte *_buffer);
	uint32_t getFrameNum();

protected:
	uint32_t frame_num = 0;
	uint32_t baud_rate;
	byte buffer[width];
	HardwareSerial ser;
};

#endif // _CAMERA_H
