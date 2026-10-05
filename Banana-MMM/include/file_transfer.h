#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <Arduino.h>
#include <SD.h>
#include "radio.h"

// -----------------------------------------------------------------------------
// Simple stop-and-wait file transfer protocol over CC1101.
//
// Every packet is sent through the CC1101 in variable-length mode with the
// hardware CRC enabled, so corrupted frames are already dropped by the radio
// before they reach us.  On top of that we add a small header that carries a
// packet type, sequence number and (for file payload) file data.
//
// Wire format (payload bytes written to the TX FIFO, i.e. after the
// length byte that the library prepends):
//
//   START :  [type=1][fileId][sizeLo][sizeMi][sizeHi][sizeHh][filename...'\0']
//   DATA  :  [type=2][seq][data... up to PAYLOAD bytes]
//   ACK   :  [type=3][nextSeq]
//   NACK  :  [type=4][expectedSeq]
//   END   :  [type=5][fileId]
//
// The CC1101 FIFO is 64 bytes; one byte is used by the length field in TX and
// two status bytes are appended in RX, so keep PAYLOAD conservative.
// -----------------------------------------------------------------------------

namespace FileTransfer {

// Packet type identifiers.
enum PktType : uint8_t {
	PKT_START = 0x01,
	PKT_DATA  = 0x02,
	PKT_ACK   = 0x03,
	PKT_NACK  = 0x04,
	PKT_END   = 0x05,
};

// Maximum number of raw payload bytes carried in a DATA packet.
// 1 (type) + 1 (seq) + PAYLOAD <= CC1101 safe size (~58).
static const uint8_t  PAYLOAD      = 48;
static const uint8_t  MAX_DATA_PKT = 2 + PAYLOAD;  // type + seq + data
static const uint8_t  MAX_FILENAME = 30;
static const uint8_t  MAX_RETRIES  = 10;
static const uint16_t ACK_TIMEOUT_MS = 350;     // wait-for-ACK timeout
// Default on-air bit rate used by the sender to size the transmit delay.
// Override with FileSender::setBitrate / FileReceiver::setBitrate if you
// call CustomRadio::setBaudRate() to a different value.
static const uint32_t DEFAULT_BITRATE = 4800;

// -----------------------------------------------------------------------------
// FileSender  -- breaks a file on the SD card into DATA chunks and drives
//                the TX side of the protocol via a supplied CustomRadio.
// -----------------------------------------------------------------------------
class FileSender {
public:
	explicit FileSender(CustomRadio &radio);

	// Call this after radio.setBaudRate() if you use a different bit rate
	// than DEFAULT_BITRATE so the GDO-less SendData() delay is sized right.
	void setBitrate(uint32_t bps) { _bitrate = bps; }

	// Send an entire file from the SD card.  Returns true on success.
	bool sendFile(const char *filename, uint8_t fileId = 0);

	// Send a file from a memory buffer (for telemetry / snapshots not yet on SD).
	bool sendBuffer(const char *filename, uint8_t fileId,
	                const uint8_t *data, uint32_t length);

	// Diagnostics: number of retries / packets used by the last sendFile().
	uint16_t lastRetries() const { return _retries; }
	uint32_t lastPackets() const { return _packets; }

private:
	bool waitAck(uint8_t expectedSeq, uint8_t *rxSeq);
	bool sendPacket(const uint8_t *data, uint8_t len);
	uint16_t txDelayFor(uint8_t payloadLen) const;

	CustomRadio &_radio;
	uint16_t _retries;
	uint32_t _packets;
	uint32_t _bitrate = DEFAULT_BITRATE;
	uint8_t  _rxBuf[64];
};

// -----------------------------------------------------------------------------
// FileReceiver  -- listens on a CustomRadio, reassembles a received file onto
//                  the SD card, and sends ACKs back.
//
// The receiver uses a *different* radio module than the sender in the current
// hardware (radio_rx / radio_tx), but we drive both with the same class: the
// caller tells us which CustomRadio to listen on and which to send ACKs over.
// -----------------------------------------------------------------------------
class FileReceiver {
public:
	explicit FileReceiver(CustomRadio &rxRadio, CustomRadio &txRadio);

	void setBitrate(uint32_t bps) { _bitrate = bps; }

	// Block until a file is received (or timeout).  Returns true when a file
	// was successfully written to the SD card.  outFilename is filled with
	// the local name the file was stored under.
	bool receiveFile(String &outFilename, uint32_t timeoutMs = 30000);

	uint32_t lastReceivedBytes() const { return _bytesReceived; }
	uint8_t  lastFileId()        const { return _fileId; }

private:
	bool     waitPacket(uint8_t type, uint8_t seq, uint32_t timeoutMs);
	bool     sendAck(uint8_t nextSeq);
	void     flushRx();
	uint16_t txDelayFor(uint8_t payloadLen) const;

	CustomRadio &_rx;
	CustomRadio &_tx;
	uint32_t _bitrate = DEFAULT_BITRATE;
	uint8_t  _rxBuf[64];
	uint8_t  _fileId;
	uint32_t _fileSize;
	uint32_t _bytesReceived;
	char     _filename[MAX_FILENAME + 1];
};

} // namespace FileTransfer

#endif // FILE_TRANSFER_H
