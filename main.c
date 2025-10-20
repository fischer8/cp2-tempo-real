#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_task_wdt.h"z
#define STUDENT_TAG "{Henrique Fischer-RM:87352}"
#define QUEUE_LENGTH 5
#define TIMEOUT_RECEIVE_MS 2000
#define SUPERVISOR_DELAY_MS 3000
#define WATCHDOG_TIMEOUT_SEC 4
#define GENERATE_PERIOD_MS 1000

static const char *LOG_TAG = "CONCURRENT_SYSTEM";

static QueueHandle_t queue_handle;

volatile bool generator_status = true;
volatile bool receiver_status = true;

static void task_generator(void *arg);
static void task_receiver(void *arg);
static void task_supervisor(void *arg);

void app_main(void)
{
    ESP_LOGI(LOG_TAG, STUDENT_TAG " Sistema inicializando...");

    queue_handle = xQueueCreate(QUEUE_LENGTH, sizeof(int));
    if (queue_handle == NULL) {
        ESP_LOGE(LOG_TAG, STUDENT_TAG " Falha ao criar fila!");
        return;
    }

    xTaskCreate(task_generator, "TaskGen", 2048, NULL, 5, NULL);
    xTaskCreate(task_receiver, "TaskRecv", 2048, NULL, 5, NULL);
    xTaskCreate(task_supervisor, "TaskSup", 2048, NULL, 3, NULL);

    ESP_LOGI(LOG_TAG, STUDENT_TAG " Todas as tarefas criadas.");
}

static void task_generator(void *arg)
{
    esp_task_wdt_add(NULL);
    int counter = 0;

    while (true)
    {
        esp_task_wdt_reset();

        if (xQueueSend(queue_handle, &counter, 0) == pdPASS) {
            ESP_LOGI(LOG_TAG, STUDENT_TAG " [GERADOR] Enviado valor: %d", counter);
        } else {
            ESP_LOGW(LOG_TAG, STUDENT_TAG " [GERADOR] Fila cheia, valor %d descartado", counter);
        }

        counter++;
        generator_status = true;
        vTaskDelay(pdMS_TO_TICKS(GENERATE_PERIOD_MS));
    }
}

static void task_receiver(void *arg)
{
    esp_task_wdt_add(NULL);
    int received_data;
    int consecutive_timeouts = 0;

    while (true)
    {
        esp_task_wdt_reset();

        if (xQueueReceive(queue_handle, &received_data, pdMS_TO_TICKS(TIMEOUT_RECEIVE_MS)) == pdPASS) {
            consecutive_timeouts = 0;
            receiver_status = true;

            int *buffer = malloc(sizeof(int));
            if (buffer != NULL) {
                *buffer = received_data;
                ESP_LOGI(LOG_TAG, STUDENT_TAG " [RECEPTOR] Dado recebido: %d", *buffer);
                free(buffer);
            } else {
                ESP_LOGE(LOG_TAG, STUDENT_TAG " [RECEPTOR] Falha na alocação de memória!");
                receiver_status = false;
            }
        } else {
            consecutive_timeouts++;
            receiver_status = false;
            switch (consecutive_timeouts) {
                case 1:
                    ESP_LOGW(LOG_TAG, STUDENT_TAG " [RECEPTOR] Timeout 1 - Sem dados");
                    break;
                case 2:
                    ESP_LOGE(LOG_TAG, STUDENT_TAG " [RECEPTOR] Timeout 2 - Sistema possivelmente lento");
                    break;
                default:
                    ESP_LOGE(LOG_TAG, STUDENT_TAG " [RECEPTOR] Timeout 3 - Possível travamento do gerador");
                    break;
            }
        }
    }
}

static void task_supervisor(void *arg)
{
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(SUPERVISOR_DELAY_MS));

        const char *gen_status = generator_status ? "OK" : "FALHA";
        const char *recv_status = receiver_status ? "OK" : "AVISO";

        printf("\n");
        printf("--- [SUPERVISOR] Status do Sistema | %s ---\n", STUDENT_TAG);
        printf("    Gerador:      %s\n", gen_status);
        printf("    Receptor:     %s\n", recv_status);
        printf("    Heap disponível: %ld bytes\n", esp_get_free_heap_size());
        printf("------------------------------------------------------------\n\n");
    }
}
