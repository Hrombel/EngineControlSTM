/*
 * settings.h
 *
 *  Created on: Dec 8, 2019
 *      Author: Администратор
 */

#ifndef INC_SETTINGS_H_
#define INC_SETTINGS_H_

#include <Arduino.h>

/* Максимальное количество одновременных подключений по CAN-шине */
#define MAX_CONNECTIONS 10

//#define PRIVATE_KEY "IV3RGmmSJGK2jXALuw9FPPpHbhn4uxOgE6efKiOaWRz8X0OkgTvlbmjxkJLGvcOk"
#define PRIVATE_KEY "password"
#define KEY_LENGTH (sizeof(PRIVATE_KEY) / sizeof(char))

 /** Сколько ждать после включения зажигания? */
#define IGNITION_TIME		7000
/** Максимальное время кручения стартера  */
#define STARTER_TIME 		2000
/** Время задержки после неудачной попытки запуска двигателя **/
#define STARTER_FAIL_DELAY	1000
/**  Время задержки перед выключением зажигания */
#define IGNITION_STOP_DELAY	1000
/* Минимальные обороты холостого хода двигателя */
#define IDLING_RPM			900
/* Размер буфера для приема данных по UART */
#define RX_BUFFER_SIZE      32



#define EN_PIN 5
#define TX_PIN 1
#define STARTER_PIN 12
#define IGNITION_PIN 14

#define TxSetOutput() pinMode(TX_PIN, OUTPUT)
#define TxWriteHigh() digitalWrite(TX_PIN, HIGH)
#define TxWriteLow() digitalWrite(TX_PIN, LOW)
#define UARTWriteByte(byte) Serial.write(byte)
#define UARTReadByte() Serial.read()
#define UARTBytesAvailable() Serial.available()
#define UARTReadBytes(buf, len) Serial.readBytes(buf, len)

#if RX_BUFFER_SIZE > 64
#define InitUART() Serial.begin(10400); Serial.setRxBufferSize(RX_BUFFER_SIZE - 64)
#else
#define InitUART() Serial.begin(19200)
#endif

void log(uint8_t);

#endif /* INC_SETTINGS_H_ */