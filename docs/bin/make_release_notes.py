#!/usr/bin/env python3
"""Writes the combined release notes of one DataReel line, newest release first (11/20/2026):

  RELEASE_NOTES.txt          every RELEASE_NOTES_<MAJOR>.*.txt of the line in one text file
  docs/release_notes.html    the same as one page, with a contents list of the releases

usage: python3 docs/bin/make_release_notes.py [MAJOR]   (from any folder; MAJOR: the
       first number of version.txt by default, 7 in a 7.x tree, 6 in a 6.x tree)

The per-release files RELEASE_NOTES_<version>.txt stay the source: edit those and run this
script again, never the combined files. The page head and footer are those of txt2html.py.
"""
import html, os, re, sys

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import txt2html  # noqa: E402


def version_key(v):
    return [int(x) for x in re.findall(r'\d+', v)]


def releases(major):
    out = []
    for f in os.listdir(TOP):
        m = re.fullmatch(r'RELEASE_NOTES_(%s(?:\.\d+)*)\.txt' % major, f)
        if m:
            out.append((m.group(1), f))
    return sorted(out, key=lambda x: version_key(x[0]), reverse=True)


def vid(v):
    return 'v' + v.replace('.', '_')


def main():
    vt = open(os.path.join(TOP, 'version.txt'), encoding='utf-8').read().strip()
    major = sys.argv[1] if len(sys.argv) > 1 else vt.split('.')[0]
    rels = releases(major)
    if not rels:
        sys.exit('no RELEASE_NOTES_%s.*.txt in %s' % (major, TOP))
    titles = {}
    texts = {}
    for v, f in rels:
        t = open(os.path.join(TOP, f), encoding='utf-8').read().rstrip('\n')
        texts[v] = t
        titles[v] = t.split('\n', 1)[0].strip()
    name = 'DataReel %s Release Notes (all %s.x releases)' % (major, major)

    # The text file
    rule = '=' * 72
    txt = [name, 'DataReel Copyright (c) 2001-2026 DataReel Software Development', '',
           'Every DataReel %s.x release, newest first. Made by docs/bin/make_release_notes.py from' % major,
           'the per-release files RELEASE_NOTES_%s.*.txt; edit those, not this file.' % major, '',
           'CONTENTS', '--------']
    txt += ['- %s (%s)' % (titles[v], f) for v, f in rels]
    for v, f in rels:
        txt += ['', rule, f, rule, '', texts[v]]
    with open(os.path.join(TOP, 'RELEASE_NOTES.txt'), 'w', encoding='utf-8') as out:
        out.write('\n'.join(txt) + '\n')

    # The page: each release's notes as txt2html makes them, one level down, under its own heading
    # and with anchors of its own; RELEASE_NOTES_<v>.txt in the text links to that release on the page
    txt2html.LINKS = dict(txt2html.LINKS)
    for v, f in rels:
        txt2html.LINKS[f] = '#' + vid(v)
    parts = ['<h1>%s</h1>\n' % html.escape(name),
             '<p>Every DataReel %s.x release, newest first. Each release also has its own page and text file.</p>\n' % major,
             '<div class="dr-toc-title">Releases</div>\n<p class="dr-toc">' +
             '<br>\n'.join('<a href="#%s">%s</a>' % (vid(v), html.escape(titles[v])) for v, _ in rels) + '</p>\n']
    for v, f in rels:
        lines = texts[v].split('\n')
        start = lines.index('') if '' in lines else 0
        frag = txt2html.convert(lines[start:], titles[v])
        frag = re.sub(r'\A<h1>.*?</h1>\n', '', frag)
        frag = re.sub(r'<div class="dr-toc-title">Contents</div>\n<p class="dr-toc">.*?</p>\n', '', frag, flags=re.S)
        frag = re.sub(r'<(/?)h3>', r'<\1h4>', frag)
        frag = re.sub(r'<h2 id="([^"]*)">(.*?)</h2>', lambda m: '<h3 id="%s_%s">%s</h3>' % (vid(v), m.group(1), m.group(2)), frag)
        frag = re.sub(r'<(/?)h2>', r'<\1h3>', frag)
        intro = ''.join('%s<br>\n' % html.escape(x) for x in lines[1:start]
                        if x.strip() and not x.startswith('DataReel Copyright'))
        parts.append('<h2 id="%s">%s</h2>\n%s%s' % (vid(v), html.escape(titles[v]),
                                                    '<p>%s</p>\n' % intro if intro else '', frag))
    foot = txt2html.FOOT            # the footer of every page of this tree (its version)
    with open(os.path.join(TOP, 'docs', 'release_notes.html'), 'w', encoding='utf-8') as out:
        out.write(txt2html.HEAD.format(title=html.escape(name), src='RELEASE_NOTES_%s.*.txt' % major, readme_current=''))
        out.write(''.join(parts))
        out.write(foot)
    print('wrote RELEASE_NOTES.txt and docs/release_notes.html: %d releases' % len(rels))


main()
