#ifndef PCCWDCONTROLLER_H_
#define PCCWDCONTROLLER_H_
#include <Arduino.h>

//PCCWD + IN10Wd + OF8Wd

byte PREAMBLE = 0xAA;
byte ONCMD = 0x64;
byte OFFCMD = 0x01;

#include <RingBufCPP.h>
#include <RingBufHelpers.h>

enum PressType {
  SINGLE_PRESS = 0,
  DOUBLE_PRESS = 1,
  LONG_PRESS = 2
};


typedef std::function<void(byte id, PressType type)> ProgrammableSwitchEvent_callback;
typedef std::function<void(byte id)> On_callback;

class PCCWDController {
    RingBufCPP<byte, 200> SerialBuf;
    byte SerialInput[10];
    int SerialInputIndex = 0;
    bool isPCCWDConnected = false;

    Stream *_serial;
    const static  unsigned long pingInterval =  3000UL;
    unsigned long lastPingTime = 0 - pingInterval;

    byte pingPccwdBytes[3] = {0x57, 0x01, 0x64};

    byte initPccwdbytes[54] = {
      0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x8b,
      0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x8c,
      0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x8d,
      0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x8e,
      0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x8f,
      0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x90,
      0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x91,
      0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x92,
      0xaa, 0x02, 0xc9, 0xaa, 0x02, 0xc9
    };

    ProgrammableSwitchEvent_callback _programmableSwitchEvent_callback;
    On_callback _on_callback;
  public:

    PCCWDController(Stream* stream): _serial(stream) {}
    PCCWDController(Stream& stream): _serial(&stream) {}

    void setProgrammableSwitchEvent_callback(ProgrammableSwitchEvent_callback callback) {
      _programmableSwitchEvent_callback = callback;
    }

    void setOn_callback(On_callback callback) {
      _on_callback = callback;
    }

    void setOnSate(byte address, bool newState) {
      if (isPCCWDConnected) {
        SerialBuf.add(PREAMBLE);
        SerialBuf.add(address);
        SerialBuf.add((newState) ? ONCMD : OFFCMD);
      }
    }

    void loop() {
      unsigned long now = millis();
      //ping PCCWD if not connected
      if (!isPCCWDConnected && (now - lastPingTime >= pingInterval)) {
        lastPingTime = now;
        for (int i = 0; i < sizeof pingPccwdBytes; i++) {
          SerialBuf.add(pingPccwdBytes[i]);
        }
      }

      //send commands to PCCWD
      while (!SerialBuf.isEmpty()) {
        byte pulled;
        SerialBuf.pull(&pulled);
        _serial->write(pulled);
        delay(2);
      }

      //read button events and initialize pccwd
      while (_serial->available()) {
        // get the new byte:
        byte inChar = _serial->read();
        // add it to the inputString:
        SerialInput[SerialInputIndex++] = inChar;
        if (inChar == 0x46) {
          SerialInputIndex = 0;
          //parse string and reset
          //sprintf(chunk, "pccwd inputString: %02x,%02x,%02x,%02x", SerialInput[0], SerialInput[1], SerialInput[2], SerialInput[3]);
          //Log(chunk);
          if (SerialInput[0] == 0x49 && SerialInput[1] == 0xaa) {
            if (SerialInput[2] == 0x01 && SerialInput[3] == 0x67) {

              LOG_D("Initializing PCCWD");
              for (int i = 0; i < sizeof pingPccwdBytes; i++) {
                SerialBuf.add(pingPccwdBytes[i]);
              }
              for (int i = 0; i < sizeof initPccwdbytes; i++) {
                SerialBuf.add(initPccwdbytes[i]);
              }
              isPCCWDConnected = true;
              LOG_D("PCCWD initialized");
            } else {
              int address = SerialInput[2];
              int presses = SerialInput[3] - 1;

              if (_programmableSwitchEvent_callback)
                _programmableSwitchEvent_callback(address, (PressType) presses);

              //Proces built in connections
              //byte bulbAddress = PccwdAccessories[address][1];

              //if (bulbAddress > 0 && presses == 0) {
              //                PccwdAccessories[bulbAddress] [1] = (PccwdAccessories[bulbAddress][1] == 0) ? 1 : 0;
              //                getAccessory(bulbId.c_str(), ServiceName.c_str(), "On");
              //}
            }
          }
        }
      }
    }

  private:
};

#endif
