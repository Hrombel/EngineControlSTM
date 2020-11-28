#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>
#include <PubSubClient.h>

#include "settings.h"
#include "engine_connector.h"
#include "engine_control.h"
#include "bus_connector.h"
#include "bus_control.h"

// HardwareSerial Serial1(PA10, PA9); // Already defined
HardwareSerial Serial2(PA3, PA2);

#define IDLE            0
#define IGNITION        1
#define BUS_INIT_START  2
#define WAIT_ECU_READY  3
#define BUS_WORKING     4

#define user Serial
#define ecu Serial1
#define gsm Serial2

const char apn[] = "";
const char gprsUser[] = "";
const char gprsPass[] = "";

TinyGsm modem(gsm);
TinyGsmClient client(modem);
PubSubClient mqtt(client);


unsigned long long timer;
char state = IDLE;

HCMD cmdHandler = 0;
HBusCmd busHandler = 0;

static int rpm = 0;

const char* broker = "194.87.95.82";
uint32_t lastReconnectAttempt = 0;

bool cmdEngineStart = false;
bool cmdEngineStop = false;
bool cmdBusInit = false;
bool cmdBusStop = false;

bool startCmd = false;
bool stopCmd = false;
bool sigIgnitionStart = false;
bool sigBusStopSuccess = false;
bool sigBusRequestDelayStart = false;
bool sigBusRequestDelayStop = false;
bool sigBusInitError = false;
bool sigBusStopError = false;

bool sigEngineStartOk = false;
bool sigEngineStopOk = false;
bool sigEngineStartFail = false;
bool sigEngineIgnitionOff = false;


static const char* busMessages[] = {
  "",                      // 0
  "Connected to ECU",      // 1
  "Disconnected from ECU", // 2
  "",                      // 3
  "",                      // 4
};
static const char* busErrors[] = {
  "",                                     // 0
  "Falied to connect to ECU",             // 1
  "Cannot subscribe to invalid sensor",   // 2
  "Bus is already in connecting process", // 3
  "Bus is already initialized",           // 4
  "Bus is already in stopping process",   // 5
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
  user.printf("%x ", v);
}

bool allowExpensiveOps = true;
#define pendingMsgLen 5
struct {
  char topic[20];
  char message[50];
} 
pendingMsgs[pendingMsgLen];
byte pendingMsgIndex = 0;

void safePublish(char* topic, char* msg) {
  if(allowExpensiveOps) {
    mqtt.publish(topic, msg, true);
    return;
  }
  if(pendingMsgIndex == pendingMsgLen) return;

  sprintf(pendingMsgs[pendingMsgIndex].topic, "%s", topic);
  sprintf(pendingMsgs[pendingMsgIndex].message, "%s", msg);
  
  pendingMsgIndex++;
}

bool BusCallback(HBusCmd callId, BusCommand cmd, BusConnectorResult res, BusEvent event) {
  if(res == BUS_MESSAGE) {
    if(event.msg != BUS_REQUEST_DELAY_START && event.msg != BUS_REQUEST_DELAY_STOP)
      user.printf("MSG: %s\n\r", busMessages[event.msg]);
  }
  else {
    user.printf("ERR: %s\n\r", busErrors[event.err]);
  }

  if(res == BUS_MESSAGE) {
    if(event.msg == BUS_STOP_SUCCESS) {
      sigBusStopSuccess = true;
    }
    else if(event.msg == BUS_REQUEST_DELAY_START) {
      sigBusRequestDelayStart = true;
    }
    else if(event.msg == BUS_REQUEST_DELAY_STOP) {
      sigBusRequestDelayStop = true;
    }
  }
  else if(res == BUS_ERROR) {
    if(event.err == BUS_INIT_ERROR) {
      sigBusInitError = true;
    }
    else if(event.err == BUS_STOP_ERROR) {
      sigBusStopError = true;
    }
  }

  return false;
}

