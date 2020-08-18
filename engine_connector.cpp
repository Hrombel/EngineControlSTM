#include "settings.h"
#include "types.h"
#include "engine_connector.h"

bool KeyCompare(const char*, const char*, int);

typedef struct Subscriber {
	HCMD id;
	EngineCommand cmd;
	EngineEventHandler callback;

} Subscriber;

bool ignitionFlag = false;
bool starterFlag = false;
int rpm = 0;

static const char pkey[KEY_LENGTH] = PRIVATE_KEY;

static Subscriber subs[MAX_CONNECTIONS] = { 0 };
static unsigned int freeSubIndex = 0;
static unsigned int subId = 1;

static EngineMessage engineRes;
static EngineError engineErr;

HCMD EngineCmd(EngineCommand cmd, const char* key, const EngineEventHandler callback) {
	EngineEvent e;

	if (!KeyCompare(key, pkey, KEY_LENGTH)) {
		callback(0, cmd, KEY_INCORRECT, e);
		return 0;
	}
	if (cmd >= CMD_MAX_ITEM) {
		callback(0, cmd, INVALID_CMD, e);
		return 0;
	}
	if (freeSubIndex == MAX_CONNECTIONS) {
		callback(0, cmd, MAX_SUBS_REACHED, e);
		return 0;
	}

	Subscriber sub = { subId, cmd, callback };
	subs[freeSubIndex++] = sub;

	switch (cmd) {
	case CMD_START_ENGINE:
		engine_control_tick(false, false, true, false);
		break;
	}

	return subId++;
}

void EngineConnectorTick() {
	engine_control_tick(false, false, false, false);
}

void EngineControlEvent(bool isError, EngineEvent event) {
	EngineConnectorResult res = isError ? ENGINE_ERROR : ENGINE_MESSAGE;

	for (unsigned i = 0; i < freeSubIndex;) {
		if (subs[i].callback(subs[i].id, subs[i].cmd, res, event)) {
			for (unsigned j = i + 1; j < freeSubIndex; j++)
				subs[j - 1] = subs[j];
			freeSubIndex--;
		}
		else i++;
	}
}