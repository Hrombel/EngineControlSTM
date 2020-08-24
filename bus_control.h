#ifndef INC_BUS_CONTROL_H_
#define INC_BUS_CONTROL_H_

typedef enum {
    /* Инициализация соединения с ЭБУ */
	BUS_CMD_INIT,
    /* Сброс соединения с ЭБУ */
    BUS_CMD_STOP,

	BUS_CMD_MAX_ITEM

} BusCommand;

typedef enum {
	BUS_MESSAGE_NONE,
    /* Соединение с ЭБУ успешно установлено */
    BUS_INIT_SUCCESS,
    /* Соединения с ЭБУ разорвано успешо */
    BUS_STOP_SUCCESS,

} BusMessage;

typedef enum {
	BUS_ERROR_NONE,
    /* Ошибка установки соединения с ЭБУ */
    BUS_INIT_ERROR,
    /* Попытка подписки на некорректный датчик */
    BUS_INCORRECT_SENSOR_ERROR,
    /* Шина итак в процессе запуска */
    BUS_INIT_PROGRESS,
    /* Шина итак запущена */
    BUS_ALREADY_INIT,
    /* Шина итак в процессе остановки */
    BUS_STOP_PROGRESS,
    /* Шина итак остановлена */
    BUS_ALREADY_STOPPED,
} BusError;

typedef union {
	char msg;
	char err;
} BusEvent;

typedef unsigned char BusSensor;

bool BusSubscribe(BusSensor sensor);

void BusControlEvent(bool isError, BusEvent event);
bool BusSensorValue(BusSensor sensor, int value);

void bus_tick(bool initSignal, bool stopSignal);

#endif /* INC_BUS_CONTROL_H_ */