#ifndef PCCWDCONTROLLER_H_
#define PCCWDCONTROLLER_H_
#include <Arduino.h>
#include "freertos/ringbuf.h"


#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__)


//PCCWD + IN10Wd + OF8Wd

#define IN10Wd_ADDRESS 75
#define OF8Wd_ADDRESS 167

byte PREAMBLE = 0xAA;
byte ONCMD = 0x64;
byte OFFCMD = 0x01;



enum PressType {
  SINGLE_PRESS = 0,
  DOUBLE_PRESS = 1,
  LONG_PRESS = 2
};


typedef std::function<void(byte id, PressType type)> ProgrammableSwitchEvent_callback;


class PCCWDController {
    
    RingbufHandle_t buf_handle;


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

  public:

    PCCWDController(Stream* stream): _serial(stream) {
      buf_handle = xRingbufferCreate(1028, RINGBUF_TYPE_NOSPLIT);
      if (buf_handle == NULL) {
        LOG_D("Failed to create ring buffer\n");
      }
    }
    PCCWDController(Stream& stream): _serial(&stream) {
      buf_handle = xRingbufferCreate(1028, RINGBUF_TYPE_NOSPLIT);
      if (buf_handle == NULL) {
        LOG_D("Failed to create ring buffer\n");
      }
    }

    void setProgrammableSwitchEvent_callback(ProgrammableSwitchEvent_callback callback) {
      _programmableSwitchEvent_callback = callback;
    }

    void setOnSate(byte address, bool newState) {
      if (isPCCWDConnected) {
        LOG_D("updating lightbulb at address: %u to state: %u", address, newState);
        byte cmd[3]={PREAMBLE,address,(newState) ? ONCMD : OFFCMD};
        UBaseType_t res =  xRingbufferSend(buf_handle, cmd, sizeof(cmd), pdMS_TO_TICKS(1000));
        if (res != pdTRUE) {
          LOG_D("Failed to send item\n");
        }
       
      }
    }

    void loop() {
      unsigned long now = millis();
      //ping PCCWD if not connected
      if (!isPCCWDConnected && (now - lastPingTime >= pingInterval)) {
        lastPingTime = now;
        UBaseType_t res =  xRingbufferSend(buf_handle, pingPccwdBytes, sizeof(pingPccwdBytes), pdMS_TO_TICKS(1000));
        if (res != pdTRUE) {
          LOG_D("Failed to PING PCCWD\n");
        }
      }

      //send commands to PCCWD
     
      byte *item ;
      do {
        size_t item_size;
        item = (byte *)xRingbufferReceive(buf_handle, &item_size, pdMS_TO_TICKS(1000));
        
        //Check received item
        if (item != NULL) {
          for (int i = 0; i < item_size; i++) {
            _serial->write(item[i]);
            delay(20); //PCCWD is really slow and connot cope with 14400 event when is set to it...
          }  
          vRingbufferReturnItem(buf_handle, (void *)item);
        }
      } while (item != NULL);



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
              UBaseType_t res =  xRingbufferSend(buf_handle, pingPccwdBytes, sizeof(pingPccwdBytes), pdMS_TO_TICKS(1000));
              if (res != pdTRUE) {
                LOG_D("Failed to INIT PCCWD 1\n");
              }
              res =  xRingbufferSend(buf_handle, initPccwdbytes, sizeof(initPccwdbytes), pdMS_TO_TICKS(1000));
              if (res != pdTRUE) {
                LOG_D("Failed to INIT PCCWD 2\n");
              }
           
              isPCCWDConnected = true;
              LOG_D("PCCWD initialized");
            } else {
              int address = SerialInput[2];
              int presses = SerialInput[3] - 1;

              if (_programmableSwitchEvent_callback)
                _programmableSwitchEvent_callback(address, (PressType) presses);


            }
          }
        }
      }
    }

  private:
};

#endif
