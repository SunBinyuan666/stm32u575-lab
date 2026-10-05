/***********************************************************************************
 * @file      tim.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V2.0.0
 * @date      2026-09-28
 * @brief     TIM3 CH1(PC6)=风扇开关量; TIM4 CH3(PB8)=三角渐变波形
 *
 *            方案 Z: 风扇实测两线无刷, 不支持 PWM 调速, 改为开关量(全速+kick 启动);
 *            PWM 学习需求由 TIM4_CH3/PB8(底板 J6.23 CAN-TX 空脚)承担,
 *            PC7(J6.9) 直连震动马达, 恒低保持安静, 不再输出波形。
 *
 * @history   V2.1.0 渐变波形迁移 TIM3_CH2->TIM4_CH3(PB8), PC7 恒低  ---2026-09-28
 *            V2.0.0 方案Z重构: 风扇开关量+CH2渐变回归               ---2026-09-28
 *            V1.1.0 CH2镜像+kick-start+uint16溢出修复              ---2026-09-28
 *            V1.0.0 首版: TIM3 双通道 25kHz PWM, 挡位限幅          ---2026-09-28
 **********************************************************************************/
#include "tim.h"
#include "main.h"                           /* Error_Handler 声明            */

/* ==========================================================================
 * 私有变量
 * ========================================================================== */
static TIM_HandleTypeDef s_htim3 = {0};     /* TIM3 句柄: 风扇 CH1           */
static TIM_HandleTypeDef s_htim4 = {0};     /* TIM4 句柄: 渐变波形 CH3       */

static uint8_t  s_fan_on    = 0U;           /* 风扇开关状态: 1=运行 0=停止   */
static uint16_t s_fan_kick  = 0U;           /* 踢启动剩余时间(ms)           */

static uint8_t  s_sweep_run  = 0U;          /* 渐变运行: 1=跑 0=停          */
static uint8_t  s_sweep_duty = 0U;          /* 渐变当前占空比(%)            */
static int8_t   s_sweep_dir  = 1;           /* 渐变方向: 1=增 -1=减         */
static uint16_t s_sweep_tick = 0U;          /* 渐变步进节拍计数             */

/*************************************
 * 函数名称 ： tim3_pwm_set_ch1
 * 描述     ： 内部写 CH1 占空比(Stop→SET_COMPARE→Start 保相位干净)
 * 输入     ： duty_percent - 占空比 0~100(%)
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
static void tim3_pwm_set_ch1(uint8_t duty_percent)
{
    HAL_TIM_PWM_Stop(&s_htim3, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(&s_htim3, TIM_CHANNEL_1,
                          (uint32_t)duty_percent * (TIM_PWM_ARR / 100U));
    __HAL_TIM_SET_COUNTER(&s_htim3, 0U);
    HAL_TIM_PWM_Start(&s_htim3, TIM_CHANNEL_1);
}

/*************************************
 * 函数名称 ： tim3_pwm_set_ch2
 * 描述     ： PC7(J6.9 MOTOR)恒低配置: 直连震动马达,
 *           ： 固件不在此脚输出波形, 防止马达误动作
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
static void tim3_pwm_set_ch2_low(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    HAL_TIM_PWM_Stop(&s_htim3, TIM_CHANNEL_2);

    /* PC7 切回普通推挽输出并置低(AF 解除) */
    gpio_init.Pin       = GPIO_PIN_7;
    gpio_init.Mode      = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull      = GPIO_NOPULL;
    gpio_init.Speed     = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio_init);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
}

/*************************************
 * 函数名称 ： tim4_sweep_set_ch3
 * 描述     ： 写渐变波形占空比到 TIM4_CH3(PB8, J6.23 空脚)
 * 输入     ： duty_percent - 占空比 0~100(%)
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
static void tim4_sweep_set_ch3(uint8_t duty_percent)
{
    __HAL_TIM_SET_COMPARE(&s_htim4, TIM_CHANNEL_3,
                          (uint32_t)duty_percent * (TIM_PWM_ARR / 100U));
}

/*************************************
 * 函数名称 ： tim3_pwm_init
 * 描述     ： TIM3 双通道 PWM 初始化
 *           ： CH1=PC6(风扇) CH2=PC7(渐变波形)
 *           ： 160MHz/6400 = 25kHz, 分辨率 1/6400
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void tim3_pwm_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};
    TIM_OC_InitTypeDef oc_init = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* --- TIM3: PC6 复用推挽 AF2=TIM3_CH1(风扇); PC7 先按 AF 配, 稍后恒低 --- */
    gpio_init.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    gpio_init.Mode      = GPIO_MODE_AF_PP;
    gpio_init.Pull      = GPIO_NOPULL;
    gpio_init.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio_init.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOC, &gpio_init);

    /* --- TIM4: PB8 复用推挽 AF2=TIM4_CH3(渐变波形, J6.23 空脚) --- */
    gpio_init.Pin       = GPIO_PIN_8;
    gpio_init.Mode      = GPIO_MODE_AF_PP;
    gpio_init.Pull      = GPIO_NOPULL;
    gpio_init.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio_init.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* 时基: PSC=1599 → 100kHz 计数, ARR=10000-1 → 10Hz */
    s_htim3.Instance               = TIM3;
    s_htim3.Init.Prescaler         = TIM_PWM_PSC;
    s_htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    s_htim3.Init.Period            = TIM_PWM_ARR - 1U;
    s_htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    s_htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&s_htim3) != HAL_OK)
    {
        Error_Handler();
    }

    /* --- TIM4 时基: 与 TIM3 同参数(独立计数器, 渐变波形专用) --- */
    s_htim4.Instance               = TIM4;
    s_htim4.Init                   = s_htim3.Init;
    if (HAL_TIM_PWM_Init(&s_htim4) != HAL_OK)
    {
        Error_Handler();
    }

    /* --- 两定时器通道配置: PWM1 模式 --- */
    oc_init.OCMode     = TIM_OCMODE_PWM1;
    oc_init.Pulse      = 0U;
    oc_init.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc_init.OCFastMode = TIM_OCFAST_DISABLE;
    if ((HAL_TIM_PWM_ConfigChannel(&s_htim3, &oc_init, TIM_CHANNEL_1) != HAL_OK) ||
        (HAL_TIM_PWM_ConfigChannel(&s_htim3, &oc_init, TIM_CHANNEL_2) != HAL_OK) ||
        (HAL_TIM_PWM_ConfigChannel(&s_htim4, &oc_init, TIM_CHANNEL_3) != HAL_OK))
    {
        Error_Handler();
    }

    HAL_TIM_PWM_Start(&s_htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&s_htim4, TIM_CHANNEL_3);

    /* 初始状态: 风扇关(CH1=0), PC7 恒低(马达静默), 渐变停(PB8=0) */
    tim3_pwm_set_ch1(0U);
    tim3_pwm_set_ch2_low();
    tim4_sweep_set_ch3(0U);
}

