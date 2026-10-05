/***********************************************************************************
 * @file      usart.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.1.0
 * @date      2026-09-27
 * @brief     USART1调试串口实现（PA9=TX/PA10=RX, 115200-8-N-1）
 *            printf经_write重定向到USART1（GCC newlib）
 *
 * @history   V1.1.0 按WT-WI-PE-200 B1规范重构               ---2026-09-27
 *            V1.0.0 首版: 阻塞式发送+printf重定向           ---2026-09-27
 **********************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "usart.h"
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/
#define USART1_BAUDRATE         115200U
#define USART1_TX_TIMEOUT_MS    HAL_MAX_DELAY

/* Variable definitions ------------------------------------------------------*/
UART_HandleTypeDef g_uart1_handle;

/* Functions -----------------------------------------------------------------*/
/*************************************
 * 函数名称 ： usart1_init
 * 描述     ： USART1初始化, 115200-8-N-1, 收发方向
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void usart1_init(void)
{
    g_uart1_handle.Instance = USART1;

    /* 指定初始化器明确每个成员(B1规则6-1) */
    g_uart1_handle.Init.BaudRate          = USART1_BAUDRATE;
    g_uart1_handle.Init.WordLength        = UART_WORDLENGTH_8B;
    g_uart1_handle.Init.StopBits          = UART_STOPBITS_1;
    g_uart1_handle.Init.Parity            = UART_PARITY_NONE;
    g_uart1_handle.Init.Mode              = UART_MODE_TX_RX;
    g_uart1_handle.Init.HwFlowCtl         = UART_HWCONTROL_NONE;
    g_uart1_handle.Init.OverSampling      = UART_OVERSAMPLING_16;
    g_uart1_handle.Init.OneBitSampling    = UART_ONE_BIT_SAMPLE_DISABLE;
    g_uart1_handle.Init.ClockPrescaler    = UART_PRESCALER_DIV1;
    g_uart1_handle.FifoMode               = UART_FIFOMODE_DISABLE;
    g_uart1_handle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&g_uart1_handle) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&g_uart1_handle,
                                      UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&g_uart1_handle,
                                      UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_DisableFifoMode(&g_uart1_handle) != HAL_OK)
    {
        Error_Handler();
    }
}

/*************************************
 * 函数名称 ： HAL_UART_MspInit
 * 描述     ： USART1底层初始化(PA9/PA10复用+时钟), 覆盖HAL弱符号
 * 输入     ： huart - UART句柄
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio_init = {0};

    if (huart->Instance == USART1)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_USART1_CLK_ENABLE();

        /* PA9=USART1_TX, PA10=USART1_RX: AF7复用推挽, 经J6跳线接CH340E */
        gpio_init.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
        gpio_init.Mode      = GPIO_MODE_AF_PP;
        gpio_init.Pull      = GPIO_NOPULL;
        gpio_init.Speed     = GPIO_SPEED_FREQ_LOW;
        gpio_init.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &gpio_init);
    }
}

/*************************************
 * 函数名称 ： _write
 * 描述     ： GCC newlib printf字符输出重定向（非Keil的__io_putchar）
 * 输入     ： file - 文件描述符(未用)
 *            ptr  - 数据缓冲区
 *            len  - 字节数
 * 输出     ： 无
 * 返回     ： 实际发送字节数
 **************************************/
int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&g_uart1_handle, (uint8_t *)ptr, len, USART1_TX_TIMEOUT_MS);
    return len;
}
