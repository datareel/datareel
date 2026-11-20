#!/usr/bin/env python3
"""Refreshes the DataReel 7 AI guides.

1. In each docs/ai/*.md guide, replaces the code between
       <!-- sample: path [from="text"] -->
       <!-- end sample -->
   with the current contents of path (relative to the top folder),
   starting at the first line that contains text when from is given.
2. Writes llms.txt (an index, see https://llmstxt.org) and
   llms-full.txt (AGENTS.md and every guide in one file).

usage: python3 docs/bin/make_ai_docs.py   (from any folder)
"""
import os, re

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
AI = os.path.join(TOP, 'docs', 'ai')

# Guides in reading order: (file, title, one-line description)
GUIDES = [
    ('tls-comms.md', 'TLS communications',
     'TLS clients and servers, HTTPS, SMTP, POP3, FTPS and gxStream, with complete programs'),
    ('object-database.md', 'Object database',
     'gxDatabase block files, gxBtree indexes, POD and InfoHog persistent objects, vector indexes and sets'),
    ('building.md', 'Building and linking',
     'CMake options, file format settings, find_package(DataReel), running the tests'),
    ('porting-from-6x.md', 'Porting from 6.x',
     'Replacements for the removed classes and the move to TLS-only networking'),
    ('utilities.md', 'Text, CSV, single-instance and file utilities',
     'strip_ansi/AnsiStripper, gxCSVReader/gxCSVWriter, gxProcessLock, hexdump, ffind, txfilter (7.4)'),
    ('troubleshooting-and-support.md', 'Troubleshooting and support',
     'Read-only checks, problems by area, when to stop and escalate, the support summary'),
    ('python.md', 'The datareel Python module (7.5 beta)',
     'Build the pip package, read gxObjRepl nodes, vector index files and block files from Python'),
]

# 11/20/2026 (7.5): other Markdown files whose samples are refreshed too (not in llms-full.txt)
OTHER_DOCS = ['python/docs/users-manual.md']

SAMPLE = re.compile(r'(<!-- sample: (\S+)(?: from="([^"]*)")? -->\n)(.*?)(<!-- end sample -->)',
                    re.S)

def sample_text(path, start):
    lines = open(os.path.join(TOP, path), encoding='utf-8').read().split('\n')
    if start:
        for i, l in enumerate(lines):
            if start in l:
                lines = lines[i:]
                break
        else:
            raise SystemExit('%s: "%s" not found' % (path, start))
    # Drop the DataReel end of file banner and trailing blank lines
    while lines and (not lines[-1].strip() or lines[-1].startswith('// ---')
                     or 'End of File' in lines[-1]):
        lines.pop()
    return '\n'.join(lines)

def refresh_guide(fname, folder=AI):
    path = os.path.join(folder, fname)
    text = open(path, encoding='utf-8').read()
    def repl(m):
        code = sample_text(m.group(2), m.group(3))
        src = m.group(2) + (' (license header omitted)' if m.group(3) else '')
        # 11/20/2026 (7.5): Python samples are fenced as python with a # comment
        if m.group(2).endswith('.py'):
            return '%s```python\n# %s\n%s\n```\n%s' % (m.group(1), src, code, m.group(5))
        return '%s```cpp\n// %s\n%s\n```\n%s' % (m.group(1), src, code, m.group(5))
    new = SAMPLE.sub(repl, text)
    if new != text:
        open(path, 'w', encoding='utf-8').write(new)
        print('updated', os.path.relpath(path, TOP))
    return new

def strip_markers(text):
    return re.sub(r'<!-- (sample: [^>]*|end sample) -->\n', '', text)

