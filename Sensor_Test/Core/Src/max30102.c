/***********************************************************************************
 * @file      max30102.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.1.1
 * @date      2026-09-27
 * @brief     MAX30102心率血氧传感器驱动实现（I2C地址0x57, FIFO模式）
 *            SpO2模式(Red+IR), 100sps/17bit/4k nA量程, LED电流6.4mA
 *
 * @history   V1.1.1 修复PART_ID寄存器地址错误(0xFE修订号->0xFF器件ID) ---2026-09-27
 *            V1.1.0 按WT-WI-PE-200 B1规范重构                         ---2026-09-27
 *            V1.0.0 首版: 复位+FIFO连续采样                 ---2026-09-27
 **********************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "max30102.h"
#include "i2c.h"
#include <stdbool.h>
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define MAX30102_ADDR_WRITE     (0x57U << 1)    /* 7位地址0x57, 写=0xAE */

/* 寄存器地址 */
#define MAX30102_REG_INT_STATUS1    0x00U   /* 读即清: A_FULL/PPG_RDY/ALC_OVF/PWR_RDY */
#define MAX30102_REG_INT_ENABLE1    0x02U
#define MAX30102_REG_INT_ENABLE2    0x03U
#define MAX30102_REG_FIFO_WR_PTR    0x04U
#define MAX30102_REG_OVF_COUNTER    0x05U
#define MAX30102_REG_FIFO_RD_PTR    0x06U
#define MAX30102_REG_FIFO_DATA      0x07U   /* burst读地址不递增 */
#define MAX30102_REG_FIFO_CONFIG    0x08U
#define MAX30102_REG_MODE_CONFIG    0x09U   /* 0x40=复位(自清), 0x03=SpO2模式 */
#define MAX30102_REG_SPO2_CONFIG    0x0AU
#define MAX30102_REG_LED1_PA        0x0CU   /* Red, 0.2mA每bit */
#define MAX30102_REG_LED2_PA        0x0DU   /* IR */
#define MAX30102_REG_PART_ID        0xFFU   /* Part ID固定0x15(0xFE是修订号,每片不同) */

/* 配置值 */
#define MAX30102_MODE_RESET         0x40U   /* 软复位, 位自清零 */
#define MAX30102_MODE_SPO2          0x03U   /* SpO2模式(Red+IR) */
/* SpO2配置: ADC_RGE=01(4096nA) SR=001(100sps) PW=10(215us/17bit) */
#define MAX30102_SPO2_CFG_VALUE     0x27U
#define MAX30102_LED_CURRENT_6MA4   0x20U   /* 0.2mA每bit, 0x20=6.4mA */
/* FIFO配置: 不平均/满后回卷/剩余16样本触发中断(中断未使能,仅占位) */
#define MAX30102_FIFO_CFG_VALUE     0x1FU
#define MAX30102_PART_ID_VALUE      0x15U   /* 应答的器件ID */

#define MAX30102_RESET_DELAY_MS     10U
#define MAX30102_START_DELAY_MS     50U
#define MAX30102_I2C_TIMEOUT_MS     200U

#define MAX30102_PTR_MASK           0x1FU   /* FIFO指针5bit, 回绕模32 */
#define MAX30102_DATA_MASK          0x3FFFFUL  /* 18bit有效数据掩码 */
#define MAX30102_BYTES_PER_CH       3U      /* 每通道3字节(左对齐18bit) */
#define MAX30102_BYTES_PER_SAMPLE   (MAX30102_BYTES_PER_CH * 2U)    /* Red+IR */

/* Private function prototypes -----------------------------------------------*/
static bool max30102_write_reg(uint8_t reg, uint8_t val);
static bool max30102_read_reg(uint8_t reg, uint8_t *p_val);

/* Functions -----------------------------------------------------------------*/
/*************************************
 * 函数名称 ： max30102_write_reg
 * 描述     ： 写单个寄存器
 * 输入     ： reg - 寄存器地址
 *            val  - 写入值
 * 输出     ： 无
 * 返回     ： true=成功
 **************************************/
static bool max30102_write_reg(uint8_t reg, uint8_t val)
{
    return (HAL_I2C_Mem_Write(&g_hi2c1, MAX30102_ADDR_WRITE, reg,
                              I2C_MEMADD_SIZE_8BIT, &val, 1U,
                              MAX30102_I2C_TIMEOUT_MS) == HAL_OK);
}

/*************************************
 * 函数名称 ： max30102_read_reg
 * 描述     ： 读单个寄存器
 * 输入     ： reg - 寄存器地址
 * 输出     ： p_val - 读出值
 * 返回     ： true=成功
 **************************************/
static bool max30102_read_reg(uint8_t reg, uint8_t *p_val)
{
    return (HAL_I2C_Mem_Read(&g_hi2c1, MAX30102_ADDR_WRITE, reg,
                             I2C_MEMADD_SIZE_8BIT, p_val, 1U,
                             MAX30102_I2C_TIMEOUT_MS) == HAL_OK);
}

