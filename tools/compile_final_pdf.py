import os
import subprocess

html_path = os.path.abspath(r"C:\os fileX\deliverables\IEEE_Paper\source\IEEE_File_Explorer.html")
pdf_path = os.path.abspath(r"C:\os fileX\deliverables\IEEE_Paper\IEEE_File_Explorer_Final.pdf")
docx_path = os.path.abspath(r"C:\os fileX\deliverables\IEEE_Paper\IEEE_File_Explorer_Final.docx")

chrome = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
if not os.path.exists(chrome):
    chrome = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"

print("Compiling PDF via browser:", chrome)
cmd = [
    chrome,
    "--headless=new",
    "--disable-gpu",
    "--no-pdf-header-footer",
    f"--print-to-pdf={pdf_path}",
    html_path,
]

res = subprocess.run(cmd, capture_output=True, text=True)
print("PDF Return code:", res.returncode)
if os.path.exists(pdf_path):
    print("Generated PDF size:", os.path.getsize(pdf_path), "bytes")

# Also generate DOCX via Word COM
ps_script = f'''
$word = New-Object -ComObject Word.Application
$word.Visible = $false
$doc = $word.Documents.Open("{html_path}")
$doc.SaveAs2("{docx_path}", 16)
$doc.Close()
$word.Quit()
Write-Output "DOCX generated successfully"
'''

res_word = subprocess.run(["powershell", "-NoProfile", "-Command", ps_script], capture_output=True, text=True)
print("DOCX output:", res_word.stdout)
if os.path.exists(docx_path):
    print("Generated DOCX size:", os.path.getsize(docx_path), "bytes")

