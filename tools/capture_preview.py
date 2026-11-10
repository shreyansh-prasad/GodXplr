import subprocess
import os

chrome = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
html_path = os.path.abspath(r"C:\os fileX\deliverables\IEEE_Paper\source\IEEE_File_Explorer.html")
out_img = os.path.abspath(r"C:\os fileX\deliverables\IEEE_Paper\figures\pdf_rendered_preview.png")

cmd = [
    chrome,
    "--headless=new",
    "--disable-gpu",
    "--window-size=1200,3200",
    f"--screenshot={out_img}",
    html_path,
]

subprocess.run(cmd, check=True)
print("Screenshot generated:", os.path.exists(out_img), "size:", os.path.getsize(out_img) if os.path.exists(out_img) else 0)

