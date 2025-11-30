/**
 * @file    CH_Calib.c
 * @brief   多通道多项式校准系统实现
 * @version 2.0
 */

#include "CH_Calib.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 * 全局变量
 *============================================================================*/

CalibCh_t g_calib_ch[CALIB_MAX_CHANNELS];
uint8_t   g_calib_ch_cnt = 0;

/*============================================================================
 * 私有变量
 *============================================================================*/

/* 每通道存储占用空间 */
#define CALIB_DATA_SIZE     sizeof(CalibData_t)

/* 错误描述 */
static const char *s_err_str[] = {
    "OK", "Param", "ChID", "Storage", "CRC", "NoData", "Parse", "Overflow"
};

/* CRC16查表 (MODBUS) */
static const uint16_t s_crc16_tab[256] = {
    0x0000,0xC0C1,0xC181,0x0140,0xC301,0x03C0,0x0280,0xC241,
    0xC601,0x06C0,0x0780,0xC741,0x0500,0xC5C1,0xC481,0x0440,
    0xCC01,0x0CC0,0x0D80,0xCD41,0x0F00,0xCFC1,0xCE81,0x0E40,
    0x0A00,0xCAC1,0xCB81,0x0B40,0xC901,0x09C0,0x0880,0xC841,
    0xD801,0x18C0,0x1980,0xD941,0x1B00,0xDBC1,0xDA81,0x1A40,
    0x1E00,0xDEC1,0xDF81,0x1F40,0xDD01,0x1DC0,0x1C80,0xDC41,
    0x1400,0xD4C1,0xD581,0x1540,0xD701,0x17C0,0x1680,0xD641,
    0xD201,0x12C0,0x1380,0xD341,0x1100,0xD1C1,0xD081,0x1040,
    0xF001,0x30C0,0x3180,0xF141,0x3300,0xF3C1,0xF281,0x3240,
    0x3600,0xF6C1,0xF781,0x3740,0xF501,0x35C0,0x3480,0xF441,
    0x3C00,0xFCC1,0xFD81,0x3D40,0xFF01,0x3FC0,0x3E80,0xFE41,
    0xFA01,0x3AC0,0x3B80,0xFB41,0x3900,0xF9C1,0xF881,0x3840,
    0x2800,0xE8C1,0xE981,0x2940,0xEB01,0x2BC0,0x2A80,0xEA41,
    0xEE01,0x2EC0,0x2F80,0xEF41,0x2D00,0xEDC1,0xEC81,0x2C40,
    0xE401,0x24C0,0x2580,0xE541,0x2700,0xE7C1,0xE681,0x2640,
    0x2200,0xE2C1,0xE381,0x2340,0xE101,0x21C0,0x2080,0xE041,
    0xA001,0x60C0,0x6180,0xA141,0x6300,0xA3C1,0xA281,0x6240,
    0x6600,0xA6C1,0xA781,0x6740,0xA501,0x65C0,0x6480,0xA441,
    0x6C00,0xACC1,0xAD81,0x6D40,0xAF01,0x6FC0,0x6E80,0xAE41,
    0xAA01,0x6AC0,0x6B80,0xAB41,0x6900,0xA9C1,0xA881,0x6840,
    0x7800,0xB8C1,0xB981,0x7940,0xBB01,0x7BC0,0x7A80,0xBA41,
    0xBE01,0x7EC0,0x7F80,0xBF41,0x7D00,0xBDC1,0xBC81,0x7C40,
    0xB401,0x74C0,0x7580,0xB541,0x7700,0xB7C1,0xB681,0x7640,
    0x7200,0xB2C1,0xB381,0x7340,0xB101,0x71C0,0x7080,0xB041,
    0x5000,0x90C1,0x9181,0x5140,0x9301,0x53C0,0x5280,0x9241,
    0x9601,0x56C0,0x5780,0x9741,0x5500,0x95C1,0x9481,0x5440,
    0x9C01,0x5CC0,0x5D80,0x9D41,0x5F00,0x9FC1,0x9E81,0x5E40,
    0x5A00,0x9AC1,0x9B81,0x5B40,0x9901,0x59C0,0x5880,0x9841,
    0x8801,0x48C0,0x4980,0x8941,0x4B00,0x8BC1,0x8A81,0x4A40,
    0x4E00,0x8EC1,0x8F81,0x4F40,0x8D01,0x4DC0,0x4C80,0x8C41,
    0x4400,0x84C1,0x8581,0x4540,0x8701,0x47C0,0x4680,0x8641,
    0x8201,0x42C0,0x4380,0x8341,0x4100,0x81C1,0x8081,0x4040,
};

