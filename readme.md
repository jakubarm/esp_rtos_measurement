# ESP RTOS Measurement

The project implements the measurement of the operations of FreeRTOS on ESP32 utilizing ESP-IDF. The measurement scenarios are modified to use multi-core (tasks on different cores or force rescheduling).

## Measured operations

Context switching time - the time between the semaphore give (low priority task) and semaphor take (high priority task).
Periodic task jitter - the precision of task delay when forced the rescheduling to another core.