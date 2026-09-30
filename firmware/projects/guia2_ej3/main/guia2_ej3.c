
/**
 * @mainpage Guía 2 - Ejercicio 3
 *
 * @section genDesc Descripción general
 *
 * El programa implementa un sistema de medición de distancia utilizando
 * el sensor ultrasónico HC-SR04.
 *
 * La aplicación utiliza dos tareas de FreeRTOS:
 *
 * - Una tarea para realizar la medición de distancia.
 * - Una tarea para controlar la visualización mediante LEDs y LCD.
 *
 * La medición se realiza periódicamente cada un segundo mediante un timer.
 *
 * TEC1 permite activar o detener la medición.
 * TEC2 permite activar o desactivar el modo HOLD.
 *
 * Además, la aplicación incorpora comunicación UART con la PC.
 * El comando 'O' permite activar o detener la medición y el comando 'H'
 * permite activar o desactivar el modo HOLD.
 *
 * La distancia medida se envía a la PC mediante UART utilizando
 * un formato de tres dígitos.
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
#include "timer_mcu.h"
#include "uart_mcu.h"


/*==================[macros and definitions]===============================*/

/**
 * @def MEASURE_PERIOD_US
 * @brief Período entre mediciones de distancia expresado en microsegundos.
 *
 * El valor 1000000 us corresponde a un período de un segundo.
 */
#define MEASURE_PERIOD_US 1000000


/**
 * @def UART_BAUD_RATE
 * @brief Velocidad utilizada para la comunicación UART con la PC.
 */
#define UART_BAUD_RATE 115200


/*==================[internal data definition]=============================*/

/**
 * @brief Última distancia medida por el sensor HC-SR04.
 *
 * La distancia se almacena en centímetros y se actualiza cada vez
 * que se realiza una nueva medición.
 */
volatile uint16_t distancia = 0;


/**
 * @brief Distancia almacenada al activar el modo HOLD.
 *
 * Este valor permanece constante mientras HOLD se encuentre activo
 * y es utilizado para mantener congelado el valor enviado por UART.
 */
volatile uint16_t distancia_hold = 0;


/**
 * @brief Indica si la medición de distancia se encuentra activa.
 *
 * true: medición activa.
 *
 * false: medición detenida.
 */
volatile bool medicion_activa = false;


/**
 * @brief Indica el estado del modo HOLD.
 *
 * true: el valor mostrado permanece congelado.
 *
 * false: la visualización se actualiza normalmente.
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


/*==================[internal functions declaration]=======================*/

/**
 * @fn void ToggleMedicion(void)
 * @brief Cambia el estado de la medición.
 *
 * Invierte el valor de la variable medicion_activa.
 *
 * Esta función es utilizada cuando se recibe el comando 'O'
 * desde la PC mediante UART.
 *
 * Luego notifica a DisplayTask para actualizar inmediatamente
 * el estado de la visualización.
 *
 * @return No retorna ningún valor.
 */
void ToggleMedicion(void)
{
    medicion_activa = !medicion_activa;

    /*
     * Avisamos a DisplayTask para que reaccione
     * inmediatamente al cambio.
     */
    xTaskNotifyGive(display_task_handle);
}


/**
 * @fn void ToggleHold(void)
 * @brief Cambia el estado del modo HOLD.
 *
 * Si HOLD se encuentra desactivado, guarda la distancia actual
 * en distancia_hold y activa el modo HOLD.
 *
 * Si HOLD ya se encuentra activado, lo desactiva.
 *
 * Esta función es utilizada cuando se recibe el comando 'H'
 * desde la PC mediante UART.
 *
 * @return No retorna ningún valor.
 */
void ToggleHold(void)
{
    /*
     * Si HOLD estaba desactivado, significa que
     * ahora vamos a activarlo.
     *
     * Antes de activarlo guardamos la distancia
     * que queremos congelar.
     */
    if (hold == false)
    {
        distancia_hold = distancia;
        hold = true;
    }

    /*
     * Si HOLD ya estaba activado,
     * simplemente lo desactivamos.
     */
    else
    {
        hold = false;
    }

    /*
     * Avisamos a DisplayTask para actualizar
     * inmediatamente el estado del LCD.
     */
    xTaskNotifyGive(display_task_handle);
}


/**
 * @fn void SendDistanceUart(uint16_t valor)
 * @brief Envía una distancia hacia la PC mediante UART.
 *
 * Convierte el valor de distancia recibido en tres caracteres ASCII
 * y agrega la unidad "cm".
 *
 * Por ejemplo:
 *
 * 005 cm
 *
 * 027 cm
 *
 * 125 cm
 *
 * @param[in] valor Distancia en centímetros que se desea enviar.
 *
 * @return No retorna ningún valor.
 */