/*============================================================================
 * 私有函数
 *============================================================================*/

static CalibCh_t* find_ch(uint8_t id)
{
    for (uint8_t i = 0; i < g_calib_ch_cnt; i++) {
        if (g_calib_ch[i].id == id) return &g_calib_ch[i];
    }
    return NULL;
}

static uint32_t get_ch_addr(uint8_t id)
{
    return (id - 1) * CALIB_DATA_SIZE;
}

/*============================================================================
 * CRC16
 *============================================================================*/

uint16_t Calib_CRC16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc = (crc >> 8) ^ s_crc16_tab[(crc ^ *data++) & 0xFF];
    }
    return crc;
}

/*============================================================================
 * 核心API实现
 *============================================================================*/

CalibErr_t Calib_Init(void)
{
    memset(g_calib_ch, 0, sizeof(g_calib_ch));
    g_calib_ch_cnt = 0;
    
    /* 初始化存储 */
    if (Storage_Init() != STORAGE_OK) {
        return CALIB_ERR_STORAGE;
    }
    
    return CALIB_OK;
}

CalibErr_t Calib_RegisterCh(uint8_t id, const char *name)
{
    if (id == 0 || id > CALIB_MAX_CHANNELS || name == NULL) {
        return CALIB_ERR_PARAM;
    }
    
    /* 检查是否已存在 */
    CalibCh_t *ch = find_ch(id);
    if (ch != NULL) {
        /* 更新名称 */
        strncpy(ch->name, name, CALIB_NAME_MAX_LEN - 1);
        return CALIB_OK;
    }
    
    if (g_calib_ch_cnt >= CALIB_MAX_CHANNELS) {
        return CALIB_ERR_OVERFLOW;
    }
    
    ch = &g_calib_ch[g_calib_ch_cnt++];
    ch->id = id;
    strncpy(ch->name, name, CALIB_NAME_MAX_LEN - 1);
    ch->name[CALIB_NAME_MAX_LEN - 1] = '\0';
    ch->coeff_cnt = 0;
    ch->valid = false;
    
    return CALIB_OK;
}

CalibErr_t Calib_SetCoeffs(uint8_t id, const double *coeffs, uint8_t cnt)
{
    if (coeffs == NULL || cnt == 0 || cnt > CALIB_MAX_COEFFS) {
        return CALIB_ERR_PARAM;
    }
    
    CalibCh_t *ch = find_ch(id);
    if (ch == NULL) return CALIB_ERR_CH_ID;
    
    memcpy(ch->coeffs, coeffs, cnt * sizeof(double));
    ch->coeff_cnt = cnt;
    ch->valid = true;
    
    return CALIB_OK;
}

CalibErr_t Calib_GetCoeffs(uint8_t id, double *coeffs, uint8_t *cnt)
{
    if (coeffs == NULL || cnt == NULL) return CALIB_ERR_PARAM;
    
    CalibCh_t *ch = find_ch(id);
    if (ch == NULL) return CALIB_ERR_CH_ID;
    if (!ch->valid) return CALIB_ERR_NO_DATA;
    
    memcpy(coeffs, ch->coeffs, ch->coeff_cnt * sizeof(double));
    *cnt = ch->coeff_cnt;
    
    return CALIB_OK;
}

CalibErr_t Calib_Apply(uint8_t id, double x, double *y)
{
    if (y == NULL) return CALIB_ERR_PARAM;
    
    CalibCh_t *ch = find_ch(id);
    if (ch == NULL) return CALIB_ERR_CH_ID;
    
    if (!ch->valid || ch->coeff_cnt == 0) {
        *y = x;
        return CALIB_ERR_NO_DATA;
    }
    
    /* Horner法则 */
    *y = Calib_Compute(ch->coeffs, ch->coeff_cnt, x);
    return CALIB_OK;
}

CalibErr_t Calib_ApplyBatch(uint8_t id, const double *x, double *y, uint16_t cnt)
{
    if (x == NULL || y == NULL || cnt == 0) return CALIB_ERR_PARAM;
    
    CalibCh_t *ch = find_ch(id);
    if (ch == NULL) return CALIB_ERR_CH_ID;
    
    if (!ch->valid || ch->coeff_cnt == 0) {
        memcpy(y, x, cnt * sizeof(double));
        return CALIB_ERR_NO_DATA;
    }
    
    const double *c = ch->coeffs;
    const uint8_t n = ch->coeff_cnt;
    
    for (uint16_t i = 0; i < cnt; i++) {
        y[i] = Calib_Compute(c, n, x[i]);
    }
    
    return CALIB_OK;
}

