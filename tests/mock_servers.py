#!/usr/bin/env python3
"""Minimal TLS mail and FTPS servers used by tests/run_tests.sh to test the
DataReel 7 SMTP, POP3 and FTPS clients. Not for any other use.

usage: mock_servers.py cert.pem key.pem workdir base_port
  base_port+0  SMTP, implicit TLS      base_port+1  SMTP, STARTTLS
  base_port+2  POP3, implicit TLS      base_port+3  POP3, STLS
  base_port+4  FTPS, implicit TLS      base_port+5  FTPS, AUTH TLS
  base_port+6  SMTP without STARTTLS (the client must refuse it)
  base_port+7  FTPS passive data port (one transfer at a time)
  base_port+8  IMAP, implicit TLS      base_port+9  IMAP, STARTTLS (LOGINDISABLED
                                                    until TLS starts)
  base_port+10 IMAP, plain text
Received mail is written to workdir/mail.txt; FTP files live in
workdir/ftp. User "test", password "secret" (IMAP also accepts the 8-bit
user "t\u00ebst" with password "s\u00ebcret", sent as literals).
"""
import os, socket, socketserver, ssl, sys, threading, base64

CERT, KEY, WORK, BASE = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
FTPDIR = os.path.join(WORK, 'ftp')
os.makedirs(FTPDIR, exist_ok=True)
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.minimum_version = ssl.TLSVersion.TLSv1_2
ctx.load_cert_chain(CERT, KEY)

class Conn:
    def __init__(self, sock):
        self.sock = sock
        self.f = sock.makefile('rb')
    def send(self, line):
        self.sock.sendall((line + '\r\n').encode())
    def read(self, n):
        b = self.f.read(n)
        if len(b) != n: raise EOFError
        return b
    def sendraw(self, b):
        self.sock.sendall(b)
    def readline(self):
        l = self.f.readline()
        if not l: raise EOFError
        return l.decode('utf-8', 'replace').rstrip('\r\n')
    def tls(self):
        self.f.close()
        self.sock = ctx.wrap_socket(self.sock, server_side=True)
        self.f = self.sock.makefile('rb')

