/***********************************************************************************
 * @file      app_freertos.c
 * @author    Sunbinyuan（binyuan.sun@welltest.cn）
 * @version   V1.2.0
 * @date      2026-09-27
 * @brief     FreeRTOS应用层: 传感器采集任务 + 打印任务(队列守门人)
 *            采集任务1s一轮询三传感器, 数据经队列发打印任务串行输出
 *
 * @history   V1.2.0 传感器采集RTOS化(队列+双任务)                ---2026-09-27
 ***********************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
#include "sht20.h"
#include "ap3216c.h"
#include "max30102.h"

/* Private defines -----------------------------------------------------------*/
#define SENSOR_POLL_PERIOD_MS   1000U   /* 采集任务周期 */
#define PRINT_QUEUE_LEN         8U      /* 打印队列深度(条) */
#define PRINT_MSG_MAX           128U    /* 单条打印消息最大长度 */
#define AP3216C_LUX_PER_COUNT_PCT 35U   /* 0.35lux/count as percent */
#define SENSOR_TASK_STACK       512U    /* 采集任务栈(字节, CMSIS单位) */
#define PRINT_TASK_STACK        512U    /* 打印任务栈(字节) */

/* Private variables ---------------------------------------------------------*/
osThreadId_t sensorTaskHandle;
osThreadId_t printTaskHandle;
osMessageQueueId_t printQueueHandle;

/* 任务属性: CMSIS stack_size单位=字节(与xTaskCreate的字差4倍) */
const osThreadAttr_t sensorTask_attributes = {
    .name = "sensorTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = SENSOR_TASK_STACK,
};
const osThreadAttr_t printTask_attributes = {
    .name = "printTask",
    .priority = (osPriority_t) osPriorityAboveNormal,
    .stack_size = PRINT_TASK_STACK,
};

/* Private function prototypes -----------------------------------------------*/
void SensorTask(void *argument);
void PrintTask(void *argument);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void configureTimerForRunTimeStats(void);
unsigned long getRunTimeCounterValue(void);

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
__weak void configureTimerForRunTimeStats(void)
{

}

__weak unsigned long getRunTimeCounterValue(void)
{
    return 0;
}
/* USER CODE END 1 */

/* USER CODE BEGIN PREPOSTSLEEP */
__weak void PreSleepProcessing(uint32_t ulExpectedIdleTime)
{
    /* place for user code */
}

__weak void PostSleepProcessing(uint32_t ulExpectedIdleTime)
{
    /* place for user code */
}
/* USER CODE END PREPOSTSLEEP */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void)
{
    /* 打印队列: 采集任务只投递消息, 打印任务独占UART(消除竞争) */
    printQueueHandle = osMessageQueueNew(PRINT_QUEUE_LEN, PRINT_MSG_MAX, NULL);
    if (printQueueHandle == NULL)
    {
        Error_Handler();
    }

    sensorTaskHandle = osThreadNew(SensorTask, NULL, &sensorTask_attributes);
    if (sensorTaskHandle == NULL)
    {
        Error_Handler();
    }
    printTaskHandle = osThreadNew(PrintTask, NULL, &printTask_attributes);
    if (printTaskHandle == NULL)
    {
        Error_Handler();
    }
}

/*************************************
 * 函数名称 ： SensorTask
 * 描述     ： 传感器采集任务: 周期轮询三传感器, 格式化后投入打印队列
 * 输入     ： argument - 未用
 * 输出     ： 无
 * 返回     ： 无(不退出)
 **************************************/
void SensorTask(void *argument)
{
    (void)argument;
    float temp = 0.0F;
    float hum = 0.0F;
    Ap3216c_Data_t ap_data = {0};
    Max30102_Sample_t mx_samples[MAX30102_FIFO_DEPTH] = {{0}};
    uint8_t mx_count;
    char msg[PRINT_MSG_MAX];
    unsigned int als_lux;
    uint32_t loop_cnt = 0U;

    /* 首轮先排空MAX30102 FIFO残样, 从此刻起数据连续 */
    (void)max30102_read_fifo(mx_samples, MAX30102_FIFO_DEPTH);

    for (;;)
    {
        loop_cnt++;
        /* SHT20: 温湿度 */
        if (sht20_read_temp_hum(&temp, &hum))
        {
            (void)snprintf(msg, sizeof(msg),
                           "[SHT20]    T=%.2f C, RH=%.2f %%\r\n", temp, hum);
        }
        else
        {
            (void)snprintf(msg, sizeof(msg), "[SHT20]    read error\r\n");
        }
        (void)osMessageQueuePut(printQueueHandle, msg, 0U, 0U);

        /* AP3216C: 光感/接近/红外 */
        if (ap3216c_read_data(&ap_data))
        {
            als_lux = (unsigned int)(ap_data.als_raw
                                     * AP3216C_LUX_PER_COUNT_PCT / 100U);
            (void)snprintf(msg, sizeof(msg),
                           "[AP3216C]  ALS=%u (lux=%u), PS=%u, IR=%u, OBJ=%d\r\n",
                           ap_data.als_raw, als_lux,
                           ap_data.ps_raw, ap_data.ir_raw,
                           (int)ap_data.object_near);
        }
        else
        {
            (void)snprintf(msg, sizeof(msg), "[AP3216C]  read error\r\n");
        }
        (void)osMessageQueuePut(printQueueHandle, msg, 0U, 0U);

        /* MAX30102: 排空FIFO取最新样本 */
        mx_count = max30102_read_fifo(mx_samples, MAX30102_FIFO_DEPTH);
        if (mx_count > 0U)
        {
            (void)snprintf(msg, sizeof(msg),
                           "[MAX30102] %u samples, last Red=%lu IR=%lu (loop=%lu)\r\n",
                           mx_count,
                           mx_samples[mx_count - 1U].red,
                           mx_samples[mx_count - 1U].ir,
                           loop_cnt);
        }
        else
        {
            (void)snprintf(msg, sizeof(msg),
                           "[MAX30102] fifo empty (loop=%lu)\r\n", loop_cnt);
        }
        (void)osMessageQueuePut(printQueueHandle, msg, 0U, 0U);

        osDelay(SENSOR_POLL_PERIOD_MS);
    }
}

/*************************************
 * 函数名称 ： PrintTask
 * 描述     ： 打印守门人任务: 独占UART, 从队列取消息串行输出
 *            (printf三宗罪对策: 多任务打印全部经此队列串行化)
 * 输入     ： argument - 未用
 * 输出     ： 无
 * 返回     ： 无(不退出)
 **************************************/
void PrintTask(void *argument)
{
    (void)argument;
    char msg[PRINT_MSG_MAX];
    osStatus_t status;

    for (;;)
    {
        /* 阻塞等待: 无消息时不占CPU */
        status = osMessageQueueGet(printQueueHandle, msg, NULL, osWaitForever);
        if (status == osOK)
        {
            printf("%s", msg);
        }
    }
}
