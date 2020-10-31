#include "bus_control.h"
#include "engine_control.h"
#include "settings.h"

#define IDLE                           0
#define FIRST_HIGH 		                 1
#define LOW 	                         2
#define SECOND_HIGH 		               3
#define UART_INIT 		                 4
#define START_COMMUNICATION 		       5
#define START_COMMUNICATION_RESPONSE	 6
#define RESPONSE_DELAY		             7
#define SENSOR_REQUEST 		             8
#define SENSOR_RESPONSE	  	           9
#define STOP_COMMUNICATION	          10
#define STOP_COMMUNICATION_RESPONSE	  11

typedef struct {
  const unsigned char bytes[6];
  const unsigned char responseLen;
  int (*convert)(const unsigned char* response);
} Command;

static unsigned char responseBuf[RX_BUFFER_SIZE];
static unsigned char responseBufIndex;

static const unsigned char initMsg[] = { 0xC1, 0x33, 0xF1, 0x81, 0x66 };
static const unsigned char initMsgLen = sizeof(initMsg);

static const unsigned char stopMsg[] = { 0xC1, 0x33, 0xF1, 0x82, 0x67 };
static const unsigned char stopMsgLen = sizeof(stopMsg);

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

static int state = IDLE;
static unsigned long long timer;

static BusEvent e;

static bool stopping = false;
static int avail;

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

inline void uartClean() {
  avail = UARTBytesAvailable();
  while(avail > 0) {
    // Чистим входной буфер
    avail -= UARTReadBytes(responseBuf, avail < RX_BUFFER_SIZE ? avail : RX_BUFFER_SIZE);
  }
}

