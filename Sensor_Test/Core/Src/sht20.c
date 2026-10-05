/***********************************************************************************
 * @file      sht20.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.1.0
 * @date      2026-09-27
 * @brief     SHT20温湿度传感器驱动实现（I2C地址0x40, hold master模式）
 *            兼容SI7006（扩展板U11为SI7006/SHT20兼容位, 命令与换算公式相同）
 *
 * @history   V1.1.0 按WT-WI-PE-200 B1规范重构               ---2026-09-27
 *            V1.0.0 首版: 软复位+温度湿度采集               ---2026-09-27
 **********************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "sht20.h"
#include "i2c.h"
#include <stdbool.h>

/* Private defines -----------------------------------------------------------*/
#define SHT20_ADDR_WRITE        (0x40U << 1)    /* 7位地址0x40, 写=0x80 */
#define SHT20_ADDR_READ         ((0x40U << 1) | 0x01U)  /* 读=0x81 */

#define SHT20_CMD_TEMP          0xE3U   /* 触发温度测量(hold master), 最长85ms */
#define SHT20_CMD_RH            0xE5U   /* 触发湿度测量(hold master), 最长29ms */
#define SHT20_CMD_SOFT_RESET    0xFEU   /* 软复位, 最长15ms */

#define SHT20_RESET_DELAY_MS    15U     /* 软复位等待时间 */
#define SHT20_I2C_TIMEOUT_MS    200U    /* hold模式含时钟延展, 超时要给足 */

#define SHT20_RAW_MASK          0xFFFCU /* 末2位为状态位, 换算前必须清零 */

/* Private function prototypes -----------------------------------------------*/
static bool sht20_read_raw(uint8_t cmd, uint16_t *p_raw);

/* Functions -----------------------------------------------------------------*/
/*************************************
 * 函数名称 ： sht20_read_raw
 * 描述     ： 发测量命令并读取原始16位数据（hold master模式）
 *            SHT20测量期间拉低SCL(时钟延展), HAL阻塞式API天然兼容
 * 输入     ： cmd - 测量命令(SHT20_CMD_TEMP/ SHT20_CMD_RH)
 * 输出     ： p_raw - 原始ADC值(已清末2位状态位)
 * 返回     ： true=成功, false=失败
 **************************************/
static bool sht20_read_raw(uint8_t cmd, uint16_t *p_raw)
{
    uint8_t rx_buf[3] = {0U};
    uint16_t raw;

    /* 两段式: 写命令(无寄存器地址) -> 读2字节数据 */
    if (HAL_I2C_Master_Transmit(&g_hi2c1, SHT20_ADDR_WRITE, &cmd,
                                1U, SHT20_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return false;
    }
    if (HAL_I2C_Master_Receive(&g_hi2c1, SHT20_ADDR_READ, rx_buf,
                               3U, SHT20_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return false;
    }
    raw = (uint16_t)(((uint16_t)rx_buf[0] << 8) | (uint16_t)rx_buf[1]);
    *p_raw = (uint16_t)(raw & SHT20_RAW_MASK);
    return true;
}

/*************************************
 * 函数名称 ： sht20_init
 * 描述     ： 软复位传感器（恢复默认12bit RH/14bit T分辨率）
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： true=成功
 **************************************/
bool sht20_init(void)
{
    uint8_t cmd = SHT20_CMD_SOFT_RESET;

    if (HAL_I2C_Master_Transmit(&g_hi2c1, SHT20_ADDR_WRITE, &cmd,
                                1U, SHT20_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return false;
    }
    HAL_Delay(SHT20_RESET_DELAY_MS);
    return true;
}

/*************************************
 * 函数名称 ： sht20_read_temp_hum
 * 描述     ： 采集温度与相对湿度并换算物理量
 *            T = -46.85 + 175.72*raw/65536 (摄氏度)
 *            RH = -6 + 125*raw/65536 (%RH)
 * 输入     ： 无
 * 输出     ： p_temp - 温度(摄氏度)
 *            p_hum  - 相对湿度(%RH)
 * 返回     ： true=成功
 **************************************/
bool sht20_read_temp_hum(float *p_temp, float *p_hum)
{
    uint16_t raw_temp = 0U;
    uint16_t raw_rh = 0U;

    if ((p_temp == NULL) || (p_hum == NULL))
    {
        return false;
    }
    if (sht20_read_raw(SHT20_CMD_TEMP, &raw_temp) != true)
    {
        return false;
    }
    if (sht20_read_raw(SHT20_CMD_RH, &raw_rh) != true)
    {
        return false;
    }
    *p_temp = -46.85F + (175.72F * (float)raw_temp / 65536.0F);
    *p_hum  = -6.0F + (125.0F * (float)raw_rh / 65536.0F);
    return true;
}
