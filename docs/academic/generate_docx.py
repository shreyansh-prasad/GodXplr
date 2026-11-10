import os
# Generate DOCX via Word COM

# We can use python with powershell or comtypes or docx if needed,
# or PowerShell with Word COM:
ps_script = r'''
$word = New-Object -ComObject Word.Application
$word.Visible = $false
$htmlPath = "C:\os fileX\docs\academic\ieee_paper.html"
$docxPath = "C:\os fileX\2025UCS1708_Shreyansh_Prasad_GodXplr_IEEE.docx"
$doc = $word.Documents.Open($htmlPath)
# wdFormatXMLDocument = 16
$doc.SaveAs2($docxPath, 16)
$doc.Close()
$word.Quit()
Write-Output "DOCX generated successfully: $docxPath"
'''

import subprocess
res = subprocess.run(["powershell", "-NoProfile", "-Command", ps_script], capture_output=True, text=True)
print("Return code:", res.returncode)
print("Stdout:", res.stdout)
print("Stderr:", res.stderr)
