#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

#include <algorithm> // std::min

#include "settings.h"
#include "engine_connector.h"
#include "engine_control.h"
#include "bus_connector.h"
#include "bus_control.h"


#define IDLE            0
#define IGNITION        1
#define BUS_INIT_START  2
#define WAIT_ECU_READY  3
#define BUS_WORKING     4

#ifndef STASSID
#define STASSID "SevenEye"
#define STAPSK  "DiTaKsA712892"
#endif

#define BAUD_LOGGER 115200

////////////////////////////////////////////////////////////

#define logger (&Serial1)

//how many clients should be able to telnet to this ESP8266
#define MAX_SRV_CLIENTS 1
const char* ssid = STASSID;
const char* password = STAPSK;

const int port = 23;

WiFiServer server(port);
WiFiClient serverClients[MAX_SRV_CLIENTS];

unsigned long long timer;
char state = IDLE;
char cmd = 0;

HCMD cmdHandler = 0;
HBusCmd busHandler = 0;
#define EngineStart(key) cmdHandler = EngineCmd(CMD_START_ENGINE, key, EngineCallback, cmdHandler)
#define EngineStop(key) cmdHandler = EngineCmd(CMD_STOP_ENGINE, key, EngineCallback, cmdHandler)
#define BusInit() busHandler = BusCmd(BUS_CMD_INIT, BusCallback, busHandler)
#define BusStop() busHandler = BusCmd(BUS_CMD_STOP, BusCallback, busHandler)

static int rpm = 0;

static const char* busMessages[] = {
  "",                     // 0
  "Connected to ECU",     // 1
  "Disconnected from ECU" // 2
};
static const char* busErrors[] = {
  "",                                     // 0
  "Falied to connect to ECU",             // 1
  "Cannot subscribe to invalid sensor",   // 2
  "Bus is already in connecting process", // 3
  "Bus is already initialized",           // 4
  "Bus is already in stopping proceses",  // 5
  "Bus is already stopped",               // 6
  "Failed to stop bus connection",        // 7
  "Cannot init bus while stopping"        // 8
};

static const char* engineMessages[] = {
  "",                   // 0
  "Engine started",     // 1
  "Engine stopped",     // 2
  "Ignition enabled",   // 3
  "Ignition disabled",  // 4
  "Starter enabled",    // 5
  "Starter disabled",   // 6
};
static const char* engineErrors[] = {
  "",                                             // 0
  "Cannot start the engine",                      // 1
  "Engine stalled",                               // 2
  "Can't enable starter without ignition",        // 3
  "Can't enable starter while engine is working", // 4
  "Engine is already stopped",                    // 5
  "Engine is in manual mode",                     // 6
  "Engine start is in progress",                  // 7
  "Engine switched to manual",                    // 8
  "Engine start is aborted",                      // 9
  "Engine already started",                       // 10
};

void log(uint8_t v) {
  serverClients[0].printf("%x ", v);
}

bool BusCallback(HBusCmd callId, BusCommand cmd, BusConnectorResult res, BusEvent event) {
  if(res == BUS_MESSAGE) {
    serverClients[0].printf("MSG: %s\n\r", busMessages[event.msg]);
  }
  else {
    serverClients[0].printf("ERR: %s\n\r", busErrors[event.err]);
  }

  if(res == BUS_MESSAGE) {
    if(event.msg == BUS_STOP_SUCCESS) {
      state = IDLE;
    }
  }
  else if(res == BUS_ERROR) {
    if(event.err == BUS_INIT_ERROR) {
      state = IDLE;
      EngineStop("password");
    }
    else if(event.err == BUS_STOP_ERROR) {
      state = IDLE;
    }
  }

  return false;
}

bool EngineCallback(HCMD callId, EngineCommand cmd, EngineConnectorResult res, EngineEvent event) {
  if(res == ENGINE_MESSAGE) {
    serverClients[0].printf("MSG: %s\n\r", engineMessages[event.msg]);
  }
  else {
    serverClients[0].printf("ERR: %s\n\r", engineErrors[event.err]);
  }

  if(res == ENGINE_MESSAGE) {
    if(event.msg == ENGINE_IGNITION_ON) {
      if(state == IGNITION)
        state = BUS_INIT_START;
    }
    else if(event.msg == ENGINE_IGNITION_OFF) {
      if(state == IGNITION || state == BUS_INIT_START || state == WAIT_ECU_READY) {
        state = IDLE;
      }
      else if(state == WAIT_ECU_READY || state == BUS_WORKING) {
        BusStop();
      }
    }
  }

	return false;
}

bool UpdateSensorCallback(HBusSub id, BusSensor sensor, int value) {
  if(sensor == 0) {
    serverClients[0].printf("RPM: %d\n\r", value);
    rpm = value;
  }

  return false;
}

unsigned long GetTime() {
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
	return rpm;
}

void ownTick() {
  switch (state)
  {
  case IDLE:
    break;
  case IGNITION:
    break;
  case BUS_INIT_START:
    timer = GetTime();
    state = WAIT_ECU_READY;
    break;
  case WAIT_ECU_READY:
    if(GetTime() - timer > 3000) {
      state = BUS_WORKING;
      BusInit();
    }
    break;
  case BUS_WORKING:
    break;
  }
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

  BusConnectorSubscribe(0, UpdateSensorCallback);
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
        serverClients[i].println("List of commands:\n\rs - start engine\n\re - stop engine");
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
    case 's':
      serverClients[0].println("Sending start command...");
      if(state == IDLE)
        state = IGNITION;
      EngineStart("password");
      break;
    case 'e':
      serverClients[0].println("Sending stop command...");
      if(state == BUS_INIT_START || state == IGNITION)
        state = IDLE;
      EngineStop("password");
      break;
    
    default:
      serverClients[0].println("Unrecognized input");
      break;
    }
  }

  ownTick();
  BusConnectorTick();
  EngineConnectorTick();
}
