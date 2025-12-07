# ADC校准系统 MCU端完整手册

> 版本 2.0 | 支持HAL库与标准库 | 多种存储方案

---

## 目录

1. [系统概述](#1-系统概述)
2. [快速开始](#2-快速开始)
3. [API参考手册](#3-api参考手册)
4. [HAL库驱动移植](#4-hal库驱动移植)
5. [标准库驱动移植](#5-标准库驱动移植)
6. [校准公式应用](#6-校准公式应用)
7. [通信协议处理](#7-通信协议处理)
8. [性能优化技巧](#8-性能优化技巧)
9. [常见问题解答](#9-常见问题解答)

---

## 1. 系统概述

### 1.1 功能特点

本校准系统提供完整的多通道ADC校准解决方案：

- **存储驱动抽象层** - 函数指针实现，轻松适配不同存储硬件
- **多存储方案支持** - 内部Flash、AT24Cxx EEPROM、W25Qxx SPI Flash
- **Horner法则优化** - 高效多项式计算，减少乘法次数
- **CRC16数据校验** - 查表法实现，保障数据完整性
- **完善通信协议** - 与PC上位机无缝对接

### 1.2 文件结构

| 文件名 | 说明 |
|--------|------|
| `CH_Calib.h/c` | 校准系统核心，包含通道管理、系数计算、存储接口 |
| `Calib_Storage.h/c` | 存储驱动抽象层，提供统一的读写接口 |
| `Calib_Storage_Port.c` | 驱动移植示例，包含多种存储方案 |
| `Examples_HAL_StdLib.c/h` | 完整使用例程，HAL库与标准库示例 |

### 1.3 存储方案对比

| 存储类型 | 优点 | 缺点 | 适用场景 |
|----------|------|------|----------|
| AT24Cxx EEPROM | 字节擦写、寿命长、无需擦除 | 容量小、速度慢 | 小数据量、频繁更新 |
| W25Qxx Flash | 容量大、速度快 | 需先擦后写、扇区4KB | 大数据量、批量存储 |
| 内部Flash | 无需外部器件 | 擦写次数有限、需按页擦除 | 成本敏感、简单应用 |

### 1.4 驱动选择宏定义

在编译时定义以下宏之一来选择驱动：

```c
// HAL库驱动
#define USE_HAL_AT24CXX           // HAL库 + AT24Cxx EEPROM
#define USE_HAL_W25QXX            // HAL库 + W25Qxx SPI Flash
#define USE_HAL_INTERNAL_FLASH    // HAL库 + STM32内部Flash

// 标准库驱动
#define USE_STDLIB_AT24CXX        // 标准库 + AT24Cxx EEPROM
#define USE_STDLIB_W25QXX         // 标准库 + W25Qxx SPI Flash
#define USE_STDLIB_INTERNAL_FLASH // 标准库 + STM32内部Flash
```

**Keil设置方法**: Options → C/C++ → Preprocessor Symbols → Define

---

## 2. 快速开始

### 2.1 移植三步曲

移植校准系统只需三步：

1. **实现底层读写函数** - 根据硬件平台实现init/read/write/erase函数
2. **定义驱动结构体** - 填充StorageDriver_t结构体
3. **注册并使用** - 调用Storage_RegisterDriver()注册驱动

### 2.2 驱动结构体说明

```c
typedef struct {
    const char *name;        // 驱动名称 (如 "AT24C64")
    uint32_t base_addr;      // 基础地址 (校准数据存储起始位置)
    uint32_t total_size;     // 可用总大小
    uint32_t page_size;      // 页大小 (写入对齐)
    uint32_t sector_size;    // 扇区大小 (擦除对齐, EEPROM设为1)
    
    StorageInit_t  init;     // 初始化函数 (可选, 可为NULL)
    StorageRead_t  read;     // 读取函数 (必须)
    StorageWrite_t write;    // 写入函数 (必须)
    StorageErase_t erase;    // 擦除函数 (Flash必须, EEPROM可为NULL)
    StorageReady_t is_ready; // 就绪检查 (可选)
} StorageDriver_t;
```

### 2.3 完整初始化流程

```c
void System_CalibInit(void)
{
    // 步骤1: 注册存储驱动
    Storage_RegisterDriver(&g_my_driver);
    
    // 步骤2: 初始化校准系统
    CalibErr_t err = Calib_Init();
    if (err != CALIB_OK) {
        printf("Calib Init Failed: %s\n", Calib_ErrStr(err));
        while (1);
    }
    
    // 步骤3: 注册通道 (ID范围: 1 ~ CALIB_MAX_CHANNELS)
    Calib_RegisterCh(1, "FAN_I");
    Calib_RegisterCh(2, "CR_I");
    Calib_RegisterCh(3, "270_U");
    Calib_RegisterCh(4, "270_I");
    Calib_RegisterCh(5, "CAP_U");
    Calib_RegisterCh(6, "OUT_U");
    Calib_RegisterCh(7, "CAP_I");
    
    // 步骤4: 从存储器加载校准数据
    Calib_LoadAll();
    
    // 步骤5: 检查是否有有效数据，没有则使用默认值
    for (int i = 0; i < g_calib_ch_cnt; i++) {
        if (!g_calib_ch[i].valid) {
            double default_coeffs[] = {0.0, 1.0};  // y = x
            Calib_SetCoeffs(g_calib_ch[i].id, default_coeffs, 2);
        }
    }
    
    // 步骤6: 打印调试信息
    Calib_PrintInfo();
}
```

---

## 3. API参考手册

### 3.1 存储层API (Calib_Storage.h)

#### Storage_RegisterDriver

```c
StorageError_t Storage_RegisterDriver(const StorageDriver_t *driver);
```

**功能**: 注册存储驱动

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| driver | const StorageDriver_t* | 驱动结构体指针 |

**返回值**:
| 值 | 说明 |
|----|------|
| STORAGE_OK | 注册成功 |
| STORAGE_ERR_PARAM | 参数错误 (driver为NULL或read/write为NULL) |

**示例**:
```c
// 注册AT24Cxx驱动
StorageError_t err = Storage_RegisterDriver(&g_at24cxx_driver);
if (err != STORAGE_OK) {
    printf("Driver register failed!\n");
}
```

---

#### Storage_GetDriver

```c
const StorageDriver_t* Storage_GetDriver(void);
```

**功能**: 获取当前已注册的驱动

**参数**: 无

**返回值**: 驱动指针，未注册返回NULL

**示例**:
```c
const StorageDriver_t *drv = Storage_GetDriver();
if (drv != NULL) {
    printf("Current driver: %s\n", drv->name);
    printf("Total size: %u bytes\n", drv->total_size);
}
```

---

#### Storage_Init

```c
StorageError_t Storage_Init(void);
```

**功能**: 初始化存储设备

**参数**: 无

**返回值**:
| 值 | 说明 |
|----|------|
| STORAGE_OK | 初始化成功 |
| STORAGE_ERR_NO_DRIVER | 未注册驱动 |
| STORAGE_ERR_NOT_INIT | 驱动初始化失败 |

**说明**: 会调用驱动的init函数（如果提供）

**示例**:
```c
Storage_RegisterDriver(&g_at24cxx_driver);
if (Storage_Init() != STORAGE_OK) {
    printf("Storage init failed!\n");
}
```

---

#### Storage_Read

```c
StorageError_t Storage_Read(uint32_t addr, uint8_t *buf, uint32_t len);
```

**功能**: 从存储设备读取数据

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| addr | uint32_t | 相对地址 (自动加上base_addr) |
| buf | uint8_t* | 数据缓冲区 |
| len | uint32_t | 读取长度 |

**返回值**:
| 值 | 说明 |
|----|------|
| STORAGE_OK | 读取成功 |
| STORAGE_ERR_NO_DRIVER | 未注册驱动 |
| STORAGE_ERR_PARAM | 参数错误或地址越界 |
| STORAGE_ERR_READ | 读取失败 |

**示例**:
```c
uint8_t buffer[64];
// 从偏移地址0x100读取64字节
if (Storage_Read(0x100, buffer, 64) == STORAGE_OK) {
    printf("Read success\n");
}
```

---

#### Storage_Write

```c
StorageError_t Storage_Write(uint32_t addr, const uint8_t *buf, uint32_t len);
```

**功能**: 向存储设备写入数据

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| addr | uint32_t | 相对地址 |
| buf | const uint8_t* | 数据缓冲区 |
| len | uint32_t | 写入长度 |

**返回值**:
| 值 | 说明 |
|----|------|
| STORAGE_OK | 写入成功 |
| STORAGE_ERR_NO_DRIVER | 未注册驱动 |
| STORAGE_ERR_PARAM | 参数错误或地址越界 |
| STORAGE_ERR_WRITE | 写入失败 |

**注意**: Flash设备写入前需要先擦除

**示例**:
```c
uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
if (Storage_Write(0x100, data, sizeof(data)) == STORAGE_OK) {
    printf("Write success\n");
}
```

---

#### Storage_Erase

```c
StorageError_t Storage_Erase(uint32_t addr, uint32_t len);
```

**功能**: 擦除存储区域

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| addr | uint32_t | 起始地址 |
| len | uint32_t | 擦除长度 |

**返回值**:
| 值 | 说明 |
|----|------|
| STORAGE_OK | 擦除成功 (或驱动无erase函数) |
| STORAGE_ERR_NO_DRIVER | 未注册驱动 |
| STORAGE_ERR_PARAM | 地址越界 |
| STORAGE_ERR_ERASE | 擦除失败 |

**说明**: EEPROM不需要擦除，此函数直接返回OK

**示例**:
```c
// 擦除一个扇区 (Flash)
if (Storage_Erase(0, 4096) == STORAGE_OK) {
    printf("Erase success\n");
}
```

---

#### Storage_WriteVerify

```c
StorageError_t Storage_WriteVerify(uint32_t addr, const uint8_t *buf, uint32_t len);
```

**功能**: 写入数据并回读验证

**参数**: 同Storage_Write

**返回值**:
| 值 | 说明 |
|----|------|
| STORAGE_OK | 写入并验证成功 |
| STORAGE_ERR_VERIFY | 验证失败 (写入数据与回读不一致) |
| 其他 | 同Storage_Write |

**说明**: 内部使用32字节缓冲区分块验证，节省RAM

**示例**:
```c
// 写入关键数据时使用验证写入
CalibData_t data = {...};
if (Storage_WriteVerify(0, (uint8_t*)&data, sizeof(data)) != STORAGE_OK) {
    printf("Write verify failed!\n");
}
```

---

#### Storage_IsReady

```c
bool Storage_IsReady(void);
```

**功能**: 检查存储设备是否就绪

**参数**: 无

**返回值**: true-就绪，false-未就绪

**示例**:
```c
if (Storage_IsReady()) {
    // 可以进行读写操作
}
```

---

### 3.2 校准核心API (CH_Calib.h)

#### Calib_Init

```c
CalibErr_t Calib_Init(void);
```

**功能**: 初始化校准系统

**参数**: 无

**返回值**:
| 值 | 说明 |
|----|------|
| CALIB_OK | 初始化成功 |
| CALIB_ERR_STORAGE | 存储初始化失败 |

**说明**: 调用前需先注册存储驱动

**示例**:
```c
Storage_RegisterDriver(&g_my_driver);

if (Calib_Init() != CALIB_OK) {
    printf("Calib init failed!\n");
    while(1);
}
```

---

#### Calib_RegisterCh

```c
CalibErr_t Calib_RegisterCh(uint8_t id, const char *name);
```

**功能**: 注册校准通道

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| id | uint8_t | 通道ID (1 ~ CALIB_MAX_CHANNELS) |
| name | const char* | 通道名称 (最大15字符) |

**返回值**:
| 值 | 说明 |
|----|------|
| CALIB_OK | 注册成功 |
| CALIB_ERR_PARAM | ID超出范围或name为NULL |
| CALIB_ERR_OVERFLOW | 已达最大通道数 |

**说明**: 
- ID必须从1开始，不能为0
- 重复注册相同ID会更新名称
- 名称会被截断到CALIB_NAME_MAX_LEN-1字符

**示例**:
```c
Calib_RegisterCh(1, "FAN_I");     // 风扇电流
Calib_RegisterCh(2, "CR_I");      // 整流电流
Calib_RegisterCh(3, "270_U");     // 270V电压
Calib_RegisterCh(4, "270_I");     // 270V电流
Calib_RegisterCh(5, "CAP_U");     // 电容电压
Calib_RegisterCh(6, "OUT_U");     // 输出电压
Calib_RegisterCh(7, "CAP_I");     // 电容电流
```

---

#### Calib_SetCoeffs

```c
CalibErr_t Calib_SetCoeffs(uint8_t id, const double *coeffs, uint8_t cnt);
```

**功能**: 设置通道校准系数

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| id | uint8_t | 通道ID |
| coeffs | const double* | 系数数组 [a0, a1, a2, ...] (升幂排列) |
| cnt | uint8_t | 系数数量 (1 ~ CALIB_MAX_COEFFS) |

**返回值**:
| 值 | 说明 |
|----|------|
| CALIB_OK | 设置成功 |
| CALIB_ERR_PARAM | 参数错误 |
| CALIB_ERR_CH_ID | 通道ID未注册 |

**说明**: 
- 系数按升幂排列: y = a0 + a1*x + a2*x² + ...
- 设置后通道变为有效状态

**示例**:
```c
// 一次多项式: y = 0.00125*x - 0.5
double coeffs_1st[] = {-0.5, 0.00125};
Calib_SetCoeffs(1, coeffs_1st, 2);

// 二次多项式: y = 1.5e-7*x² + 0.095*x - 1.2
double coeffs_2nd[] = {-1.2, 0.095, 1.5e-7};
Calib_SetCoeffs(2, coeffs_2nd, 3);

// 三次多项式: y = 8.03e-9*x³ - 3.58e-5*x² + 1.575*x - 18.58
double coeffs_3rd[] = {-18.58, 1.575, -3.58e-5, 8.03e-9};
Calib_SetCoeffs(3, coeffs_3rd, 4);
```

---

#### Calib_GetCoeffs

```c
CalibErr_t Calib_GetCoeffs(uint8_t id, double *coeffs, uint8_t *cnt);
```

**功能**: 获取通道校准系数

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| id | uint8_t | 通道ID |
| coeffs | double* | 系数输出缓冲区 (至少CALIB_MAX_COEFFS个元素) |
| cnt | uint8_t* | 系数数量输出 |

**返回值**:
| 值 | 说明 |
|----|------|
| CALIB_OK | 获取成功 |
| CALIB_ERR_PARAM | 参数错误 |
| CALIB_ERR_CH_ID | 通道ID未注册 |
| CALIB_ERR_NO_DATA | 通道无有效数据 |

**示例**:
```c
double coeffs[CALIB_MAX_COEFFS];
uint8_t cnt;

if (Calib_GetCoeffs(1, coeffs, &cnt) == CALIB_OK) {
    printf("CH1 has %d coefficients:\n", cnt);
    for (int i = 0; i < cnt; i++) {
        printf("  a%d = %e\n", i, coeffs[i]);
    }
}
```

---

#### Calib_Apply

```c
CalibErr_t Calib_Apply(uint8_t id, double x, double *y);
```

**功能**: 应用校准计算 (单点)

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| id | uint8_t | 通道ID |
| x | double | ADC原始值 |
| y | double* | 校准结果输出 |

**返回值**:
| 值 | 说明 |
|----|------|
| CALIB_OK | 计算成功 |
| CALIB_ERR_PARAM | y为NULL |
| CALIB_ERR_CH_ID | 通道ID未注册 |
| CALIB_ERR_NO_DATA | 通道无有效数据 (此时*y=x) |

**说明**: 使用Horner法则高效计算多项式

**示例**:
```c
double adc_raw = 2048.0;
double voltage;

CalibErr_t err = Calib_Apply(3, adc_raw, &voltage);
if (err == CALIB_OK) {
    printf("270V Voltage: %.2f V\n", voltage);
} else if (err == CALIB_ERR_NO_DATA) {
    printf("No calibration data, using raw: %.2f\n", voltage);
}
```

---

#### Calib_ApplyBatch

```c
CalibErr_t Calib_ApplyBatch(uint8_t id, const double *x, double *y, uint16_t cnt);
```

**功能**: 批量校准计算

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| id | uint8_t | 通道ID |
| x | const double* | ADC原始值数组 |
| y | double* | 校准结果输出数组 |
| cnt | uint16_t | 数据点数量 |

**返回值**: 同Calib_Apply

**说明**: 适合DMA采集后批量处理

**示例**:
```c
#define SAMPLE_CNT 100
double adc_buffer[SAMPLE_CNT];
double calibrated[SAMPLE_CNT];

// 假设adc_buffer已由DMA填充
Calib_ApplyBatch(1, adc_buffer, calibrated, SAMPLE_CNT);

// 计算平均值
double sum = 0;
for (int i = 0; i < SAMPLE_CNT; i++) {
    sum += calibrated[i];
}
double average = sum / SAMPLE_CNT;
```

---

#### Calib_Compute (内联函数)

```c
static inline double Calib_Compute(const double *coeffs, uint8_t cnt, double x);
```

**功能**: 直接计算多项式 (高性能)

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| coeffs | const double* | 系数数组指针 |
| cnt | uint8_t | 系数数量 |
| x | double | 输入值 |

**返回值**: 计算结果

**说明**: 
- 内联函数，无函数调用开销
- 适合中断/高频采样场景
- 需自行确保参数有效

**示例**:
```c
// 在定时器中断中使用
void TIM1_UP_IRQHandler(void)
{
    CalibCh_t *ch = &g_calib_ch[0];
    if (ch->valid) {
        double result = Calib_Compute(ch->coeffs, ch->coeff_cnt, (double)ADC_VALUE);
        // 使用result...
    }
}
```

---

### 3.3 存储API (CH_Calib.h)

#### Calib_Save

```c
CalibErr_t Calib_Save(uint8_t id);
```

**功能**: 保存单通道校准数据到存储器

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| id | uint8_t | 通道ID |

**返回值**:
| 值 | 说明 |
|----|------|
| CALIB_OK | 保存成功 |
| CALIB_ERR_CH_ID | 通道ID未注册 |
| CALIB_ERR_STORAGE | 存储写入失败 |

**说明**: 
- 写入前自动计算CRC16
- 使用Storage_WriteVerify确保数据正确

**示例**:
```c
// 设置系数后保存
double coeffs[] = {-0.5, 0.00125};
Calib_SetCoeffs(1, coeffs, 2);

if (Calib_Save(1) == CALIB_OK) {
    printf("CH1 saved\n");
}
```

---

#### Calib_Load

```c
CalibErr_t Calib_Load(uint8_t id);
```

**功能**: 从存储器加载单通道校准数据

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| id | uint8_t | 通道ID |

**返回值**:
| 值 | 说明 |
|----|------|
| CALIB_OK | 加载成功 |
| CALIB_ERR_CH_ID | 通道ID未注册 |
| CALIB_ERR_STORAGE | 存储读取失败 |
| CALIB_ERR_NO_DATA | 无有效数据 (magic不匹配) |
| CALIB_ERR_CRC | CRC校验失败 |

**说明**: 自动验证magic和CRC

**示例**:
```c
CalibErr_t err = Calib_Load(1);
switch (err) {
    case CALIB_OK:
        printf("CH1 loaded successfully\n");
        break;
    case CALIB_ERR_NO_DATA:
        printf("CH1 no saved data\n");
        break;
    case CALIB_ERR_CRC:
        printf("CH1 data corrupted!\n");
        break;
    default:
        printf("CH1 load error: %s\n", Calib_ErrStr(err));
}
```

---

#### Calib_SaveAll

```c
CalibErr_t Calib_SaveAll(void);
```

**功能**: 保存所有已注册通道的校准数据

**参数**: 无

**返回值**: 最后一个错误码，全部成功返回CALIB_OK

**示例**:
```c
// 批量设置系数后一次性保存
Calib_SetCoeffs(1, coeffs1, 2);
Calib_SetCoeffs(2, coeffs2, 3);
Calib_SetCoeffs(3, coeffs3, 2);

if (Calib_SaveAll() == CALIB_OK) {
    printf("All channels saved\n");
}
```

---

#### Calib_LoadAll

```c
CalibErr_t Calib_LoadAll(void);
```

**功能**: 加载所有已注册通道的校准数据

**参数**: 无

**返回值**: 最后一个错误码 (忽略CALIB_ERR_NO_DATA)

**说明**: 单个通道加载失败不影响其他通道

**示例**:
```c
// 系统启动时加载所有校准数据
Calib_RegisterCh(1, "FAN_I");
Calib_RegisterCh(2, "CR_I");
Calib_RegisterCh(3, "270_U");

Calib_LoadAll();

// 检查各通道状态
for (int i = 0; i < g_calib_ch_cnt; i++) {
    printf("CH%d %s: %s\n", 
           g_calib_ch[i].id, 
           g_calib_ch[i].name,
           g_calib_ch[i].valid ? "OK" : "No Data");
}
```

---

### 3.4 协议API (CH_Calib.h)

#### Calib_ProcessFrame

```c
CalibErr_t Calib_ProcessFrame(const uint8_t *data, uint16_t len,
                               uint8_t *resp, uint16_t *resp_len);
```

**功能**: 处理接收到的协议帧

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| data | const uint8_t* | 接收数据 |
| len | uint16_t | 数据长度 |
| resp | uint8_t* | 响应缓冲区 (建议256字节) |
| resp_len | uint16_t* | 响应长度输出 |

**返回值**:
| 值 | 说明 |
|----|------|
| CALIB_OK | 处理成功 |
| CALIB_ERR_PARAM | 参数错误 |
| CALIB_ERR_PARSE | 帧解析失败 |
| CALIB_ERR_CRC | CRC校验失败 |
| 其他 | 命令执行错误 |

**支持的命令**:
| 命令 | 帧头 | 功能 |
|------|------|------|
| CalCh | 0xAA + "CalCh" | 写入校准系数 |
| RBCalCh | 0xAA + "RBCalCh" | 读取通道列表 |
| RdCoef | 0xAA + "RdCoef" | 读取通道系数 |

**示例**:
```c
void UART_ProcessCallback(void)
{
    uint8_t resp[256];
    uint16_t resp_len = 0;
    
    CalibErr_t err = Calib_ProcessFrame(g_uart_rx_buf, g_uart_rx_len, resp, &resp_len);
    
    if (resp_len > 0) {
        UART_Send(resp, resp_len);
    } else if (err != CALIB_OK) {
        char err_msg[32];
        int len = sprintf(err_msg, "ERR:%s\r\n", Calib_ErrStr(err));
        UART_Send((uint8_t*)err_msg, len);
    }
}
```

---

#### Calib_PackChList

```c
int Calib_PackChList(char *buf, uint32_t size);
```

**功能**: 打包通道列表为字符串

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| buf | char* | 输出缓冲区 |
| size | uint32_t | 缓冲区大小 |

**返回值**: 实际长度，-1表示失败

**输出格式**: `CH:1:FAN_I,2:CR_I,3:270_U`

**示例**:
```c
char buf[128];
int len = Calib_PackChList(buf, sizeof(buf));
if (len > 0) {
    printf("%s\n", buf);
}
// 输出: CH:1:FAN_I,2:CR_I,3:270_U,4:270_I,5:CAP_U
```

---

#### Calib_PackCoeffs

```c
int Calib_PackCoeffs(uint8_t id, uint8_t *buf, uint32_t size);
```

**功能**: 打包通道系数为二进制帧

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| id | uint8_t | 通道ID |
| buf | uint8_t* | 输出缓冲区 |
| size | uint32_t | 缓冲区大小 |

**返回值**: 实际长度，-1表示失败

**帧格式**: `0xAA + "Coef" + len + ch_id + coeff_cnt + coeffs[n*8] + CRC16`

**示例**:
```c
uint8_t frame[128];
int len = Calib_PackCoeffs(1, frame, sizeof(frame));
if (len > 0) {
    UART_Send(frame, len);
}
```

---

### 3.5 工具API (CH_Calib.h)

#### Calib_CRC16

```c
uint16_t Calib_CRC16(const uint8_t *data, uint16_t len);
```

**功能**: 计算CRC16-MODBUS校验

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| data | const uint8_t* | 数据 |
| len | uint16_t | 数据长度 |

**返回值**: CRC16值

**说明**: 使用查表法，初始值0xFFFF

**示例**:
```c
uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
uint16_t crc = Calib_CRC16(data, sizeof(data));
printf("CRC16: 0x%04X\n", crc);
```

---

#### Calib_PrintInfo

```c
void Calib_PrintInfo(void);
```

**功能**: 打印校准系统调试信息

**参数**: 无

**返回值**: 无

**输出示例**:
```
===== Calib System =====
Storage: AT24C64-HAL
Channels: 7
------------------------
[1] FAN_I: y = 1.2500e-03*x^1 + -5.0000e-01
[2] CR_I: y = 9.5000e-02*x^1 + -1.2000e+00
[3] 270_U: (no data)
...
========================
```

**示例**:
```c
// 系统初始化完成后打印
Calib_Init();
Calib_RegisterCh(1, "FAN_I");
Calib_LoadAll();
Calib_PrintInfo();  // 查看加载状态
```

---

#### Calib_ErrStr

```c
const char* Calib_ErrStr(CalibErr_t err);
```

**功能**: 获取错误码描述字符串

**参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| err | CalibErr_t | 错误码 |

**返回值**: 错误描述字符串

**错误码对照**:
| 错误码 | 描述 |
|--------|------|
| CALIB_OK | "OK" |
| CALIB_ERR_PARAM | "Param" |
| CALIB_ERR_CH_ID | "ChID" |
| CALIB_ERR_STORAGE | "Storage" |
| CALIB_ERR_CRC | "CRC" |
| CALIB_ERR_NO_DATA | "NoData" |
| CALIB_ERR_PARSE | "Parse" |
| CALIB_ERR_OVERFLOW | "Overflow" |

**示例**:
```c
CalibErr_t err = Calib_Load(1);
if (err != CALIB_OK) {
    printf("Load failed: %s\n", Calib_ErrStr(err));
}
```

---

### 3.6 全局变量

#### g_calib_ch

```c
extern CalibCh_t g_calib_ch[CALIB_MAX_CHANNELS];
```

**说明**: 通道信息数组，可直接访问获取最高性能

**结构体定义**:
```c
typedef struct {
    uint8_t  id;                        // 通道ID (1-based)
    char     name[CALIB_NAME_MAX_LEN];  // 名称
    uint8_t  coeff_cnt;                 // 系数数量
    double   coeffs[CALIB_MAX_COEFFS];  // 系数
    bool     valid;                     // 数据有效
} CalibCh_t;
```

**示例**:
```c
// 直接访问通道信息
for (int i = 0; i < g_calib_ch_cnt; i++) {
    CalibCh_t *ch = &g_calib_ch[i];
    if (ch->valid) {
        printf("CH%d: %d coefficients\n", ch->id, ch->coeff_cnt);
    }
}
```

---

#### g_calib_ch_cnt

```c
extern uint8_t g_calib_ch_cnt;
```

**说明**: 已注册的通道数量

**示例**:
```c
printf("Registered channels: %d\n", g_calib_ch_cnt);
```

---

### 3.7 配置宏 (CH_Calib.h)

| 宏 | 默认值 | 说明 |
|----|--------|------|
| CALIB_MAX_CHANNELS | 8 | 最大通道数 |
| CALIB_MAX_COEFFS | 6 | 最大系数数 (最高5次多项式) |
| CALIB_NAME_MAX_LEN | 16 | 通道名称最大长度 |
| CALIB_MAGIC | 0xCA1B | 数据有效标识 |
| CALIB_FRAME_HEADER | 0xAA | 协议帧头 |

**修改示例**:
```c
// 在CH_Calib.h中修改
#define CALIB_MAX_CHANNELS      16      // 扩展到16通道
#define CALIB_MAX_COEFFS        8       // 支持7次多项式
```

---

## 4. HAL库驱动移植

### 4.1 AT24Cxx EEPROM (I2C)

#### 配置参数

```c
#define AT24CXX_I2C_HANDLE      hi2c1           // I2C句柄
#define AT24CXX_DEVICE_ADDR     0xA0            // 设备地址 (7位地址左移1位)
#define AT24CXX_PAGE_SIZE       32              // AT24C64页大小, AT24C02=8
#define AT24CXX_TOTAL_SIZE      8192            // AT24C64=8KB
#define AT24CXX_WRITE_DELAY     5               // 写入延时ms
#define AT24CXX_TIMEOUT         100             // I2C超时ms

extern I2C_HandleTypeDef AT24CXX_I2C_HANDLE;
```

#### 初始化函数

```c
static int hal_at24cxx_init(void)
{
    // 检测设备是否存在
    HAL_StatusTypeDef status;
    status = HAL_I2C_IsDeviceReady(&hi2c1, AT24CXX_DEVICE_ADDR, 3, AT24CXX_TIMEOUT);
    return (status == HAL_OK) ? 0 : -1;
}
```

#### 读取函数

```c
static int hal_at24cxx_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    HAL_StatusTypeDef status;
    
    // AT24C64及以上使用16位地址
    #if (AT24CXX_TOTAL_SIZE > 256)
        status = HAL_I2C_Mem_Read(&hi2c1, 
                                   AT24CXX_DEVICE_ADDR,
                                   (uint16_t)addr, 
                                   I2C_MEMADD_SIZE_16BIT,
                                   buf, 
                                   (uint16_t)len, 
                                   AT24CXX_TIMEOUT);
    #else
        // AT24C02/04/08使用8位地址
        status = HAL_I2C_Mem_Read(&hi2c1, 
                                   AT24CXX_DEVICE_ADDR,
                                   (uint16_t)addr, 
                                   I2C_MEMADD_SIZE_8BIT,
                                   buf, 
                                   (uint16_t)len, 
                                   AT24CXX_TIMEOUT);
    #endif
    
    return (status == HAL_OK) ? 0 : -1;
}
```

#### 写入函数 (分页处理)

```c
static int hal_at24cxx_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    HAL_StatusTypeDef status;
    uint32_t written = 0;
    
    while (written < len) {
        // 计算当前页剩余空间
        uint32_t page_offset = (addr + written) % AT24CXX_PAGE_SIZE;
        uint32_t page_remain = AT24CXX_PAGE_SIZE - page_offset;
        uint32_t to_write = (len - written) > page_remain ? page_remain : (len - written);
        
        // 写入一页
        #if (AT24CXX_TOTAL_SIZE > 256)
            status = HAL_I2C_Mem_Write(&hi2c1,
                                        AT24CXX_DEVICE_ADDR,
                                        (uint16_t)(addr + written),
                                        I2C_MEMADD_SIZE_16BIT,
                                        (uint8_t*)(buf + written),
                                        (uint16_t)to_write,
                                        AT24CXX_TIMEOUT);
        #else
            status = HAL_I2C_Mem_Write(&hi2c1,
                                        AT24CXX_DEVICE_ADDR,
                                        (uint16_t)(addr + written),
                                        I2C_MEMADD_SIZE_8BIT,
                                        (uint8_t*)(buf + written),
                                        (uint16_t)to_write,
                                        AT24CXX_TIMEOUT);
        #endif
        
        if (status != HAL_OK) return -1;
        
        HAL_Delay(AT24CXX_WRITE_DELAY);  // 等待写入完成
        written += to_write;
    }
    
    return 0;
}
```

#### 驱动结构体

```c
const StorageDriver_t g_hal_at24cxx_driver = {
    .name        = "AT24C64-HAL",
    .base_addr   = 0,
    .total_size  = AT24CXX_TOTAL_SIZE,
    .page_size   = AT24CXX_PAGE_SIZE,
    .sector_size = 1,               // EEPROM无扇区概念
    
    .init        = hal_at24cxx_init,
    .read        = hal_at24cxx_read,
    .write       = hal_at24cxx_write,
    .erase       = NULL,            // EEPROM不需要擦除
    .is_ready    = hal_at24cxx_ready,
};
```

---

### 4.2 W25Qxx SPI Flash

#### 配置参数

```c
#define W25QXX_SPI_HANDLE       hspi1
#define W25QXX_CS_GPIO          GPIOA
#define W25QXX_CS_PIN           GPIO_PIN_4
#define W25QXX_SECTOR_SIZE      4096            // 4KB扇区
#define W25QXX_PAGE_SIZE        256             // 256字节页
#define W25QXX_TOTAL_SIZE       (8 * 1024 * 1024)   // 8MB (W25Q64)
#define W25QXX_CALIB_BASE       0x700000        // 校准数据起始地址
#define W25QXX_CALIB_SIZE       0x100000        // 分配1MB

#define W25QXX_CS_LOW()         HAL_GPIO_WritePin(W25QXX_CS_GPIO, W25QXX_CS_PIN, GPIO_PIN_RESET)
#define W25QXX_CS_HIGH()        HAL_GPIO_WritePin(W25QXX_CS_GPIO, W25QXX_CS_PIN, GPIO_PIN_SET)
```

#### SPI读写基础函数

```c
static uint8_t hal_w25qxx_spi_rw(uint8_t data)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&hspi1, &data, &rx, 1, 100);
    return rx;
}

static void hal_w25qxx_write_enable(void)
{
    W25QXX_CS_LOW();
    hal_w25qxx_spi_rw(0x06);  // Write Enable命令
    W25QXX_CS_HIGH();
}

static void hal_w25qxx_wait_busy(void)
{
    uint8_t status;
    W25QXX_CS_LOW();
    hal_w25qxx_spi_rw(0x05);  // Read Status Register
    do {
        status = hal_w25qxx_spi_rw(0xFF);
    } while (status & 0x01);
    W25QXX_CS_HIGH();
}
```

#### 读取函数

```c
static int hal_w25qxx_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    W25QXX_CS_LOW();
    
    hal_w25qxx_spi_rw(0x03);  // Read Data命令
    hal_w25qxx_spi_rw((addr >> 16) & 0xFF);
    hal_w25qxx_spi_rw((addr >> 8) & 0xFF);
    hal_w25qxx_spi_rw(addr & 0xFF);
    
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = hal_w25qxx_spi_rw(0xFF);
    }
    
    W25QXX_CS_HIGH();
    return 0;
}
```

#### 页编程函数

```c
static int hal_w25qxx_write_page(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    hal_w25qxx_write_enable();
    
    W25QXX_CS_LOW();
    hal_w25qxx_spi_rw(0x02);  // Page Program命令
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
```

#### 扇区擦除函数

```c
static int hal_w25qxx_erase(uint32_t addr, uint32_t len)
{
    uint32_t start_sector = addr / W25QXX_SECTOR_SIZE;
    uint32_t end_sector = (addr + len - 1) / W25QXX_SECTOR_SIZE;
    
    for (uint32_t s = start_sector; s <= end_sector; s++) {
        uint32_t sector_addr = s * W25QXX_SECTOR_SIZE;
        
        hal_w25qxx_write_enable();
        
        W25QXX_CS_LOW();
        hal_w25qxx_spi_rw(0x20);  // Sector Erase命令
        hal_w25qxx_spi_rw((sector_addr >> 16) & 0xFF);
        hal_w25qxx_spi_rw((sector_addr >> 8) & 0xFF);
        hal_w25qxx_spi_rw(sector_addr & 0xFF);
        W25QXX_CS_HIGH();
        
        hal_w25qxx_wait_busy();
    }
    
    return 0;
}
```

#### 驱动结构体

```c
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
```

---

### 4.3 STM32内部Flash

#### 配置参数 (STM32F103)

```c
/* Flash布局: 0x08000000 - 0x0800FFFF (64KB)
 * 程序区: 0x08000000 - 0x0800EFFF (60KB)
 * 校准区: 0x0800F000 - 0x0800FFFF (4KB, 最后4页)
 */
#define FLASH_PAGE_SIZE         1024            // STM32F103页大小1KB
#define FLASH_CALIB_START       0x0800F000      // 校准数据起始
#define FLASH_CALIB_SIZE        (4 * 1024)      // 4KB
```

#### 读取函数

```c
static int hal_internal_flash_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    // 直接内存映射读取
    memcpy(buf, (void*)addr, len);
    return 0;
}
```

#### 写入函数 (半字编程)

```c
static int hal_internal_flash_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    HAL_StatusTypeDef status;
    
    HAL_FLASH_Unlock();
    
    // STM32F1按半字(16bit)编程
    for (uint32_t i = 0; i < len; i += 2) {
        uint16_t half_word;
        
        half_word = buf[i];
        if (i + 1 < len) {
            half_word |= (uint16_t)buf[i + 1] << 8;
        } else {
            half_word |= 0xFF00;  // 奇数长度时填充0xFF
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
```

#### 页擦除函数

```c
static int hal_internal_flash_erase(uint32_t addr, uint32_t len)
{
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;
    HAL_StatusTypeDef status;
    
    // 计算需要擦除的页
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
```

#### 驱动结构体

```c
const StorageDriver_t g_hal_internal_flash_driver = {
    .name        = "InternalFlash-HAL",
    .base_addr   = FLASH_CALIB_START,
    .total_size  = FLASH_CALIB_SIZE,
    .page_size   = 2,               // 半字写入
    .sector_size = FLASH_PAGE_SIZE,
    
    .init        = hal_internal_flash_init,
    .read        = hal_internal_flash_read,
    .write       = hal_internal_flash_write,
    .erase       = hal_internal_flash_erase,
    .is_ready    = hal_internal_flash_ready,
};
```

---

## 5. 标准库驱动移植

标准库驱动与HAL库类似，主要区别在于底层API调用方式。

### 5.1 API对比表

#### I2C操作

| HAL库 | 标准库 |
|-------|--------|
| `HAL_I2C_Mem_Read()` | `I2C_GenerateSTART()` + `I2C_Send7bitAddress()` + ... |
| `HAL_I2C_Mem_Write()` | 需手动处理起始、地址、数据、停止 |
| `HAL_I2C_IsDeviceReady()` | 尝试发送地址检测ACK |

#### SPI操作

| HAL库 | 标准库 |
|-------|--------|
| `HAL_SPI_TransmitReceive()` | `SPI_I2S_SendData()` + `SPI_I2S_ReceiveData()` |
| `HAL_GPIO_WritePin()` | `GPIO_SetBits()` / `GPIO_ResetBits()` |

#### Flash操作

| HAL库 | 标准库 |
|-------|--------|
| `HAL_FLASH_Unlock()` | `FLASH_Unlock()` |
| `HAL_FLASH_Program()` | `FLASH_ProgramHalfWord()` |
| `HAL_FLASHEx_Erase()` | `FLASH_ErasePage()` |

### 5.2 标准库I2C基础函数

```c
#include "stm32f10x.h"
#include "stm32f10x_i2c.h"

#define STDLIB_AT24_I2C         I2C1
#define STDLIB_AT24_ADDR        0xA0
#define STDLIB_AT24_TIMEOUT     10000

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
```

### 5.3 标准库AT24Cxx读写

```c
static int stdlib_at24cxx_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    // 发送地址
    if (stdlib_i2c_start() != 0) return -1;
    if (stdlib_i2c_send_addr(STDLIB_AT24_ADDR, I2C_Direction_Transmitter) != 0) {
        stdlib_i2c_stop();
        return -1;
    }
    
    // 16位地址
    stdlib_i2c_send_byte((addr >> 8) & 0xFF);
    stdlib_i2c_send_byte(addr & 0xFF);
    
    // 重新启动，切换为读取
    if (stdlib_i2c_start() != 0) return -1;
    if (stdlib_i2c_send_addr(STDLIB_AT24_ADDR, I2C_Direction_Receiver) != 0) {
        stdlib_i2c_stop();
        return -1;
    }
    
    // 读取数据
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = stdlib_i2c_recv_byte(i < len - 1);  // 最后一字节NACK
    }
    
    stdlib_i2c_stop();
    return 0;
}
```

### 5.4 标准库SPI基础函数

```c
#include "stm32f10x.h"
#include "stm32f10x_spi.h"

#define STDLIB_W25_SPI          SPI1
#define STDLIB_W25_CS_PORT      GPIOA
#define STDLIB_W25_CS_PIN       GPIO_Pin_4

#define STDLIB_W25_CS_LOW()     GPIO_ResetBits(STDLIB_W25_CS_PORT, STDLIB_W25_CS_PIN)
#define STDLIB_W25_CS_HIGH()    GPIO_SetBits(STDLIB_W25_CS_PORT, STDLIB_W25_CS_PIN)

static uint8_t stdlib_spi_rw(uint8_t data)
{
    while (SPI_I2S_GetFlagStatus(STDLIB_W25_SPI, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(STDLIB_W25_SPI, data);
    while (SPI_I2S_GetFlagStatus(STDLIB_W25_SPI, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(STDLIB_W25_SPI);
}
```

### 5.5 标准库Flash操作

```c
#include "stm32f10x_flash.h"

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
    uint32_t end_addr = addr + len;
    
    FLASH_Unlock();
    
    while (addr < end_addr) {
        if (FLASH_ErasePage(addr) != FLASH_COMPLETE) {
            FLASH_Lock();
            return -1;
        }
        addr += STDLIB_FLASH_PAGE_SIZE;
    }
    
    FLASH_Lock();
    return 0;
}
```

---

## 6. 校准公式应用

### 6.1 多项式格式

系数采用**升幂排列**: `[a0, a1, a2, ..., an]`

对应多项式: `y = a0 + a1×x + a2×x² + ... + an×xⁿ`

### 6.2 Horner法则原理

将多项式转换为嵌套形式，减少乘法次数：

```
原始: y = a0 + a1*x + a2*x² + a3*x³
Horner: y = a0 + x*(a1 + x*(a2 + x*a3))
```

**优化效果**: n次多项式从 `2n-1` 次乘法优化为 `n-1` 次

```c
static inline double Calib_Compute(const double *coeffs, uint8_t cnt, double x)
{
    if (cnt == 0) return x;
    double y = coeffs[cnt - 1];
    for (int i = cnt - 2; i >= 0; i--) {
        y = y * x + coeffs[i];
    }
    return y;
}
```

### 6.3 应用方式

#### 方式1: 标准API调用 (简单)

```c
double raw = 2048.0;
double calibrated;
CalibErr_t err = Calib_Apply(CH_FAN_I, raw, &calibrated);
if (err == CALIB_OK) {
    // 使用calibrated值
}
```

#### 方式2: 批量处理 (适合DMA采集)

```c
double raw[100], result[100];
Calib_ApplyBatch(CH_FAN_I, raw, result, 100);
```

#### 方式3: 高性能内联 (适合中断)

```c
CalibCh_t *ch = &g_calib_ch[0];  // 直接访问全局数组
if (ch->valid) {
    double y = Calib_Compute(ch->coeffs, ch->coeff_cnt, adc_value);
}
```

#### 方式4: 缓存系数指针 (最高性能)

```c
// 缓存结构
typedef struct {
    const double *coeffs;
    uint8_t cnt;
    bool valid;
} CalibCache_t;

CalibCache_t g_calib_cache[7];

// 初始化时建立缓存
void CalibExample_InitCache(void)
{
    for (int i = 0; i < g_calib_ch_cnt; i++) {
        if (g_calib_ch[i].valid) {
            int idx = g_calib_ch[i].id - 1;
            g_calib_cache[idx].coeffs = g_calib_ch[i].coeffs;
            g_calib_cache[idx].cnt = g_calib_ch[i].coeff_cnt;
            g_calib_cache[idx].valid = true;
        }
    }
}

// 中断中使用 - 零开销查找
void TIM1_UP_IRQHandler(void)
{
    if (g_calib_cache[0].valid) {
        result = Calib_Compute(g_calib_cache[0].coeffs, 
                               g_calib_cache[0].cnt, 
                               (double)g_adc_raw[0]);
    }
}
```

#### 方式5: 定点数优化 (无FPU的MCU)

```c
// Q16.16定点数格式
#define Q16_SHIFT   16
#define Q16_ONE     (1 << Q16_SHIFT)

typedef struct {
    int32_t coeffs[CALIB_MAX_COEFFS];
    uint8_t cnt;
    bool valid;
} FixedPointCalib_t;

// 转换为定点数
void ConvertToFixed(void)
{
    for (int i = 0; i < g_calib_ch_cnt; i++) {
        for (int j = 0; j < g_calib_ch[i].coeff_cnt; j++) {
            // double转Q16.16定点数
            g_fixed_calib[i].coeffs[j] = (int32_t)(g_calib_ch[i].coeffs[j] * Q16_ONE);
        }
    }
}

// 定点数Horner计算
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
```

### 6.4 手动设置校准系数

```c
void CalibExample_ManualSetup(void)
{
    // 一次多项式: y = 0.00125*x - 0.5
    double ch1_coeffs[] = {-0.5, 0.00125};
    Calib_SetCoeffs(CH_FAN_I, ch1_coeffs, 2);
    
    // 二次多项式: y = 1.5e-7*x^2 + 0.095*x - 1.2
    double ch5_coeffs[] = {-1.2, 0.095, 1.5e-7};
    Calib_SetCoeffs(CH_CAP_U, ch5_coeffs, 3);
    
    // 保存到存储器
    Calib_SaveAll();
}
```

---

## 7. 通信协议处理

### 7.1 帧格式

| 字段 | 长度 | 说明 |
|------|------|------|
| Header | 1字节 | 固定值 `0xAA` |
| Command | 5-7字节 | ASCII命令标识 |
| Length | 1字节 | 帧总长度 (仅写入命令) |
| Data | N字节 | 数据内容 |
| CRC16 | 2字节 | CRC-16/MODBUS校验 (小端序) |

### 7.2 命令列表

| 命令 | 标识 | 方向 | 说明 |
|------|------|------|------|
| 写入系数 | `CalCh` | PC→MCU | 写入指定通道的校准系数并保存 |
| 读取通道列表 | `RBCalCh` | PC→MCU | 获取所有已注册通道的ID和名称 |
| 读取系数 | `RdCoef` | PC→MCU | 读取指定通道的校准系数 |

### 7.3 响应格式

```
通道列表响应:  CH:1:FAN_I,2:CR_I,3:270_U\r\n
操作成功响应:  OK:CH1,4 coeffs\r\n
```

### 7.4 串口接收处理 (HAL库)

```c
#define UART_RX_BUF_SIZE    256
static uint8_t g_uart_rx_buf[UART_RX_BUF_SIZE];
static volatile uint16_t g_uart_rx_len = 0;
static volatile uint8_t g_uart_rx_complete = 0;

// 串口空闲中断回调
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1) {
        g_uart_rx_len = Size;
        g_uart_rx_complete = 1;
    }
}

// 主循环处理
void CalibProtocol_Process(void)
{
    if (!g_uart_rx_complete) return;
    
    uint8_t resp_buf[256];
    uint16_t resp_len = 0;
    
    CalibErr_t err = Calib_ProcessFrame(g_uart_rx_buf, g_uart_rx_len, resp_buf, &resp_len);
    
    if (resp_len > 0) {
        HAL_UART_Transmit(&huart1, resp_buf, resp_len, 100);
    } else if (err != CALIB_OK) {
        resp_len = sprintf((char*)resp_buf, "ERR:%s\r\n", Calib_ErrStr(err));
        HAL_UART_Transmit(&huart1, resp_buf, resp_len, 100);
    }
    
    g_uart_rx_complete = 0;
}
```

### 7.5 串口接收处理 (标准库)

```c
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET) {
        // 清除空闲中断
        volatile uint8_t clear = USART1->SR;
        clear = USART1->DR;
        (void)clear;
        
        // 停止DMA，获取接收长度
        DMA_Cmd(DMA1_Channel5, DISABLE);
        g_uart_rx_len = UART_RX_BUF_SIZE - DMA_GetCurrDataCounter(DMA1_Channel5);
        g_uart_rx_complete = 1;
        
        // 重新启动DMA接收
        DMA_SetCurrDataCounter(DMA1_Channel5, UART_RX_BUF_SIZE);
        DMA_Cmd(DMA1_Channel5, ENABLE);
    }
}
```

---

## 8. 性能优化技巧

### 8.1 减少函数调用开销

- 使用 `Calib_Compute()` 内联函数而非 `Calib_Apply()`
- 初始化时缓存系数指针，避免运行时查找
- 直接访问 `g_calib_ch` 全局数组

### 8.2 无FPU优化

对于无浮点单元的MCU，将double系数转换为定点数：

```c
// Q16.16定点数格式
#define Q16_SHIFT   16
#define Q16_ONE     (1 << Q16_SHIFT)

// 转换: int32_t fixed = (int32_t)(double_val * Q16_ONE);
// 计算: result = (y * x) >> Q16_SHIFT;
```

### 8.3 存储优化

- Flash写入前先擦除整个扇区
- EEPROM分页写入，每页后等待5ms
- 使用 `Storage_WriteVerify()` 确保写入正确

### 8.4 中断场景优化

```c
// 初始化时缓存 (只执行一次)
static const double *s_ch1_coeffs;
static uint8_t s_ch1_cnt;

void Init_Cache(void)
{
    CalibCh_t *ch = &g_calib_ch[0];
    if (ch->valid) {
        s_ch1_coeffs = ch->coeffs;
        s_ch1_cnt = ch->coeff_cnt;
    }
}

// 定时器中断 (高频执行)
void TIM1_UP_IRQHandler(void)
{
    // 直接使用缓存的指针，零开销
    double result = Calib_Compute(s_ch1_coeffs, s_ch1_cnt, (double)ADC_VALUE);
}
```

---

## 9. 常见问题解答

### Q1: 存储读写失败?

1. 检查硬件连接 (I2C/SPI信号线)
2. 确认设备地址正确 (AT24Cxx默认0xA0)
3. Flash写入前是否已擦除?
4. EEPROM写入后是否等待足够时间?

### Q2: CRC校验失败?

1. 确认数据未被意外修改
2. 检查存储区域是否被其他程序覆盖
3. Flash可能已达到擦写寿命

### Q3: 校准数据加载为空?

1. 首次使用需要先保存数据
2. 检查magic字段是否匹配0xCA1B
3. 存储区域可能被程序更新覆盖

### Q4: 如何选择存储方案?

- **频繁更新 + 小数据量** → AT24Cxx EEPROM
- **大数据量 + 不常更新** → W25Qxx SPI Flash
- **成本敏感 + 简单应用** → 内部Flash

### Q5: 如何添加自定义存储驱动?

1. 实现read和write函数 (必须)
2. 如需擦除，实现erase函数
3. 填充StorageDriver_t结构体
4. 调用Storage_RegisterDriver()注册

### Q6: 支持的多项式阶数?

- 最大支持6个系数 (5次多项式)
- 可通过修改 `CALIB_MAX_COEFFS` 调整

### Q7: 最大支持多少通道?

- 默认8通道
- 可通过修改 `CALIB_MAX_CHANNELS` 调整
- 每通道占用56字节存储空间

---

## 附录: main函数完整示例

### HAL库版本

```c
#include "main.h"
#include "CH_Calib.h"
#include "Calib_Storage.h"

extern const StorageDriver_t g_hal_at24cxx_driver;

int main(void)
{
    // HAL初始化
    HAL_Init();
    SystemClock_Config();
    
    // 外设初始化 (CubeMX生成)
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    MX_ADC1_Init();
    
    // 校准系统初始化
    Storage_RegisterDriver(&g_hal_at24cxx_driver);
    Calib_Init();
    
    Calib_RegisterCh(1, "FAN_I");
    Calib_RegisterCh(2, "CR_I");
    Calib_RegisterCh(3, "270_U");
    
    Calib_LoadAll();
    Calib_PrintInfo();
    
    // 启动DMA ADC采集
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)g_adc_raw, 7);
    
    // 启动串口DMA接收
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, g_uart_rx_buf, 256);
    
    while (1)
    {
        CalibProtocol_Process();
        
        // 应用校准
        double calibrated;
        Calib_Apply(1, (double)g_adc_raw[0], &calibrated);
        
        HAL_Delay(10);
    }
}
```

### 标准库版本

```c
#include "stm32f10x.h"
#include "CH_Calib.h"
#include "Calib_Storage.h"

extern const StorageDriver_t g_stdlib_at24cxx_driver;

int main(void)
{
    SystemInit();
    
    // 外设初始化
    GPIO_Configuration();
    I2C_Configuration();
    USART_Configuration();
    ADC_Configuration();
    DMA_Configuration();
    
    // 校准系统初始化
    Storage_RegisterDriver(&g_stdlib_at24cxx_driver);
    Calib_Init();
    
    Calib_RegisterCh(1, "FAN_I");
    Calib_RegisterCh(2, "CR_I");
    Calib_RegisterCh(3, "270_U");
    
    Calib_LoadAll();
    Calib_PrintInfo();
    
    while (1)
    {
        CalibProtocol_Process();
        
        double calibrated;
        Calib_Apply(1, (double)g_adc_raw[0], &calibrated);
    }
}
```

---

*文档版本: 2.0*  
*更多信息请参考源代码注释*