void SendDistanceUart(uint16_t valor)
{
    char mensaje[9];

    /*
     * Armamos los tres dígitos ASCII.
     */
    mensaje[0] = (char)('0' + ((valor / 100) % 10));
    mensaje[1] = (char)('0' + ((valor / 10) % 10));
    mensaje[2] = (char)('0' + (valor % 10));

    /*
     * Agregamos espacio y unidad.
     */
    mensaje[3] = ' ';
    mensaje[4] = 'c';
    mensaje[5] = 'm';

    /*
     * Cambio de línea.
     */
    mensaje[6] = '\r';
    mensaje[7] = '\n';

    /*
     * Fin del string.
     */
    mensaje[8] = '\0';

    /*
     * Enviamos el string por UART_PC.
     */
    UartSendString(
        UART_PC,
        mensaje
    );
}


/*==================[interrupt functions]=================================*/

/**
 * @fn void FuncTimerA(void *param)
 * @brief Función ejecutada ante la interrupción del TIMER_A.
 *
 * Cada un segundo envía una notificación a DistanceTask
 * para iniciar una nueva medición de distancia.
 *
 * @param[in] param Parámetro de la interrupción. No se utiliza en esta aplicación.
 *
 * @return No retorna ningún valor.
 */
void FuncTimerA(void *param)
{
    vTaskNotifyGiveFromISR(
        distance_task_handle,
        pdFALSE
    );
}


/**
 * @fn void FuncTec1(void *param)
 * @brief Función ejecutada ante la interrupción de TEC1.
 *
 * Permite activar o detener la medición de distancia cambiando
 * el estado de la variable medicion_activa.
 *
 * Luego notifica a DisplayTask para actualizar inmediatamente
 * el estado del sistema.
 *
 * @param[in] param Parámetro de la interrupción. No se utiliza en esta aplicación.
 *
 * @return No retorna ningún valor.
 */
void FuncTec1(void *param)
{
    medicion_activa = !medicion_activa;

    vTaskNotifyGiveFromISR(
        display_task_handle,
        pdFALSE
    );
}


/**
 * @fn void FuncTec2(void *param)
 * @brief Función ejecutada ante la interrupción de TEC2.
 *
 * Permite activar o desactivar el modo HOLD.
 *
 * Al activar HOLD se almacena la distancia actual en distancia_hold.
 * Al desactivarlo, la visualización vuelve a actualizarse normalmente.
 *
 * Luego notifica a DisplayTask para actualizar el estado del LCD.
 *
 * @param[in] param Parámetro de la interrupción. No se utiliza en esta aplicación.
 *
 * @return No retorna ningún valor.
 */
void FuncTec2(void *param)
{
    /*
     * Si HOLD estaba apagado, guardamos
     * la distancia actual antes de congelarla.
     */
    if (hold == false)
    {
        distancia_hold = distancia;
        hold = true;
    }

    /*
     * Si ya estaba activado,
     * desactivamos HOLD.
     */
    else
    {
        hold = false;
    }

    vTaskNotifyGiveFromISR(
        display_task_handle,
        pdFALSE
    );
}


/**
 * @fn void FuncUart(void *param)
 * @brief Función ejecutada al recibir información desde la PC mediante UART.
 *
 * Lee el carácter recibido y realiza un eco enviando nuevamente
 * dicho carácter hacia la PC.
 *
 * El comando 'O' permite activar o detener la medición.
 *
 * El comando 'H' permite activar o desactivar el modo HOLD.
 *
 * @param[in] param Parámetro de la función UART. No se utiliza en esta aplicación.
 *
 * @return No retorna ningún valor.
 */
void FuncUart(void *param)
{
    uint8_t caracter;

    /*
     * Leemos el byte recibido.
     */
    if (UartReadByte(UART_PC, &caracter))
    {
        /*
         * ECO:
         *
         * Devolvemos a la PC el carácter
         * que acabamos de recibir.
         */
        UartSendByte(
            UART_PC,
            (char *)&caracter
        );

        /*
         * 'O' replica TEC1.
         */
        if (caracter == 'O')
        {
            ToggleMedicion();
        }

        /*
         * 'H' replica TEC2.
         */
        else if (caracter == 'H')
        {
            ToggleHold();
        }
    }
}


/*==================[tasks]================================================*/

/*-----------------------------------------------------------------------
 * TAREA 1: MEDICIÓN DE DISTANCIA
 *-----------------------------------------------------------------------*/

