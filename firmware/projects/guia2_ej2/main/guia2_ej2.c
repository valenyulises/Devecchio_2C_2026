
/**
 * @file guia2_ej2.c
 * @brief Medidor de distancia por ultrasonido utilizando interrupciones y timer.
 *
 * @details
 * La aplicación mide distancia mediante el sensor ultrasónico HC-SR04.
 * La medición puede activarse o detenerse mediante TEC1, mientras que TEC2
 * permite activar o desactivar el modo HOLD del display.
 *
 * El control de las teclas se realiza mediante interrupciones y el período
 * de medición se controla mediante TIMER_A.
 *
 * La aplicación utiliza dos tareas de FreeRTOS:
 * - DistanceTask: realiza la medición de distancia.
 * - DisplayTask: actualiza los LEDs y el display LCD.
 *
 * @author Valentino Devecchio
 * @date 2026
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led.h"
#include "switch.h"
#include "hc_sr04.h"
#include "lcditse0803.h"
#include "timer_mcu.h"


/*==================[macros and definitions]===============================*/

/**
 * @brief Período de medición del sensor ultrasónico.
 *
 * El valor está expresado en microsegundos.
 * 1000000 us = 1 segundo.
 */
#define MEASURE_PERIOD_US 1000000


/*==================[internal data definition]=============================*/

/**
 * @brief Última distancia medida por el sensor HC-SR04.
 *
 * El valor se encuentra expresado en centímetros.
 */
volatile uint16_t distancia = 0;

/**
 * @brief Indica si la medición de distancia se encuentra activa.
 *
 * - true: la medición está activa.
 * - false: la medición está detenida.
 */
volatile bool medicion_activa = false;

/**
 * @brief Indica el estado del modo HOLD.
 *
 * - true: el valor mostrado en el LCD queda congelado.
 * - false: el LCD se actualiza con las nuevas mediciones.
 */
volatile bool hold = false;


/**
 * @brief Handle de la tarea encargada de medir la distancia.
 */
TaskHandle_t distance_task_handle = NULL;

/**
 * @brief Handle de la tarea encargada de la visualización.
 */
TaskHandle_t display_task_handle = NULL;


/*==================[interrupt functions]=================================*/

/**
 * @brief Función ejecutada por la interrupción del TIMER_A.
 *
 * Cada vez que vence el período del timer se envía una notificación
 * a DistanceTask para realizar una nueva medición.
 *
 * @param param Puntero a parámetros de la función. No utilizado.
 */
void FuncTimerA(void *param)
{
    vTaskNotifyGiveFromISR(
        distance_task_handle,
        pdFALSE
    );
}


/**
 * @brief Función ejecutada por la interrupción de TEC1.
 *
 * Cambia el estado de la medición entre activa y detenida.
 * Luego notifica a DisplayTask para actualizar inmediatamente
 * el estado de los LEDs y del LCD.
 *
 * @param param Puntero a parámetros de la función. No utilizado.
 */
void FuncTec1(void *param)
{
    medicion_activa = !medicion_activa;

    /*
     * Avisamos a DisplayTask para que reaccione
     * inmediatamente al cambio.
     */
    vTaskNotifyGiveFromISR(
        display_task_handle,
        pdFALSE
    );
}


/**
 * @brief Función ejecutada por la interrupción de TEC2.
 *
 * Cambia el estado del modo HOLD. Luego notifica a DisplayTask
 * para que actualice el estado correspondiente del LCD.
 *
 * @param param Puntero a parámetros de la función. No utilizado.
 */
void FuncTec2(void *param)
{
    hold = !hold;

    /*
     * Avisamos a DisplayTask para actualizar
     * inmediatamente el estado del LCD.
     */
    vTaskNotifyGiveFromISR(
        display_task_handle,
        pdFALSE
    );
}


/*==================[tasks]================================================*/

/**
 * @brief Tarea encargada de realizar la medición de distancia.
 *
 * La tarea permanece bloqueada esperando una notificación proveniente
 * de la interrupción del TIMER_A. Cuando recibe la notificación,
 * verifica si la medición está activa y, en ese caso, obtiene la
 * distancia utilizando el sensor HC-SR04.
 *
 * Después de obtener una nueva medición, notifica a DisplayTask
 * para actualizar la visualización.
 *
 * @param pvParameter Parámetro recibido por la tarea. No utilizado.
 */
