/*
 * Author: Arne Mauer
 */

#ifndef OUTBOXQMESSAGE_H_
#define OUTBOXQMESSAGE_H_

#include "DucoPacket.h"

class OutboxQMessage{
    public:
		DucoPacket packet;   
		bool hasSent;
		bool waitForAck;
		bool ackReceived;
		unsigned long ackTimer;
		uint8_t sendRetries;
        OutboxQMessage(){
			hasSent = true;
			waitForAck = false;
			ackReceived = true;
			ackTimer = 0;
			sendRetries = 0;               
        }
};


#endif /* OUTBOXQMESSAGE_H_ */