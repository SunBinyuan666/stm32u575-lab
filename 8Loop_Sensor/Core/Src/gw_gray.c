/***********************************************************************************
 * @file      gw_gray.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.0.0
 * @date      2026-09-28
 * @brief     感为8路灰度传感器驱动实现 (并行/串行/I2C 三模式)
 *
 *            驱动使用流程:
 *            1. gw_gray_init()      初始化 GPIO/I2C
 *            2. I2C 模式建议先 gw_gray_ping() 等待传感器就绪 (手册 §7.13.3)
 *            3. gw_gray_read_digital() 读 8 路数字量 (bit0=第1路...bit7=第8路)
 *
 * @history   V1.0.0 首版: 并行/串行/I2C 三模式驱动                ---2026-09-28
 **********************************************************************************/

/* ==========================================================================
 * 包含
 * ========================================================================== */
#include "gw_gray.h"
#include "main.h"

#if GW_GRAY_MODE_I2C
/* ==========================================================================
 * I2C 模式
 * ========================================================================== */

static I2C_HandleTypeDef s_gw_gray_hi2c;        /* I2C 句柄 (I2C1, 100kHz)  */

/*************************************
 * 函数名称 ： gw_gray_i2c_init
 * 描述     ： 初始化 I2C1 (PB6=SCL, PB7=SDA, 100kHz 标准模式)
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
static void gw_gray_i2c_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    /* I2C1 与 GPIOB 时钟使能 */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    /* PB6=SCL, PB7=SDA: 复用开漏, 上拉由外部 1k~10k 电阻提供 */
    gpio_init.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    gpio_init.Mode      = GPIO_MODE_AF_OD;
    gpio_init.Pull      = GPIO_NOPULL;
    gpio_init.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio_init.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    s_gw_gray_hi2c.Instance             = I2C1;
    /* 100kHz 标准模式 @160MHz (与 Sensor_Test 已验证值一致) */
    s_gw_gray_hi2c.Init.Timing          = 0x30909DECU;
    s_gw_gray_hi2c.Init.OwnAddress1     = 0x00U;
    s_gw_gray_hi2c.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    s_gw_gray_hi2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    s_gw_gray_hi2c.Init.OwnAddress2     = 0x00U;
    s_gw_gray_hi2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    s_gw_gray_hi2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    s_gw_gray_hi2c.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&s_gw_gray_hi2c) != HAL_OK)
    {
        Error_Handler();
    }
}

/*************************************
 * 函数名称 ： gw_gray_init
 * 描述     ： 传感器初始化入口 (I2C 模式: 初始化 I2C1 外设)
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void gw_gray_init(void)
{
    gw_gray_i2c_init();
}

/*************************************
 * 函数名称 ： gw_gray_ping
 * 描述     ： ping 网络诊断 (命令 0xAA), 阻塞直到传感器应答 0x66
 *            手册 §7.13.3: 主控与传感器同时上电时, 主控初始化可能先完成,
 *            必须用 ping 同步, 否则首条命令可能丢失
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： HAL_OK-在线; HAL_ERROR-超时未应答
 **************************************/
HAL_StatusTypeDef gw_gray_ping(void)
{
    uint8_t ack = 0U;
    uint8_t retry;

    for (retry = 0U; retry < 100U; retry++)
    {
        /* 写命令 0xAA (带停止位) */
        if (HAL_I2C_Master_Transmit(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                    (uint8_t *)&(uint8_t){GW_GRAY_CMD_PING},
                                    1U, 100U) == HAL_OK)
        {
            /* 读应答 */
            if ((HAL_I2C_Master_Receive(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                        &ack, 1U, 100U) == HAL_OK) &&
                (ack == GW_GRAY_CMD_PING_ACK))
            {
                return HAL_OK;
            }
        }
        HAL_Delay(10U);     /* 传感器上电初始化约需数十 ms, 轮询等待 */
    }
    return HAL_ERROR;
}

