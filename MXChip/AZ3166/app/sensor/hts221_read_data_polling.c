/*
 ******************************************************************************
 * @file    read_data_polling.c
 * @author  MEMS Software Solution Team
 * @brief   This file show the simplest way to get data from sensor.
 *
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/*
 * This example was developed using the following STMicroelectronics
 * evaluation boards:
 *
 * - STEVAL_MKI109V3 + STEVAL-MKI141V2
 * - NUCLEO_F411RE + STEVAL-MKI141V2
 *
 * and STM32CubeMX tool with STM32CubeF4 MCU Package
 *
 * Used interfaces:
 *
 * STEVAL_MKI109V3    - Host side:   USB (Virtual COM)
 *                    - Sensor side: SPI(Default) / I2C(supported)
 *
 * NUCLEO_STM32F411RE - Host side: UART(COM) to USB bridge
 *                    - I2C(Default) / SPI(N/A)
 *
 * If you need to run this example on a different hardware platform a
 * modification of the functions: `platform_write`, `platform_read`,
 * `tx_com` and 'platform_init' is required.
 *
 */

/* STMicroelectronics evaluation boards definition
 *
 * Please uncomment ONLY the evaluation boards in use.
 * If a different hardware is used please comment all
 * following target board and redefine yours.
 */
   
   #include "stm32f4xx_hal.h"
#include "sensor.h"
//#define STEVAL_MKI109V3
#define NUCLEO_F411RE


extern I2C_HandleTypeDef I2cHandle;
extern UART_HandleTypeDef UartHandle;
#if defined(STEVAL_MKI109V3)
/* MKI109V3: Define communication interface */
#define SENSOR_BUS hspi2

/* MKI109V3: Vdd and Vddio power supply values */
#define PWM_3V3 915

#elif defined(NUCLEO_F411RE)
/* NUCLEO_F411RE: Define communication interface */
//#define SENSOR_BUS hi2c1
#define hi2c1 I2cHandle
#endif

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "hts221_reg.h"
//#include "gpio.h"
//#include "i2c.h"
#if defined(STEVAL_MKI109V3)
#include "usbd_cdc_if.h"
#include "spi.h"
#elif defined(NUCLEO_F411RE)
//#include "usart.h"
#endif

typedef union {
    int16_t i16bit;
    uint8_t u8bit[2];
} axis1bit16_t;

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static axis1bit16_t data_raw_humidity;
static axis1bit16_t data_raw_temperature;
static uint8_t whoamI;

/* Extern variables ----------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
/*
 *   WARNING:
 *   Functions declare in this section are defined at the end of this file
 *   and are strictly related to the hardware platform used.
 *
 */
static int32_t platform_write(void *handle, uint8_t reg, uint8_t *bufp,
                              uint16_t len);
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len);
//static void platform_delay(uint32_t ms);
static void platform_init(void);

/*
 *  Function used to apply coefficient
 */
typedef struct {
  float x0;
  float y0;
  float x1;
  float y1;
} lin_t;

float linear_interpolation(lin_t *lin, int16_t x) {
    return ((lin->y1 - lin->y0) * x + ((lin->x1 * lin->y0) - (lin->x0 * lin->y1)))
            / (lin->x1 - lin->x0);
}

/* Initialize mems driver interface */
static stmdev_ctx_t dev_ctx =
{
    platform_write,
    platform_read,
    &hi2c1,
};

/* humidity calibration coefficient */
  lin_t lin_hum;
/* Temperature calibration coefficient */
  lin_t lin_temp;
/* Main Example --------------------------------------------------------------*/
void hts221_config(void)
{

  /* Initialize platform specific hardware */
  platform_init();

  /* Check device ID */
  whoamI = 0;
  hts221_device_id_get(&dev_ctx, &whoamI);
  if ( whoamI != HTS221_ID )
    while(1); /*manage here device not found */

  /* Read humidity calibration coefficient */
  hts221_hum_adc_point_0_get(&dev_ctx, &lin_hum.x0);
  hts221_hum_rh_point_0_get(&dev_ctx, &lin_hum.y0);
  hts221_hum_adc_point_1_get(&dev_ctx, &lin_hum.x1);
  hts221_hum_rh_point_1_get(&dev_ctx, &lin_hum.y1);

  /* Read temperature calibration coefficient */
  hts221_temp_adc_point_0_get(&dev_ctx, &lin_temp.x0);
  hts221_temp_deg_point_0_get(&dev_ctx, &lin_temp.y0);
  hts221_temp_adc_point_1_get(&dev_ctx, &lin_temp.x1);
  hts221_temp_deg_point_1_get(&dev_ctx, &lin_temp.y1);

  /* Enable Block Data Update */
  hts221_block_data_update_set(&dev_ctx, PROPERTY_ENABLE);
  /* Set Output Data Rate */
  hts221_data_rate_set(&dev_ctx, HTS221_ODR_1Hz);
  /* Device power on */
  hts221_power_on_set(&dev_ctx, PROPERTY_ENABLE);
}

