#!/usr/bin/env python3
"""Convert the DataReel HTML documentation to the modern page style.

Usage:  python3 docs/bin/modernize_docs.py [--force] [files...]

With no files every .htm/.html page under docs/ is converted. Pages that
were already converted (they carry <meta name="generator"
content="datareel-docs-modern">) are skipped, so new pages written in the
old inc2help layout can be added at any time and converted by running
this script again. After adding a link to a p_lchild.htm nav page, run
with --force to rebuild the Library, Examples and Utilities browser pages
from it.

Each page keeps all of its text. The script:
  - adds the shared stylesheet (css/datareel.css) and script
    (js/datareel.js), a site header and a footer
  - turns section headings into <h2>, the Topics list into a contents box,
    function entries into cards and function lists into a grid
  - turns source listings into dark code blocks with highlighted comments
  - replaces the frameset pages (classes.htm, examples.htm, utils.htm)
    with a sidebar and a content frame
  - removes the old logo and menu tables, <P><HR></P> separators and
    "End Of Document" boxes

Written 10/14/2026.
"""
import html
import os
import re
import sys

DOCS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
MARK = '<meta name="generator" content="datareel-docs-modern">'
I = re.IGNORECASE
S = re.DOTALL


def read_version():
    try:
        with open(os.path.join(DOCS, "..", "version.txt")) as f:
            return f.read().strip()
    except OSError:
        return ""


VERSION = read_version()

SECTIONS = [  # (label, path from docs/, key)
    ("Readme", "index.html", "readme"),
    ("Library", "classes/classes.htm", "classes"),
    ("Examples", "examples/examples.htm", "examples"),
    ("Utilities", "utils/utils.htm", "utils"),
    ("Cluster Manager", "drcm.html", "drcm"),
    ("Code Review", "code_review_fixes.html", "review"),
]

BROWSERS = {  # frameset page -> (nav page, default content page, title, lead)
    "classes/classes.htm": ("classes/p_lchild.htm", "p_rchild.htm", "Class Library",
                            "Reference for every DataReel class and function."),
    "examples/examples.htm": ("examples/p_lchild.htm", "p_rchild.htm", "Examples",
                              "Example programs that show how each part of the library is used."),
    "utils/utils.htm": ("utils/p_lchild.htm", "p_rchild.htm", "Utilities",
                        "Utility programs included with DataReel."),
}


# ---------------------------------------------------------------- helpers
def rel_prefix(path):
    depth = path.count("/")
    return "../" * depth


def section_key(path):
    if path.startswith("classes/"):
        return "classes"
    if path.startswith("examples/"):
        return "examples"
    if path.startswith("utils/"):
        return "utils"
    if path.startswith("code_review"):
        return "review"
    if path.startswith("drcm"):
        return "drcm"
    return "readme"


def strip_tags(s):
    return re.sub(r"<[^>]*>", "", s)


def text_of(s):
    t = strip_tags(s).replace("&nbsp;", " ")
    return re.sub(r"\s+", " ", t).strip()


def header_html(path):
    rel = rel_prefix(path)
    key = section_key(path)
    links = []
    for label, target, k in SECTIONS:
        cur = ' aria-current="page"' if k == key else ""
        links.append(f'<a href="{rel}{target}"{cur}>{label}</a>')
    return (f'<header class="dr-header">\n'
            f'  <a class="dr-brand" href="{rel}index.html">'
            f'<img class="dr-logo-light" src="{rel}images/datareel_logo.svg" alt="DataReel" width="173" height="48">'
            f'<img class="dr-logo-dark" src="{rel}images/datareel_logo_dark.svg" alt="DataReel" width="173" height="48">'
            f'<span>C++ Toolkit Documentation</span></a>\n'
            f'  <nav class="dr-nav" aria-label="Documentation sections">{"".join(links)}</nav>\n'
            f'</header>')


def footer_html():
    ver = f"DataReel {VERSION} &middot; " if VERSION else ""
    return ('<footer class="dr-footer">' + ver +
            'Copyright (c) 2001-2026 DataReel Software Development &middot; '
            'Documentation style updated 10/14/2026</footer>')


