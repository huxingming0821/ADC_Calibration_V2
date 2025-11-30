using System;
using System.Windows.Forms;

namespace CalibrationTool
{
    internal static class Program
    {
        [STAThread]
        static void Main()
        {
            // 启用高DPI支持
            Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
            
            // 启用视觉样式
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            
            // 运行主窗体
            Application.Run(new MainForm());
        }
    }
}