/*************************************
 * 函数名称 ： gw_gray_read_digital
 * 描述     ： 读 8 路数字量 (命令 0xDD)。bit0=第1路 ... bit7=第8路;
 *            1=白场(高电平), 0=黑场(低电平)
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 8 位数字量; 通讯失败返回 0x00 (全黑, 便于上层判错)
 **************************************/
uint8_t gw_gray_read_digital(void)
{
    uint8_t cmd = GW_GRAY_CMD_DIGITAL;
    uint8_t val = 0x00U;

    /* 写命令 (带停止位, 简单可靠; 后续可优化为不复位反复读) */
    if (HAL_I2C_Master_Transmit(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                &cmd, 1U, 100U) != HAL_OK)
    {
        return 0x00U;
    }
    /* 读 1 字节数字量 */
    if (HAL_I2C_Master_Receive(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                               &val, 1U, 100U) != HAL_OK)
    {
        return 0x00U;
    }
    return val;
}

/*************************************
 * 函数名称 ： gw_gray_read_analog_all
 * 描述     ： 连续读 8 路模拟量 (命令 0xB0)。传感器内部 10bit ADC 压缩
 *            至 8bit 传输 (手册 §7.8.1)
 * 输入     ： 无
 * 输出     ： p_buf - 8 字节数组, 下标 0~7 对应第 1~8 路模拟量
 * 返回     ： HAL_OK 成功
 **************************************/
HAL_StatusTypeDef gw_gray_read_analog_all(uint8_t *p_buf)
{
    uint8_t cmd = GW_GRAY_CMD_ANALOG_ALL;

    if ((p_buf == NULL) ||
        (HAL_I2C_Master_Transmit(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                 &cmd, 1U, 100U) != HAL_OK))
    {
        return HAL_ERROR;
    }
    return HAL_I2C_Master_Receive(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                  p_buf, GW_GRAY_CH_NUM, 100U);
}

/*************************************
 * 函数名称 ： gw_gray_read_analog_one
 * 描述     ： 单通道读模拟量 (命令 0xB1~0xB8)
 * 输入     ： ch - 通道号 1~8
 * 输出     ： p_val - 模拟量 (8bit, 白场值大黑场值小)
 * 返回     ： HAL_OK 成功
 **************************************/
HAL_StatusTypeDef gw_gray_read_analog_one(uint8_t ch, uint8_t *p_val)
{
    uint8_t cmd;

    if ((p_val == NULL) || (ch < 1U) || (ch > GW_GRAY_CH_NUM))
    {
        return HAL_ERROR;
    }
    cmd = (uint8_t)(GW_GRAY_CMD_ANALOG_ONE + ch - 1U);

    if (HAL_I2C_Master_Transmit(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                &cmd, 1U, 100U) != HAL_OK)
    {
        return HAL_ERROR;
    }
    return HAL_I2C_Master_Receive(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                  p_val, 1U, 100U);
}

/*************************************
 * 函数名称 ： gw_gray_read_version
 * 描述     ： 读固件版本号 (命令 0xC1)。高 4bit=高位版本, 低 4bit=低位版本,
 *            例: V3.14 -> 0x3E
 * 输入     ： 无
 * 输出     ： p_ver - 版本号字节
 * 返回     ： HAL_OK 成功
 **************************************/
HAL_StatusTypeDef gw_gray_read_version(uint8_t *p_ver)
{
    uint8_t cmd = GW_GRAY_CMD_VERSION;

    if ((p_ver == NULL) ||
        (HAL_I2C_Master_Transmit(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                 &cmd, 1U, 100U) != HAL_OK))
    {
        return HAL_ERROR;
    }
    return HAL_I2C_Master_Receive(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                  p_ver, 1U, 100U);
}

/*************************************
 * 函数名称 ： gw_gray_read_error
 * 描述     ： 读错误信息寄存器 (命令 0xDE)。bit0=对管过曝, bit1=按键短路;
 *            读后寄存器自动清零 (手册 §7.14.1)
 * 输入     ： 无
 * 输出     ： p_err - 错误位图
 * 返回     ： HAL_OK 成功
 **************************************/