def page(path, title, body_class, content, lead_comments="", tail_comments="", main=True):
    rel = rel_prefix(path)
    body = f'<main class="dr-main">\n{content}\n</main>\n{footer_html()}' if main else content
    return (f"<!doctype html>\n{lead_comments}"
            f'<html lang="en">\n<head>\n<meta charset="utf-8">\n'
            f'<meta name="viewport" content="width=device-width, initial-scale=1">\n'
            f"{MARK}\n<title>{title}</title>\n"
            f'<link rel="stylesheet" href="{rel}css/datareel.css">\n'
            f'<link rel="icon" href="{rel}images/favicon.ico" sizes="any">\n'
            f'<link rel="icon" type="image/png" sizes="192x192" href="{rel}images/datareel_icon_192.png">\n'
            f'<link rel="apple-touch-icon" href="{rel}images/datareel_icon_apple_touch.png">\n'
            f'<script src="{rel}js/datareel.js"></script>\n'
            f"</head>\n<body class=\"{body_class}\">\n{header_html(path)}\n{body}\n</body>\n</html>\n{tail_comments}")


def split_document(src):
    """Return (leading comments, title, body inner html, trailing comments)."""
    m = re.search(r"<html\b", src, I)
    lead = src[:m.start()] if m else ""
    lead = re.sub(r"<!doctype[^>]*>\s*", "", lead, flags=I)
    lead = lead.strip()
    lead = (lead + "\n") if lead else ""
    m2 = re.search(r"</html\s*>", src, I)
    tail = src[m2.end():].strip() if m2 else ""
    tail = (tail + "\n") if tail else ""
    t = re.search(r"<title>(.*?)</title>", src, I | S)
    title = re.sub(r"\s+", " ", t.group(1)).strip() if t else ""
    b = re.search(r"<body[^>]*>(.*?)(?:</body\s*>|$)", src, I | S)
    body = b.group(1) if b else src
    return lead, title, body, tail


def balance_fix(chunk, tags=("b", "u", "font", "i")):
    """Tags left open by chunk, as a string of start tags to re-open."""
    out = ""
    for t in tags:
        opens = len(re.findall(rf"<{t}\b[^>]*>", chunk, I))
        closes = len(re.findall(rf"</{t}\s*>", chunk, I))
        if opens > closes:
            out += f"<{t.upper()}>" * (opens - closes)
    return out


def drop_unmatched_closers(chunk, tags=("b", "u", "font", "i", "a")):
    for t in tags:
        opens = len(re.findall(rf"<{t}\b[^>]*>", chunk, I))
        closes = len(re.findall(rf"</{t}\s*>", chunk, I))
        extra = closes - opens
        while extra > 0:
            # remove the first unmatched closing tag
            depth = 0
            for m in re.finditer(rf"(<{t}\b[^>]*>)|(</{t}\s*>)", chunk, I):
                if m.group(1):
                    depth += 1
                elif depth == 0:
                    chunk = chunk[:m.start()] + chunk[m.end():]
                    break
                else:
                    depth -= 1
            extra -= 1
        if opens > closes:
            chunk += f"</{t.upper()}>" * (opens - closes)
    return chunk


KNOWN_TAGS = r"(?:a|b|i|u|p|br|font|tt|em|strong|code|span|sub|sup)"


def escape_unknown_tags(s):
    """Escape things like <gxsHTMLTagInfo> that were meant as text."""
    return re.sub(r"<(?!/?" + KNOWN_TAGS + r"\b)(?!!--)([A-Za-z_][^<>]*)>", r"&lt;\1&gt;", s, flags=I)


# ---------------------------------------------------------- transforms
def convert_listing_blocks(body):
    """<FONT FACE="Courier New"><PRE> listings -> <pre class="dr-code">."""
    def fix(m):
        inner = m.group(1)
        inner = re.sub(r'<FONT COLOR="#ff0000">', '<span class="cm">', inner, flags=I)
        inner = re.sub(r"<FONT[^>]*>", "<span>", inner, flags=I)
        inner = re.sub(r"</FONT\s*>", "</span>", inner, flags=I)
        inner = escape_unknown_tags(inner)
        return f'<pre class="dr-code">{inner}</pre>'
    body = re.sub(r'<FONT FACE="Courier New"[^>]*>\s*<PRE>(.*?)</PRE>\s*</FONT>', fix, body, flags=I | S)
    return body


