#include "settings.h"
#include "engine_control.h"


#define IDLE 					0
#define IGNITION 				1
#define STARTER 				2
#define A_ENGINE_START 			3
#define A_IGNITION_AWAIT 		4
#define A_STARTER 				5
#define A_ENGINE_STARTING 		6
#define A_RPM_READ_AWAIT 		7
#define A_IGNITION_RETRY_AWAIT 	8
#define A_ENGINE_WORKING		9

#define message(event) EngineControlEvent(false, event)
#define error(event) EngineControlEvent(true, event)

void engine_control_tick(bool sig_ign_on, bool sig_starter_on, bool sig_engine_start, bool sig_engine_stop)
{
	static unsigned int state = 0;

	static unsigned int ignTimer;
	static unsigned int rpmTimer;
	static unsigned int startTimer;
	static unsigned int retries = STARTER_RETRY_N;
	static EngineEvent e;

	switch (state) {
		case IDLE: // Простой
			if (sig_ign_on) {
				SetIgnitionFlag(true);
				state = IGNITION;
			}
			else if (sig_engine_start) {
				state = A_ENGINE_START;
			}
			else if (sig_engine_stop) {
				e.err = ENGINE_ALREADY_STOPPED; error(e);
			}
			else if (sig_starter_on) {
				e.err = STARTER_WITHOUT_IGNITION; error(e);
			}

			break;
		case IGNITION: // Ручное включение зажигания
			if (!sig_ign_on) {
				SetIgnitionFlag(false);
				state = IDLE;
			}
			else if (sig_starter_on) {
				SetStarterFlag(true);
				state = STARTER;
			}
			else if (sig_engine_start || sig_engine_stop) {
				e.err = ENGINE_IN_MANUAL_MODE; error(e);
			}

			break;
		case STARTER: // Ручной запуск стартера
			if (!sig_starter_on) {
				SetStarterFlag(false);
				state = IGNITION;
			}
			else if (!sig_ign_on) {
				SetIgnitionFlag(false);
				SetStarterFlag(false);
				state = IDLE;
			}
			else if (sig_engine_start || sig_engine_stop) {
				e.err = ENGINE_IN_MANUAL_MODE; error(e);
			}

			break;
		case A_ENGINE_START: // Автоматический запуск двигателя
			SetIgnitionFlag(true);
			ignTimer = GetTime();
			e.msg = ENGINE_IGNITION_ON; message(e);
			state = A_IGNITION_AWAIT;
		case A_IGNITION_AWAIT:
			if (sig_engine_stop) {
				SetIgnitionFlag(false);
				e.msg = ENGINE_IGNITION_OFF; message(e);
				e.err = ENGINE_START_ABORTED; error(e);
				e.msg = ENGINE_STOP_OK; message(e);
				state = IDLE;
				break;
			}
			else if (sig_ign_on) {
				e.err = ENGINE_SWITCHED_TO_MANUAL; error(e);
				state = IGNITION;
				break;
			}
			else if (sig_starter_on) {
				e.err = STARTER_WITHOUT_IGNITION; error(e);
			}
			else if (sig_engine_start) {
				e.err = ENGINE_STARTING_PROGRESS; error(e);
			}

			if (GetTime() - ignTimer >= IGNITION_TIME) {
				state = A_STARTER;
			}

			break;
		case A_STARTER:
			SetStarterFlag(true);
			startTimer = GetTime();
			state = A_ENGINE_STARTING;
		case A_ENGINE_STARTING:
			if (sig_engine_stop) {
				SetStarterFlag(false);
				SetIgnitionFlag(false);
				e.msg = ENGINE_IGNITION_OFF; message(e);
				e.err = ENGINE_START_ABORTED; error(e);
				e.msg = ENGINE_STOP_OK; message(e);
				state = IDLE;
				break;
			}
			else if (sig_ign_on) {
				if (sig_starter_on) {
					state = STARTER;
				}
				else {
					SetStarterFlag(false);
					state = IGNITION;
				}
				e.err = ENGINE_SWITCHED_TO_MANUAL; error(e);
				break;
			}
			else if (sig_engine_start) {
				e.err = ENGINE_STARTING_PROGRESS; error(e);
			}
			else if (sig_starter_on) {
				e.err = STARTER_WITHOUT_IGNITION; error(e);
			}


			if (GetRPM() >= IDLING_RPM) {
				SetStarterFlag(false);
				e.msg = ENGINE_START_OK; message(e);
				rpmTimer = GetTime();
				state = A_ENGINE_WORKING;
			}
			else if (GetTime() - startTimer >= STARTER_TIME) {
				SetStarterFlag(false);
				SetIgnitionFlag(false);
				if (retries--) {
					e.err = STARTER_FAILURE; error(e);
					ignTimer = GetTime();
					state = A_IGNITION_RETRY_AWAIT;
				}
				else {
					retries = STARTER_RETRY_N;
					e.err = STARTER_FATAL_FAILURE; error(e);
					state = IDLE; // Отказ
				}
				e.msg = ENGINE_IGNITION_OFF; message(e);
			}
			else {
				rpmTimer = GetTime();
				state = A_RPM_READ_AWAIT;
			}

			break;
		case A_RPM_READ_AWAIT: // Ждем перед сканированием оборотов
			if (sig_engine_stop) {
				SetStarterFlag(false);
				SetIgnitionFlag(false);
				e.msg = ENGINE_IGNITION_OFF; message(e);
				e.err = ENGINE_START_ABORTED; error(e);
				e.msg = ENGINE_STOP_OK; message(e);
				state = IDLE;
				break;
			}
			else if (sig_ign_on) {
				if (sig_starter_on)
					state = STARTER;
				else {
					SetStarterFlag(false);
					state = IGNITION;
				}
				e.err = ENGINE_SWITCHED_TO_MANUAL; error(e);
				break;
			}
			else if (sig_engine_start) {
				e.err = ENGINE_STARTING_PROGRESS; error(e);
			}
			else if (sig_starter_on) {
				e.err = STARTER_WITHOUT_IGNITION; error(e);
			}

			if (GetTime() - rpmTimer >= RPM_READ_DELAY) {
				state = A_ENGINE_STARTING;
			}

			break;
		case A_IGNITION_RETRY_AWAIT: // Ждем после неудачной попытки запуска двигателя
			if (sig_engine_stop) {
				e.err = ENGINE_START_ABORTED; error(e);
				e.msg = ENGINE_STOP_OK; message(e);
				state = IDLE;
				break;
			}
			else if (sig_ign_on) {
				SetStarterFlag(true);
				state = IGNITION;
				e.err = ENGINE_SWITCHED_TO_MANUAL; error(e);
				break;
			}
			else if (sig_engine_start) {
				e.err = ENGINE_STARTING_PROGRESS; error(e);
			}
			else if (sig_starter_on) {
				e.err = STARTER_WITHOUT_IGNITION; error(e);
			}

			if (GetTime() - ignTimer >= STARTER_FAIL_DELAY) {
				state = A_ENGINE_START;
			}

			break;
		case A_ENGINE_WORKING: // Работа двигателя, когда мы выполнили автоматический запуск
			if (sig_ign_on) {
				e.err = ENGINE_SWITCHED_TO_MANUAL; error(e);
				state = IGNITION;
				break;
			}
			else if (sig_starter_on) {
				e.err = STARTER_WHILE_WORKING; error(e);
			}
			else if (sig_engine_start) {
				e.err = ENGINE_STARTING_PROGRESS; error(e);
			}
			else if (sig_engine_stop) {
				SetIgnitionFlag(false);
				e.msg = ENGINE_IGNITION_OFF; message(e);
				e.msg = ENGINE_STOP_OK; message(e);
				state = IDLE;
				break;
			}

			if (GetTime() - rpmTimer >= RPM_READ_DELAY) {
				if (!GetRPM()) {
					SetIgnitionFlag(false);
					e.msg = ENGINE_IGNITION_OFF; message(e);
					e.err = ENGINE_STALLED; error(e);
					state = IDLE;
				}
			}
			break;
	}
}