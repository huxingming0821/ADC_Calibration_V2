/**
 * @file    Examples_HAL_StdLib.c
 * @brief   ADC校准系统完整使用例程
 * @version 2.0
 * @author  Based on CH_Calib V2.0
 * 
 * @details 本文件包含以下内容:
 *          1. HAL库 + AT24Cxx EEPROM 完整示例
 *          2. HAL库 + W25Qxx SPI Flash 完整示例
 *          3. HAL库 + 内部Flash 完整示例
 *          4. 标准库 + AT24Cxx EEPROM 完整示例
 *          5. 标准库 + W25Qxx SPI Flash 完整示例
 *          6. 标准库 + 内部Flash 完整示例
 *          7. 校准公式应用示例
 *          8. 通信协议处理示例
 */

#include "CH_Calib.h"
#include "Calib_Storage.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 *                    第一部分: HAL库 + AT24Cxx EEPROM
 *============================================================================
 * 适用芯片: STM32F1/F4/F7/H7系列
 * 存储器件: AT24C02/04/08/16/32/64/128/256
 * 接口方式: I2C
 *============================================================================*/

#ifdef USE_HAL_AT24CXX

#include "stm32f1xx_hal.h"  /* 根据实际芯片修改 */

/*---------------------------------------------------------------------------
 * 配置参数 - 根据实际硬件修改
 *---------------------------------------------------------------------------*/
#define AT24CXX_I2C_HANDLE      hi2c1           /* I2C句柄 */
#define AT24CXX_DEVICE_ADDR     0xA0            /* 设备地址 (7位地址左移1位) */
#define AT24CXX_PAGE_SIZE       32              /* AT24C64页大小, AT24C02=8 */
#define AT24CXX_TOTAL_SIZE      8192            /* AT24C64=8KB */
#define AT24CXX_WRITE_DELAY     5               /* 写入延时ms */
#define AT24CXX_TIMEOUT         100             /* I2C超时ms */

extern I2C_HandleTypeDef AT24CXX_I2C_HANDLE;

/*---------------------------------------------------------------------------
 * 底层驱动函数实现
 *---------------------------------------------------------------------------*/

/**
 * @brief  HAL库 I2C初始化 (通常由CubeMX生成)
 */
static int hal_at24cxx_init(void)
{
    /* I2C通常由MX_I2C1_Init()初始化,这里可以做额外检测 */
    
    /* 检测设备是否存在 */
    HAL_StatusTypeDef status;
    status = HAL_I2C_IsDeviceReady(&AT24CXX_I2C_HANDLE, AT24CXX_DEVICE_ADDR, 3, AT24CXX_TIMEOUT);
    
    if (status != HAL_OK) {
        return -1;  /* 设备未检测到 */
    }
    
    return 0;
}

/**
 * @brief  HAL库 AT24Cxx读取
 * @param  addr: 存储地址
 * @param  buf: 数据缓冲区
 * @param  len: 读取长度
 * @return 0成功, -1失败
 */
static int hal_at24cxx_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    HAL_StatusTypeDef status;
    
    /* AT24C64及以上使用16位地址 */
    #if (AT24CXX_TOTAL_SIZE > 256)
        status = HAL_I2C_Mem_Read(&AT24CXX_I2C_HANDLE, 
                                   AT24CXX_DEVICE_ADDR,
                                   (uint16_t)addr, 
                                   I2C_MEMADD_SIZE_16BIT,
                                   buf, 
                                   (uint16_t)len, 
                                   AT24CXX_TIMEOUT);
    #else
        /* AT24C02/04/08使用8位地址 */
        status = HAL_I2C_Mem_Read(&AT24CXX_I2C_HANDLE, 
                                   AT24CXX_DEVICE_ADDR,
                                   (uint16_t)addr, 
                                   I2C_MEMADD_SIZE_8BIT,
                                   buf, 
                                   (uint16_t)len, 
                                   AT24CXX_TIMEOUT);
    #endif
    
    return (status == HAL_OK) ? 0 : -1;
}

/**
 * @brief  HAL库 AT24Cxx页写入
 * @note   AT24Cxx有页写入限制,跨页需要分次写入
 */
static int hal_at24cxx_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    HAL_StatusTypeDef status;
    uint32_t written = 0;
    
    while (written < len) {
        /* 计算当前页剩余空间 */
        uint32_t page_offset = (addr + written) % AT24CXX_PAGE_SIZE;
        uint32_t page_remain = AT24CXX_PAGE_SIZE - page_offset;
        uint32_t to_write = (len - written) > page_remain ? page_remain : (len - written);
        
        /* 写入一页 */
        #if (AT24CXX_TOTAL_SIZE > 256)
            status = HAL_I2C_Mem_Write(&AT24CXX_I2C_HANDLE,
                                        AT24CXX_DEVICE_ADDR,
                                        (uint16_t)(addr + written),
                                        I2C_MEMADD_SIZE_16BIT,
                                        (uint8_t*)(buf + written),
                                        (uint16_t)to_write,
                                        AT24CXX_TIMEOUT);
        #else
            status = HAL_I2C_Mem_Write(&AT24CXX_I2C_HANDLE,
                                        AT24CXX_DEVICE_ADDR,
                                        (uint16_t)(addr + written),
                                        I2C_MEMADD_SIZE_8BIT,
                                        (uint8_t*)(buf + written),
                                        (uint16_t)to_write,
                                        AT24CXX_TIMEOUT);
        #endif
        
        if (status != HAL_OK) {
            return -1;
        }
        
        /* 等待写入完成 */
        HAL_Delay(AT24CXX_WRITE_DELAY);
        
        written += to_write;
    }
    
    return 0;
}

/**
 * @brief  检测设备就绪
 */
static bool hal_at24cxx_ready(void)
{
    return (HAL_I2C_IsDeviceReady(&AT24CXX_I2C_HANDLE, AT24CXX_DEVICE_ADDR, 1, AT24CXX_TIMEOUT) == HAL_OK);
}

/*---------------------------------------------------------------------------
 * 驱动结构体定义
 *---------------------------------------------------------------------------*/
const StorageDriver_t g_hal_at24cxx_driver = {
    .name        = "AT24C64-HAL",
    .base_addr   = 0,
    .total_size  = AT24CXX_TOTAL_SIZE,
    .page_size   = AT24CXX_PAGE_SIZE,
    .sector_size = 1,               /* EEPROM无扇区概念 */
    
    .init        = hal_at24cxx_init,
    .read        = hal_at24cxx_read,
    .write       = hal_at24cxx_write,
    .erase       = NULL,            /* EEPROM不需要擦除 */
    .is_ready    = hal_at24cxx_ready,
};

