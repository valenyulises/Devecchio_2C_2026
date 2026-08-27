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


/* Declaración de la función */
int8_t convertToBcdArray(uint32_t data, uint8_t digits, uint8_t *bcd_number);


/* Programa principal utilizado para probar la función */
void app_main(void)
{
    uint8_t numero_bcd[4];
    uint8_t i;
    int8_t resultado;

    /* Se convierte el número 1234 */
    resultado = convertToBcdArray(1234, 4, numero_bcd);

    /* Se verifica si la conversión fue correcta */
    if (resultado == 0)
    {
        /* Se muestran los dígitos almacenados en el arreglo */
        for (i = 0; i < 4; i++)
        {
            printf("%d ", numero_bcd[i]);
        }

        printf("\n");
    }
    else
    {
        printf("Error: cantidad de digitos insuficiente\n");
    }
}


/* Función que convierte un número decimal
   y guarda cada dígito en un arreglo */
int8_t convertToBcdArray(uint32_t data, uint8_t digits, uint8_t *bcd_number)
{
    uint8_t i;

    /* Se recorren todas las posiciones del arreglo */
    for (i = 0; i < digits; i++)
    {
        /* Obtiene el último dígito decimal */
        bcd_number[digits - 1 - i] = (uint8_t)(data % 10);

        /* Elimina el último dígito ya procesado */
        data = data / 10;
    }

    /* Si todavía queda parte del número,
       no alcanzaron los dígitos indicados */
    if (data != 0)
    {
        return -1;
    }

    /* Conversión ok */
    return 0;
}