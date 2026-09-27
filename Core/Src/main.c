/* USER CODE BEGIN Header */
/* 注: 文件信息见顶部公司规范文件头 (WT-WI-PE-200 B1) */
/* USER CODE END Header */
/*******************************************************************************
 * @file      main.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.2.0
 * @date      2026-09-27
 * @brief     扩展板三传感器采集主程序（SHT20/AP3216C/MAX30102, I2C1）
 *
 * @history   V1.2.0 FreeRTOS迁移(采集/打印双任务+消息队列)            ---2026-09-27
 *            V1.1.0 按WT-WI-PE-200 B1规范重构(函数头/宏常量/循环拆分)  ---2026-09-27
 *            V1.0.0 首版: I2C扫描+三传感器轮询采集                   ---2026-09-27
 *******************************************************************************
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "icache.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "usart.h"
#include "i2c.h"
#include "sht20.h"
#include "ap3216c.h"
#include "max30102.h"
#include "app_freertos.h"
#include "cmsis_os2.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define I2C_SCAN_ADDR_MIN       0x08U   /* 7位地址扫描下限(跳过保留段) */
#define I2C_SCAN_ADDR_MAX       0x78U   /* 7位地址扫描上限(排除保留段) */
#define I2C_SCAN_PROBE_TRIALS   2U      /* IsDeviceReady探测次数 */
#define I2C_SCAN_TIMEOUT_MS     50U     /* 单地址探测超时 */
#define MAIN_LOOP_PERIOD_MS     2000U   /* 传感器轮询周期 */
#define AP3216C_LUX_PER_COUNT_PCT 35U   /* 默认量程0.35lux/count, 百分数避开浮点 */
#define PLL_M_DIV               3U      /* VCO输入=12MHz/3=4MHz(PLLRGE_0档) */
#define PLL_N_MUL               40U     /* VCO输出=4MHz*40=160MHz */
#define PLL_P_DIV               2U      /* SYSCLK=160MHz/2... 见时钟方案 */
#define PLL_Q_DIV               2U      /* PLLQ=80MHz(USB/RNG等) */
#define PLL_R_DIV               1U      /* PLLR=160MHz(SYSCLK) */
#define PLL_FRACN               0U      /* 无小数分频 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void i2c_bus_scan(void);
static void sensors_init(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*************************************
 * 函数名称 ： i2c_bus_scan
 * 描述     ： 扫描I2C1总线全部7位地址, 打印应答器件
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
static void i2c_bus_scan(void)
{
    uint8_t addr;
    uint8_t found = 0;

    printf("I2C bus scan (7-bit addresses):\r\n");
    for (addr = I2C_SCAN_ADDR_MIN; addr < I2C_SCAN_ADDR_MAX; addr++)
    {
        if (HAL_I2C_IsDeviceReady(&g_hi2c1, (uint16_t)(addr << 1),
                                  I2C_SCAN_PROBE_TRIALS,
                                  I2C_SCAN_TIMEOUT_MS) == HAL_OK)
        {
            printf("  device found: 0x%02X\r\n", addr);
            found++;
        }
    }
    if (found == 0U)
    {
        printf("  no device found (check wiring/extension board power)\r\n");
    }
}

/*************************************
 * 函数名称 ： sensors_init
 * 描述     ： 三个传感器逐一初始化并打印结果
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
static void sensors_init(void)
{
    if (sht20_init())
    {
        printf("SHT20     init OK\r\n");
    }
    else
    {
        printf("SHT20     NO RESPONSE (0x40)\r\n");
    }

    if (ap3216c_init())
    {
        printf("AP3216C   init OK\r\n");
    }
    else
    {
        printf("AP3216C   NO RESPONSE (0x1E)\r\n");
    }

    if (max30102_init())
    {
        printf("MAX30102  init OK (PART_ID=0x15)\r\n");
    }
    else
    {
        printf("MAX30102  NO RESPONSE (0x57)\r\n");
    }
}

/*************************************
 * 函数名称 ： sensors_poll
 * 描述     ： 三个传感器轮询采集一次并打印
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
static void sensors_poll(void)
{
    float temp = 0.0F;
    float hum = 0.0F;
    unsigned int als_lux = 0U;
    Ap3216c_Data_t ap_data = {0};
    Max30102_Sample_t mx_samples[MAX30102_FIFO_DEPTH] = {{0}};
    uint8_t mx_count;

    /* SHT20: 温湿度 */
    if (sht20_read_temp_hum(&temp, &hum))
    {
        printf("[SHT20]    T=%.2f C, RH=%.2f %%\r\n", temp, hum);
    }
    else
    {
        printf("[SHT20]    read error\r\n");
    }

    /* AP3216C: 光感/接近/红外 */
    if (ap3216c_read_data(&ap_data))
    {
        als_lux = (unsigned int)(ap_data.als_raw
                                 * AP3216C_LUX_PER_COUNT_PCT / 100U);
        printf("[AP3216C]  ALS=%u (lux=%u), PS=%u, IR=%u, OBJ=%d\r\n",
               ap_data.als_raw, als_lux,
               ap_data.ps_raw, ap_data.ir_raw,
               (int)ap_data.object_near);
    }
    else
    {
        printf("[AP3216C]  read error\r\n");
    }

    /* MAX30102: 排空FIFO打印最新样本 */
    mx_count = max30102_read_fifo(mx_samples, MAX30102_FIFO_DEPTH);
    if (mx_count > 0U)
    {
        printf("[MAX30102] %u samples, last Red=%lu IR=%lu\r\n",
               mx_count,
               mx_samples[mx_count - 1U].red,
               mx_samples[mx_count - 1U].ir);
    }
}
/* USER CODE END 0 */
/*************************************
 * 函数名称 ： main
 * 描述     ： 系统入口: 时钟/串口/I2C初始化, 传感器轮询采集
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： int(实际不可达)
 **************************************/
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* 复位全部外设, 初始化Flash接口与SysTick */
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
  usart1_init();
  printf("=== HQYJ U575 Sensor_Test V1.2.0 (FreeRTOS)\r\n");
  printf("\r\n");
  printf("[INFO] SYSCLK = %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
  printf("[INFO] Scheduler: FreeRTOS V10.4.6, tick 1000Hz\r\n");
  printf("[INFO] Tasks: sensorTask(1s poll) + printTask(queue)\r\n");
  printf("\r\n");
  printf("[READ ME] ---- Output value units ----\r\n");
  printf("  SHT20   : T = temperature (deg C), RH = relative humidity (%%)\r\n");
  printf("  AP3216C : ALS = ambient light raw (16bit), lux = ALS x 0.35\r\n");
  printf("            PS  = proximity raw (10bit), closer = bigger\r\n");
  printf("            IR  = infrared raw (10bit)\r\n");
  printf("            OBJ = object near flag (1 = object close)\r\n");
  printf("  MAX30102: Red/IR = 18bit optical raw (finger on = value up)\r\n");
  printf("            samples = FIFO drained this round\r\n");
  printf("\r\n");

  i2c1_init();
  i2c_bus_scan();
  sensors_init();

  /* Init scheduler */
  osKernelInitialize();  /* 初始化内核, RTOS对象在app_freertos.c创建 */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();
  /* 调度器接管后永不返回, 到这里说明启动失败 */
  printf("ERROR: scheduler returned!\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 调度器已接管, 正常情况到不了这里 */
    __disable_irq();
    for (;;)
    {
    }
  }
  /* USER CODE END 3 */
}

