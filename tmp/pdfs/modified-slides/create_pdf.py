from pathlib import Path

from reportlab.pdfgen import canvas


ROOT = Path(r"F:\workspace\AIRE\AI_RE\PresentationWeb")
IMAGE_DIRECTORY = ROOT / "output" / "images"
OUTPUT_PDF = ROOT / "output" / "pdf" / "AI_RE_modified_slides_03_10_11.pdf"
PAGE_SIZE = (1280, 720)
IMAGE_NAMES = [
    "slide-03-fusion-team.png",
    "slide-10-ai-design.png",
    "slide-11-demo-next-step.png",
]


pdf = canvas.Canvas(str(OUTPUT_PDF), pagesize=PAGE_SIZE)

for image_name in IMAGE_NAMES:
    pdf.drawImage(
        str(IMAGE_DIRECTORY / image_name),
        0,
        0,
        width=PAGE_SIZE[0],
        height=PAGE_SIZE[1],
    )
    pdf.showPage()

pdf.save()
