/***********************************************************************************
 * @file      ap3216c.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.1.1
 * @date      2026-09-27
 * @brief     AP3216C环境光/接近/红外三合一传感器驱动实现（I2C地址0x1E）
 *            ALS 16bit / PS+IR 10bit, 全功能连续模式, 默认量程20661lux
 *
 * @history   V1.1.1 修复PS/IR数据拼位错误(高字节截断致读数饱和)   ---2026-09-27
 *            V1.1.0 按WT-WI-PE-200 B1规范重构                     ---2026-09-27
 *            V1.0.0 首版: 软复位+三通道读取                 ---2026-09-27
 **********************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "ap3216c.h"
#include "i2c.h"
#include <stdbool.h>

/* Private defines -----------------------------------------------------------*/
#define AP3216C_ADDR_WRITE      (0x1EU << 1)    /* 7位地址0x1E, 写=0x3C */

/* 系统寄存器 */
#define AP3216C_REG_SYS_CONFIG  0x00U   /* bit2:0 工作模式, 0x03=ALS+PS+IR连续 */
#define AP3216C_REG_IR_DATA_L   0x0AU   /* bit7=IR_OF溢出, bit1:0=IR低2位 */
#define AP3216C_REG_IR_DATA_H   0x0BU
#define AP3216C_REG_ALS_DATA_L  0x0CU
#define AP3216C_REG_ALS_DATA_H  0x0DU
#define AP3216C_REG_PS_DATA_L   0x0EU   /* bit7=OBJ靠近, bit6=IR_OF, bit3:0=PS低4位 */
#define AP3216C_REG_PS_DATA_H   0x0FU

/* 工作模式(System Config bit2:0) */
#define AP3216C_MODE_SW_RESET   0x04U   /* 软复位 */
#define AP3216C_MODE_ALS_PS_IR  0x03U   /* ALS+PS+IR全功能连续 */

#define AP3216C_RESET_DELAY_MS  10U     /* 复位期间10ms内禁止发命令 */
#define AP3216C_CONV_DELAY_MS   120U    /* 等首轮转换完成(ALS 100ms) */
#define AP3216C_I2C_TIMEOUT_MS  100U

/* 位域掩码 */
#define AP3216C_PS_L_MASK       0x0FU   /* PS低字节bit3:0有效 */
#define AP3216C_IR_L_MASK       0x03U   /* IR低字节bit1:0有效 */
#define AP3216C_PS_H_MASK       0x3FU   /* PS高字节bit5:0有效(10bit=H6位+L4位) */
#define AP3216C_PS_OBJ_BIT      0x80U   /* PS低字节bit7=OBJ物体靠近 */

/* Private function prototypes -----------------------------------------------*/
static bool ap3216c_write_reg(uint8_t reg, uint8_t val);
static bool ap3216c_read_reg(uint8_t reg, uint8_t *p_val);

/* Functions -----------------------------------------------------------------*/
/*************************************
 * 函数名称 ： ap3216c_write_reg
 * 描述     ： 写单个寄存器
 * 输入     ： reg - 寄存器地址
 *            val  - 写入值
 * 输出     ： 无
 * 返回     ： true=成功
 **************************************/
static bool ap3216c_write_reg(uint8_t reg, uint8_t val)
{
    return (HAL_I2C_Mem_Write(&g_hi2c1, AP3216C_ADDR_WRITE, reg,
                              I2C_MEMADD_SIZE_8BIT, &val, 1U,
                              AP3216C_I2C_TIMEOUT_MS) == HAL_OK);
}

/*************************************
 * 函数名称 ： ap3216c_read_reg
 * 描述     ： 读单个寄存器
 * 输入     ： reg - 寄存器地址
 * 输出     ： p_val - 读出值
 * 返回     ： true=成功
 **************************************/
static bool ap3216c_read_reg(uint8_t reg, uint8_t *p_val)
{
    return (HAL_I2C_Mem_Read(&g_hi2c1, AP3216C_ADDR_WRITE, reg,
                             I2C_MEMADD_SIZE_8BIT, p_val, 1U,
                             AP3216C_I2C_TIMEOUT_MS) == HAL_OK);
}

/*************************************
 * 函数名称 ： ap3216c_init
 * 描述     ： 初始化: 软复位->默认参数->ALS+PS+IR全功能连续模式
 *            默认ALS量程20661lux(分辨率0.35lux/count), PS增益x2
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： true=成功
 **************************************/
bool ap3216c_init(void)
{
    if (ap3216c_write_reg(AP3216C_REG_SYS_CONFIG, AP3216C_MODE_SW_RESET) != true)
    {
        return false;
    }
    HAL_Delay(AP3216C_RESET_DELAY_MS);

    /* 0x10 ALS配置/0x20 PS配置/0x21 LED控制均用默认值, 直接进全功能模式 */
    if (ap3216c_write_reg(AP3216C_REG_SYS_CONFIG, AP3216C_MODE_ALS_PS_IR) != true)
    {
        return false;
    }
    HAL_Delay(AP3216C_CONV_DELAY_MS);
    return true;
}

/*************************************
 * 函数名称 ： ap3216c_read_data
 * 描述     ： 读取ALS/PS/IR三通道原始数据
 *            读序必须先L后H: 读L时硬件锁存H到临时寄存器(防撕裂)
 * 输入     ： 无
 * 输出     ： p_data - 三通道原始值+物体靠近标志
 * 返回     ： true=成功
 **************************************/
bool ap3216c_read_data(Ap3216c_Data_t *p_data)
{
    uint8_t als_lo = 0U;
    uint8_t als_hi = 0U;
    uint8_t ps_lo = 0U;
    uint8_t ps_hi = 0U;
    uint8_t ir_lo = 0U;
    uint8_t ir_hi = 0U;

    if (p_data == NULL)
    {
        return false;
    }
    /* ALS: 16bit, 先L后H */
    if ((ap3216c_read_reg(AP3216C_REG_ALS_DATA_L, &als_lo) != true)
            || (ap3216c_read_reg(AP3216C_REG_ALS_DATA_H, &als_hi) != true))
    {
        return false;
    }
    /* PS: 10bit, 先L后H */
    if ((ap3216c_read_reg(AP3216C_REG_PS_DATA_L, &ps_lo) != true)
            || (ap3216c_read_reg(AP3216C_REG_PS_DATA_H, &ps_hi) != true))
    {
        return false;
    }
    /* IR: 10bit, 先L后H */
    if ((ap3216c_read_reg(AP3216C_REG_IR_DATA_L, &ir_lo) != true)
            || (ap3216c_read_reg(AP3216C_REG_IR_DATA_H, &ir_hi) != true))
    {
        return false;
    }

    p_data->als_raw = (uint16_t)(((uint16_t)als_hi << 8) | (uint16_t)als_lo);
    /* PS 10bit = H寄存器bit5:0 << 4 | L寄存器bit3:0 (H/L均含标志位需屏蔽) */
    p_data->ps_raw  = (uint16_t)((((uint16_t)ps_hi & AP3216C_PS_H_MASK) << 4)
                                 | (uint16_t)(ps_lo & AP3216C_PS_L_MASK));
    /* IR 10bit = H寄存器bit7:0 << 2 | L寄存器bit1:0 */
    p_data->ir_raw  = (uint16_t)((((uint16_t)ir_hi) << 2)
                                 | (uint16_t)(ir_lo & AP3216C_IR_L_MASK));
    p_data->object_near = ((ps_lo & AP3216C_PS_OBJ_BIT) != 0U);
    return true;
}
