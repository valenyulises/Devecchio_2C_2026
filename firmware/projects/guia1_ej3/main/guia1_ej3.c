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

/* Modos posibles de funcionamiento */
#define ON      1
#define OFF     2
#define TOGGLE  3

/* Retardo base recomendado  */
#define DELAY_TIME 100


/* Estructura que contiene los datos necesarios
   para controlar un LED */
struct leds
{
    uint8_t mode;        /* Modo de funcionamiento: ON, OFF o TOGGLE */
    uint8_t n_led;       /* Número de LED a controlar */
    uint8_t n_ciclos;    /* Cantidad de cambios de estado */
    uint16_t periodo;    /* Tiempo entre cambios de estado */
};


/* Declaración de la función.
   Recibe un puntero a una estructura del tipo struct leds */
void ControlLed(struct leds *led);


void app_main(void)
{
    /* Inicialización de los LEDs */
    LedsInit();

    /* Se crea una estructura para configurar el LED */
    struct leds my_leds;

    /* Configuración:
       modo TOGGLE, LED 1, 10 ciclos y 500 ms */
    my_leds.mode = TOGGLE;
    my_leds.n_led = LED_1;
    my_leds.n_ciclos = 10;
    my_leds.periodo = 500;

    /* Se pasa la dirección de memoria de la estructura
       a la función ControlLed */
    ControlLed(&my_leds);
}


/* Función que controla el LED según los valores
   almacenados en la estructura */
void ControlLed(struct leds *led)
{
    uint8_t i;
    uint8_t j;
    uint16_t retardo;

    /* Se calcula cuántos retardos de 100 ms son necesarios
       para completar el período pedido */
    retardo = led->periodo / DELAY_TIME;


    /* ==================== MODO ON ==================== */

    if (led->mode == ON)
    {
        /* Según el número recibido, se enciende el LED correspondiente */
        switch (led->n_led)
        {
            case LED_1:
                LedOn(LED_1);
            break;

            case 2:
                LedOn(LED_2);
            break;

            case LED_3:
                LedOn(LED_3);
            break;
        }
    }


    /* ==================== MODO OFF ==================== */

    else if (led->mode == OFF)
    {
        /* Según el número recibido, se apaga el LED correspondiente */
        switch (led->n_led)
        {
            case 1:
                LedOff(LED_1);
            break;

            case LED_2:
                LedOff(LED_2);
            break;

            case 3:
                LedOff(LED_3);
            break;
        }
    }


    /* ================== MODO TOGGLE ================== */

    else if (led->mode == TOGGLE)
    {
        /* Se repite el cambio de estado la cantidad
           de veces indicada por n_ciclos */
        for (i = 0; i < led->n_ciclos; i++)
        {
            /* Se selecciona qué LED debe cambiar de estado */
            switch (led->n_led)
            {
                case LED_1:
                    LedToggle(LED_1);
                break;

                case LED_2:
                    LedToggle(LED_2);
                break;

                case LED_3:
                    LedToggle(LED_3);
                break;
            }

            /* Se genera el período mediante retardos de 100 ms */
            for (j = 0; j < retardo; j++)
            {
                vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
            }
        }
    }
}
