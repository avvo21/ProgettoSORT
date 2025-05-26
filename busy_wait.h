#ifndef BUSY_WAIT
#define BUSY_WAIT

// does "busy wait" calibration
void busy_wait_init();

// does a "busy wait", consuming cpu time until the given interval is passed (*)
void busy_wait(unsigned int millisec);

#endif

