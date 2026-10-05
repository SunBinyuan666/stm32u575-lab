# STM32U575 实验仓库 (2026_09)

华清远见 FS-STM32U5 套件（STM32U575RIT6 + db-v3.2 底板）课程实验，工具链为 VSCode + STM32Cube VSCode 扩展 + CMake + Ninja + arm-none-eabi-gcc。

## 实验列表

| 工程 | 内容 | 备注 |
|------|------|------|
| `Led_Key_Exti` | LED / 按键 / EXTI 外部中断入门 | |
| `Test_Project` 基础模板见上层目录 | — | 未纳入本仓库 |
| `Sensor_Test` | 传感器实验（FreeRTOS） | 含 Middlewares |
| `8Loop_Sensor` | 八路灰度传感器（I2C） | |
| `In_Total_Sensor` | 灰度传感器综合实验 | |
| `Wifi_Gyroscope` | WiFi + 陀螺仪 | |
| `Fan_DigitalTube_PWM` | 风扇开关量控制 + 数码管 + PWM 渐变波形（TIM4_CH3/PB8） | 595 驱动数码管链路 [段,位] 字节序、低有效位选 |

## 环境说明

- MCU: STM32U575RIT6 (Cortex-M33, LQFP64)
- 每个工程目录含 CubeMX `.ioc` 文件，可用 CubeMX 重新生成
- `CMakeLists.txt` + `CMakePresets.json`：VSCode/CMake/Ninja 构建
- `MDK-ARM/`：Keil 工程文件（仅启动文件与工程配置，构建产物已忽略）
- `STM32U575RITx_FLASH.ld`：GCC 链接脚本（Flash 0x08000000/2048K, RAM 0x20000000/256K 区域请按实际容量核对）
- 烧录：J-Link SWD，`flash.jlink` / `flash_full.jlink` 为烧录脚本模板

## 已排除的内容

`Drivers/`（HAL 库，62MB/工程）与 `build/` 未纳入版本控制。克隆后请用 CubeMX 打开对应 `.ioc` 重新生成 HAL 驱动，或自行从 STM32CubeU5 仓库补齐 `Drivers/STM32U5xx_HAL_Driver`。

## 硬件资源速查

详见上层目录《硬件资源》文件夹（未纳入仓库）。
