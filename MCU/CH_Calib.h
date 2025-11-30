/**
 * @file    CH_Calib.h
 * @brief   多通道多项式校准系统
 * @version 2.0
 * 
 * 特点:
 * - 使用存储抽象层，支持多种存储后端
 * - Horner法则高效计算
 * - CRC16数据校验
 * - 完善的通信协议
 */

#ifndef __CH_CALIB_H
#define __CH_CALIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "Calib_Storage.h"

/*============================================================================
 * 配置
 *============================================================================*/

#define CALIB_MAX_CHANNELS      8       /* 最大通道数 */
#define CALIB_MAX_COEFFS        6       /* 最大系数数 (最高5次多项式) */
#define CALIB_NAME_MAX_LEN      16      /* 通道名称最大长度 */
#define CALIB_MAGIC             0xCA1B  /* 数据有效标识 */

/* 协议配置 */
#define CALIB_FRAME_HEADER      0xAA
#define CALIB_CMD_WRITE         "CalCh"     /* 写入系数 */
#define CALIB_CMD_READ_LIST     "RBCalCh"   /* 读取通道列表 */
#define CALIB_CMD_READ_COEF     "RdCoef"    /* 读取系数 */

/*============================================================================
 * 错误码
 *============================================================================*/

typedef enum {
    CALIB_OK = 0,
    CALIB_ERR_PARAM,        /* 参数错误 */
    CALIB_ERR_CH_ID,        /* 通道ID无效 */
    CALIB_ERR_STORAGE,      /* 存储错误 */
    CALIB_ERR_CRC,          /* CRC校验失败 */
    CALIB_ERR_NO_DATA,      /* 无有效数据 */
    CALIB_ERR_PARSE,        /* 解析错误 */
    CALIB_ERR_OVERFLOW,     /* 溢出 */
} CalibErr_t;

/*============================================================================
 * 数据结构
 *============================================================================*/

/* 存储数据结构 (56字节) */
typedef struct __attribute__((packed)) {
    uint16_t magic;                     /* 魔数 0xCA1B */
    uint8_t  ch_id;                     /* 通道ID */
    uint8_t  coeff_cnt;                 /* 系数数量 */
    double   coeffs[CALIB_MAX_COEFFS];  /* 系数 [a0,a1,a2,...] */
    uint16_t crc16;                     /* CRC校验 */
} CalibData_t;

/* 通道运行时信息 */
typedef struct {
    uint8_t  id;                        /* 通道ID (1-based) */
    char     name[CALIB_NAME_MAX_LEN];  /* 名称 */
    uint8_t  coeff_cnt;                 /* 系数数量 */
    double   coeffs[CALIB_MAX_COEFFS];  /* 系数 */
    bool     valid;                     /* 数据有效 */
} CalibCh_t;

/*============================================================================
 * 全局变量
 *============================================================================*/

extern CalibCh_t g_calib_ch[CALIB_MAX_CHANNELS];
extern uint8_t   g_calib_ch_cnt;

/*============================================================================
 * 核心API
 *============================================================================*/

/**
 * @brief  初始化校准系统
 * @note   调用前需先注册存储驱动
 */
CalibErr_t Calib_Init(void);

/**
 * @brief  注册通道
 * @param  id: 通道ID (1~CALIB_MAX_CHANNELS)
 * @param  name: 通道名称
 */
CalibErr_t Calib_RegisterCh(uint8_t id, const char *name);

/**
 * @brief  设置通道系数
 * @param  id: 通道ID
 * @param  coeffs: 系数数组 [a0, a1, a2, ...]
 * @param  cnt: 系数数量
 */
CalibErr_t Calib_SetCoeffs(uint8_t id, const double *coeffs, uint8_t cnt);

/**
 * @brief  获取通道系数
 */
CalibErr_t Calib_GetCoeffs(uint8_t id, double *coeffs, uint8_t *cnt);

/**
 * @brief  应用校准计算 (Horner法则)
 * @param  id: 通道ID
 * @param  x: ADC原始值
 * @param  y: 输出校准值
 */
CalibErr_t Calib_Apply(uint8_t id, double x, double *y);

/**
 * @brief  批量校准计算
 */
CalibErr_t Calib_ApplyBatch(uint8_t id, const double *x, double *y, uint16_t cnt);

/**
 * @brief  内联校准计算 (用于高性能场景)
 * @note   直接传入系数指针，避免查找开销
 */
static inline double Calib_Compute(const double *coeffs, uint8_t cnt, double x)
{
    if (cnt == 0) return x;
    double y = coeffs[cnt - 1];
    for (int i = cnt - 2; i >= 0; i--) {
        y = y * x + coeffs[i];
    }
    return y;
}

/*============================================================================
 * 存储API
 *============================================================================*/

/**
 * @brief  保存单通道数据
 */
CalibErr_t Calib_Save(uint8_t id);

/**
 * @brief  加载单通道数据
 */
CalibErr_t Calib_Load(uint8_t id);

/**
 * @brief  保存所有通道
 */
CalibErr_t Calib_SaveAll(void);

/**
 * @brief  加载所有通道
 */
CalibErr_t Calib_LoadAll(void);

/*============================================================================
 * 协议API
 *============================================================================*/

/**
 * @brief  处理接收到的数据帧
 * @param  data: 接收数据
 * @param  len: 数据长度
 * @param  resp: 响应缓冲区
 * @param  resp_len: 响应长度
 */
CalibErr_t Calib_ProcessFrame(const uint8_t *data, uint16_t len,
                               uint8_t *resp, uint16_t *resp_len);

/**
 * @brief  打包通道列表
 * @return 实际长度，-1失败
 */
int Calib_PackChList(char *buf, uint32_t size);

/**
 * @brief  打包系数响应
 */
int Calib_PackCoeffs(uint8_t id, uint8_t *buf, uint32_t size);

/*============================================================================
 * 工具函数
 *============================================================================*/

/**
 * @brief  CRC16-MODBUS计算
 */
uint16_t Calib_CRC16(const uint8_t *data, uint16_t len);

/**
 * @brief  打印调试信息
 */
void Calib_PrintInfo(void);

/**
 * @brief  获取错误描述
 */
const char* Calib_ErrStr(CalibErr_t err);

#ifdef __cplusplus
}
#endif

#endif /* __CH_CALIB_H */
