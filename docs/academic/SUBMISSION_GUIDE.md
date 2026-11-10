# Google Classroom IEEE Assignment Submission Guide

This guide explains how to compile the IEEE academic paper and submit the final PDF to Google Classroom.

---

## Deliverables Generated in Repository

1. **LaTeX Source File:** [`docs/academic/godxplr_ieee_paper.tex`](file:///c:/os%20fileX/docs/academic/godxplr_ieee_paper.tex)
   - Fully compliant with official `IEEEtran` conference specifications.
   - Includes Title, Abstract, Index Terms, Sections I–VII, KaTeX/LaTeX equations, comparative benchmark tables, and IEEE references.
2. **Markdown Academic Paper:** [`docs/academic/GODXPLR_IEEE_PAPER.md`](file:///c:/os%20fileX/docs/academic/GODXPLR_IEEE_PAPER.md)
   - Publication-grade markdown view for immediate reading or copy-pasting.

---

## Option 1: 1-Minute PDF Generation via Overleaf (Recommended)

Overleaf has the official `IEEEtran` compiler and packages pre-installed in the cloud with zero setup:

1. Open your browser and go to [https://www.overleaf.com](https://www.overleaf.com).
2. Log in (or sign in with your student Google / institutional account).
3. Click **New Project** $\to$ **Blank Project** (name it `GodXplr_IEEE_Paper`).
4. In the left file tree, open `main.tex`.
5. Open [`docs/academic/godxplr_ieee_paper.tex`](file:///c:/os%20fileX/docs/academic/godxplr_ieee_paper.tex), copy all its contents (`Ctrl+A`, `Ctrl+C`), and paste them into Overleaf's editor replacing the default text.
6. Click the green **Recompile** button.
7. Click the **Download PDF** icon next to Recompile.
8. Rename the downloaded PDF to your university format (e.g., `2025UCS1708_Shreyansh_Prasad_GodXplr.pdf`).

---

## Option 2: Microsoft Word or Google Docs (Official IEEE Template)

If your professor prefers a `.docx` or you want to edit in Word / Google Docs:

1. Download the official IEEE 2-column conference Word template from IEEE:
   - [IEEE Conference Template (.DOCX)](https://www.ieee.org/conferences/publishing/templates.html)
2. Open the `.docx` template in Microsoft Word or upload to Google Docs.
3. Copy the sections directly from [`docs/academic/GODXPLR_IEEE_PAPER.md`](file:///c:/os%20fileX/docs/academic/GODXPLR_IEEE_PAPER.md):
   - **Title & Metadata:** Title, Shreyansh Prasad, NSUT, Roll No: 2025UCS1708.
   - **Abstract & Index Terms**
   - **Sections I to VII** with formatted subheadings.
   - **Tables I & II** (already formatted with clear columns).
   - **Equations:** Insert equation objects using the KaTeX formulas provided.
   - **References [1]–[10]**
4. In Word/Google Docs, click **File $\to$ Download $\to$ PDF Document (.pdf)**.

---

## Option 3: Local Command-Line Compilation (MiKTeX / TeX Live)

If you have a local LaTeX distribution installed on your Windows machine:

```powershell
cd "c:\os fileX\docs\academic"
pdflatex godxplr_ieee_paper.tex
bibtex godxplr_ieee_paper
pdflatex godxplr_ieee_paper.tex
pdflatex godxplr_ieee_paper.tex
```

This will produce `godxplr_ieee_paper.pdf` directly in the folder.

---

## Submitting in Google Classroom

1. Go to [Google Classroom](https://classroom.google.com).
2. Navigate to your course assignment created by your professor.
3. In the right-hand **Your work** panel, click **+ Add or create** $\to$ **File**.
4. Upload `2025UCS1708_Shreyansh_Prasad_GodXplr.pdf`.
5. *(Optional but recommended)* In the **Private comments** box, write:
   > "Respected Professor,  
   > Please find attached my project assignment report formatted according to IEEE standards:  
   > **Title:** GodXplr: A High-Performance, Zero-Hydration Native Filesystem Management and Content Retrieval Architecture  
   > **Student:** Shreyansh Prasad (Roll No: 2025UCS1708, CSE, NSUT)  
   > **GitHub Repository:** https://github.com/shreyansh-prasad/GodXplr.git"
6. Click **Turn In** / **Submit**.
