
#ifndef INC_ENGINE_CONNECTOR_H_
#define INC_ENGINE_CONNECTOR_H_

#include "engine_control.h"

/* ID вызова команды к двигателю */
typedef unsigned int HCMD;

typedef enum EngineCommand {

	CMD_START_ENGINE,
	CMD_STOP_ENGINE,

	// Максимальный элемент команд, требующих авторизации ключом
	CMD_AUTH_MAX,

	CMD_MAX_ITEM

} EngineCommand;

typedef enum {

	/* Пришло сообщение от подсистемы управления двигателем */
	ENGINE_MESSAGE,
	/* Пришла ошибка от подсистемы управления двигателем */
	ENGINE_ERROR,
	/* Достигнут потолок подписок */
	MAX_SUBS_REACHED,
	/* Передан неверный идентификатор подписки */
	INCORRECT_ID,
	/* Неизвестная ошибка */
	UNKNOWN_ERROR,

	/* Метка начала ошибок входных данных */
	INVALID_INPUT_START,
	/* Вызвана неверная команда */
	INVALID_CMD,
	/* Неверный ключ доступа */
	KEY_INCORRECT,

} EngineConnectorResult;

void EngineControlEvent(bool isError, EngineEvent event);

typedef bool(*EngineEventHandler)(HCMD, EngineCommand, EngineConnectorResult, EngineEvent);

HCMD EngineCmd(EngineCommand cmd, const char* key, const EngineEventHandler callback, HCMD id = 0);
#define EngineConnectorTick() engine_control_tick(false, false, false, false)

#endif /* INC_ENGINE_CONNECTOR_H_ */