HAL_StatusTypeDef gw_gray_read_error(uint8_t *p_err)
{
    uint8_t cmd = GW_GRAY_CMD_ERROR;

    if ((p_err == NULL) ||
        (HAL_I2C_Master_Transmit(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                 &cmd, 1U, 100U) != HAL_OK))
    {
        return HAL_ERROR;
    }
    return HAL_I2C_Master_Receive(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                  p_err, 1U, 100U);
}

/*************************************
 * 函数名称 ： gw_gray_read_channel_enable
 * 描述     ： 读传输通道使能寄存器 (命令 0xCE)。每 bit 对应一路, 置位使能;
 *            复位值 0xFF, 不掉电保存
 * 输入     ： 无
 * 输出     ： p_mask - 通道使能位图
 * 返回     ： HAL_OK 成功
 **************************************/
HAL_StatusTypeDef gw_gray_read_channel_enable(uint8_t *p_mask)
{
    uint8_t cmd = GW_GRAY_CMD_CH_ENABLE;

    if ((p_mask == NULL) ||
        (HAL_I2C_Master_Transmit(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                 &cmd, 1U, 100U) != HAL_OK))
    {
        return HAL_ERROR;
    }
    return HAL_I2C_Master_Receive(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                  p_mask, 1U, 100U);
}

/*************************************
 * 函数名称 ： gw_gray_write_channel_enable
 * 描述     ： 写传输通道使能寄存器 (命令 0xCE+1字节数据)。
 *            例: 0b01010101 = 只传输第 1/3/5/7 路
 * 输入     ： mask - 通道使能位图 (bit0=第1路 ... bit7=第8路)
 * 输出     ： 无
 * 返回     ： HAL_OK 成功
 **************************************/
HAL_StatusTypeDef gw_gray_write_channel_enable(uint8_t mask)
{
    uint8_t buf[2];

    buf[0] = GW_GRAY_CMD_CH_ENABLE;
    buf[1] = mask;
    return HAL_I2C_Master_Transmit(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                   buf, 2U, 100U);
}

/*************************************
 * 函数名称 ： gw_gray_software_reset
 * 描述     ： 设备软件重启 (命令 0xC0)。重启后须重新 gw_gray_ping() 同步
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： HAL_OK 成功
 **************************************/
HAL_StatusTypeDef gw_gray_software_reset(void)
{
    uint8_t cmd = GW_GRAY_CMD_RESET;
    return HAL_I2C_Master_Transmit(&s_gw_gray_hi2c, GW_GRAY_I2C_ADDR_7BIT,
                                   &cmd, 1U, 100U);
}

#endif /* GW_GRAY_MODE_I2C */

#if GW_GRAY_MODE_SERIAL
/* ==========================================================================
 * 串行模式 (手册 §6): CLK 推挽输出, DAT 上拉输入 (PULL 跳线帽必须插)
 * 时序三要点 (手册 §6.2):
 *   1. 高电平写低电平读: CLK 拉低后才能读 DAT
 *   2. CLK 高电平维持 >= 5us
 *   3. 每帧 8 个时钟间隔须远小于 1ms, 帧间 delay>=1ms 归零防错位
 * ========================================================================== */

/*************************************
 * 函数名称 ： gw_gray_init
 * 描述     ： 传感器初始化入口 (串行模式: CLK 推挽输出, DAT 上拉输入)
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void gw_gray_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* CLK: 推挽输出, 空闲低电平 */
    gpio_init.Pin   = GW_GRAY_SER_CLK_PIN;
    gpio_init.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull  = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GW_GRAY_SER_CLK_PORT, &gpio_init);
    HAL_GPIO_WritePin(GW_GRAY_SER_CLK_PORT, GW_GRAY_SER_CLK_PIN, GPIO_PIN_RESET);

    /* DAT: 上拉输入 (开漏模式, PULL 跳线帽必须插, 3.3V 主控保护) */
    gpio_init.Pin   = GW_GRAY_SER_DAT_PIN;
    gpio_init.Mode  = GPIO_MODE_INPUT;
    gpio_init.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(GW_GRAY_SER_DAT_PORT, &gpio_init);
}

