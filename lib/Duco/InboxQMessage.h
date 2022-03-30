/*
 * Author: Arne Mauer
 */

#ifndef INBOXQMESSAGE_H_
#define INBOXQMESSAGE_H_

#include "DucoPacket.h"


class InboxQMessage
{
	public:

        DucoPacket packet;   
        unsigned long time_received_message; // millis
        bool messageProcessed;
        InboxQMessage(){
                time_received_message = 0;
                messageProcessed = true;         
        }

        
};


#endif /* DUCOPACKET_H_ */