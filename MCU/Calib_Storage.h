/**
 * @file    Calib_Storage.h
 * @brief   校准数据存储驱动抽象层 - 函数指针实现
 * @version 2.0
 * 
 * 特点:
 * - 使用函数指针实现驱动抽象，便于移植
 * - 支持多种存储后端: 内部Flash, AT24CXX, SPI Flash
 * - 用户只需实现底层读写函数即可
 */

#ifndef __CALIB_STORAGE_H
#define __CALIB_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*============================================================================
 * 错误码定义
 *============================================================================*/
typedef enum {
    STORAGE_OK = 0,
    STORAGE_ERR_PARAM,      /* 参数错误 */
    STORAGE_ERR_BUSY,       /* 设备忙 */
    STORAGE_ERR_TIMEOUT,    /* 超时 */
    STORAGE_ERR_WRITE,      /* 写入失败 */
    STORAGE_ERR_READ,       /* 读取失败 */
    STORAGE_ERR_ERASE,      /* 擦除失败 */
    STORAGE_ERR_VERIFY,     /* 校验失败 */
    STORAGE_ERR_NOT_INIT,   /* 未初始化 */
    STORAGE_ERR_NO_DRIVER,  /* 无驱动 */
} StorageError_t;

/*============================================================================
 * 函数指针类型定义 - 用户需要实现这些函数
 *============================================================================*/

/**
 * @brief  初始化函数指针类型
 * @return 0成功，非0失败
 */
typedef int (*StorageInit_t)(void);

/**
 * @brief  读取函数指针类型
 * @param  addr: 起始地址
 * @param  buf: 数据缓冲区
 * @param  len: 数据长度
 * @return 0成功，非0失败
 */
typedef int (*StorageRead_t)(uint32_t addr, uint8_t *buf, uint32_t len);

/**
 * @brief  写入函数指针类型
 * @param  addr: 起始地址
 * @param  buf: 数据缓冲区
 * @param  len: 数据长度
 * @return 0成功，非0失败
 */
typedef int (*StorageWrite_t)(uint32_t addr, const uint8_t *buf, uint32_t len);

/**
 * @brief  擦除函数指针类型
 * @param  addr: 起始地址
 * @param  len: 擦除长度
 * @return 0成功，非0失败
 */
typedef int (*StorageErase_t)(uint32_t addr, uint32_t len);

/**
 * @brief  检查设备是否就绪
 * @return true就绪，false未就绪
 */
typedef bool (*StorageReady_t)(void);

/*============================================================================
 * 存储驱动结构体
 *============================================================================*/

typedef struct {
    const char      *name;          /* 驱动名称 (如 "AT24C64", "W25Q64") */
    uint32_t        base_addr;      /* 基础地址 (校准数据存储起始位置) */
    uint32_t        total_size;     /* 可用总大小 */
    uint32_t        page_size;      /* 页大小 (写入对齐) */
    uint32_t        sector_size;    /* 扇区大小 (擦除对齐, EEPROM可设为1) */
    
    /* 函数指针 - 用户需要实现 */
    StorageInit_t   init;           /* 初始化 (可选, 可为NULL) */
    StorageRead_t   read;           /* 读取 (必须) */
    StorageWrite_t  write;          /* 写入 (必须) */
    StorageErase_t  erase;          /* 擦除 (可选, EEPROM可为NULL) */
    StorageReady_t  is_ready;       /* 就绪检查 (可选) */
} StorageDriver_t;

/*============================================================================
 * API函数
 *============================================================================*/

/**
 * @brief  注册存储驱动
 * @param  driver: 驱动结构体指针
 * @return STORAGE_OK成功
 */
StorageError_t Storage_RegisterDriver(const StorageDriver_t *driver);

/**
 * @brief  获取当前驱动
 * @return 驱动指针，未注册返回NULL
 */
const StorageDriver_t* Storage_GetDriver(void);

/**
 * @brief  初始化存储
 * @return STORAGE_OK成功
 */
StorageError_t Storage_Init(void);

/**
 * @brief  读取数据
 * @param  addr: 相对地址 (自动加上base_addr)
 * @param  buf: 数据缓冲区
 * @param  len: 数据长度
 * @return STORAGE_OK成功
 */
StorageError_t Storage_Read(uint32_t addr, uint8_t *buf, uint32_t len);

/**
 * @brief  写入数据
 * @param  addr: 相对地址
 * @param  buf: 数据缓冲区
 * @param  len: 数据长度
 * @return STORAGE_OK成功
 */
StorageError_t Storage_Write(uint32_t addr, const uint8_t *buf, uint32_t len);

/**
 * @brief  擦除数据
 * @param  addr: 相对地址
 * @param  len: 擦除长度
 * @return STORAGE_OK成功
 */
StorageError_t Storage_Erase(uint32_t addr, uint32_t len);

/**
 * @brief  写入并验证
 * @param  addr: 相对地址
 * @param  buf: 数据缓冲区
 * @param  len: 数据长度
 * @return STORAGE_OK成功
 */
StorageError_t Storage_WriteVerify(uint32_t addr, const uint8_t *buf, uint32_t len);

/**
 * @brief  检查存储是否就绪
 * @return true就绪
 */
bool Storage_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* __CALIB_STORAGE_H */
