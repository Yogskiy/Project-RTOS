#ifndef TASKS_H
#define TASKS_H

#include <Arduino.h>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
}

#include "data_structures.h"

// ============ GLOBAL SYNCHRONIZATION OBJECTS (defined in main.cpp) ============

// Queues
extern QueueHandle_t rfidDataQueue;   // inputTask  → authTask
extern QueueHandle_t eventLogQueue;   // authTask   → displayTask

// Semaphores
extern SemaphoreHandle_t rfidReadSemaphore;     // ISR → inputTask
extern SemaphoreHandle_t wifiConnectedSemaphore;

// Mutexes
extern SemaphoreHandle_t serialMutex;   // Protect Serial output

// Shared state
extern SecurityState securityState;

// ============ TASK DECLARATIONS ============

/**
 * @brief Input Task — waits for RFID ISR semaphore, reads UID, queues to authTask.
 * Priority: HIGH (3)   Period: 50 ms
 */
void vInputTask(void *pvParameters);

/**
 * @brief Auth Task — dequeues RFID UID, verifies with XAMPP server via HTTP,
 *        calculates rolling token, updates server, logs event.
 * Priority: HIGH (3)   Period: 100 ms (blocks on queue)
 */
void vAuthTask(void *pvParameters);

/**
 * @brief Comm Task — periodic WiFi health check and server heartbeat.
 * Priority: MEDIUM (2)   Period: 5000 ms
 */
void vCommTask(void *pvParameters);

/**
 * @brief Security Task — monitors failed attempts, manages lockout timer.
 * Priority: MEDIUM (2)   Period: 1000 ms
 */
void vSecurityTask(void *pvParameters);

/**
 * @brief Display Task — reads eventLogQueue, updates 16x2 LCD.
 * Priority: LOW (1)   Period: 250 ms
 */
void vDisplayTask(void *pvParameters);

// ============ ISR ============

/**
 * @brief Minimal RFID interrupt — only gives rfidReadSemaphore.
 */
void rfidISR();

// ============ SETUP HELPERS ============

void initializeSynchronization();
void createAllTasks();
void printTaskStats();

#endif // TASKS_H