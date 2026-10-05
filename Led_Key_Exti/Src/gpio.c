/***********************************************************************************
 * @file      gpio.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.0.0
 * @date      2026-09-24
 * @brief     LED与按键引脚初始化（LED1=PC4, LED2=PC3低电平点亮; KEY1=PC9,
 *            KEY2=PC8, KEY3=PC5, USER=PA12低电平有效, 下降沿EXTI中断）
 *
 * @history   V1.0.0 首版: LED/KEY/EXTI引脚配置                 ---2026-09-24
 *            V1.1.0 增加KEY3(PC5/EXTI5)                          ---2026-09-25
 **********************************************************************************/
#include "gpio.h"
#include "stm32u5xx_it.h"

/* USER CODE BEGIN 0 */

/* GPIO初始化参数表: LED为推挽输出, 按键为下降沿中断+上拉 */
static GPIO_InitTypeDef gpio_init_tab[6] =
{
    /* LED1=PC4, LED2=PC3: 推挽输出, 低速 */
    {.Pin = LED1_Pin, .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_LOW},
    {.Pin = LED2_Pin, .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_LOW},
    /* KEY1=PC9: 下降沿中断, 上拉(底板外部电路上拉, 内部上拉双保险) */
    {.Pin = KEY1_Pin, .Mode = GPIO_MODE_IT_FALLING, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_LOW},
    /* KEY2=PC8: 同上 */
    {.Pin = KEY2_Pin, .Mode = GPIO_MODE_IT_FALLING, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_LOW},
    /* KEY3=PC5: 下降沿中断, 内部上拉 */
    {.Pin = KEY3_Pin, .Mode = GPIO_MODE_IT_FALLING, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_LOW},
    /* USER=PA12: 下降沿中断, 内部上拉 */
    {.Pin = USER_KEY_Pin, .Mode = GPIO_MODE_IT_FALLING, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_LOW},
};

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/**
  * @brief GPIO初始化: LED输出 + 按键EXTI中断
  * @note  KEY1(PC9)=EXTI9, KEY2(PC8)=EXTI8, KEY3(PC5)=EXTI5, USER(PA12)=EXTI12,
  *        四条独立IRQ(U5每条EXTI线独立中断通道)
  * @retval None
  */
void MX_GPIO_Init(void)
{

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* Configure GPIO pin Output Level : LED1(PC4)/LED2(PC3) 熄灭(高电平) */
  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin | LED2_Pin, GPIO_PIN_SET);

  /* Configure GPIO pins : LED输出 + 按键EXTI */
  HAL_GPIO_Init(LED1_GPIO_Port, &gpio_init_tab[0]);
  HAL_GPIO_Init(LED1_GPIO_Port, &gpio_init_tab[1]);
  HAL_GPIO_Init(KEY1_GPIO_Port, &gpio_init_tab[2]);
  HAL_GPIO_Init(KEY2_GPIO_Port, &gpio_init_tab[3]);
  HAL_GPIO_Init(KEY3_GPIO_Port, &gpio_init_tab[4]);
  HAL_GPIO_Init(USER_KEY_GPIO_Port, &gpio_init_tab[5]);

  /* EXTI中断: 使能NVIC通道, 按键优先级次高于SysTick */
  HAL_NVIC_SetPriority(KEY1_EXTI_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(KEY1_EXTI_IRQn);
  HAL_NVIC_SetPriority(KEY2_EXTI_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(KEY2_EXTI_IRQn);
  HAL_NVIC_SetPriority(KEY3_EXTI_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(KEY3_EXTI_IRQn);
  HAL_NVIC_SetPriority(USER_KEY_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(USER_KEY_IRQn);

  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */
}

/* USER CODE BEGIN 3 */

/* USER CODE END 3 */