def convert_headings(body):
    out = []
    pending_toc = False
    for line in body.split("\n"):
        if re.search(r"FONT SIZE=4", line, I) and re.search(r"</P>\s*$", line, I):
            names = re.findall(r'NAME="([^"]+)"', line, I)
            title = text_of(line).rstrip(":").strip()
            reopen = balance_fix(line)
            if title.lower() in ("topics", "contents"):
                out.append('<div class="dr-toc-title">Contents</div>' + reopen)
                pending_toc = True
                continue
            idattr = f' id="{names[0]}"' if names else ""
            extra = "".join(f'<a name="{n}"></a>' for n in names[1:])
            out.append(f"<h2{idattr}>{extra}{title}</h2>{reopen}")
            continue
        if pending_toc and re.match(r"\s*<P>", line, I):
            line = re.sub(r"<P>", '<P class="dr-toc">', line, count=1, flags=I)
            pending_toc = False
        out.append(line)
    return "\n".join(out)


FN_RE = re.compile(r'^(\s*(?:</?(?:FONT|U|B)[^>]*>|<A NAME="[^"]*">)*)(<FONT COLOR="#000080">)(.*?)(</FONT>)(.*)$', I)


def convert_functions(body):
    out = []
    for line in body.split("\n"):
        if 'COLOR="#000080"' not in line.upper().replace("'", '"'):
            out.append(line)
            continue
        m = FN_RE.match(line)
        if not m:
            out.append(line)
            continue
        prefix, _, sig, _, rest = m.groups()
        # prefix: keep closing tags, move anchors into the signature
        prefix_close = "".join(re.findall(r"</(?:FONT|U|B)\s*>", prefix, I))
        names = re.findall(r'NAME="([^"]+)"', prefix + sig, I)
        sig_text = escape_unknown_tags(sig)
        sig_text = re.sub(r"<(?!/?(?:a|i|u|tt|sub|sup)\b)[^>]*>", "", sig_text, flags=I)  # drop b/p/font/br
        sig_text = re.sub(r"<A NAME=\"[^\"]*\">", "", sig_text, flags=I)
        sig_text = re.sub(r"</A\s*>", "", sig_text, flags=I).strip()
        desc = rest
        desc = re.sub(r"^\s*(?:&nbsp;)?\s*-\s*(?:&nbsp;)?\s*", "", desc)
        desc = re.sub(r"</P>\s*$", "", desc, flags=I).strip()
        desc = drop_unmatched_closers(desc)
        idattr = f' id="{names[0]}"' if names else ""
        extra = "".join(f'<a name="{n}"></a>' for n in names[1:])
        desc_html = f'<p class="dr-desc">{desc}</p>' if desc else ""
        out.append(f'{prefix_close}<div class="dr-fn"{idattr}>{extra}<code class="dr-sig">{sig_text}</code>{desc_html}</div>')
    return "\n".join(out)


def convert_fn_lists(body):
    def fix(m):
        links = re.findall(r"<A HREF=\"#[^\"]+\">(.*?)</A>", m.group(2), I | S)
        if len(links) >= 3 and sum(1 for l in links if "(" in l or "::" in l) >= len(links) * 0.6:
            return f'<P class="dr-fnlist">{m.group(2)}</P>'
        return m.group(0)
    return re.sub(r'<P( class="[^"]*")?>(\s*(?:<A HREF="#[^"]+">[^<]*</A>(?:<BR>)?\s*)+)</P>', fix, body, flags=I)