class Server(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True
    address_family = socket.AF_INET6
    def server_bind(self):
        self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        super().server_bind()

def make(handler, implicit):
    class H(socketserver.BaseRequestHandler):
        def handle(self):
            s = self.request
            if implicit:
                try: s = ctx.wrap_socket(s, server_side=True)
                except Exception: return
            try: handler(Conn(s), implicit)
            except (EOFError, ConnectionError, ssl.SSLError): pass
    return H

def smtp(c, secure, offer_starttls=True):
    c.send('220 mock ESMTP')
    while True:
        l = c.readline(); u = l.upper()
        if u.startswith('EHLO'):
            ext = ['250-mock']
            if not secure and offer_starttls: ext.append('250-STARTTLS')
            ext.append('250-AUTH PLAIN LOGIN')
            ext.append('250 8BITMIME')
            for e in ext: c.send(e)
        elif u == 'STARTTLS' and not secure:
            c.send('220 go ahead'); c.tls(); secure = True
        elif u.startswith('AUTH PLAIN '):
            creds = base64.b64decode(l[11:]).split(b'\0')
            c.send('235 ok' if creds[1:] == [b'test', b'secret'] and secure else '535 no')
        elif u.startswith('MAIL FROM:') or u.startswith('RCPT TO:'):
            c.send('250 ok' if secure else '530 TLS required')
        elif u == 'DATA':
            c.send('354 send it')
            lines = []
            while True:
                d = c.readline()
                if d == '.': break
                lines.append(d[1:] if d.startswith('.') else d)
            with open(os.path.join(WORK, 'mail.txt'), 'w') as f:
                f.write('\n'.join(lines) + '\n')
            c.send('250 queued')
        elif u == 'QUIT':
            c.send('221 bye'); return
        else:
            c.send('500 what')

def pop3(c, secure):
    msgs = ['Subject: one\r\n\r\nFirst message\r\n.dot line\r\n',
            'Subject: two\r\n\r\nSecond message\r\n']
    c.send('+OK mock POP3')
    user = None
    while True:
        l = c.readline(); u = l.upper()
        if u == 'STLS' and not secure:
            c.send('+OK begin TLS'); c.tls(); secure = True
        elif u.startswith('USER '):
            user = l[5:]; c.send('+OK' if secure else '-ERR TLS required')
        elif u.startswith('PASS '):
            c.send('+OK logged in' if (user, l[5:]) == ('test', 'secret') else '-ERR no')
        elif u == 'STAT':
            c.send('+OK %d %d' % (len(msgs), sum(len(m) for m in msgs)))
        elif u == 'LIST':
            c.send('+OK')
            for i, m in enumerate(msgs): c.send('%d %d' % (i + 1, len(m)))
            c.send('.')
        elif u.startswith('RETR '):
            m = msgs[int(l[5:]) - 1]
            c.send('+OK')
            for line in m.split('\r\n')[:-1]:
                c.send('.' + line if line.startswith('.') else line)
            c.send('.')
        elif u == 'QUIT':
            c.send('+OK bye'); return
        else:
            c.send('-ERR what')

def ftps(c, secure):
    c.send('220 mock FTPS')
    prot_p = False
    cwd = FTPDIR
    pasv = None
    def data_close(d):
        # Send close_notify; the client may already have closed its side
        try: d.unwrap()
        except (OSError, ValueError): pass
        d.close()
    def data_conn():
        s, _ = pasv.accept()
        pasv.close()
        if not prot_p: return s
        return ctx.wrap_socket(s, server_side=True)
    while True:
        l = c.readline(); u = l.upper()
        if u == 'AUTH TLS':
            c.send('234 go'); c.tls(); secure = True
        elif u.startswith('USER'):
            c.send('331 password' if secure else '530 TLS required')
        elif u.startswith('PASS'):
            c.send('230 ok' if l[5:] == 'secret' else '530 no')
        elif u == 'PBSZ 0':
            c.send('200 ok')
        elif u == 'PROT P':
            prot_p = True; c.send('200 ok')
        elif u.startswith('TYPE'):
            c.send('200 ok')
        elif u == 'EPSV':
            pasv = socket.socket(socket.AF_INET6)
            pasv.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
            pasv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            pasv.bind(('::', BASE + 7)); pasv.listen(1)
            c.send('229 Entering Extended Passive Mode (|||%d|)' % pasv.getsockname()[1])
        elif u.startswith('LIST') or u.startswith('NLST'):
            if not prot_p: c.send('521 PROT P required'); continue
            c.send('150 here it comes')
            d = data_conn()
            d.sendall(''.join(n + '\r\n' for n in sorted(os.listdir(cwd))).encode())
            data_close(d)
            c.send('226 done')
        elif u.startswith('RETR '):
            path = os.path.join(cwd, os.path.basename(l[5:]))
            if not os.path.isfile(path): c.send('550 no file'); continue
            c.send('150 sending')
            d = data_conn()
            with open(path, 'rb') as f: d.sendall(f.read())
            data_close(d)
            c.send('226 done')
        elif u.startswith('STOR '):
            path = os.path.join(cwd, os.path.basename(l[5:]))
            c.send('150 send it')
            d = data_conn()
            with open(path, 'wb') as f:
                while True:
                    b = d.recv(65536)
                    if not b: break
                    f.write(b)
            data_close(d)
            c.send('226 stored')
        elif u.startswith('SIZE '):
            path = os.path.join(cwd, os.path.basename(l[5:]))
            c.send('213 %d' % os.path.getsize(path) if os.path.isfile(path) else '550 no')
        elif u == 'PWD':
            c.send('257 "/" is the directory')
        elif u == 'QUIT':
            c.send('221 bye'); return
        else:
            c.send('502 not implemented')

# 11/16/2026 (7.4): IMAP for gxsIMAPClient. The bodies hold the text that
# cut the 2007 client's reads short: "NO" and "BAD" words, a "}" and a
# line that looks like a tagged reply.
IMAP_MSGS = [
    ('17-Apr-2007 12:00:00 -0400', 'Tue, 17 Apr 2007 11:59:00 -0400', 'one (report)',
     'Subject: one (report)\r\nFrom: a@example.com\r\n\r\n',
     'First message\r\nNO problems, nothing BAD here }\r\nA0006 OK not a reply\r\nEnd\r\n'),
    ('18-Apr-2007 08:30:00 -0400', 'Wed, 18 Apr 2007 08:29:00 -0400', 'two',
     'Subject: two\r\n\r\n', 'Second\r\n'),
]

def imap_astrings(c, rest, n):
    """Read n astrings (quoted, literal or atom) from rest, asking for literals"""
    out = []
    while len(out) < n:
        rest = rest.lstrip(' ')
        if rest.startswith('"'):
            i, v = 1, ''
            while rest[i] != '"':
                if rest[i] == '\\': i += 1
                v += rest[i]; i += 1
            out.append(v); rest = rest[i + 1:]
        elif rest.startswith('{') and rest.endswith('}'):
            size = int(rest[1:-1])
            c.send('+ go ahead')
            out.append(c.read(size).decode('utf-8'))
            rest = c.readline()
        else:
            v = rest.split(' ', 1)
            out.append(v[0]); rest = v[1] if len(v) > 1 else ''
    return out

def imap(c, secure, starttls_port=False):
    def caps():
        cap = 'IMAP4rev1'
        if starttls_port and not secure: cap += ' STARTTLS LOGINDISABLED'
        return cap
    c.send('* OK [CAPABILITY %s] mock IMAP' % caps())
    selected = False
    while True:
        l = c.readline()
        tag, _, rest = l.partition(' ')
        cmd, _, args = rest.partition(' ')
        u = cmd.upper()
        if u == 'CAPABILITY':
            c.send('* CAPABILITY ' + caps()); c.send(tag + ' OK done')
        elif u == 'STARTTLS' and starttls_port and not secure:
            c.send(tag + ' OK begin TLS'); c.tls(); secure = True
        elif u == 'LOGIN':
            user, pw = imap_astrings(c, args, 2)
            if starttls_port and not secure:
                c.send(tag + ' NO LOGIN is disabled before TLS')
            elif (user, pw) in (('test', 'secret'), ('tëst', 'sëcret'), ('quote', 'a b"c\\d')):
                c.send(tag + ' OK logged in')
            else:
                c.send(tag + ' NO [AUTHENTICATIONFAILED] wrong user or password')
        elif u in ('SELECT', 'EXAMINE'):
            name = imap_astrings(c, args, 1)[0]
            if name.upper() != 'INBOX':
                c.send(tag + ' NO no such mailbox'); continue
            c.send('* FLAGS (\\Seen \\Deleted)')
            c.send('* %d EXISTS' % len(IMAP_MSGS)); c.send('* 0 RECENT')
            selected = True
            c.send(tag + ' OK [READ-WRITE] selected')
        elif u == 'LIST':
            c.send('* LIST (\\HasNoChildren) "/" INBOX')
            c.send('* LIST (\\HasNoChildren) "/" "Sent Items"')
            c.send(tag + ' OK listed')
        elif u == 'SEARCH' and selected:
            a = args.upper()
            if a.startswith('SUBJECT '):
                want = args[8:].strip('"').lower()
                hits = [i + 1 for i, m in enumerate(IMAP_MSGS) if want in m[2].lower()]
            else:
                hits = list(range(1, len(IMAP_MSGS) + 1))
            c.send('* SEARCH' + ''.join(' %d' % h for h in hits)); c.send(tag + ' OK searched')
        elif u == 'FETCH' and selected:
            num, _, items = args.partition(' ')
            n = int(num)
            if n < 1 or n > len(IMAP_MSGS):
                c.send(tag + ' BAD no such message'); continue
            idate, edate, subj, hdr, body = IMAP_MSGS[n - 1]
            c.send('* %d FETCH (FLAGS (\\Recent))' % n) # unsolicited, before the reply
            out = b'* %d FETCH (' % n
            parts = []
            for it in items.strip('()').upper().split():
                if it == 'INTERNALDATE':
                    parts.append(b'INTERNALDATE "' + idate.encode() + b'"')
                elif it == 'ENVELOPE':
                    parts.append(('ENVELOPE ("%s" "%s" NIL NIL NIL NIL NIL NIL NIL "<%d@mock>")' % (edate, subj, n)).encode())
                elif it.startswith('BODY.PEEK['):
                    sect = it[10:-1]
                    data = {'HEADER': hdr, 'TEXT': body, '': hdr + body}[sect]
                    if n == 2 and sect == 'TEXT': # a quoted string, not a literal
                        parts.append(b'BODY[TEXT] "Second\\r\\n"'.replace(b'\\r\\n', b''))
                        continue
                    d = data.encode()
                    parts.append(b'BODY[' + sect.encode() + b'] {%d}\r\n' % len(d) + d)
            out += b' '.join(parts) + b')\r\n'
            c.sendraw(out)
            c.send(tag + ' OK fetched')
        elif u in ('STORE', 'EXPUNGE', 'NOOP'):
            c.send(tag + ' OK done')
        elif u == 'LOGOUT':
            c.send('* BYE logging out'); c.send(tag + ' OK bye'); return
        else:
            c.send(tag + ' BAD unknown command')

servers = [(0, smtp, True), (1, smtp, False), (2, pop3, True), (3, pop3, False),
           (4, ftps, True), (5, ftps, False),
           (6, lambda c, s: smtp(c, s, offer_starttls=False), False),
           (8, imap, True), (9, lambda c, s: imap(c, s, starttls_port=True), False),
           (10, imap, False)]
for off, h, implicit in servers:
    srv = Server(('::', BASE + off), make(h, implicit))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
print('ready', flush=True)
threading.Event().wait()
