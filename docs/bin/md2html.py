#!/usr/bin/env python3
"""Converts README.md into a page in the documentation style (the page
head and footer of txt2html.py).

usage: python3 docs/bin/md2html.py README.md docs/readme.html "Page Title"

Python standard library only. It handles the Markdown README.md uses:
# headings, paragraphs, - lists (with indented continuation lines),
| tables |, ``` fenced code, `code`, **bold** and [text](link) links.
Links are written for the page's place in docs/: docs/x becomes x, a
file txt2html.py has a page for links to that page, and any other
repository file gets ../ in front.
"""
import html, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from txt2html import HEAD, FOOT, LINKS, anchor  # noqa: E402

PAGES = dict(LINKS)
PAGES.update({'README.txt': 'index.html',
              'README_windows_setup.txt': 'README_windows_setup.html'})


def link_target(url):
    if re.match(r'[a-z]+:', url) or url.startswith('#'):
        return url
    path, _, frag = url.partition('#')
    if path in PAGES:
        path = PAGES[path]
    elif path.startswith('docs/'):
        path = path[len('docs/'):]
        # 11/18/2026: a Markdown page in docs/ links to its generated page, when it has one
        here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        if path.endswith('.md') and os.path.exists(os.path.join(here, path[:-3] + '.html')):
            path = path[:-3] + '.html'
    else:
        path = '../' + path
    return path + ('#' + frag if frag else '')


def inline(s):
    # Code spans first, kept out of the other rules
    parts = re.split(r'(`[^`]+`)', s)
    out = []
    for p in parts:
        if p.startswith('`') and p.endswith('`') and len(p) > 1:
            out.append('<code>%s</code>' % html.escape(p[1:-1]))
            continue
        t = html.escape(p, quote=False)
        t = re.sub(r'\[([^\]]+)\]\(([^)\s]+)\)',
                   lambda m: '<a href="%s">%s</a>' % (html.escape(link_target(m.group(2))), m.group(1)), t)
        t = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', t)
        out.append(t)
    return ''.join(out)


def cells(row):
    row = row.strip()
    if row.startswith('|'):
        row = row[1:]
    if row.endswith('|'):
        row = row[:-1]
    return [c.strip() for c in row.split('|')]


def convert(lines):
    out = []
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        s = line.strip()
        if not s:
            i += 1
            continue
        if s.startswith('```'):
            i += 1
            code = []
            while i < n and not lines[i].strip().startswith('```'):
                code.append(lines[i])
                i += 1
            i += 1
            out.append('<pre>%s</pre>\n' % html.escape('\n'.join(code)))
            continue
        m = re.match(r'(#{1,6})\s+(.*)', s)
        if m:
            level = len(m.group(1))
            text = m.group(2).strip()
            if level == 1:
                out.append('<h1>%s</h1>\n' % inline(text))
            else:
                out.append('<h%d id="%s">%s</h%d>\n' % (level, anchor(text), inline(text), level))
            i += 1
            continue
        if s.startswith('|'):
            rows = []
            while i < n and lines[i].strip().startswith('|'):
                rows.append(cells(lines[i]))
                i += 1
            head, body = rows[0], [r for r in rows[1:] if not all(re.match(r'^:?-+:?$', c) for c in r)]
            t = ['<table>\n<thead><tr>', ''.join('<th>%s</th>' % inline(c) for c in head), '</tr></thead>\n<tbody>\n']
            for r in body:
                t.append('<tr>%s</tr>\n' % ''.join('<td>%s</td>' % inline(c) for c in r))
            t.append('</tbody>\n</table>\n')
            out.append(''.join(t))
            continue
        if s.startswith('- '):
            items = []
            while i < n and (lines[i].strip().startswith('- ') or
                             (lines[i].startswith('  ') and lines[i].strip() and items)):
                if lines[i].strip().startswith('- '):
                    items.append(lines[i].strip()[2:])
                else:
                    items[-1] += ' ' + lines[i].strip()
                i += 1
            out.append('<ul>\n%s</ul>\n' % ''.join('<li>%s</li>\n' % inline(x) for x in items))
            continue
        para = []
        while i < n and lines[i].strip() and not re.match(r'(#{1,6}\s|```|\||- )', lines[i].strip()):
            para.append(lines[i].strip())
            i += 1
        out.append('<p>%s</p>\n' % inline(' '.join(para)))
    return ''.join(out)


def main():
    src, dst, title = sys.argv[1], sys.argv[2], sys.argv[3]
    lines = open(src, encoding='utf-8').read().split('\n')
    head = HEAD.format(title=html.escape(title), src=src.split('/')[-1], readme_current='')
    head = head.replace('by docs/bin/txt2html.py', 'by docs/bin/md2html.py')
    with open(dst, 'w', encoding='utf-8') as f:
        f.write(head)
        f.write(convert(lines))
        f.write(FOOT)


if __name__ == '__main__':
    main()
