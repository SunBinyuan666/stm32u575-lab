/***********************************************************************************
 * @file      main.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.1.0
 * @date      2026-09-25
 * @brief     LED_KEY_EXTI demo: 四路按键EXTI中断(标志位)+主循环消抖+USART1计数打印
 *            KEY1(PC9)→翻转LED1; KEY2(PC8)→翻转LED2; KEY3(PC5)→翻转LED2;
 *            USER(PA12)→仅计数. 每次有效按键打印各自计数值.
 *
 * @history   V1.0.0 首版: KEY1/KEY2/USER三键                      ---2026-09-24
 *            V1.1.0 增加KEY3(PC5); 清除调试代码, 恢复正式结构     ---2026-09-25
 **********************************************************************************/
#include "main.h"
#include <stdio.h>
#include "gpio.h"
#include "icache.h"
#include "usart.h"

/* USER CODE BEGIN PV */
/* 按键标志位: ISR置位, 主循环消费后清零 */
volatile uint8_t g_key1_flag = 0;
volatile uint8_t g_key2_flag = 0;
volatile uint8_t g_key3_flag = 0;
volatile uint8_t g_user_flag = 0;

/* 各键有效按键计数 */
static uint16_t s_key1_cnt = 0;
static uint16_t s_key2_cnt = 0;
static uint16_t s_key3_cnt = 0;
static uint16_t s_user_cnt = 0;
/* USER CODE END PV */

#define KEY_DEBOUNCE_MS 10U                     /* 按键消抖延时 */

static void SystemClock_Config(void);
static void SystemPower_Config(void);

/*************************************
 * 函数名称 ： led1_toggle
 * 描述     ： LED1翻转动作(供按键状态机回调)
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
static void led1_toggle(void) { LED1_TOGGLE(); }

/*************************************
 * 函数名称 ： led2_toggle
 * 描述     ： LED2翻转动作(供按键状态机回调)
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
static void led2_toggle(void) { LED2_TOGGLE(); }

/*************************************
 * 函数名称 ： key_state_machine
 * 描述     ： 单个按键的消抖状态机: 标志触发→延时10ms→确认→计数/执行动作
 * 输入     ： flag - 按键标志指针(消费后清零); cnt - 计数指针
 *           action - 确认按下后执行的动作(NULL=不动作)
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
static void key_state_machine(volatile uint8_t *flag, uint16_t *cnt,
                              void (*action)(void))
{
    if (*flag == 1U)
    {
        *flag = 0;
        HAL_Delay(KEY_DEBOUNCE_MS);             /* 消抖: 延时后消费标志 */
        if (action != NULL)
        {
            action();
        }
        (*cnt)++;
        printf("cnt=%u\r\n", *cnt);
    }
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();

  SystemClock_Config();
  SystemPower_Config();

  MX_GPIO_Init();
  MX_ICACHE_Init();

  uart1_init(115200);

  printf("\r\n=== LED_KEY_EXTI demo, SYSCLK=%lu MHz ===\r\n",
         HAL_RCC_GetSysClockFreq() / 1000000U);

  while (1)
  {
    key_state_machine(&g_key1_flag, &s_key1_cnt, led1_toggle);
    key_state_machine(&g_key2_flag, &s_key2_cnt, led2_toggle);
    key_state_machine(&g_key3_flag, &s_key3_cnt, led2_toggle);
    key_state_machine(&g_user_flag, &s_user_cnt, NULL);     /* USER只计数 */
  }
}

/**
  * @brief System Clock Configuration
  * @note  HSE=12MHz(华清核心板实焊), PLL M=3/N=40/P=1 → SYSCLK=160MHz
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
  RCC_OscInitStruct.PLL.PLLM = 3;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 1;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_0;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Power Configure
  * @retval None
  */
static void SystemPower_Config(void)
{
  HAL_PWREx_EnableVddA();
}

/**
  * @brief  Function executed in case of error detected in the application.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
