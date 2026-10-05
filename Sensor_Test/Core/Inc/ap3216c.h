/***********************************************************************************
 * @file      ap3216c.h
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.1.1
 * @date      2026-09-27
 * @brief     AP3216C环境光/接近/红外三合一传感器对外接口
 *            寄存器定义属内部实现, 不对外暴露(信息隐藏)
 *
 * @history   V1.1.1 修复PS/IR数据拼位错误                         ---2026-09-27
 *            V1.1.0 按WT-WI-PE-200 B1规范重构接口                 ---2026-09-27
 *            V1.0.0 首版: 软复位+三通道读取                    ---2026-09-27
 **********************************************************************************/
#ifndef AP3216C_H
#define AP3216C_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "main.h"

/* Types ---------------------------------------------------------------------*/
/* 三通道原始数据(PS/IR 10bit, ALS 16bit) */
typedef struct {
    uint16_t als_raw;       /* 环境光原始值, 乘0.35=lux(默认量程20661lux) */
    uint16_t ps_raw;        /* 接近10bit原始值 */
    uint16_t ir_raw;        /* 红外10bit原始值 */
    bool     object_near;   /* true=物体靠近(PS OBJ标志) */
} Ap3216c_Data_t;

/* Function Prototypes -------------------------------------------------------*/
bool ap3216c_init(void);
bool ap3216c_read_data(Ap3216c_Data_t *p_data);

#endif /* AP3216C_H */