/*============================================================================
 * 存储API实现
 *============================================================================*/

CalibErr_t Calib_Save(uint8_t id)
{
    CalibCh_t *ch = find_ch(id);
    if (ch == NULL) return CALIB_ERR_CH_ID;
    
    CalibData_t data = {0};
    data.magic = CALIB_MAGIC;
    data.ch_id = id;
    data.coeff_cnt = ch->coeff_cnt;
    memcpy(data.coeffs, ch->coeffs, ch->coeff_cnt * sizeof(double));
    
    /* 计算CRC (不含CRC字段) */
    data.crc16 = Calib_CRC16((uint8_t*)&data, sizeof(CalibData_t) - 2);
    
    /* 写入存储 */
    if (Storage_WriteVerify(get_ch_addr(id), (uint8_t*)&data, sizeof(CalibData_t)) != STORAGE_OK) {
        return CALIB_ERR_STORAGE;
    }
    
    return CALIB_OK;
}

CalibErr_t Calib_Load(uint8_t id)
{
    CalibCh_t *ch = find_ch(id);
    if (ch == NULL) return CALIB_ERR_CH_ID;
    
    CalibData_t data;
    
    /* 读取 */
    if (Storage_Read(get_ch_addr(id), (uint8_t*)&data, sizeof(CalibData_t)) != STORAGE_OK) {
        return CALIB_ERR_STORAGE;
    }
    
    /* 验证魔数 */
    if (data.magic != CALIB_MAGIC) {
        return CALIB_ERR_NO_DATA;
    }
    
    /* 验证CRC */
    uint16_t crc = Calib_CRC16((uint8_t*)&data, sizeof(CalibData_t) - 2);
    if (crc != data.crc16) {
        return CALIB_ERR_CRC;
    }
    
    /* 验证数据 */
    if (data.coeff_cnt == 0 || data.coeff_cnt > CALIB_MAX_COEFFS) {
        return CALIB_ERR_NO_DATA;
    }
    
    /* 应用 */
    ch->coeff_cnt = data.coeff_cnt;
    memcpy(ch->coeffs, data.coeffs, data.coeff_cnt * sizeof(double));
    ch->valid = true;
    
    return CALIB_OK;
}

CalibErr_t Calib_SaveAll(void)
{
    CalibErr_t ret = CALIB_OK;
    for (uint8_t i = 0; i < g_calib_ch_cnt; i++) {
        CalibErr_t err = Calib_Save(g_calib_ch[i].id);
        if (err != CALIB_OK) ret = err;
    }
    return ret;
}

CalibErr_t Calib_LoadAll(void)
{
    CalibErr_t ret = CALIB_OK;
    for (uint8_t i = 0; i < g_calib_ch_cnt; i++) {
        CalibErr_t err = Calib_Load(g_calib_ch[i].id);
        if (err != CALIB_OK && err != CALIB_ERR_NO_DATA) ret = err;
    }
    return ret;
}

/*============================================================================
 * 协议API实现
 *============================================================================*/

int Calib_PackChList(char *buf, uint32_t size)
{
    if (buf == NULL || size == 0) return -1;
    
    int len = snprintf(buf, size, "CH:");
    for (uint8_t i = 0; i < g_calib_ch_cnt && (uint32_t)len < size; i++) {
        if (i > 0) len += snprintf(buf + len, size - len, ",");
        len += snprintf(buf + len, size - len, "%u:%s", 
                       g_calib_ch[i].id, g_calib_ch[i].name);
    }
    return len;
}

int Calib_PackCoeffs(uint8_t id, uint8_t *buf, uint32_t size)
{
    CalibCh_t *ch = find_ch(id);
    if (ch == NULL || buf == NULL) return -1;
    
    /* 格式: AA + "Coef" + len + ch_id + coeff_cnt + coeffs + CRC16 */
    uint32_t total = 1 + 4 + 1 + 1 + 1 + ch->coeff_cnt * 8 + 2;
    if (size < total) return -1;
    
    int idx = 0;
    buf[idx++] = CALIB_FRAME_HEADER;
    buf[idx++] = 'C'; buf[idx++] = 'o'; buf[idx++] = 'e'; buf[idx++] = 'f';
    buf[idx++] = (uint8_t)total;
    buf[idx++] = id;
    buf[idx++] = ch->coeff_cnt;
    
    for (uint8_t i = 0; i < ch->coeff_cnt; i++) {
        memcpy(&buf[idx], &ch->coeffs[i], 8);
        idx += 8;
    }
    
    uint16_t crc = Calib_CRC16(&buf[1], idx - 1);
    buf[idx++] = crc & 0xFF;
    buf[idx++] = (crc >> 8) & 0xFF;
    
    return idx;
}

