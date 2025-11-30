/**
 * @file    Calib_Storage.c
 * @brief   校准数据存储驱动抽象层实现
 * @version 2.0
 */

#include "Calib_Storage.h"
#include <string.h>

/*============================================================================
 * 私有变量
 *============================================================================*/

static const StorageDriver_t *s_driver = NULL;
static bool s_initialized = false;

/*============================================================================
 * API实现
 *============================================================================*/

StorageError_t Storage_RegisterDriver(const StorageDriver_t *driver)
{
    if (driver == NULL) {
        return STORAGE_ERR_PARAM;
    }
    
    /* 验证必须的函数指针 */
    if (driver->read == NULL || driver->write == NULL) {
        return STORAGE_ERR_PARAM;
    }
    
    s_driver = driver;
    s_initialized = false;
    
    return STORAGE_OK;
}

const StorageDriver_t* Storage_GetDriver(void)
{
    return s_driver;
}

StorageError_t Storage_Init(void)
{
    if (s_driver == NULL) {
        return STORAGE_ERR_NO_DRIVER;
    }
    
    /* 调用驱动初始化 (如果提供) */
    if (s_driver->init != NULL) {
        if (s_driver->init() != 0) {
            return STORAGE_ERR_NOT_INIT;
        }
    }
    
    s_initialized = true;
    return STORAGE_OK;
}

StorageError_t Storage_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    if (s_driver == NULL) {
        return STORAGE_ERR_NO_DRIVER;
    }
    
    if (buf == NULL || len == 0) {
        return STORAGE_ERR_PARAM;
    }
    
    /* 检查地址范围 */
    if (addr + len > s_driver->total_size) {
        return STORAGE_ERR_PARAM;
    }
    
    /* 计算实际地址 */
    uint32_t real_addr = s_driver->base_addr + addr;
    
    /* 调用驱动读取 */
    if (s_driver->read(real_addr, buf, len) != 0) {
        return STORAGE_ERR_READ;
    }
    
    return STORAGE_OK;
}

StorageError_t Storage_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    if (s_driver == NULL) {
        return STORAGE_ERR_NO_DRIVER;
    }
    
    if (buf == NULL || len == 0) {
        return STORAGE_ERR_PARAM;
    }
    
    /* 检查地址范围 */
    if (addr + len > s_driver->total_size) {
        return STORAGE_ERR_PARAM;
    }
    
    /* 计算实际地址 */
    uint32_t real_addr = s_driver->base_addr + addr;
    
    /* 调用驱动写入 */
    if (s_driver->write(real_addr, buf, len) != 0) {
        return STORAGE_ERR_WRITE;
    }
    
    return STORAGE_OK;
}

StorageError_t Storage_Erase(uint32_t addr, uint32_t len)
{
    if (s_driver == NULL) {
        return STORAGE_ERR_NO_DRIVER;
    }
    
    /* 擦除是可选的 (EEPROM不需要显式擦除) */
    if (s_driver->erase == NULL) {
        return STORAGE_OK;
    }
    
    /* 检查地址范围 */
    if (addr + len > s_driver->total_size) {
        return STORAGE_ERR_PARAM;
    }
    
    /* 计算实际地址 */
    uint32_t real_addr = s_driver->base_addr + addr;
    
    /* 调用驱动擦除 */
    if (s_driver->erase(real_addr, len) != 0) {
        return STORAGE_ERR_ERASE;
    }
    
    return STORAGE_OK;
}

StorageError_t Storage_WriteVerify(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    StorageError_t err;
    
    /* 写入 */
    err = Storage_Write(addr, buf, len);
    if (err != STORAGE_OK) {
        return err;
    }
    
    /* 回读验证 - 使用小缓冲区分块验证以节省RAM */
    #define VERIFY_BUF_SIZE 32
    uint8_t verify_buf[VERIFY_BUF_SIZE];
    uint32_t offset = 0;
    
    while (offset < len) {
        uint32_t chunk = (len - offset) > VERIFY_BUF_SIZE ? VERIFY_BUF_SIZE : (len - offset);
        
        err = Storage_Read(addr + offset, verify_buf, chunk);
        if (err != STORAGE_OK) {
            return err;
        }
        
        if (memcmp(buf + offset, verify_buf, chunk) != 0) {
            return STORAGE_ERR_VERIFY;
        }
        
        offset += chunk;
    }
    
    return STORAGE_OK;
}

bool Storage_IsReady(void)
{
    if (s_driver == NULL) {
        return false;
    }
    
    if (s_driver->is_ready != NULL) {
        return s_driver->is_ready();
    }
    
    return s_initialized;
}
