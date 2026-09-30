
/**
 * @mainpage Guía 2 - Ejercicio 4
 *
 * @section genDesc Descripción general
 *
 * El programa implementa la adquisición y transmisión de una señal
 * utilizando los conversores D/A y A/D del sistema.
 *
 * Se utiliza una señal ECG almacenada digitalmente en un arreglo de
 * 231 muestras. Cada muestra es enviada periódicamente al conversor
 * D/A para generar la señal analógica.
 *
 * La señal analógica es aplicada a la entrada CH1 del conversor A/D,
 * donde se realiza nuevamente su conversión a un valor digital.
 *
 * Las muestras obtenidas mediante el A/D son convertidas a caracteres
 * ASCII y enviadas hacia la PC mediante comunicación UART.
 *
 * El sistema trabaja con una frecuencia de muestreo de 500 Hz.
 * Para generar este período de muestreo se utiliza TIMER_A, que
 * interrumpe cada 2 ms y notifica a la tarea principal.
 *
 * @author Valentino De Vecchio
 */


/*==================[inclusions]=============================================*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "analog_io_mcu.h"
#include "uart_mcu.h"
#include "timer_mcu.h"


/*==================[macros and definitions]===============================*/

/**
 * @def SAMPLE_PERIOD_US
 * @brief Período de muestreo de la señal expresado en microsegundos.
 *
 * La frecuencia de muestreo utilizada es de 500 Hz.
 *
 * T = 1 / 500 Hz = 0,002 s = 2 ms = 2000 us.
 */
#define SAMPLE_PERIOD_US    2000


/**
 * @def UART_BAUD_RATE
 * @brief Velocidad utilizada para la comunicación UART con la PC.
 */
#define UART_BAUD_RATE      115200


/**
 * @def BUFFER_SIZE
 * @brief Cantidad de muestras almacenadas de la señal ECG.
 */
#define BUFFER_SIZE         231


/*==================[internal data definition]=============================*/

/**
 * @brief Handle correspondiente a la tarea principal.
 *
 * Es utilizado por la interrupción del timer para enviar una
 * notificación a MainTask cada 2 ms.
 */
TaskHandle_t main_task_handle = NULL;


/**
 * @brief Arreglo que contiene las muestras digitales de la señal ECG.
 *
 * Cada elemento del arreglo representa una muestra consecutiva
 * de la señal. El arreglo contiene un total de 231 muestras.
 */
const char ecg[BUFFER_SIZE] = {
    76, 77, 78, 77, 79, 86, 81, 76, 84, 93, 85, 80,
    89, 95, 89, 85, 93, 98, 94, 88, 98, 105, 96, 91,
    99, 105, 101, 96, 102, 106, 101, 96, 100, 107, 101,
    94, 100, 104, 100, 91, 99, 103, 98, 91, 96, 105, 95,
    88, 95, 100, 94, 85, 93, 99, 92, 84, 91, 96, 87, 80,
    83, 92, 86, 78, 84, 89, 79, 73, 81, 83, 78, 70, 80, 82,
    79, 69, 80, 82, 81, 70, 75, 81, 77, 74, 79, 83, 82, 72,
    80, 87, 79, 76, 85, 95, 87, 81, 88, 93, 88, 84, 87, 94,
    86, 82, 85, 94, 85, 82, 85, 95, 86, 83, 92, 99, 91, 88,
    94, 98, 95, 90, 97, 105, 104, 94, 98, 114, 117, 124, 144,
    180, 210, 236, 253, 227, 171, 99, 49, 34, 29, 43, 69, 89,
    89, 90, 98, 107, 104, 98, 104, 110, 102, 98, 103, 111, 101,
    94, 103, 108, 102, 95, 97, 106, 100, 92, 101, 103, 100, 94, 98,
    103, 96, 90, 98, 103, 97, 90, 99, 104, 95, 90, 99, 104, 100, 93,
    100, 106, 101, 93, 101, 105, 103, 96, 105, 112, 105, 99, 103, 108,
    99, 96, 102, 106, 99, 90, 92, 100, 87, 80, 82, 88, 77, 69, 75, 79,
    74, 67, 71, 78, 72, 67, 73, 81, 77, 71, 75, 84, 79, 77, 77, 76, 76
};


/*==================[interrupt functions]=================================*/

/**
 * @fn void FuncTimerA(void *param)
 * @brief Función ejecutada ante la interrupción de TIMER_A.
 *
 * La función se ejecuta periódicamente cada 2 ms y envía una
 * notificación a MainTask para iniciar el procesamiento de una
 * nueva muestra.
 *
 * Este período de 2 ms permite obtener una frecuencia de
 * muestreo de 500 Hz.
 *
 * @param[in] param Parámetro de la interrupción. No se utiliza en esta aplicación.
 *
 * @return No retorna ningún valor.
 */
void FuncTimerA(void *param)
{
    vTaskNotifyGiveFromISR(
        main_task_handle,
        pdFALSE
    );
}


/*==================[tasks]================================================*/

/*-----------------------------------------------------------------------
 * TAREA PRINCIPAL
 *-----------------------------------------------------------------------*/

