/***********************************************************************************
 * @file      sht20.h
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.1.0
 * @date      2026-09-27
 * @brief     SHT20温湿度传感器对外接口（hold master模式, 兼容SI7006）
 *
 * @history   V1.1.0 按WT-WI-PE-200 B1规范重构接口              ---2026-09-27
 *            V1.0.0 首版: 软复位+温度湿度采集                  ---2026-09-27
 **********************************************************************************/
#ifndef SHT20_H
#define SHT20_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "main.h"

/* Function Prototypes -------------------------------------------------------*/
bool sht20_init(void);
bool sht20_read_temp_hum(float *p_temp, float *p_hum);

#endif /* SHT20_H */
