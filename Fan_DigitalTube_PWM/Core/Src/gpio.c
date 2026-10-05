/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins
     PH0-OSC_IN (PH0)   ------> RCC_OSC_IN
     PH1-OSC_OUT (PH1)   ------> RCC_OSC_OUT
*/
void MX_GPIO_Init(void)
{

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  /* USER CODE BEGIN GPIO_Init_Clock */
  __HAL_RCC_GPIOC_CLK_ENABLE();             /* 底板按键 KEY1~3 在 PC9/PC8/PC5 */
  /* USER CODE END GPIO_Init_Clock */

}

/* USER CODE BEGIN 2 */

/*************************************
 * 函数名称 ： key_gpio_init
 * 描述     ： 底板按键 KEY1~3 GPIO 输入初始化(PC9/PC8/PC5, 低电平按下,
 *           ： 板载外部上拉, 内部配 NOPULL 即可)
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void key_gpio_init(void)
{
  GPIO_InitTypeDef gpio_init = {0};

  gpio_init.Pin   = GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_9;
  gpio_init.Mode  = GPIO_MODE_INPUT;
  gpio_init.Pull  = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &gpio_init);
}

/* USER CODE END 2 */
