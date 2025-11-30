using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO.Ports;
using System.Linq;
using System.Management;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using MathNet.Numerics.LinearAlgebra;
using MathNet.Numerics.LinearAlgebra.Double;

namespace CalibrationTool
{
    public partial class MainForm : Form
    {
        #region 常量
        private const byte FrameHeader = 0xAA;
        private const ushort Crc16Init = 0xFFFF;
        private const ushort Crc16Poly = 0xA001;
        private const int RxTimeoutMs = 150;
        private const string AppVersion = "V2.0.0";
        
        // 颜色主题
        private static readonly Color PrimaryColor = Color.FromArgb(24, 144, 255);
        private static readonly Color SuccessColor = Color.FromArgb(82, 196, 26);
        private static readonly Color ErrorColor = Color.FromArgb(245, 34, 45);
        private static readonly Color BgColor = Color.FromArgb(240, 242, 245);
        private static readonly Color CardBgColor = Color.White;
        private static readonly Color BorderColor = Color.FromArgb(217, 217, 217);
        private static readonly Color TextColor = Color.FromArgb(38, 38, 38);
        private static readonly Color TextSecondaryColor = Color.FromArgb(140, 140, 140);
        #endregion

        #region 私有字段
        private SerialPort _serial;
        private System.Timers.Timer _rxTimer;
        private readonly StringBuilder _rxBuffer = new StringBuilder();
        private readonly Dictionary<string, string> _channelMap = new Dictionary<string, string>();
        private readonly Dictionary<string, string> _portMap = new Dictionary<string, string>();
        private double[] _coefficients;
        private string _currentPort;
        private bool _isConnected;
        private readonly object _lockObj = new object();
        private static readonly ushort[] Crc16Table = GenerateCrc16Table();
        
        // DPI缩放因子
        private float _dpiScale = 1.0f;
        #endregion

        #region 构造函数
        public MainForm()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            SuspendLayout();
            // 
            // MainForm
            // 
            AutoScaleDimensions = new SizeF(192F, 192F);
            AutoScaleMode = AutoScaleMode.Dpi;
            ClientSize = new Size(548, 458);
            Margin = new Padding(6);
            Name = "MainForm";
            StartPosition = FormStartPosition.WindowsDefaultBounds;
            Text = "CalibrationTool";
            ResumeLayout(false);
        }

        protected override void OnLoad(EventArgs e)
        {
            base.OnLoad(e);
            
            // 计算DPI缩放因子
            using (var g = this.CreateGraphics())
            {
                _dpiScale = g.DpiX / 96f;
            }
            
            // 根据DPI设置窗体大小
            this.ClientSize = ScaleSize(new Size(1100, 750));
            this.MinimumSize = ScaleSize(new Size(900, 600));
            
            // 设置字体（会随DPI自动缩放）
            this.Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Regular, GraphicsUnit.Point);
            
            // 构建UI
            SetupUI();
            SetupEvents();
            SetupTimer();
            
            // 初始化串口
            _serial = new SerialPort();
            _serial.DataReceived += Serial_DataReceived;
            _serial.ErrorReceived += Serial_ErrorReceived;
            
            cbBaudRate.Items.AddRange(new object[] { 
                "9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600" 
            });
            cbBaudRate.SelectedIndex = 4;
            
