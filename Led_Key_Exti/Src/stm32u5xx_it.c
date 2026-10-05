/***********************************************************************************
 * @file      stm32u5xx_it.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.1.0
 * @date      2026-09-25
 * @brief     中断服务函数: SysTick + 四路按键EXTI (U5每线独立IRQ)
 *            ISR只置标志位, 消抖与业务全部在主循环
 *
 * @history   V1.0.0 首版: KEY1/KEY2/USER三路EXTI             ---2026-09-24
 *            V1.1.0 增加KEY3(PC5/EXTI5), ISR按线号排序       ---2026-09-25
 **********************************************************************************/
#include "main.h"
#include "stm32u5xx_it.h"
#include "gpio.h"

extern UART_HandleTypeDef g_uart1_handle;

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  HAL_IncTick();
}

/******************************************************************************/
/* STM32U5xx Peripheral Interrupt Handlers                                    */
/******************************************************************************/

/**
  * @brief This function handles EXTI line9 interrupt: KEY1=PC9.
  */
void EXTI9_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(KEY1_Pin);
}

/**
  * @brief This function handles EXTI line8 interrupt: KEY2=PC8.
  */
void EXTI8_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(KEY2_Pin);
}

/**
  * @brief This function handles EXTI line5 interrupt: KEY3=PC5.
  */
void EXTI5_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(KEY3_Pin);
}

/**
  * @brief This function handles EXTI line12 interrupt: USER=PA12.
  */
void EXTI12_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(USER_KEY_Pin);
}

/**
  * @brief EXTI下降沿回调(U5拆分版, 非F1/F4共用回调): 只置标志快进快出
  * @param GPIO_Pin: 触发的引脚号
  * @retval None
  */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == KEY1_Pin)
  {
    g_key1_flag = 1;
  }
  else if (GPIO_Pin == KEY2_Pin)
  {
    g_key2_flag = 1;
  }
  else if (GPIO_Pin == KEY3_Pin)
  {
    g_key3_flag = 1;
  }
  else if (GPIO_Pin == USER_KEY_Pin)
  {
    g_user_flag = 1;
  }
}