#endif /* USE_HAL_AT24CXX */


/*============================================================================
 *                    第二部分: HAL库 + W25Qxx SPI Flash
 *============================================================================
 * 适用芯片: STM32F1/F4/F7/H7系列
 * 存储器件: W25Q16/32/64/128
 * 接口方式: SPI
 *============================================================================*/

#ifdef USE_HAL_W25QXX

#include "stm32f1xx_hal.h"

/*---------------------------------------------------------------------------
 * 配置参数
 *---------------------------------------------------------------------------*/
#define W25QXX_SPI_HANDLE       hspi1
#define W25QXX_CS_GPIO          GPIOA
#define W25QXX_CS_PIN           GPIO_PIN_4
#define W25QXX_SECTOR_SIZE      4096
#define W25QXX_PAGE_SIZE        256
#define W25QXX_TOTAL_SIZE       (8 * 1024 * 1024)   /* 8MB (W25Q64) */
#define W25QXX_CALIB_BASE       0x700000            /* 校准数据起始地址,最后1MB */
#define W25QXX_CALIB_SIZE       0x100000            /* 分配1MB */

extern SPI_HandleTypeDef W25QXX_SPI_HANDLE;

/* W25Qxx命令定义 */
#define W25X_WriteEnable        0x06
#define W25X_WriteDisable       0x04
#define W25X_ReadStatusReg1     0x05
#define W25X_ReadData           0x03
#define W25X_PageProgram        0x02
#define W25X_SectorErase        0x20
#define W25X_ChipErase          0xC7
#define W25X_ReadID             0x90

/* CS控制宏 */
#define W25QXX_CS_LOW()         HAL_GPIO_WritePin(W25QXX_CS_GPIO, W25QXX_CS_PIN, GPIO_PIN_RESET)
#define W25QXX_CS_HIGH()        HAL_GPIO_WritePin(W25QXX_CS_GPIO, W25QXX_CS_PIN, GPIO_PIN_SET)

/*---------------------------------------------------------------------------
 * 底层SPI操作
 *---------------------------------------------------------------------------*/

static uint8_t hal_w25qxx_spi_rw(uint8_t data)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&W25QXX_SPI_HANDLE, &data, &rx, 1, 100);
    return rx;
}

static uint16_t hal_w25qxx_read_id(void)
{
    uint16_t id = 0;
    
    W25QXX_CS_LOW();
    hal_w25qxx_spi_rw(W25X_ReadID);
    hal_w25qxx_spi_rw(0x00);
    hal_w25qxx_spi_rw(0x00);
    hal_w25qxx_spi_rw(0x00);
    id = hal_w25qxx_spi_rw(0xFF) << 8;
    id |= hal_w25qxx_spi_rw(0xFF);
    W25QXX_CS_HIGH();
    
    return id;
}

static void hal_w25qxx_write_enable(void)
{
    W25QXX_CS_LOW();
    hal_w25qxx_spi_rw(W25X_WriteEnable);
    W25QXX_CS_HIGH();
}

static void hal_w25qxx_wait_busy(void)
{
    uint8_t status;
    uint32_t timeout = 100000;
    
    W25QXX_CS_LOW();
    hal_w25qxx_spi_rw(W25X_ReadStatusReg1);
    do {
        status = hal_w25qxx_spi_rw(0xFF);
        timeout--;
    } while ((status & 0x01) && timeout > 0);
    W25QXX_CS_HIGH();
}

/*---------------------------------------------------------------------------
 * 驱动接口实现
 *---------------------------------------------------------------------------*/

static int hal_w25qxx_init(void)
{
    /* SPI通常由MX_SPI1_Init()初始化 */
    
    /* 检测芯片ID */
    uint16_t id = hal_w25qxx_read_id();
    
    /* 验证ID (W25Q64: 0xEF16, W25Q128: 0xEF17) */
    if ((id & 0xFF00) != 0xEF00) {
        return -1;  /* 未检测到W25Qxx */
    }
    
    return 0;
}

static int hal_w25qxx_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    W25QXX_CS_LOW();
    
    hal_w25qxx_spi_rw(W25X_ReadData);
    hal_w25qxx_spi_rw((addr >> 16) & 0xFF);
    hal_w25qxx_spi_rw((addr >> 8) & 0xFF);
    hal_w25qxx_spi_rw(addr & 0xFF);
    
    /* 使用DMA读取大块数据效率更高 */
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = hal_w25qxx_spi_rw(0xFF);
    }
    
    W25QXX_CS_HIGH();
    return 0;
}

static int hal_w25qxx_write_page(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    hal_w25qxx_write_enable();
    
    W25QXX_CS_LOW();
    hal_w25qxx_spi_rw(W25X_PageProgram);
    hal_w25qxx_spi_rw((addr >> 16) & 0xFF);
    hal_w25qxx_spi_rw((addr >> 8) & 0xFF);
    hal_w25qxx_spi_rw(addr & 0xFF);
    
    for (uint32_t i = 0; i < len; i++) {
        hal_w25qxx_spi_rw(buf[i]);
    }
    
    W25QXX_CS_HIGH();
    hal_w25qxx_wait_busy();
    
    return 0;
}

static int hal_w25qxx_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t written = 0;
    
    while (written < len) {
        uint32_t page_offset = addr % W25QXX_PAGE_SIZE;
        uint32_t page_remain = W25QXX_PAGE_SIZE - page_offset;
        uint32_t to_write = (len - written) > page_remain ? page_remain : (len - written);
        
        if (hal_w25qxx_write_page(addr, buf + written, to_write) != 0) {
            return -1;
        }
        
        addr += to_write;
        written += to_write;
    }
    
    return 0;
}

static int hal_w25qxx_erase(uint32_t addr, uint32_t len)
{
    /* 计算需要擦除的扇区 */
    uint32_t start_sector = addr / W25QXX_SECTOR_SIZE;
    uint32_t end_sector = (addr + len - 1) / W25QXX_SECTOR_SIZE;
    
    for (uint32_t s = start_sector; s <= end_sector; s++) {
        uint32_t sector_addr = s * W25QXX_SECTOR_SIZE;
        
        hal_w25qxx_write_enable();
        
        W25QXX_CS_LOW();
        hal_w25qxx_spi_rw(W25X_SectorErase);
        hal_w25qxx_spi_rw((sector_addr >> 16) & 0xFF);
        hal_w25qxx_spi_rw((sector_addr >> 8) & 0xFF);
        hal_w25qxx_spi_rw(sector_addr & 0xFF);
        W25QXX_CS_HIGH();
        
        hal_w25qxx_wait_busy();
    }
    
    return 0;
}

