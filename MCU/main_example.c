/**
 * @file    main_example.c
 * @brief   校准系统使用示例 - 展示如何移植和使用
 * @version 2.0
 */

#include "CH_Calib.h"
#include "Calib_Storage.h"

/*============================================================================
 * 步骤1: 实现你的底层驱动函数
 *============================================================================*/

/* 假设你使用AT24C64 EEPROM */
/* 这些函数需要根据你的硬件平台实现 */

/* 你的I2C EEPROM读写函数声明 */
extern void I2C_EEPROM_Read(uint16_t addr, uint8_t *buf, uint16_t len);
extern void I2C_EEPROM_Write(uint16_t addr, uint8_t *buf, uint16_t len);
extern void Delay_ms(uint32_t ms);

/* 实现存储驱动接口 */
static int my_eeprom_init(void)
{
    /* 初始化I2C等 */
    /* MX_I2C1_Init(); */
    return 0;
}

static int my_eeprom_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    I2C_EEPROM_Read((uint16_t)addr, buf, (uint16_t)len);
    return 0;
}

static int my_eeprom_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    /* AT24Cxx需要分页写入 */
    #define PAGE_SIZE 32  /* AT24C64 */
    
    uint32_t written = 0;
    while (written < len) {
        uint32_t page_offset = (addr + written) % PAGE_SIZE;
        uint32_t page_remain = PAGE_SIZE - page_offset;
        uint32_t to_write = (len - written) > page_remain ? page_remain : (len - written);
        
        I2C_EEPROM_Write((uint16_t)(addr + written), (uint8_t*)(buf + written), (uint16_t)to_write);
        Delay_ms(5);  /* 等待写入完成 */
        
        written += to_write;
    }
    return 0;
}

static bool my_eeprom_ready(void)
{
    /* 检测EEPROM是否存在 */
    return true;  /* 简化处理 */
}

/*============================================================================
 * 步骤2: 定义驱动结构体
 *============================================================================*/

const StorageDriver_t g_my_eeprom_driver = {
    .name        = "AT24C64",
    .base_addr   = 0,           /* 从地址0开始存储 */
    .total_size  = 8192,        /* 8KB */
    .page_size   = 32,
    .sector_size = 1,           /* EEPROM无扇区 */
    
    .init        = my_eeprom_init,
    .read        = my_eeprom_read,
    .write       = my_eeprom_write,
    .erase       = NULL,        /* EEPROM不需要显式擦除 */
    .is_ready    = my_eeprom_ready,
};

/*============================================================================
 * 步骤3: 初始化和使用
 *============================================================================*/

/* 串口接收缓冲区 */
static uint8_t g_uart_rx_buf[256];
static uint16_t g_uart_rx_len = 0;
static uint8_t g_uart_rx_flag = 0;

void System_Init(void)
{
    /* 1. 注册存储驱动 */
    Storage_RegisterDriver(&g_my_eeprom_driver);
    
    /* 2. 初始化校准系统 */
    Calib_Init();
    
    /* 3. 注册通道 */
    Calib_RegisterCh(1, "FAN_I");
    Calib_RegisterCh(2, "CR_I");
    Calib_RegisterCh(3, "270_U");
    Calib_RegisterCh(4, "270_I");
    Calib_RegisterCh(5, "CAP_U");
    Calib_RegisterCh(6, "OUT_U");
    Calib_RegisterCh(7, "CAP_I");
    
    /* 4. 加载已存储的校准数据 */
    Calib_LoadAll();
    
    /* 5. 打印信息 (调试用) */
    Calib_PrintInfo();
}

/* 主循环处理 */
void Main_Loop(void)
{
    /* 处理串口命令 */
    if (g_uart_rx_flag) {
        uint8_t resp[256];
        uint16_t resp_len = 0;
        
        CalibErr_t err = Calib_ProcessFrame(g_uart_rx_buf, g_uart_rx_len, resp, &resp_len);
        
        if (resp_len > 0) {
            /* 发送响应 */
            /* UART_Send(resp, resp_len); */
        }
        
        g_uart_rx_flag = 0;
        g_uart_rx_len = 0;
    }
}

/* 应用校准的示例 */
void ADC_Process(void)
{
    /* 假设从ADC读取了原始值 */
    double adc_raw[7] = {1000, 2000, 3000, 4000, 5000, 6000, 7000};
    double calibrated[7];
    
    /* 方式1: 逐个计算 */
    for (int i = 0; i < 7; i++) {
        Calib_Apply(i + 1, adc_raw[i], &calibrated[i]);
    }
    
    /* 方式2: 批量计算 (更高效) */
    /* Calib_ApplyBatch(1, adc_raw, calibrated, 7); */
    
    /* 方式3: 直接使用内联函数 (最高效，用于中断) */
    /* 
    CalibCh_t *ch = &g_calib_ch[0];
    if (ch->valid) {
        double result = Calib_Compute(ch->coeffs, ch->coeff_cnt, adc_raw[0]);
    }
    */
}

/*============================================================================
 * UART接收示例 (需要根据你的平台实现)
 *============================================================================*/

/* 在UART中断中调用 */
void UART_RxCallback(uint8_t byte)
{
    if (g_uart_rx_len < sizeof(g_uart_rx_buf)) {
        g_uart_rx_buf[g_uart_rx_len++] = byte;
    }
}

/* 在UART空闲中断或定时器中调用 */
void UART_IdleCallback(void)
{
    if (g_uart_rx_len > 0) {
        g_uart_rx_flag = 1;
    }
}

/*============================================================================
 * main函数示例
 *============================================================================*/

/*
int main(void)
{
    // HAL初始化
    HAL_Init();
    SystemClock_Config();
    
    // 外设初始化
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_Init();
    MX_ADC1_Init();
    
    // 校准系统初始化
    System_Init();
    
    while (1) {
        Main_Loop();
        ADC_Process();
        HAL_Delay(10);
    }
}
*/
