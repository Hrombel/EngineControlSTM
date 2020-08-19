#include "settings.h"
#include "bus_connector.h"

typedef struct {
	HBusCmd id;
	BusCommand cmd;
	BusEventHandler callback;

} Subscriber;

static Subscriber subs[MAX_CONNECTIONS] = { 0 };
static unsigned int freeSubIndex = 0;
static HBusCmd subId = 1;

static BusMessage engineRes;
static BusError engineErr;

bool BusSensorValue(BusSensor sensor, int value) {
    // Не реализовано
}

HBusCmd BusCmd(BusCommand cmd, const BusEventHandler callback, HBusCmd id) {
	static BusEvent e;
	static HBusCmd currentId;

	if (cmd >= CMD_MAX_ITEM) {
		callback(0, cmd, BUS_INVALID_CMD, e);
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
			callback(0, cmd, BUS_INCORRECT_ID, e);
			return 0;
		}
	}
	else {
		if (freeSubIndex == MAX_CONNECTIONS) {
			callback(0, cmd, BUS_MAX_SUBS_REACHED, e);
			return 0;
		}

		currentId = subId++;
		subs[freeSubIndex++] = { currentId, cmd, callback };
	}

	switch (cmd) {
	case BUS_CMD_INIT:
		bus_tick(true);
		break;
	}
	
	return currentId;
}

void BusControlEvent(bool isError, BusEvent event) {
	BusConnectorResult res = isError ? BUS_ERROR : BUS_MESSAGE;

	for (int i = 0; i < freeSubIndex;) {
		if (subs[i].callback(subs[i].id, subs[i].cmd, res, event)) {
			for (int j = i + 1; j < freeSubIndex; j++)
				subs[j - 1] = subs[j];
			freeSubIndex--;
		}
		else i++;
	}
}