#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

#include <algorithm> // std::min

#include "settings.h"
#include "io.h"


#ifndef STASSID
#define STASSID "SevenEye"
#define STAPSK  "DiTaKsA712892"
#endif

#define BAUD_SERIAL 10400
#define BAUD_LOGGER 115200
#define RXBUFFERSIZE 1024

////////////////////////////////////////////////////////////

#define logger (&Serial1)

#define EN_PIN 5
#define TX_PIN 1
#define STARTER_PIN 5
#define IGNITION_PIN 6

#define DATA_BUF_SIZE  1024 // bytes

//how many clients should be able to telnet to this ESP8266
#define MAX_SRV_CLIENTS 1
const char* ssid = STASSID;
const char* password = STAPSK;

const int port = 23;

WiFiServer server(port);
WiFiClient serverClients[MAX_SRV_CLIENTS];

bool initialized = false;
char cmd = 0;
int state;
unsigned long timer;
uint8_t responseBuf[128];
uint8_t responseBufIndex;

const uint8_t initMsg[] = { 0xC1, 0x33, 0xF1, 0x81, 0x66 };
const uint8_t initMsgLen = sizeof(initMsg);

typedef struct {
  const uint8_t bytes[6];
  const uint8_t responseLen;
  int (*convert)(const uint8_t* response);

} Command;

const Command cmds[] = {
  {
    { 0xC2, 0x33, 0xF1, 0x01, 0x0C, 0xF3 }, 14, // Обороты двигателя
    [](const uint8_t* response) {
      uint16_t res; 
      auto rpm = (uint8_t*)&res;
      rpm[0] = response[6+6];
      rpm[1] = response[6+5];
      return res / 4;
    }
  },
  // { 
  //   { 0xC2, 0x33, 0xF1, 0x01, 0x05, 0xEC }, 13, // Температура охлаждающей жидкости
  //   [](const uint8_t* response) { return response[6+5] - 40; } 
  // },
  // {
  //   { 0xC2, 0x33, 0xF1, 0x01, 0x0F, 0xF6 }, 13, // Температура воздуха
  //   [](const uint8_t* response) { return response[6+5] - 40; } 
  // },
  // {
  //   { 0xC2, 0x33, 0xF1, 0x01, 0x0D, 0xF4 }, 13, // Скорость
  //   [](const uint8_t* response) { return (int)response[6+5]; }
  // }
};

const int cmdLen = sizeof(cmds[0].bytes);
const int cmdsLen = sizeof(cmds) / sizeof(Command);

uint8_t sendingCmd;
uint8_t sendingByte;

uint16_t sensors[cmdsLen];
int currentSensor;

/**
	const char* key
	EngineEventHandler callback
*/
#define EngineStart(key, callback) EngineCmd(CMD_START_ENGINE, key, callback)

bool StartEngineCallback(HCMD callId, EngineCommand cmd, EngineConnectorResult res, EngineEvent event) {
  if(serverClients[0].available()) {
    if (res == ENGINE_MESSAGE)
      serverClients[0].printf("%d: ENGINE MESSAGE: %d\n", callId, event.msg);
    else if (res == ENGINE_ERROR)
      serverClients[0].printf("%d: ENGINE ERROR: %d\n", callId, event.err);
  }

	return false;
}

int GetTime() {
	return millis();
}

bool KeyCompare(const char* key1, const char* key2, int len) {

	char notOk = 0;
	for (int i = 0; i < len; i++)
		notOk |= key1[i] ^ key2[i];

	return !notOk;
}

void SetIgnitionFlag(bool val) {
	digitalWrite(IGNITION_PIN, val ? HIGH : LOW);
}

void SetStarterFlag(bool val) {
	digitalWrite(STARTER_PIN, val ? HIGH : LOW);
}

int GetRPM() {
	return sensors[0];
}

