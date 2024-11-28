//
// config.h
//
// compilation options for the Toolkit
//

// Set to 1 to use the RTOS scheduler
// Set to 0 to use a single main run loop
#define USE_RTOS_TASKS 1

// Set to 1 to use DREQ delays (yeilds) when
// running in RTOS mode
// This means that the VLSI handshaking will not block 
// other tasks
#define USE_RTOS_DREQ_DELAY 1

// Delay times are in milliseconds
#define RTOS_DELAY_SERVER_TASK      10
#define RTOS_DELAY_LISTENER_TASK    20
#define RTOS_DELAY_ENCODER_TASK     30  // smaller=fewer chirps

// Set to 1 to run the VLSI streaming tasks at higher 
// priority than the Web/WiFi tasks
// Set to 0 otherwise
// If the priority is higher then the VLSI tasks should
// still run even when the WiFi server is blocking
#define RTOS_HIPRIORITY_VLSI 1

//
// END OF config.h
