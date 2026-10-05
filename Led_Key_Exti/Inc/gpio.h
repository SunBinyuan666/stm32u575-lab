/***********************************************************************************
 * @file      gpio.h
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.0.0
 * @date      2026-09-24
 * @brief     LED与按键引脚定义及初始化声明（华清FS-STM32U5底板, 均低电平有效）
 *
 * @history   V1.0.0 首版                                     ---2026-09-24
 *            V1.1.0 增加KEY3(PC5/EXTI5)定义                  ---2026-09-25
 **********************************************************************************/
#ifndef __GPIO_H
#define __GPIO_H

#include "main.h"

/* LED引脚: 低电平点亮 -------------------------------------------------------*/
#define LED1_Pin        GPIO_PIN_4
#define LED1_GPIO_Port  GPIOC
#define LED2_Pin        GPIO_PIN_3
#define LED2_GPIO_Port  GPIOC

#define LED1_ON()       HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET)
#define LED1_OFF()      HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET)
#define LED1_TOGGLE()   HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin)
#define LED2_ON()       HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET)
#define LED2_OFF()      HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET)
#define LED2_TOGGLE()   HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin)

/* 按键引脚: 均低电平有效 (USER=PA12内部上拉, KEY1/2=PC9/PC8外部电路上拉) ------*/
#define USER_KEY_Pin        GPIO_PIN_12
#define USER_KEY_GPIO_Port  GPIOA
#define USER_KEY_IRQn       EXTI12_IRQn

#define KEY1_Pin        GPIO_PIN_9
#define KEY1_GPIO_Port  GPIOC
#define KEY1_EXTI_IRQn  EXTI9_IRQn

#define KEY2_Pin        GPIO_PIN_8
#define KEY2_GPIO_Port  GPIOC
#define KEY2_EXTI_IRQn  EXTI8_IRQn   /* U5每条EXTI线独立IRQ, 无EXTI9_5共享 */

/* KEY3=PC5: 底板按键, 下降沿中断+内部上拉 */
#define KEY3_Pin        GPIO_PIN_5
#define KEY3_GPIO_Port  GPIOC
#define KEY3_EXTI_IRQn  EXTI5_IRQn

/* 按键标志位: ISR置位, 主循环消费并清零 (定义在main.c) ---------------------*/
extern volatile uint8_t g_key1_flag;
extern volatile uint8_t g_key2_flag;
extern volatile uint8_t g_user_flag;
extern volatile uint8_t g_key3_flag;

void MX_GPIO_Init(void);

#endif /* __GPIO_H */
