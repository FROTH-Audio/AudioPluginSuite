#!/usr/bin/env python3
"""Convert each plugin's Markdown manual and local screenshots into a PDF."""

import argparse
from html.parser import HTMLParser
from pathlib import Path
from xml.sax.saxutils import escape

import markdown
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.platypus import (
    HRFlowable,
    Image,
    Paragraph,
    Preformatted,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[2]
VOID_TAGS = {
    "area", "base", "br", "col", "embed", "hr", "img", "input", "link",
    "meta", "param", "source", "track", "wbr",
}


class Element:
    def __init__(self, tag, attrs=None):
        self.tag = tag
        self.attrs = dict(attrs or ())
        self.children = []


class HtmlTreeParser(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.root = Element("root")
        self.stack = [self.root]

    def handle_starttag(self, tag, attrs):
        element = Element(tag, attrs)
        self.stack[-1].children.append(element)
        if tag not in VOID_TAGS:
            self.stack.append(element)

    def handle_startendtag(self, tag, attrs):
        self.stack[-1].children.append(Element(tag, attrs))

    def handle_endtag(self, tag):
        for index in range(len(self.stack) - 1, 0, -1):
            if self.stack[index].tag == tag:
                del self.stack[index:]
                return

    def handle_data(self, data):
        self.stack[-1].children.append(data)


def text_content(element):
    return "".join(child if isinstance(child, str) else text_content(child)
                   for child in element.children)


def inline_markup(element):
    parts = []
    for child in element.children:
        if isinstance(child, str):
            parts.append(escape(child).replace("\n", " "))
            continue

        content = inline_markup(child)
        if child.tag in {"b", "strong"}:
            parts.append(f"<b>{content}</b>")
        elif child.tag in {"i", "em"}:
            parts.append(f"<i>{content}</i>")
        elif child.tag == "code":
            parts.append(f'<font name="Courier">{content}</font>')
        elif child.tag == "a":
            href = escape(child.attrs.get("href", ""), {'"': "&quot;"})
            parts.append(f'<link href="{href}" color="#2D8C9E">{content}</link>')
        elif child.tag == "br":
            parts.append("<br/>")
        elif child.tag != "img":
            parts.append(content)
    return "".join(parts)


def find_elements(element, tag):
    for child in element.children:
        if isinstance(child, str):
            continue
        if child.tag == tag:
            yield child
        yield from find_elements(child, tag)


def make_styles():
    base = getSampleStyleSheet()
    return {
        "body": ParagraphStyle(
            "ManualBody", parent=base["BodyText"], fontName="Helvetica",
            fontSize=9.5, leading=13, spaceAfter=6,
        ),
        "h1": ParagraphStyle(
            "ManualTitle", parent=base["Title"], fontName="Helvetica-Bold",
            fontSize=23, leading=28, textColor=colors.HexColor("#18333D"),
            alignment=TA_CENTER, spaceAfter=14,
        ),
        "h2": ParagraphStyle(
            "ManualH2", parent=base["Heading1"], fontName="Helvetica-Bold",
            fontSize=16, leading=20, textColor=colors.HexColor("#18333D"),
            spaceBefore=12, spaceAfter=7,
        ),
        "h3": ParagraphStyle(
            "ManualH3", parent=base["Heading2"], fontName="Helvetica-Bold",
            fontSize=12, leading=15, textColor=colors.HexColor("#2D6873"),
            spaceBefore=9, spaceAfter=5,
        ),
        "h4": ParagraphStyle(
            "ManualH4", parent=base["Heading3"], fontName="Helvetica-Bold",
            fontSize=10.5, leading=13, spaceBefore=7, spaceAfter=4,
        ),
        "quote": ParagraphStyle(
            "ManualQuote", parent=base["BodyText"], leftIndent=12,
            textColor=colors.HexColor("#44535A"), spaceAfter=6,
        ),
        "code": ParagraphStyle(
            "ManualCode", fontName="Courier", fontSize=7.5, leading=10,
            leftIndent=8, rightIndent=8, backColor=colors.HexColor("#F2F4F5"),
            borderColor=colors.HexColor("#D7DEE1"), borderWidth=0.5,
            borderPadding=6, spaceBefore=4, spaceAfter=8,
        ),
        "cell": ParagraphStyle(
            "ManualTableCell", parent=base["BodyText"], fontSize=8, leading=10,
            spaceAfter=0,
        ),
        "header_cell": ParagraphStyle(
            "ManualTableHeader", parent=base["BodyText"], fontName="Helvetica-Bold",
            fontSize=8, leading=10, spaceAfter=0,
        ),
    }


class ManualRenderer:
    def __init__(self, manual_path, page_width):
        self.manual_path = manual_path
        self.page_width = page_width
        self.styles = make_styles()
        self.story = []

    def image(self, element):
        src = element.attrs.get("src")
        if not src:
            raise ValueError(f"{self.manual_path}: image has no src")
        path = (self.manual_path.parent / src).resolve()
        if not path.is_file():
            raise FileNotFoundError(f"{self.manual_path}: missing image {path}")

        image = Image(str(path), hAlign="CENTER")
        scale = min(self.page_width / image.imageWidth, 540 / image.imageHeight, 1.0)
        image.drawWidth = image.imageWidth * scale
        image.drawHeight = image.imageHeight * scale
        image.spaceBefore = 4
        image.spaceAfter = 9
        return image

    def paragraph(self, element, style):
        images = [child for child in element.children
                  if isinstance(child, Element) and child.tag == "img"]
        if not images:
            content = inline_markup(element).strip()
            if content:
                self.story.append(Paragraph(content, style))
            return

        text_parts = []
        for child in element.children:
            if isinstance(child, Element) and child.tag == "img":
                content = "".join(text_parts).strip()
                if content:
                    self.story.append(Paragraph(content, style))
                text_parts = []
                self.story.append(self.image(child))
            else:
                wrapper = Element("span")
                wrapper.children = [child]
                text_parts.append(inline_markup(wrapper))
        content = "".join(text_parts).strip()
        if content:
            self.story.append(Paragraph(content, style))

    def table(self, element):
        rows = []
        for row in find_elements(element, "tr"):
            cells = []
            for cell in row.children:
                if not isinstance(cell, Element) or cell.tag not in {"th", "td"}:
                    continue
                style = self.styles["header_cell"] if cell.tag == "th" else self.styles["cell"]
                cells.append(Paragraph(inline_markup(cell).strip(), style))
            if cells:
                rows.append(cells)
        if not rows:
            return

        column_count = max(map(len, rows))
        for row in rows:
            row.extend([""] * (column_count - len(row)))
        table = Table(rows, colWidths=[self.page_width / column_count] * column_count,
                      repeatRows=1, hAlign="LEFT")
        table.setStyle(TableStyle([
            ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#E8EEF0")),
            ("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#B7C3C7")),
            ("VALIGN", (0, 0), (-1, -1), "TOP"),
            ("LEFTPADDING", (0, 0), (-1, -1), 5),
            ("RIGHTPADDING", (0, 0), (-1, -1), 5),
            ("TOPPADDING", (0, 0), (-1, -1), 4),
            ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
        ]))
        self.story.extend([table, Spacer(1, 8)])

    def blocks(self, element):
        for child in element.children:
            if isinstance(child, str):
                continue
            tag = child.tag
            if tag in {"h1", "h2", "h3", "h4"}:
                self.paragraph(child, self.styles[tag])
            elif tag == "p":
                self.paragraph(child, self.styles["body"])
            elif tag == "img":
                self.story.append(self.image(child))
            elif tag == "table":
                self.table(child)
            elif tag == "pre":
                self.story.append(Preformatted(text_content(child), self.styles["code"]))
            elif tag in {"ul", "ol"}:
                item_number = 0
                for item in child.children:
                    if isinstance(item, Element) and item.tag == "li":
                        wrapper = Element("span")
                        nested_lists = []
                        for item_child in item.children:
                            if isinstance(item_child, Element) and item_child.tag in {"ul", "ol"}:
                                nested_lists.append(item_child)
                            else:
                                wrapper.children.append(item_child)
                        text = inline_markup(wrapper).strip()
                        if text:
                            item_number += 1
                            bullet = f"{item_number}." if tag == "ol" else "-"
                            self.story.append(Paragraph(text, self.styles["body"], bulletText=bullet))
                        for nested in nested_lists:
                            self.blocks(nested)
            elif tag == "blockquote":
                for item in child.children:
                    if isinstance(item, Element) and item.tag == "p":
                        self.paragraph(item, self.styles["quote"])
            elif tag == "hr":
                self.story.extend([Spacer(1, 4), HRFlowable(width="100%", thickness=0.6,
                                                             color=colors.HexColor("#B7C3C7")),
                                   Spacer(1, 8)])
            elif tag == "br":
                self.story.append(Spacer(1, 4))
            elif tag == "div":
                self.blocks(child)
            elif tag == "section":
                self.blocks(child)


def build_pdf(manual_path, output_path):
    page_width, _ = A4
    left = right = 18 * mm
    top = 17 * mm
    bottom = 17 * mm
    doc = SimpleDocTemplate(
        str(output_path), pagesize=A4,
        leftMargin=left, rightMargin=right, topMargin=top, bottomMargin=bottom,
        title=manual_path.parent.name + " User Manual",
        author="FROTH Audio",
    )
    renderer = ManualRenderer(manual_path, page_width - left - right)
    html = markdown.markdown(
        manual_path.read_text(encoding="utf-8"),
        extensions=["tables", "fenced_code", "sane_lists"],
    )
    parser = HtmlTreeParser()
    parser.feed(html)
    parser.close()
    renderer.blocks(parser.root)
    if not renderer.story:
        raise ValueError(f"{manual_path}: no content to render")

    plugin_name = manual_path.parent.name

    def draw_footer(canvas, document):
        canvas.saveState()
        canvas.setFont("Helvetica", 8)
        canvas.setFillColor(colors.HexColor("#66767C"))
        canvas.drawString(left, 9 * mm, plugin_name + " User Manual")
        canvas.drawRightString(A4[0] - right, 9 * mm, f"Page {document.page}")
        canvas.restoreState()

    doc.build(renderer.story, onFirstPage=draw_footer, onLaterPages=draw_footer)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build" / "manual-pdfs")
    args = parser.parse_args()
    manuals = sorted((ROOT / "plugins").glob("*/MANUAL.md"))
    if not manuals:
        raise SystemExit("No plugins/*/MANUAL.md files found")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    for manual in manuals:
        output = args.output_dir / f"{manual.parent.name}-Manual.pdf"
        build_pdf(manual, output)
        print(f"Wrote {output}")


if __name__ == "__main__":
    main()