uint8_t ecuTick() {
    switch (state)
    {
    case 0:
        serverClients[0].println("Initializing...");
        timer = millis();
        pinMode(TX_PIN, OUTPUT);
        digitalWrite(TX_PIN, HIGH);
        state++;
        break;
    case 1:
        if(millis() - timer >= 300) {
            digitalWrite(TX_PIN, LOW);
            timer = millis();
            state++;
        }
        break;
    case 2:
        if(millis() - timer >= 25) {
            digitalWrite(TX_PIN, HIGH);
            timer = millis();
            state++;
        }

        break;
    case 3:
        if(millis() - timer >= 25) {
            Serial.begin(BAUD_SERIAL);
            Serial.setRxBufferSize(RXBUFFERSIZE);
            sendingByte = 0;
            state++;
        }

        break;
    case 4:
        if(millis() - timer >= 10) {
            Serial.write(initMsg[sendingByte++]);
            Serial.flush();
            timer = millis();
            if(sendingByte == initMsgLen) {
              responseBufIndex = 0;
              state++;
            }
        }
        break;
    case 5:
        if(Serial.available()) {
          responseBuf[responseBufIndex++] = Serial.read();
          if(responseBufIndex == 12) {
            timer = millis();

            if(responseBuf[5+3] == 0xC1) {
              serverClients[0].println("INIT Success! Starting command loop...");
            }
            else {
              serverClients[0].println("INIT Failed! Aborting connection...");
              return 1;
            }
            sendingCmd = 0;
            currentSensor = 0;
            state++;
          }
        }

        break;
    case 6:
      if(millis() - timer >= 100) {
        sendingByte = 0;
        state++;
      }
      break;
    case 7:
      if(millis() - timer >= 10) {
        Serial.write(cmds[sendingCmd].bytes[sendingByte++]);
        timer = millis();
        if(sendingByte == 6) {
          responseBufIndex = 0;
          state++;
        }
      }
      break;
    case 8:
      if(millis() - timer > 60) {
        // Чистим входной буфер
        Serial.readBytes(responseBuf, Serial.available());
        sendingCmd = 0;
        sendingByte = 0;
        state = 7;
      }
      else if(Serial.available()) {
        responseBuf[responseBufIndex++] = Serial.read();
        timer = millis();

        if(responseBufIndex == cmds[sendingCmd].responseLen) {
          sensors[sendingCmd] = cmds[sendingCmd].convert(responseBuf);

          if(++sendingCmd == cmdsLen)
            sendingCmd = 0;

          state = 6;
        }
        
      }
      break;
    }

    return 0;
}

void setup() {
  
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);

  digitalWrite(STARTER_PIN, LOW);
  pinMode(STARTER_PIN, OUTPUT);
  digitalWrite(IGNITION_PIN, LOW);
  pinMode(IGNITION_PIN, OUTPUT);

  logger->begin(BAUD_LOGGER);
  logger->println("\n\nUsing Serial1 for logging");

  logger->println(ESP.getFullVersion());
  logger->printf("Serial baud: %d (8n1: %d KB/s)\n", BAUD_SERIAL, BAUD_SERIAL * 8 / 10 / 1024);
  logger->printf("Serial receive buffer size: %d bytes\n", RXBUFFERSIZE);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  logger->print("\nConnecting to ");
  logger->println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    logger->print('.');
    delay(500);
  }

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    logger->println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    logger->println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    logger->printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    logger->printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      logger->println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      logger->println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      logger->println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      logger->println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      logger->println("End Failed");
    }
  });
  ArduinoOTA.begin();

  //start server
  server.begin();
  server.setNoDelay(true);

  logger->print("Ready! Use 'telnet ");
}

void loop() {
  ArduinoOTA.handle();
  //check if there are any new clients
  if (server.hasClient()) {
    //find free/disconnected spot
    int i;
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      if (!serverClients[i]) { // equivalent to !serverClients[i].connected()
        serverClients[i] = server.available();
        serverClients[i].println("List of commands:\n\ri - init bus\n\rs - start engine");
        break;
      }
    }

    //no free/disconnected spot so reject
    if (i == MAX_SRV_CLIENTS) {
      server.available().println("busy");
      // hints: server.available() is a WiFiClient with short-term scope
      // when out of scope, a WiFiClient will
      // - flush() - all data will be sent
      // - stop() - automatically too
      logger->printf("server is busy with %d active connections\n", MAX_SRV_CLIENTS);
    }
  }


  while (serverClients[0].available()) {
    cmd = serverClients[0].read();
    switch (cmd)
    {
    case 'i':
      initialized = true;
      state = 0;
      timer = millis();
      break;
    case 's':
      serverClients[0].println("Sending start command...");
      EngineStart("password", StartEngineCallback);
      break;
    
    default:
      serverClients[0].println("Unrecognized input");
      break;
    }
  }

  if(initialized) {
    int res = ecuTick();
    if(res != 0)
      initialized = false;
  }

  EngineConnectorTick();
}