def main():
    guides = {f: refresh_guide(f) for f, _, _ in GUIDES}
    for f in OTHER_DOCS:
        refresh_guide(f, TOP)

    index = ['# DataReel 7', '',
             '> DataReel 7 is an object database (OODB) and cross platform '
             'communications toolkit for C++17 (LGPL): embedded object '
             'databases (block files, disk B-tree indexes, persistent objects) '
             'and TLS-only communications (TLS '
             'client/server, HTTPS, SMTP, POP3, FTPS) on Linux and Windows.', '',
             'All classes are in namespace datareel. Link with CMake target '
             'DataReel::datareel. Network calls return 0 on success; database '
             'calls return gxDatabaseError (gxDBASE_NO_ERROR is 0). Records and '
             'keys must be fixed-size (no std::string or pointers). Certificates '
             'are verified by default; there is no plain text HTTP, SMTP, POP3 '
             'or FTP.', '',
             'AI-assisted use only: before any AI assistance, review your '
             "agency's or company's AI usage policies and get formal written "
             'approval from your management team; use only approved enterprise '
             'AI models; a person reviews and approves every step (see "Before '
             'you use AI assistance" in AGENTS.md). Naming an AI tool is a '
             'reference only, not an endorsement.', '',
             '## Guides', '',
             '- [AGENTS.md](AGENTS.md): AI-assisted use rules, setup, new builds, '
             'upgrades, forks, troubleshooting and support, and the rules for '
             'writing code with DataReel and changing the library']
    for f, title, desc in GUIDES:
        index.append('- [%s](docs/ai/%s): %s' % (title, f, desc))
    index += ['', '## Reference', '',
              '- [Readme](README.txt): What the library is for, building, file formats',
              '- [Python package](python/README.md): the datareel pip package (7.5 beta 1), '
              'its [user\'s manual](python/docs/users-manual.md), [build guide](python/docs/building.md) '
              'and [AGENTS.md](python/AGENTS.md)',
              '- [Release notes, all 7.x](RELEASE_NOTES.txt): every 7.x release in one file, newest first',
              '- [Release notes 7.5](RELEASE_NOTES_7.5.txt): 7.5 beta 1, the datareel Python module',
              '- [Release notes 7.4](RELEASE_NOTES_7.4.txt): Vector extensions: '
              'vector index file, exact and HNSW nearest-neighbor search',
              '- [Release notes 7.3.3](RELEASE_NOTES_7.3.3.txt): Database '
              'replication fixes (seal per folder, promote-check under the apply lock)',
              '- [Release notes 7.3.2](RELEASE_NOTES_7.3.2.txt): Database '
              'replication fixes (channel resync, staged copies, promote)',
              '- [Release notes 7.3.1](RELEASE_NOTES_7.3.1.txt): Security scan '
              'fixes for damaged files and journals',
              '- [Release notes 7.3](RELEASE_NOTES_7.3.txt): Memory-mapped reads, '
              'faster key-order scans and random reads',
              '- [Release notes 7.2](RELEASE_NOTES_7.2.txt): Journal (crash and '
              'power loss safe transactions), page cache, faster I/O and B-tree',
              '- [Release notes 7.1](RELEASE_NOTES_7.1.txt): gxRDBMS removed, '
              'reserved error codes',
              '- [Release notes 7.0](RELEASE_NOTES_7.0.txt): Removed classes, '
              'rewritten classes, fixes, porting notes',
              '- [Class reference](docs/classes/classes.htm): HTML pages for every class',
              '- [Examples](docs/examples/examples.htm): The example programs '
              'and how to run them',
              '', '## Optional', '',
              '- [llms-full.txt](llms-full.txt): AGENTS.md and all guides in one file', '']
    open(os.path.join(TOP, 'llms.txt'), 'w', encoding='utf-8').write('\n'.join(index))

    full = ['# DataReel 7: AGENTS.md and AI guides in one file', '',
            'Generated by docs/bin/make_ai_docs.py from AGENTS.md and docs/ai/*.md.',
            '']
    agents = open(os.path.join(TOP, 'AGENTS.md'), encoding='utf-8').read()
    full += ['=' * 72, 'FILE: AGENTS.md', '=' * 72, '', agents.rstrip(), '']
    for f, _, _ in GUIDES:
        full += ['=' * 72, 'FILE: docs/ai/' + f, '=' * 72, '',
                 strip_markers(guides[f]).rstrip(), '']
    open(os.path.join(TOP, 'llms-full.txt'), 'w', encoding='utf-8').write('\n'.join(full))
    print('wrote llms.txt and llms-full.txt')

main()
