Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies "System.Drawing" @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public class WinDWMViewsClean {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint nFlags);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr hwnd, IntPtr hdc);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool UpdateWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool InvalidateRect(IntPtr hWnd, IntPtr lpRect, bool bErase);
    [DllImport("gdi32.dll")] public static extern bool BitBlt(IntPtr hdcDest, int xDest, int yDest, int w, int h, IntPtr hdcSrc, int xSrc, int ySrc, uint rop);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    public static void Capture(IntPtr hwnd, string path) {
        SetProcessDPIAware();
        ShowWindow(hwnd, 5); // SW_SHOW
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
                if (!pw) {
                    pw = PrintWindow(hwnd, hdc, 0);
                }
                if (!pw) {
                    IntPtr wdc = GetDC(hwnd);
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

$outDir = "C:\Users\Lenovo\.gemini\antigravity\brain\89dea918-a93e-47a5-98f1-f7242de86735"

function CaptureOneView($arg, $outputFilename) {
    Get-Process godxplr -ErrorAction SilentlyContinue | Stop-Process -Force
    Remove-Item 'C:\os fileX\build\godxplr_hwnd.txt' -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 400

    $p = Start-Process -FilePath 'C:\os fileX\build\src\app\Release\godxplr.exe' -ArgumentList $arg -PassThru
    $hwndVal = 0
    for ($i = 0; $i -lt 40; $i++) {
        if (Test-Path 'C:\os fileX\build\godxplr_hwnd.txt') {
            $hwndVal = [int64](Get-Content 'C:\os fileX\build\godxplr_hwnd.txt')
            if ($hwndVal -gt 0) { break }
        }
        Start-Sleep -Milliseconds 100
    }
    Write-Host "Launched with $arg, HWND: $hwndVal"
    $hwnd = [IntPtr]$hwndVal

    [WinDWMViewsClean]::MoveWindow($hwnd, 30, 30, 1420, 880, $true)
    Start-Sleep -Milliseconds 1200

    $targetPath = Join-Path $outDir $outputFilename
    [WinDWMViewsClean]::Capture($hwnd, $targetPath)
    $len = (Get-Item $targetPath).Length
    Write-Host "Saved $outputFilename ($len bytes)"

    $p | Stop-Process -Force
    Start-Sleep -Milliseconds 300
}

# 1. Capture This PC
CaptureOneView '"godxplr://thispc"' "godxplr_apple_thispc.png"

# 2. Capture Home View
CaptureOneView '"godxplr://home"' "godxplr_apple_home.png"

Write-Host "All individual views captured cleanly!"
