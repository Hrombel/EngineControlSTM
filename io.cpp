#include "settings.h"
#include "types.h"
#include "engine_connector.h"
#include "io.h"

bool CmdCallback(HCMD callId, EngineCommand cmd, EngineConnectorResult res, EngineEvent event);
void ResponseSend(HCMD callId, EngineConnectorResult res, EngineEvent event);

static RemoteConnection subs[MAX_CONNECTIONS + 1] = { 0 };
static unsigned int freeSubIndex = 0;
static unsigned int subId = 1;


bool CmdCallback(HCMD callId, EngineCommand cmd, EngineConnectorResult res, EngineEvent event) {

	switch (cmd)
	{
	case CMD_START_ENGINE:

		if (res == ENGINE_MESSAGE && event.msg == ENGINE_START_OK)
			break;

		if (res == ENGINE_ERROR) {
			switch (event.err)
			{
			case ENGINE_IN_MANUAL_MODE:
			case ENGINE_STARTING_PROGRESS:
			case ENGINE_SWITCHED_TO_MANUAL:
			case ENGINE_START_ABORTED:
				break;
			}
		}

		return false;
	case CMD_STOP_ENGINE:

		if (res == ENGINE_MESSAGE && event.msg == ENGINE_STOP_OK)
			break;

		if (res == ENGINE_ERROR) {
			switch (event.err)
			{
			case ENGINE_ALREADY_STOPPED:
			case ENGINE_IN_MANUAL_MODE:
			case ENGINE_SWITCHED_TO_MANUAL:
				break;
			}
		}

		return false;
	}

	ResponseSend(callId, res, event);
	return true;
}

void ResponseSend(HCMD callId, EngineConnectorResult res, EngineEvent event) {
	char response[2] = { res, event.msg };

	for (unsigned i = 0; i < freeSubIndex; i++) {
		if (subs[i].callId == callId) {
			// Отправляем response[]
			freeSubIndex--;
			return;
		}
	}

}
