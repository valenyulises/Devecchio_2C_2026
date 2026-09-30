
/**
 * @mainpage Guía 2 - Ejercicio 1
 *
 * @section genDesc Descripción general
 *
 * El programa implementa un sistema de medición de distancia utilizando
 * el sensor ultrasónico HC-SR04.
 *
 * La aplicación utiliza tres tareas de FreeRTOS:
 *
 * - Una tarea para realizar la medición de distancia.
 * - Una tarea para controlar la visualización mediante LEDs y LCD.
 * - Una tarea para controlar las teclas TEC1 y TEC2.
 *
 * TEC1 permite activar o detener la medición.
 * TEC2 permite activar o desactivar el modo HOLD del LCD.
 *
 * @author Valentino De Vecchio
 */


/*==================[inclusions]=============================================*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led.h"
#include "switch.h"
#include "hc_sr04.h"
#include "lcditse0803.h"


/*==================[macros and definitions]===============================*/

/**
 * @def MEASURE_PERIOD
 * @brief Período de actualización de la medición de distancia en milisegundos.
 */
#define MEASURE_PERIOD  1000

/**
 * @def CONTROL_PERIOD
 * @brief Período de actualización de las tareas de control y visualización.
 */
#define CONTROL_PERIOD  20

/**
 * @def DEBOUNCE_PERIOD
 * @brief Tiempo utilizado para eliminar el rebote mecánico de los pulsadores.
 */
#define DEBOUNCE_PERIOD 50


/*==================[internal data definition]=============================*/

/**
 * @brief Última distancia medida por el sensor HC-SR04.
 *
 * La distancia se almacena en centímetros y es compartida entre
 * las tareas de medición y visualización.
 */
volatile uint16_t distancia = 0;

/**
 * @brief Indica si la medición de distancia se encuentra activa.
 *
 * true: medición activa.
 *
 * false: medición detenida.
 */
volatile bool medicion_activa = false;

/**
 * @brief Indica el estado del modo HOLD del LCD.
 *
 * true: el valor mostrado en el LCD permanece congelado.
 *
 * false: el LCD se actualiza normalmente.
 */
volatile bool hold = false;


/**
 * @brief Handle correspondiente a la tarea de medición de distancia.
 */
TaskHandle_t distance_task_handle = NULL;

/**
 * @brief Handle correspondiente a la tarea de visualización.
 */
TaskHandle_t display_task_handle = NULL;

/**
 * @brief Handle correspondiente a la tarea de control de teclas.
 */
TaskHandle_t control_task_handle = NULL;


/*==================[internal functions declaration]=======================*/


/*-----------------------------------------------------------------------
 * TAREA 1: MEDICIÓN DE DISTANCIA
 *-----------------------------------------------------------------------*/

/**
 * @fn static void DistanceTask(void *pvParameter)
 * @brief Tarea encargada de realizar la medición de distancia.
 *
 * Mientras la medición se encuentre activa, obtiene la distancia
 * utilizando el sensor HC-SR04 y actualiza la variable global
 * distancia cada un segundo.
 *
 * @param[in] pvParameter Parámetro de la tarea. No se utiliza en esta aplicación.
 *
 * @return No retorna ningún valor.
 */
static void DistanceTask(void *pvParameter)
{
    while (true)
    {
        /*
         * Solo mide cuando la medición está activa.
         */
        if (medicion_activa == true)
        {
            /* Lee distancia directamente en centímetros */
            distancia = HcSr04ReadDistanceInCentimeters();

            printf("Distancia: %u cm\n", distancia);
        }

        /*
         * Refresco de la medición cada 1 segundo.
         */
        vTaskDelay(MEASURE_PERIOD / portTICK_PERIOD_MS);
    }
}


/*-----------------------------------------------------------------------
 * TAREA 2: VISUALIZACIÓN
 *-----------------------------------------------------------------------*/

/**
 * @fn static void DisplayTask(void *pvParameter)
 * @brief Tarea encargada de controlar los LEDs y el LCD.
 *
 * Utiliza la distancia medida para encender los LEDs según distintos
 * rangos de distancia. Además, actualiza el LCD siempre que el modo
 * HOLD se encuentre desactivado.
 *
 * @param[in] pvParameter Parámetro de la tarea. No se utiliza en esta aplicación.
 *
 * @return No retorna ningún valor.
 */
static void DisplayTask(void *pvParameter)
{
    while (true)
    {
        /*
         * Si la medición está activa,
         * actualizamos los LEDs.
         */
        if (medicion_activa == true)
        {
            /*-----------------------------------------------------------
             * CONTROL DE LEDs
             *-----------------------------------------------------------*/

            if (distancia < 10)
            {
                LedOff(LED_1);
                LedOff(LED_2);
                LedOff(LED_3);
            }

            else if (distancia < 20)
            {
                LedOn(LED_1);
                LedOff(LED_2);
                LedOff(LED_3);
            }

            else if (distancia < 30)
            {
                LedOn(LED_1);
                LedOn(LED_2);
                LedOff(LED_3);
            }

            else
            {
                LedOn(LED_1);
                LedOn(LED_2);
                LedOn(LED_3);
            }


            /*-----------------------------------------------------------
             * CONTROL DEL LCD
             *-----------------------------------------------------------*/

            /*
             * Si HOLD está desactivado,
             * el LCD muestra continuamente la distancia actual.
             */
            if (hold == false)
            {
                LcdItsE0803Write(distancia);
            }
        }

        else
        {
            /*
             * Si TEC1 detuvo la medición,
             * todos los LEDs quedan apagados.
             */
            LedOff(LED_1);
            LedOff(LED_2);
            LedOff(LED_3);

            /*
             * Si no estamos en HOLD,
             * apagamos también el display.
             */
            if (hold == false)
            {
                LcdItsE0803Off();
            }
        }


        /*
         * Esta tarea revisa el estado frecuentemente.
         */
        vTaskDelay(CONTROL_PERIOD / portTICK_PERIOD_MS);
    }
}


