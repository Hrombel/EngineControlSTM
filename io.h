/*
 * io.h
 *
 *  Created on: Dec 8, 2019
 *      Author: Администратор
 */

#ifndef INC_IO_H_
#define INC_IO_H_

#include "engine_connector.h"

typedef unsigned REMOTE_ID;

typedef struct RemoteConnection {
	REMOTE_ID id;
	HCMD callId;

} RemoteConnection;

typedef struct CANRequest {
	REMOTE_ID clientId;


} CANRequest;


#endif /* INC_IO_H_ */