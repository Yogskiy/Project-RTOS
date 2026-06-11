#include "tasks.h"
#include "config.h"
#include "security.h"
#include <Arduino.h>

/**
 * @brief Input Task - menunggu interrupt RFID, membaca UID, lalu mengirim data.
 * Priority: HIGH (3)
 * Period: 50ms
 * 
 * Workflow:
 * 1. Menunggu rfidReadSemaphore yang diberikan oleh rfidISR().
 * 2. Membaca UID dari reader RFID di task context, bukan di ISR.
 * 3. Mengirim payload RFIDData ke rfidDataQueue untuk AuthTask.
 * 4. Menjaga periode loop dengan vTaskDelayUntil().
 *
 * Desain RTOS:
 * - ISR hanya memberi semaphore; task ini mengerjakan I/O yang bisa lambat.
 * - Queue memisahkan timing pembacaan kartu dari timing HTTP AuthTask.
 */
void vInputTask(void *pvParameters) {
    // Buffer lokal aman karena xQueueSend() menyalin isi struct ke queue.
    RFIDData rfidData;
    BaseType_t xStatus;

    // Basis waktu untuk vTaskDelayUntil(); menjaga periode relatif stabil
    // walaupun waktu eksekusi di dalam loop sedikit berubah.
    TickType_t xLastWakeTime = xTaskGetTickCount();

    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
    Serial.println("[Input] Task started - waiting for RFID data...");
    xSemaphoreGive(serialMutex);

    while (1) {
        // Menunggu sinyal dari ISR dengan timeout 50 ms. Timeout membuat task
        // tetap bangun periodik sehingga delayUntil di bawah tidak kehilangan ritme.
        if (xSemaphoreTake(rfidReadSemaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Simulasi pembacaan RFID. Pada hardware asli, blok ini diganti dengan
            // transaksi SPI/I2C ke reader, tetap di task context agar aman blocking.
            static uint8_t test_uid[4] = {0x12, 0x34, 0x56, 0x78};
            
            // Produksi: baca UID dari reader RFID via SPI.
            // uint8_t uid[4];
            // rfid_read_uid(uid);
            
            // Timestamp memakai millis() agar AuthTask/DisplayTask tahu kapan
            // kartu terdeteksi tanpa harus membaca waktu ulang.
            rfidData.timestamp = millis();
            rfidData.uid[0] = test_uid[0];
            rfidData.uid[1] = test_uid[1];
            rfidData.uid[2] = test_uid[2];
            rfidData.uid[3] = test_uid[3];

            // Kirim ke AuthTask. Timeout 10 ms membatasi blocking bila queue penuh,
            // sehingga task input tidak tersandera oleh proses autentikasi lambat.
            xStatus = xQueueSend(rfidDataQueue, &rfidData, pdMS_TO_TICKS(10));

            if (xStatus == pdPASS) {
                // Serial adalah resource bersama antar-task; mutex menjaga log UID
                // tidak bercampur dengan log Auth/Comm/Security.
                xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                Serial.printf("[Input] UID detected: %02X%02X%02X%02X at %ld ms\n",
                              rfidData.uid[0], rfidData.uid[1],
                              rfidData.uid[2], rfidData.uid[3],
                              rfidData.timestamp);
                xSemaphoreGive(serialMutex);
            } else {
                // Queue penuh berarti AuthTask belum sempat mengonsumsi data.
                // UID dibuang eksplisit agar sistem tetap responsif.
                xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                Serial.println("[Input] ERROR: Queue full, dropped UID");
                xSemaphoreGive(serialMutex);
            }
        }

        // Delay periodik berbasis tick absolut; lebih stabil dibanding vTaskDelay()
        // karena drift loop dikoreksi oleh FreeRTOS.
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(INPUT_TASK_PERIOD));
    }
}
