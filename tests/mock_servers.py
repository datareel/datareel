#!/usr/bin/env python3
"""Minimal TLS mail and FTPS servers used by tests/run_tests.sh to test the
DataReel 7 SMTP, POP3 and FTPS clients. Not for any other use.

usage: mock_servers.py cert.pem key.pem workdir base_port
  base_port+0  SMTP, implicit TLS      base_port+1  SMTP, STARTTLS
  base_port+2  POP3, implicit TLS      base_port+3  POP3, STLS
  base_port+4  FTPS, implicit TLS      base_port+5  FTPS, AUTH TLS
  base_port+6  SMTP without STARTTLS (the client must refuse it)
  base_port+7  FTPS passive data port (one transfer at a time)
Received mail is written to workdir/mail.txt; FTP files live in
workdir/ftp. User "test", password "secret".
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

servers = [(0, smtp, True), (1, smtp, False), (2, pop3, True), (3, pop3, False),
           (4, ftps, True), (5, ftps, False),
           (6, lambda c, s: smtp(c, s, offer_starttls=False), False)]
for off, h, implicit in servers:
    srv = Server(('::', BASE + off), make(h, implicit))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
print('ready', flush=True)
threading.Event().wait()
