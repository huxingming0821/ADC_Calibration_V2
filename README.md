# ADC 校准系统 V2.0

<p align="center">
  <strong>多通道多项式校准解决方案</strong><br>
  嵌入式 MCU 校准库 + Windows PC 上位机工具
</p>

---

## 📖 项目简介

本项目提供一套完整的 ADC（模数转换器）校准解决方案，适用于需要高精度数据采集的嵌入式系统。系统分为两部分：

- **MCU 端**：运行在嵌入式设备上的 C 语言校准库，支持多通道多项式校准计算与数据持久化存储
- **PC 端**：Windows WPF 上位机工具，提供多项式拟合计算、误差验证与串口下载功能

---

## ✨ 核心特性

### MCU 端

| 特性 | 说明 |
|------|------|
| 🔌 **存储驱动抽象** | 函数指针实现，轻松适配不同存储硬件 |
| 💾 **多存储支持** | 内部 Flash、AT24Cxx EEPROM、SPI Flash (W25Qxx)、LittleFS |
| ⚡ **高效计算** | Horner 法则优化多项式计算，减少乘法次数 |
| ✅ **数据校验** | CRC16-MODBUS 查表法校验，保障数据完整性 |
| 📡 **通信协议** | 完善的串口通信协议，与上位机无缝对接 |
| 🎯 **资源友好** | 精简代码设计，适合资源受限的 MCU 平台 |

### PC 端

| 特性 | 说明 |
|------|------|
| 🎨 **现代 UI** | WPF + Material Design 风格界面，美观易用 |
| 📊 **多项式拟合** | 基于 MathNet.Numerics SVD 分解，支持 1-5 阶多项式 |
| 🔗 **串口管理** | 自动识别设备，显示友好名称 (通过 WMI 查询) |
| 📋 **一体化** | 多项式拟合与串口下载合二为一 |
| ✔️ **实时验证** | 拟合结果可视化，误差一目了然 |
| 📏 **高 DPI 支持** | 适配高分辨率显示器 (PerMonitorV2) |

---

## 📁 项目结构

```
ADC_Calibration_V2/
│
├── mcu/                            # MCU 端代码 (C 语言)
│   ├── CH_Calib.h                  # 校准系统核心头文件
│   ├── CH_Calib.c                  # 校准系统核心实现
│   ├── Calib_Storage.h             # 存储驱动抽象层头文件
│   ├── Calib_Storage.c             # 存储驱动抽象层实现
│   ├── Calib_Storage_Port.c        # 驱动移植示例 (多种存储方案)
│   ├── Examples_HAL_StdLib.c       # 详细使用例程
│   ├── Examples_HAL_StdLib.h       # 详细使用例程头文件
│   ├── main_example.c              # 使用示例代码
│   └── MCU_Calib_Manual.md         # 详细使用例程 & API说明
│
├── pc/                             # PC 端代码 (C# WPF)
│   ├── CalibrationTool.sln         # Visual Studio 解决方案
│   ├── CalibrationTool.csproj      # .NET 项目文件
│   ├── App.xaml                    # 应用程序资源与主题配置
│   ├── App.xaml.cs                 # 应用程序入口
│   ├── MainWindow.xaml             # 主窗体 UI 定义
│   ├── MainWindow.xaml.cs          # 主窗体逻辑代码
│   ├── build.bat                   # 编译脚本
│   ├── publish.bat                 # 发布脚本
│   └── .gitignore                  # Git 忽略配置
│
└── README.md                       # 本文档
```

---

## 🔧 MCU 端使用指南

### 移植概述

系统采用函数指针实现存储抽象，移植只需三步：

1. 实现底层读写函数
2. 定义驱动结构体
3. 注册并使用

### 步骤一：实现底层读写函数

根据你的硬件平台实现以下函数接口：