def remove_chrome(body):
    # inc2help comment paragraph, separators, end-of-document box
    body = re.sub(r"<P>\s*(<!-- HTML file generated by:[^>]*-->)\s*</P>", r"\1", body, flags=I)
    body = re.sub(r"<P>\s*<HR>\s*</P>", "", body, flags=I)
    body = re.sub(r'<P ALIGN="CENTER">\s*<CENTER>\s*<TABLE[^>]*>\s*<TR>\s*<TD[^>]*>\s*<P[^>]*>\s*<B>End Of Document</B>\s*</TD>\s*</TR>\s*</TABLE>\s*</CENTER>\s*</P>', "", body, flags=I | S)
    body = re.sub(r"<P>\s*&nbsp;\s*</P>", "", body, flags=I)
    body = re.sub(r'<H1 ALIGN="CENTER">', "<H1>", body, flags=I)
    # old logo and menu (readme style pages)
    body = re.sub(r"<!-- Start company logo -->.*?<!-- End Menu -->", "", body, flags=I | S)
    body = re.sub(r'<center>\s*<img src="[^"]*drlogo1\.jpg"[^>]*>\s*</center>', "", body, flags=I)
    body = re.sub(r'<table bgcolor="(?:black|#?0{6})"[^>]*>\s*<tbody>\s*<tr>\s*<td>\s*<table.*?</table>\s*</td>\s*</tr>\s*</tbody>\s*</table>', "", body, flags=I | S)
    body = re.sub(r'<hr color="#000000"[^>]*>', "", body, flags=I)
    body = re.sub(r"<!--\s*(?:Begin|End) Title Bar\s*-->", "", body, flags=I)
    return body


def convert_title_bars(body):
    def fix(m):
        inner = m.group(1)
        names = re.findall(r'name="([^"]+)"', inner, I)
        title = text_of(inner)
        idattr = f' id="{names[0]}"' if names else ""
        if title.lower() == "contents":
            return '<div class="dr-toc-title">Contents</div><!--dr-toc-next-->'
        return f"<h2{idattr}>{title}</h2>"
    body = re.sub(r'<table[^>]*>\s*<tbody>\s*<tr>\s*<td bgcolor="#?A0CCFF"[^>]*>(.*?)</td>\s*</tr>\s*</tbody>\s*</table>', fix, body, flags=I | S)
    return re.sub(r"<!--dr-toc-next-->\s*<p>", '<p class="dr-toc">', body, flags=I)


def convert_features(body):
    m = re.search(r'(<h2 id="Features">Features</h2>)\s*(<table border="1".*?</table>)', body, I | S)
    if not m:
        return body
    rows = re.findall(r"<tr>(.*?)</tr>", m.group(2), I | S)
    if len(rows) < 2:
        return body
    heads = [text_of(c) for c in re.findall(r"<td[^>]*>(.*?)</td>", rows[0], I | S)]
    cells = re.findall(r"<td[^>]*>(.*?)</td>", rows[1], I | S)
    secs = []
    for h, c in zip(heads, cells):
        items = [text_of(x) for x in re.split(r"<br\s*/?>", c, flags=I)]
        items = [x for x in items if x]
        lis = "".join(f"<li>{x}</li>" for x in items)
        secs.append(f"<section><h3>{h}</h3><ul>{lis}</ul></section>")
    grid = '<div class="dr-features">' + "".join(secs) + "</div>"
    return body[:m.start()] + m.group(1) + "\n" + grid + body[m.end():]


def convert_div_commands(body):
    """Readme build instructions written as <div>&gt; cmd</div> lines."""
    lines = body.split("\n")
    out, buf = [], []

    def flush():
        if buf:
            out.append('<pre class="dr-code">' + "\n".join(buf) + "</pre>")
            buf.clear()
    for line in lines:
        m = re.match(r"\s*<div>(&gt;.*?)</div>\s*$", line, I)
        if m:
            buf.append(m.group(1).replace("&nbsp;", " "))
            continue
        if buf and not line.strip():
            continue
        flush()
        m = re.match(r"\s*<div>\s*&nbsp;\s*</div>\s*$", line, I)
        if m:
            continue
        line = re.sub(r"^(\s*)<div>(.*)</div>\s*$", r"\1<p>\2</p>", line, flags=I)
        out.append(line)
    flush()
    return "\n".join(out)


def wrap_tables(body):
    return re.sub(r"(<table\b.*?</table>)", r'<div class="dr-tablewrap">\1</div>', body, flags=I | S)


def tidy(body):
    # Only inside tags: text such as link="..." in code samples is kept
    body = re.sub(r"<[A-Za-z][^<>]*>",
                  lambda m: re.sub(r'\s(?:bgcolor|link|vlink|alink)="[^"]*"', "", m.group(0), flags=I),
                  body)
    body = re.sub(r"\n{3,}", "\n\n", body)
    return body.strip()