static bool hal_w25qxx_ready(void)
{
    uint16_t id = hal_w25qxx_read_id();
    return ((id & 0xFF00) == 0xEF00);
}

/*---------------------------------------------------------------------------
 * 驱动结构体
 *---------------------------------------------------------------------------*/
const StorageDriver_t g_hal_w25qxx_driver = {
    .name        = "W25Q64-HAL",
    .base_addr   = W25QXX_CALIB_BASE,
    .total_size  = W25QXX_CALIB_SIZE,
    .page_size   = W25QXX_PAGE_SIZE,
    .sector_size = W25QXX_SECTOR_SIZE,
    
    .init        = hal_w25qxx_init,
    .read        = hal_w25qxx_read,
    .write       = hal_w25qxx_write,
    .erase       = hal_w25qxx_erase,
    .is_ready    = hal_w25qxx_ready,
};

#endif /* USE_HAL_W25QXX */


/*============================================================================
 *                    第三部分: HAL库 + 内部Flash
 *============================================================================
 * 适用芯片: STM32F103C8T6 (64KB Flash) / STM32F103RCT6 (256KB Flash)
 * 注意: 不同系列Flash结构不同,需要根据实际芯片调整
 *============================================================================*/

#ifdef USE_HAL_INTERNAL_FLASH

#include "stm32f1xx_hal.h"

/*---------------------------------------------------------------------------
 * 配置参数 - STM32F103C8T6 示例
 *---------------------------------------------------------------------------*/
/* Flash布局: 0x08000000 - 0x0800FFFF (64KB)
 * 程序区: 0x08000000 - 0x0800EFFF (60KB)
 * 校准区: 0x0800F000 - 0x0800FFFF (4KB, 最后4页)
 */
#define FLASH_PAGE_SIZE         1024            /* STM32F103页大小1KB */
#define FLASH_CALIB_START       0x0800F000      /* 校准数据起始 */
#define FLASH_CALIB_SIZE        (4 * 1024)      /* 4KB */
#define FLASH_CALIB_PAGES       4

/*---------------------------------------------------------------------------
 * 驱动接口实现
 *---------------------------------------------------------------------------*/

static int hal_internal_flash_init(void)
{
    /* 内部Flash无需初始化 */
    return 0;
}

static int hal_internal_flash_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    /* 直接内存映射读取 */
    memcpy(buf, (void*)addr, len);
    return 0;
}

static int hal_internal_flash_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    HAL_StatusTypeDef status;
    
    /* 解锁Flash */
    HAL_FLASH_Unlock();
    
    /* STM32F1按半字(16bit)编程 */
    for (uint32_t i = 0; i < len; i += 2) {
        uint16_t half_word;
        
        half_word = buf[i];
        if (i + 1 < len) {
            half_word |= (uint16_t)buf[i + 1] << 8;
        } else {
            half_word |= 0xFF00;  /* 奇数长度时填充0xFF */
        }
        
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, half_word);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return -1;
        }
    }
    
    HAL_FLASH_Lock();
    return 0;
}

static int hal_internal_flash_erase(uint32_t addr, uint32_t len)
{
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;
    HAL_StatusTypeDef status;
    
    /* 计算需要擦除的页 */
    uint32_t start_page = (addr - FLASH_BASE) / FLASH_PAGE_SIZE;
    uint32_t num_pages = (len + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
    
    HAL_FLASH_Unlock();
    
    erase_init.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = FLASH_BASE + start_page * FLASH_PAGE_SIZE;
    erase_init.NbPages     = num_pages;
    
    status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    
    HAL_FLASH_Lock();
    
    return (status == HAL_OK) ? 0 : -1;
}

static bool hal_internal_flash_ready(void)
{
    return true;  /* 内部Flash始终就绪 */
}

/*---------------------------------------------------------------------------
 * 驱动结构体
 *---------------------------------------------------------------------------*/
const StorageDriver_t g_hal_internal_flash_driver = {
    .name        = "InternalFlash-HAL",
    .base_addr   = FLASH_CALIB_START,
    .total_size  = FLASH_CALIB_SIZE,
    .page_size   = 2,               /* 半字写入 */
    .sector_size = FLASH_PAGE_SIZE,
    
    .init        = hal_internal_flash_init,
    .read        = hal_internal_flash_read,
    .write       = hal_internal_flash_write,
    .erase       = hal_internal_flash_erase,
    .is_ready    = hal_internal_flash_ready,
};

#endif /* USE_HAL_INTERNAL_FLASH */


/*============================================================================
 *                    第四部分: 标准库 + AT24Cxx EEPROM
 *============================================================================
 * 适用芯片: STM32F1系列 (使用标准外设库)
 * 存储器件: AT24C02/04/08/16/32/64
 *============================================================================*/

#ifdef USE_STDLIB_AT24CXX

#include "stm32f10x.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

/*---------------------------------------------------------------------------
 * 配置参数
 *---------------------------------------------------------------------------*/
#define STDLIB_AT24_I2C         I2C1
#define STDLIB_AT24_ADDR        0xA0
#define STDLIB_AT24_PAGE_SIZE   32              /* AT24C64 */
#define STDLIB_AT24_TOTAL_SIZE  8192
#define STDLIB_AT24_TIMEOUT     10000

/*---------------------------------------------------------------------------
 * 标准库I2C底层操作
 *---------------------------------------------------------------------------*/

static uint8_t stdlib_i2c_start(void)
{
    uint32_t timeout = STDLIB_AT24_TIMEOUT;
    
    I2C_GenerateSTART(STDLIB_AT24_I2C, ENABLE);
    while (!I2C_CheckEvent(STDLIB_AT24_I2C, I2C_EVENT_MASTER_MODE_SELECT)) {
        if (--timeout == 0) return 1;
    }
    return 0;
}

static uint8_t stdlib_i2c_send_addr(uint8_t addr, uint8_t direction)
{
    uint32_t timeout = STDLIB_AT24_TIMEOUT;
    uint32_t event = (direction == I2C_Direction_Transmitter) ? 
                     I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED :
                     I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED;
    
    I2C_Send7bitAddress(STDLIB_AT24_I2C, addr, direction);
    while (!I2C_CheckEvent(STDLIB_AT24_I2C, event)) {
        if (--timeout == 0) return 1;
    }
    return 0;
}

static uint8_t stdlib_i2c_send_byte(uint8_t data)
{
    uint32_t timeout = STDLIB_AT24_TIMEOUT;
    
    I2C_SendData(STDLIB_AT24_I2C, data);
    while (!I2C_CheckEvent(STDLIB_AT24_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        if (--timeout == 0) return 1;
    }
    return 0;
}

static uint8_t stdlib_i2c_recv_byte(uint8_t ack)
{
    uint32_t timeout = STDLIB_AT24_TIMEOUT;
    
    I2C_AcknowledgeConfig(STDLIB_AT24_I2C, ack ? ENABLE : DISABLE);
    while (!I2C_CheckEvent(STDLIB_AT24_I2C, I2C_EVENT_MASTER_BYTE_RECEIVED)) {
        if (--timeout == 0) return 0xFF;
    }
    return I2C_ReceiveData(STDLIB_AT24_I2C);
}

static void stdlib_i2c_stop(void)
{
    I2C_GenerateSTOP(STDLIB_AT24_I2C, ENABLE);
}

/*---------------------------------------------------------------------------
 * 延时函数 (简单实现,建议使用SysTick)
 *---------------------------------------------------------------------------*/
static void stdlib_delay_ms(uint32_t ms)
{
    /* 简单延时,72MHz时钟下约1ms */
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile uint32_t j = 0; j < 8000; j++);
    }
}

