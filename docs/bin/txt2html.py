#!/usr/bin/env python3
"""Converts the DataReel text documents (README.txt, RELEASE_NOTES_*.txt)
into pages in the documentation style.

usage: python3 docs/bin/txt2html.py input.txt output.html "Page Title"

Text rules: a line followed by a line of dashes is a section heading;
lines starting with "- " are list items (indented lines continue them);
blocks of indented lines are shown as preformatted text; other blocks
are paragraphs. URLs become links.
"""
import html, re, sys

HEAD = '''<!doctype html>
<!-- Generated from {src} by docs/bin/txt2html.py -->
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="generator" content="datareel-docs-modern">
<title>{title}</title>
<link rel="stylesheet" href="css/datareel.css">
<link rel="icon" href="images/favicon.ico" sizes="any">
<link rel="icon" type="image/png" sizes="192x192" href="images/datareel_icon_192.png">
<link rel="apple-touch-icon" href="images/datareel_icon_apple_touch.png">
<script src="js/datareel.js"></script>
</head>
<body class="dr-doc">
<header class="dr-header">
  <a class="dr-brand" href="index.html"><img class="dr-logo-light" src="images/datareel_logo.svg" alt="DataReel" width="173" height="48"><img class="dr-logo-dark" src="images/datareel_logo_dark.svg" alt="DataReel" width="173" height="48"><span>C++ Toolkit Documentation</span></a>
  <nav class="dr-nav" aria-label="Documentation sections"><a href="index.html"{readme_current}>Readme</a><a href="classes/classes.htm">Library</a><a href="examples/examples.htm">Examples</a><a href="utils/utils.htm">Utilities</a><a href="drcm.html">Cluster Manager</a><a href="code_review_fixes.html">Code Review</a></nav>
</header>
<main class="dr-main">
'''
FOOT = '''</main>
<footer class="dr-footer">DataReel 7.0 &middot; Copyright (c) 2001-2026 DataReel Software Development</footer>
</body>
</html>
'''

LINKS = {
  'RELEASE_NOTES_7.0.txt': 'release_notes_7_0.html',
  'RELEASE_NOTES_6.0.txt': 'release_notes_6_0.html',
  'CODE_REVIEW_FIXES.txt': 'code_review_fixes.html',
  'README_windows_openssl.txt': '../README_windows_openssl.txt',
  'AGENTS.md': '../AGENTS.md',
  'llms-full.txt': '../llms-full.txt',
}

def inline(s):
    s = html.escape(s)
    s = re.sub(r'(https?://[^\s<]+[^\s<.,)])', r'<a href="\1">\1</a>', s)
    s = re.sub(r'docs/([\w/]+\.html?)', r'<a href="\1">docs/\1</a>', s)
    s = re.sub(r'(?<![-\w/])llms\.txt', '<a href="../llms.txt">llms.txt</a>', s)
    s = re.sub(r'\bdocs/ai\b(?!/)', '<a href="ai/README.md">docs/ai</a>', s)
    for k, v in LINKS.items():
        s = s.replace(k, '<a href="%s">%s</a>' % (v, k))
    return s

def join_lines(buf):
    # Joins wrapped lines, keeping words hyphenated at a line end together
    out = ''
    for x in buf:
        x = x.strip()
        out += x if not out or out.endswith('-') else ' ' + x
    return out

def anchor(t):
    return re.sub(r'[^A-Za-z0-9]+', '', t.title())

def convert(lines, title):
    out = ['<h1>%s</h1>\n' % html.escape(title)]
    i = 0
    n = len(lines)
    toc = []
    body = []
    def flush_para(buf):
        if not buf: return
        text = join_lines(buf)
        # A short line by itself without a period is a subheading
        if len(buf) == 1 and len(text) < 40 and not text.endswith(('.', ':')) \
           and not text.startswith('http'):
            body.append('<h3>%s</h3>\n' % inline(text))
        else:
            body.append('<p>%s</p>\n' % inline(text))
    while i < n:
        l = lines[i].rstrip('\n')
        nxt = lines[i+1].rstrip('\n') if i + 1 < n else ''
        if l.strip() and re.fullmatch(r'-{3,}', nxt.strip()) and not l.startswith(' '):
            heading = l.strip().capitalize() if l.replace('6.x', '').isupper() else l.strip()
            heading = re.sub(r'(?i)datareel', 'DataReel', heading)
            heading = re.sub(r'\b6\.x\b|\bTls\b|\bCmake\b', lambda m: m.group(0).upper().replace('X', 'x'), heading)
            a = anchor(heading)
            if heading == 'Contents':
                # The generated table of contents replaces the text one
                i += 2
                while i < n and not (i + 1 < n and re.fullmatch(r'-{3,}', lines[i+1].strip())):
                    i += 1
                continue
            toc.append((a, heading))
            body.append('<h2 id="%s">%s</h2>\n' % (a, html.escape(heading)))
            i += 2
            continue
        if not l.strip():
            i += 1
            continue
        if l.startswith('- '):
            items = []
            while i < n and (lines[i].startswith('- ') or
                             (lines[i].startswith('  ') and items)):
                if lines[i].startswith('- '): items.append(lines[i][2:].strip())
                else: items[-1] = join_lines([items[-1], lines[i]])
                i += 1
            body.append('<ul>\n' + ''.join('<li>%s</li>\n' % inline(x) for x in items) + '</ul>\n')
            continue
        if l.startswith(' '):
            block = []
            while i < n and (lines[i].startswith(' ') or not lines[i].strip()):
                if not lines[i].strip() and (i + 1 >= n or not lines[i+1].startswith(' ')):
                    break
                block.append(lines[i].rstrip('\n'))
                i += 1
            body.append('<pre>%s</pre>\n' % html.escape('\n'.join(block)))
            continue
        # A preformatted block of short columns (tables) starts at column 0
        # when the next lines are indented continuation columns
        para = []
        while i < n and lines[i].strip() and not lines[i].startswith('- ') \
              and not lines[i].startswith(' ') and \
              not (i + 1 < n and re.fullmatch(r'-{3,}', lines[i+1].strip())):
            para.append(lines[i])
            i += 1
        if len(para) > 1 and any(re.search(r'\S {2,}\S', p) for p in para):
            body.append('<pre>%s</pre>\n' % html.escape('\n'.join(p.rstrip() for p in para)))
        else:
            flush_para(para)
    if toc:
        out.append('<div class="dr-toc-title">Contents</div>\n<p class="dr-toc">' +
                   '<br>\n'.join('<a href="#%s">%s</a>' % (a, html.escape(h)) for a, h in toc) +
                   '</p>\n')
    return ''.join(out) + ''.join(body)

def main():
    src, dst, title = sys.argv[1], sys.argv[2], sys.argv[3]
    lines = open(src, encoding='utf-8').read().split('\n')
    # Skip the title lines above the first blank line
    start = lines.index('') if '' in lines else 0
    content = convert(lines[start:], title)
    readme = ' aria-current="page"' if dst.endswith('index.html') else ''
    with open(dst, 'w', encoding='utf-8') as f:
        f.write(HEAD.format(title=html.escape(title), src=src.split('/')[-1],
                            readme_current=readme))
        f.write(content)
        f.write(FOOT)

main()
