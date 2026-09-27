/***********************************************************************************
 * @file      usart.h
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.1.0
 * @date      2026-09-27
 * @brief     USART1调试串口对外接口
 *
 * @history   V1.1.0 句柄命名/宏常量化(WT-WI-PE-200 B1)         ---2026-09-27
 *            V1.0.0 首版                                       ---2026-09-27
 **********************************************************************************/
#ifndef USART_H
#define USART_H

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Variable declarations -----------------------------------------------------*/
extern UART_HandleTypeDef g_uart1_handle;

/* Function Prototypes -------------------------------------------------------*/
void usart1_init(void);

#endif /* USART_H */
