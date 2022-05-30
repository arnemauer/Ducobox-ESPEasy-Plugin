/*
 * Author: Arne Mauer
 */

#ifndef INBOXQMESSAGE_H_
#define INBOXQMESSAGE_H_

#include "DucoPacket.h"

class InboxQMessage{
	public:
        DucoPacket packet;   
        unsigned long timeReceivedMessage; // stores the time (millis) we received te message. If we can't process the message within 900ms stop processing the message.
        bool messageProcessed;
        InboxQMessage(){
            timeReceivedMessage = 0;
            messageProcessed = true;         
        }      
};
#endif /* INBOXQMESSAGE_H_ */