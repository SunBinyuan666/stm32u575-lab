/***********************************************************************************
 * @file      gw_gray.h
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.0.0
 * @date      2026-09-28
 * @brief     感为8路灰度传感器驱动 (支持 并行/串行/I2C 三种通讯模式)
 *
 *            传感器供电 5V, 输出逻辑 (手册 §2.4):
 *            某路接近白场 -> 高电平(1); 接近黑场 -> 低电平(0)
 *            数字量 bit0=第1路 ... bit7=第8路
 *
 *            3.3V 主控 (STM32U575) 接线注意 (手册 §5.3/§6.3):
 *            - 并行/串行模式: 必须插 PULL 跳线帽 (开漏模式) + GPIO 上拉输入
 *            - I2C 模式: 无需开漏, 总线由主机上拉 (外置 1k~10k 到 3.3V)
 *
 * @history   V1.0.0 首版: 并行/串行/I2C 三模式驱动                ---2026-09-28
 **********************************************************************************/
#ifndef __GW_GRAY_H
#define __GW_GRAY_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * 包含
 * ========================================================================== */
#include "stm32u5xx_hal.h"

/* ==========================================================================
 * 通讯模式选择 (三选一, 编译期决定)
 * ========================================================================== */
#define GW_GRAY_MODE_PARALLEL   0   /* 并行: 8 根 GPIO 读 8 路数字量      */
#define GW_GRAY_MODE_SERIAL     0   /* 串行: CLK+DAT 两线移位读取         */
#define GW_GRAY_MODE_I2C        1   /* I2C: 命令+数据, 功能最全           */

/* ==========================================================================
 * 公共常量
 * ========================================================================== */
#define GW_GRAY_CH_NUM          8U  /* 通道数                              */

/* 输出电平含义 (手册 §2.4) */
#define GW_GRAY_LEVEL_WHITE     1U  /* 白场 -> 高电平                      */
#define GW_GRAY_LEVEL_BLACK     0U  /* 黑场 -> 低电平                      */

/* 错误寄存器位定义 (手册 §7.14, 命令 0xDE) */
#define GW_GRAY_ERR_OVEREXP     0x01U   /* bit0: 对管过曝 (强光)           */
#define GW_GRAY_ERR_KEY_SHORT   0x02U   /* bit1: 按键对地短路超15s         */

#if GW_GRAY_MODE_I2C
/* ==========================================================================
 * I2C 模式配置
 * ========================================================================== */
/* 7bit 从机地址 = 5bit软件地址(出厂0b10011) + 2bit硬件地址(跳线帽AD1/AD0)
 * 默认 (均不插跳线帽): 0b1001110 = 0x4E
 * 若 AD1/AD0 均插跳线帽则为 0x4F。HAL 库使用 8bit 地址(自动补读写位)     */
#define GW_GRAY_I2C_ADDR_7BIT   (0x4EU << 1)

/* 命令符 (手册 §7.17) */
#define GW_GRAY_CMD_DIGITAL     0xDDU    /* 读8路数字量(打包1字节)         */
#define GW_GRAY_CMD_ANALOG_ONE  0xB1U    /* 单通道模拟量, 0xB1~0xB8        */
#define GW_GRAY_CMD_ANALOG_ALL  0xB0U    /* 连续读8路模拟量                */
#define GW_GRAY_CMD_CH_ENABLE   0xCEU    /* 传输通道使能寄存器 (读写)      */
#define GW_GRAY_CMD_CH_FLAT     0xCFU    /* 归一化使能寄存器 (V3.6+, 读写) */
#define GW_GRAY_CMD_GRAY_B      0xD0U    /* 灰黑阈值参数 (读写, 8路连续)   */
#define GW_GRAY_CMD_GRAY_W      0xD1U    /* 灰白阈值参数 (读写, 8路连续)   */
#define GW_GRAY_CMD_ADDR        0xADU    /* 软件地址配置 (V3.6+连发两次)   */
#define GW_GRAY_CMD_PING        0xAAU    /* ping 诊断, 应答 0x66           */
#define GW_GRAY_CMD_PING_ACK    0x66U
#define GW_GRAY_CMD_ERROR       0xDEU    /* 读错误信息 (读后自动清零)      */
#define GW_GRAY_CMD_RESET       0xC0U    /* 设备软件重启                   */
#define GW_GRAY_CMD_VERSION     0xC1U    /* 固件版本号, 高4bit.低4bit      */

#endif /* GW_GRAY_MODE_I2C */

#if GW_GRAY_MODE_PARALLEL
/* ==========================================================================
 * 并行模式配置 — 按实际接线修改以下 8 个引脚
 * ========================================================================== */
#define GW_GRAY_PAR_PORT        GPIOA
#define GW_GRAY_PAR_PIN_CH1     GPIO_PIN_0   /* 传感器并口1 */
#define GW_GRAY_PAR_PIN_CH2     GPIO_PIN_1   /* 传感器并口2 */
#define GW_GRAY_PAR_PIN_CH3     GPIO_PIN_2
#define GW_GRAY_PAR_PIN_CH4     GPIO_PIN_3
#define GW_GRAY_PAR_PIN_CH5     GPIO_PIN_4
#define GW_GRAY_PAR_PIN_CH6     GPIO_PIN_5
#define GW_GRAY_PAR_PIN_CH7     GPIO_PIN_6
#define GW_GRAY_PAR_PIN_CH8     GPIO_PIN_7
#endif /* GW_GRAY_MODE_PARALLEL */

#if GW_GRAY_MODE_SERIAL
/* ==========================================================================
 * 串行模式配置 — CLK 推挽输出, DAT 上拉输入 (PULL 跳线帽必须插)
 * ========================================================================== */
#define GW_GRAY_SER_CLK_PORT    GPIOA
#define GW_GRAY_SER_CLK_PIN     GPIO_PIN_8
#define GW_GRAY_SER_DAT_PORT    GPIOA
#define GW_GRAY_SER_DAT_PIN     GPIO_PIN_9
#endif /* GW_GRAY_MODE_SERIAL */

/* ==========================================================================
 * 函数声明
 * ========================================================================== */

/* --- 通用 (三模式均有) ---------------------------------------------------*/
void     gw_gray_init(void);
uint8_t  gw_gray_read_digital(void);

/* --- 仅 I2C 模式 ---------------------------------------------------------*/
#if GW_GRAY_MODE_I2C
HAL_StatusTypeDef gw_gray_ping(void);               /* 阻塞直到应答0x66     */
HAL_StatusTypeDef gw_gray_read_analog_all(uint8_t *p_buf);    /* 8路模拟量  */
HAL_StatusTypeDef gw_gray_read_analog_one(uint8_t ch, uint8_t *p_val);
HAL_StatusTypeDef gw_gray_read_version(uint8_t *p_ver);
HAL_StatusTypeDef gw_gray_read_error(uint8_t *p_err);
HAL_StatusTypeDef gw_gray_read_channel_enable(uint8_t *p_mask);
HAL_StatusTypeDef gw_gray_write_channel_enable(uint8_t mask);
HAL_StatusTypeDef gw_gray_software_reset(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __GW_GRAY_H */
