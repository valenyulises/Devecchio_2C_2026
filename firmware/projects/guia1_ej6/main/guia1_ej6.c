/**
 * @mainpage Ejercicio 6 - Display LCD
 *
 * @section genDesc Descripcion general
 *
 * El programa permite mostrar un numero decimal en un display LCD
 * de tres digitos utilizando lineas GPIO.
 *
 * Se reutilizan las funciones desarrolladas en los ejercicios 4 y 5
 * para separar el numero en digitos BCD y enviar cada digito
 * mediante los GPIO correspondientes.
 *
 * @author Valentino De Vecchio
 */


#include <stdint.h>
#include <stdbool.h>
#include "gpio_mcu.h"


/**
 * @def BCD_BITS
 * @brief Cantidad de bits utilizados para representar un digito BCD.
 */
#define BCD_BITS    4


/**
 * @def LCD_DIGITS
 * @brief Cantidad de digitos disponibles en el display LCD.
 */
#define LCD_DIGITS  3


/**
 * @brief Estructura utilizada para almacenar la configuracion de un GPIO.
 *
 * Contiene el pin GPIO utilizado y la direccion configurada para dicho pin.
 */
typedef struct
{
    gpio_t pin;
    io_t dir;
} gpioConf_t;


/**
 * @brief Vector utilizado para enviar los cuatro bits correspondientes
 * a un digito BCD mediante GPIO20, GPIO21, GPIO22 y GPIO23.
 */
gpioConf_t bcd_gpio[BCD_BITS] =
{
    {GPIO_20, GPIO_OUTPUT},   /* b0 */
    {GPIO_21, GPIO_OUTPUT},   /* b1 */
    {GPIO_22, GPIO_OUTPUT},   /* b2 */
    {GPIO_23, GPIO_OUTPUT}    /* b3 */
};


/**
 * @brief Vector utilizado para seleccionar cada uno de los tres
 * digitos del display LCD.
 */
gpioConf_t digit_gpio[LCD_DIGITS] =
{
    {GPIO_19, GPIO_OUTPUT},   /* Dígito 1 */
    {GPIO_18, GPIO_OUTPUT},   /* Dígito 2 */
    {GPIO_9,  GPIO_OUTPUT}    /* Dígito 3 */
};


/**
 * @fn int8_t convertToBcdArray(uint32_t data, uint8_t digits, uint8_t *bcd_number)
 * @brief Separa un numero decimal y almacena sus digitos en un arreglo.
 *
 * @param[in] data Numero decimal de 32 bits que se desea convertir.
 * @param[in] digits Cantidad de digitos que se desean obtener.
 * @param[out] bcd_number Arreglo donde se almacenan los digitos obtenidos.
 *
 * @return 0 si la conversion fue correcta.
 * @return -1 si la cantidad de digitos es insuficiente.
 */
int8_t convertToBcdArray(uint32_t data,
                         uint8_t digits,
                         uint8_t *bcd_number)
{
    uint8_t i;

    /* Se obtienen los dígitos desde el final del número */
    for (i = 0; i < digits; i++)
    {
        bcd_number[digits - 1 - i] = (uint8_t)(data % 10);

        /* Se elimina el dígito ya procesado */
        data = data / 10;
    }

    /* Si todavía quedó parte del número,
       la cantidad de dígitos era insuficiente */
    if (data != 0)
    {
        return -1;
    }

    return 0;
}


/**
 * @fn void BcdToGpio(uint8_t bcd_digit, gpioConf_t *gpio)
 * @brief Envia un digito BCD mediante cuatro lineas GPIO.
 *
 * @param[in] bcd_digit Digito BCD que se desea enviar.
 * @param[in] gpio Vector que contiene la configuracion de los GPIO
 * utilizados para enviar los cuatro bits BCD.
 *
 * @return No retorna ningun valor.
 */
void BcdToGpio(uint8_t bcd_digit,
               gpioConf_t *gpio)
{
    uint8_t i;
    bool estado;

    /* Se recorren los cuatro bits BCD */
    for (i = 0; i < BCD_BITS; i++)
    {
        /* Se obtiene individualmente cada bit */
        estado = (bool)((bcd_digit >> i) & 0x01);

        /* Se coloca el bit en el GPIO correspondiente */
        GPIOState(gpio[i].pin, estado);
    }
}


/**
 * @fn int8_t DisplayNumber(uint32_t data, uint8_t digits, gpioConf_t *bcd, gpioConf_t *digit)
 * @brief Muestra un numero decimal en el display LCD.
 *
 * @param[in] data Numero decimal de 32 bits que se desea mostrar.
 * @param[in] digits Cantidad de digitos que se desean mostrar.
 * @param[in] bcd Vector de GPIO utilizados para enviar los bits BCD.
 * @param[in] digit Vector de GPIO utilizados para seleccionar
 * cada posicion del display.
 *
 * @return 0 si el numero se mostro correctamente.
 * @return -1 si la cantidad de digitos es invalida o insuficiente.
 */
int8_t DisplayNumber(uint32_t data,
                     uint8_t digits,
                     gpioConf_t *bcd,
                     gpioConf_t *digit)
{
    uint8_t i;
    uint8_t bcd_number[LCD_DIGITS];
    uint8_t posicion_inicial;

    /* Verifica que la cantidad de dígitos sea válida.
       El display tiene como máximo 3 posiciones. */
    if ((digits == 0) || (digits > LCD_DIGITS))
    {
        return -1;
    }

    /* Separa el número recibido en sus dígitos. */
    if (convertToBcdArray(data, digits, bcd_number) != 0)
    {
        return -1;
    }

    /* Calcula desde qué posición del display empezar.
       Esto permite alinear los números hacia la derecha.

       3 dígitos -> posición inicial 0
       2 dígitos -> posición inicial 1
       1 dígito  -> posición inicial 2 */
    posicion_inicial = LCD_DIGITS - digits;

    /* Recorre cada dígito del número */
    for (i = 0; i < digits; i++)
    {
        /* Envía el dígito actual como BCD por GPIO20-23 */
        BcdToGpio(bcd_number[i], bcd);

        /* Selecciona la posición correcta del display
           teniendo en cuenta el desplazamiento calculado */
        GPIOOn(digit[posicion_inicial + i].pin);
        GPIOOff(digit[posicion_inicial + i].pin);
    }

    /* Todo salió ok */
    return 0;
}


/**
 * @fn void app_main(void)
 * @brief Funcion principal del programa.
 *
 * Inicializa los GPIO utilizados para las lineas BCD y para la
 * seleccion de los digitos del display. Luego muestra el numero
 * indicado mediante la funcion DisplayNumber().
 *
 * @return No retorna ningun valor.
 */
void app_main(void)
{
    uint8_t i;

    /* Inicialización de GPIO20-23:
       líneas utilizadas para enviar el BCD */
    for (i = 0; i < BCD_BITS; i++)
    {
        GPIOInit(bcd_gpio[i].pin, bcd_gpio[i].dir);
        GPIOOff(bcd_gpio[i].pin);
    }

    /* Inicialización de GPIO19, GPIO18 y GPIO9:
       selección de los tres dígitos del LCD */
    for (i = 0; i < LCD_DIGITS; i++)
    {
        GPIOInit(digit_gpio[i].pin, digit_gpio[i].dir);
        GPIOOff(digit_gpio[i].pin);
    }

    /* Ejemplo:
       muestra el número utilizando 3 dígitos */
    DisplayNumber(334, 3, bcd_gpio, digit_gpio);
}