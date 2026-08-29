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
 
*/




#include <stdint.h>
#include <stdbool.h>
#include "gpio_mcu.h"

#define BCD_BITS 4

/* Estructura que se quiere */
typedef struct
{
    gpio_t pin;   /* Número de GPIO */
    io_t dir;     /* Dirección: entrada o salida(en este caso todos salidas) */
} gpioConf_t;


/* Vector que relaciona cada bit BCD con un GPIO */
gpioConf_t bcd_gpio[BCD_BITS] =
{
    {GPIO_20, GPIO_OUTPUT},   /* b0 */
    {GPIO_21, GPIO_OUTPUT},   /* b1 */
    {GPIO_22, GPIO_OUTPUT},   /* b2 */
    {GPIO_23, GPIO_OUTPUT}    /* b3 */
};


/* Envía los cuatro bits de un dígito BCD
   hacia los cuatro GPIO correspondientes */
void BcdToGpio(uint8_t bcd_digit, gpioConf_t *gpio)
{
    uint8_t i;
    bool estado;

    /* Recorre los cuatro bits del número BCD */
    for (i = 0; i < BCD_BITS; i++)
    {
        /* Desplaza el bit que queremos analizar hasta b0
           y aplica una máscara para obtener únicamente 0 o 1 */
        estado = (bool)((bcd_digit >> i) & 0x01);

        /* Coloca el estado del bit en el GPIO correspondiente */
        GPIOState(gpio[i].pin, estado);
    }
}


void app_main(void)
{
    uint8_t i;

    /* Inicializa GPIO_20 a GPIO_23 como salidas */
    for (i = 0; i < BCD_BITS; i++)
    {
        GPIOInit(bcd_gpio[i].pin, bcd_gpio[i].dir);
    }

    /* Ejemplo: envía el BCD correspondiente al número 8 */
    BcdToGpio(8, bcd_gpio);
}