bool EngineCallback(HCMD callId, EngineCommand cmd, EngineConnectorResult res, EngineEvent event) {
  if(res == ENGINE_MESSAGE) {
    user.printf("MSG: %s\n\r", engineMessages[event.msg]);
  }
  else {
    user.printf("ERR: %s\n\r", engineErrors[event.err]);
  }

  if(res == ENGINE_MESSAGE) {
    if(event.msg == ENGINE_START_OK) {
      sigEngineStartOk = true;
    }
    else if(event.msg == ENGINE_STOP_OK) {
      sigEngineStopOk = true;
    }
    else if(event.msg == ENGINE_IGNITION_ON) {
      sigIgnitionStart = true;
    }
    else if(event.msg == ENGINE_IGNITION_OFF) {
      sigEngineIgnitionOff = true;
    }
  }
  else {
    if(event.err == ENGINE_START_FAIL) {
      sigEngineStartFail = true;
    }
  }

	return false;
}

bool UpdateSensorCallback(HBusSub id, BusSensor sensor, int value) {
  if(sensor == 0) {
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

void EngineStart() {
  cmdEngineStart = true;
}
void EngineStop() {
  cmdEngineStop = true;
}

void BusInit() {
  cmdBusInit = true;
}
void BusStop() {
  cmdBusStop = true;
}

void ownTick(
  bool start_cmd, 
  bool stop_cmd, 
  bool sig_ignition_started, 
  bool sig_bus_stop_success,
  bool sig_bus_request_delay_start,
  bool sig_bus_request_delay_stop,
  bool sig_bus_init_error,
  bool sig_bus_stop_error,
  bool sig_engine_start_ok,
  bool sig_engine_stop_ok,
  bool sig_engine_start_fail,
  bool sig_engine_ignition_off
) {
  switch (state)
  {
  case IDLE:
    if(start_cmd) {
      state = IGNITION;
      allowExpensiveOps = false;
      
      user.println("Sending start command...");
      EngineStart();
    }
    else if(stop_cmd) {
      user.println("Sending stop command...");
      EngineStop();
    }

    if(sig_bus_stop_success || sig_bus_stop_error) {
      allowExpensiveOps = true;
    }
    if(sig_bus_request_delay_start) {
      allowExpensiveOps = true;
    }
    if(sig_bus_request_delay_stop) {
      allowExpensiveOps = false;
    }
    if(sig_bus_init_error) {
      EngineStop();
    }
    if(sig_engine_start_ok) {
      safePublish("matiz/engine/status", "on");
    }
    if(sig_engine_ignition_off) {
      // ???
    }
    if(sig_engine_stop_ok) {
      allowExpensiveOps = true;
      safePublish("matiz/engine/status", "off");
    }
    if(sig_engine_start_fail) {
      safePublish("matiz/engine/status", "off");
    }

    break;
  case IGNITION:
    if(start_cmd) {
      user.println("Sending start command...");
      EngineStart();
    }
    else if(stop_cmd) {
      state = IDLE;
      user.println("Sending stop command...");
      EngineStop();
    }

    if(sig_ignition_started) {
      timer = GetTime();
      state = BUS_INIT_START;
    }
    if(sig_bus_stop_success || sig_bus_stop_error) {
      allowExpensiveOps = true;
      state = IDLE;
    }
    if(sig_bus_request_delay_start) {
      allowExpensiveOps = true;
    }
    if(sig_bus_request_delay_stop) {
      allowExpensiveOps = false;
    }
    if(sig_bus_init_error) {
      state = IDLE;
      EngineStop();
    }
    if(sig_engine_start_ok) {
      safePublish("matiz/engine/status", "on");
    }
    if(sig_engine_ignition_off) {
      state = IDLE;
    }
    if(sig_engine_stop_ok) {
      allowExpensiveOps = true;
      safePublish("matiz/engine/status", "off");
    }
    if(sig_engine_start_fail) {
      safePublish("matiz/engine/status", "off");
    }
    break;
  case BUS_INIT_START:
    if(start_cmd) {
      user.println("Sending start command...");
      EngineStart();
    }
    else if(stop_cmd) {
      state = IDLE;
      user.println("Sending stop command...");
      EngineStop();
    }
    
    if(sig_bus_stop_success || sig_bus_stop_error) {
      allowExpensiveOps = true;
      state = IDLE;
    }
    if(sig_bus_request_delay_start) {
      allowExpensiveOps = true;
    }
    if(sig_bus_request_delay_stop) {
      allowExpensiveOps = false;
    }
    if(sig_bus_init_error) {
      state = IDLE;
      EngineStop();
    }
    if(sig_engine_start_ok) {
      safePublish("matiz/engine/status", "on");
    }
    if(sig_engine_ignition_off) {
      state = IDLE;
    }
    if(sig_engine_stop_ok) {
      allowExpensiveOps = true;
      safePublish("matiz/engine/status", "on");
    }
    if(sig_engine_start_fail) {
      safePublish("matiz/engine/status", "off");
    }

    state = WAIT_ECU_READY;
    break;
  case WAIT_ECU_READY:
    if(start_cmd) {
      user.println("Sending start command...");
      EngineStart();
    }
    else if(stop_cmd) {
      state = IDLE;
      user.println("Sending stop command...");
      EngineStop();
    }
    
    if(sig_bus_stop_success || sig_bus_stop_error) {
      allowExpensiveOps = true;
      state = IDLE;
    }
    if(GetTime() - timer > 3000) {
      state = BUS_WORKING;
      BusInit();
    }
    if(sig_bus_request_delay_start) {
      allowExpensiveOps = true;
    }
    if(sig_bus_request_delay_stop) {
      allowExpensiveOps = false;
    }
    if(sig_bus_init_error) {
      state = IDLE;
      EngineStop();
    }
    if(sig_engine_start_ok) {
      safePublish("matiz/engine/status", "on");
    }
    if(sig_engine_ignition_off) {
      state = IDLE;
    }
    if(sig_engine_stop_ok) {
      allowExpensiveOps = true;
      safePublish("matiz/engine/status", "off");
    }
    if(sig_engine_start_fail) {
      safePublish("matiz/engine/status", "off");
    }
    break;
  case BUS_WORKING:
    if(start_cmd) {
      user.println("Sending start command...");
      EngineStart();
    }
    else if(stop_cmd) {
      user.println("Sending stop command...");
      EngineStop();
    }
    
    if(sig_bus_stop_success || sig_bus_stop_error) {
      allowExpensiveOps = true;
      state = IDLE;
    }
    if(sig_bus_request_delay_start) {
      allowExpensiveOps = true;
    }
    if(sig_bus_request_delay_stop) {
      allowExpensiveOps = false;
    }
    if(sig_bus_init_error) {
      state = IDLE;
      EngineStop();
    }
    if(sig_engine_start_ok) {
      safePublish("matiz/engine/status", "on");
    }
    if(sig_engine_ignition_off) {
      state = IDLE;
      BusStop();
    }
    if(sig_engine_stop_ok) {
      allowExpensiveOps = true;
      safePublish("matiz/engine/status", "off");
    }
    if(sig_engine_start_fail) {
      safePublish("matiz/engine/status", "off");
    }
    break;
  }

}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  user.print("Message arrived [");
  user.print(topic);
  user.print("]: ");
  user.write((char*)payload, len);
  user.println();
  

  // Only proceed if incoming message's topic matches
  if (strncmp(topic, "matiz/engine", len) == 0) {
    if(strncmp((char*)payload, "on", len) == 0) {
      startCmd = true;
    }
    else if(strncmp((char*)payload, "off", len) == 0) {
      stopCmd = true;
    }
  }
}

boolean mqttConnect() {
  user.print("Connecting to ");
  user.print(broker);

  // Connect to MQTT Broker
  boolean status = mqtt.connect("Matiz", "hrombel", "p2r0o1g6ears");

  if (status == false) {
    user.println(" fail");
    return false;
  }
  user.println(" success");
  mqtt.subscribe("matiz/engine");
  return mqtt.connected();
}
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200

void setup() {
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH );

  digitalWrite(STARTER_PIN, LOW);
  pinMode(STARTER_PIN, OUTPUT);
  digitalWrite(IGNITION_PIN, LOW);
  pinMode(IGNITION_PIN, OUTPUT);

  user.begin(115200);
  user.println("Wait...");
  TinyGsmAutoBaud(gsm, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  delay(6000);
  user.println("Initializing modem...");
  modem.restart();

  String modemInfo = modem.getModemInfo();
  user.print("Modem Info: ");
  user.println(modemInfo);

  user.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    user.println(" fail");
    delay(10000);
    return;
  }
  user.println(" success");

  if (modem.isNetworkConnected()) {
    user.println("Network connected");
  }

  // GPRS connection parameters are usually set after network registration
  user.print(F("Connecting to "));
  user.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    user.println(" fail");
    delay(10000);
    return;
  }
  user.println(" success");

  if (modem.isGprsConnected()) {
    user.println("GPRS connected");
  }

  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);

  BusConnectorSubscribe(0, UpdateSensorCallback);

  while(!mqtt.connected()) {
    user.println("=== MQTT NOT CONNECTED ===");
    mqttConnect();
    delay(10000);
  }

  // TODO: Наверное в будущем стоит проверять, 
  // работает ли двигатель в данный момент (на случай если в обход МК есть доступ к зажиганию)
  safePublish("matiz/engine/status", "off");
}

