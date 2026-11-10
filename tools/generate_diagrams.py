import os
from PIL import Image, ImageDraw, ImageFont

def create_architecture_diagram(output_path):
    # Professional academic architecture diagram
    w, h = 1200, 700
    img = Image.new("RGB", (w, h), (255, 255, 255))
    draw = ImageDraw.Draw(img)

    # Colors
    c_border = (40, 40, 40)
    c_bg_ui = (235, 243, 250)
    c_bg_ops = (240, 248, 240)
    c_bg_core = (253, 245, 230)
    c_bg_os = (245, 245, 245)
    c_text = (20, 20, 20)
    c_accent = (0, 102, 204)

    # Outer border
    draw.rectangle([(20, 20), (w - 20, h - 20)], outline=c_border, width=2)
    draw.text((w // 2 - 180, 30), "GODXPLR MULTI-TIER SYSTEM ARCHITECTURE", fill=c_accent)

    # 1. UI Layer
    draw.rectangle([(50, 70), (w - 50, 190)], fill=c_bg_ui, outline=c_border, width=1)
    draw.text((65, 80), "1. Virtualized Sensory GUI Shell (Native Win32 GDI / Apple Design System)", fill=c_accent)
    boxes_ui = [
        ("MainWindow (Win32)", "Event Loop & HWND"),
        ("VirtualizedView", "O(1) Viewport Projection"),
        ("GdiRenderer", "Double-Buffered BitBlt"),
        ("TactileFeedback", "Sub-16ms Intent Loop"),
        ("PdfInspector", "Stream Dictionary Parser")
    ]
    bw = 200
    for i, (b_title, b_sub) in enumerate(boxes_ui):
        bx = 70 + i * 215
        draw.rectangle([(bx, 110), (bx + bw, 170)], fill=(255, 255, 255), outline=c_border, width=1)
        draw.text((bx + 15, 120), b_title, fill=c_text)
        draw.text((bx + 15, 142), b_sub, fill=(100, 100, 100))

    # Arrow 1
    draw.text((w // 2 - 120, 200), "v  Shared Memory IPC / Non-Blocking Queue  v", fill=(120, 120, 120))

    # 2. Resilient Operations & Provider SPI
    draw.rectangle([(50, 230), (w - 50, 350)], fill=c_bg_ops, outline=c_border, width=1)
    draw.text((65, 240), "2. Resilient File Operations & Virtual Storage Provider SPI", fill=(0, 128, 64))
    boxes_ops = [
        ("FileOpsEngine", "ACID Transactions"),
        ("WalJournal", "Append-Only Undo/Redo"),
        ("ZeroHydrationGate", "Reparse Point Filter"),
        ("CloudProviderSPI", "OneDrive / GDrive / MTP"),
        ("ArchiveVFS", "Zip / 7z Stream Provider")
    ]
    for i, (b_title, b_sub) in enumerate(boxes_ops):
        bx = 70 + i * 215
        draw.rectangle([(bx, 270), (bx + bw, 330)], fill=(255, 255, 255), outline=c_border, width=1)
        draw.text((bx + 15, 280), b_title, fill=c_text)
        draw.text((bx + 15, 302), b_sub, fill=(100, 100, 100))

    # Arrow 2
    draw.text((w // 2 - 130, 360), "v  Direct Kernel DeviceIOControl (FSCTL)  v", fill=(120, 120, 120))

    # 3. High-Performance Core Indexer
    draw.rectangle([(50, 390), (w - 50, 510)], fill=c_bg_core, outline=c_border, width=1)
    draw.text((65, 400), "3. High-Performance In-Memory Search Core", fill=(180, 90, 0))
    boxes_core = [
        ("MftRawEnumerator", "Cold FSCTL_ENUM_USN"),
        ("UsnJournalTailer", "Hot 50ms Real-Time Poll"),
        ("SimdSearch (AVX2)", "256-bit Vector Matching"),
        ("TrigramIndex", "Inverted Bit-Packed Lists"),
        ("PackedRecordStore", "132.5 B / Record Contig.")
    ]
    for i, (b_title, b_sub) in enumerate(boxes_core):
        bx = 70 + i * 215
        draw.rectangle([(bx, 430), (bx + bw, 490)], fill=(255, 255, 255), outline=c_border, width=1)
        draw.text((bx + 12, 440), b_title, fill=c_text)
        draw.text((bx + 12, 462), b_sub, fill=(100, 100, 100))

    # Arrow 3
    draw.text((w // 2 - 110, 520), "v  Windows Kernel Subsystems & CPU SIMD  v", fill=(120, 120, 120))

    # 4. OS & Hardware Abstraction
    draw.rectangle([(50, 550), (w - 50, 670)], fill=c_bg_os, outline=c_border, width=1)
    draw.text((65, 560), "4. Windows Kernel & Hardware Subsystems", fill=(80, 80, 80))
    boxes_os = [
        ("NTFS Filesystem", "Volume C: / MFT Table"),
        ("cldflt.sys Minifilter", "Cloud Filter API"),
        ("Win32 GDI / DWM", "Double-Buffered Compositor"),
        ("AVX2 Hardware", "Intel / AMD 256-bit SIMD"),
        ("NVMe Storage", "PCIe 4.0 Direct Block I/O")
    ]
    for i, (b_title, b_sub) in enumerate(boxes_os):
        bx = 70 + i * 215
        draw.rectangle([(bx, 590), (bx + bw, 650)], fill=(255, 255, 255), outline=c_border, width=1)
        draw.text((bx + 15, 600), b_title, fill=c_text)
        draw.text((bx + 15, 622), b_sub, fill=(100, 100, 100))

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, "PNG")
    print("Saved:", output_path)

def create_workflow_diagram(output_path):
    w, h = 1100, 480
    img = Image.new("RGB", (w, h), (255, 255, 255))
    draw = ImageDraw.Draw(img)

    c_border = (40, 40, 40)
    c_accent = (0, 102, 204)

    draw.rectangle([(20, 20), (w - 20, h - 20)], outline=c_border, width=2)
    draw.text((w // 2 - 160, 30), "GODXPLR EXECUTION & WORKFLOW LIFECYCLE", fill=c_accent)

    steps = [
        ("Phase 1: Bootstrap", "Open Raw Volume \\\\.\\C:\nIssue FSCTL_ENUM_USN_DATA\nIngest 100k+ MFT records in 54ms\nPopulate packed contiguous RAM"),
        ("Phase 2: USN Tailing", "Poll FSCTL_READ_USN_JOURNAL\n50ms asynchronous tick\nCapture creates, deletes, renames\nZero-drop real-time sync"),
        ("Phase 3: SIMD Query", "User keystroke event\nBroadcast needles to AVX2\n8-lane 256-bit parallel scan\np50 latency: 655 microseconds"),
        ("Phase 4: O(1) Render", "Project visible [istart, iend]\nZero UI widget explosion\nPaint 25-45 rows via GDI BitBlt\nSub-16ms tactile interaction")
    ]

    bx = 45
    bw = 230
    for i, (s_title, s_desc) in enumerate(steps):
        draw.rectangle([(bx, 90), (bx + bw, 390)], fill=(248, 250, 252), outline=c_border, width=1)
        draw.text((bx + 15, 110), s_title, fill=c_accent)
        draw.line([(bx + 15, 135), (bx + bw - 15, 135)], fill=(200, 200, 200), width=1)
        y = 150
        for line in s_desc.split('\n'):
            draw.text((bx + 15, y), line, fill=(50, 50, 50))
            y += 30
        if i < len(steps) - 1:
            draw.text((bx + bw + 6, 230), "===>", fill=(0, 102, 204))
        bx += 260

    draw.text((w // 2 - 250, 420), "Result: Sub-millisecond continuous indexing with 132.5 B/record memory density", fill=(100, 100, 100))
    img.save(output_path, "PNG")
    print("Saved:", output_path)

def create_module_interaction_diagram(output_path):
    w, h = 1000, 520
    img = Image.new("RGB", (w, h), (255, 255, 255))
    draw = ImageDraw.Draw(img)

    c_border = (40, 40, 40)
    c_accent = (0, 102, 204)

    draw.rectangle([(20, 20), (w - 20, h - 20)], outline=c_border, width=2)
    draw.text((w // 2 - 160, 30), "CORE COMPONENT INTERACTION & DATA FLOW", fill=c_accent)

    # Left: User Input / UI
    draw.rectangle([(60, 100), (320, 440)], fill=(240, 245, 255), outline=c_border, width=1)
    draw.text((80, 120), "USER INTERACTION (UI)", fill=c_accent)
    draw.text((80, 160), "1. Keyboard Search Query\n2. Viewport Scroll Offset\n3. Pointer Down / Up (isPressed)\n4. Context Menu / File Mutation\n5. Quick Look Document Request", fill=(40, 40, 40))

    # Center: Core Orchestration
    draw.rectangle([(370, 100), (630, 440)], fill=(255, 250, 240), outline=c_border, width=1)
    draw.text((390, 120), "CORE COORDINATION", fill=(180, 90, 0))
    draw.text((390, 160), "SearchEngine:\n  Dispatches needles to SIMD\n  Computes ranking score\n\nFileOpsEngine:\n  Creates transaction record\n  Appends to WAL Journal\n\nZeroHydrationGate:\n  Inspects reparse point attributes", fill=(40, 40, 40))

    # Right: Storage & OS
    draw.rectangle([(680, 100), (940, 440)], fill=(240, 255, 245), outline=c_border, width=1)
    draw.text((700, 120), "STORAGE & PLATFORM", fill=(0, 128, 64))
    draw.text((700, 160), "NTFS USN Change Journal:\n  Low-overhead event tailing\n\nWAL Log File on Disk:\n  Synchronous flush via write-thru\n\nCloud Minifilter Driver:\n  Guarded against hydration\n\nWin32 GDI Memory DC:\n  Double-buffered BitBlt repaint", fill=(40, 40, 40))

    # Connectors
    draw.text((328, 260), "===>", fill=(100, 100, 100))
    draw.text((638, 260), "===>", fill=(100, 100, 100))

    img.save(output_path, "PNG")
    print("Saved:", output_path)

if __name__ == "__main__":
    d_dir = r"C:\os fileX\deliverables\IEEE_Paper\figures\diagrams"
    create_architecture_diagram(os.path.join(d_dir, "diagram01_system_architecture.png"))
    create_workflow_diagram(os.path.join(d_dir, "diagram02_application_workflow.png"))
    create_module_interaction_diagram(os.path.join(d_dir, "diagram03_module_interaction.png"))