/*************************************
 * 函数名称 ： gw_gray_read_digital
 * 描述     ： 串行移位读 8 路数字量。每个时钟下降沿读 1 位,
 *            高电平写低电平读, CLK 高电平维持 5us (手册 §6.4 例程移植)
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 8 位数字量 (bit0=第1路 ... bit7=第8路)
 **************************************/
uint8_t gw_gray_read_digital(void)
{
    uint8_t ret = 0U;
    uint8_t i;

    for (i = 0U; i < GW_GRAY_CH_NUM; i++)
    {
        /* 时钟拉低, 进入读取段 (低电平读, 无需延时) */
        HAL_GPIO_WritePin(GW_GRAY_SER_CLK_PORT, GW_GRAY_SER_CLK_PIN,
                          GPIO_PIN_RESET);
        ret = (uint8_t)(ret |
              (uint8_t)((HAL_GPIO_ReadPin(GW_GRAY_SER_DAT_PORT,
                                          GW_GRAY_SER_DAT_PIN) != GPIO_PIN_RESET)
                        << i));

        /* 时钟拉高, 维持至少 5us (移位寄存器运算时间) */
        HAL_GPIO_WritePin(GW_GRAY_SER_CLK_PORT, GW_GRAY_SER_CLK_PIN,
                          GPIO_PIN_SET);
        delay_us(5U);
    }
    return ret;
}

#endif /* GW_GRAY_MODE_SERIAL */

#if GW_GRAY_MODE_PARALLEL
/* ==========================================================================
 * 并行模式 (手册 §5): 8 根 GPIO 独立读
 * 普通(PULL 跳线帽不插): 悬浮输入; 开漏(PULL 插): 上拉输入 (3.3V 主控必用)
 * ========================================================================== */

static const uint16_t s_gw_gray_par_pin[GW_GRAY_CH_NUM] =
{
    GW_GRAY_PAR_PIN_CH1, GW_GRAY_PAR_PIN_CH2,
    GW_GRAY_PAR_PIN_CH3, GW_GRAY_PAR_PIN_CH4,
    GW_GRAY_PAR_PIN_CH5, GW_GRAY_PAR_PIN_CH6,
    GW_GRAY_PAR_PIN_CH7, GW_GRAY_PAR_PIN_CH8
};

/*************************************
 * 函数名称 ： gw_gray_init
 * 描述     ： 传感器初始化入口 (并行模式: 8 路 GPIO 悬浮输入)
 *            注: 若插了 PULL 跳线帽, 请把 GPIO_NOPULL 改为 GPIO_PULLUP
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void gw_gray_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};
    uint8_t i;

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio_init.Pin   = (GW_GRAY_PAR_PIN_CH1 | GW_GRAY_PAR_PIN_CH2 |
                       GW_GRAY_PAR_PIN_CH3 | GW_GRAY_PAR_PIN_CH4 |
                       GW_GRAY_PAR_PIN_CH5 | GW_GRAY_PAR_PIN_CH6 |
                       GW_GRAY_PAR_PIN_CH7 | GW_GRAY_PAR_PIN_CH8);
    gpio_init.Mode  = GPIO_MODE_INPUT;
    gpio_init.Pull  = GPIO_NOPULL;      /* PULL 跳线帽插上时改 GPIO_PULLUP */
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GW_GRAY_PAR_PORT, &gpio_init);

    (void)i;
}

/*************************************
 * 函数名称 ： gw_gray_read_digital
 * 描述     ： 并行读 8 路数字量, 逐路读电平打包
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 8 位数字量 (bit0=第1路 ... bit7=第8路)
 **************************************/
uint8_t gw_gray_read_digital(void)
{
    uint8_t ret = 0U;
    uint8_t i;

    for (i = 0U; i < GW_GRAY_CH_NUM; i++)
    {
        if (HAL_GPIO_ReadPin(GW_GRAY_PAR_PORT, s_gw_gray_par_pin[i]) !=
            GPIO_PIN_RESET)
        {
            ret |= (uint8_t)(1U << i);
        }
    }
    return ret;
}

#endif /* GW_GRAY_MODE_PARALLEL */
