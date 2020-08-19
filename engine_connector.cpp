#include "settings.h"
#include "engine_connector.h"

bool KeyCompare(const char*, const char*, int);

typedef struct {
	HCMD id;
	EngineCommand cmd;
	EngineEventHandler callback;

} Subscriber;

static const char pkey[KEY_LENGTH] = PRIVATE_KEY;

static Subscriber subs[MAX_CONNECTIONS] = { 0 };
static unsigned int freeSubIndex = 0;
static HCMD subId = 1;

static EngineMessage engineRes;
static EngineError engineErr;

HCMD EngineCmd(EngineCommand cmd, const char* key, const EngineEventHandler callback, HCMD id) {
	EngineEvent e;
	HCMD currentId;
	
	if (!KeyCompare(key, pkey, KEY_LENGTH)) {
		callback(0, cmd, KEY_INCORRECT, e);
		return 0;
	}
	if (cmd >= CMD_MAX_ITEM) {
		callback(0, cmd, INVALID_CMD, e);
		return 0;
	}

	if(id) {
		currentId = 0;
		for(int i = 0; i < freeSubIndex; i++) {
			if(subs[i].id == id) {
				currentId = id;
				subs[i] = { id, cmd, callback };
				break;
			}
		}
		if(!currentId) {
			callback(0, cmd, INCORRECT_ID, e);
			return 0;
		}
	}
	else {
		if (freeSubIndex == MAX_CONNECTIONS) {
			callback(0, cmd, MAX_SUBS_REACHED, e);
			return 0;
		}

		currentId = subId++;
		subs[freeSubIndex++] = { currentId, cmd, callback };
	}

	switch (cmd) {
	case CMD_START_ENGINE:
		engine_control_tick(false, false, true, false);
		break;
	case CMD_STOP_ENGINE:
		engine_control_tick(false, false, false, true);
		break;
	}
	
	return currentId;
}

void EngineControlEvent(bool isError, EngineEvent event) {
	EngineConnectorResult res = isError ? ENGINE_ERROR : ENGINE_MESSAGE;

	for (int i = 0; i < freeSubIndex;) {
		if (subs[i].callback(subs[i].id, subs[i].cmd, res, event)) {
			for (int j = i + 1; j < freeSubIndex; j++)
				subs[j - 1] = subs[j];
			freeSubIndex--;
		}
		else i++;
	}
}