/*---------------------------------------------------------------------------
 * 驱动接口实现
 *---------------------------------------------------------------------------*/

static int stdlib_at24cxx_init(void)
{
    GPIO_InitTypeDef gpio_init;
    I2C_InitTypeDef i2c_init;
    
    /* 使能时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    
    /* 配置I2C引脚 PB6-SCL, PB7-SDA */
    gpio_init.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_OD;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio_init);
    
    /* 配置I2C */
    I2C_DeInit(STDLIB_AT24_I2C);
    i2c_init.I2C_Mode = I2C_Mode_I2C;
    i2c_init.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c_init.I2C_OwnAddress1 = 0x00;
    i2c_init.I2C_Ack = I2C_Ack_Enable;
    i2c_init.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    i2c_init.I2C_ClockSpeed = 400000;  /* 400kHz */
    I2C_Init(STDLIB_AT24_I2C, &i2c_init);
    
    I2C_Cmd(STDLIB_AT24_I2C, ENABLE);
    
    /* 检测设备 */
    if (stdlib_i2c_start() != 0) return -1;
    if (stdlib_i2c_send_addr(STDLIB_AT24_ADDR, I2C_Direction_Transmitter) != 0) {
        stdlib_i2c_stop();
        return -1;
    }
    stdlib_i2c_stop();
    
    return 0;
}

static int stdlib_at24cxx_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    /* 发送地址 */
    if (stdlib_i2c_start() != 0) return -1;
    if (stdlib_i2c_send_addr(STDLIB_AT24_ADDR, I2C_Direction_Transmitter) != 0) {
        stdlib_i2c_stop();
        return -1;
    }
    
    /* 16位地址 */
    #if (STDLIB_AT24_TOTAL_SIZE > 256)
        stdlib_i2c_send_byte((addr >> 8) & 0xFF);
    #endif
    stdlib_i2c_send_byte(addr & 0xFF);
    
    /* 重新启动,切换为读取 */
    if (stdlib_i2c_start() != 0) return -1;
    if (stdlib_i2c_send_addr(STDLIB_AT24_ADDR, I2C_Direction_Receiver) != 0) {
        stdlib_i2c_stop();
        return -1;
    }
    
    /* 读取数据 */
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = stdlib_i2c_recv_byte(i < len - 1);  /* 最后一字节NACK */
    }
    
    stdlib_i2c_stop();
    return 0;
}

static int stdlib_at24cxx_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t written = 0;
    
    while (written < len) {
        uint32_t page_offset = (addr + written) % STDLIB_AT24_PAGE_SIZE;
        uint32_t page_remain = STDLIB_AT24_PAGE_SIZE - page_offset;
        uint32_t to_write = (len - written) > page_remain ? page_remain : (len - written);
        
        /* 开始写入 */
        if (stdlib_i2c_start() != 0) return -1;
        if (stdlib_i2c_send_addr(STDLIB_AT24_ADDR, I2C_Direction_Transmitter) != 0) {
            stdlib_i2c_stop();
            return -1;
        }
        
        /* 发送地址 */
        #if (STDLIB_AT24_TOTAL_SIZE > 256)
            stdlib_i2c_send_byte(((addr + written) >> 8) & 0xFF);
        #endif
        stdlib_i2c_send_byte((addr + written) & 0xFF);
        
        /* 写入数据 */
        for (uint32_t i = 0; i < to_write; i++) {
            stdlib_i2c_send_byte(buf[written + i]);
        }
        
        stdlib_i2c_stop();
        
        /* 等待写入完成 */
        stdlib_delay_ms(5);
        
        written += to_write;
    }
    
    return 0;
}

static bool stdlib_at24cxx_ready(void)
{
    if (stdlib_i2c_start() != 0) return false;
    uint8_t ret = stdlib_i2c_send_addr(STDLIB_AT24_ADDR, I2C_Direction_Transmitter);
    stdlib_i2c_stop();
    return (ret == 0);
}

/*---------------------------------------------------------------------------
 * 驱动结构体
 *---------------------------------------------------------------------------*/
const StorageDriver_t g_stdlib_at24cxx_driver = {
    .name        = "AT24C64-StdLib",
    .base_addr   = 0,
    .total_size  = STDLIB_AT24_TOTAL_SIZE,
    .page_size   = STDLIB_AT24_PAGE_SIZE,
    .sector_size = 1,
    
    .init        = stdlib_at24cxx_init,
    .read        = stdlib_at24cxx_read,
    .write       = stdlib_at24cxx_write,
    .erase       = NULL,
    .is_ready    = stdlib_at24cxx_ready,
};

#endif /* USE_STDLIB_AT24CXX */


/*============================================================================
 *                    第五部分: 标准库 + W25Qxx SPI Flash
 *============================================================================*/

#ifdef USE_STDLIB_W25QXX

#include "stm32f10x.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

/*---------------------------------------------------------------------------
 * 配置参数
 *---------------------------------------------------------------------------*/
#define STDLIB_W25_SPI          SPI1
#define STDLIB_W25_CS_PORT      GPIOA
#define STDLIB_W25_CS_PIN       GPIO_Pin_4
#define STDLIB_W25_SECTOR_SIZE  4096
#define STDLIB_W25_PAGE_SIZE    256
#define STDLIB_W25_CALIB_BASE   0x700000
#define STDLIB_W25_CALIB_SIZE   0x100000

