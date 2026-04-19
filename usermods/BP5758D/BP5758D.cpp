#include "wled.h"

// Default GPIOs. Override by editing these macros before compile if needed.
#ifndef BP5758D_DATA_PIN
  #define BP5758D_DATA_PIN 5
#endif
#ifndef BP5758D_CLK_PIN
  #define BP5758D_CLK_PIN 4
#endif

// BP5758D channel order used by this usermod.
enum : uint8_t {
  BP_CH_RED = 0,
  BP_CH_GREEN,
  BP_CH_BLUE,
  BP_CH_CW,
  BP_CH_WW,
  BP_CH_COUNT
};

class BP5758DDriver {
  private:
    static const uint8_t _addrCurrent = 0x40;
    static const uint8_t _addrPwmDirect = 0x66; // lower nibble 0110: direct grayscale addressing mode
    static const uint16_t _ackTimeoutUs = 250;

    uint8_t _pinData;
    uint8_t _pinClock;
    uint8_t _currents[BP_CH_COUNT] = {10, 10, 10, 20, 20};
    uint8_t _values[BP_CH_COUNT] = {0, 0, 0, 0, 0};
    bool _online = false;

    inline void sdaLow() {
      pinMode(_pinData, OUTPUT);
      digitalWrite(_pinData, LOW);
    }

    inline void sdaHigh() {
      pinMode(_pinData, INPUT_PULLUP);
    }

    inline void sclLow() {
      pinMode(_pinClock, OUTPUT);
      digitalWrite(_pinClock, LOW);
    }

    inline void sclHigh() {
      pinMode(_pinClock, INPUT_PULLUP);
    }

    inline void i2cDelay() {
      delayMicroseconds(2);
    }

    bool waitAck() {
      sdaHigh();
      i2cDelay();
      sclHigh();

      uint32_t startUs = micros();
      while (digitalRead(_pinData) == HIGH) {
        if ((uint32_t)(micros() - startUs) > _ackTimeoutUs) {
          sclLow();
          return false;
        }
      }

      i2cDelay();
      sclLow();
      return true;
    }

    void startCondition() {
      sdaHigh();
      sclHigh();
      i2cDelay();
      sdaLow();
      i2cDelay();
      sclLow();
    }

    void stopCondition() {
      sdaLow();
      i2cDelay();
      sclHigh();
      i2cDelay();
      sdaHigh();
      i2cDelay();
    }

    bool writeByte(uint8_t value) {
      for (uint8_t i = 0; i < 8; i++) {
        sclLow();
        if (value & 0x80) sdaHigh(); else sdaLow();
        i2cDelay();
        sclHigh();
        i2cDelay();
        value <<= 1;
      }
      sclLow();
      return waitAck();
    }

    static uint8_t clampCurrent(uint8_t ma) {
      if (ma < 5) return 5;
      if (ma > 60) return 60;
      return ma;
    }

    // BP5758D current register uses coarse mA steps (2mA granularity in this mapping).
    // This mapping is intentionally simple and safe for initial bring-up.
    static uint8_t encodeCurrent(uint8_t ma) {
      uint8_t clamped = clampCurrent(ma);
      return (clamped - 4) / 2;
    }

    bool sendFrame(uint8_t command, const uint8_t* data, uint8_t len) {
      startCondition();
      if (!writeByte(command)) {
        stopCondition();
        return false;
      }

      for (uint8_t i = 0; i < len; i++) {
        if (!writeByte(data[i])) {
          stopCondition();
          return false;
        }
      }

      stopCondition();
      return true;
    }

  public:
    BP5758DDriver(uint8_t pinData, uint8_t pinClock)
      : _pinData(pinData), _pinClock(pinClock) {}

    bool begin() {
      sdaHigh();
      sclHigh();
      _online = true;
      return applyCurrent();
    }

    bool applyCurrent() {
      uint8_t payload[BP_CH_COUNT];
      for (uint8_t i = 0; i < BP_CH_COUNT; i++) payload[i] = encodeCurrent(_currents[i]);
      _online = sendFrame(_addrCurrent, payload, BP_CH_COUNT);
      return _online;
    }

    bool setCurrent(uint8_t r, uint8_t g, uint8_t b, uint8_t cw, uint8_t ww) {
      _currents[BP_CH_RED] = r;
      _currents[BP_CH_GREEN] = g;
      _currents[BP_CH_BLUE] = b;
      _currents[BP_CH_CW] = cw;
      _currents[BP_CH_WW] = ww;
      return applyCurrent();
    }

    inline void setChannel(uint8_t channel, uint8_t value) {
      if (channel < BP_CH_COUNT) _values[channel] = value;
    }

    bool update() {
      // BP5758D grayscale registers accept two bytes per channel.
      // Expand 8-bit WLED values to 10-bit-ish space by left shift.
      uint8_t payload[BP_CH_COUNT * 2];
      for (uint8_t i = 0; i < BP_CH_COUNT; i++) {
        uint16_t level = (uint16_t)_values[i] << 2;
        payload[(i * 2)]     = (level >> 8) & 0x03;
        payload[(i * 2) + 1] = level & 0xFF;
      }
      _online = sendFrame(_addrPwmDirect, payload, sizeof(payload));
      return _online;
    }

    inline bool isOnline() const {
      return _online;
    }
};

class BP5758DUsermod : public Usermod {
  private:
    BP5758DDriver _bp = BP5758DDriver(BP5758D_DATA_PIN, BP5758D_CLK_PIN);

  public:
    void setup() override {
      _bp.begin();
      _bp.setCurrent(10, 10, 10, 20, 20);
    }

    void loop() override {
      uint32_t c = strip.getPixelColor(0);

      uint8_t r = ((c >> 16) & 0xFF) * bri / 255;
      uint8_t g = ((c >> 8) & 0xFF) * bri / 255;
      uint8_t b = (c & 0xFF) * bri / 255;
      uint8_t w = ((c >> 24) & 0xFF) * bri / 255;

      uint8_t cct = strip.getMainSegment().currentCCT();
      uint8_t cw = (uint16_t)w * cct / 255;
      uint8_t ww = (uint16_t)w * (255 - cct) / 255;

      _bp.setChannel(BP_CH_RED, r);
      _bp.setChannel(BP_CH_GREEN, g);
      _bp.setChannel(BP_CH_BLUE, b);
      _bp.setChannel(BP_CH_CW, cw);
      _bp.setChannel(BP_CH_WW, ww);
      _bp.update();
    }

    uint16_t getId() override {
      return USERMOD_ID_BP5758D;
    }
};

static BP5758DUsermod bp5758d;
REGISTER_USERMOD(bp5758d);