/*************************************
 * 函数名称 ： SystemClock_Config
 * 描述     ： 系统时钟配置: HSE 12MHz -> PLL(M=3,N=40,R=1) -> SYSCLK 160MHz
 *            电压档SCALE1(160MHz上限), AHB/APB1/2/3全DIV1
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
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
  RCC_OscInitStruct.PLL.PLLM = PLL_M_DIV;
  RCC_OscInitStruct.PLL.PLLN = PLL_N_MUL;
  RCC_OscInitStruct.PLL.PLLP = PLL_P_DIV;
  RCC_OscInitStruct.PLL.PLLQ = PLL_Q_DIV;
  RCC_OscInitStruct.PLL.PLLR = PLL_R_DIV;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_0;
  RCC_OscInitStruct.PLL.PLLFRACN = PLL_FRACN;
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
/*************************************
 * 函数名称 ： HAL_TIM_PeriodElapsedCallback
 * 描述     ： TIM时基周期中断回调(TIM6), 累加HAL系统滴答
 *            (FreeRTOS占用SysTick后, HAL时基由TIM6承担)
 * 输入     ： htim - 定时器句柄
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        HAL_IncTick();
    }
}
/* USER CODE END 4 */

/*************************************
 * 函数名称 ： Error_Handler
 * 描述     ： 错误处理: 关中断并死循环(便于调试器定位)
 * 输入     ： 无
 * 输出     ： 无
 * 返回     ： 无
 **************************************/
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
  /* 用户可在此打印断言失败的文件名与行号 */
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