```c
// 初始化函数 (可选)
static int my_init(void)
{
    // 初始化 I2C / SPI 等外设
    // MX_I2C1_Init();
    return 0;  // 返回 0 表示成功
}

// 读取函数 (必须实现)
static int my_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    // 调用你的 EEPROM/Flash 读取函数
    // I2C_EEPROM_Read((uint16_t)addr, buf, (uint16_t)len);
    return 0;
}

// 写入函数 (必须实现)
static int my_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    // 调用你的 EEPROM/Flash 写入函数
    // 注意: AT24Cxx 等 EEPROM 需要分页写入并等待
    return 0;
}

// 擦除函数 (Flash 必须, EEPROM 可选)
static int my_erase(uint32_t addr, uint32_t len)
{
    // Flash 擦除实现
    return 0;
}

// 就绪检查函数 (可选)
static bool my_ready(void)
{
    return true;
}
```

### 步骤二：定义驱动结构体

```c
const StorageDriver_t g_my_driver = {
    .name        = "AT24C64",       // 驱动名称
    .base_addr   = 0,               // 存储起始地址
    .total_size  = 8192,            // 总容量 (字节)
    .page_size   = 32,              // 页大小 (写入对齐)
    .sector_size = 1,               // 扇区大小 (EEPROM 设为 1)
    
    .init        = my_init,         // 初始化函数 (可为 NULL)
    .read        = my_read,         // 读取函数 (必须)
    .write       = my_write,        // 写入函数 (必须)
    .erase       = NULL,            // 擦除函数 (EEPROM 可为 NULL)
    .is_ready    = my_ready,        // 就绪检查 (可为 NULL)
};
```

### 步骤三：注册并使用

```c
void System_Init(void)
{
    // 1. 注册存储驱动
    Storage_RegisterDriver(&g_my_driver);
    
    // 2. 初始化校准系统
    Calib_Init();
    
    // 3. 注册校准通道 (ID 范围: 1 ~ CALIB_MAX_CHANNELS)
    Calib_RegisterCh(1, "FAN_I");
    Calib_RegisterCh(2, "CR_I");
    Calib_RegisterCh(3, "270_U");
    Calib_RegisterCh(4, "270_I");
    Calib_RegisterCh(5, "CAP_U");
    Calib_RegisterCh(6, "OUT_U");
    Calib_RegisterCh(7, "CAP_I");
    
    // 4. 从存储加载已有校准数据
    Calib_LoadAll();
    
    // 5. 打印调试信息 (可选)
    Calib_PrintInfo();
}
```

### 应用校准

```c
// 方式一：标准单点计算
double raw_adc = 2048.0;
double calibrated;
CalibErr_t err = Calib_Apply(1, raw_adc, &calibrated);
if (err == CALIB_OK) {
    // 使用 calibrated 值
}

// 方式二：批量处理 (适合 DMA 采集)
double raw[100], result[100];
Calib_ApplyBatch(1, raw, result, 100);

// 方式三：高性能内联 (适用于中断/高频采样)
CalibCh_t *ch = &g_calib_ch[0];  // 直接访问全局数组
if (ch->valid) {
    double y = Calib_Compute(ch->coeffs, ch->coeff_cnt, adc_value);
}
```

### 支持的存储方案

在 `Calib_Storage_Port.c` 中提供了以下存储方案的移植示例：

| 存储类型 | 宏定义 | 特点 |
|----------|--------|------|
| AT24Cxx EEPROM | `USE_AT24CXX_STORAGE` | I2C 接口，字节写入，无需擦除 |
| W25Qxx SPI Flash | `USE_W25QXX_STORAGE` | 大容量，需先擦后写，扇区 4KB |
| STM32 内部 Flash | `USE_INTERNAL_FLASH_STORAGE` | 无需外部器件，按页擦除 |
| LittleFS 文件系统 | `USE_LITTLEFS_STORAGE` | 带磨损均衡，掉电安全，推荐复杂应用 |

使用时在编译选项中定义对应的宏即可启用相应驱动。

### 配置选项

在 `CH_Calib.h` 中可调整以下配置：

