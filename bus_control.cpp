#include "bus_control.h"
#include "engine_control.h"
#include "settings.h"

typedef struct {
  const unsigned char bytes[6];
  const unsigned char responseLen;
  int (*convert)(const unsigned char* response);
} Command;

static unsigned char responseBuf[128];
static unsigned char responseBufIndex;

static const unsigned char initMsg[] = { 0xC1, 0x33, 0xF1, 0x81, 0x66 };
static const unsigned char initMsgLen = sizeof(initMsg);

static const Command cmds[] = {
  {
    { 0xC2, 0x33, 0xF1, 0x01, 0x0C, 0xF3 }, 14, // Обороты двигателя
    [](const unsigned char* response) {
      unsigned short res; 
      auto rpm = (unsigned char*)&res;
      rpm[0] = response[6+6];
      rpm[1] = response[6+5];
      return res / 4;
    }
  },
  { 
    { 0xC2, 0x33, 0xF1, 0x01, 0x05, 0xEC }, 13, // Температура охлаждающей жидкости
    [](const unsigned char* response) { return response[6+5] - 40; } 
  },
  {
    { 0xC2, 0x33, 0xF1, 0x01, 0x0F, 0xF6 }, 13, // Температура воздуха
    [](const unsigned char* response) { return response[6+5] - 40; } 
  },
  {
    { 0xC2, 0x33, 0xF1, 0x01, 0x0D, 0xF4 }, 13, // Скорость
    [](const unsigned char* response) { return (int)response[6+5]; }
  }
};

static const int cmdLen = sizeof(cmds[0].bytes);
static const int cmdsLen = sizeof(cmds) / sizeof(Command);

static BusSensor subs[cmdsLen] = { 0 };
static unsigned char subsCount = 0;
static unsigned char sendingSub;

static BusSensor sendingCmd;
static unsigned char sendingByte;

static int state = 0;
static unsigned long long timer;

static BusEvent e;

#define message(event) BusControlEvent(false, event)
#define error(event)   BusControlEvent(true, event)

bool BusSubscribe(BusSensor sensor) {
  if(sensor >= cmdsLen) {
    e.err = BUS_INCORRECT_SENSOR_ERROR; error(e);
    return false;
  }

  for(int i = 0; i < subsCount; i++) {
    if(subs[i] == sensor) return true;
  }

  subs[subsCount++] = sensor;
  return true;
}

void bus_tick(bool initSignal, bool stopSignal) {
    switch (state)
    {
    case 0:
      if(initSignal)
        state++;
      else if(stopSignal)
        { e.err = BUS_ALREADY_STOPPED; error(e); }

      break;
    case 1:
      if(initSignal)
        { e.err = BUS_INIT_PROGRESS; error(e); }
      else if(stopSignal) {
        e.msg = BUS_STOP_SUCCESS; message(e);
        state = 0;
      }
      else {
        timer = GetTime();
        TxSetOutput();
        TxWriteHigh();
        state++;
      }
      break;
    case 2:
      if(initSignal)
        { e.err = BUS_INIT_PROGRESS; error(e); }
      else if(stopSignal) {
        e.msg = BUS_STOP_SUCCESS; message(e);
        state = 0;
      }
      else if(GetTime() - timer >= 300) {
        TxWriteLow();
        timer = GetTime();
        state++;
      }
      break;
    case 3:
      if(initSignal)
        { e.err = BUS_INIT_PROGRESS; error(e); }
      else if(stopSignal) {
        e.msg = BUS_STOP_SUCCESS; message(e);
        state = 0;
      }
      else if(GetTime() - timer >= 25) {
        TxWriteHigh();
        timer = GetTime();
        state++;
      }

      break;
    case 4:
      if(initSignal)
        { e.err = BUS_INIT_PROGRESS; error(e); }
      else if(stopSignal) {
        e.msg = BUS_STOP_SUCCESS; message(e);
        state = 0;
      }
      else if(GetTime() - timer >= 25) {
        InitUART();
        sendingByte = 0;
        state++;
      }

      break;
    case 5:
      if(initSignal)
        { e.err = BUS_INIT_PROGRESS; error(e); }
      else if(stopSignal) {
        e.msg = BUS_STOP_SUCCESS; message(e);
        state = 0;
      }
      else if(GetTime() - timer >= 10) {
        UARTWriteByte(initMsg[sendingByte++]);
        timer = GetTime();
        if(sendingByte == initMsgLen) {
          responseBufIndex = 0;
          state++;
        }
      }
      break;
    case 6:
      if(initSignal)
        { e.err = BUS_INIT_PROGRESS; error(e); }
      else if(stopSignal) {
        e.msg = BUS_STOP_SUCCESS; message(e);
        state = 0;
      }
      else if(GetTime() - timer >= 100) {
        e.err = BUS_INIT_ERROR; error(e);
        state = 0;
      }
      else if(UARTBytesAvailable()) {
        responseBuf[responseBufIndex++] = UARTReadByte();
        if(responseBufIndex == 12) {
          timer = GetTime();

          if(responseBuf[5+3] == 0xC1) {
            e.msg = BUS_INIT_SUCCESS; message(e);
            sendingSub = 0;
            
            sendingCmd = subsCount ? subs[sendingSub] : 0;
            state++;
          }
          else {
            e.err = BUS_INIT_ERROR; error(e);
            state = 0;
          }
        }
      }

      break;
    case 7:
      if(initSignal)
        { e.err = BUS_ALREADY_INIT; error(e); }
      else if(stopSignal) {
        e.msg = BUS_STOP_SUCCESS; message(e);
        state = 0;
      }
      else if(GetTime() - timer >= 100) {
        sendingByte = 0;
        state++;
      }
      break;
    case 8:
      if(initSignal)
        { e.err = BUS_ALREADY_INIT; error(e); }
      else if(stopSignal) {
        e.msg = BUS_STOP_SUCCESS; message(e);
        state = 0;
      }
      else if(GetTime() - timer >= 10) {
        UARTWriteByte(cmds[sendingCmd].bytes[sendingByte++]);
        timer = GetTime();
        if(sendingByte == 6) {
          responseBufIndex = 0;
          state++;
        }
      }
      break;
    case 9:
      if(initSignal)
        { e.err = BUS_ALREADY_INIT; error(e); }
      else if(stopSignal) {
        e.msg = BUS_STOP_SUCCESS; message(e);
        state = 0;
      }
      else if(GetTime() - timer > 60) {
        // Чистим входной буфер
        UARTReadBytes(responseBuf, UARTBytesAvailable());
        sendingCmd = 0;
        sendingByte = 0;
        state = 8;
      }
      else if(UARTBytesAvailable()) {
        responseBuf[responseBufIndex++] = UARTReadByte();
        timer = GetTime();

        if(responseBufIndex == cmds[sendingCmd].responseLen) {
          int value = cmds[sendingCmd].convert(responseBuf);

          if(subsCount && BusSensorValue(sendingCmd, value)) {
            for (int i = sendingSub + 1; i < subsCount; i++)
              subs[i-1] = subs[i];
            subsCount--;
          }

          if(subsCount) {
            if(++sendingSub >= subsCount)
              sendingSub = 0;
            sendingCmd = subs[sendingSub];
          }
          else
            sendingCmd = 0;

          state = 7;
        }
        
      }
      break;
    }
}