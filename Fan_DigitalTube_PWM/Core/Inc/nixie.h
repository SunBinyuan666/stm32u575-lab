/***********************************************************************************
 * @file      nixie.h
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.0.0
 * @date      2026-09-28
 * @brief     74HC595 菊花链四位共阳数码管驱动(SPI2 + PB12 软件锁存)
 *
 *            硬件链: SPI2_MOSI(PB15)→U2(段选)→QH'→U6(位选), SPI2_SCK(PB13)共时钟,
 *            PB12=RCLK 锁存沿; FJ3461AH 共阳, 段码低电平点亮。
 *            华清 U5 教程接线: 扩展板 J1(SPI4 丝印)桥接底板 J6(SPI2 实脚)。
 *
 * @history   V1.0.0 首版: 帧缓冲+字节序[段,位]+锁存沿+共阳段码表         ---2026-09-28
 **********************************************************************************/
#ifndef __NIXIE_H
#define __NIXIE_H

#include "stm32u5xx_hal.h"

/* ==========================================================================
 * 参数宏定义
 * ========================================================================== */
#define NIXIE_DIGIT_NUM         4U      /* 四位数码管                            */
#define NIXIE_SPI_PRESCALER     128U    /* 160M/128=1.25MHz: 教程实测过快会错位  */
#define NIXIE_LATCH_DELAY_US    2U      /* 锁存沿保持时间(us)                    */

/* ==========================================================================
 * 函数声明
 * ========================================================================== */
void nixie_init(void);
void nixie_set_digit(uint8_t index, uint8_t seg_pattern);
void nixie_show_number(uint16_t value);
void nixie_refresh(void);

#endif /* __NIXIE_H */