#define STDLIB_W25_CS_LOW()     GPIO_ResetBits(STDLIB_W25_CS_PORT, STDLIB_W25_CS_PIN)
#define STDLIB_W25_CS_HIGH()    GPIO_SetBits(STDLIB_W25_CS_PORT, STDLIB_W25_CS_PIN)

/*---------------------------------------------------------------------------
 * SPI底层操作
 *---------------------------------------------------------------------------*/

static uint8_t stdlib_spi_rw(uint8_t data)
{
    while (SPI_I2S_GetFlagStatus(STDLIB_W25_SPI, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(STDLIB_W25_SPI, data);
    while (SPI_I2S_GetFlagStatus(STDLIB_W25_SPI, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(STDLIB_W25_SPI);
}

static void stdlib_w25_write_enable(void)
{
    STDLIB_W25_CS_LOW();
    stdlib_spi_rw(0x06);
    STDLIB_W25_CS_HIGH();
}

static void stdlib_w25_wait_busy(void)
{
    uint8_t status;
    STDLIB_W25_CS_LOW();
    stdlib_spi_rw(0x05);
    do {
        status = stdlib_spi_rw(0xFF);
    } while (status & 0x01);
    STDLIB_W25_CS_HIGH();
}

/*---------------------------------------------------------------------------
 * 驱动接口实现
 *---------------------------------------------------------------------------*/

static int stdlib_w25qxx_init(void)
{
    GPIO_InitTypeDef gpio_init;
    SPI_InitTypeDef spi_init;
    
    /* 使能时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);
    
    /* CS引脚 */
    gpio_init.GPIO_Pin = STDLIB_W25_CS_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(STDLIB_W25_CS_PORT, &gpio_init);
    STDLIB_W25_CS_HIGH();
    
    /* SPI引脚 PA5-SCK, PA6-MISO, PA7-MOSI */
    gpio_init.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio_init);
    
    gpio_init.GPIO_Pin = GPIO_Pin_6;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio_init);
    
    /* 配置SPI */
    spi_init.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi_init.SPI_Mode = SPI_Mode_Master;
    spi_init.SPI_DataSize = SPI_DataSize_8b;
    spi_init.SPI_CPOL = SPI_CPOL_High;
    spi_init.SPI_CPHA = SPI_CPHA_2Edge;
    spi_init.SPI_NSS = SPI_NSS_Soft;
    spi_init.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    spi_init.SPI_FirstBit = SPI_FirstBit_MSB;
    spi_init.SPI_CRCPolynomial = 7;
    SPI_Init(STDLIB_W25_SPI, &spi_init);
    SPI_Cmd(STDLIB_W25_SPI, ENABLE);
    
    /* 检测芯片ID */
    uint16_t id;
    STDLIB_W25_CS_LOW();
    stdlib_spi_rw(0x90);
    stdlib_spi_rw(0x00);
    stdlib_spi_rw(0x00);
    stdlib_spi_rw(0x00);
    id = stdlib_spi_rw(0xFF) << 8;
    id |= stdlib_spi_rw(0xFF);
    STDLIB_W25_CS_HIGH();
    
    return ((id & 0xFF00) == 0xEF00) ? 0 : -1;
}

static int stdlib_w25qxx_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    STDLIB_W25_CS_LOW();
    stdlib_spi_rw(0x03);
    stdlib_spi_rw((addr >> 16) & 0xFF);
    stdlib_spi_rw((addr >> 8) & 0xFF);
    stdlib_spi_rw(addr & 0xFF);
    
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = stdlib_spi_rw(0xFF);
    }
    
    STDLIB_W25_CS_HIGH();
    return 0;
}

static int stdlib_w25qxx_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t written = 0;
    
    while (written < len) {
        uint32_t page_offset = addr % STDLIB_W25_PAGE_SIZE;
        uint32_t page_remain = STDLIB_W25_PAGE_SIZE - page_offset;
        uint32_t to_write = (len - written) > page_remain ? page_remain : (len - written);
        
        stdlib_w25_write_enable();
        
        STDLIB_W25_CS_LOW();
        stdlib_spi_rw(0x02);
        stdlib_spi_rw((addr >> 16) & 0xFF);
        stdlib_spi_rw((addr >> 8) & 0xFF);
        stdlib_spi_rw(addr & 0xFF);
        
        for (uint32_t i = 0; i < to_write; i++) {
            stdlib_spi_rw(buf[written + i]);
        }
        
        STDLIB_W25_CS_HIGH();
        stdlib_w25_wait_busy();
        
        addr += to_write;
        written += to_write;
    }
    
    return 0;
}

static int stdlib_w25qxx_erase(uint32_t addr, uint32_t len)
{
    uint32_t start_sector = addr / STDLIB_W25_SECTOR_SIZE;
    uint32_t end_sector = (addr + len - 1) / STDLIB_W25_SECTOR_SIZE;
    
    for (uint32_t s = start_sector; s <= end_sector; s++) {
        uint32_t sector_addr = s * STDLIB_W25_SECTOR_SIZE;
        
        stdlib_w25_write_enable();
        
        STDLIB_W25_CS_LOW();
        stdlib_spi_rw(0x20);
        stdlib_spi_rw((sector_addr >> 16) & 0xFF);
        stdlib_spi_rw((sector_addr >> 8) & 0xFF);
        stdlib_spi_rw(sector_addr & 0xFF);
        STDLIB_W25_CS_HIGH();
        
        stdlib_w25_wait_busy();
    }
    
    return 0;
}

static bool stdlib_w25qxx_ready(void)
{
    uint16_t id;
    STDLIB_W25_CS_LOW();
    stdlib_spi_rw(0x90);
    stdlib_spi_rw(0x00);
    stdlib_spi_rw(0x00);
    stdlib_spi_rw(0x00);
    id = stdlib_spi_rw(0xFF) << 8;
    id |= stdlib_spi_rw(0xFF);
    STDLIB_W25_CS_HIGH();
    return ((id & 0xFF00) == 0xEF00);
}

/*---------------------------------------------------------------------------
 * 驱动结构体
 *---------------------------------------------------------------------------*/
const StorageDriver_t g_stdlib_w25qxx_driver = {
    .name        = "W25Q64-StdLib",
    .base_addr   = STDLIB_W25_CALIB_BASE,
    .total_size  = STDLIB_W25_CALIB_SIZE,
    .page_size   = STDLIB_W25_PAGE_SIZE,
    .sector_size = STDLIB_W25_SECTOR_SIZE,
    
    .init        = stdlib_w25qxx_init,
    .read        = stdlib_w25qxx_read,
    .write       = stdlib_w25qxx_write,
    .erase       = stdlib_w25qxx_erase,
    .is_ready    = stdlib_w25qxx_ready,
};

