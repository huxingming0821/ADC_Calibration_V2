using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO.Ports;
using System.Management;
using System.Runtime.CompilerServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Threading;
using MaterialDesignThemes.Wpf;
using MathNet.Numerics.LinearAlgebra;
using MathNet.Numerics.LinearAlgebra.Double;

namespace CalibrationTool;

public partial class MainWindow : Window
{
    // Dialog host identifier
    private const string DialogHostId = "RootDialog";
    #region Constants
    private const byte FrameHeader = 0xAA;
    private const ushort Crc16Init = 0xFFFF;
    private const ushort Crc16Poly = 0xA001;
    private const int RxTimeoutMs = 150;
    private static readonly ushort[] Crc16Table = GenerateCrc16Table();
    #endregion

    #region Fields
    private SerialPort? _serial;
    private System.Timers.Timer? _rxTimer;
    private readonly StringBuilder _rxBuffer = new();
    private readonly List<byte> _rxBinaryBuffer = new();  // 二进制数据缓冲区
    private bool _expectBinaryResponse;                    // 是否期待二进制响应
    private readonly Dictionary<string, string> _channelMap = new();
    private readonly Dictionary<string, string> _portMap = new();
    private double[]? _coefficients;
    private bool _isConnected;
    private readonly object _lockObj = new();
    private Encoding _serialEncoding = Encoding.UTF8;     // 串口文本编码

    // Data collections for DataGrids
    public ObservableCollection<DataItem> AdcItems { get; } = new();
    public ObservableCollection<DataItem> RealItems { get; } = new();
    public ObservableCollection<VerifyItem> VerifyItems { get; } = new();
    #endregion

    #region Constructor
    public MainWindow()
    {
        // 注册GB2312编码提供程序
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
        
        InitializeComponent();
        
        // Bind data sources
        dgAdc.ItemsSource = AdcItems;
        dgReal.ItemsSource = RealItems;
        dgVerify.ItemsSource = VerifyItems;

        // Initialize serial port
        _serial = new SerialPort();
        _serial.DataReceived += Serial_DataReceived;
        _serial.ErrorReceived += Serial_ErrorReceived;

        // Setup timer
        _rxTimer = new System.Timers.Timer(RxTimeoutMs);
        _rxTimer.AutoReset = false;
        _rxTimer.Elapsed += RxTimer_Elapsed;

        // Refresh ports on load
        Loaded += (s, e) =>
        {
            RefreshPorts();
            Log("程序启动就绪");
        };
    }
    #endregion

    #region Polynomial Fitting
    private void BtnFit_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            var adcValues = GetGridValues(AdcItems);
            var realValues = GetGridValues(RealItems);

            if (adcValues.Count == 0 || realValues.Count == 0)
            {
                ShowError("请输入ADC值和真实值");
                return;
            }

            if (adcValues.Count != realValues.Count)
            {
                ShowError($"数据数量不匹配: ADC({adcValues.Count}) vs 真实值({realValues.Count})");
                return;
            }

            int degree = cbDegree.SelectedIndex + 1;
            if (degree >= adcValues.Count)
            {
                ShowError($"多项式阶数({degree})不能大于等于数据点数({adcValues.Count})");
                return;
            }

            _coefficients = PolynomialFit(adcValues, realValues, degree);

            txtResult.Text = "y = " + FormatFormula(_coefficients);
            txtResult.Foreground = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#262626"));

            VerifyItems.Clear();
            double maxError = 0;
            
            for (int i = 0; i < adcValues.Count; i++)
            {
                double predicted = EvaluatePolynomial(_coefficients, adcValues[i]);
                double error = Math.Abs(realValues[i]) > 1e-10 
                    ? Math.Abs(predicted - realValues[i]) / Math.Abs(realValues[i]) * 100 
                    : 0;
                if (error > maxError) maxError = error;

                VerifyItems.Add(new VerifyItem
                {
                    Adc = adcValues[i].ToString("F2"),
                    Predicted = predicted.ToString("F6"),
                    Actual = realValues[i].ToString("F6"),
                    Error = error.ToString("F4") + "%",
                    ErrorBrush = error > 1 
                        ? new SolidColorBrush((Color)ColorConverter.ConvertFromString("#F5222D")) 
                        : new SolidColorBrush((Color)ColorConverter.ConvertFromString("#262626"))
                });
            }