/**
 * @fn static void MainTask(void *pvParameter)
 * @brief Tarea encargada del procesamiento periódico de la señal ECG.
 *
 * La tarea permanece bloqueada hasta recibir una notificación
 * generada por TIMER_A cada 2 ms.
 *
 * En cada ejecución envía una muestra de la señal ECG al conversor
 * D/A, realiza una lectura de la entrada analógica CH1 mediante el
 * conversor A/D y envía el valor obtenido hacia la PC mediante UART.
 *
 * Luego avanza a la siguiente muestra almacenada en el arreglo ecg.
 * Al alcanzar la última muestra, el índice vuelve a cero para repetir
 * nuevamente la señal.
 *
 * @param[in] pvParameter Parámetro de la tarea. No se utiliza en esta aplicación.
 *
 * @return No retorna ningún valor.
 */
static void MainTask(void *pvParameter)
{
    /*
     * Variable donde guardamos el resultado
     * de la conversión A/D.
     */
    uint16_t lectura;


    /*
     * Posición actual dentro del arreglo ecg[].
     */
    uint16_t indice = 0;


    while (true)
    {
        /*
         * La tarea queda bloqueada esperando
         * la interrupción periódica del timer.
         */
        ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY
        );


        /*-------------------------------------------------------
         * DIGITAL -> ANALÓGICO
         *------------------------------------------------------*/

        /*
         * Enviamos una muestra del ECG al DAC/SDM.
         *
         * El driver recibe uint8_t, por eso hacemos
         * la conversión explícita.
         */
        AnalogOutputWrite(
            (uint8_t)ecg[indice]
        );


        /*-------------------------------------------------------
         * ANALÓGICO -> DIGITAL
         *------------------------------------------------------*/

        /*
         * Leemos la señal que entra por CH1.
         *
         * Esta es la señal proveniente del DAC
         * luego de pasar por el potenciómetro.
         */
        AnalogInputReadSingle(
            CH1,
            &lectura
        );


        /*-------------------------------------------------------
         * ENVÍO POR UART
         *------------------------------------------------------*/

        /*
         * Convertimos el valor numérico del ADC
         * a caracteres ASCII.
         *
         * Ejemplo:
         *
         * lectura = 323
         *
         * UART transmite:
         *
         * '3' '2' '3'
         */
        UartSendString(
            UART_PC,
            (char *)UartItoa(lectura, 10)
        );


        /*
         * IMPORTANTE:
         *
         * Indicamos que terminó esta muestra.
         *
         * Sin estos caracteres el osciloscopio recibe:
         *
         * 323324322325...
         *
         * y no puede separar las muestras.
         *
         * Ahora recibe:
         *
         * 323\r\n
         * 324\r\n
         * 322\r\n
         */
        UartSendString(
            UART_PC,
            "\r\n"
        );


        /*-------------------------------------------------------
         * SIGUIENTE MUESTRA DEL ECG
         *------------------------------------------------------*/

        indice++;


        /*
         * Cuando llegamos al final de las
         * 231 muestras, volvemos a la primera.
         */
        if (indice >= BUFFER_SIZE)
        {
            indice = 0;
        }
    }
}


/*==================[external functions definition]========================*/

/**
 * @fn void app_main(void)
 * @brief Función principal de la aplicación.
 *
 * Inicializa el conversor A/D utilizando CH1 en modo de lectura
 * individual, inicializa la salida D/A y configura la comunicación
 * UART con la PC a 115200 baudios.
 *
 * Posteriormente crea MainTask y configura TIMER_A con un período
 * de 2000 us, correspondiente a una frecuencia de muestreo de 500 Hz.
 *
 * Finalmente inicia el timer para generar las interrupciones
 * periódicas utilizadas durante el procesamiento de la señal.
 *
 * @return No retorna ningún valor.
 */
void app_main(void)
{
    /*-------------------------------------------------------
     * CONFIGURACIÓN DEL ADC
     *------------------------------------------------------*/

    analog_input_config_t adc_config = {

        /*
         * Utilizamos la entrada CH1.
         */
        .input = CH1,

        /*
         * Lectura individual.
         */
        .mode = ADC_SINGLE
    };


    AnalogInputInit(
        &adc_config
    );


    /*-------------------------------------------------------
     * CONFIGURACIÓN DEL DAC / SDM
     *------------------------------------------------------*/

    /*
     * Inicializa la salida analógica.
     *
     * Según el driver de la cátedra,
     * la salida se encuentra en GPIO 0.
     */
    AnalogOutputInit();


    /*-------------------------------------------------------
     * CONFIGURACIÓN DE UART
     *------------------------------------------------------*/

    serial_config_t uart_config = {

        .port = UART_PC,

        .baud_rate = UART_BAUD_RATE,

        /*
         * No utilizamos recepción por interrupciones.
         */
        .func_p = UART_NO_INT,

        .param_p = NULL
    };


    UartInit(
        &uart_config
    );


    /*-------------------------------------------------------
     * CREACIÓN DE LA TAREA
     *------------------------------------------------------*/

    xTaskCreate(
        &MainTask,
        "MainTask",
        2048,
        NULL,
        5,
        &main_task_handle
    );


    /*-------------------------------------------------------
     * CONFIGURACIÓN DEL TIMER
     *------------------------------------------------------*/

    timer_config_t timer_adc = {

        .timer = TIMER_A,

        /*
         * 2000 us = 2 ms
         *
         * Por lo tanto:
         *
         * fs = 500 Hz
         */
        .period = SAMPLE_PERIOD_US,

        .func_p = FuncTimerA,

        .param_p = NULL
    };


    TimerInit(
        &timer_adc
    );


    /*
     * Comenzamos las interrupciones periódicas.
     */
    TimerStart(
        timer_adc.timer
    );
}