# ---------------------------------------------------------- page kinds
def nav_groups(nav_path):
    with open(os.path.join(DOCS, nav_path)) as f:
        src = f.read()
    body = split_document(src)[2]
    parts = re.split(r"<u>\s*<b>(.*?)</b>\s*</u>", body, flags=I | S)
    if len(parts) < 3:  # page already converted: groups are <summary> blocks
        parts = re.split(r"<summary>(.*?)</summary>", body, flags=I | S)
    groups = []
    for i in range(1, len(parts), 2):
        title = text_of(parts[i]).rstrip(":").strip()
        links = re.findall(r'<a href="([^"]+)"[^>]*>(.*?)</a>', parts[i + 1], I | S)
        links = [(h, text_of(t)) for h, t in links if text_of(t)]
        if links:
            groups.append((title, links))
    return groups


def nav_label_map():
    labels = {}
    for nav in ("examples/p_lchild.htm", "classes/p_lchild.htm", "utils/p_lchild.htm"):
        base = os.path.dirname(nav)
        try:
            for _, links in nav_groups(nav):
                for h, t in links:
                    p = os.path.normpath(os.path.join(base, h.split("#")[0])).replace(os.sep, "/")
                    labels.setdefault(p, t)
        except OSError:
            pass
    return labels


def groups_html(groups, target=None):
    tgt = f' target="{target}"' if target else ""
    out = []
    for title, links in groups:
        items = "\n".join(f'    <a href="{h}"{tgt}>{html.escape(t)}</a>' for h, t in links)
        out.append(f'  <details class="dr-group" open>\n    <summary>{html.escape(title)}</summary>\n{items}\n  </details>')
    return "\n".join(out)


def build_browser(path, src):
    nav, default, title, lead = BROWSERS[path]
    groups = nav_groups(nav)
    side = (f'<aside class="dr-sidebar" aria-label="{title} pages">\n'
            f'  <input class="dr-search" type="search" placeholder="Filter {title.lower()}..." aria-label="Filter {title.lower()}">\n'
            f"{groups_html(groups, 'PRIGHTCHILD')}\n</aside>")
    content = (f'<div class="dr-browser">\n{side}\n'
               f'<iframe class="dr-frame" name="PRIGHTCHILD" src="{default}" title="{title} content"></iframe>\n</div>')
    lead_c = split_document(src)[0]
    return page(path, f"DataReel {title}", "dr-browser-page", content, lead_c, main=False)


def build_navpage(path, src):
    lead, title, body, tail = split_document(src)
    frame_page = {v[0]: k for k, v in BROWSERS.items()}.get(path)
    name = BROWSERS[frame_page][2] if frame_page else "Index"
    groups = nav_groups(path)
    content = (f"<h1>{name} Index</h1>\n"
               f'<p class="dr-lead">All pages in this section. The <a href="{os.path.basename(frame_page) if frame_page else "#"}">{name.lower()} browser</a> shows the same list next to the page you are reading.</p>\n'
               f'<div class="dr-sidebar" style="border:0;padding:0;background:none">\n{groups_html(groups)}\n</div>')
    return page(path, title or f"DataReel {name} Index", "dr-nav-page", content, lead, tail)


def build_listing(path, src, labels):
    lead, title, body, tail = split_document(src)
    body = convert_listing_blocks(body)
    label = labels.get(path)
    fname = re.search(r"(?:Source|Include) Code File Name:\s*([\w.\-]+)", src, I)
    h1 = label or (fname.group(1) if fname else title) or os.path.basename(path)
    meta = f'<p class="dr-listing-meta">Source file: <code>{fname.group(1)}</code></p>\n' if fname else ""
    body = remove_chrome(body)
    content = f"<h1>{html.escape(h1)}</h1>\n{meta}{tidy(body)}"
    return page(path, title or h1, "dr-listing", content, lead, tail)


def build_diagram(path, src):
    lead, title, body, tail = split_document(src)
    m = re.search(r'<img\s+src="([^"]+)"', body, I)
    img = m.group(1) if m else ""
    h1 = title or "Diagram"
    content = f'<h1>{html.escape(h1)}</h1>\n<div class="dr-diagram"><img src="{img}" alt="{html.escape(h1)}"></div>'
    return page(path, h1, "dr-diagram-page", content, lead, tail)