/**
 * @fn static void DistanceTask(void *pvParameter)
 * @brief Tarea encargada de realizar la medición de distancia.
 *
 * La tarea permanece bloqueada hasta recibir una notificación
 * generada por TIMER_A cada un segundo.
 *
 * Si la medición se encuentra activa, obtiene la distancia
 * utilizando el sensor HC-SR04.
 *
 * El sensor continúa realizando mediciones aunque HOLD se
 * encuentre activo.
 *
 * Si HOLD está desactivado se envía por UART la distancia actual.
 * Si HOLD está activado se envía la distancia almacenada en
 * distancia_hold.
 *
 * Finalmente se notifica a DisplayTask para actualizar
 * la visualización.
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
         * Esperamos la notificación del timer.
         */
        ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY
        );

        /*
         * Solamente medimos si la medición
         * se encuentra activa.
         */
        if (medicion_activa == true)
        {
            /*
             * El sensor continúa midiendo incluso
             * si HOLD está activado.
             */
            distancia =
                HcSr04ReadDistanceInCentimeters();

            /*
             * Si HOLD está desactivado,
             * enviamos la distancia actual.
             */
            if (hold == false)
            {
                SendDistanceUart(
                    distancia
                );
            }

            /*
             * Si HOLD está activado,
             * enviamos siempre la distancia
             * que quedó congelada.
             */
            else
            {
                SendDistanceUart(
                    distancia_hold
                );
            }

            /*
             * Despertamos la tarea
             * de visualización.
             */
            xTaskNotifyGive(
                display_task_handle
            );
        }
    }
}


/*-----------------------------------------------------------------------
 * TAREA 2: VISUALIZACIÓN
 *-----------------------------------------------------------------------*/

/**
 * @fn static void DisplayTask(void *pvParameter)
 * @brief Tarea encargada de controlar los LEDs y el LCD.
 *
 * La tarea permanece bloqueada hasta recibir una notificación.
 *
 * Si la medición está activa, utiliza la distancia real medida
 * para controlar los LEDs según los distintos rangos establecidos.
 *
 * El LCD muestra la distancia actual mientras HOLD se encuentre
 * desactivado. Si HOLD está activo, el LCD conserva el último
 * valor mostrado.
 *
 * Si la medición está detenida, se apagan los LEDs y, si HOLD
 * no está activo, también se apaga el LCD.
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
         * Esperamos una notificación.
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
            /*-----------------------------------------------------------
             * CONTROL DE LEDs
             *-----------------------------------------------------------*/

            /*
             * Los LEDs siguen representando
             * la distancia REAL medida por el sensor,
             * incluso durante HOLD.
             */
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
             * actualizamos normalmente el LCD.
             *
             * Si HOLD está activado no escribimos
             * nada nuevo, por lo que permanece
             * mostrando el último valor.
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
             * Si la medición está detenida,
             * apagamos los LEDs.
             */
            LedOff(LED_1);
            LedOff(LED_2);
            LedOff(LED_3);

            /*
             * Si no estamos en HOLD,
             * apagamos también el LCD.
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
 * @fn void app_main(void)
 * @brief Función principal de la aplicación.
 *
 * Inicializa los LEDs, las teclas, el sensor ultrasónico HC-SR04,
 * el LCD y la comunicación UART.
 *
 * Posteriormente crea las tareas DistanceTask y DisplayTask,
 * configura las interrupciones correspondientes a TEC1 y TEC2
 * y configura TIMER_A con un período de un segundo.
 *
 * Finalmente inicia el timer para comenzar a generar las
 * interrupciones periódicas utilizadas para realizar las mediciones.
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
    HcSr04Init(
        GPIO_3,
        GPIO_2
    );

    /* Inicializa LCD de la cátedra */
    LcdItsE0803Init();


    /*-------------------------------------------------------------
     * CONFIGURACIÓN UART
     *-------------------------------------------------------------*/

    serial_config_t my_uart = {

        .port = UART_PC,

        .baud_rate = UART_BAUD_RATE,

        /*
         * FuncUart será llamada cuando llegue
         * información desde la PC.
         */
        .func_p = FuncUart,

        .param_p = NULL
    };

    UartInit(
        &my_uart
    );


    /*-------------------------------------------------------------
     * CREACIÓN DE LAS DOS TAREAS
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


    /*-------------------------------------------------------------
     * INTERRUPCIONES DE TECLAS
     *-------------------------------------------------------------*/

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


    /*-------------------------------------------------------------
     * CONFIGURACIÓN DEL TIMER
     *-------------------------------------------------------------*/

    timer_config_t timer_medicion = {

        .timer = TIMER_A,

        .period = MEASURE_PERIOD_US,

        .func_p = FuncTimerA,

        .param_p = NULL
    };

    TimerInit(
        &timer_medicion
    );

    TimerStart(
        timer_medicion.timer
    );
}