void loop() {

  static uint32_t loopStart;
  if(allowExpensiveOps) {

    if (!mqtt.connected()) {
      user.println("=== MQTT NOT CONNECTED ===");
      // Reconnect every 10 seconds
      uint32_t t = millis();
      if (t - lastReconnectAttempt > 10000L) {
        lastReconnectAttempt = t;
        if (mqttConnect()) {
          lastReconnectAttempt = 0;
        }
      }
      delay(100);
      return;
    }
    mqtt.loop();

    if(pendingMsgIndex) {
      pendingMsgIndex--;
      loopStart = millis();
      mqtt.publish(pendingMsgs[pendingMsgIndex].topic, pendingMsgs[pendingMsgIndex].message, true);
      user.printf("Time Publish: %d\n\r", millis() - loopStart);
    }
  }

  ownTick(
    startCmd, 
    stopCmd, 
    sigIgnitionStart, 
    sigBusStopSuccess,
    sigBusRequestDelayStart,
    sigBusRequestDelayStop,
    sigBusInitError,
    sigBusStopError,
    sigEngineStartOk,
    sigEngineStopOk,
    sigEngineStartFail,
    sigEngineIgnitionOff
  );
  startCmd = false; 
  stopCmd = false;
  sigIgnitionStart = false;
  sigBusStopSuccess = false;
  sigBusRequestDelayStart = false;
  sigBusRequestDelayStop = false;
  sigBusInitError = false;
  sigBusStopError = false;
  sigEngineStartOk = false;
  sigEngineStopOk = false;
  sigEngineStartFail = false;
  sigEngineIgnitionOff = false;

  if(cmdEngineStart) {
    cmdHandler = EngineCmd(CMD_START_ENGINE, "password", EngineCallback, cmdHandler);
    cmdEngineStart = false;
  }
  if(cmdEngineStop) {
    cmdHandler = EngineCmd(CMD_STOP_ENGINE, "password", EngineCallback, cmdHandler);
    cmdEngineStop = false;
  }

  if(cmdBusInit) {
    busHandler = BusCmd(BUS_CMD_INIT, BusCallback, busHandler);
    cmdBusInit = false;
  }
  if(cmdBusStop) {
    busHandler = BusCmd(BUS_CMD_STOP, BusCallback, busHandler);
    cmdBusStop = false;
  }

  EngineConnectorTick();
  BusConnectorTick();
}
