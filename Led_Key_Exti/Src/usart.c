/***********************************************************************************
 * @file      usart.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.0.0
 * @date      2026-09-24
 * @brief     USART1调试串口驱动 (PA9=TX, PA10=RX, 115200-8-N-1)
 *            阻塞式发送, printf经GCC newlib的_write重定向
 *
 * @history   V1.0.0 首版                                 ---2026-09-24
 **********************************************************************************/
#include "usart.h"

UART_HandleTypeDef g_uart1_handle;      /* USART1句柄(全局, 供_write/重定向使用) */

/*************************************
 * 函数名称 ： uart1_init
 * 描述     ： USART1初始化, PA9=TX/PA10=RX复用推挽, 115200-8-N-1
 * 输入     ： baudrate - 波特率(如115200)
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void uart1_init(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA9=TX, PA10=RX: 复用推挽, AF7=USART1 */
    gpio_init_struct.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    gpio_init_struct.Mode      = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull      = GPIO_PULLUP;
    gpio_init_struct.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio_init_struct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);

    g_uart1_handle.Instance          = USART1;
    g_uart1_handle.Init.BaudRate     = baudrate;
    g_uart1_handle.Init.WordLength   = UART_WORDLENGTH_8B;
    g_uart1_handle.Init.StopBits     = UART_STOPBITS_1;
    g_uart1_handle.Init.Parity       = UART_PARITY_NONE;
    g_uart1_handle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    g_uart1_handle.Init.Mode         = UART_MODE_TX_RX;
    g_uart1_handle.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    g_uart1_handle.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    g_uart1_handle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&g_uart1_handle) != HAL_OK)
    {
        Error_Handler();
    }
    /* U5的FIFO默认关闭即可, 无需额外使能 */
}

/*************************************
 * 函数名称 ： _write
 * 描述     ： GCC newlib printf重定向: 字符串经USART1阻塞发出
 * 输入     ： file - 文件描述符(未用); ptr - 数据缓冲; len - 长度
 * 输出     ： 无
 * 返回     ： 实际发送字节数
 **************************************/
int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&g_uart1_handle, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}