#endif /* USE_STDLIB_W25QXX */


/*============================================================================
 *                    第六部分: 标准库 + 内部Flash
 *============================================================================*/

#ifdef USE_STDLIB_INTERNAL_FLASH

#include "stm32f10x.h"
#include "stm32f10x_flash.h"

#define STDLIB_FLASH_PAGE_SIZE  1024
#define STDLIB_FLASH_CALIB_ADDR 0x0800F000
#define STDLIB_FLASH_CALIB_SIZE 4096

static int stdlib_internal_flash_init(void)
{
    return 0;
}

static int stdlib_internal_flash_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    memcpy(buf, (void*)addr, len);
    return 0;
}

static int stdlib_internal_flash_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    FLASH_Unlock();
    
    for (uint32_t i = 0; i < len; i += 2) {
        uint16_t half_word = buf[i];
        if (i + 1 < len) {
            half_word |= (uint16_t)buf[i + 1] << 8;
        } else {
            half_word |= 0xFF00;
        }
        
        if (FLASH_ProgramHalfWord(addr + i, half_word) != FLASH_COMPLETE) {
            FLASH_Lock();
            return -1;
        }
    }
    
    FLASH_Lock();
    return 0;
}

static int stdlib_internal_flash_erase(uint32_t addr, uint32_t len)
{
    uint32_t start_page = addr;
    uint32_t end_addr = addr + len;
    
    FLASH_Unlock();
    
    while (start_page < end_addr) {
        if (FLASH_ErasePage(start_page) != FLASH_COMPLETE) {
            FLASH_Lock();
            return -1;
        }
        start_page += STDLIB_FLASH_PAGE_SIZE;
    }
    
    FLASH_Lock();
    return 0;
}

static bool stdlib_internal_flash_ready(void)
{
    return true;
}

const StorageDriver_t g_stdlib_internal_flash_driver = {
    .name        = "InternalFlash-StdLib",
    .base_addr   = STDLIB_FLASH_CALIB_ADDR,
    .total_size  = STDLIB_FLASH_CALIB_SIZE,
    .page_size   = 2,
    .sector_size = STDLIB_FLASH_PAGE_SIZE,
    
    .init        = stdlib_internal_flash_init,
    .read        = stdlib_internal_flash_read,
    .write       = stdlib_internal_flash_write,
    .erase       = stdlib_internal_flash_erase,
    .is_ready    = stdlib_internal_flash_ready,
};

#endif /* USE_STDLIB_INTERNAL_FLASH */


/*============================================================================
 *                    第七部分: 校准公式应用示例
 *============================================================================
 * 说明: 展示如何在实际项目中应用校准系统
 *============================================================================*/

/**
 * @brief  校准公式应用完整示例
 * 
 * 假设场景: 7通道ADC采集系统
 * CH1: 风扇电流 (FAN_I)    - 电流互感器 + 采样电阻
 * CH2: 整流电流 (CR_I)     - 霍尔传感器
 * CH3: 270V电压 (270_U)    - 电阻分压
 * CH4: 270V电流 (270_I)    - 分流器
 * CH5: 电容电压 (CAP_U)    - 电阻分压
 * CH6: 输出电压 (OUT_U)    - 电阻分压
 * CH7: 电容电流 (CAP_I)    - 霍尔传感器
 */

/* 通道定义 */
typedef enum {
    CH_FAN_I = 1,
    CH_CR_I  = 2,
    CH_270_U = 3,
    CH_270_I = 4,
    CH_CAP_U = 5,
    CH_OUT_U = 6,
    CH_CAP_I = 7,
} CalibChannel_e;

/* ADC原始值 (由DMA采集更新) */
volatile uint16_t g_adc_raw[7];

/* 校准后的物理量 */
typedef struct {
    double fan_current;     /* A */
    double cr_current;      /* A */
    double voltage_270;     /* V */
    double current_270;     /* A */
    double cap_voltage;     /* V */
    double out_voltage;     /* V */
    double cap_current;     /* A */
} PhysicalValues_t;

PhysicalValues_t g_physical;

/**
 * @brief  方式1: 标准API调用 (简单但有函数调用开销)
 */
void CalibExample_StandardAPI(void)
{
    /* 逐通道校准 */
    Calib_Apply(CH_FAN_I, (double)g_adc_raw[0], &g_physical.fan_current);
    Calib_Apply(CH_CR_I,  (double)g_adc_raw[1], &g_physical.cr_current);
    Calib_Apply(CH_270_U, (double)g_adc_raw[2], &g_physical.voltage_270);
    Calib_Apply(CH_270_I, (double)g_adc_raw[3], &g_physical.current_270);
    Calib_Apply(CH_CAP_U, (double)g_adc_raw[4], &g_physical.cap_voltage);
    Calib_Apply(CH_OUT_U, (double)g_adc_raw[5], &g_physical.out_voltage);
    Calib_Apply(CH_CAP_I, (double)g_adc_raw[6], &g_physical.cap_current);
}

/**
 * @brief  方式2: 批量处理 (适合数据连续时)
 */
void CalibExample_BatchProcess(void)
{
    double raw[7], result[7];
    
    /* 转换为double */
    for (int i = 0; i < 7; i++) {
        raw[i] = (double)g_adc_raw[i];
    }
    
    /* 如果所有通道使用相同校准系数,可以批量处理 */
    /* Calib_ApplyBatch(1, raw, result, 7); */
    
    /* 通常各通道系数不同,需要分别处理 */
    for (int i = 0; i < 7; i++) {
        Calib_Apply(i + 1, raw[i], &result[i]);
    }
    
    /* 赋值 */
    g_physical.fan_current = result[0];
    g_physical.cr_current  = result[1];
    g_physical.voltage_270 = result[2];
    g_physical.current_270 = result[3];
    g_physical.cap_voltage = result[4];
    g_physical.out_voltage = result[5];
    g_physical.cap_current = result[6];
}

/**
 * @brief  方式3: 高性能直接计算 (适合中断/高频采样)
 * @note   直接访问全局数组,避免查找开销
 */
