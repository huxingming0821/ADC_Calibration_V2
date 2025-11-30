/**
 * @file    Calib_Storage_Port.c
 * @brief   存储驱动移植示例 - 用户根据实际硬件修改
 * @version 2.0
 * 
 * 使用方法:
 * 1. 根据你的硬件平台，实现对应的底层读写函数
 * 2. 选择一个驱动并注册
 * 3. 调用 Storage_Init() 初始化
 */

#include "Calib_Storage.h"

/* ========== 根据你的硬件包含对应的头文件 ========== */
// #include "24cxx.h"      /* AT24CXX EEPROM */
// #include "w25qxx.h"     /* W25Qxx SPI Flash */
// #include "stm32f1xx_hal.h"  /* STM32 HAL */

/*============================================================================
 * 方案1: AT24CXX EEPROM 驱动移植
 *============================================================================*/

#ifdef USE_AT24CXX_STORAGE

/* 
 * 用户需要实现以下函数，或者直接调用现有的EEPROM驱动
 * 示例假设你已有 AT24CXX_Read() 和 AT24CXX_Write() 函数
 */

/* 初始化 (可选) */
static int at24cxx_port_init(void)
{
    /* 如果需要初始化I2C等，在这里做 */
    /* 例如: MX_I2C1_Init(); */
    return 0;  /* 返回0表示成功 */
}

/* 读取 */
static int at24cxx_port_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    /* 调用你的EEPROM读取函数 */
    /* AT24CXX_Read(addr, buf, len); */
    
    /* 示例实现 (需要替换为实际函数) */
    extern void AT24CXX_Read(uint16_t addr, uint8_t *buf, uint16_t len);
    AT24CXX_Read((uint16_t)addr, buf, (uint16_t)len);
    return 0;
}

/* 写入 - 需要处理页写入 */
static int at24cxx_port_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    /* 调用你的EEPROM写入函数 */
    /* 注意: AT24CXX有页写入限制，每页写入后需要等待 */
    
    extern void AT24CXX_Write(uint16_t addr, uint8_t *buf, uint16_t len);
    
    #define AT24CXX_PAGE_SIZE   8   /* 根据型号调整: AT24C02=8, AT24C64=32 */
    #define AT24CXX_WRITE_DELAY 5   /* 写入延时ms */
    
    uint32_t written = 0;
    while (written < len) {
        /* 计算当前页剩余空间 */
        uint32_t page_offset = (addr + written) % AT24CXX_PAGE_SIZE;
        uint32_t page_remain = AT24CXX_PAGE_SIZE - page_offset;
        uint32_t to_write = (len - written) > page_remain ? page_remain : (len - written);
        
        /* 写入 */
        AT24CXX_Write((uint16_t)(addr + written), (uint8_t*)(buf + written), (uint16_t)to_write);
        
        /* 等待写入完成 - 使用你的延时函数 */
        /* HAL_Delay(AT24CXX_WRITE_DELAY); */
        for (volatile uint32_t i = 0; i < 50000; i++);
        
        written += to_write;
    }
    
    return 0;
}

/* 就绪检查 */
static bool at24cxx_port_ready(void)
{
    /* 检测AT24CXX是否存在 */
    extern uint8_t AT24CXX_Check(void);
    return (AT24CXX_Check() == 0);
}

/* AT24CXX驱动定义 */
const StorageDriver_t g_at24cxx_driver = {
    .name        = "AT24C64",
    .base_addr   = 0,               /* EEPROM从地址0开始 */
    .total_size  = 8192,            /* AT24C64: 8KB, 根据型号调整 */
    .page_size   = 32,              /* AT24C64页大小32字节 */
    .sector_size = 1,               /* EEPROM无扇区概念 */
    
    .init        = at24cxx_port_init,
    .read        = at24cxx_port_read,
    .write       = at24cxx_port_write,
    .erase       = NULL,            /* EEPROM不需要擦除 */
    .is_ready    = at24cxx_port_ready,
};

#endif /* USE_AT24CXX_STORAGE */

/*============================================================================
 * 方案2: W25Qxx SPI Flash 驱动移植
 *============================================================================*/

#ifdef USE_W25QXX_STORAGE

/* 
 * W25Qxx需要先擦除再写入
 * 建议配合LittleFS使用，但这里提供直接操作的简单实现
 */

static int w25qxx_port_init(void)
{
    /* 初始化SPI和W25Qxx */
    extern void W25QXX_Init(void);
    W25QXX_Init();
    return 0;
}

