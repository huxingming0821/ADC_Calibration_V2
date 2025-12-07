/**
 * @file    Examples_HAL_StdLib.h
 * @brief   ADC校准系统完整使用例程 - 头文件
 * @version 2.0
 * 
 * @details 驱动选择说明:
 *          在编译时定义以下宏之一来选择驱动:
 * 
 *          HAL库驱动:
 *          - USE_HAL_AT24CXX         : HAL库 + AT24Cxx EEPROM
 *          - USE_HAL_W25QXX          : HAL库 + W25Qxx SPI Flash
 *          - USE_HAL_INTERNAL_FLASH  : HAL库 + STM32内部Flash
 * 
 *          标准库驱动:
 *          - USE_STDLIB_AT24CXX      : 标准库 + AT24Cxx EEPROM
 *          - USE_STDLIB_W25QXX       : 标准库 + W25Qxx SPI Flash
 *          - USE_STDLIB_INTERNAL_FLASH: 标准库 + STM32内部Flash
 * 
 *          在Keil中: Options -> C/C++ -> Preprocessor Symbols -> Define
 *          例如: USE_HAL_AT24CXX
 */

#ifndef __EXAMPLES_HAL_STDLIB_H
#define __EXAMPLES_HAL_STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "CH_Calib.h"
#include "Calib_Storage.h"

/*============================================================================
 * 通道枚举定义
 *============================================================================*/

/**
 * @brief  校准通道ID枚举
 * @note   ID从1开始,最大CALIB_MAX_CHANNELS
 */
typedef enum {
    CH_FAN_I = 1,       /**< 风扇电流 */
    CH_CR_I  = 2,       /**< 整流电流 */
    CH_270_U = 3,       /**< 270V电压 */
    CH_270_I = 4,       /**< 270V电流 */
    CH_CAP_U = 5,       /**< 电容电压 */
    CH_OUT_U = 6,       /**< 输出电压 */
    CH_CAP_I = 7,       /**< 电容电流 */
} CalibChannel_e;

/*============================================================================
 * 物理量结构体
 *============================================================================*/

/**
 * @brief  校准后的物理量
 */
typedef struct {
    double fan_current;     /**< 风扇电流 (A) */
    double cr_current;      /**< 整流电流 (A) */
    double voltage_270;     /**< 270V电压 (V) */
    double current_270;     /**< 270V电流 (A) */
    double cap_voltage;     /**< 电容电压 (V) */
    double out_voltage;     /**< 输出电压 (V) */
    double cap_current;     /**< 电容电流 (A) */
} PhysicalValues_t;

/*============================================================================
 * 外部变量声明
 *============================================================================*/

extern volatile uint16_t g_adc_raw[7];      /**< ADC原始值 */
extern PhysicalValues_t g_physical;          /**< 物理量 */

/*============================================================================
 * HAL库驱动声明
 *============================================================================*/

#ifdef USE_HAL_AT24CXX
extern const StorageDriver_t g_hal_at24cxx_driver;
#endif

#ifdef USE_HAL_W25QXX
extern const StorageDriver_t g_hal_w25qxx_driver;
#endif

#ifdef USE_HAL_INTERNAL_FLASH
extern const StorageDriver_t g_hal_internal_flash_driver;
#endif

/*============================================================================
 * 标准库驱动声明
 *============================================================================*/

#ifdef USE_STDLIB_AT24CXX
extern const StorageDriver_t g_stdlib_at24cxx_driver;
#endif

#ifdef USE_STDLIB_W25QXX
extern const StorageDriver_t g_stdlib_w25qxx_driver;
#endif

#ifdef USE_STDLIB_INTERNAL_FLASH
extern const StorageDriver_t g_stdlib_internal_flash_driver;
#endif

/*============================================================================
 * API函数声明
 *============================================================================*/

/**
 * @brief  系统校准初始化 (HAL库版本)
 * @note   包含驱动注册、系统初始化、通道注册、数据加载
 */
void System_CalibInit_HAL(void);

/**
 * @brief  系统校准初始化 (标准库版本)
 */
void System_CalibInit_StdLib(void);

/**
 * @brief  标准API调用方式校准
 * @note   简单易用,适合低频采样
 */
void CalibExample_StandardAPI(void);

/**
 * @brief  批量处理方式校准
 */
void CalibExample_BatchProcess(void);

/**
 * @brief  高性能直接计算
 * @note   直接访问全局数组,减少函数调用开销
 */
void CalibExample_HighPerformance(void);

/**
 * @brief  初始化系数缓存
 * @note   用于高性能模式,系统初始化后调用
 */
void CalibExample_InitCache(void);

/**
 * @brief  转换为定点数格式
 * @note   用于无FPU的MCU
 */
void CalibExample_ConvertToFixed(void);

/**
 * @brief  手动设置校准系数
 * @note   用于出厂校准或调试
 */
void CalibExample_ManualSetup(void);

/**
 * @brief  协议处理
 * @note   在主循环中调用
 */
void CalibProtocol_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* __EXAMPLES_HAL_STDLIB_H */