void bus_tick(bool initSignal, bool stopSignal) {
    switch (state)
    {
    case IDLE:
      if(stopSignal)
        { e.err = BUS_ALREADY_STOPPED; error(e); }

      if(initSignal)
        state = FIRST_HIGH;

      break;
    case FIRST_HIGH:
      if(initSignal)
        { e.err = BUS_INIT_PROGRESS; error(e); }
      
      if(stopSignal) {
        state = IDLE;
        e.msg = BUS_STOP_SUCCESS; message(e);
      }
      else {
        timer = GetTime();
        TxSetOutput();
        TxWriteHigh();
        state = LOW;
      }
      break;
    case LOW:
      if(initSignal)
        { e.err = BUS_INIT_PROGRESS; error(e); }
      
      if(stopSignal) {
        state = IDLE;
        e.msg = BUS_STOP_SUCCESS; message(e);
      }
      else if(GetTime() - timer >= 300) {
        TxWriteLow();
        timer = GetTime();
        state = SECOND_HIGH;
      }
      break;
    case SECOND_HIGH:
      if(initSignal)
        { e.err = BUS_INIT_PROGRESS; error(e); }

      if(stopSignal) {
        state = IDLE;
        e.msg = BUS_STOP_SUCCESS; message(e);
      }
      else if(GetTime() - timer >= 25) {
        TxWriteHigh();
        timer = GetTime();
        state = UART_INIT;
      }

      break;
    case UART_INIT:
      if(initSignal)
        { e.err = BUS_INIT_PROGRESS; error(e); }

      if(stopSignal) {
        state = IDLE;
        e.msg = BUS_STOP_SUCCESS; message(e);
      }
      else if(GetTime() - timer >= 25) {
        InitUART();
        sendingByte = 0;
        state = START_COMMUNICATION;
      }

      break;
    case START_COMMUNICATION:
      if(initSignal)
        { e.err = BUS_INIT_PROGRESS; error(e); }
      
      if(stopSignal) {
        state = IDLE;
        e.msg = BUS_STOP_SUCCESS; message(e);
      }
      else if(GetTime() - timer >= 10) {
        UARTWriteByte(initMsg[sendingByte++]);
        timer = GetTime();
        if(sendingByte == initMsgLen) {
          uartClean();
          responseBufIndex = 0;
          state = START_COMMUNICATION_RESPONSE;
        }
      }
      break;
    case START_COMMUNICATION_RESPONSE:
      if(initSignal)
        { e.err = BUS_INIT_PROGRESS; error(e); }
      
      if(stopSignal) {
        stopping = true;
      }

      if(GetTime() - timer > 100) {
        state = IDLE;
        e.err = BUS_INIT_ERROR; error(e);
        if(stopping) {
          stopping = false;
          e.msg = BUS_STOP_SUCCESS; message(e);
        }
      }
      else if(UARTBytesAvailable()) {
        responseBuf[responseBufIndex++] = UARTReadByte();
        if(responseBufIndex == 12) {
          timer = GetTime();

          if(responseBuf[5+3] == 0xC1) {
            state = RESPONSE_DELAY;
            if(!stopping) {
              e.msg = BUS_INIT_SUCCESS; message(e);
              sendingSub = 0;
              
              sendingCmd = subsCount ? subs[sendingSub] : 0;
            }
          }
          else {
            stopping = false;
            state = IDLE;
            e.err = BUS_INIT_ERROR; error(e);
          }
        }
      }

      break;
    case RESPONSE_DELAY:
      if(initSignal)
        { e.err = BUS_ALREADY_INIT; error(e); }
      
      if(stopSignal) {
        stopping = true;
      }

      if(GetTime() - timer >= 100) {
        sendingByte = 0;
        if(stopping)
          state = STOP_COMMUNICATION;
        else
          state = SENSOR_REQUEST;
      }
      break;
    case SENSOR_REQUEST:
      if(initSignal)
        { e.err = BUS_ALREADY_INIT; error(e); }
      
      if(stopSignal) {
        stopping = true;
      }

      if(GetTime() - timer >= 10) {
        UARTWriteByte(cmds[sendingCmd].bytes[sendingByte++]);
        timer = GetTime();
        if(sendingByte == 6) {
          uartClean();
          responseBufIndex = 0;
          state = SENSOR_RESPONSE;
        }
      }
      break;
    case SENSOR_RESPONSE:
      if(initSignal)
        { e.err = BUS_ALREADY_INIT; error(e); }
      
      if(stopSignal) {
        stopping = true;
      }
      
      if(GetTime() - timer > 60) {
        sendingByte = 0;
        if(stopping)
          state = STOP_COMMUNICATION;
        else
          state = SENSOR_REQUEST;
      }
      else if(UARTBytesAvailable()) {
        responseBuf[responseBufIndex++] = UARTReadByte();
        timer = GetTime();
        
        if(responseBufIndex == cmds[sendingCmd].responseLen) {
          if(!stopping) {
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
          }

          state = RESPONSE_DELAY;
        }
        
      }
      break;

    case STOP_COMMUNICATION:
      if(initSignal) {
        e.err = BUS_INIT_STOP_PROGRESS; error(e);
      }
      if(stopSignal) {
        e.err = BUS_STOP_PROGRESS; error(e);
      }

      if(GetTime() - timer >= 10) {
        UARTWriteByte(stopMsg[sendingByte++]);
        timer = GetTime();
        if(sendingByte == stopMsgLen) {
          responseBufIndex = 0;
          state = STOP_COMMUNICATION_RESPONSE;
        }
      }
      break;
    case STOP_COMMUNICATION_RESPONSE:
      if(initSignal) {
        e.err = BUS_INIT_STOP_PROGRESS; error(e);
      }
      if(stopSignal) {
        e.err = BUS_STOP_PROGRESS; error(e);
      }

      if(GetTime() - timer > 100) {
        stopping = false;
        state = IDLE;
        e.err = BUS_STOP_ERROR; error(e);
      }
      else if(UARTBytesAvailable()) {
        responseBuf[responseBufIndex++] = UARTReadByte();
        if(responseBufIndex == 12) {
          timer = GetTime();
          stopping = false;
          
          if(responseBuf[5+3] == 0xC2) {
            state = IDLE;
            e.msg = BUS_STOP_SUCCESS; message(e);
          }
          else {
            state = IDLE;
            e.err = BUS_STOP_ERROR; error(e);
          }
        }
      }

      break;
    }
}