static int w25qxx_port_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    extern void W25QXX_Read(uint8_t *buf, uint32_t addr, uint16_t len);
    W25QXX_Read(buf, addr, (uint16_t)len);
    return 0;
}

static int w25qxx_port_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    /* 注意: 写入前需要确保已擦除，否则只能将1变为0 */
    extern void W25QXX_Write_NoCheck(uint8_t *buf, uint32_t addr, uint16_t len);
    W25QXX_Write_NoCheck((uint8_t*)buf, addr, (uint16_t)len);
    return 0;
}

static int w25qxx_port_erase(uint32_t addr, uint32_t len)
{
    extern void W25QXX_Erase_Sector(uint32_t sector);
    
    #define W25QXX_SECTOR_SIZE  4096
    
    /* 计算需要擦除的扇区 */
    uint32_t start_sector = addr / W25QXX_SECTOR_SIZE;
    uint32_t end_sector = (addr + len - 1) / W25QXX_SECTOR_SIZE;
    
    for (uint32_t s = start_sector; s <= end_sector; s++) {
        W25QXX_Erase_Sector(s);
    }
    
    return 0;
}

static bool w25qxx_port_ready(void)
{
    extern uint16_t W25QXX_ReadID(void);
    uint16_t id = W25QXX_ReadID();
    return (id != 0xFFFF && id != 0x0000);
}

/* W25Qxx驱动定义 */
const StorageDriver_t g_w25qxx_driver = {
    .name        = "W25Q64",
    .base_addr   = 0x00100000,      /* 使用1MB偏移处，避开程序区 */
    .total_size  = 0x00100000,      /* 分配1MB给校准数据 */
    .page_size   = 256,             /* W25Qxx页大小256字节 */
    .sector_size = 4096,            /* 扇区4KB */
    
    .init        = w25qxx_port_init,
    .read        = w25qxx_port_read,
    .write       = w25qxx_port_write,
    .erase       = w25qxx_port_erase,
    .is_ready    = w25qxx_port_ready,
};

#endif /* USE_W25QXX_STORAGE */

/*============================================================================
 * 方案3: STM32内部Flash驱动移植
 *============================================================================*/

#ifdef USE_INTERNAL_FLASH_STORAGE

/* 
 * 内部Flash写入前需要擦除整个扇区/页
 * 建议使用RAM缓冲区管理
 */

#include "stm32f1xx_hal.h"  /* 根据你的芯片修改 */

/* 配置 - 根据芯片型号调整 */
#define FLASH_PAGE_SIZE     1024    /* STM32F103: 1KB/page */
#define FLASH_CALIB_ADDR    0x0800F800  /* 使用最后2KB */
#define FLASH_CALIB_SIZE    2048

static int internal_flash_port_init(void)
{
    return 0;  /* 内部Flash无需初始化 */
}

static int internal_flash_port_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    /* 直接内存读取 */
    memcpy(buf, (void*)addr, len);
    return 0;
}

