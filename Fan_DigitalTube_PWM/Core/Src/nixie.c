/***********************************************************************************
 * @file      nixie.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.0.0
 * @date      2026-09-28
 * @brief     74HC595 菊花链四位共阳数码管驱动(SPI2 + PB12 软件锁存)
 *
 *            数据流: SPI 发 2 字节 [段码, 位选] → U2 锁存段选 → U6 锁存位选,
 *            PB12 上升沿同时锁存两级 595。位扫描由外部定时节拍调用 nixie_refresh。
 *
 * @history   V1.0.0 首版: 帧缓冲+字节序[段,位]+锁存沿+共阳段码表         ---2026-09-28
 **********************************************************************************/
#include "nixie.h"
#include "main.h"                           /* Error_Handler 声明            */

/* ==========================================================================
 * 私有变量
 * ========================================================================== */
static SPI_HandleTypeDef s_hspi2 = {0};     /* SPI2 句柄                    */

static uint8_t s_seg_buf[NIXIE_DIGIT_NUM] = {0};    /* 各位段码帧缓冲       */
static uint8_t s_cur_digit = 0U;            /* 当前扫描位索引 0~3           */

/* 共阳段码表(低电平点亮): 索引 0~9 数字, 10=灭 */
static const uint8_t sc_seg_digit[11] =
{
    0xC0U,  /* 0: abcdef 亮             */
    0xF9U,  /* 1: bc                    */
    0xA4U,  /* 2: abdeg                 */
    0xB0U,  /* 3: abcdg                 */
    0x99U,  /* 4: bcfg                  */
    0x92U,  /* 5: acdfg                 */
    0x82U,  /* 6: acdefg                */
    0xF8U,  /* 7: abc                   */
    0x80U,  /* 8: 全亮                  */
    0x90U,  /* 9: abcdfg                */
    0xFFU   /* 10: 全灭(消隐)           */
};

/* 位选表(经 U6 输出): 原理图证实 U6.QA~QD 经 100R 驱动 MMBT5551 PNP(Q4~Q7),
   PNP 低电平导通给位 COM 供电 → 位选低有效(FS_MP1A_Extend_v3.3 p2 文字层证据) */
static const uint8_t sc_digit_sel[NIXIE_DIGIT_NUM] =
{
    0xFEU,  /* G1: 第 1 位              */
    0xFDU,  /* G2: 第 2 位              */
    0xFBU,  /* G3: 第 3 位              */
    0xF7U   /* G4: 第 4 位              */
};

/*************************************
 * 函数名称 ： nixie_latch_pulse
 * 描述     ： PB12 锁存沿: 拉高保持 NLLIE_LATCH_DELAY_US 后拉低
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
static void nixie_latch_pulse(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    /* 100kHz 计数环路下 2us≈数十 NOP, 用短循环保沿宽 */
    for (volatile uint32_t i = 0U; i < 100U; i++)
    {
        __NOP();
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
}

/*************************************
 * 函数名称 ： nixie_init
 * 描述     ： SPI2 主模式初始化(1.25MHz, 8bit, MSB) + PB12 锁存脚
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void nixie_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB13=SCK, PB15=MOSI: AF5=SPI2 (DS13737 p.127) */
    gpio_init.Pin       = GPIO_PIN_13 | GPIO_PIN_15;
    gpio_init.Mode      = GPIO_MODE_AF_PP;
    gpio_init.Pull      = GPIO_NOPULL;
    gpio_init.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio_init.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* PB12=RCLK 锁存: 普通推挽输出, 默认低 */
    gpio_init.Pin       = GPIO_PIN_12;
    gpio_init.Mode      = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull      = GPIO_NOPULL;
    gpio_init.Speed     = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio_init);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);

    /* SPI2 主模式: 软件片选(SSI/SSM), 8bit MSB, 1.25MHz */
    s_hspi2.Instance               = SPI2;
    s_hspi2.Init.Mode              = SPI_MODE_MASTER;
    s_hspi2.Init.Direction         = SPI_DIRECTION_2LINES_TXONLY;
    s_hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    s_hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;
    s_hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;
    s_hspi2.Init.NSS               = SPI_NSS_SOFT;
    s_hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
    s_hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    s_hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    s_hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    if (HAL_SPI_Init(&s_hspi2) != HAL_OK)
    {
        Error_Handler();
    }

    /* 帧缓冲清零(全灭), 消隐位选先发一轮防止上电鬼影 */
    for (uint8_t i = 0U; i < NIXIE_DIGIT_NUM; i++)
    {
        s_seg_buf[i] = sc_seg_digit[10];
    }
}

/*************************************
 * 函数名称 ： nixie_set_digit
 * 描述     ： 设置指定位的原始段码(帧缓冲, 下次 refresh 时输出)
 * 输入     ： index - 位索引 0~3(左→右)
 *           ： seg_pattern - 共阳段码(低电平亮的笔段为 0)
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void nixie_set_digit(uint8_t index, uint8_t seg_pattern)
{
    if (index < NIXIE_DIGIT_NUM)
    {
        s_seg_buf[index] = seg_pattern;
    }
}

/*************************************
 * 函数名称 ： nixie_show_number
 * 描述     ： 四位显示十进制数 0~9999, 前导零消隐
 * 输入     ： value - 0~9999
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void nixie_show_number(uint16_t value)
{
    uint8_t digits[NIXIE_DIGIT_NUM];
    uint8_t lead = 1U;                      /* 前导零消隐标志               */

    if (value > 9999U)
    {
        value = 9999U;
    }

    digits[0] = (uint8_t)(value / 1000U);
    digits[1] = (uint8_t)((value / 100U) % 10U);
    digits[2] = (uint8_t)((value / 10U) % 10U);
    digits[3] = (uint8_t)(value % 10U);

    for (uint8_t i = 0U; i < NIXIE_DIGIT_NUM; i++)
    {
        if ((lead != 0U) && (digits[i] == 0U) && (i < (NIXIE_DIGIT_NUM - 1U)))
        {
            s_seg_buf[i] = sc_seg_digit[10];        /* 前导零消隐           */
        }
        else
        {
            lead = 0U;
            s_seg_buf[i] = sc_seg_digit[digits[i]];
        }
    }
}

/*************************************
 * 函数名称 ： nixie_refresh
 * 描述     ： 点亮当前扫描位: 发 [段码, 位选] 2 字节 + 锁存沿
 *           ： 由外部按 2~5ms 节拍轮询调用, 每次推进一位
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void nixie_refresh(void)
{
    uint8_t frame[2];

    frame[0] = s_seg_buf[s_cur_digit];      /* 第 1 字节: 段码(先入 U2 近端) */
    frame[1] = sc_digit_sel[s_cur_digit];   /* 第 2 字节: 位选(后入 U6 远端) */

    HAL_SPI_Transmit(&s_hspi2, frame, 2U, 10U);
    nixie_latch_pulse();

    s_cur_digit = (uint8_t)((s_cur_digit + 1U) % NIXIE_DIGIT_NUM);
}
