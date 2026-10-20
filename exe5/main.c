/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>


#include <stdio.h>
#include <string.h> 
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

const int BTN_PIN_R = 28;
const int BTN_PIN_Y = 21;

const int LED_PIN_R = 5;
const int LED_PIN_Y = 10;

QueueHandle_t xQueueBtn;
SemaphoreHandle_t xSemaphoreLedR;
SemaphoreHandle_t xSemaphoreLedY;

void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x4) { // fall edge
        xQueueSendFromISR(xQueueBtn, &gpio, 0);
    }
}

void btn_task(void* p) {
    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);

    gpio_init(BTN_PIN_Y);
    gpio_set_dir(BTN_PIN_Y, GPIO_IN);
    gpio_pull_up(BTN_PIN_Y);

    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);
    gpio_set_irq_enabled(BTN_PIN_Y, GPIO_IRQ_EDGE_FALL, true);

    uint gpio;
    while (true) {
        if (xQueueReceive(xQueueBtn, &gpio, portMAX_DELAY) == pdTRUE) {
            if (gpio == BTN_PIN_R) {
                xSemaphoreGive(xSemaphoreLedR);
            } else if (gpio == BTN_PIN_Y) {
                xSemaphoreGive(xSemaphoreLedY);
            }
        }
    }
}

void led_r_task(void* p) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    gpio_put(LED_PIN_R, 0);

    bool blinking = false;
    while (true) {
        if (!blinking) {
            // apagado: espera o botao para comecar a piscar
            xSemaphoreTake(xSemaphoreLedR, portMAX_DELAY);
            blinking = true;
        }

        // usa o timeout do semaforo como delay de 100 ms, assim o
        // botao e' atendido na hora, mesmo no meio de uma piscada
        gpio_put(LED_PIN_R, 1);
        if (xSemaphoreTake(xSemaphoreLedR, pdMS_TO_TICKS(100)) == pdTRUE) {
            blinking = false;
        }
        gpio_put(LED_PIN_R, 0);
        if (blinking &&
            xSemaphoreTake(xSemaphoreLedR, pdMS_TO_TICKS(100)) == pdTRUE) {
            blinking = false;
        }
    }
}

void led_y_task(void* p) {
    gpio_init(LED_PIN_Y);
    gpio_set_dir(LED_PIN_Y, GPIO_OUT);
    gpio_put(LED_PIN_Y, 0);

    bool blinking = false;
    while (true) {
        if (!blinking) {
            xSemaphoreTake(xSemaphoreLedY, portMAX_DELAY);
            blinking = true;
        }

        gpio_put(LED_PIN_Y, 1);
        if (xSemaphoreTake(xSemaphoreLedY, pdMS_TO_TICKS(100)) == pdTRUE) {
            blinking = false;
        }
        gpio_put(LED_PIN_Y, 0);
        if (blinking &&
            xSemaphoreTake(xSemaphoreLedY, pdMS_TO_TICKS(100)) == pdTRUE) {
            blinking = false;
        }
    }
}

int main() {
    stdio_init_all();

    xQueueBtn = xQueueCreate(32, sizeof(uint));
    xSemaphoreLedR = xSemaphoreCreateBinary();
    xSemaphoreLedY = xSemaphoreCreateBinary();

    xTaskCreate(btn_task, "BTN_Task 1", 256, NULL, 1, NULL);
    xTaskCreate(led_r_task, "LED_R_Task", 256, NULL, 1, NULL);
    xTaskCreate(led_y_task, "LED_Y_Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while(1){}

    return 0;
}
