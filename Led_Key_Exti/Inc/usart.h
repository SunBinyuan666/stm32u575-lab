/***********************************************************************************
 * @file      usart.h
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.0.0
 * @date      2026-09-24
 * @brief     USART1调试串口驱动声明 (PA9=TX, PA10=RX, 经核心板J6跳线接CH340E)
 *
 * @history   V1.0.0 首版: 阻塞式发送+printf重定向         ---2026-09-24
 **********************************************************************************/
#ifndef __USART_H
#define __USART_H

#include "main.h"

extern UART_HandleTypeDef g_uart1_handle;

void uart1_init(uint32_t baudrate);

#endif /* __USART_H */