```c
#define CALIB_MAX_CHANNELS      8       // 最大通道数
#define CALIB_MAX_COEFFS        6       // 最大系数数 (支持最高 5 次多项式)
#define CALIB_NAME_MAX_LEN      16      // 通道名称最大长度
#define CALIB_MAGIC             0xCA1B  // 数据有效标识魔数
```

---

## 💻 PC 端使用指南

### 系统要求

- **操作系统**: Windows 7 / 10 / 11
- **运行环境**: .NET 8.0 Desktop Runtime 或 SDK
- **开发环境**: Visual Studio 2022 (可选)

### 编译运行

**方式一：使用编译脚本**

```batch
build.bat
```

**方式二：命令行编译**

```batch
dotnet restore
dotnet build -c Release
dotnet run
```

**方式三：Visual Studio**

1. 双击打开 `CalibrationTool.sln`
2. 按 F5 运行或 Ctrl+Shift+B 编译

### 发布独立程序

运行 `publish.bat` 可选择以下发布方式：

| 选项 | 说明 | 输出位置 |
|------|------|----------|
| 1 | Framework-dependent (依赖 .NET 运行时，较小) | `publish\framework-dependent\` |
| 2 | Self-contained x64 (独立运行，含运行时) | `publish\win-x64\` |
| 3 | Self-contained x86 (独立运行，32位) | `publish\win-x86\` |

或手动发布：

```batch
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o publish
```

### NuGet 依赖

| 包名 | 版本 | 用途 |
|------|------|------|
| MaterialDesignThemes | 5.1.* | Material Design UI 控件库 |
| MaterialDesignColors | 3.1.* | Material Design 颜色主题 |
| MathNet.Numerics | 5.0.* | 多项式拟合数学计算 (SVD 分解) |
| System.IO.Ports | 8.0.* | 串口通信 |
| System.Management | 8.0.* | WMI 查询 (获取串口友好名称) |

### 功能说明

#### 📊 多项式拟合

1. 在左侧两个表格中分别输入 **ADC 原始值** 和对应的 **真实值**（物理量）
2. 设置多项式阶数（1-5 阶，推荐 3-4 阶以避免过拟合）
3. 点击 **[拟合]** 按钮生成校准公式
4. 查看右侧验证表格，确认各点误差在可接受范围内
5. 使用 **单点验证** 功能测试任意 ADC 值的计算结果
6. 点击 **[复制]** 将公式复制到剪贴板

#### 📡 串口下载

1. 选择串口端口和波特率（默认 115200）
2. 点击 **[连接]** 建立通信
3. 点击 **[回读]** 获取设备端注册的通道列表
4. 从下拉框选择目标通道
5. 输入公式或点击 **[使用拟合结果]** 自动填入
6. 点击 **[发送到设备]** 将校准系数下载到 MCU

#### 📚 使用帮助

应用程序内置使用帮助标签页，包含详细的操作指引和公式格式说明。

---

## 📡 通信协议

### 帧格式

| 字段 | 长度 (字节) | 说明 |
|------|-------------|------|
| Header | 1 | 固定值 `0xAA` |
| Command | 5-7 | ASCII 命令标识字符串 |
| Length | 1 | 帧总长度 (仅写入命令) |
| Data | N | 数据内容 (可变) |
| CRC16 | 2 | CRC-16/MODBUS 校验 (小端序) |

### 命令列表

| 命令 | 标识 | 方向 | 格式说明 |
|------|------|------|----------|
| 写入系数 | `CalCh` | PC → MCU | `0xAA` + `CalCh` + Len(1) + ChID(1) + Coeffs(N×8) + CRC(2) |
| 读取通道列表 | `RBCalCh` | PC → MCU | `0xAA` + `RBCalCh` (共 8 字节) |
| 读取系数 | `RdCoef` | PC → MCU | `0xAA` + `RdCoef` + ChID(1) |

### 响应格式

```
通道列表响应:  CH:1:FAN_I,2:CR_I,3:270_U\r\n
操作成功响应:  OK:CH1,4 coeffs\r\n
```

### CRC16 计算说明

- **算法**: CRC-16/MODBUS (查表法)
- **初始值**: `0xFFFF`
- **多项式**: `0xA001` (反转后)
- **计算范围**: **从 Command 开始，不包含帧头 (0xAA) 和 CRC 字段本身**

---

## 📐 技术细节

### 数据存储结构

```c
typedef struct __attribute__((packed)) {
    uint16_t magic;                     // 魔数 0xCA1B (校验数据有效性)
    uint8_t  ch_id;                     // 通道 ID
    uint8_t  coeff_cnt;                 // 系数数量 (1-6)
    double   coeffs[CALIB_MAX_COEFFS];  // 系数数组 [a0, a1, a2, ..., an]
    uint16_t crc16;                     // CRC 校验
} CalibData_t;  // 总计 56 字节
```

每个通道占用 56 字节存储空间，8 通道共需 448 字节。

### 多项式格式

系数统一采用**升幂排列**：`[a0, a1, a2, ..., an]`

对应多项式：`y = a0 + a1×x + a2×x² + ... + an×xⁿ`

### Horner 法则优化

Horner 法则将多项式计算转换为嵌套形式，显著减少运算次数：

```c
// 原始: y = a0 + a1*x + a2*x² + a3*x³
// Horner: y = a0 + x*(a1 + x*(a2 + x*a3))

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