/*************************************
 * 函数名称 ： max30102_init
 * 描述     ： 初始化: 验ID->复位->清中断->清FIFO指针->配置->进SpO2模式
 *            配置: ADC量程4096nA/采样率100sps/脉宽215us(17bit)
 *            Red+IR LED电流各6.4mA
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： true=成功
 **************************************/
bool max30102_init(void)
{
    uint8_t part_id = 0U;

    /* 验证器件ID */
    if ((max30102_read_reg(MAX30102_REG_PART_ID, &part_id) != true)
            || (part_id != MAX30102_PART_ID_VALUE))
    {
        return false;
    }

    /* 软复位(位自清零) */
    if (max30102_write_reg(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_RESET) != true)
    {
        return false;
    }
    HAL_Delay(MAX30102_RESET_DELAY_MS);

    /* 清中断: 上电PWR_RDY必置位, 关全部中断使能 */
    (void)max30102_write_reg(MAX30102_REG_INT_ENABLE1, 0x00U);
    (void)max30102_write_reg(MAX30102_REG_INT_ENABLE2, 0x00U);
    (void)max30102_write_reg(MAX30102_REG_INT_STATUS1, 0x00U);

    /* 清FIFO三个指针 */
    (void)max30102_write_reg(MAX30102_REG_FIFO_WR_PTR, 0x00U);
    (void)max30102_write_reg(MAX30102_REG_OVF_COUNTER, 0x00U);
    (void)max30102_write_reg(MAX30102_REG_FIFO_RD_PTR, 0x00U);

    /* 采集参数配置 */
    (void)max30102_write_reg(MAX30102_REG_FIFO_CONFIG, MAX30102_FIFO_CFG_VALUE);
    (void)max30102_write_reg(MAX30102_REG_SPO2_CONFIG, MAX30102_SPO2_CFG_VALUE);
    (void)max30102_write_reg(MAX30102_REG_LED1_PA, MAX30102_LED_CURRENT_6MA4);
    (void)max30102_write_reg(MAX30102_REG_LED2_PA, MAX30102_LED_CURRENT_6MA4);

    /* 进SpO2模式(Red+IR) */
    if (max30102_write_reg(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2) != true)
    {
        return false;
    }
    HAL_Delay(MAX30102_START_DELAY_MS);
    return true;
}

/*************************************
 * 函数名称 ： max30102_read_fifo
 * 描述     ： 从FIFO读取全部可用样本（红光/红外各一值每样本）
 *            FIFO数据每通道3字节左对齐, 18bit值=3字节拼合后掩码
 *            读FIFO_DATA寄存器地址不递增, burst连读即连续吐FIFO字节
 * 输入     ： p_samples - 样本数组
 *            max_num    - 数组容量(不超过MAX30102_FIFO_DEPTH)
 * 输出     ： p_samples - 读取的样本(红光/红外成对)
 * 返回     ： 实际读取的样本数(0=无数据或失败)
 **************************************/
uint8_t max30102_read_fifo(Max30102_Sample_t p_samples[], uint8_t max_num)
{
    uint8_t wr_ptr = 0U;
    uint8_t rd_ptr = 0U;
    uint8_t count;
    uint8_t rx_buf[MAX30102_FIFO_DEPTH * MAX30102_BYTES_PER_SAMPLE] = {0U};
    uint8_t index;
    uint16_t byte_base;
    uint16_t rx_len;

    if ((p_samples == NULL) || (max_num == 0U))
    {
        return 0U;
    }
    if ((max30102_read_reg(MAX30102_REG_FIFO_WR_PTR, &wr_ptr) != true)
            || (max30102_read_reg(MAX30102_REG_FIFO_RD_PTR, &rd_ptr) != true))
    {
        return 0U;
    }

    /* 指针回绕处理: 5bit模32 */
    count = (uint8_t)((wr_ptr - rd_ptr) & MAX30102_PTR_MASK);
    if (count > max_num)
    {
        count = max_num;
    }
    if (count == 0U)
    {
        return 0U;
    }

    /* 连续读count*6字节(FIFO_DATA地址不递增, 固定吐数据) */
    rx_len = (uint16_t)count * MAX30102_BYTES_PER_SAMPLE;
    if (HAL_I2C_Mem_Read(&g_hi2c1, MAX30102_ADDR_WRITE, MAX30102_REG_FIFO_DATA,
                         I2C_MEMADD_SIZE_8BIT, rx_buf, rx_len,
                         MAX30102_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return 0U;
    }

    for (index = 0U; index < count; index++)
    {
        byte_base = (uint16_t)index * MAX30102_BYTES_PER_SAMPLE;
        /* 每样本: 先Red后IR各3字节, 左对齐18bit */
        p_samples[index].red = (((uint32_t)rx_buf[byte_base] << 16)
                                | ((uint32_t)rx_buf[byte_base + 1U] << 8)
                                | (uint32_t)rx_buf[byte_base + 2U])
                               & MAX30102_DATA_MASK;
        p_samples[index].ir  = (((uint32_t)rx_buf[byte_base + 3U] << 16)
                                | ((uint32_t)rx_buf[byte_base + 4U] << 8)
                                | (uint32_t)rx_buf[byte_base + 5U])
                               & MAX30102_DATA_MASK;
    }
    return count;
}