static int internal_flash_port_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    HAL_StatusTypeDef status;
    
    /* 解锁Flash */
    HAL_FLASH_Unlock();
    
    /* 按半字(16bit)写入 - STM32F1 */
    for (uint32_t i = 0; i < len; i += 2) {
        uint16_t half_word = buf[i];
        if (i + 1 < len) {
            half_word |= (buf[i + 1] << 8);
        } else {
            half_word |= 0xFF00;  /* 填充 */
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

static int internal_flash_port_erase(uint32_t addr, uint32_t len)
{
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;
    
    HAL_FLASH_Unlock();
    
    /* 计算需要擦除的页数 */
    uint32_t start_page = (addr - 0x08000000) / FLASH_PAGE_SIZE;
    uint32_t num_pages = (len + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
    
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = 0x08000000 + start_page * FLASH_PAGE_SIZE;
    erase_init.NbPages = num_pages;
    
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    
    HAL_FLASH_Lock();
    
    return (status == HAL_OK) ? 0 : -1;
}

static bool internal_flash_port_ready(void)
{
    return true;  /* 内部Flash始终就绪 */
}

/* 内部Flash驱动定义 */
const StorageDriver_t g_internal_flash_driver = {
    .name        = "Internal Flash",
    .base_addr   = FLASH_CALIB_ADDR,
    .total_size  = FLASH_CALIB_SIZE,
    .page_size   = 2,               /* 半字写入 */
    .sector_size = FLASH_PAGE_SIZE,
    
    .init        = internal_flash_port_init,
    .read        = internal_flash_port_read,
    .write       = internal_flash_port_write,
    .erase       = internal_flash_port_erase,
    .is_ready    = internal_flash_port_ready,
};

#endif /* USE_INTERNAL_FLASH_STORAGE */

/*============================================================================
 * 方案4: LittleFS + SPI Flash (推荐用于复杂应用)
 *============================================================================*/

#ifdef USE_LITTLEFS_STORAGE

#include "lfs.h"

/* LittleFS实例 */
static lfs_t s_lfs;
static bool s_lfs_mounted = false;

/* LittleFS配置 - 需要用户实现底层接口 */
extern int lfs_flash_read(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, void *buffer, lfs_size_t size);
extern int lfs_flash_prog(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, const void *buffer, lfs_size_t size);
extern int lfs_flash_erase(const struct lfs_config *c, lfs_block_t block);
extern int lfs_flash_sync(const struct lfs_config *c);

static uint8_t s_lfs_read_buf[256];
static uint8_t s_lfs_prog_buf[256];
static uint8_t s_lfs_lookahead_buf[16];

static const struct lfs_config s_lfs_cfg = {
    .read  = lfs_flash_read,
    .prog  = lfs_flash_prog,
    .erase = lfs_flash_erase,
    .sync  = lfs_flash_sync,
    
    .read_size      = 1,
    .prog_size      = 1,
    .block_size     = 4096,
    .block_count    = 256,          /* 1MB */
    .cache_size     = 256,
    .lookahead_size = 16,
    .block_cycles   = 500,
    
    .read_buffer      = s_lfs_read_buf,
    .prog_buffer      = s_lfs_prog_buf,
    .lookahead_buffer = s_lfs_lookahead_buf,
};

static int littlefs_port_init(void)
{
    int err = lfs_mount(&s_lfs, &s_lfs_cfg);
    if (err != LFS_ERR_OK) {
        /* 挂载失败，尝试格式化 */
        err = lfs_format(&s_lfs, &s_lfs_cfg);
        if (err != LFS_ERR_OK) return -1;
        
        err = lfs_mount(&s_lfs, &s_lfs_cfg);
        if (err != LFS_ERR_OK) return -1;
    }
    
    s_lfs_mounted = true;
    return 0;
}

/* LittleFS使用文件方式存储，这里简化为块设备方式 */
/* 实际应用中建议使用文件操作 */

static int littlefs_port_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    /* 直接读取底层Flash */
    return lfs_flash_read(&s_lfs_cfg, addr / 4096, addr % 4096, buf, len);
}

static int littlefs_port_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    return lfs_flash_prog(&s_lfs_cfg, addr / 4096, addr % 4096, buf, len);
}

static int littlefs_port_erase(uint32_t addr, uint32_t len)
{
    uint32_t start_block = addr / 4096;
    uint32_t end_block = (addr + len - 1) / 4096;
    
    for (uint32_t b = start_block; b <= end_block; b++) {
        if (lfs_flash_erase(&s_lfs_cfg, b) != 0) return -1;
    }
    return 0;
}

static bool littlefs_port_ready(void)
{
    return s_lfs_mounted;
}

/* LittleFS驱动定义 */
const StorageDriver_t g_littlefs_driver = {
    .name        = "LittleFS",
    .base_addr   = 0,
    .total_size  = 256 * 4096,      /* 1MB */
    .page_size   = 256,
    .sector_size = 4096,
    
    .init        = littlefs_port_init,
    .read        = littlefs_port_read,
    .write       = littlefs_port_write,
    .erase       = littlefs_port_erase,
    .is_ready    = littlefs_port_ready,
};

#endif /* USE_LITTLEFS_STORAGE */

/*============================================================================
 * 使用示例
 *============================================================================*/

/*
void Calib_Storage_Setup(void)
{
    // 选择一个驱动注册
    #if defined(USE_AT24CXX_STORAGE)
        Storage_RegisterDriver(&g_at24cxx_driver);
    #elif defined(USE_W25QXX_STORAGE)
        Storage_RegisterDriver(&g_w25qxx_driver);
    #elif defined(USE_INTERNAL_FLASH_STORAGE)
        Storage_RegisterDriver(&g_internal_flash_driver);
    #elif defined(USE_LITTLEFS_STORAGE)
        Storage_RegisterDriver(&g_littlefs_driver);
    #endif
    
    // 初始化
    Storage_Init();
}
*/
