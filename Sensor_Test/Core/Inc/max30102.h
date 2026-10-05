/***********************************************************************************
 * @file      max30102.h
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.1.1
 * @date      2026-09-27
 * @brief     MAX30102心率血氧传感器对外接口（FIFO模式, SpO2双通道）
 *            寄存器定义属内部实现, 不对外暴露(信息隐藏)
 *
 * @history   V1.1.1 修复PART_ID寄存器地址错误                     ---2026-09-27
 *            V1.1.0 按WT-WI-PE-200 B1规范重构接口                 ---2026-09-27
 *            V1.0.0 首版: 复位+FIFO连续采样                    ---2026-09-27
 **********************************************************************************/
#ifndef MAX30102_H
#define MAX30102_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "main.h"

/* Macros --------------------------------------------------------------------*/
#define MAX30102_FIFO_DEPTH     32U     /* 芯片FIFO深度(样本) */

/* Types ---------------------------------------------------------------------*/
/* 单个采样样本: 红光/红外各18bit原始值 */
typedef struct {
    uint32_t red;               /* 红光通道18bit原始值 */
    uint32_t ir;                /* 红外通道18bit原始值 */
} Max30102_Sample_t;

/* Function Prototypes -------------------------------------------------------*/
bool max30102_init(void);
uint8_t max30102_read_fifo(Max30102_Sample_t p_samples[], uint8_t max_num);

#endif /* MAX30102_H */
