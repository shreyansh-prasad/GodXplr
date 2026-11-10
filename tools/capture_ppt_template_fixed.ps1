Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies "System.Drawing" @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public class WinDWMTemplateCap {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint nFlags);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool UpdateWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool InvalidateRect(IntPtr hWnd, IntPtr lpRect, bool bErase);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    public static void Capture(IntPtr hwnd, string path) {
        SetProcessDPIAware();
        ShowWindow(hwnd, 5);
        SetForegroundWindow(hwnd);
        InvalidateRect(hwnd, IntPtr.Zero, false);
        UpdateWindow(hwnd);

        RECT rc;
        GetClientRect(hwnd, out rc);
        int w = rc.Right - rc.Left;
        int h = rc.Bottom - rc.Top;
        if (w <= 0) w = 1420;
        if (h <= 0) h = 880;

        using (Bitmap bmp = new Bitmap(w, h)) {
            using (Graphics g = Graphics.FromImage(bmp)) {
                IntPtr hdc = g.GetHdc();
                bool pw = PrintWindow(hwnd, hdc, 2);
                if (!pw) pw = PrintWindow(hwnd, hdc, 0);
                g.ReleaseHdc(hdc);
            }
            bmp.Save(path, ImageFormat.Png);
        }
    }
}
'@

$outDir = 'C:\Users\Lenovo\.gemini\antigravity\brain\89dea918-a93e-47a5-98f1-f7242de86735'

Get-Process godxplr -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item 'C:\os fileX\build\godxplr_hwnd.txt' -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 400

# 1. Launch in NSUT directory to test ppt tempelate.pdf
$p = Start-Process -FilePath 'C:\os fileX\build\src\app\Release\godxplr.exe' -ArgumentList '"C:\Users\Lenovo\OneDrive\NSUT"' -PassThru
$hwndVal = 0
for ($i = 0; $i -lt 40; $i++) {
    if (Test-Path 'C:\os fileX\build\godxplr_hwnd.txt') {
        $hwndVal = [int64](Get-Content 'C:\os fileX\build\godxplr_hwnd.txt')
        if ($hwndVal -gt 0) { break }
    }
    Start-Sleep -Milliseconds 100
}
Write-Host "Launched HWND: $hwndVal"
$hwnd = [IntPtr]$hwndVal

[WinDWMTemplateCap]::MoveWindow($hwnd, 30, 30, 1420, 880, $true)
Start-Sleep -Milliseconds 1200

# Toggle preview pane ON (WM_USER + 105 = 0x0469)
[WinDWMTemplateCap]::SendMessage($hwnd, 0x0469, [IntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Milliseconds 400

# Select row 10: ppt tempelate.pdf (WM_USER + 110 = 0x046E, wParam = 10)
[WinDWMTemplateCap]::SendMessage($hwnd, 0x046E, [IntPtr]10, [IntPtr]::Zero)
Start-Sleep -Milliseconds 800

$capPath = Join-Path $outDir 'godxplr_ppt_template_fixed.png'
[WinDWMTemplateCap]::Capture($hwnd, $capPath)
Write-Host "Saved godxplr_ppt_template_fixed.png ($( (Get-Item $capPath).Length ) bytes)"

# 2. Test Responsive Resizing: Compact Screen (1024x720)
[WinDWMTemplateCap]::MoveWindow($hwnd, 30, 30, 1024, 720, $true)
Start-Sleep -Milliseconds 800
$capCompact = Join-Path $outDir 'godxplr_responsive_compact.png'
[WinDWMTemplateCap]::Capture($hwnd, $capCompact)
Write-Host "Saved godxplr_responsive_compact.png ($( (Get-Item $capCompact).Length ) bytes)"

# 3. Test Responsive Resizing: Widescreen (1580x920)
[WinDWMTemplateCap]::MoveWindow($hwnd, 30, 30, 1580, 920, $true)
Start-Sleep -Milliseconds 800
$capWide = Join-Path $outDir 'godxplr_responsive_wide.png'
[WinDWMTemplateCap]::Capture($hwnd, $capWide)
Write-Host "Saved godxplr_responsive_wide.png ($( (Get-Item $capWide).Length ) bytes)"

$p | Stop-Process -Force
Start-Sleep -Milliseconds 400

# 4. Test Home View Recent Items (Clean columns, no overlapping Path tooltip)
$p2 = Start-Process -FilePath 'C:\os fileX\build\src\app\Release\godxplr.exe' -ArgumentList '"godxplr://home"' -PassThru
$hwndVal2 = 0
for ($i = 0; $i -lt 40; $i++) {
    if (Test-Path 'C:\os fileX\build\godxplr_hwnd.txt') {
        $hwndVal2 = [int64](Get-Content 'C:\os fileX\build\godxplr_hwnd.txt')
        if ($hwndVal2 -gt 0) { break }
    }
    Start-Sleep -Milliseconds 100
}
$hwnd2 = [IntPtr]$hwndVal2
[WinDWMTemplateCap]::MoveWindow($hwnd2, 30, 30, 1420, 880, $true)
Start-Sleep -Milliseconds 1200
$capHomeFixed = Join-Path $outDir 'godxplr_home_clean_columns.png'
[WinDWMTemplateCap]::Capture($hwnd2, $capHomeFixed)
Write-Host "Saved godxplr_home_clean_columns.png ($( (Get-Item $capHomeFixed).Length ) bytes)"

$p2 | Stop-Process -Force
Write-Host "All verification captures completed successfully!"