            ShowSuccess($"拟合完成! 最大误差: {maxError:F4}%");
            UpdateStatus($"拟合完成 | {degree}阶 | 最大误差: {maxError:F4}%", true);
        }
        catch (Exception ex)
        {
            ShowError("拟合失败: " + ex.Message);
        }
    }

    private void BtnClear_Click(object sender, RoutedEventArgs e)
    {
        AdcItems.Clear();
        RealItems.Clear();
        VerifyItems.Clear();
        txtResult.Text = "拟合结果将显示在这里...";
        txtResult.Foreground = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#8C8C8C"));
        txtSingleAdc.Text = "";
        txtSingleReal.Text = "";
        txtSingleResult.Text = "";
        _coefficients = null;
        UpdateStatus("就绪", true);
    }

    private void BtnCopy_Click(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrEmpty(txtResult.Text) || txtResult.Text.Contains("将显示"))
        {
            ShowWarning("没有可复制的公式");
            return;
        }
        Clipboard.SetText(txtResult.Text);
        ShowSuccess("公式已复制到剪贴板");
    }

    private void BtnCalcSingle_Click(object sender, RoutedEventArgs e)
    {
        if (_coefficients == null || _coefficients.Length == 0)
        {
            ShowWarning("请先进行拟合计算");
            return;
        }

        if (!double.TryParse(txtSingleAdc.Text, out double adcValue))
        {
            ShowWarning("请输入有效的ADC值");
            return;
        }

        double predicted = EvaluatePolynomial(_coefficients, adcValue);

        if (double.TryParse(txtSingleReal.Text, out double realValue) && Math.Abs(realValue) > 1e-10)
        {
            double error = Math.Abs(predicted - realValue) / Math.Abs(realValue) * 100;
            txtSingleResult.Text = $"ADC: {adcValue}\r\n预测值: {predicted:F6}\r\n实际值: {realValue}\r\n误差: {error:F4}%";
        }
        else
        {
            txtSingleResult.Text = $"ADC: {adcValue}\r\n预测值: {predicted:F6}";
        }
    }

    private static double[] PolynomialFit(List<double> x, List<double> y, int degree)
    {
        int n = x.Count;
        var X = Matrix<double>.Build.Dense(n, degree + 1, (i, j) => Math.Pow(x[i], j));
        var Y = Vector<double>.Build.Dense(y.ToArray());
        return X.Svd(true).Solve(Y).ToArray();
    }

    private static double EvaluatePolynomial(double[] coeffs, double x)
    {
        double y = coeffs[coeffs.Length - 1];
        for (int i = coeffs.Length - 2; i >= 0; i--)
            y = y * x + coeffs[i];
        return y;
    }

    private static string FormatFormula(double[] coeffs)
    {
        var parts = new List<string>();
        for (int i = coeffs.Length - 1; i >= 0; i--)
        {
            string term = i switch
            {
                0 => coeffs[i].ToString("G10"),
                1 => $"{coeffs[i]:G10}*x",
                _ => $"{coeffs[i]:G10}*x^{i}"
            };
            parts.Add(term);
        }
        return string.Join(" + ", parts);
    }

    private static List<double> GetGridValues(ObservableCollection<DataItem> items)
    {
        var values = new List<double>();
        foreach (var item in items)
        {
            if (!string.IsNullOrWhiteSpace(item.Value) && double.TryParse(item.Value, out double val))
                values.Add(val);
        }
        return values;
    }
    #endregion

    #region Serial Communication
    private void RefreshPorts()
    {
        cbPort.Items.Clear();
        _portMap.Clear();

        try
        {
            var ports = SerialPort.GetPortNames();
            if (ports.Length == 0)
            {
                return;
            }

            try
            {
                using var searcher = new ManagementObjectSearcher("SELECT Name FROM Win32_PnPEntity WHERE Name LIKE '%(COM%)'");
                foreach (var device in searcher.Get())
                {
                    if (device["Name"] is string name)
                    {
                        var match = Regex.Match(name, @"\(COM(\d+)\)");
                        if (match.Success)
                        {
                            var portName = $"COM{match.Groups[1].Value}";
                            if (ports.Contains(portName))
                            {
                                var displayName = $"{portName} - {name.Replace($"({portName})", "").Trim()}";
                                if (!_portMap.ContainsKey(displayName))
                                {
                                    _portMap[displayName] = portName;
                                    cbPort.Items.Add(displayName);
                                }
                            }
                        }
                    }
                }
            }
            catch { /* Ignore WMI errors */ }

            foreach (var port in ports)
            {
                if (!_portMap.Values.Contains(port))
                {
                    _portMap[port] = port;
                    cbPort.Items.Add(port);
                }
            }

            if (cbPort.Items.Count > 0)
                cbPort.SelectedIndex = 0;
        }
        catch (Exception ex)
        {
            ShowError("刷新端口失败: " + ex.Message);
        }
    }

    private void BtnRefresh_Click(object sender, RoutedEventArgs e)
    {
        RefreshPorts();
    }

    private void BtnConnect_Click(object sender, RoutedEventArgs e)
    {
        if (_isConnected)
            Disconnect();
        else
            Connect();
    }

    private void Connect()
    {
        try
        {
            if (cbPort.SelectedItem == null || !_portMap.TryGetValue(cbPort.SelectedItem.ToString()!, out string? portName))
            {
                ShowWarning("请选择有效的串口");
                return;
            }

            if (_serial == null) return;

            _serial.PortName = portName;
            _serial.BaudRate = int.Parse(((ComboBoxItem)cbBaudRate.SelectedItem).Content.ToString()!);
            _serial.DataBits = 8;
            _serial.Parity = Parity.None;
            _serial.StopBits = StopBits.One;
            _serial.ReadTimeout = 1000;
            _serial.WriteTimeout = 1000;

            _serial.Open();
            _isConnected = true;

            UpdateConnectionUI(true);
            Log($"已连接 {cbPort.SelectedItem} @ {_serial.BaudRate}");
            ShowSuccess("串口连接成功");
        }
        catch (Exception ex)
        {
            ShowError("连接失败: " + ex.Message);
            Log($"连接失败: {ex.Message}");
        }
    }

    private void Disconnect()
    {
        try
        {
            if (_serial?.IsOpen == true)
                _serial.Close();
            
            _isConnected = false;
            _channelMap.Clear();
            cbChannel.Items.Clear();
            UpdateConnectionUI(false);
            Log("已断开连接");
        }
        catch (Exception ex)
        {
            ShowError("断开失败: " + ex.Message);
        }
    }

    private void UpdateConnectionUI(bool connected)
    {
        _isConnected = connected;
        
        txtConnect.Text = connected ? "断开" : "连接";
        iconConnect.Kind = connected 
            ? PackIconKind.LanDisconnect 
            : PackIconKind.LanConnect;
        
        btnConnect.Background = connected 
            ? new SolidColorBrush((Color)ColorConverter.ConvertFromString("#F5222D"))
            : new SolidColorBrush((Color)ColorConverter.ConvertFromString("#52C41A"));
        btnConnect.BorderBrush = btnConnect.Background;

        statusIndicator.Fill = connected 
            ? new SolidColorBrush((Color)ColorConverter.ConvertFromString("#52C41A"))
            : new SolidColorBrush((Color)ColorConverter.ConvertFromString("#F5222D"));
        
        txtConnStatus.Text = connected ? "已连接" : "未连接";
        txtConnStatus.Foreground = connected 
            ? new SolidColorBrush((Color)ColorConverter.ConvertFromString("#52C41A"))
            : new SolidColorBrush((Color)ColorConverter.ConvertFromString("#F5222D"));

        btnReadChannels.IsEnabled = connected;
        btnSendFormula.IsEnabled = connected;
        btnReadCoeffs.IsEnabled = connected;
        cbChannel.IsEnabled = connected;
    }

    private void BtnReadChannels_Click(object sender, RoutedEventArgs e)
    {
        if (!_isConnected)
        {
            ShowWarning("请先连接串口");
            return;
        }

        try
        {
            _rxBuffer.Clear();
            byte[] cmd = [0xAA, 0x52, 0x42, 0x43, 0x61, 0x6C, 0x43, 0x68];
            _serial?.Write(cmd, 0, cmd.Length);
            Log("TX: 回读通道命令");
        }
        catch (Exception ex)
        {
            ShowError("发送失败: " + ex.Message);
        }
    }

    private void BtnSendFormula_Click(object sender, RoutedEventArgs e)
    {
        if (!_isConnected)
        {
            ShowWarning("请先连接串口");
            return;
        }

        if (cbChannel.SelectedIndex < 0)
        {
            ShowWarning("请先选择通道");
            return;
        }

        try
        {
            var formulaText = txtFormula.Text;
            if (string.IsNullOrWhiteSpace(formulaText))
            {
                ShowWarning("请输入公式");
                return;
            }

            var coeffs = ParseFormula(formulaText);
            if (coeffs == null || coeffs.Length == 0)
            {
                ShowWarning("公式解析失败");
                return;
            }

            if (coeffs.Length > 6)
            {
                ShowWarning("最多支持6个系数");
                return;
            }

            string chName = cbChannel.SelectedItem.ToString()!;
            if (!_channelMap.TryGetValue(chName, out string? chIdStr) || !byte.TryParse(chIdStr, out byte chId))
            {
                ShowError("通道ID无效");
                return;
            }

            byte[] frame = BuildWriteFrame(chId, coeffs);
            _serial?.Write(frame, 0, frame.Length);
            Log($"TX: 发送CH{chId}系数 [{string.Join(", ", coeffs.Select(c => c.ToString("E4")))}]");
            ShowSuccess($"已发送到通道 {chName}");
        }
        catch (Exception ex)
        {
            ShowError("发送失败: " + ex.Message);
        }
    }

    private void BtnUseFormula_Click(object sender, RoutedEventArgs e)
    {
        if (_coefficients == null || _coefficients.Length == 0)
        {
            ShowWarning("请先进行拟合计算");
            return;
        }

        txtFormula.Text = "y = " + FormatFormula(_coefficients);
        ShowSuccess("已填入拟合公式");
    }

    private void BtnReadCoeffs_Click(object sender, RoutedEventArgs e)
    {
        if (!_isConnected)
        {
            ShowWarning("请先连接串口");
            return;
        }

        if (cbChannel.SelectedIndex < 0)
        {
            ShowWarning("请先选择通道");
            return;
        }

        try
        {
            string chName = cbChannel.SelectedItem.ToString()!;
            if (!_channelMap.TryGetValue(chName, out string? chIdStr) || !byte.TryParse(chIdStr, out byte chId))
            {
                ShowError("通道ID无效");
                return;
            }

            // 清空缓冲区，设置期待二进制响应
            lock (_lockObj)
            {
                _rxBuffer.Clear();
                _rxBinaryBuffer.Clear();
                _expectBinaryResponse = true;
            }

            // 构建读取系数命令: AA + "RdCoef" + ch_id
            byte[] cmd = new byte[8];
            cmd[0] = FrameHeader;    // 0xAA
            cmd[1] = (byte)'R';
            cmd[2] = (byte)'d';
            cmd[3] = (byte)'C';
            cmd[4] = (byte)'o';
            cmd[5] = (byte)'e';
            cmd[6] = (byte)'f';
            cmd[7] = chId;

            _serial?.Write(cmd, 0, cmd.Length);
            Log($"TX: 读取CH{chId}系数命令");
        }
        catch (Exception ex)
        {
            ShowError("发送失败: " + ex.Message);
        }
    }

    private void BtnClearLog_Click(object sender, RoutedEventArgs e)
    {
        txtLog.Text = "";
    }

    private void CbEncoding_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        // 防止初始化时触发
        if (!IsLoaded || cbEncoding?.SelectedItem is not ComboBoxItem item) return;
        
        string encoding = item.Content?.ToString() ?? "UTF-8";
        _serialEncoding = encoding switch
        {
            "GB2312" => Encoding.GetEncoding("GB2312"),
            _ => Encoding.UTF8
        };
        Log($"编码已切换为: {encoding}");
    }

    private void Serial_DataReceived(object sender, SerialDataReceivedEventArgs e)
    {
        try
        {
            if (_serial?.IsOpen != true) return;
            
            int count = _serial.BytesToRead;
            byte[] buffer = new byte[count];
            _serial.Read(buffer, 0, count);
            
            lock (_lockObj)
            {
                if (_expectBinaryResponse)
                {
                    // 二进制模式：存入字节缓冲区
                    _rxBinaryBuffer.AddRange(buffer);
                }
                else
                {
                    // 文本模式：使用选定的编码存入字符串缓冲区
                    _rxBuffer.Append(_serialEncoding.GetString(buffer));
                }
            }
            
            _rxTimer?.Stop();
            _rxTimer?.Start();
        }
        catch { /* Ignore */ }
    }

    private void Serial_ErrorReceived(object sender, SerialErrorReceivedEventArgs e)
    {
        Dispatcher.Invoke(() => Log($"串口错误: {e.EventType}"));
    }

    private void RxTimer_Elapsed(object? sender, System.Timers.ElapsedEventArgs e)
    {
        Dispatcher.Invoke(() =>
        {
            try
            {
                bool isBinary;
                string textData = "";
                byte[] binaryData = Array.Empty<byte>();

                lock (_lockObj)
                {
                    isBinary = _expectBinaryResponse;
                    if (isBinary)
                    {
                        binaryData = _rxBinaryBuffer.ToArray();
                        _rxBinaryBuffer.Clear();
                        _expectBinaryResponse = false;
                    }
                    else
                    {
                        textData = _rxBuffer.ToString().Trim();
                        _rxBuffer.Clear();
                    }
                }

                if (isBinary && binaryData.Length > 0)
                {
                    // 处理二进制响应 (系数读取)
                    Log($"RX: [{string.Join(" ", binaryData.Select(b => b.ToString("X2")))}]");
                    ParseCoefficientsResponse(binaryData);
                }
                else if (!string.IsNullOrEmpty(textData))
                {
                    // 处理文本响应
                    Log($"RX: {textData}");
                    
                    if (textData.StartsWith("CH:"))
                        ParseChannels(textData);
                    else if (textData.StartsWith("OK:"))
                        ShowSuccess(textData);
                }
            }
            catch (Exception ex)
            {
                Log($"处理错误: {ex.Message}");
            }
        });
    }

    private void ParseChannels(string data)
    {
        _channelMap.Clear();
        cbChannel.Items.Clear();

        foreach (var item in data[3..].Trim().Split(',', StringSplitOptions.RemoveEmptyEntries))
        {
            var parts = item.Split(':');
            if (parts.Length == 2)
            {
                _channelMap[parts[1].Trim()] = parts[0].Trim();
                cbChannel.Items.Add(parts[1].Trim());
            }
        }

        if (cbChannel.Items.Count > 0)
        {
            cbChannel.SelectedIndex = 0;
            ShowSuccess($"获取到 {cbChannel.Items.Count} 个通道");
        }
    }

    /// <summary>
    /// 解析系数响应帧
    /// 格式: AA + "Coef" + len + ch_id + coeff_cnt + coeffs[coeff_cnt * 8] + CRC16(2)
    /// </summary>
    private void ParseCoefficientsResponse(byte[] data)
    {
        try
        {
            // 最小帧长度: 1(AA) + 4(Coef) + 1(len) + 1(ch_id) + 1(coeff_cnt) + 8(至少1个系数) + 2(CRC) = 18
            if (data.Length < 18)
            {
                ShowError("响应数据长度不足");
                return;
            }

            // 检查帧头
            if (data[0] != FrameHeader)
            {
                ShowError("响应帧头错误");
                return;
            }

            // 检查命令字 "Coef"
            if (data[1] != 'C' || data[2] != 'o' || data[3] != 'e' || data[4] != 'f')
            {
                ShowError("响应命令字错误");
                return;
            }

            byte frameLen = data[5];
            if (data.Length < frameLen)
            {
                ShowError("响应数据不完整");
                return;
            }

            // 验证CRC
            ushort recvCrc = (ushort)(data[frameLen - 2] | (data[frameLen - 1] << 8));
            ushort calcCrc = CalcCrc16(data, 1, frameLen - 3);
            if (recvCrc != calcCrc)
            {
                ShowError($"CRC校验失败 (收到:{recvCrc:X4}, 计算:{calcCrc:X4})");
                return;
            }

            byte chId = data[6];
            byte coeffCnt = data[7];

            if (coeffCnt == 0 || coeffCnt > 6)
            {
                ShowError($"系数数量无效: {coeffCnt}");
                return;
            }

            // 解析系数 (每个系数8字节, double)
            double[] coeffs = new double[coeffCnt];
            for (int i = 0; i < coeffCnt; i++)
            {
                coeffs[i] = BitConverter.ToDouble(data, 8 + i * 8);
            }

            // 生成公式字符串
            string formula = "y = " + FormatFormula(coeffs);
            
            // 更新串口下载页的公式输入框
            txtFormula.Text = formula;

            // 同时更新拟合结果区域（多项式拟合页），使用户可以进行模拟计算
            _coefficients = coeffs;
            txtResult.Text = formula;
            txtResult.Foreground = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#262626"));

            Log($"成功读取CH{chId}系数: [{string.Join(", ", coeffs.Select(c => c.ToString("E4")))}]");
            ShowSuccess($"已读取通道{chId}的{coeffCnt}个系数，可在\"多项式拟合\"页进行模拟计算");
        }
        catch (Exception ex)
        {
            ShowError($"解析系数失败: {ex.Message}");
            Log($"解析异常: {ex}");
        }
    }

    private static double[]? ParseFormula(string formula)
    {
        if (string.IsNullOrWhiteSpace(formula)) return null;

        // 移除 "y = " 前缀
        formula = Regex.Replace(formula, @"^\s*y\s*=\s*", "", RegexOptions.IgnoreCase);
        formula = formula.Replace(" ", "");

        var coeffDict = new Dictionary<int, double>();
        int maxPower = 0;

        // 使用正则直接匹配所有多项式项（包括科学计数法）
        // 匹配模式: 可选符号 + 可选数字(含科学计数法) + 可选的 *x^n
        // 支持: -3.6E-05*x^2, 0.37*x, -120.6, +x, -x^2 等格式
        string pattern = @"([+-]?(?:\d+\.?\d*|\d*\.\d+)?(?:[Ee][+-]?\d+)?)\*?([xX](?:\^(\d+))?)?";
        
        var matches = Regex.Matches(formula, pattern);
        
        foreach (Match match in matches)
        {
            if (!match.Success || string.IsNullOrEmpty(match.Value)) continue;
            
            string coeffStr = match.Groups[1].Value;
            bool hasX = match.Groups[2].Success && !string.IsNullOrEmpty(match.Groups[2].Value);
            
            // 跳过空匹配或只有符号但没有x的情况
            if (string.IsNullOrEmpty(coeffStr) && !hasX) continue;
            if ((coeffStr == "+" || coeffStr == "-") && !hasX) continue;
            
            // 处理只有符号的情况 (如 +x, -x, +, -)
            if (string.IsNullOrEmpty(coeffStr))
                coeffStr = "1";
            else if (coeffStr == "+" || coeffStr == "-")
                coeffStr += "1";
            
            if (!double.TryParse(coeffStr, System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out double coeff))
                continue;

            int power = 0;
            if (match.Groups[2].Success && !string.IsNullOrEmpty(match.Groups[2].Value))
            {
                // 有 x 项
                power = 1;
                if (match.Groups[3].Success && !string.IsNullOrEmpty(match.Groups[3].Value))
                {
                    // 有幂次
                    power = int.Parse(match.Groups[3].Value);
                }
            }

            if (coeffDict.ContainsKey(power))
                coeffDict[power] += coeff;
            else
                coeffDict[power] = coeff;

            if (power > maxPower) maxPower = power;
        }

        if (coeffDict.Count == 0) return null;

        var result = new double[maxPower + 1];
        for (int i = 0; i <= maxPower; i++)
            result[i] = coeffDict.TryGetValue(i, out double value) ? value : 0;

        return result;
    }

    private static byte[] BuildWriteFrame(byte chId, double[] coeffs)
    {
        int totalLen = 1 + 5 + 1 + 1 + coeffs.Length * 8 + 2;
        byte[] frame = new byte[totalLen];
        int idx = 0;

        frame[idx++] = FrameHeader;
        frame[idx++] = (byte)'C';
        frame[idx++] = (byte)'a';
        frame[idx++] = (byte)'l';
        frame[idx++] = (byte)'C';
        frame[idx++] = (byte)'h';
        frame[idx++] = (byte)totalLen;
        frame[idx++] = chId;

        foreach (var c in coeffs)
        {
            Array.Copy(BitConverter.GetBytes(c), 0, frame, idx, 8);
            idx += 8;
        }

        ushort crc = CalcCrc16(frame, 1, totalLen - 3);
        frame[idx++] = (byte)(crc & 0xFF);
        frame[idx] = (byte)((crc >> 8) & 0xFF);

        return frame;
    }
    #endregion

    #region CRC16
    private static ushort[] GenerateCrc16Table()
    {
        var table = new ushort[256];
        for (int i = 0; i < 256; i++)
        {
            ushort crc = (ushort)i;
            for (int j = 0; j < 8; j++)
                crc = (crc & 1) != 0 ? (ushort)((crc >> 1) ^ Crc16Poly) : (ushort)(crc >> 1);
            table[i] = crc;
        }
        return table;
    }

    private static ushort CalcCrc16(byte[] data, int start, int len)
    {
        ushort crc = Crc16Init;
        for (int i = start; i < start + len; i++)
            crc = (ushort)((crc >> 8) ^ Crc16Table[(crc ^ data[i]) & 0xFF]);
        return crc;
    }
    #endregion

    #region Helper Methods
    private void Log(string msg)
    {
        if (!Dispatcher.CheckAccess())
        {
            Dispatcher.Invoke(() => Log(msg));
            return;
        }

        txtLog.AppendText($"[{DateTime.Now:HH:mm:ss}] {msg}\r\n");
        txtLog.ScrollToEnd();
    }

    private void UpdateStatus(string msg, bool success)
    {
        txtStatus.Text = msg;
        var color = success 
            ? (Color)ColorConverter.ConvertFromString("#52C41A") 
            : (Color)ColorConverter.ConvertFromString("#F5222D");
        txtStatus.Foreground = new SolidColorBrush(color);
        statusIcon.Foreground = new SolidColorBrush(color);
        statusIcon.Kind = success 
            ? PackIconKind.CheckCircle 
            : PackIconKind.AlertCircle;
    }
    #endregion

    #region Toast Notification System
    
    /// <summary>
    /// Toast消息类型
    /// </summary>
    private enum ToastType
    {
        Success,
        Warning,
        Error,
        Info
    }

    /// <summary>
    /// 显示成功提示
    /// </summary>
    private void ShowSuccess(string msg) => ShowToast(msg, ToastType.Success);
    
    /// <summary>
    /// 显示警告提示
    /// </summary>
    private void ShowWarning(string msg) => ShowToast(msg, ToastType.Warning);
    
    /// <summary>
    /// 显示错误提示
    /// </summary>
    private void ShowError(string msg) => ShowToast(msg, ToastType.Error);

    /// <summary>
    /// 显示Toast气泡提示（AntDesign风格）
    /// </summary>
    private void ShowToast(string message, ToastType type, int durationMs = 3000)
    {
        if (!Dispatcher.CheckAccess())
        {
            Dispatcher.Invoke(() => ShowToast(message, type, durationMs));
            return;
        }

        // 获取类型对应的颜色和图标
        var (iconKind, bgColor, iconColor) = type switch
        {
            ToastType.Success => (PackIconKind.CheckCircle, "#F6FFED", "#52C41A"),
            ToastType.Warning => (PackIconKind.AlertCircle, "#FFFBE6", "#FAAD14"),
            ToastType.Error => (PackIconKind.CloseCircle, "#FFF2F0", "#FF4D4F"),
            ToastType.Info => (PackIconKind.InformationOutline, "#E6F7FF", "#1890FF"),
            _ => (PackIconKind.InformationOutline, "#E6F7FF", "#1890FF")
        };

        // 创建Toast容器
        var toastBorder = new Border
        {
            Background = new SolidColorBrush((Color)ColorConverter.ConvertFromString(bgColor)!),
            BorderBrush = new SolidColorBrush((Color)ColorConverter.ConvertFromString(iconColor)!),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(8),
            Padding = new Thickness(16, 12, 16, 12),
            Margin = new Thickness(0, 0, 0, 8),
            Opacity = 0,
            RenderTransform = new TranslateTransform(0, -20),
            Effect = new System.Windows.Media.Effects.DropShadowEffect
            {
                BlurRadius = 12,
                ShadowDepth = 4,
                Opacity = 0.15,
                Direction = 270
            },
            MinWidth = 200,
            MaxWidth = 400
        };

        // 创建内容面板
        var contentPanel = new StackPanel
        {
            Orientation = Orientation.Horizontal
        };

        // 图标
        var icon = new PackIcon
        {
            Kind = iconKind,
            Width = 20,
            Height = 20,
            Foreground = new SolidColorBrush((Color)ColorConverter.ConvertFromString(iconColor)!),
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(0, 0, 10, 0)
        };

        // 消息文本
        var messageText = new TextBlock
        {
            Text = message,
            FontSize = 14,
            Foreground = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#262626")!),
            VerticalAlignment = VerticalAlignment.Center,
            TextWrapping = TextWrapping.Wrap,
            MaxWidth = 340
        };

        contentPanel.Children.Add(icon);
        contentPanel.Children.Add(messageText);
        toastBorder.Child = contentPanel;

        // 添加到容器
        ToastContainer.Items.Add(toastBorder);

        // 淡入动画
        var fadeInOpacity = new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(300))
        {
            EasingFunction = new CubicEase { EasingMode = EasingMode.EaseOut }
        };
        var slideIn = new DoubleAnimation(-20, 0, TimeSpan.FromMilliseconds(300))
        {
            EasingFunction = new CubicEase { EasingMode = EasingMode.EaseOut }
        };

        toastBorder.BeginAnimation(OpacityProperty, fadeInOpacity);
        ((TranslateTransform)toastBorder.RenderTransform).BeginAnimation(TranslateTransform.YProperty, slideIn);

        // 设置自动消失定时器
        var timer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(durationMs)
        };
        timer.Tick += (s, args) =>
        {
            timer.Stop();
            
            // 淡出动画
            var fadeOutOpacity = new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(300))
            {
                EasingFunction = new CubicEase { EasingMode = EasingMode.EaseIn }
            };
            var slideOut = new DoubleAnimation(0, -20, TimeSpan.FromMilliseconds(300))
            {
                EasingFunction = new CubicEase { EasingMode = EasingMode.EaseIn }
            };

            fadeOutOpacity.Completed += (sender, e) =>
            {
                ToastContainer.Items.Remove(toastBorder);
            };

            toastBorder.BeginAnimation(OpacityProperty, fadeOutOpacity);
            ((TranslateTransform)toastBorder.RenderTransform).BeginAnimation(TranslateTransform.YProperty, slideOut);
        };
        timer.Start();
    }

    #endregion

    protected override void OnClosing(CancelEventArgs e)
    {
        base.OnClosing(e);
        Disconnect();
        _rxTimer?.Stop();
        _rxTimer?.Dispose();
        _serial?.Dispose();
    }
}

#region Data Models
public class DataItem : INotifyPropertyChanged
{
    private string? _value;
    
    public string? Value
    {
        get => _value;
        set
        {
            _value = value;
            OnPropertyChanged();
        }
    }

    public event PropertyChangedEventHandler? PropertyChanged;
    
    protected void OnPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}

public class VerifyItem
{
    public string? Adc { get; set; }
    public string? Predicted { get; set; }
    public string? Actual { get; set; }
    public string? Error { get; set; }
    public SolidColorBrush? ErrorBrush { get; set; }
}
#endregion
