/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "icache.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tim.h"
#include "nixie.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define KEY_SAMPLE_MS           20U     /* 按键采样周期(ms)                      */
#define KEY_DEBOUNCE_COUNT      3U      /* 连续采样次数=有效(3*20=60ms)          */
#define NIXIE_SCAN_MS           3U      /* 数码管位扫描周期(ms), 4位约12ms/帧    */
#define DISPLAY_UPDATE_MS       100U    /* 显示内容刷新周期(ms)                  */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t  s_key1_cnt = 0U;            /* KEY1 消抖计数                 */
static uint8_t  s_key2_cnt = 0U;            /* KEY2 消抖计数                 */
static uint8_t  s_key3_cnt = 0U;            /* KEY3 消抖计数                 */
static uint8_t  s_key1_last = 0U;           /* KEY1 上次有效状态(边沿检测)   */
static uint8_t  s_key2_last = 0U;
static uint8_t  s_key3_last = 0U;
static uint32_t s_last_key_tick = 0U;       /* 上次按键采样时刻              */
static uint32_t s_last_scan_tick = 0U;      /* 上次位扫描时刻                */
static uint32_t s_last_disp_tick = 0U;      /* 上次显示内容刷新时刻          */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void key_process(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ICACHE_Init();
  /* USER CODE BEGIN 2 */
  key_gpio_init();                          /* KEY1~3 输入(PC9/PC8/PC5)      */
  tim3_pwm_init();                          /* TIM3 CH1=风扇, TIM4 CH3=渐变  */
  nixie_init();                             /* SPI2 + 595 数码管             */
  nixie_show_number(0U);                    /* 上电显示 0                    */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 1ms 节拍任务: 踢启动时序 + 渐变引擎 */
    fan_process();
    sweep_process();

    /* 3ms 节拍任务: 数码管位扫描(轮到哪位亮哪位) */
    if ((HAL_GetTick() - s_last_scan_tick) >= NIXIE_SCAN_MS)
    {
      s_last_scan_tick = HAL_GetTick();
      nixie_refresh();
    }

    /* 100ms 节拍任务: 刷新显示内容 = 当前渐变占空比(0~100) */
    if ((HAL_GetTick() - s_last_disp_tick) >= DISPLAY_UPDATE_MS)
    {
      s_last_disp_tick = HAL_GetTick();
      nixie_show_number((uint16_t)sweep_get_duty());
    }

    /* 20ms 节拍任务: 按键采样消抖 */
    if ((HAL_GetTick() - s_last_key_tick) >= KEY_SAMPLE_MS)
    {
      s_last_key_tick = HAL_GetTick();
      key_process();
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/*************************************
 * 函数名称 ： key_process
 * 描述     ： 按键 20ms 采样消抖状态机, 连续 3 次同电平=有效,
 *           ： 下降沿触发动作(方案Z: KEY1开风扇 KEY2关风扇 KEY3启停渐变)
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
static void key_process(void)
{
  /* --- KEY1(PC9): 开风扇 --- */
  if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9) == GPIO_PIN_RESET)
  {
    if (s_key1_cnt < KEY_DEBOUNCE_COUNT)
    {
      s_key1_cnt++;
    }
    else if (s_key1_last == 0U)
    {
      s_key1_last = 1U;
      fan_set_on(1U);
    }
  }
  else
  {
    s_key1_cnt  = 0U;
    s_key1_last = 0U;
  }

  /* --- KEY2(PC8): 关风扇 --- */
  if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_8) == GPIO_PIN_RESET)
  {
    if (s_key2_cnt < KEY_DEBOUNCE_COUNT)
    {
      s_key2_cnt++;
    }
    else if (s_key2_last == 0U)
    {
      s_key2_last = 1U;
      fan_set_on(0U);
    }
  }
  else
  {
    s_key2_cnt  = 0U;
    s_key2_last = 0U;
  }

  /* --- KEY3(PC5): 渐变波形启停切换 --- */
  if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_5) == GPIO_PIN_RESET)
  {
    if (s_key3_cnt < KEY_DEBOUNCE_COUNT)
    {
      s_key3_cnt++;
    }
    else if (s_key3_last == 0U)
    {
      s_key3_last = 1U;
      sweep_set_run((sweep_get_run() != 0U) ? 0U : 1U);
    }
  }
  else
  {
    s_key3_cnt  = 0U;
    s_key3_last = 0U;
  }
}

/**
  * @brief System Clock Configuration
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

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
  RCC_OscInitStruct.PLL.PLLM = 3;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = 2;
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

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