void CalibExample_HighPerformance(void)
{
    /* 直接使用内联函数,最小开销 */
    CalibCh_t *ch;
    
    /* CH1 - 风扇电流 */
    ch = &g_calib_ch[0];  /* 注意: 数组索引从0开始 */
    if (ch->valid && ch->id == CH_FAN_I) {
        g_physical.fan_current = Calib_Compute(ch->coeffs, ch->coeff_cnt, (double)g_adc_raw[0]);
    }
    
    /* CH2 - 整流电流 */
    ch = &g_calib_ch[1];
    if (ch->valid && ch->id == CH_CR_I) {
        g_physical.cr_current = Calib_Compute(ch->coeffs, ch->coeff_cnt, (double)g_adc_raw[1]);
    }
    
    /* ... 其他通道类似 ... */
}

/**
 * @brief  方式4: 缓存系数指针 (最高性能,适合定时器中断)
 * @note   系统初始化后缓存系数指针,运行时零开销查找
 */

/* 缓存结构 */
typedef struct {
    const double *coeffs;
    uint8_t cnt;
    bool valid;
} CalibCache_t;

CalibCache_t g_calib_cache[7];

/* 初始化时建立缓存 */
void CalibExample_InitCache(void)
{
    const uint8_t ch_ids[] = {CH_FAN_I, CH_CR_I, CH_270_U, CH_270_I, CH_CAP_U, CH_OUT_U, CH_CAP_I};
    
    for (int i = 0; i < 7; i++) {
        g_calib_cache[i].valid = false;
        
        /* 查找对应通道 */
        for (int j = 0; j < g_calib_ch_cnt; j++) {
            if (g_calib_ch[j].id == ch_ids[i] && g_calib_ch[j].valid) {
                g_calib_cache[i].coeffs = g_calib_ch[j].coeffs;
                g_calib_cache[i].cnt = g_calib_ch[j].coeff_cnt;
                g_calib_cache[i].valid = true;
                break;
            }
        }
    }
}

/* 中断中使用 - 零开销查找 */
void TIM1_UP_IRQHandler(void)  /* 示例: 定时器中断 */
{
    /* 清除中断标志... */
    
    /* 使用缓存的系数直接计算 */
    if (g_calib_cache[0].valid) {
        g_physical.fan_current = Calib_Compute(g_calib_cache[0].coeffs, 
                                                g_calib_cache[0].cnt, 
                                                (double)g_adc_raw[0]);
    }
    
    if (g_calib_cache[1].valid) {
        g_physical.cr_current = Calib_Compute(g_calib_cache[1].coeffs, 
                                               g_calib_cache[1].cnt, 
                                               (double)g_adc_raw[1]);
    }
    
    /* ... 其他通道 ... */
}

/**
 * @brief  方式5: 定点数优化 (适合无FPU的MCU)
 * @note   将double系数转换为定点数,加速计算
 */

/* 定点数格式: Q16.16 (16位整数,16位小数) */
#define Q16_SHIFT   16
#define Q16_ONE     (1 << Q16_SHIFT)

typedef struct {
    int32_t coeffs[CALIB_MAX_COEFFS];
    uint8_t cnt;
    bool valid;
} FixedPointCalib_t;

FixedPointCalib_t g_fixed_calib[7];

/* 转换为定点数 */
void CalibExample_ConvertToFixed(void)
{
    for (int i = 0; i < g_calib_ch_cnt; i++) {
        if (!g_calib_ch[i].valid) continue;
        
        int idx = g_calib_ch[i].id - 1;
        if (idx >= 0 && idx < 7) {
            g_fixed_calib[idx].cnt = g_calib_ch[i].coeff_cnt;
            g_fixed_calib[idx].valid = true;
            
            for (int j = 0; j < g_calib_ch[i].coeff_cnt; j++) {
                /* double转Q16.16定点数 */
                g_fixed_calib[idx].coeffs[j] = (int32_t)(g_calib_ch[i].coeffs[j] * Q16_ONE);
            }
        }
    }
}

/* 定点数Horner计算 */
static inline int32_t FixedPoint_Compute(const int32_t *coeffs, uint8_t cnt, int32_t x)
{
    if (cnt == 0) return x;
    
    int64_t y = coeffs[cnt - 1];
    for (int i = cnt - 2; i >= 0; i--) {
        y = (y * x) >> Q16_SHIFT;
        y += coeffs[i];
    }
    
    return (int32_t)y;
}


/*============================================================================
 *                    第八部分: 通信协议处理示例
 *============================================================================*/

/* 串口接收缓冲区 */
#define UART_RX_BUF_SIZE    256
static uint8_t g_uart_rx_buf[UART_RX_BUF_SIZE];
static volatile uint16_t g_uart_rx_len = 0;
static volatile uint8_t g_uart_rx_complete = 0;

/* 串口发送 (需要用户实现) */
extern void UART_SendData(uint8_t *data, uint16_t len);

/**
 * @brief  串口空闲中断处理 (HAL库示例)
 */
#ifdef USE_HAL_UART
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1) {
        g_uart_rx_len = Size;
        g_uart_rx_complete = 1;
    }
}
#endif

/**
 * @brief  串口空闲中断处理 (标准库示例)
 */
#ifdef USE_STDLIB_UART
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET) {
        /* 清除空闲中断 */
        volatile uint8_t clear = USART1->SR;
        clear = USART1->DR;
        (void)clear;
        
        /* 停止DMA,获取接收长度 */
        DMA_Cmd(DMA1_Channel5, DISABLE);
        g_uart_rx_len = UART_RX_BUF_SIZE - DMA_GetCurrDataCounter(DMA1_Channel5);
        g_uart_rx_complete = 1;
        
        /* 重新启动DMA接收 */
        DMA_SetCurrDataCounter(DMA1_Channel5, UART_RX_BUF_SIZE);
        DMA_Cmd(DMA1_Channel5, ENABLE);
    }
}
#endif

/**
 * @brief  协议处理主循环
 */
void CalibProtocol_Process(void)
{
    if (!g_uart_rx_complete) return;
    
    uint8_t resp_buf[256];
    uint16_t resp_len = 0;
    
    /* 处理帧 */
    CalibErr_t err = Calib_ProcessFrame(g_uart_rx_buf, g_uart_rx_len, resp_buf, &resp_len);
    
    /* 发送响应 */
    if (resp_len > 0) {
        UART_SendData(resp_buf, resp_len);
    } else if (err != CALIB_OK) {
        /* 发送错误响应 */
        resp_len = sprintf((char*)resp_buf, "ERR:%s\r\n", Calib_ErrStr(err));
        UART_SendData(resp_buf, resp_len);
    }
    
    /* 重置标志 */
    g_uart_rx_complete = 0;
    g_uart_rx_len = 0;
}

/**
 * @brief  手动设置校准系数示例
 * @note   用于出厂校准或调试
 */