**优化效果**：n 次多项式从 `2n-1` 次乘法优化为 `n-1` 次。

### PC 端 SVD 拟合

使用 MathNet.Numerics 的奇异值分解 (SVD) 求解最小二乘问题：

```csharp
private static double[] PolynomialFit(List<double> x, List<double> y, int degree)
{
    int n = x.Count;
    // 构建 Vandermonde 矩阵
    var X = Matrix<double>.Build.Dense(n, degree + 1, (i, j) => Math.Pow(x[i], j));
    var Y = Vector<double>.Build.Dense(y.ToArray());
    // SVD 求解
    return X.Svd(true).Solve(Y).ToArray();
}
```

---

## ⚠️ 注意事项

### MCU 端

1. **EEPROM 写入延时**：AT24Cxx 等 EEPROM 每次页写入后需等待约 5ms
2. **Flash 先擦后写**：SPI Flash 和内部 Flash 写入前必须擦除对应扇区
3. **系数数量限制**：最多支持 6 个系数（最高 5 次多项式）
4. **通道数量限制**：默认最大 8 通道，可通过 `CALIB_MAX_CHANNELS` 修改
5. **存储空间规划**：每通道 56 字节，请确保存储区域足够

### PC 端

1. **数据点数量**：多项式阶数必须小于数据点数（n 阶需要至少 n+1 个点）
2. **过拟合风险**：高阶多项式（>4阶）可能导致过拟合，在数据点之外区域误差增大
3. **公式格式**：支持科学计数法，如 `8.03e-9*x^3 + -3.58e-5*x^2 + 1.575*x + -18.58`
4. **串口权限**：部分系统可能需要管理员权限访问串口

---

## 🔄 版本历史

### V2.0.0 (当前版本)

- ✅ PC 端重构为 WPF + Material Design 现代 UI
- ✅ 升级至 .NET 8.0，支持高 DPI 显示
- ✅ 优化多项式拟合算法，采用 SVD 分解
- ✅ 增强串口稳定性，支持 WMI 获取设备友好名称
- ✅ 添加单点验证功能
- ✅ 统一 CRC16 查表法实现
- ✅ 重构存储层，采用函数指针实现驱动抽象
- ✅ 合并多项式拟合工具和串口下载工具
- ✅ 增加 LittleFS 支持

### V1.0.0

- 初始版本

---

## 📄 许可证

MIT License

---

## 👨‍💻 作者

© 2025 胡兴明. All Rights Reserved.
