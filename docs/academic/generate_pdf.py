import subprocess
import os

chrome_paths = [
    r"C:\Program Files\Google\Chrome\Application\chrome.exe",
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
]

chrome = None
for p in chrome_paths:
    if os.path.exists(p):
        chrome = p
        break

html_path = os.path.abspath(r"C:\os fileX\docs\academic\ieee_paper.html")
pdf_path = os.path.abspath(r"C:\os fileX\2025UCS1708_Shreyansh_Prasad_GodXplr_IEEE.pdf")

print("Using browser:", chrome)
print("Input HTML:", html_path)
print("Output PDF:", pdf_path)

cmd = [
    chrome,
    "--headless=new",
    "--disable-gpu",
    "--no-pdf-header-footer",
    f"--print-to-pdf={pdf_path}",
    html_path,
]

res = subprocess.run(cmd, capture_output=True, text=True)
print("Return code:", res.returncode)
print("Stdout:", res.stdout)
print("Stderr:", res.stderr)
print("PDF exists:", os.path.exists(pdf_path))
if os.path.exists(pdf_path):
    print("PDF size:", os.path.getsize(pdf_path), "bytes")

