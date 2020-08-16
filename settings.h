/*
 * settings.h
 *
 *  Created on: Dec 8, 2019
 *      Author: Администратор
 */

#ifndef INC_SETTINGS_H_
#define INC_SETTINGS_H_

/* Максимальное количество одновременных подключений по CAN-шине */
#define MAX_CONNECTIONS 10

//#define PRIVATE_KEY "IV3RGmmSJGK2jXALuw9FPPpHbhn4uxOgE6efKiOaWRz8X0OkgTvlbmjxkJLGvcOk"
#define PRIVATE_KEY "password"
#define KEY_LENGTH (sizeof(PRIVATE_KEY) / sizeof(char))

 /** Сколько ждать после включения зажигания? */
#define IGNITION_TIME		2000
/** Максимальное время кручения стартера  */
#define STARTER_TIME 		3000
/** Максимальное количество попыток повторного запуска двигателя */
#define STARTER_RETRY_N		2
/** Время между считываниями скорости вращения коленвала  */
#define RPM_READ_DELAY 		100
/** Время задержки после неудачной попытки запуска двигателя **/
#define STARTER_FAIL_DELAY	1000
/* Минимальные обороты холостого хода двигателя */
#define IDLING_RPM			900


#endif /* INC_SETTINGS_H_ */