            RefreshPorts();
            UpdateConnectionUI(false);
            Log("程序启动就绪");
        }
        
        // DPI缩放辅助方法
        private int Scale(int value) => (int)(value * _dpiScale);
        private float Scale(float value) => value * _dpiScale;
        private Size ScaleSize(Size size) => new Size(Scale(size.Width), Scale(size.Height));
        private Padding ScalePadding(Padding p) => new Padding(Scale(p.Left), Scale(p.Top), Scale(p.Right), Scale(p.Bottom));
        
        protected override void OnDpiChanged(DpiChangedEventArgs e)
        {
            base.OnDpiChanged(e);
            _dpiScale = e.DeviceDpiNew / 96f;
            // 窗体会自动重新缩放
        }
        #endregion

        #region UI设置
        private void SetupUI()
        {
            var mainContainer = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                ColumnCount = 1,
                RowCount = 2,
                Padding = ScalePadding(new Padding(10)),
                BackColor = BgColor
            };
            mainContainer.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            mainContainer.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(32)));
            
            var tabControl = new TabControl
            {
                Dock = DockStyle.Fill,
                Font = new Font("Microsoft YaHei UI", 10F, FontStyle.Regular, GraphicsUnit.Point),
                Padding = new Point(Scale(12), Scale(4)),
            };
            
            var tabFit = new TabPage { Text = "📊 多项式拟合", BackColor = BgColor, Padding = ScalePadding(new Padding(6)) };
            tabFit.Controls.Add(CreateFitPanel());
            
            var tabSerial = new TabPage { Text = "📡 串口下载", BackColor = BgColor, Padding = ScalePadding(new Padding(6)) };
            tabSerial.Controls.Add(CreateSerialPanel());
            
            var tabHelp = new TabPage { Text = "❓ 使用帮助", BackColor = BgColor, Padding = ScalePadding(new Padding(6)) };
            tabHelp.Controls.Add(CreateHelpPanel());
            
            tabControl.TabPages.Add(tabFit);
            tabControl.TabPages.Add(tabSerial);
            tabControl.TabPages.Add(tabHelp);
            
            mainContainer.Controls.Add(tabControl, 0, 0);
            mainContainer.Controls.Add(CreateStatusBar(), 0, 1);
            this.Controls.Add(mainContainer);
        }

        private Panel CreateStatusBar()
        {
            var statusBar = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = CardBgColor,
                Padding = ScalePadding(new Padding(10, 0, 10, 0))
            };
            
            statusBar.Paint += (s, e) =>
            {
                using (var pen = new Pen(BorderColor))
                    e.Graphics.DrawRectangle(pen, 0, 0, statusBar.Width - 1, statusBar.Height - 1);
            };
            
            lblVersion = new Label 
            { 
                Text = AppVersion, 
                Dock = DockStyle.Right, 
                AutoSize = true,
                ForeColor = TextSecondaryColor,
                TextAlign = ContentAlignment.MiddleRight,
                Padding = ScalePadding(new Padding(6, 6, 6, 6))
            };
            
            lblStatus = new Label 
            { 
                Text = "✓ 就绪", 
                Dock = DockStyle.Left, 
                AutoSize = true,
                ForeColor = SuccessColor,
                TextAlign = ContentAlignment.MiddleLeft,
                Padding = ScalePadding(new Padding(6, 6, 6, 6))
            };
            
            statusBar.Controls.Add(lblVersion);
            statusBar.Controls.Add(lblStatus);
            return statusBar;
        }

        private Control CreateFitPanel()
        {
            var panel = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                ColumnCount = 2,
                RowCount = 1,
                BackColor = BgColor
            };
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 42));
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 58));
            
            // 左侧面板
            var leftCard = CreateCard("数据输入");
            var leftContent = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                RowCount = 5,
                ColumnCount = 2,
                Padding = ScalePadding(new Padding(4))
            };
            leftContent.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(28)));
            leftContent.RowStyles.Add(new RowStyle(SizeType.Percent, 55));
            leftContent.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(44)));
            leftContent.RowStyles.Add(new RowStyle(SizeType.Percent, 18));
            leftContent.RowStyles.Add(new RowStyle(SizeType.Percent, 27));
            leftContent.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
            leftContent.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
            
            leftContent.Controls.Add(CreateLabel("📥 ADC原始值", true), 0, 0);
            leftContent.Controls.Add(CreateLabel("📤 真实值", true), 1, 0);
            
            dgvAdc = CreateDataGridView("ADC值");
            leftContent.Controls.Add(dgvAdc, 0, 1);
            
            dgvReal = CreateDataGridView("真实值");
            leftContent.Controls.Add(dgvReal, 1, 1);
            
            // 控制面板
            var controlPanel = new FlowLayoutPanel 
            { 
                Dock = DockStyle.Fill, 
                FlowDirection = FlowDirection.LeftToRight,
                WrapContents = false,
                Padding = ScalePadding(new Padding(0, 4, 0, 4))
            };
            
            controlPanel.Controls.Add(CreateLabel("阶数:", false));
            nudDegree = new NumericUpDown 
            { 
                Value = 3, Minimum = 1, Maximum = 5, 
                Width = Scale(55), 
                Height = Scale(26),
                Font = this.Font
            };
            controlPanel.Controls.Add(nudDegree);
            controlPanel.Controls.Add(new Panel { Width = Scale(15), Height = 1 });
            
            btnFit = CreateButton("🔄 拟合", true);
            btnClear = CreateButton("🗑️ 清空", false);
            btnCopyFormula = CreateButton("📋 复制", false);
            controlPanel.Controls.Add(btnFit);
            controlPanel.Controls.Add(btnClear);
            controlPanel.Controls.Add(btnCopyFormula);
            
            leftContent.Controls.Add(controlPanel, 0, 2);
            leftContent.SetColumnSpan(controlPanel, 2);
            
            txtResult = CreateMultiLineTextBox("拟合结果显示在这里...", true);
            leftContent.Controls.Add(txtResult, 0, 3);
            leftContent.SetColumnSpan(txtResult, 2);
            
            leftContent.Controls.Add(CreateSingleCalcPanel(), 0, 4);
            leftContent.SetColumnSpan(leftContent.GetControlFromPosition(0, 4), 2);
            
            leftCard.Controls.Add(leftContent);
            panel.Controls.Add(leftCard, 0, 0);
            
            // 右侧面板
            var rightCard = CreateCard("验证结果");
            dgvVerify = CreateVerifyGridView();
            rightCard.Controls.Add(dgvVerify);
            panel.Controls.Add(rightCard, 1, 0);
            
            return panel;
        }

        private Panel CreateSingleCalcPanel()
        {
            var panel = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Color.FromArgb(250, 250, 250),
                Padding = ScalePadding(new Padding(6))
            };
            
            var layout = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                RowCount = 3,
                ColumnCount = 3
            };
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(24)));
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(32)));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 38));
            layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 38));
            layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 24));
            
            layout.Controls.Add(CreateLabel("单点验证 - ADC:", false), 0, 0);
            layout.Controls.Add(CreateLabel("真实值(可选):", false), 1, 0);
            
            txtSingleAdc = CreateSingleLineTextBox("输入ADC值");
            txtSingleAdc.Margin = new Padding(0, 0, Scale(3), 0);
            layout.Controls.Add(txtSingleAdc, 0, 1);
            
            txtSingleReal = CreateSingleLineTextBox("计算误差用");
            txtSingleReal.Margin = new Padding(0, 0, Scale(3), 0);
            layout.Controls.Add(txtSingleReal, 1, 1);
            
            btnCalcSingle = CreateButton("计算", true);
            btnCalcSingle.Dock = DockStyle.Fill;
            layout.Controls.Add(btnCalcSingle, 2, 1);
            
            txtSingleResult = CreateMultiLineTextBox("", true);
            layout.Controls.Add(txtSingleResult, 0, 2);
            layout.SetColumnSpan(txtSingleResult, 3);
            
            panel.Controls.Add(layout);
            return panel;
        }

        private Control CreateSerialPanel()
        {
            var panel = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                ColumnCount = 2,
                RowCount = 1,
                BackColor = BgColor
            };
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, Scale(320)));
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            
            // 左侧
            var leftCard = CreateCard("串口设置");
            var leftContent = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                RowCount = 7,
                ColumnCount = 1,
                Padding = ScalePadding(new Padding(4))
            };
            leftContent.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(60)));
            leftContent.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(60)));
            leftContent.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(48)));
            leftContent.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(24)));
            leftContent.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(60)));
            leftContent.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(120)));
            leftContent.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            
            // 端口选择
            var portPanel = CreateFormRow("串口端口:");
            cbPort = CreateComboBox();
            cbPort.Width = Scale(170);
            btnRefreshPort = CreateButton("🔄", false);
            btnRefreshPort.Width = Scale(36);
            btnRefreshPort.Height = Scale(26);
            btnRefreshPort.MinimumSize = new Size(Scale(36), Scale(26));
            var portFlow = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.LeftToRight };
            portFlow.Controls.Add(cbPort);
            portFlow.Controls.Add(btnRefreshPort);
            portPanel.Controls.Add(portFlow, 0, 1);
            leftContent.Controls.Add(portPanel, 0, 0);
            
            // 波特率
            var baudPanel = CreateFormRow("波特率:");
            cbBaudRate = CreateComboBox();
            cbBaudRate.Width = Scale(170);
            baudPanel.Controls.Add(cbBaudRate, 0, 1);
            leftContent.Controls.Add(baudPanel, 0, 1);
            
            // 连接按钮
            var connPanel = new FlowLayoutPanel 
            { 
                Dock = DockStyle.Fill, 
                FlowDirection = FlowDirection.LeftToRight,
                Padding = ScalePadding(new Padding(0, 6, 0, 0))
            };
            btnConnect = CreateButton("🔗 连接", true);
            btnConnect.Width = Scale(90);
            btnConnect.Height = Scale(32);
            
            lblConnStatus = new Label
            {
                Text = "● 未连接",
                ForeColor = ErrorColor,
                Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Bold, GraphicsUnit.Point),
                AutoSize = true,
                Padding = ScalePadding(new Padding(10, 8, 0, 0))
            };
            connPanel.Controls.Add(btnConnect);
            connPanel.Controls.Add(lblConnStatus);
            leftContent.Controls.Add(connPanel, 0, 2);
            
            // 分隔线
            var divider = new Panel
            {
                Dock = DockStyle.Fill,
                Height = 1,
                BackColor = BorderColor,
                Margin = ScalePadding(new Padding(0, 8, 0, 8))
            };
            leftContent.Controls.Add(divider, 0, 3);
            
            // 通道选择
            var chPanel = CreateFormRow("目标通道:");
            var chFlow = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.LeftToRight };
            cbChannel = CreateComboBox();
            cbChannel.Width = Scale(150);
            btnReadChannels = CreateButton("📖 回读", false);
            btnReadChannels.Width = Scale(70);
            chFlow.Controls.Add(cbChannel);
            chFlow.Controls.Add(btnReadChannels);
            chPanel.Controls.Add(chFlow, 0, 1);
            leftContent.Controls.Add(chPanel, 0, 4);
            
            // 公式输入
            var formulaPanel = CreateFormRow("多项式公式:");
            var formulaContainer = new Panel { Dock = DockStyle.Fill };
            
            btnUseCurrentFormula = CreateButton("📋 使用拟合结果", false);
            btnUseCurrentFormula.Dock = DockStyle.Top;
            btnUseCurrentFormula.Height = Scale(26);
            
            txtFormula = CreateMultiLineTextBox("格式: y = a*x^3 + b*x^2 + c*x + d", false);
            txtFormula.Dock = DockStyle.Fill;
            
            formulaContainer.Controls.Add(txtFormula);
            formulaContainer.Controls.Add(btnUseCurrentFormula);
            formulaPanel.Controls.Add(formulaContainer, 0, 1);
            formulaPanel.RowStyles[1] = new RowStyle(SizeType.Percent, 100);
            leftContent.Controls.Add(formulaPanel, 0, 5);
            
            // 操作按钮
            var opPanel = new FlowLayoutPanel 
            { 
                Dock = DockStyle.Fill, 
                FlowDirection = FlowDirection.TopDown,
                Padding = ScalePadding(new Padding(0, 10, 0, 0))
            };
            btnSendFormula = CreateButton("📤 发送到设备", true);
            btnSendFormula.Width = Scale(280);
            btnSendFormula.Height = Scale(38);
            btnSendFormula.Font = new Font("Microsoft YaHei UI", 10F, FontStyle.Bold, GraphicsUnit.Point);
            
            btnReadCoeffs = CreateButton("📥 读取设备系数", false);
            btnReadCoeffs.Width = Scale(280);
            btnReadCoeffs.Height = Scale(32);
            btnReadCoeffs.Margin = new Padding(0, Scale(6), 0, 0);
            
            opPanel.Controls.Add(btnSendFormula);
            opPanel.Controls.Add(btnReadCoeffs);
            leftContent.Controls.Add(opPanel, 0, 6);
            
            leftCard.Controls.Add(leftContent);
            panel.Controls.Add(leftCard, 0, 0);
            
            // 右侧日志
            var rightCard = CreateCard("通信日志");
            var logContainer = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                RowCount = 2,
                ColumnCount = 1
            };
            logContainer.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(32)));
            logContainer.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            
            btnClearLog = CreateButton("🗑️ 清空", false);
            btnClearLog.Dock = DockStyle.Left;
            btnClearLog.Width = Scale(80);
            logContainer.Controls.Add(btnClearLog, 0, 0);
            
            txtLog = new TextBox
            {
                Dock = DockStyle.Fill,
                Multiline = true,
                ReadOnly = true,
                ScrollBars = ScrollBars.Vertical,
                Font = new Font("Consolas", 9F, FontStyle.Regular, GraphicsUnit.Point),
                BackColor = Color.FromArgb(250, 250, 250),
                ForeColor = TextColor,
                BorderStyle = BorderStyle.FixedSingle
            };
            logContainer.Controls.Add(txtLog, 0, 1);
            
            rightCard.Controls.Add(logContainer);
            panel.Controls.Add(rightCard, 1, 0);
            
            return panel;
        }

        private Control CreateHelpPanel()
        {
            var card = CreateCard("使用说明");
            
            var helpText = new RichTextBox
            {
                Dock = DockStyle.Fill,
                ReadOnly = true,
                BorderStyle = BorderStyle.None,
                BackColor = CardBgColor,
                Font = new Font("Microsoft YaHei UI", 9.5F, FontStyle.Regular, GraphicsUnit.Point),
                Padding = ScalePadding(new Padding(12))
            };
            
            helpText.Text = $@"
  📊 多项式拟合功能
  ─────────────────────────────────────────
  1. 在左侧表格中输入ADC原始值和对应的真实值
  2. 设置多项式阶数 (建议3-4阶)
  3. 点击 [拟合] 生成校准公式
  4. 查看右侧验证结果，确认误差在可接受范围内
  5. 可使用单点计算功能验证任意ADC值


  📡 串口下载功能
  ─────────────────────────────────────────
  1. 选择正确的串口和波特率
  2. 点击 [连接] 建立通信
  3. 点击 [回读] 获取设备通道列表
  4. 选择目标通道
  5. 输入公式或点击 [使用拟合结果]
  6. 点击 [发送到设备] 完成下载


  📝 公式格式
  ─────────────────────────────────────────
  • y = 8.03e-9*x^3 + -3.58e-5*x^2 + 1.575*x + -18.58
  • 支持科学计数法


  ⚠️ 注意事项
  ─────────────────────────────────────────
  • 确保ADC值和真实值数量相同
  • 多项式阶数不宜过高，避免过拟合


  版本: {AppVersion}
";
            
            card.Controls.Add(helpText);
            return card;
        }
        
        #region UI辅助方法
        private Panel CreateCard(string title)
        {
            var card = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = CardBgColor,
                Padding = ScalePadding(new Padding(12, 36, 12, 12)),
                Margin = ScalePadding(new Padding(4))
            };
            
            card.Paint += (s, e) =>
            {
                var g = e.Graphics;
                g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
                
                using (var pen = new Pen(BorderColor))
                    g.DrawRectangle(pen, 0, 0, card.Width - 1, card.Height - 1);
                
                var headerHeight = Scale(32);
                using (var brush = new SolidBrush(Color.FromArgb(250, 250, 250)))
                    g.FillRectangle(brush, 1, 1, card.Width - 2, headerHeight);
                
                using (var pen = new Pen(BorderColor))
                    g.DrawLine(pen, 0, headerHeight + 1, card.Width, headerHeight + 1);
                
                using (var font = new Font("Microsoft YaHei UI", 9.5F, FontStyle.Bold, GraphicsUnit.Point))
                using (var brush = new SolidBrush(TextColor))
                    g.DrawString(title, font, brush, Scale(12), Scale(8));
            };
            
            return card;
        }
        
        private Label CreateLabel(string text, bool bold)
        {
            return new Label
            {
                Text = text,
                AutoSize = true,
                ForeColor = bold ? TextColor : TextSecondaryColor,
                Font = new Font("Microsoft YaHei UI", 9F, bold ? FontStyle.Bold : FontStyle.Regular, GraphicsUnit.Point),
                Padding = ScalePadding(new Padding(0, 2, 6, 2))
            };
        }
        
        private Button CreateButton(string text, bool primary)
        {
            var btn = new Button
            {
                Text = text,
                AutoSize = true,
                MinimumSize = ScaleSize(new Size(65, 28)),
                FlatStyle = FlatStyle.Flat,
                Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Regular, GraphicsUnit.Point),
                Cursor = Cursors.Hand,
                Padding = ScalePadding(new Padding(8, 2, 8, 2))
            };
            
            if (primary)
            {
                btn.BackColor = PrimaryColor;
                btn.ForeColor = Color.White;
                btn.FlatAppearance.BorderSize = 0;
            }
            else
            {
                btn.BackColor = Color.White;
                btn.ForeColor = TextColor;
                btn.FlatAppearance.BorderColor = BorderColor;
                btn.FlatAppearance.BorderSize = 1;
            }
            
            btn.MouseEnter += (s, e) => btn.BackColor = primary ? Color.FromArgb(64, 169, 255) : Color.FromArgb(245, 245, 245);
            btn.MouseLeave += (s, e) => btn.BackColor = primary ? PrimaryColor : Color.White;
            
            return btn;
        }
        
        private TextBox CreateSingleLineTextBox(string placeholder)
        {
            var txt = new TextBox
            {
                Dock = DockStyle.Fill,
                Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Regular, GraphicsUnit.Point),
                BackColor = Color.White,
                ForeColor = TextSecondaryColor,
                BorderStyle = BorderStyle.FixedSingle,
                Text = placeholder,
                Tag = placeholder
            };
            
            txt.GotFocus += (s, e) =>
            {
                if (txt.Text == (string)txt.Tag)
                {
                    txt.Text = "";
                    txt.ForeColor = TextColor;
                }
            };
            
            txt.LostFocus += (s, e) =>
            {
                if (string.IsNullOrWhiteSpace(txt.Text))
                {
                    txt.Text = (string)txt.Tag;
                    txt.ForeColor = TextSecondaryColor;
                }
            };
            
            return txt;
        }
        
        private TextBox CreateMultiLineTextBox(string placeholder, bool readOnly)
        {
            var txt = new TextBox
            {
                Dock = DockStyle.Fill,
                Multiline = true,
                ReadOnly = readOnly,
                ScrollBars = ScrollBars.Vertical,
                Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Regular, GraphicsUnit.Point),
                BackColor = readOnly ? Color.FromArgb(250, 250, 250) : Color.White,
                ForeColor = readOnly ? TextColor : TextSecondaryColor,
                BorderStyle = BorderStyle.FixedSingle,
                Text = placeholder,
                Tag = placeholder
            };
            
            if (!readOnly && !string.IsNullOrEmpty(placeholder))
            {
                txt.GotFocus += (s, e) =>
                {
                    if (txt.Text == (string)txt.Tag)
                    {
                        txt.Text = "";
                        txt.ForeColor = TextColor;
                    }
                };
                
                txt.LostFocus += (s, e) =>
                {
                    if (string.IsNullOrWhiteSpace(txt.Text))
                    {
                        txt.Text = (string)txt.Tag;
                        txt.ForeColor = TextSecondaryColor;
                    }
                };
            }
            
            return txt;
        }
        
        private ComboBox CreateComboBox()
        {
            return new ComboBox
            {
                DropDownStyle = ComboBoxStyle.DropDownList,
                Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Regular, GraphicsUnit.Point),
                FlatStyle = FlatStyle.Flat
            };
        }
        
        private DataGridView CreateDataGridView(string columnName)
        {
            var dgv = new DataGridView
            {
                Dock = DockStyle.Fill,
                AllowUserToAddRows = true,
                AllowUserToDeleteRows = true,
                AllowUserToResizeRows = false,
                ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.DisableResizing,
                ColumnHeadersHeight = Scale(28),
                RowHeadersWidth = Scale(40),
                RowTemplate = { Height = Scale(24) },
                BackgroundColor = Color.White,
                BorderStyle = BorderStyle.FixedSingle,
                GridColor = BorderColor,
                Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Regular, GraphicsUnit.Point),
                SelectionMode = DataGridViewSelectionMode.CellSelect,
                DefaultCellStyle = new DataGridViewCellStyle
                {
                    SelectionBackColor = Color.FromArgb(230, 247, 255),
                    SelectionForeColor = TextColor
                },
                ColumnHeadersDefaultCellStyle = new DataGridViewCellStyle
                {
                    BackColor = Color.FromArgb(250, 250, 250),
                    ForeColor = TextColor,
                    Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Bold, GraphicsUnit.Point),
                    Alignment = DataGridViewContentAlignment.MiddleCenter
                },
                EnableHeadersVisualStyles = false
            };
            
            dgv.Columns.Add("Value", columnName);
            dgv.Columns[0].Width = Scale(100);
            dgv.Columns[0].DefaultCellStyle.Alignment = DataGridViewContentAlignment.MiddleCenter;
            dgv.RowPostPaint += Dgv_RowPostPaint;
            
            return dgv;
        }
        
        private DataGridView CreateVerifyGridView()
        {
            var dgv = new DataGridView
            {
                Dock = DockStyle.Fill,
                AllowUserToAddRows = false,
                ReadOnly = true,
                AllowUserToResizeRows = false,
                ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.DisableResizing,
                ColumnHeadersHeight = Scale(32),
                RowHeadersVisible = false,
                RowTemplate = { Height = Scale(26) },
                AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
                BackgroundColor = Color.White,
                BorderStyle = BorderStyle.FixedSingle,
                GridColor = BorderColor,
                Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Regular, GraphicsUnit.Point),
                DefaultCellStyle = new DataGridViewCellStyle
                {
                    SelectionBackColor = Color.FromArgb(230, 247, 255),
                    SelectionForeColor = TextColor,
                    Alignment = DataGridViewContentAlignment.MiddleCenter
                },
                ColumnHeadersDefaultCellStyle = new DataGridViewCellStyle
                {
                    BackColor = Color.FromArgb(250, 250, 250),
                    ForeColor = TextColor,
                    Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Bold, GraphicsUnit.Point),
                    Alignment = DataGridViewContentAlignment.MiddleCenter
                },
                EnableHeadersVisualStyles = false
            };
            
            dgv.Columns.Add("ADC", "ADC值");
            dgv.Columns.Add("Predicted", "预测值");
            dgv.Columns.Add("Actual", "实际值");
            dgv.Columns.Add("Error", "误差(%)");
            
            return dgv;
        }
        
        private TableLayoutPanel CreateFormRow(string labelText)
        {
            var panel = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                RowCount = 2,
                ColumnCount = 1
            };
            panel.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(22)));
            panel.RowStyles.Add(new RowStyle(SizeType.Absolute, Scale(30)));
            panel.Controls.Add(CreateLabel(labelText, false), 0, 0);
            return panel;
        }
        #endregion
        
        #endregion

        #region 事件绑定
        private void SetupEvents()
        {
            btnFit.Click += BtnFit_Click;
            btnClear.Click += BtnClear_Click;
            btnCopyFormula.Click += BtnCopyFormula_Click;
            btnCalcSingle.Click += BtnCalcSingle_Click;
            btnRefreshPort.Click += (s, e) => RefreshPorts();
            btnConnect.Click += BtnConnect_Click;
            btnReadChannels.Click += BtnReadChannels_Click;
            btnSendFormula.Click += BtnSendFormula_Click;
            btnReadCoeffs.Click += BtnReadCoeffs_Click;
            btnUseCurrentFormula.Click += BtnUseCurrentFormula_Click;
            btnClearLog.Click += (s, e) => txtLog.Text = "";
        }

        private void SetupTimer()
        {
            _rxTimer = new System.Timers.Timer(RxTimeoutMs);
            _rxTimer.AutoReset = false;
            _rxTimer.Elapsed += RxTimer_Elapsed;
        }
        #endregion

        #region 多项式拟合
        private void BtnFit_Click(object sender, EventArgs e)
        {
            try
            {
                var adcValues = GetGridValues(dgvAdc);
                var realValues = GetGridValues(dgvReal);
                
                if (adcValues.Count == 0 || realValues.Count == 0)
                { ShowError("请输入ADC值和真实值"); return; }
                
                if (adcValues.Count != realValues.Count)
                { ShowError($"数据数量不匹配: ADC({adcValues.Count}) vs 真实值({realValues.Count})"); return; }
                
                int degree = (int)nudDegree.Value;
                if (degree >= adcValues.Count)
                { ShowError($"多项式阶数({degree})不能大于等于数据点数({adcValues.Count})"); return; }
                
                _coefficients = PolynomialFit(adcValues, realValues, degree);
                
                txtResult.Text = "y = " + FormatFormula(_coefficients);
                txtResult.ForeColor = TextColor;
                
                dgvVerify.Rows.Clear();
                double maxError = 0;
                for (int i = 0; i < adcValues.Count; i++)
                {
                    double predicted = EvaluatePolynomial(_coefficients, adcValues[i]);
                    double error = Math.Abs(predicted - realValues[i]) / Math.Abs(realValues[i]) * 100;
                    if (error > maxError) maxError = error;
                    
                    var rowIndex = dgvVerify.Rows.Add(
                        adcValues[i].ToString("F2"),
                        predicted.ToString("F6"),
                        realValues[i].ToString("F6"),
                        error.ToString("F4") + "%"
                    );
                    
                    if (error > 1)
                        dgvVerify.Rows[rowIndex].Cells[3].Style.ForeColor = ErrorColor;
                }
                
                ShowSuccess($"拟合完成! 最大误差: {maxError:F4}%");
                lblStatus.Text = $"✓ 拟合完成 | {degree}阶 | 最大误差: {maxError:F4}%";
                lblStatus.ForeColor = SuccessColor;
            }
            catch (Exception ex) { ShowError("拟合失败: " + ex.Message); }
        }

        private void BtnClear_Click(object sender, EventArgs e)
        {
            dgvAdc.Rows.Clear();
            dgvReal.Rows.Clear();
            dgvVerify.Rows.Clear();
            txtResult.Text = "";
            ResetTextBox(txtSingleAdc);
            ResetTextBox(txtSingleReal);
            txtSingleResult.Text = "";
            _coefficients = null;
            lblStatus.Text = "✓ 就绪";
            lblStatus.ForeColor = SuccessColor;
        }
        
        private void ResetTextBox(TextBox txt)
        {
            if (txt.Tag != null)
            {
                txt.Text = (string)txt.Tag;
                txt.ForeColor = TextSecondaryColor;
            }
        }

        private void BtnCopyFormula_Click(object sender, EventArgs e)
        {
            if (string.IsNullOrEmpty(txtResult.Text))
            { ShowWarning("没有可复制的公式"); return; }
            Clipboard.SetText(txtResult.Text);
            ShowSuccess("公式已复制到剪贴板");
        }

        private void BtnCalcSingle_Click(object sender, EventArgs e)
        {
            if (_coefficients == null || _coefficients.Length == 0)
            { ShowWarning("请先进行拟合计算"); return; }
            
            var adcText = txtSingleAdc.Text;
            if (adcText == (string)txtSingleAdc.Tag || !double.TryParse(adcText, out double adcValue))
            { ShowWarning("请输入有效的ADC值"); return; }
            
            double predicted = EvaluatePolynomial(_coefficients, adcValue);
            
            var realText = txtSingleReal.Text;
            if (realText != (string)txtSingleReal.Tag && double.TryParse(realText, out double realValue))
            {
                double error = Math.Abs(predicted - realValue) / Math.Abs(realValue) * 100;
                txtSingleResult.Text = $"ADC: {adcValue}\r\n预测值: {predicted:F6}\r\n实际值: {realValue}\r\n误差: {error:F4}%";
            }
            else
                txtSingleResult.Text = $"ADC: {adcValue}\r\n预测值: {predicted:F6}";
        }

        private double[] PolynomialFit(List<double> x, List<double> y, int degree)
        {
            int n = x.Count;
            var X = Matrix<double>.Build.Dense(n, degree + 1, (i, j) => Math.Pow(x[i], j));
            var Y = Vector<double>.Build.Dense(y.ToArray());
            return X.Svd(true).Solve(Y).ToArray();
        }

        private double EvaluatePolynomial(double[] coeffs, double x)
        {
            double y = coeffs[coeffs.Length - 1];
            for (int i = coeffs.Length - 2; i >= 0; i--)
                y = y * x + coeffs[i];
            return y;
        }

        private string FormatFormula(double[] coeffs)
        {
            var parts = new List<string>();
            for (int i = coeffs.Length - 1; i >= 0; i--)
            {
                string term = i == 0 ? coeffs[i].ToString("G10") : i == 1 ? $"{coeffs[i]:G10}*x" : $"{coeffs[i]:G10}*x^{i}";
                parts.Add(term);
            }
            return string.Join(" + ", parts);
        }

        private List<double> GetGridValues(DataGridView dgv)
        {
            var values = new List<double>();
            foreach (DataGridViewRow row in dgv.Rows)
                if (row.Cells[0].Value != null && double.TryParse(row.Cells[0].Value.ToString(), out double val))
                    values.Add(val);
            return values;
        }
        #endregion

        #region 串口通信
        private void RefreshPorts()
        {
            cbPort.Items.Clear();
            _portMap.Clear();
            
            try
            {
                var ports = SerialPort.GetPortNames();
                if (ports.Length == 0) { ShowWarning("未检测到串口"); return; }
                
                try
                {
                    using (var searcher = new ManagementObjectSearcher("SELECT Name FROM Win32_PnPEntity WHERE Name LIKE '%(COM%)'"))
                    {
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
                }
                catch { }
                
                foreach (var port in ports)
                    if (!_portMap.Values.Contains(port))
                    {
                        _portMap[port] = port;
                        cbPort.Items.Add(port);
                    }
                
                if (cbPort.Items.Count > 0) cbPort.SelectedIndex = 0;
            }
            catch (Exception ex) { ShowError("刷新端口失败: " + ex.Message); }
        }

        private void BtnConnect_Click(object sender, EventArgs e)
        {
            if (_isConnected) Disconnect();
            else Connect();
        }

        private void Connect()
        {
            try
            {
                if (cbPort.SelectedItem == null || !_portMap.TryGetValue(cbPort.SelectedItem.ToString(), out string portName))
                { ShowWarning("请选择有效的串口"); return; }
                
                _serial.PortName = portName;
                _serial.BaudRate = Convert.ToInt32(cbBaudRate.SelectedItem?.ToString() ?? "115200");
                _serial.DataBits = 8;
                _serial.Parity = Parity.None;
                _serial.StopBits = StopBits.One;
                _serial.ReadTimeout = 1000;
                _serial.WriteTimeout = 1000;
                
                _serial.Open();
                _currentPort = portName;
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
                if (_serial.IsOpen) _serial.Close();
                _isConnected = false;
                _currentPort = null;
                _channelMap.Clear();
                cbChannel.Items.Clear();
                UpdateConnectionUI(false);
                Log("已断开连接");
            }
            catch (Exception ex) { ShowError("断开失败: " + ex.Message); }
        }

        private void UpdateConnectionUI(bool connected)
        {
            _isConnected = connected;
            btnConnect.Text = connected ? "❌ 断开" : "🔗 连接";
            btnConnect.BackColor = connected ? ErrorColor : PrimaryColor;
            lblConnStatus.Text = connected ? "● 已连接" : "● 未连接";
            lblConnStatus.ForeColor = connected ? SuccessColor : ErrorColor;
            btnReadChannels.Enabled = connected;
            btnSendFormula.Enabled = connected;
            btnReadCoeffs.Enabled = connected;
        }

        private void BtnReadChannels_Click(object sender, EventArgs e)
        {
            if (!_isConnected) { ShowWarning("请先连接串口"); return; }
            try
            {
                _rxBuffer.Clear();
                byte[] cmd = new byte[] { 0xAA, 0x52, 0x42, 0x43, 0x61, 0x6C, 0x43, 0x68 };
                _serial.Write(cmd, 0, cmd.Length);
                Log("TX: 回读通道命令");
            }
            catch (Exception ex) { ShowError("发送失败: " + ex.Message); }
        }

        private void BtnSendFormula_Click(object sender, EventArgs e)
        {
            if (!_isConnected) { ShowWarning("请先连接串口"); return; }
            if (cbChannel.SelectedIndex < 0) { ShowWarning("请先选择通道"); return; }
            
            try
            {
                var formulaText = txtFormula.Text;
                if (formulaText == (string)txtFormula.Tag) { ShowWarning("请输入公式"); return; }
                
                var coeffs = ParseFormula(formulaText);
                if (coeffs == null || coeffs.Length == 0) { ShowWarning("公式解析失败"); return; }
                if (coeffs.Length > 6) { ShowWarning("最多支持6个系数"); return; }
                
                string chName = cbChannel.SelectedItem.ToString();
                if (!_channelMap.TryGetValue(chName, out string chIdStr) || !byte.TryParse(chIdStr, out byte chId))
                { ShowError("通道ID无效"); return; }
                
                byte[] frame = BuildWriteFrame(chId, coeffs);
                _serial.Write(frame, 0, frame.Length);
                Log($"TX: 发送CH{chId}系数 [{string.Join(", ", coeffs.Select(c => c.ToString("E4")))}]");
                ShowSuccess($"已发送到通道 {chName}");
            }
            catch (Exception ex) { ShowError("发送失败: " + ex.Message); }
        }

        private void BtnReadCoeffs_Click(object sender, EventArgs e)
        {
            if (!_isConnected) { ShowWarning("请先连接串口"); return; }
            if (cbChannel.SelectedIndex < 0) { ShowWarning("请先选择通道"); return; }
            ShowWarning("此功能需要设备端支持");
        }

        private void BtnUseCurrentFormula_Click(object sender, EventArgs e)
        {
            if (_coefficients == null || _coefficients.Length == 0) { ShowWarning("请先进行拟合计算"); return; }
            txtFormula.Text = "y = " + FormatFormula(_coefficients);
            txtFormula.ForeColor = TextColor;
            ShowSuccess("已填入拟合公式");
        }

        private byte[] BuildWriteFrame(byte chId, double[] coeffs)
        {
            int totalLen = 1 + 5 + 1 + 1 + coeffs.Length * 8 + 2;
            byte[] frame = new byte[totalLen];
            int idx = 0;
            
            frame[idx++] = FrameHeader;
            frame[idx++] = (byte)'C'; frame[idx++] = (byte)'a'; frame[idx++] = (byte)'l';
            frame[idx++] = (byte)'C'; frame[idx++] = (byte)'h';
            frame[idx++] = (byte)totalLen;
            frame[idx++] = chId;
            
            foreach (var c in coeffs)
            {
                Array.Copy(BitConverter.GetBytes(c), 0, frame, idx, 8);
                idx += 8;
            }
            
            ushort crc = CalcCrc16(frame, 1, totalLen - 3);
            frame[idx++] = (byte)(crc & 0xFF);
            frame[idx++] = (byte)((crc >> 8) & 0xFF);
            
            return frame;
        }

        private double[] ParseFormula(string formula)
        {
            if (string.IsNullOrWhiteSpace(formula)) return null;
            
            formula = Regex.Replace(formula, @"^\s*y\s*=\s*", "", RegexOptions.IgnoreCase);
            formula = formula.Replace(" ", "").Replace("-", "+-");
            
            var coeffDict = new Dictionary<int, double>();
            int maxPower = 0;
            
            foreach (var term in formula.Split(new[] { '+' }, StringSplitOptions.RemoveEmptyEntries))
            {
                if (string.IsNullOrEmpty(term)) continue;
                
                double coeff; int power = 0;
                
                if (term.Contains('x') || term.Contains('X'))
                {
                    var match = Regex.Match(term, @"^([+-]?\d*\.?\d*(?:[Ee][+-]?\d+)?)\*?[xX](?:\^(\d+))?");
                    if (match.Success)
                    {
                        string coeffStr = match.Groups[1].Value;
                        if (string.IsNullOrEmpty(coeffStr) || coeffStr == "+" || coeffStr == "-") coeffStr += "1";
                        coeff = double.Parse(coeffStr);
                        power = string.IsNullOrEmpty(match.Groups[2].Value) ? 1 : int.Parse(match.Groups[2].Value);
                    }
                    else continue;
                }
                else
                {
                    if (!double.TryParse(term, out coeff)) continue;
                }
                
                if (coeffDict.ContainsKey(power)) coeffDict[power] += coeff;
                else coeffDict[power] = coeff;
                if (power > maxPower) maxPower = power;
            }
            
            if (coeffDict.Count == 0) return null;
            
            var result = new double[maxPower + 1];
            for (int i = 0; i <= maxPower; i++)
                result[i] = coeffDict.ContainsKey(i) ? coeffDict[i] : 0;
            return result;
        }

        private void Serial_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                if (!_serial.IsOpen) return;
                int count = _serial.BytesToRead;
                byte[] buffer = new byte[count];
                _serial.Read(buffer, 0, count);
                lock (_lockObj) { _rxBuffer.Append(Encoding.UTF8.GetString(buffer)); }
                _rxTimer.Stop();
                _rxTimer.Start();
            }
            catch { }
        }

        private void Serial_ErrorReceived(object sender, SerialErrorReceivedEventArgs e)
        {
            this.Invoke(new Action(() => Log($"串口错误: {e.EventType}")));
        }

        private void RxTimer_Elapsed(object sender, System.Timers.ElapsedEventArgs e)
        {
            this.Invoke(new Action(() =>
            {
                try
                {
                    string data;
                    lock (_lockObj) { data = _rxBuffer.ToString().Trim(); _rxBuffer.Clear(); }
                    if (string.IsNullOrEmpty(data)) return;
                    
                    Log($"RX: {data}");
                    if (data.StartsWith("CH:")) ParseChannels(data);
                    else if (data.StartsWith("OK:")) ShowSuccess(data);
                }
                catch (Exception ex) { Log($"处理错误: {ex.Message}"); }
            }));
        }

        private void ParseChannels(string data)
        {
            _channelMap.Clear();
            cbChannel.Items.Clear();
            
            foreach (var item in data.Substring(3).Trim().Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries))
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

        private ushort CalcCrc16(byte[] data, int start, int len)
        {
            ushort crc = Crc16Init;
            for (int i = start; i < start + len; i++)
                crc = (ushort)((crc >> 8) ^ Crc16Table[(crc ^ data[i]) & 0xFF]);
            return crc;
        }
        #endregion

        #region 工具方法
        private void Log(string msg)
        {
            if (txtLog.InvokeRequired) { txtLog.Invoke(new Action(() => Log(msg))); return; }
            txtLog.AppendText($"[{DateTime.Now:HH:mm:ss}] {msg}\r\n");
        }

        private void ShowSuccess(string msg) => MessageBox.Show(msg, "成功", MessageBoxButtons.OK, MessageBoxIcon.Information);
        private void ShowWarning(string msg) => MessageBox.Show(msg, "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        private void ShowError(string msg) => MessageBox.Show(msg, "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);

        private void Dgv_RowPostPaint(object sender, DataGridViewRowPostPaintEventArgs e)
        {
            var dgv = sender as DataGridView;
            using (var brush = new SolidBrush(TextSecondaryColor))
            using (var font = new Font("Microsoft YaHei UI", 8F, FontStyle.Regular, GraphicsUnit.Point))
            {
                var num = (e.RowIndex + 1).ToString();
                var size = e.Graphics.MeasureString(num, font);
                e.Graphics.DrawString(num, font, brush,
                    e.RowBounds.Location.X + (dgv.RowHeadersWidth - size.Width) / 2,
                    e.RowBounds.Location.Y + (e.RowBounds.Height - size.Height) / 2);
            }
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            base.OnFormClosing(e);
            Disconnect();
            _rxTimer?.Stop();
            _rxTimer?.Dispose();
            _serial?.Dispose();
        }
        #endregion

        #region 控件声明
        private DataGridView dgvAdc, dgvReal, dgvVerify;
        private NumericUpDown nudDegree;
        private Button btnFit, btnClear, btnCopyFormula, btnCalcSingle;
        private Button btnConnect, btnRefreshPort, btnReadChannels;
        private Button btnSendFormula, btnReadCoeffs, btnUseCurrentFormula, btnClearLog;
        private TextBox txtResult, txtSingleAdc, txtSingleReal, txtSingleResult;
        private TextBox txtFormula, txtLog;
        private ComboBox cbPort, cbBaudRate, cbChannel;
        private Label lblConnStatus, lblStatus, lblVersion;
        #endregion
    }
}
