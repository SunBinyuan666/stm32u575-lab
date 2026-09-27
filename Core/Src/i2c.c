/***********************************************************************************
 * @file      i2c.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.1.0
 * @date      2026-09-27
 * @brief     I2C1总线初始化实现（PB6=SCL/PB7=SDA, 100kHz标准模式）
 *            挂载扩展板三传感器: SHT20(0x40)/AP3216C(0x1E)/MAX30102(0x57)
 *
 * @history   V1.1.0 按WT-WI-PE-200 B1规范重构               ---2026-09-27
 *            V1.0.0 首版: I2C1主机初始化                    ---2026-09-27
 **********************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "i2c.h"

/* Private defines -----------------------------------------------------------*/
/* 100kHz @ PCLK1=160MHz (CubeMX典型值) */
#define I2C1_TIMING_VALUE       0x30909DECU

/* Variable definitions ------------------------------------------------------*/
I2C_HandleTypeDef g_hi2c1;

/* Functions -----------------------------------------------------------------*/
/*************************************
 * 函数名称 ： i2c1_init
 * 描述     ： I2C1初始化, 100kHz标准模式, 7位地址
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void i2c1_init(void)
{
    g_hi2c1.Instance = I2C1;

    /* 指定初始化器明确每个成员(B1规则6-1) */
    g_hi2c1.Init.Timing            = I2C1_TIMING_VALUE;
    g_hi2c1.Init.OwnAddress1       = 0x00U;
    g_hi2c1.Init.AddressingMode    = I2C_ADDRESSINGMODE_7BIT;
    g_hi2c1.Init.DualAddressMode   = I2C_DUALADDRESS_DISABLE;
    g_hi2c1.Init.OwnAddress2       = 0x00U;
    g_hi2c1.Init.OwnAddress2Masks  = I2C_OA2_NOMASK;
    g_hi2c1.Init.GeneralCallMode   = I2C_GENERALCALL_DISABLE;
    /* 允许时钟延展(SHT20 hold模式依赖) */
    g_hi2c1.Init.NoStretchMode     = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&g_hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
    /* Analog noise filter 默认开启, 不额外配置 */
}

/*************************************
 * 函数名称 ： HAL_I2C_MspInit
 * 描述     ： I2C底层初始化(GPIO复用+时钟), 覆盖HAL弱符号
 * 输入     ： hi2c - I2C句柄
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef gpio_init = {0};

    if (hi2c->Instance == I2C1)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_I2C1_CLK_ENABLE();

        /* PB6=I2C1_SCL, PB7=I2C1_SDA: AF4开漏, 板上已有6.8k外部上拉 */
        gpio_init.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
        gpio_init.Mode      = GPIO_MODE_AF_OD;
        gpio_init.Pull      = GPIO_NOPULL;
        gpio_init.Speed     = GPIO_SPEED_FREQ_LOW;
        gpio_init.Alternate = GPIO_AF4_I2C1;
        HAL_GPIO_Init(GPIOB, &gpio_init);
    }
}
