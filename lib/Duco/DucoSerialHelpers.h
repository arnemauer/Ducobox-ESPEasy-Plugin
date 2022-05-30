//#################################### Duco Serial Helpers ##################################
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################
#ifndef DUCO_SERIAL_HELPER_H
#define DUCO_SERIAL_HELPER_H

#include <stdint.h>
class String;

// define buffers, large, indeed. The entire datagram checksum will be checked at once
#define DUCO_SERIAL_BUFFER_SIZE 180 // duco networklist is max 150 chars
extern uint8_t duco_serial_buf[DUCO_SERIAL_BUFFER_SIZE];
extern unsigned int duco_serial_bytes_read;
extern unsigned int duco_serial_rowCounter; 

extern uint8_t serialSendCommandCurrentChar;
extern bool serialSendCommandInProgress;
/* To support non-blocking code, each plugins checks if the serial port is in use by another task before 
claiming the serial port. After a task is done using the serial port the variable 'serialPortInUseByTask'
is set to a default value '255'. 

If espeasy calling 'PLUGIN_READ' from a task, it is possible that the serual port is in use.
If that is the case the task will set a flag for itself. In 'PLUGIN_ONCE_A_SECOND' the task will check if
there is a flag and if so, check if the serial port is in use. When the serial port isnt used by a 
plugin (serialPortInUseByTask=255) than it will call 'PLUGIN_READ' to start receiving data.
*/

extern uint8_t serialPortInUseByTask; 
extern unsigned long ducoSerialStartReading; // filled with millis 
extern char serialSendCommand[30]; // sensorinfo, network, nodeparaget xx xx

typedef enum {
    DUCO_MESSAGE_END = 1, // status after receiving the end of a message (0x0d 0x20 0x20 )
    DUCO_MESSAGE_ROW_END = 2, // end of a row
    DUCO_MESSAGE_TIMEOUT = 3,
    DUCO_MESSAGE_ARRAY_OVERFLOW = 4,
    DUCO_MESSAGE_FIFO_EMPTY = 5,
} DucoSerialMessageStatus;

void DucoSerialStartSendCommand(const char *command);
void DucoSerialSendCommand(String logPrefix);
void DucoSerialFlush();
uint8_t DucoSerialInterrupt();
void DucoTaskStopSerial(String logPrefix);
void DucoThrowErrorMessage(String logPrefix, uint8_t messageStatus);
bool DucoSerialCheckCommandInResponse(String logPrefix, const char* command);
void DucoSerialLogArray(String logPrefix, uint8_t array[], unsigned int len, int fromByte);

#endif // DUCO_SERIAL_HELPER_H
