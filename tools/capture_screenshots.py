import os
import time
import subprocess
import ctypes
from ctypes import wintypes
from PIL import Image

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

# Win32 definitions
PW_RENDERFULLCONTENT = 2

def capture_window(hwnd, output_path):
    rect = wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    width = rect.right - rect.left
    height = rect.bottom - rect.top

    if width <= 0 or height <= 0:
        print(f"Invalid window dimensions: {width}x{height}")
        return False

    hdc_screen = user32.GetDC(hwnd)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
    hbitmap = gdi32.CreateCompatibleBitmap(hdc_screen, width, height)
    h_old = gdi32.SelectObject(hdc_mem, hbitmap)

    # PrintWindow
    success = user32.PrintWindow(hwnd, hdc_mem, PW_RENDERFULLCONTENT)
    if not success:
        # Fallback to standard PrintWindow
        user32.PrintWindow(hwnd, hdc_mem, 0)

    # Read bitmap bits into PIL Image
    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [
            ('biSize', wintypes.DWORD),
            ('biWidth', wintypes.LONG),
            ('biHeight', wintypes.LONG),
            ('biPlanes', wintypes.WORD),
            ('biBitCount', wintypes.WORD),
            ('biCompression', wintypes.DWORD),
            ('biSizeImage', wintypes.DWORD),
            ('biXPelsPerMeter', wintypes.LONG),
            ('biYPelsPerMeter', wintypes.LONG),
            ('biClrUsed', wintypes.DWORD),
            ('biClrImportant', wintypes.DWORD)
        ]

    bmi = BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.biWidth = width
    bmi.biHeight = -height  # top-down
    bmi.biPlanes = 1
    bmi.biBitCount = 32
    bmi.biCompression = 0  # BI_RGB

    buffer_size = width * height * 4
    buffer = ctypes.create_string_buffer(buffer_size)

    gdi32.GetDIBits(hdc_mem, hbitmap, 0, height, buffer, ctypes.byref(bmi), 0)

    image = Image.frombuffer('RGBA', (width, height), buffer, 'raw', 'BGRA', 0, 1)
    rgb_image = image.convert('RGB')
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    rgb_image.save(output_path, "PNG")
    print(f"Captured: {output_path} ({width}x{height})")

    # Cleanup
    gdi32.SelectObject(hdc_mem, h_old)
    gdi32.DeleteObject(hbitmap)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(hwnd, hdc_screen)
    return True

def run_and_capture(path_arg, output_file, wait_sec=1.5):
    exe = r"C:\os fileX\build\src\app\Release\godxplr.exe"
    args = [exe]
    if path_arg:
        args.append(path_arg)
    
    proc = subprocess.Popen(args)
    time.sleep(wait_sec)

    hwnd = user32.FindWindowW(None, "GodXplr")
    if not hwnd:
        print("Window 'GodXplr' not found, searching by EnumWindows...")
        found_hwnds = []
        def enum_cb(h, l):
            length = user32.GetWindowTextLengthW(h)
            if length > 0:
                buf = ctypes.create_unicode_buffer(length + 1)
                user32.GetWindowTextW(h, buf, length + 1)
                if "GodXplr" in buf.value:
                    found_hwnds.append(h)
            return True
        WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
        user32.EnumWindows(WNDENUMPROC(enum_cb), 0)
        if found_hwnds:
            hwnd = found_hwnds[0]

    if hwnd:
        user32.ShowWindow(hwnd, 5) # SW_SHOW
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.5)
        capture_window(hwnd, output_file)
        user32.PostMessageW(hwnd, 0x0010, 0, 0) # WM_CLOSE
    else:
        print("Could not find window handle for GodXplr!")

    try:
        proc.wait(timeout=2)
    except:
        proc.kill()

if __name__ == "__main__":
    out_dir = r"C:\os fileX\deliverables\IEEE_Paper\figures\screenshots"
    # 1. Main Demo directory
    run_and_capture(r"C:\os fileX\IEEE_Demo", os.path.join(out_dir, "fig01_main_interface.png"), 1.8)
    # 2. Documents subdirectory
    run_and_capture(r"C:\os fileX\IEEE_Demo\Documents", os.path.join(out_dir, "fig02_directory_navigation.png"), 1.8)
    # 3. Source_Code subdirectory
    run_and_capture(r"C:\os fileX\IEEE_Demo\Source_Code", os.path.join(out_dir, "fig03_file_operations.png"), 1.8)