CalibErr_t Calib_ProcessFrame(const uint8_t *data, uint16_t len,
                               uint8_t *resp, uint16_t *resp_len)
{
    if (data == NULL || len < 8) return CALIB_ERR_PARAM;
    if (data[0] != CALIB_FRAME_HEADER) return CALIB_ERR_PARSE;
    
    const char *cmd = (const char*)&data[1];
    
    /* 写入系数: AA + "CalCh" + len + ch_id + coeffs + CRC */
    if (memcmp(cmd, "CalCh", 5) == 0 && len >= 10) {
        uint8_t frame_len = data[6];
        if (len < frame_len) return CALIB_ERR_PARSE;
        
        /* 验证CRC */
        uint16_t recv_crc = data[frame_len - 2] | (data[frame_len - 1] << 8);
        uint16_t calc_crc = Calib_CRC16(&data[1], frame_len - 3);
        if (recv_crc != calc_crc) return CALIB_ERR_CRC;
        
        uint8_t ch_id = data[7];
        uint8_t coeff_cnt = (frame_len - 10) / 8;
        
        if (coeff_cnt > CALIB_MAX_COEFFS) return CALIB_ERR_OVERFLOW;
        
        double coeffs[CALIB_MAX_COEFFS];
        for (uint8_t i = 0; i < coeff_cnt; i++) {
            memcpy(&coeffs[i], &data[8 + i * 8], 8);
        }
        
        CalibErr_t err = Calib_SetCoeffs(ch_id, coeffs, coeff_cnt);
        if (err != CALIB_OK) return err;
        
        err = Calib_Save(ch_id);
        
        if (resp && resp_len) {
            *resp_len = snprintf((char*)resp, 64, "OK:CH%u,%u coeffs\r\n", ch_id, coeff_cnt);
        }
        return err;
    }
    
    /* 读取通道列表: AA + "RBCalCh" */
    if (memcmp(cmd, "RBCalCh", 7) == 0) {
        if (resp && resp_len) {
            int ret = Calib_PackChList((char*)resp, 256);
            if (ret > 0) {
                resp[ret++] = '\r';
                resp[ret++] = '\n';
                *resp_len = ret;
            }
        }
        return CALIB_OK;
    }
    
    /* 读取系数: AA + "RdCoef" + ch_id */
    if (memcmp(cmd, "RdCoef", 6) == 0 && len >= 8) {
        uint8_t ch_id = data[7];
        if (resp && resp_len) {
            int ret = Calib_PackCoeffs(ch_id, resp, 256);
            if (ret > 0) *resp_len = ret;
        }
        return CALIB_OK;
    }
    
    return CALIB_ERR_PARSE;
}

/*============================================================================
 * 工具函数
 *============================================================================*/

void Calib_PrintInfo(void)
{
    const StorageDriver_t *drv = Storage_GetDriver();
    
    printf("\n===== Calib System =====\n");
    printf("Storage: %s\n", drv ? drv->name : "None");
    printf("Channels: %u\n", g_calib_ch_cnt);
    printf("------------------------\n");
    
    for (uint8_t i = 0; i < g_calib_ch_cnt; i++) {
        CalibCh_t *ch = &g_calib_ch[i];
        printf("[%u] %s: ", ch->id, ch->name);
        
        if (ch->valid) {
            printf("y = ");
            for (int j = ch->coeff_cnt - 1; j >= 0; j--) {
                if (j < (int)ch->coeff_cnt - 1) printf(" + ");
                printf("%.4e", ch->coeffs[j]);
                if (j > 0) printf("*x^%d", j);
            }
        } else {
            printf("(no data)");
        }
        printf("\n");
    }
    printf("========================\n");
}

const char* Calib_ErrStr(CalibErr_t err)
{
    if (err < sizeof(s_err_str) / sizeof(s_err_str[0])) {
        return s_err_str[err];
    }
    return "Unknown";
}
