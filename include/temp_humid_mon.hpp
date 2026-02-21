#ifndef _TEMP_HUMID_MON
#define _TEMP_HUMID_MON

#include "global.hpp"

extern void taskMonitorTempHumid(void *pvParameters);

// TempHumidMon produce a Queue notifying temperature/humidity changes, so who ever need to access the Queue need to 
// call this function to increase the receiver count.
void tempHumidMonQueueReceiverCountInc();

#endif //_TEMP_HUMID_MON