def build_setup(path, src):
    lead, title, body, tail = split_document(src)
    body = re.sub(r'\s(?:style|lang|dir)="[^"]*"', "", body, flags=I)
    body = re.sub(r"<p>\s*<br\s*/?>\s*</p>", "", body, flags=I)
    paras = re.findall(r"<p>(.*?)</p>", body, I | S)
    out, buf = [], []

    def flush():
        if buf:
            out.append('<pre class="dr-code">' + "\n".join(buf) + "</pre>")
            buf.clear()
    first = True
    for p in paras:
        t = re.sub(r"\s*\n\s*", " ", p).strip()
        if first:
            out.append(f"<h1>{t}</h1>")
            first = False
            continue
        if re.match(r"(&gt;|#|C:\\|PS |\$ )", t):
            buf.append(t)
            continue
        flush()
        out.append(f"<p>{t}</p>")
    flush()
    return page(path, title or "Windows Setup", "dr-doc", "\n".join(out), lead, tail)


def build_reference(path, src, labels):
    lead, title, body, tail = split_document(src)
    body = remove_chrome(body)
    body = convert_title_bars(body)
    body = convert_features(body)
    body = convert_div_commands(body)
    body = convert_listing_blocks(body)
    body = convert_headings(body)
    body = convert_functions(body)
    body = convert_fn_lists(body)
    body = wrap_tables(body)
    body = tidy(body)
    if not re.search(r"<h1\b", body, I):
        h1 = labels.get(path) or title
        if h1:
            body = f"<h1>{html.escape(h1)}</h1>\n{body}"
    return page(path, title or text_of((re.search(r"<h1[^>]*>(.*?)</h1>", body, I | S) or [None, ""])[1]),
                "dr-doc", body, lead, tail)


def kind_of(path, src):
    if path in BROWSERS or re.search(r"<frameset", src, I):
        return "browser"
    if os.path.basename(path) == "p_lchild.htm":
        return "nav"
    if os.path.basename(path) == "README_windows_setup.html":
        return "setup"
    body = split_document(src)[2]
    if re.search(r'FONT FACE="Courier New"', body, I) and "inc2help" not in src:
        return "listing"
    if re.search(r"<img", body, I) and len(text_of(body)) < 40:
        return "diagram"
    return "reference"


def convert(path, force=False, labels=None):
    full = os.path.join(DOCS, path)
    with open(full, encoding="utf-8", errors="surrogateescape") as f:
        src = f.read()
    k = kind_of(path, src)
    if MARK in src and not (force and k == "browser"):
        return None
    if k == "browser":
        if path not in BROWSERS:
            return None
        new = build_browser(path, src)
    elif k == "nav":
        new = build_navpage(path, src)
    elif k == "setup":
        new = build_setup(path, src)
    elif k == "listing":
        new = build_listing(path, src, labels)
    elif k == "diagram":
        new = build_diagram(path, src)
    else:
        new = build_reference(path, src, labels)
    with open(full, "w", encoding="utf-8", errors="surrogateescape") as f:
        f.write(new)
    return k


def main(argv):
    force = "--force" in argv
    files = [a for a in argv if not a.startswith("--")]
    if not files:
        for root, _, names in os.walk(DOCS):
            for n in names:
                if n.endswith((".htm", ".html")):
                    files.append(os.path.relpath(os.path.join(root, n), DOCS))
    else:
        files = [os.path.relpath(os.path.abspath(f), DOCS) for f in files]
    files = sorted(f.replace(os.sep, "/") for f in files)
    labels = nav_label_map()  # read before the nav pages are converted
    # nav pages last so the browser pages can still read the old markup
    files.sort(key=lambda p: os.path.basename(p) == "p_lchild.htm")
    counts = {}
    for p in files:
        k = convert(p, force, labels)
        counts[k or "skipped"] = counts.get(k or "skipped", 0) + 1
    print(", ".join(f"{k}: {v}" for k, v in sorted(counts.items())))


if __name__ == "__main__":
    main(sys.argv[1:])