static uint32_t timeout = 5;
hts221_data_t hts221_data_read(void)
{
  hts221_data_t reading = {0};

  /* Read samples in polling mode */

    /* Read output only if new value is available */
    hts221_reg_t reg;
    
    /* Read output only if new value is available */
    while((reg.status_reg.h_da!=1) && (reg.status_reg.t_da!=1) && (timeout>0))
    {
      hts221_status_get(&dev_ctx, &reg.status_reg);
      timeout--;
    }

    /* Read humidity data */
    memset(data_raw_humidity.u8bit, 0x00, sizeof(int16_t));
    hts221_humidity_raw_get(&dev_ctx, data_raw_humidity.u8bit);
    reading.humidity_perc = linear_interpolation(&lin_hum, data_raw_humidity.i16bit);
    if (reading.humidity_perc < 0) reading.humidity_perc = 0;
    if (reading.humidity_perc > 100) reading.humidity_perc = 100;

    /* Read temperature data */
    memset(data_raw_temperature.u8bit, 0x00, sizeof(int16_t));
    hts221_temperature_raw_get(&dev_ctx, data_raw_temperature.u8bit);
    reading.temperature_degC = linear_interpolation(&lin_temp, data_raw_temperature.i16bit);
    
  return reading;
}

/*
 * @brief  Write generic device register (platform dependent)
 *
 * @param  handle    customizable argument. In this examples is used in
 *                   order to select the correct sensor bus handler.
 * @param  reg       register to write
 * @param  bufp      pointer to data to write in register reg
 * @param  len       number of consecutive register to write
 *
 */
static int32_t platform_write(void *handle, uint8_t reg, uint8_t *bufp,
                              uint16_t len)
{
  if (handle == &hi2c1)
  {
    /* Write multiple command */
    reg |= 0x80;
    HAL_I2C_Mem_Write(handle, HTS221_I2C_ADDRESS, reg,
                      I2C_MEMADD_SIZE_8BIT, bufp, len, 1000);
  }
#ifdef STEVAL_MKI109V3
  else if (handle == &hspi2)
  {
    /* Write multiple command */
    reg |= 0x40;
    HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(handle, &reg, 1, 1000);
    HAL_SPI_Transmit(handle, bufp, len, 1000);
    HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_SET);
  }
#endif
  return 0;
}

/*
 * @brief  Read generic device register (platform dependent)
 *
 * @param  handle    customizable argument. In this examples is used in
 *                   order to select the correct sensor bus handler.
 * @param  reg       register to read
 * @param  bufp      pointer to buffer that store the data read
 * @param  len       number of consecutive register to read
 *
 */
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len)
{
  if (handle == &hi2c1)
  {
    /* Read multiple command */
    reg |= 0x80;
    HAL_I2C_Mem_Read(handle, HTS221_I2C_ADDRESS, reg,
                     I2C_MEMADD_SIZE_8BIT, bufp, len, 1000);
  }
#ifdef STEVAL_MKI109V3
  else if (handle == &hspi2)
  {
    /* Read multiple command */
    reg |= 0xC0;
    HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(handle, &reg, 1, 1000);
    HAL_SPI_Receive(handle, bufp, len, 1000);
    HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_SET);
  }
#endif
  return 0;
}

/*
 * @brief  platform specific delay (platform dependent)
 *
 * @param  ms        delay in ms
 *
 */
//static void platform_delay(uint32_t ms)
//{
//  HAL_Delay(ms);
//}

/*
 * @brief  platform specific initialization (platform dependent)
 */
static void platform_init(void)
{
#if defined(STEVAL_MKI109V3)
  TIM3->CCR1 = PWM_3V3;
  TIM3->CCR2 = PWM_3V3;
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_Delay(1000);
#endif
}

