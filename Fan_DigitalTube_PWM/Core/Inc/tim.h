/***********************************************************************************
 * @file      tim.h
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V2.0.0
 * @date      2026-09-28
 * @brief     TIM3 CH1(PC6)=风扇开关量; TIM4 CH3(PB8)=三角渐变波形输出
 *
 * @history   V2.1.0 渐变波形迁移 TIM4_CH3/PB8(空脚), PC7 恒低     ---2026-09-28
 *            V2.0.0 方案Z重构: 风扇开关量+CH2渐变回归               ---2026-09-28
 *            V1.1.0 CH2镜像+kick-start+uint16溢出修复              ---2026-09-28
 *            V1.0.0 首版: TIM3 双通道 25kHz PWM, 挡位限幅          ---2026-09-28
 **********************************************************************************/
#ifndef __TIM_H
#define __TIM_H

#include "stm32u5xx_hal.h"

/* ==========================================================================
 * 参数宏定义
 * ========================================================================== */
#define TIM_PWM_FREQ_HZ         10U     /* PWM 载波 10Hz: 实验观察用, 方波脉宽  */
                                        /*  呼吸肉眼可见(空脚输出, 无负载顾虑)  */
#define TIM_PWM_PSC             1599U   /* 160MHz/(1599+1)=100kHz 计数时钟      */
#define TIM_PWM_CLK_HZ          100000U /* TIM 计数时钟 = 160MHz/(PSC+1)        */
#define TIM_PWM_ARR             (TIM_PWM_CLK_HZ / TIM_PWM_FREQ_HZ)  /* 6400     */
#define FAN_DUTY_ON             100U    /* 风扇运行占空比: 两线风扇只支持全速    */
#define FAN_KICK_MS             500U    /* 开风扇踢启动时长(ms, 100% 冲刺)       */
#define SWEEP_DUTY_STEP         1U      /* 渐变波形每步占空比步进(%)             */
#define SWEEP_PERIOD_TICKS      100U    /* 渐变步进间隔 tick 数(1ms*100=100ms)   */
                                        /*  上升 10s, 完整三角往返 20s           */

/* ==========================================================================
 * 函数声明
 * ========================================================================== */
void tim3_pwm_init(void);
void fan_set_on(uint8_t on);
void fan_process(void);
uint8_t fan_get_on(void);
void sweep_set_run(uint8_t run);
void sweep_process(void);
uint8_t sweep_get_run(void);
uint8_t sweep_get_duty(void);

#endif /* __TIM_H */
