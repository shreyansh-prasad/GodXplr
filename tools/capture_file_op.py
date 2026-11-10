import os
import time
import subprocess
import ctypes
from ctypes import wintypes
from PIL import Image

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

PW_RENDERFULLCONTENT = 2
WM_COMMAND = 0x0111
kCmdNewFolder = 3001
WM_CLOSE = 0x0010

def capture_window(hwnd, output_path):
    rect = wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    width = rect.right - rect.left
    height = rect.bottom - rect.top

    if width <= 0 or height <= 0:
        return False

    hdc_screen = user32.GetDC(hwnd)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
    hbitmap = gdi32.CreateCompatibleBitmap(hdc_screen, width, height)
    h_old = gdi32.SelectObject(hdc_mem, hbitmap)

    user32.PrintWindow(hwnd, hdc_mem, PW_RENDERFULLCONTENT)

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
    bmi.biHeight = -height
    bmi.biPlanes = 1
    bmi.biBitCount = 32
    bmi.biCompression = 0

    buffer_size = width * height * 4
    buffer = ctypes.create_string_buffer(buffer_size)

    gdi32.GetDIBits(hdc_mem, hbitmap, 0, height, buffer, ctypes.byref(bmi), 0)

    image = Image.frombuffer('RGBA', (width, height), buffer, 'raw', 'BGRA', 0, 1)
    rgb_image = image.convert('RGB')
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    rgb_image.save(output_path, "PNG")

    gdi32.SelectObject(hdc_mem, h_old)
    gdi32.DeleteObject(hbitmap)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(hwnd, hdc_screen)
    return True

def capture_file_op_demo():
    exe = r"C:\os fileX\build\src\app\Release\godxplr.exe"
    demo_dir = r"C:\os fileX\IEEE_Demo"
    
    # Remove any existing "New folder" in IEEE_Demo if present
    nf = os.path.join(demo_dir, "New folder")
    if os.path.exists(nf):
        try: os.rmdir(nf)
        except: pass

    proc = subprocess.Popen([exe, demo_dir])
    time.sleep(1.5)

    hwnd = user32.FindWindowW(None, "GodXplr")
    if hwnd:
        user32.ShowWindow(hwnd, 5)
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.5)

        # Trigger kCmdNewFolder (3001)
        print("Sending WM_COMMAND kCmdNewFolder (3001)...")
        user32.SendMessageW(hwnd, WM_COMMAND, kCmdNewFolder, 0)
        time.sleep(0.8) # Wait for creation and refresh

        out_path = r"C:\os fileX\deliverables\IEEE_Paper\figures\screenshots\fig03_file_operations.png"
        capture_window(hwnd, out_path)
        print("Captured file operation screenshot to:", out_path)
        user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
    else:
        print("GodXplr window not found!")

    try: proc.wait(timeout=2)
    except: proc.kill()

if __name__ == "__main__":
    capture_file_op_demo()