/*-----------------------------------------------------------------------
 * TAREA 3: CONTROL DE TECLAS
 *-----------------------------------------------------------------------*/

/**
 * @fn static void ControlTask(void *pvParameter)
 * @brief Tarea encargada de controlar las teclas TEC1 y TEC2.
 *
 * TEC1 permite activar o detener la medición de distancia.
 *
 * TEC2 permite activar o desactivar el modo HOLD del LCD.
 *
 * Además, se implementa una espera hasta que la tecla sea liberada
 * y un tiempo de antirrebote para evitar múltiples detecciones
 * durante una sola pulsación.
 *
 * @param[in] pvParameter Parámetro de la tarea. No se utiliza en esta aplicación.
 *
 * @return No retorna ningún valor.
 */
static void ControlTask(void *pvParameter)
{
    int8_t teclas;

    while (true)
    {
        /* Leer las teclas */
        teclas = SwitchesRead();


        /*-----------------------------------------------------------
         * TEC1: activar / detener medición
         *-----------------------------------------------------------*/

        if (teclas == SWITCH_1)
        {
            /*
             * Cambia el estado de la medición.
             *
             * false -> true
             * true  -> false
             */
            medicion_activa = !medicion_activa;

            printf("Medicion activa: %d\n", medicion_activa);


            /*
             * Si acabamos de DETENER la medición
             * y HOLD no está activo:
             *
             * apagamos inmediatamente LEDs y LCD.
             */
            if ((medicion_activa == false) &&
                (hold == false))
            {
                LedOff(LED_1);
                LedOff(LED_2);
                LedOff(LED_3);

                LcdItsE0803Off();
            }


            /*
             * IMPORTANTE:
             *
             * Esperamos hasta que TEC1 sea liberada.
             * Así una sola pulsación produce UN solo cambio.
             */
            while (SwitchesRead() == SWITCH_1)
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }

            /*
             * Pequeño tiempo adicional para eliminar rebotes.
             */
            vTaskDelay(DEBOUNCE_PERIOD / portTICK_PERIOD_MS);
        }


        /*-----------------------------------------------------------
         * TEC2: activar / desactivar HOLD
         *-----------------------------------------------------------*/

        else if (teclas == SWITCH_2)
        {
            /*
             * Cambia el estado de HOLD.
             *
             * false -> true
             * true  -> false
             */
            hold = !hold;

            printf("HOLD: %d\n", hold);


            /*
             * Si acabamos de sacar HOLD,
             * actualizamos inmediatamente el LCD
             * con la distancia actual.
             */
            if ((hold == false) &&
                (medicion_activa == true))
            {
                LcdItsE0803Write(distancia);
            }


            /*
             * Esperamos hasta que TEC2 sea liberada.
             *
             * Así evitamos:
             *
             * HOLD true -> false -> true
             *
             * debido al rebote de una sola pulsación.
             */
            while (SwitchesRead() == SWITCH_2)
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }

            /*
             * Tiempo de antirrebote.
             */
            vTaskDelay(DEBOUNCE_PERIOD / portTICK_PERIOD_MS);
        }


        /*
         * La tarea vuelve a revisar las teclas cada 20 ms.
         */
        vTaskDelay(CONTROL_PERIOD / portTICK_PERIOD_MS);
    }
}


/*==================[external functions definition]========================*/


/**
 * @fn void app_main(void)
 * @brief Función principal de la aplicación.
 *
 * Inicializa los LEDs, las teclas, el sensor ultrasónico HC-SR04
 * y el LCD. Posteriormente crea las tres tareas de FreeRTOS:
 * medición, visualización y control.
 *
 * @return No retorna ningún valor.
 */
void app_main(void)
{
    /*-------------------------------------------------------------
     * INICIALIZACIÓN DEL HARDWARE
     *-------------------------------------------------------------*/

    /* Inicializa LEDs */
    LedsInit();

    /* Inicializa TEC1 y TEC2 */
    SwitchesInit();

    /*
     * Inicializa sensor ultrasónico:
     *
     * ECHO    -> GPIO_3
     * TRIGGER -> GPIO_2
     */
    HcSr04Init(GPIO_3, GPIO_2);

    /* Inicializa LCD de la cátedra */
    LcdItsE0803Init();


    /*-------------------------------------------------------------
     * CREACIÓN DE LAS TRES TAREAS
     *-------------------------------------------------------------*/

    xTaskCreate(
        &DistanceTask,
        "DistanceTask",
        2048,
        NULL,
        5,
        &distance_task_handle
    );

    xTaskCreate(
        &DisplayTask,
        "DisplayTask",
        2048,
        NULL,
        5,
        &display_task_handle
    );

    xTaskCreate(
        &ControlTask,
        "ControlTask",
        2048,
        NULL,
        5,
        &control_task_handle
    );
}