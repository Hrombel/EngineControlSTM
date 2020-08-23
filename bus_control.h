#ifndef INC_BUS_CONTROL_H_
#define INC_BUS_CONTROL_H_

typedef enum {
    /* Инициализация соединения с двигателем */
	BUS_CMD_INIT,

	BUS_CMD_MAX_ITEM

} BusCommand;

typedef enum {
	BUS_MESSAGE_NONE,
    /* Соединение с ЭБУ успешно установлено */
    BUS_INIT_SUCCESS,

} BusMessage;

typedef enum {
	BUS_ERROR_NONE,
    /* Ошибка установки соединения с ЭБУ */
    BUS_INIT_ERROR,
    /* Попытка подписки на некорректный датчик */
    BUS_INCORRECT_SENSOR_ERROR,
    /* Шина итак в процессе */
    BUS_INIT_PROGRESS,
} BusError;

typedef union {
	char msg;
	char err;
} BusEvent;

typedef unsigned char BusSensor;

bool BusSubscribe(BusSensor sensor);

void BusControlEvent(bool isError, BusEvent event);
bool BusSensorValue(BusSensor sensor, int value);

void bus_tick(bool initSignal);

#endif /* INC_BUS_CONTROL_H_ */