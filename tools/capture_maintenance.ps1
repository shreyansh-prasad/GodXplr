Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies "System.Drawing" @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public class WinDWMMaintenanceCap {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowW(string lpClassName, string lpWindowName);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint nFlags);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern IntPtr GetWindowDC(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr hwnd, IntPtr hdc);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
    [DllImport("gdi32.dll")] public static extern bool BitBlt(IntPtr hdcDest, int xDest, int yDest, int w, int h, IntPtr hdcSrc, int xSrc, int ySrc, uint rop);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    public static void Capture(IntPtr hwnd, string path) {
        SetProcessDPIAware();
        RECT rc;
        GetWindowRect(hwnd, out rc);
        int w = rc.Right - rc.Left;
        int h = rc.Bottom - rc.Top;
        if (w <= 0) w = 1420;
        if (h <= 0) h = 880;
        using (Bitmap bmp = new Bitmap(w, h)) {
            using (Graphics g = Graphics.FromImage(bmp)) {
                IntPtr hdc = g.GetHdc();
                bool pw = PrintWindow(hwnd, hdc, 2);
                if (!pw) {
                    IntPtr wdc = GetWindowDC(hwnd);
                    BitBlt(hdc, 0, 0, w, h, wdc, 0, 0, 0x00CC0020);
                    ReleaseDC(hwnd, wdc);
                }
                g.ReleaseHdc(hdc);
            }
            bmp.Save(path, ImageFormat.Png);
        }
    }
}
'@

Get-Process godxplr -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400

# Launch godxplr
$p = Start-Process -FilePath 'C:\os fileX\build\src\app\Release\godxplr.exe' -PassThru
Start-Sleep -Milliseconds 1500

$hwnd = [WinDWMMaintenanceCap]::FindWindowW("GodXplrMainWindowClass", "GodXplr")
Write-Host "Found HWND: $hwnd"

[WinDWMMaintenanceCap]::MoveWindow($hwnd, 30, 30, 1420, 880, $true)
Start-Sleep -Milliseconds 600

$outDir = "C:\Users\Lenovo\.gemini\antigravity\brain\89dea918-a93e-47a5-98f1-f7242de86735"

# 1. Navigate to This PC (WM_USER + 112 = 0x0470)
[WinDWMMaintenanceCap]::SendMessage($hwnd, 0x0470, [IntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Milliseconds 700

$capThisPc = Join-Path $outDir "godxplr_thispc_storage_overview.png"
[WinDWMMaintenanceCap]::Capture($hwnd, $capThisPc)
Write-Host "Captured 1: This PC Storage Overview -> $capThisPc"

# 2. Navigate to C:\os fileX\test_gallery to show Maintenance Filter Pills & Status Bar Storage Lens
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public class WinDWMNav {
    [StructLayout(LayoutKind.Sequential)]
    public struct COPYDATASTRUCT {
        public IntPtr dwData;
        public int cbData;
        public IntPtr lpData;
    }
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, ref COPYDATASTRUCT lParam);

    public static void Navigate(IntPtr hwnd, string path) {
        IntPtr pStr = Marshal.StringToHGlobalUni(path);
        COPYDATASTRUCT cds = new COPYDATASTRUCT();
        cds.dwData = (IntPtr)2;
        cds.cbData = (path.Length + 1) * 2;
        cds.lpData = pStr;
        SendMessage(hwnd, 0x004A, IntPtr.Zero, ref cds);
        Marshal.FreeHGlobal(pStr);
    }
}
'@

[WinDWMNav]::Navigate($hwnd, "C:\os fileX\test_gallery")
Start-Sleep -Milliseconds 600

$capDirMaint = Join-Path $outDir "godxplr_directory_maintenance_pills.png"
[WinDWMMaintenanceCap]::Capture($hwnd, $capDirMaint)
Write-Host "Captured 2: Directory Maintenance Filter Pills -> $capDirMaint"

# 3. Select file and toggle Preview Pane
# WM_USER + 110 = Select Row (1 = main.cpp)
[WinDWMMaintenanceCap]::SendMessage($hwnd, 0x046E, [IntPtr]1, [IntPtr]::Zero)
Start-Sleep -Milliseconds 300
# WM_USER + 105 = Toggle Preview Pane
[WinDWMMaintenanceCap]::SendMessage($hwnd, 0x0469, [IntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Milliseconds 700

$capDirPreview = Join-Path $outDir "godxplr_directory_preview_maintenance.png"
[WinDWMMaintenanceCap]::Capture($hwnd, $capDirPreview)
Write-Host "Captured 3: Directory Preview with Maintenance Status Bar -> $capDirPreview"

Get-Process godxplr -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Host "Capture completed successfully!"