static void DistanceTask(void *pvParameter)
{
    while (true)
    {
        /*
         * La tarea queda bloqueada acá.
         *
         * No usa vTaskDelay().
         *
         * Se despierta cuando FuncTimerA()
         * envía una notificación.
         */
        ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY
        );


        /*
         * Solamente se mide si TEC1 dejó
         * la medición activa.
         */
        if (medicion_activa == true)
        {
            distancia =
                HcSr04ReadDistanceInCentimeters();

            printf(
                "Distancia: %u cm\n",
                distancia
            );


            /*
             * Como tenemos una nueva distancia,
             * despertamos la tarea de visualización.
             */
            xTaskNotifyGive(
                display_task_handle
            );
        }
    }
}


/**
 * @brief Tarea encargada de actualizar los LEDs y el display LCD.
 *
 * La tarea permanece bloqueada hasta recibir una notificación.
 * Si la medición está activa, los LEDs se actualizan según la
 * distancia medida:
 *
 * - Distancia menor a 10 cm: todos los LEDs apagados.
 * - Distancia entre 10 y 20 cm: LED 1 encendido.
 * - Distancia entre 20 y 30 cm: LED 1 y LED 2 encendidos.
 * - Distancia mayor o igual a 30 cm: los tres LEDs encendidos.
 *
 * Si el modo HOLD está desactivado, el LCD muestra la última
 * distancia medida. Si la medición se encuentra detenida, los
 * LEDs se apagan y, siempre que HOLD no esté activo, también
 * se apaga el LCD.
 *
 * @param pvParameter Parámetro recibido por la tarea. No utilizado.
 */
static void DisplayTask(void *pvParameter)
{
    while (true)
    {
        /*
         * La tarea espera hasta recibir
         * una notificación.
         */
        ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY
        );


        /*
         * Si la medición está activa,
         * actualizamos LEDs y LCD.
         */
        if (medicion_activa == true)
        {
            /*------------------ LEDs ------------------*/

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


            /*------------------ LCD ------------------*/

            /*
             * Si HOLD está desactivado,
             * actualizamos el display.
             */
            if (hold == false)
            {
                LcdItsE0803Write(
                    distancia
                );
            }
        }

        else
        {
            /*
             * Si TEC1 detuvo la medición,
             * apagamos LEDs.
             */
            LedOff(LED_1);
            LedOff(LED_2);
            LedOff(LED_3);


            /*
             * Si no estamos en HOLD,
             * también apagamos el LCD.
             */
            if (hold == false)
            {
                LcdItsE0803Off();
            }
        }
    }
}


/*==================[external functions definition]========================*/

/**
 * @brief Función principal de la aplicación.
 *
 * Inicializa los periféricos utilizados por el sistema:
 * LEDs, teclas, sensor ultrasónico HC-SR04 y display LCD.
 *
 * Luego crea las tareas DistanceTask y DisplayTask, configura
 * las interrupciones correspondientes a TEC1 y TEC2, y configura
 * TIMER_A con un período de un segundo para controlar la frecuencia
 * de las mediciones.
 */
void app_main(void)
{
    /*---------------- Inicialización de hardware ----------------*/

    LedsInit();

    SwitchesInit();

    HcSr04Init(
        GPIO_3,
        GPIO_2
    );

    LcdItsE0803Init();


    /*---------------- Creación de tareas ----------------*/

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


    /*---------------- Interrupciones de teclas ----------------*/

    SwitchActivInt(
        SWITCH_1,
        FuncTec1,
        NULL
    );

    SwitchActivInt(
        SWITCH_2,
        FuncTec2,
        NULL
    );


    /*---------------- Configuración del timer ----------------*/

    timer_config_t timer_medicion = {

        .timer = TIMER_A,

        .period = MEASURE_PERIOD_US,

        .func_p = FuncTimerA,

        .param_p = NULL
    };


    /*
     * Inicializamos el timer.
     */
    TimerInit(
        &timer_medicion
    );


    /*
     * Iniciamos el conteo.
     */
    TimerStart(
        timer_medicion.timer
    );
}
