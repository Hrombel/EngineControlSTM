#ifndef INC_BUS_CONNECTOR_H_
#define INC_BUS_CONNECTOR_H_

#include "bus_control.h"

/* ID вызова команды к ЭБУ */
typedef unsigned int HBusCmd;

typedef enum {

	/* Пришло сообщение от подсистемы работы с ЭБУ */
	BUS_MESSAGE,
	/* Пришла ошибка от подсистемы управления двигателем */
	BUS_ERROR,
	/* Достигнут потолок подписок */
	BUS_MAX_SUBS_REACHED,
	/* Передан неверный идентификатор подписки */
	BUS_INCORRECT_ID,
	/* Неизвестная ошибка */
	BUS_UNKNOWN_ERROR,

	/* Метка начала ошибок входных данных */
	BUS_INVALID_INPUT_START,
	/* Вызвана неверная команда */
	BUS_INVALID_CMD,

} BusConnectorResult;

typedef unsigned HBusSub;
typedef bool(*BusValueHandler)(HBusSub, BusSensor, int);
HBusSub BusConnectorSubscribe(BusSensor, const BusValueHandler, HBusSub = 0);

typedef bool(*BusEventHandler)(HBusCmd, BusCommand, BusConnectorResult, BusEvent);
HBusCmd BusCmd(BusCommand cmd, const BusEventHandler callback, HBusCmd id = 0);
#define BusConnectorTick() bus_tick(false, false)

#endif /* INC_BUS_CONNECTOR_H_ */