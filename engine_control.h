#ifndef INC_ENGINE_CONTROL_H_
#define INC_ENGINE_CONTROL_H_

#include "types.h"

typedef enum EngineMessage {
	MESSAGE_NONE,
	/* Двигатель запущен */
	ENGINE_START_OK,
	/* Двигатель остановлен */
	ENGINE_STOP_OK,

} EngineMessage;

typedef enum EngineError {
	ERROR_NONE,
	/* Неудачная попытка запуска двигателя */
	STARTER_FAILURE,
	/* Все попытки запуска двигателя закончились неудачей */
	STARTER_FATAL_FAILURE,
	/* Двигатель заглох */
	ENGINE_STALLED,
	/* Нельзя запускать стартер, не включив зажигание */
	STARTER_WITHOUT_IGNITION,
	/* Нельзя запускать стартер, когда двигатель уже работает */
	STARTER_WHILE_WORKING,
	/* Попытка остановки остановленного двигателя */
	ENGINE_ALREADY_STOPPED,
	/* Попытка выполнения автоматического действия, когда двигатель находится в ручном режиме */
	ENGINE_IN_MANUAL_MODE,
	/* Двигатель уже в процессе автоматического запуска */
	ENGINE_STARTING_PROGRESS,
	/* Режим управления двигателем переключен на ручной режим */
	ENGINE_SWITCHED_TO_MANUAL,
	/* Запуск двигателя был отменен пользователем */
	ENGINE_START_ABORTED,
	
} EngineError;

typedef union EngineEvent {
	char msg;
	char err;
} EngineEvent;

void SetIgnitionFlag(bool);
void SetStarterFlag(bool);

int GetRPM();

void EngineControlEvent(bool isError, EngineEvent event);
int GetTime();

void engine_control_tick(bool sig_ign_on, bool sig_starter_on, bool sig_engine_start, bool sig_engine_stop);


#endif /* INC_ENGINE_CONTROL_H_ */