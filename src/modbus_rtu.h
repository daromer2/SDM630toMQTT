// Non-blocking Modbus RTU master. One request in flight at a time; poll() from loop().
#pragma once
#include <Arduino.h>

enum MbResult : int {
  MB_PENDING = -1,
  MB_OK = 0,
  // 1..11 = Modbus exception codes from the slave
  MB_TIMEOUT = 0x100,
  MB_BAD_CRC,
  MB_BAD_SLAVE,
  MB_BAD_FUNCTION,
  MB_BAD_LENGTH,
};

class ModbusRtu {
 public:
  void begin(Stream *serial, int8_t dePin, uint16_t timeoutMs);
  // Sends a read request (fc 3 or 4). Returns false if busy.
  bool request(uint8_t slave, uint8_t fc, uint16_t addr, uint16_t qty);
  // Call often. Returns MB_PENDING until done.
  int poll();
  bool busy() const { return _busy; }
  void setTimeout(uint16_t ms) { _timeout = ms; }
  // valid after MB_OK: register data (big endian bytes)
  const uint8_t *data() const { return _buf + 3; }
  uint8_t dataLen() const { return _buf[2]; }

  static const char *errorStr(int code);

 private:
  Stream *_s = nullptr;
  int8_t _de = -1;
  uint16_t _timeout = 500;
  bool _busy = false;
  uint8_t _slave = 0, _fc = 0;
  uint16_t _qty = 0;
  uint32_t _start = 0;
  uint8_t _buf[260];
  uint16_t _len = 0;
};

uint16_t modbusCrc(const uint8_t *data, size_t len);