void CalibExample_ManualSetup(void)
{
    /* 示例: 设置CH1 (风扇电流) 的校准系数
     * 假设校准结果: y = 0.00125*x - 0.5
     * 即: a0 = -0.5, a1 = 0.00125 (一次多项式)
     */
    double ch1_coeffs[] = {-0.5, 0.00125};
    Calib_SetCoeffs(CH_FAN_I, ch1_coeffs, 2);
    
    /* 示例: 设置CH3 (270V电压) 的校准系数
     * 假设校准结果: y = 0.0875*x + 2.5
     */
    double ch3_coeffs[] = {2.5, 0.0875};
    Calib_SetCoeffs(CH_270_U, ch3_coeffs, 2);
    
    /* 示例: 设置CH5 (电容电压) 使用二次多项式
     * y = 1.5e-7*x^2 + 0.095*x - 1.2
     */
    double ch5_coeffs[] = {-1.2, 0.095, 1.5e-7};
    Calib_SetCoeffs(CH_CAP_U, ch5_coeffs, 3);
    
    /* 保存到存储器 */
    Calib_SaveAll();
    
    /* 更新缓存 (如果使用高性能模式) */
    CalibExample_InitCache();
}


/*============================================================================
 *                    第九部分: 完整初始化流程
 *============================================================================*/

/**
 * @brief  系统初始化 - HAL库版本
 */
void System_CalibInit_HAL(void)
{
    /* 步骤1: 选择并注册存储驱动 */
    #if defined(USE_HAL_AT24CXX)
        Storage_RegisterDriver(&g_hal_at24cxx_driver);
    #elif defined(USE_HAL_W25QXX)
        Storage_RegisterDriver(&g_hal_w25qxx_driver);
    #elif defined(USE_HAL_INTERNAL_FLASH)
        Storage_RegisterDriver(&g_hal_internal_flash_driver);
    #else
        #error "Please define a storage driver!"
    #endif
    
    /* 步骤2: 初始化校准系统 */
    CalibErr_t err = Calib_Init();
    if (err != CALIB_OK) {
        /* 初始化失败处理 */
        printf("Calib Init Failed: %s\n", Calib_ErrStr(err));
        while (1);
    }
    
    /* 步骤3: 注册校准通道 */
    Calib_RegisterCh(CH_FAN_I, "FAN_I");
    Calib_RegisterCh(CH_CR_I,  "CR_I");
    Calib_RegisterCh(CH_270_U, "270_U");
    Calib_RegisterCh(CH_270_I, "270_I");
    Calib_RegisterCh(CH_CAP_U, "CAP_U");
    Calib_RegisterCh(CH_OUT_U, "OUT_U");
    Calib_RegisterCh(CH_CAP_I, "CAP_I");
    
    /* 步骤4: 从存储器加载校准数据 */
    err = Calib_LoadAll();
    if (err != CALIB_OK && err != CALIB_ERR_NO_DATA) {
        printf("Calib Load Warning: %s\n", Calib_ErrStr(err));
    }
    
    /* 步骤5: 检查是否有有效数据,没有则使用默认值 */
    for (int i = 0; i < g_calib_ch_cnt; i++) {
        if (!g_calib_ch[i].valid) {
            /* 设置默认校准: y = x (无校准) */
            double default_coeffs[] = {0.0, 1.0};
            Calib_SetCoeffs(g_calib_ch[i].id, default_coeffs, 2);
            printf("CH%d using default calibration\n", g_calib_ch[i].id);
        }
    }
    
    /* 步骤6: 初始化高性能缓存 (可选) */
    CalibExample_InitCache();
    
    /* 步骤7: 打印调试信息 */
    Calib_PrintInfo();
}

/**
 * @brief  系统初始化 - 标准库版本
 */
void System_CalibInit_StdLib(void)
{
    /* 与HAL版本类似,只是驱动不同 */
    #if defined(USE_STDLIB_AT24CXX)
        Storage_RegisterDriver(&g_stdlib_at24cxx_driver);
    #elif defined(USE_STDLIB_W25QXX)
        Storage_RegisterDriver(&g_stdlib_w25qxx_driver);
    #elif defined(USE_STDLIB_INTERNAL_FLASH)
        Storage_RegisterDriver(&g_stdlib_internal_flash_driver);
    #else
        #error "Please define a storage driver!"
    #endif
    
    /* 后续步骤与HAL版本相同... */
    Calib_Init();
    
    Calib_RegisterCh(CH_FAN_I, "FAN_I");
    Calib_RegisterCh(CH_CR_I,  "CR_I");
    Calib_RegisterCh(CH_270_U, "270_U");
    Calib_RegisterCh(CH_270_I, "270_I");
    Calib_RegisterCh(CH_CAP_U, "CAP_U");
    Calib_RegisterCh(CH_OUT_U, "OUT_U");
    Calib_RegisterCh(CH_CAP_I, "CAP_I");
    
    Calib_LoadAll();
    CalibExample_InitCache();
    Calib_PrintInfo();
}


/*============================================================================
 *                    第十部分: main函数示例
 *============================================================================*/

#if 0  /* 示例代码,实际使用时删除#if 0 */

/* HAL库 main函数示例 */
int main(void)
{
    /* HAL初始化 */
    HAL_Init();
    SystemClock_Config();
    
    /* 外设初始化 (CubeMX生成) */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_ADC1_Init();
    
    #if defined(USE_HAL_AT24CXX)
        MX_I2C1_Init();
    #elif defined(USE_HAL_W25QXX)
        MX_SPI1_Init();
    #endif
    
    /* 校准系统初始化 */
    System_CalibInit_HAL();
    
    /* 启动DMA ADC采集 */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)g_adc_raw, 7);
    
    /* 启动串口DMA接收 */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, g_uart_rx_buf, UART_RX_BUF_SIZE);
    
    /* 主循环 */
    while (1) {
        /* 处理串口命令 */
        CalibProtocol_Process();
        
        /* 应用校准 (非中断场景) */
        CalibExample_StandardAPI();
        
        /* 其他任务... */
        
        HAL_Delay(10);
    }
}

/* 标准库 main函数示例 */
int main(void)
{
    /* 系统初始化 */
    SystemInit();
    
    /* 外设初始化 */
    GPIO_Configuration();
    USART_Configuration();
    ADC_Configuration();
    DMA_Configuration();
    Timer_Configuration();
    
    /* 校准系统初始化 */
    System_CalibInit_StdLib();
    
    /* 主循环 */
    while (1) {
        CalibProtocol_Process();
        CalibExample_StandardAPI();
    }
}

#endif

/* 文件结束 */
