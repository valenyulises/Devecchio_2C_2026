/*! @mainpage Template
 *
 * @section genDesc General Description
 *
 * This section describes how the program works.
 *
 * <a href="https://drive.google.com/...">Operation Example</a>
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	PIN_X	 	| 	GPIO_X		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 12/09/2023 | Document creation		                         |
 *
 * @author Albano Peñalva (albano.penalva@uner.edu.ar)
 *
 */



#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "switch.h"

/* Tiempo entre cada cambio de estado de los LEDs */
#define CONFIG_BLINK_PERIOD 500

void app_main(void)
{
    /* Variable donde se guarda el estado de las teclas */
    uint8_t teclas;

    /* Inicialización de los LEDs y las teclas de la placa */
    LedsInit();
    SwitchesInit();

    /* Bucle principal: se ejecuta continuamente */
    while(1)
    {
        /* Lee qué teclas están presionadas */
        teclas = SwitchesRead();

        /* Se analiza el estado de las teclas */
        switch(teclas)
        {
            /* Si solamente está presionada la tecla 1,
               titila LED_1 y los demás permanecen apagados */
            case SWITCH_1:
                LedToggle(LED_1);
                LedOff(LED_2);
                LedOff(LED_3);
            break;

            /* Si solamente está presionada la tecla 2,
               titila LED_2 y los demás permanecen apagados */
            case SWITCH_2:
                LedOff(LED_1);
                LedToggle(LED_2);
                LedOff(LED_3);
            break;

            /* Si están presionadas simultáneamente las teclas 1 y 2,
               titila LED_3 y se apagan LED_1 y LED_2 */
            case (SWITCH_1 | SWITCH_2):
                LedOff(LED_1);
                LedOff(LED_2);
                LedToggle(LED_3);
            break;

            /* Si no hay ninguna tecla presionada,
               se apagan todos los LEDs */
            default:
                LedOff(LED_1);
                LedOff(LED_2);
                LedOff(LED_3);
            break;
        }

        /* Espera 500 ms antes de volver a leer las teclas.
           Esto determina la velocidad de titilación */
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}