/*************************************
 * 函数名称 ： fan_set_on
 * 描述     ： 风扇开关控制; 开=100%全速(先 kick 500ms, 两线风扇实测
 *           ： 仅支持全速运行, 见 PLAN.md 硬件结论节), 关=立即停
 * 输入     ： on - 1=开 0=关
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void fan_set_on(uint8_t on)
{
    if (on != 0U)
    {
        s_fan_on   = 1U;
        s_fan_kick = FAN_KICK_MS;           /* 踢启动: 保证克服静摩擦       */
        tim3_pwm_set_ch1(FAN_DUTY_ON);
    }
    else
    {
        s_fan_on   = 0U;
        s_fan_kick = 0U;
        tim3_pwm_set_ch1(0U);
    }
}

/*************************************
 * 函数名称 ： fan_process
 * 描述     ： 风扇踢启动时序处理, 主循环每 1ms 调一次
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void fan_process(void)
{
    if ((s_fan_on != 0U) && (s_fan_kick != 0U))
    {
        s_fan_kick--;
        /* 踢启动结束即回落到运行占空比(当前=100%, 预留未来降速可能) */
        if (s_fan_kick == 0U)
        {
            tim3_pwm_set_ch1(FAN_DUTY_ON);
        }
    }
}

/*************************************
 * 函数名称 ： fan_get_on
 * 描述     ： 读风扇开关状态
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 1=运行 0=停止
 **************************************/
uint8_t fan_get_on(void)
{
    return s_fan_on;
}

/*************************************
 * 函数名称 ： sweep_set_run
 * 描述     ： 渐变波形启停; 停止时 CH2 立即归零
 * 输入     ： run - 1=运行 0=停止
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void sweep_set_run(uint8_t run)
{
    s_sweep_run = (run != 0U) ? 1U : 0U;
    if (s_sweep_run == 0U)
    {
        s_sweep_duty = 0U;
        s_sweep_dir  = 1;
        s_sweep_tick = 0U;
        tim4_sweep_set_ch3(0U);
    }
}

/*************************************
 * 函数名称 ： sweep_process
 * 描述     ： CH2 三角渐变引擎, 主循环每 1ms 调一次,
 *           ： 每 SWEEP_PERIOD_TICKS(20ms)步进 1%, 全程 0→100→0 约 4s
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void sweep_process(void)
{
    if (s_sweep_run == 0U)
    {
        return;
    }

    s_sweep_tick++;
    if (s_sweep_tick < SWEEP_PERIOD_TICKS)
    {
        return;
    }
    s_sweep_tick = 0U;

    /* 端点掉头, 其余步进(0/100 处不越界防回绕) */
    if ((s_sweep_dir > 0) && (s_sweep_duty >= 100U))
    {
        s_sweep_dir = -1;
    }
    else if ((s_sweep_dir < 0) && (s_sweep_duty == 0U))
    {
        s_sweep_dir = 1;
    }
    else
    {
        s_sweep_duty = (uint8_t)(s_sweep_duty + s_sweep_dir);
    }
    tim4_sweep_set_ch3(s_sweep_duty);
}

/*************************************
 * 函数名称 ： sweep_set_run
 * 描述     ： 读渐变波形运行状态
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 1=运行 0=停止
 **************************************/
uint8_t sweep_get_run(void)
{
    return s_sweep_run;
}

/*************************************
 * 函数名称 ： sweep_get_duty
 * 描述     ： 读渐变当前占空比(数码管显示用)
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 当前占空比 0~100(%)
 **************************************/
uint8_t sweep_get_duty(void)
{
    return s_sweep_duty;
}
