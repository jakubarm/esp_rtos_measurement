/*
    Multi-core measurement
    Jakub Arm
    Brno University of Technology
    12.1.2021
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "xtensa/core-macros.h"
#include <sstream>
#include <vector>
#include <math.h>

static const char *TAG = "main";

#define SEMAPHORE
//#define JITTER
//#define GRANULARITY

static const int TASK1_PRIORITY = 4;
static const int TASK_STACK = 32768;
static const int TASK1_PERIOD_MS = 10;
static const int TASK1_CPU = 0;
static TaskHandle_t	task1;

static const int TASK2_PRIORITY = 2;
static const int TASK2_PERIOD_MS = 20;
static const int TASK2_CPU = 0;//1 single x multi core
static TaskHandle_t	task2;

static const int TASK3_PRIORITY = 6;
static const int TASK3_PERIOD_MS = 1;
static const int TASK3_CPU = 0;
static TaskHandle_t	task3;

static const int TASK4_PRIORITY = 6;
static const int TASK4_PERIOD_MS = 1;
static const int TASK4_CPU = 1;
static TaskHandle_t	task4;

static const int TASK5_PRIORITY = 10;
static const int TASK5_PERIOD_MS = 1000;
static const int TASK5_CPU = 1;
static TaskHandle_t	task5;

static SemaphoreHandle_t semaphore;
static SemaphoreHandle_t semaphore2;

static const int MEASURE_COUNT = 1000;
struct Measurement {
    std::string name;
    uint32_t counter;
    int start;
    int end;
    int values[MEASURE_COUNT];
    double average;
    double dispersion;
    int min;
    int max;
};

static volatile struct Measurement measureSemaphore = {
    .name = "Sema",
    .counter = 0,
};
static struct Measurement measurePeriodic1 = {
    .name = "Per1",
    .counter = 0,
};
static struct Measurement measurePeriodic2 = {
    .name = "Per2",
    .counter = 0,
};

static void task1_setup(void);
static void task1_func(void* pvParameters);
static void task1_loop(void);
static void task2_setup(void);
static void task2_func(void* pvParameters);
static void task2_loop(void);
static void task3_setup(void);
static void task3_func(void* pvParameters);
static void task4_setup(void);
static void task4_func(void* pvParameters);
static void task5_setup(void);
static void task5_func(void* pvParameters);
static void evaluation(Measurement& measurement);
static void add(Measurement& measurement);

extern "C" {
    void app_main(void);
}

void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    #if defined (JITTER) || defined(SEMAPHORE)
    vSemaphoreCreateBinary(semaphore);
    vSemaphoreCreateBinary(semaphore2);
    task1_setup();
    #endif
    #ifdef JITTER
    task4_setup();
    task3_setup();
    #endif
    #ifdef SEMAPHORE
    task2_setup();
    #endif
    #ifdef GRANULARITY
    task5_setup();
    #endif
}

static void task1_setup(void)
{
    #ifdef SEMAPHORE
    BaseType_t ret = xTaskCreatePinnedToCore(&task1_func, "task1", TASK_STACK, NULL, TASK1_PRIORITY, &task1, TASK1_CPU);
    #endif
    #ifdef JITTER
    BaseType_t ret = xTaskCreate(&task1_func, "task1", TASK_STACK, NULL, TASK1_PRIORITY, &task1);
    #endif
    #if defined (JITTER) || defined(SEMAPHORE)
    if (ret != pdPASS) {
    }
    #endif
}

static void task1_func(void* pvParameters)
{
    #ifdef SEMAPHORE
    for( ;; ) {
        vTaskDelay(pdMS_TO_TICKS(TASK1_PERIOD_MS));
        task1_loop();
    }
    #endif

    #ifdef JITTER
    TickType_t xLastWakeTime = xTaskGetTickCount ();
    measurePeriodic1.start = XTHAL_GET_CCOUNT();
    for( ;; ) {
        vTaskDelayUntil(&xLastWakeTime, TASK1_PERIOD_MS / portTICK_PERIOD_MS);

        measurePeriodic1.end = XTHAL_GET_CCOUNT();
        add(measurePeriodic1);
        measurePeriodic1.start = XTHAL_GET_CCOUNT();
        int core = xPortGetCoreID();
        //ESP_LOGE(TAG, "Core: %d", core);
        if (core == 0) {
            vTaskSuspend(task4);
            vTaskResume(task3);
        } else {
            vTaskSuspend(task3);
            vTaskResume(task4);
        }
    }
    #endif
}

static void task1_loop(void)
{
    if (semaphore == NULL) {
        ESP_LOGE(TAG, "Called before semaphore1 created");
        return;
    }

    if (xSemaphoreTake(semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
        measureSemaphore.end = XTHAL_GET_CCOUNT();
        xSemaphoreGive(semaphore2);
    } else {
        ESP_LOGE(TAG, "Could not get semaphore1");
    }
}

static void task2_setup(void)
{
    BaseType_t ret = xTaskCreatePinnedToCore(&task2_func, "task2", TASK_STACK, NULL, TASK2_PRIORITY, &task2, TASK2_CPU);
    if (ret != pdPASS) {
    }
}

static void task2_func(void* pvParameters)
{
    #ifdef JITTER
    TickType_t xLastWakeTime = xTaskGetTickCount ();
    measurePeriodic2.start = XTHAL_GET_CCOUNT();
    for( ;; ) {
        vTaskDelayUntil(&xLastWakeTime, TASK2_PERIOD_MS / portTICK_PERIOD_MS);

        measurePeriodic2.end = XTHAL_GET_CCOUNT();
        add(measurePeriodic2);
        measurePeriodic2.start = XTHAL_GET_CCOUNT();
    }
    #endif

    #ifdef SEMAPHORE
    for( ;; ) {
        vTaskDelay(pdMS_TO_TICKS(TASK2_PERIOD_MS));
        task2_loop();
    }
    #endif
}

static void task2_loop(void)
{
    if (semaphore2 == NULL) {
        ESP_LOGE(TAG, "Called before semaphore2 created");
        return;
    }

    measureSemaphore.start = XTHAL_GET_CCOUNT();
    xSemaphoreGive(semaphore);

    if (xSemaphoreTake(semaphore2, pdMS_TO_TICKS(50)) == pdTRUE) {
        add(const_cast<Measurement&>(measureSemaphore));
    } else {
        ESP_LOGE(TAG, "Could not get semaphore2");
    }
}

static void task3_setup(void)
{
    BaseType_t ret = xTaskCreatePinnedToCore(&task3_func, "task3", TASK_STACK, NULL, TASK3_PRIORITY, &task3, TASK3_CPU);
    if (ret != pdPASS) {
    }
}

static void task3_func(void* pvParameters)
{
    long a = 0;
    for( ;; ) {
        vTaskDelay(pdMS_TO_TICKS(5));
        for (int i = 0; i < 10; i++)
            a = i * 2;
    }
}

static void task4_setup(void)
{
    BaseType_t ret = xTaskCreatePinnedToCore(&task4_func, "task4", TASK_STACK, NULL, TASK4_PRIORITY, &task4, TASK4_CPU);
    if (ret != pdPASS) {
    }
}

static void task4_func(void* pvParameters)
{
    long a = 0;
    vTaskSuspend(task4);
    for( ;; ) {
        vTaskDelay(pdMS_TO_TICKS(5));
        for (int i = 0; i < 10; i++)
            a = i * 2;
    }
}

static void task5_setup(void)
{
    BaseType_t ret = xTaskCreatePinnedToCore(&task5_func, "task5", TASK_STACK, NULL, TASK5_PRIORITY, &task5, TASK5_CPU);
    if (ret != pdPASS) {
    }
}

static void task5_func(void* pvParameters)
{
    int results[1000];
    int start, end = 0;
    std::ostringstream res;
    for( ;; ) {
        vTaskDelay(pdMS_TO_TICKS(TASK5_PERIOD_MS));
        for (int i = 0; i < 1000; i++) {
            if (i != 0)
                res << ";";

            start = XTHAL_GET_CCOUNT();
            end = XTHAL_GET_CCOUNT();
            results[i] = end - start;
            res << results[i];
        }
        ESP_LOGW(TAG, "Timer granularity: %s", res.str().c_str());
    }
}

static void add(Measurement& measurement)
{
    int delta = measurement.end - measurement.start;

    measurement.values[measurement.counter] = delta;

    measurement.counter++;

    if (measurement.counter > MEASURE_COUNT) {
        measurement.counter = 0;
        evaluation(measurement);
    }
}

static void evaluation(Measurement& measurement)
{
    //std::ostringstream res_semaphore;
    for(int i = 0; i < MEASURE_COUNT; i++)
    {
        if (i != 0) {
            //res_semaphore << ";";

            if (measurement.values[i] < measurement.min) {
                measurement.min = measurement.values[i];
            }
            if (measurement.values[i] > measurement.max) {
                measurement.max = measurement.values[i];
            }
            measurement.average += (double)measurement.values[i];
        } else {
            measurement.min = measurement.values[i];
            measurement.max = measurement.values[i];
            measurement.average = (double)measurement.values[i];
        }
        //res_semaphore << values_semaphore[i];
    }
    measurement.average /= MEASURE_COUNT;
    for(int i = 0; i < MEASURE_COUNT; i++)
    {
        measurement.dispersion += pow(measurement.values[i] - measurement.average, 2);
    }
    measurement.dispersion /= MEASURE_COUNT * (MEASURE_COUNT - 1);
    measurement.dispersion = pow(measurement.dispersion, 0.5);

    measurement.average /= ets_get_cpu_frequency()*1000;
    measurement.dispersion /= ets_get_cpu_frequency()*1000;
    double min = (double)measurement.min / (ets_get_cpu_frequency()*1000);
    double max = (double)measurement.max / (ets_get_cpu_frequency()*1000);
    ESP_LOGW(TAG, "%s - Average: %.3f Disperse: %.3f Min: %.3f Max: %.3f", measurement.name.c_str(), measurement.average, measurement.dispersion, min, max);
}