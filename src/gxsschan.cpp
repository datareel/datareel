// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxsschan.cpp
// C++ Compiler Used: MSVC, MINGW
// Produced By: DataReel Software Development Team
// File Creation Date: 10/24/2026
// Date Last Modified: 10/24/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ------------- Program Description and Details ------------- //
// ----------------------------------------------------------- //
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
USA

Windows Schannel version of the gxSSL class. This file is compiled as
part of gxssl.cpp when __USE_GX_SSL_EXTENSIONS__ and __USE_GX_SCHANNEL__
are defined. It uses the TLS support built into Windows, so programs
do not need OpenSSL. Link with secur32.lib, crypt32.lib and ncrypt.lib.

Differences from the OpenSSL version:

- Clients do not verify the server certificate during the handshake,
  the same as the OpenSSL version. Call VerifyCert() after the
  handshake. VerifyCert() checks the certificate chain and the host
  name with the Windows certificate store, or with the CA list set by
  SetCAList(), and returns the Windows CERT_E_* code in place of the
  X509_V_* code. 0 means the certificate is valid in both versions.

- Servers load the certificate and private key with InitCTX(). The
  certificate can be a PEM or DER file. The private key can be a PEM
  or DER PKCS#8 key ("BEGIN PRIVATE KEY", or "BEGIN ENCRYPTED PRIVATE
  KEY" with the passphrase), an RSA key ("BEGIN RSA PRIVATE KEY") or
  an EC key ("BEGIN EC PRIVATE KEY"). A .pfx or .p12 file can be
  passed as the certificate file with the key file set to null or the
  same file. Schannel needs the key in a Windows key store, so the key
  is imported into the current user's key store under a name made
  from the certificate thumbprint, and deleted by DestroyCTX().

- Schannel picks the protocol version, cipher suites and key exchange
  groups from the Windows settings. gxSSL_TLSv1_2 turns off TLS 1.3.
  The other protocol settings use the Windows defaults, which on
  Windows 10 and 11 are TLS 1.2 and TLS 1.3.

- The OpenSSL key and certificate generation functions,
  MakeRSAPrivateKey(), MakeCertReq() and MakeDHParms(), are not
  supported. Use the openssl command line program or the Windows
  certificate tools to make keys and certificates. LoadDHParms(),
  InitPRNG() and MakeRSAEphKey() do nothing and return 0, because
  Schannel handles key exchange and random numbers itself.
*/
// ----------------------------------------------------------- //
#if defined (__GX_SSL_SCHANNEL_SOURCE__)

#include <stdio.h>
#include <string.h>

// --------------------------------------------------------------
// Schannel context and connection objects
// --------------------------------------------------------------
struct gxSchannelCTX
{
  gxSchannelCTX() {
    memset(&cred, 0, sizeof(cred));
    has_cred = 0;
    is_inbound = 0;
    cert = 0;
    cert_store = 0;
    ca_store = 0;
    key = 0;
  }

  CredHandle cred;           // Schannel credentials handle
  int has_cred;              // True if the credentials handle is set
  int is_inbound;            // True for server credentials
  PCCERT_CONTEXT cert;       // Local certificate, servers or client certs
  HCERTSTORE cert_store;     // Store holding the local cert and its chain
  HCERTSTORE ca_store;       // Trusted CAs loaded from the CA list
  NCRYPT_KEY_HANDLE key;     // Imported private key, deleted with the CTX
};

struct gxSchannelConn
{
  gxSchannelConn() {
    memset(&ctxt, 0, sizeof(ctxt));
    memset(&sizes, 0, sizeof(sizes));
    has_ctxt = 0;
    handshake_done = 0;
    is_accept = 0;
    closed = 0;
    s = 0;
    rx = 0; rx_len = 0; rx_size = 0;
    pt = 0; pt_len = 0; pt_pos = 0; pt_size = 0;
    tx = 0; tx_size = 0;
  }
  ~gxSchannelConn() {
    if(rx) delete[] rx;
    if(pt) delete[] pt;
    if(tx) delete[] tx;
  }

  CtxtHandle ctxt;                  // Schannel security context
  SecPkgContext_StreamSizes sizes;  // TLS record header, trailer and size
  int has_ctxt;                     // True if ctxt is set
  int handshake_done;               // True after the handshake completes
  int is_accept;                    // True for server side connections
  int closed;                       // True after the peer closed TLS
  SOCKET s;                         // Connected socket

  char *rx;     // Encrypted bytes received and not yet decrypted
  int rx_len;
  int rx_size;
  char *pt;     // Decrypted bytes not yet passed to the caller
  int pt_len;
  int pt_pos;
  int pt_size;
  char *tx;     // Buffer used to encrypt outgoing records
  int tx_size;
};

// Status of the last Schannel or Windows crypto call, reported by
// GlobalSSLError(). Like the OpenSSL error queue this is a global value.
static long gxSchannelLastStatus = 0;

// Size of the receive buffer, large enough for one TLS record
const int gxSchannelRxSize = 16384 + 2048;

// --------------------------------------------------------------
// Internal helper functions
// --------------------------------------------------------------
static int gxsc_grow(char *&buf, int &size, int used, int needed)
// Make sure buf can hold needed bytes, keeping the first used bytes.
// Returns 0 if successful or -1 if memory could not be allocated.
{
  if(needed <= size) return 0;
  int new_size = size ? size : 1024;
  while(new_size < needed) new_size *= 2;
  char *p = new char[new_size];
  if(!p) return -1;
  if(buf) {
    if(used > 0) memmove(p, buf, used);
    delete[] buf;
  }
  buf = p;
  size = new_size;
  return 0;
}

static int gxsc_read_file(const char *fname, MemoryBuffer &mbuf)
// Read a whole file into mbuf. Returns 0 if successful.
{
  mbuf.Clear();
  if(!fname) return -1;
  FILE *fp = fopen(fname, "rb");
  if(!fp) return -1;
  char buf[4096];
  size_t n;
  while((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
    mbuf.Cat(buf, (__ULWORD__)n);
  }
  fclose(fp);
  return mbuf.length() > 0 ? 0 : -1;
}

static int gxsc_is_pem(const MemoryBuffer &mbuf)
// Returns true if the buffer holds PEM text
{
  const char *p = (const char *)mbuf.m_buf();
  unsigned len = (unsigned)mbuf.length();
  const char *tag = "-----BEGIN ";
  unsigned tag_len = (unsigned)strlen(tag);
  for(unsigned i = 0; i + tag_len <= len; i++) {
    if(memcmp(p + i, tag, tag_len) == 0) return 1;
  }
  return 0;
}

static int gxsc_pem_next(const MemoryBuffer &mbuf, unsigned &offset,
			 gxString &label, MemoryBuffer &der)
// Find the next PEM block at or after offset. Passes back the label
// ("CERTIFICATE", "PRIVATE KEY", etc.) and the decoded DER bytes, and
// moves offset past the block. Returns 0 if a block was found.
{
  const char *p = (const char *)mbuf.m_buf();
  unsigned len = (unsigned)mbuf.length();
  const char *begin_tag = "-----BEGIN ";
  const char *end_tag = "-----END ";
  unsigned bt_len = (unsigned)strlen(begin_tag);
  unsigned et_len = (unsigned)strlen(end_tag);
  label.Clear();
  der.Clear();

  unsigned i = offset;
  for(; i + bt_len <= len; i++) {
    if(memcmp(p + i, begin_tag, bt_len) == 0) break;
  }
  if(i + bt_len > len) return -1;
  unsigned lbl_start = i + bt_len;
  unsigned lbl_end = lbl_start;
  while(lbl_end < len && p[lbl_end] != '-') lbl_end++;
  if(lbl_end >= len) return -1;
  label.InsertAt(0, p + lbl_start, (lbl_end - lbl_start));
  unsigned body_start = lbl_end;
  while(body_start < len && p[body_start] == '-') body_start++;

  unsigned j = body_start;
  for(; j + et_len <= len; j++) {
    if(memcmp(p + j, end_tag, et_len) == 0) break;
  }
  if(j + et_len > len) return -1;
  unsigned body_end = j;

  // Move the offset past the END line
  unsigned k = j + et_len;
  while(k < len && p[k] != '\n') k++;
  offset = k;

  // Skip RFC 1421 headers such as Proc-Type, which mark legacy encrypted
  // keys. These are not supported.
  for(unsigned h = body_start; h < body_end; h++) {
    if(p[h] == ':') return -2;
  }

  DWORD der_len = 0;
  if(!CryptStringToBinaryA(p + body_start, body_end - body_start,
			   CRYPT_STRING_BASE64, 0, &der_len, 0, 0)) {
    return -1;
  }
  BYTE *dbuf = new BYTE[der_len];
  if(!dbuf) return -1;
  if(!CryptStringToBinaryA(p + body_start, body_end - body_start,
			   CRYPT_STRING_BASE64, dbuf, &der_len, 0, 0)) {
    delete[] dbuf;
    return -1;
  }
  der.Load(dbuf, (__ULWORD__)der_len);
  delete[] dbuf;
  return 0;
}

static void gxsc_der_len(MemoryBuffer &out, unsigned len)
// Append a DER length
{
  unsigned char b[5];
  if(len < 0x80) {
    b[0] = (unsigned char)len;
    out.Cat(b, 1);
  }
  else if(len < 0x100) {
    b[0] = 0x81; b[1] = (unsigned char)len;
    out.Cat(b, 2);
  }
  else if(len < 0x10000) {
    b[0] = 0x82; b[1] = (unsigned char)(len >> 8); b[2] = (unsigned char)len;
    out.Cat(b, 3);
  }
  else {
    b[0] = 0x83; b[1] = (unsigned char)(len >> 16);
    b[2] = (unsigned char)(len >> 8); b[3] = (unsigned char)len;
    out.Cat(b, 4);
  }
}

static void gxsc_der_tlv(MemoryBuffer &out, unsigned char tag,
			 const void *val, unsigned len)
// Append a DER tag, length and value
{
  out.Cat(&tag, 1);
  gxsc_der_len(out, len);
  if(len) out.Cat(val, (__ULWORD__)len);
}

static int gxsc_der_read(const unsigned char *p, unsigned len, unsigned &pos,
			 unsigned char &tag, unsigned &vstart, unsigned &vlen)
// Read the DER tag and length at pos. Passes back the start and length
// of the value and moves pos past the value. Returns 0 if successful.
{
  if(pos + 2 > len) return -1;
  tag = p[pos++];
  unsigned l = p[pos++];
  if(l & 0x80) {
    unsigned n = l & 0x7F;
    if(n == 0 || n > 3 || pos + n > len) return -1;
    l = 0;
    for(unsigned i = 0; i < n; i++) l = (l << 8) | p[pos++];
  }
  if(pos + l > len) return -1;
  vstart = pos;
  vlen = l;
  pos += l;
  return 0;
}

static int gxsc_wrap_pkcs8(const MemoryBuffer &key, int is_ec,
			   MemoryBuffer &pkcs8)
// Wrap a PKCS#1 RSA key or a SEC1 EC key in a PKCS#8 PrivateKeyInfo.
// Returns 0 if successful.
{
  static const unsigned char rsa_alg[] = {
    0x06,0x09,0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x01, // rsaEncryption
    0x05,0x00                                               // NULL
  };
  static const unsigned char ec_oid[] = {
    0x06,0x07,0x2A,0x86,0x48,0xCE,0x3D,0x02,0x01            // ecPublicKey
  };
  MemoryBuffer alg;
  if(is_ec) {
    // ECPrivateKey ::= SEQUENCE { version, privateKey, [0] parameters, ... }
    const unsigned char *p = (const unsigned char *)key.m_buf();
    unsigned len = (unsigned)key.length();
    unsigned pos = 0, vs, vl;
    unsigned char tag;
    if(gxsc_der_read(p, len, pos, tag, vs, vl) != 0 || tag != 0x30) return -1;
    pos = vs;
    unsigned end = vs + vl;
    const unsigned char *curve = 0;
    unsigned curve_len = 0;
    while(pos < end) {
      unsigned tlv_start = pos;
      if(gxsc_der_read(p, end, pos, tag, vs, vl) != 0) return -1;
      (void)tlv_start;
      if(tag == 0xA0) { // [0] parameters holds the named curve OID
	curve = p + vs;
	curve_len = vl;
      }
    }
    if(!curve) return -1; // The curve must be named in the key
    MemoryBuffer seq;
    seq.Cat(ec_oid, sizeof(ec_oid));
    seq.Cat(curve, (__ULWORD__)curve_len);
    gxsc_der_tlv(alg, 0x30, seq.m_buf(), (unsigned)seq.length());
  }
  else {
    gxsc_der_tlv(alg, 0x30, rsa_alg, sizeof(rsa_alg));
  }
  static const unsigned char version[] = { 0x02, 0x01, 0x00 };
  MemoryBuffer body;
  body.Cat(version, sizeof(version));
  body.Cat(alg.m_buf(), alg.length());
  gxsc_der_tlv(body, 0x04, key.m_buf(), (unsigned)key.length());
  pkcs8.Clear();
  gxsc_der_tlv(pkcs8, 0x30, body.m_buf(), (unsigned)body.length());
  return 0;
}

static int gxsc_has_ext(const char *fname, const char *ext)
// Returns true if fname ends with ext, ignoring case
{
  if(!fname || !ext) return 0;
  size_t fl = strlen(fname), el = strlen(ext);
  if(fl < el) return 0;
  return CaseICmp(fname + fl - el, ext) == 0;
}

static wchar_t *gxsc_to_wide(const char *s)
// Returns a new wide string copy of s. Free it with delete[].
{
  if(!s) s = "";
  int n = MultiByteToWideChar(CP_ACP, 0, s, -1, 0, 0);
  if(n <= 0) n = 1;
  wchar_t *w = new wchar_t[n];
  if(!w) return 0;
  if(MultiByteToWideChar(CP_ACP, 0, s, -1, w, n) <= 0) w[0] = 0;
  return w;
}

static void gxsc_free_ctx(gxSchannelCTX *c)
// Free everything held by a context object
{
  if(!c) return;
  if(c->has_cred) FreeCredentialsHandle(&c->cred);
  c->has_cred = 0;
  if(c->cert) CertFreeCertificateContext(c->cert);
  c->cert = 0;
  if(c->key) {
    // The key was imported for this context. NCryptDeleteKey also frees
    // the handle.
    if(NCryptDeleteKey(c->key, 0) != ERROR_SUCCESS) {
      NCryptFreeObject(c->key);
    }
    c->key = 0;
  }
  if(c->cert_store) CertCloseStore(c->cert_store, 0);
  c->cert_store = 0;
  if(c->ca_store) CertCloseStore(c->ca_store, 0);
  c->ca_store = 0;
}

static int gxsc_send_all(SOCKET s, const char *buf, int bytes)
// Send all bytes on the socket. Returns 0 if successful.
{
  while(bytes > 0) {
    int n = send(s, buf, bytes, 0);
    if(n <= 0) return -1;
    buf += n;
    bytes -= n;
  }
  return 0;
}

// --------------------------------------------------------------
// gxSSL Schannel functions
// --------------------------------------------------------------
gxSSL::~gxSSL()
{
  if(ssl) {
    sc_shutdown();
    DestroySSL();
  }
  DestroyCTX();
}

void gxSSL::sc_set_status(long status)
// Record the status of the last Schannel call for GlobalSSLError()
{
  gxSchannelLastStatus = status;
}

int gxSSL::SetAppError(const char *s)
// Function used to set application error strings.
{
  app_err_str << clear << s;
  return ssl_error = gxSSL_APPDEF_ERROR;
}

void gxSSL::GlobalSSLError(gxString &err_str)
// Passes back the status of the last Schannel or Windows crypto call
// in the err_str variable.
{
  char sbuf[512];
  memset(sbuf, 0, sizeof(sbuf));
  DWORD code = (DWORD)gxSchannelLastStatus;
  char num[32];
  sprintf(num, "0x%08lX", (unsigned long)code);
  err_str << clear << "SSL global exception: " << num;
  if(code == 0) {
    err_str << " No errors reported";
    return;
  }
  if(FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
		    0, code, 0, sbuf, sizeof(sbuf)-1, 0) > 0) {
    // Remove the line break at the end of the system message
    size_t n = strlen(sbuf);
    while(n > 0 && (sbuf[n-1] == '\r' || sbuf[n-1] == '\n')) sbuf[--n] = 0;
    err_str << " " << sbuf;
  }
}

const char *gxSSL::SSLExceptionMessage()
// Returns a null terminated string that can
// be use to log or print an SSL exception.
{
  // Check to application defined errors
  if(ssl_error == gxSSL_APPDEF_ERROR) {
    return app_err_str.c_str();
  }

  if((int)ssl_error > (gxsMaxOpenSSLExceptionMessages-1)) {
    ssl_error = gxSSL_INVALID_ERROR_CODE;
  }

  // Find the corresponding message in the exception array
  int error = (int)ssl_error;
  return gxsOpenSSLExceptionMessages[error];
}

int gxSSL::InitSSLibrary()
// Schannel is part of Windows and needs no global init.
// Returns true.
{
  gxSSL::lib_init = 1;
  return 1;
}

int gxSSL::ReleaseSSLLibrary()
// Function used to release the SSL library.
// Returns true after releasing the library.
{
  gxSSL::lib_init = 0;
  return 1;
}

void gxSSL::init_data_members()
// Internal function used to initialized data members.
{
  ctx = 0;
  ssl = 0;
  bytes_moved = bytes_read = 0;
  ssl_proto = gxSSL_TLS;
  cert_type = gxSSL_PEM;
  ssl_error = gxSSL_NO_ERROR;
  verify_cert = 0;
  is_client = is_server = 0;
  clear_strings();
  ResetAppError();
}

void gxSSL::clear_strings()
// Internal function used to clear all string members.
{
  random_file.Clear();
  key_file.Clear();
  ca_list.Clear();
  passwd.Clear();
  cert_file.Clear();
  host_name.Clear();
}

int gxSSL::is_ssl_null()
// Returns true if the SSL object is null.
{
  if(!ssl) {
    ssl_error = gxSSL_SSL_INIT_ERROR;
    return 1;
  }
  return 0;
}

int gxSSL::is_ctx_null()
// Returns true if the SSL context object is null.
{
  if(!ctx) {
    ssl_error = gxSSL_CTX_INIT_ERROR;
    return 1;
  }
  return 0;
}

gxSSL_CTX_t *gxSSL::InitCTX(const char *cert_fname, const char *key_fname)
// Function used to initialize an SSL context object using a certificate
// and key file. Returns a pointer to the context object or a null value
// if an error occurs.
{
  cert_file = cert_fname;
  key_file = key_fname;
  return InitCTX();
}

gxSSL_CTX_t *gxSSL::InitCTX(const char *cert_fname, const char *key_fname,
			    const char *key_passphrase)
// Function used to initialize an SSL context object using a certificate
// and key file containing a pass-phrase protected key. Returns a pointer
// to the context object or a null value if an error occurs.
{
  cert_file = cert_fname;
  key_file = key_fname;
  passwd = key_passphrase;
  return InitCTX();
}

gxSSL_CTX_t *gxSSL::InitCTX()
// Function used to initialize an SSL context object. Loads the
// certificate, key and CA list, and gets the Schannel credentials.
// A context with a certificate and no MakeClient() call is a server
// context. Returns a pointer to the context object or a null value
// if an error occurs.
{
  InitSSLibrary();
  DestroyCTX();

  switch(ssl_proto) { // Datagram TLS is not supported by this class
    case gxSSL_DTLS: case gxSSL_DTLSv1: case gxSSL_DTLSv1_2:
      ssl_error = gxSSL_PROTOCOL_ERROR;
      return 0;
    default:
      break;
  }

  ctx = new gxSchannelCTX;
  if(!ctx) {
    ssl_error = gxSSL_CTX_INIT_ERROR;
    return 0;
  }

  if(!cert_file.is_null()) {
    if(sc_load_cert_and_key() != gxSSL_NO_ERROR) {
      gxSSLError err = ssl_error;
      DestroyCTX();
      ssl_error = err;
      return 0;
    }
  }

  if(!ca_list.is_null()) {
    MemoryBuffer mbuf;
    if(gxsc_read_file(ca_list.c_str(), mbuf) != 0) {
      DestroyCTX();
      ssl_error = gxSSL_CALIST_ERROR;
      return 0;
    }
    ctx->ca_store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, 0);
    if(!ctx->ca_store) {
      sc_set_status((long)GetLastError());
      DestroyCTX();
      ssl_error = gxSSL_CALIST_ERROR;
      return 0;
    }
    int num_certs = 0;
    unsigned offset = 0;
    gxString label;
    MemoryBuffer der;
    if(gxsc_is_pem(mbuf)) {
      while(gxsc_pem_next(mbuf, offset, label, der) == 0) {
	if(label != "CERTIFICATE" && label != "TRUSTED CERTIFICATE") continue;
	if(CertAddEncodedCertificateToStore(ctx->ca_store, X509_ASN_ENCODING,
					    (const BYTE *)der.m_buf(),
					    (DWORD)der.length(),
					    CERT_STORE_ADD_USE_EXISTING, 0)) {
	  num_certs++;
	}
      }
    }
    else if(CertAddEncodedCertificateToStore(ctx->ca_store, X509_ASN_ENCODING,
					     (const BYTE *)mbuf.m_buf(),
					     (DWORD)mbuf.length(),
					     CERT_STORE_ADD_USE_EXISTING, 0)) {
      num_certs++;
    }
    if(num_certs == 0) {
      DestroyCTX();
      ssl_error = gxSSL_CALIST_ERROR;
      return 0;
    }
  }

  int inbound = is_server || (!is_client && ctx->cert != 0);
  if(sc_acquire_cred(inbound) != gxSSL_NO_ERROR) {
    gxSSLError err = ssl_error;
    DestroyCTX();
    ssl_error = err;
    return 0;
  }

  ssl_error = gxSSL_NO_ERROR;
  return ctx;
}

int gxSSL::sc_acquire_cred(int inbound)
// Internal function used to get the Schannel credentials handle.
// Returns 0 if successful.
{
  if(is_ctx_null()) return ssl_error;

  DWORD flags = SCH_USE_STRONG_CRYPTO;
  if(!inbound) {
    // Check the server certificate in VerifyCert(), the same as the
    // OpenSSL version, and do not send a client cert unless one is set
    flags |= SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;
  }
  PCCERT_CONTEXT certs[1];
  certs[0] = ctx->cert;

  // Windows 10 1809 and higher use SCH_CREDENTIALS, which is needed for
  // TLS 1.3
  TLS_PARAMETERS tls_parms;
  memset(&tls_parms, 0, sizeof(tls_parms));
  if(ssl_proto == gxSSL_TLSv1_2) {
    tls_parms.grbitDisabledProtocols = SP_PROT_TLS1_3_CLIENT |
      SP_PROT_TLS1_3_SERVER;
  }
  SCH_CREDENTIALS sch_cred;
  memset(&sch_cred, 0, sizeof(sch_cred));
  sch_cred.dwVersion = SCH_CREDENTIALS_VERSION;
  sch_cred.dwFlags = flags;
  sch_cred.cTlsParameters = 1;
  sch_cred.pTlsParameters = &tls_parms;
  if(ctx->cert) {
    sch_cred.cCreds = 1;
    sch_cred.paCred = certs;
  }

  TimeStamp expiry;
  SECURITY_STATUS st = AcquireCredentialsHandleA(0, (SEC_CHAR *)UNISP_NAME_A,
    inbound ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND, 0, &sch_cred,
    0, 0, &ctx->cred, &expiry);

  if(st != SEC_E_OK) {
    // Older versions of Windows use SCHANNEL_CRED
    SCHANNEL_CRED old_cred;
    memset(&old_cred, 0, sizeof(old_cred));
    old_cred.dwVersion = SCHANNEL_CRED_VERSION;
    old_cred.dwFlags = flags;
    if(ctx->cert) {
      old_cred.cCreds = 1;
      old_cred.paCred = certs;
    }
    st = AcquireCredentialsHandleA(0, (SEC_CHAR *)UNISP_NAME_A,
      inbound ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND, 0, &old_cred,
      0, 0, &ctx->cred, &expiry);
  }

  sc_set_status((long)st);
  if(st != SEC_E_OK) {
    if(st == SEC_E_NO_CREDENTIALS || st == SEC_E_UNKNOWN_CREDENTIALS) {
      return ssl_error = gxSSL_KEY_ERROR;
    }
    return ssl_error = gxSSL_INIT_ERROR;
  }
  ctx->has_cred = 1;
  ctx->is_inbound = inbound;
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::sc_load_pfx(const char *fname)
// Internal function used to load a certificate and private key from a
// PKCS#12 (.pfx or .p12) file. Returns 0 if successful.
{
  MemoryBuffer mbuf;
  if(gxsc_read_file(fname, mbuf) != 0) return ssl_error = gxSSL_CERT_ERROR;
  CRYPT_DATA_BLOB blob;
  blob.cbData = (DWORD)mbuf.length();
  blob.pbData = (BYTE *)mbuf.m_buf();
  wchar_t *wpw = gxsc_to_wide(passwd.c_str());
  if(!wpw) return ssl_error = gxSSL_PASSWD_ERROR;
  ctx->cert_store = PFXImportCertStore(&blob, wpw, CRYPT_USER_KEYSET |
				       PKCS12_ALWAYS_CNG_KSP);
  if(!ctx->cert_store) {
    // Older versions of Windows do not take PKCS12_ALWAYS_CNG_KSP
    ctx->cert_store = PFXImportCertStore(&blob, wpw, CRYPT_USER_KEYSET);
  }
  delete[] wpw;
  if(!ctx->cert_store) {
    sc_set_status((long)GetLastError());
    return ssl_error = gxSSL_CERT_ERROR;
  }

  // Use the first certificate that has a private key
  PCCERT_CONTEXT c = 0;
  while((c = CertEnumCertificatesInStore(ctx->cert_store, c)) != 0) {
    DWORD cb = 0;
    if(CertGetCertificateContextProperty(c, CERT_KEY_PROV_INFO_PROP_ID,
					 0, &cb)) {
      ctx->cert = CertDuplicateCertificateContext(c);
      CertFreeCertificateContext(c);
      break;
    }
  }
  if(!ctx->cert) return ssl_error = gxSSL_KEY_ERROR;

  // Keep the key handle so DestroyCTX() can delete the imported key
  NCRYPT_KEY_HANDLE key = 0;
  DWORD key_spec = 0;
  BOOL must_free = FALSE;
  if(CryptAcquireCertificatePrivateKey(ctx->cert,
				       CRYPT_ACQUIRE_ONLY_NCRYPT_KEY_FLAG,
				       0, &key, &key_spec, &must_free)) {
    if(must_free) ctx->key = key;
  }
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::sc_load_cert_and_key()
// Internal function used to load the certificate and private key files.
// Returns 0 if successful.
{
  if(gxsc_has_ext(cert_file.c_str(), ".pfx") ||
     gxsc_has_ext(cert_file.c_str(), ".p12")) {
    return sc_load_pfx(cert_file.c_str());
  }

  // Load the certificate and any chain certificates that follow it
  MemoryBuffer mbuf;
  if(gxsc_read_file(cert_file.c_str(), mbuf) != 0) {
    return ssl_error = gxSSL_CERT_ERROR;
  }
  ctx->cert_store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, 0);
  if(!ctx->cert_store) {
    sc_set_status((long)GetLastError());
    return ssl_error = gxSSL_CERT_ERROR;
  }
  if(gxsc_is_pem(mbuf)) {
    unsigned offset = 0;
    gxString label;
    MemoryBuffer der;
    while(gxsc_pem_next(mbuf, offset, label, der) == 0) {
      if(label != "CERTIFICATE") continue;
      PCCERT_CONTEXT c = 0;
      if(!CertAddEncodedCertificateToStore(ctx->cert_store, X509_ASN_ENCODING,
					   (const BYTE *)der.m_buf(),
					   (DWORD)der.length(),
					   CERT_STORE_ADD_USE_EXISTING,
					   ctx->cert ? 0 : &c)) {
	sc_set_status((long)GetLastError());
	return ssl_error = gxSSL_CERT_ERROR;
      }
      if(!ctx->cert) ctx->cert = c;
    }
  }
  else {
    if(!CertAddEncodedCertificateToStore(ctx->cert_store, X509_ASN_ENCODING,
					 (const BYTE *)mbuf.m_buf(),
					 (DWORD)mbuf.length(),
					 CERT_STORE_ADD_USE_EXISTING,
					 &ctx->cert)) {
      sc_set_status((long)GetLastError());
      return ssl_error = gxSSL_CERT_ERROR;
    }
  }
  if(!ctx->cert) return ssl_error = gxSSL_CERT_ERROR;

  // A client certificate or server certificate needs its private key
  if(key_file.is_null()) return ssl_error = gxSSL_KEY_ERROR;

  // Read the private key and convert it to PKCS#8
  if(gxsc_read_file(key_file.c_str(), mbuf) != 0) {
    return ssl_error = gxSSL_KEY_ERROR;
  }
  MemoryBuffer pkcs8;
  int encrypted = 0;
  if(gxsc_is_pem(mbuf)) {
    unsigned offset = 0;
    gxString label;
    MemoryBuffer der;
    int found = 0;
    int rv;
    while((rv = gxsc_pem_next(mbuf, offset, label, der)) == 0) {
      if(label == "PRIVATE KEY") {
	pkcs8 = der; found = 1; break;
      }
      if(label == "ENCRYPTED PRIVATE KEY") {
	pkcs8 = der; encrypted = 1; found = 1; break;
      }
      if(label == "RSA PRIVATE KEY") {
	if(gxsc_wrap_pkcs8(der, 0, pkcs8) == 0) found = 1;
	break;
      }
      if(label == "EC PRIVATE KEY") {
	if(gxsc_wrap_pkcs8(der, 1, pkcs8) == 0) found = 1;
	break;
      }
    }
    if(rv == -2) {
      return SetAppError("SSL exception: Legacy encrypted PEM keys are not \
supported, convert the key to PKCS#8 with openssl pkcs8 -topk8");
    }
    if(!found) return ssl_error = gxSSL_KEY_ERROR;
  }
  else { // DER key, PKCS#8 or PKCS#1 RSA
    const unsigned char *p = (const unsigned char *)mbuf.m_buf();
    unsigned len = (unsigned)mbuf.length();
    unsigned pos = 0, vs, vl;
    unsigned char tag;
    // A PKCS#8 key has a SEQUENCE after the version, a PKCS#1 key an INTEGER
    if(gxsc_der_read(p, len, pos, tag, vs, vl) != 0 || tag != 0x30) {
      return ssl_error = gxSSL_KEY_ERROR;
    }
    pos = vs;
    if(gxsc_der_read(p, len, pos, tag, vs, vl) != 0 ||
       gxsc_der_read(p, len, pos, tag, vs, vl) != 0) {
      return ssl_error = gxSSL_KEY_ERROR;
    }
    if(tag == 0x30) {
      pkcs8 = mbuf;
    }
    else if(gxsc_wrap_pkcs8(mbuf, 0, pkcs8) != 0) {
      return ssl_error = gxSSL_KEY_ERROR;
    }
  }

  // Import the key into the current user's key store. Schannel needs a
  // stored key, so the key is given a name made from the certificate
  // thumbprint and deleted again by DestroyCTX().
  BYTE thumb[20];
  DWORD thumb_len = sizeof(thumb);
  if(!CertGetCertificateContextProperty(ctx->cert, CERT_SHA1_HASH_PROP_ID,
					thumb, &thumb_len)) {
    sc_set_status((long)GetLastError());
    return ssl_error = gxSSL_CERT_ERROR;
  }
  wchar_t key_name[64];
  wcscpy(key_name, L"DataReel-gxSSL-");
  for(DWORD i = 0; i < thumb_len; i++) {
    wchar_t hex[3];
    swprintf(hex, 3, L"%02X", (unsigned)thumb[i]);
    wcscat(key_name, hex);
  }

  NCRYPT_PROV_HANDLE prov = 0;
  SECURITY_STATUS st = NCryptOpenStorageProvider(&prov,
						 MS_KEY_STORAGE_PROVIDER, 0);
  if(st != ERROR_SUCCESS) {
    sc_set_status((long)st);
    return ssl_error = gxSSL_KEY_ERROR;
  }
  wchar_t *wpw = 0;
  NCryptBuffer nbufs[2];
  int num_bufs = 0;
  nbufs[num_bufs].BufferType = NCRYPTBUFFER_PKCS_KEY_NAME;
  nbufs[num_bufs].cbBuffer = (ULONG)((wcslen(key_name)+1) * sizeof(wchar_t));
  nbufs[num_bufs].pvBuffer = key_name;
  num_bufs++;
  if(encrypted) {
    wpw = gxsc_to_wide(passwd.c_str());
    if(!wpw) {
      NCryptFreeObject(prov);
      return ssl_error = gxSSL_PASSWD_ERROR;
    }
    nbufs[num_bufs].BufferType = NCRYPTBUFFER_PKCS_SECRET;
    nbufs[num_bufs].cbBuffer = (ULONG)((wcslen(wpw)+1) * sizeof(wchar_t));
    nbufs[num_bufs].pvBuffer = wpw;
    num_bufs++;
  }
  NCryptBufferDesc ndesc;
  ndesc.ulVersion = NCRYPTBUFFER_VERSION;
  ndesc.cBuffers = num_bufs;
  ndesc.pBuffers = nbufs;

  NCRYPT_KEY_HANDLE key = 0;
  st = NCryptImportKey(prov, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, &ndesc, &key,
		       (PBYTE)pkcs8.m_buf(), (DWORD)pkcs8.length(),
		       NCRYPT_OVERWRITE_KEY_FLAG | NCRYPT_DO_NOT_FINALIZE_FLAG);
  if(wpw) {
    SecureZeroMemory(wpw, wcslen(wpw) * sizeof(wchar_t));
    delete[] wpw;
  }
  if(st == ERROR_SUCCESS) {
    st = NCryptFinalizeKey(key, 0);
    if(st != ERROR_SUCCESS) {
      NCryptDeleteKey(key, 0);
      key = 0;
    }
  }
  NCryptFreeObject(prov);
  SecureZeroMemory(pkcs8.m_buf(), pkcs8.length());
  if(st != ERROR_SUCCESS) {
    sc_set_status((long)st);
    return ssl_error = encrypted ? gxSSL_PASSWD_ERROR : gxSSL_KEY_ERROR;
  }
  ctx->key = key;

  // Link the certificate to the stored key
  CRYPT_KEY_PROV_INFO prov_info;
  memset(&prov_info, 0, sizeof(prov_info));
  prov_info.pwszContainerName = key_name;
  prov_info.pwszProvName = (LPWSTR)MS_KEY_STORAGE_PROVIDER;
  prov_info.dwProvType = 0;
  prov_info.dwKeySpec = 0;
  if(!CertSetCertificateContextProperty(ctx->cert, CERT_KEY_PROV_INFO_PROP_ID,
					0, &prov_info)) {
    sc_set_status((long)GetLastError());
    return ssl_error = gxSSL_KEY_ERROR;
  }
  return ssl_error = gxSSL_NO_ERROR;
}

void gxSSL::DestroyCTX()
// Destroys the context object.
{
  if(ctx) {
    gxsc_free_ctx(ctx);
    delete ctx;
  }
  ctx = 0;
}

void gxSSL::DestroySSL()
// Destroys the SSL object.
{
  if(ssl) {
    if(ssl->has_ctxt) DeleteSecurityContext(&ssl->ctxt);
    delete ssl;
  }
  ssl = 0;
}

int gxSSL::OpenSSLSocket(gxsSocket_t s)
// Function used to open an SSL socket.
// Returns 0 if successful.
// Returns a non-zero value if the function fails.
{
  if(!ctx) {
    if(!InitCTX()) return ssl_error;
  }
  DestroySSL();
  ssl = new gxSchannelConn;
  if(is_ssl_null()) return ssl_error;
  ssl->s = (SOCKET)s;
  if(gxsc_grow(ssl->rx, ssl->rx_size, 0, gxSchannelRxSize) != 0) {
    DestroySSL();
    return ssl_error = gxSSL_BUFOVER_ERROR;
  }
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::sc_send_token(SecBuffer &token)
// Internal function used to send a handshake token allocated by
// Schannel and free it. Returns 0 if successful.
{
  int rv = 0;
  if(token.pvBuffer) {
    if(token.cbBuffer > 0) {
      rv = gxsc_send_all(ssl->s, (const char *)token.pvBuffer,
			 (int)token.cbBuffer);
    }
    FreeContextBuffer(token.pvBuffer);
    token.pvBuffer = 0;
    token.cbBuffer = 0;
  }
  return rv;
}

int gxSSL::sc_recv_raw(int use_timeout, int seconds, int useconds)
// Internal function used to receive more encrypted bytes into the
// receive buffer. Returns the number of bytes received, 0 if the peer
// closed the socket, -1 if an error occurs, or -2 on a timeout.
{
  if(ssl->rx_len >= ssl->rx_size) {
    if(gxsc_grow(ssl->rx, ssl->rx_size, ssl->rx_len, ssl->rx_size*2) != 0) {
      ssl_error = gxSSL_BUFOVER_ERROR;
      return -1;
    }
  }
  if(use_timeout) {
    gxSocket gxsocket_object;
    if(!gxsocket_object.ReadSelect((gxsSocket_t)ssl->s, seconds, useconds)) {
      ssl_error = gxSSL_REQUEST_TIMEOUT;
      return -2;
    }
  }
  int n = recv(ssl->s, ssl->rx + ssl->rx_len, ssl->rx_size - ssl->rx_len, 0);
  if(n < 0) {
    sc_set_status((long)WSAGetLastError());
    ssl_error = gxSSL_RECEIVE_ERROR;
    return -1;
  }
  if(n == 0) {
    ssl_error = gxSSL_DISCONNECTED_ERROR;
    return 0;
  }
  ssl->rx_len += n;
  return n;
}

int gxSSL::sc_handshake(int is_accept, int have_input)
// Internal function used to run the TLS handshake, or the handshake
// messages that follow a SEC_I_RENEGOTIATE status. If have_input is true
// the receive buffer already holds handshake bytes. Returns 0 if
// successful.
{
  if(is_ctx_null()) return ssl_error;
  if(is_ssl_null()) return ssl_error;

  DWORD isc_flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT |
    ISC_REQ_CONFIDENTIALITY | ISC_REQ_EXTENDED_ERROR |
    ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_MANUAL_CRED_VALIDATION;
  DWORD asc_flags = ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT |
    ASC_REQ_CONFIDENTIALITY | ASC_REQ_EXTENDED_ERROR |
    ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM;
  DWORD out_flags = 0;
  TimeStamp expiry;
  SEC_CHAR *target = host_name.is_null() ? 0 : (SEC_CHAR *)host_name.c_str();
  SECURITY_STATUS st;
  SecBuffer in_bufs[2], out_bufs[1];
  SecBufferDesc in_desc, out_desc;
  in_desc.ulVersion = out_desc.ulVersion = SECBUFFER_VERSION;
  in_desc.cBuffers = 2; in_desc.pBuffers = in_bufs;
  out_desc.cBuffers = 1; out_desc.pBuffers = out_bufs;

  if(!is_accept && !ssl->has_ctxt) {
    // The client sends the first handshake message
    out_bufs[0].BufferType = SECBUFFER_TOKEN;
    out_bufs[0].cbBuffer = 0;
    out_bufs[0].pvBuffer = 0;
    st = InitializeSecurityContextA(&ctx->cred, 0, target, isc_flags, 0, 0,
				    0, 0, &ssl->ctxt, &out_desc, &out_flags,
				    &expiry);
    sc_set_status((long)st);
    if(st != SEC_I_CONTINUE_NEEDED) {
      if(out_bufs[0].pvBuffer) FreeContextBuffer(out_bufs[0].pvBuffer);
      return ssl_error = gxSSL_CONNECT_ERROR;
    }
    ssl->has_ctxt = 1;
    if(sc_send_token(out_bufs[0]) != 0) {
      return ssl_error = gxSSL_TRANSMIT_ERROR;
    }
    have_input = 0;
  }

  int need_read = !have_input || ssl->rx_len == 0;
  while(1) {
    if(need_read) {
      int n = sc_recv_raw(0, 0, 0);
      if(n <= 0) {
	if(n == 0) ssl_error = gxSSL_DISCONNECTED_ERROR;
	return ssl_error;
      }
    }
    need_read = 1;

    in_bufs[0].BufferType = SECBUFFER_TOKEN;
    in_bufs[0].cbBuffer = (unsigned long)ssl->rx_len;
    in_bufs[0].pvBuffer = ssl->rx;
    in_bufs[1].BufferType = SECBUFFER_EMPTY;
    in_bufs[1].cbBuffer = 0;
    in_bufs[1].pvBuffer = 0;
    out_bufs[0].BufferType = SECBUFFER_TOKEN;
    out_bufs[0].cbBuffer = 0;
    out_bufs[0].pvBuffer = 0;

    if(is_accept) {
      st = AcceptSecurityContext(&ctx->cred,
				 ssl->has_ctxt ? &ssl->ctxt : 0,
				 &in_desc, asc_flags, 0, &ssl->ctxt,
				 &out_desc, &out_flags, &expiry);
      if(st == SEC_E_OK || st == SEC_I_CONTINUE_NEEDED ||
	 (FAILED(st) && (out_flags & ASC_RET_EXTENDED_ERROR))) {
	ssl->has_ctxt = 1;
      }
    }
    else {
      st = InitializeSecurityContextA(&ctx->cred, &ssl->ctxt, target,
				      isc_flags, 0, 0, &in_desc, 0, 0,
				      &out_desc, &out_flags, &expiry);
    }
    sc_set_status((long)st);

    if(st == SEC_E_INCOMPLETE_MESSAGE) {
      // Read the rest of the handshake message
      if(out_bufs[0].pvBuffer) FreeContextBuffer(out_bufs[0].pvBuffer);
      continue;
    }

    // Send any reply, including the alert sent with an error
    if(sc_send_token(out_bufs[0]) != 0 && !FAILED(st)) {
      return ssl_error = gxSSL_TRANSMIT_ERROR;
    }

    // Keep the bytes that belong to the next message
    if(in_bufs[1].BufferType == SECBUFFER_EXTRA && in_bufs[1].cbBuffer > 0) {
      int extra = (int)in_bufs[1].cbBuffer;
      memmove(ssl->rx, ssl->rx + (ssl->rx_len - extra), extra);
      ssl->rx_len = extra;
      need_read = 0;
    }
    else if(st != SEC_I_INCOMPLETE_CREDENTIALS) {
      ssl->rx_len = 0;
    }

    if(st == SEC_E_OK) break;
    if(st == SEC_I_CONTINUE_NEEDED) continue;
    if(st == SEC_I_INCOMPLETE_CREDENTIALS) {
      // The server asked for a client certificate and none is set.
      // Continue without one.
      need_read = 0;
      continue;
    }
    if(st == SEC_E_WRONG_PRINCIPAL || st == SEC_E_CERT_UNKNOWN ||
       st == SEC_E_UNTRUSTED_ROOT || st == SEC_E_CERT_EXPIRED) {
      return ssl_error = gxSSL_CERT_VER_ERROR;
    }
    return ssl_error = is_accept ? gxSSL_ACCEPT_ERROR : gxSSL_CONNECT_ERROR;
  }

  st = QueryContextAttributesA(&ssl->ctxt, SECPKG_ATTR_STREAM_SIZES,
			       &ssl->sizes);
  sc_set_status((long)st);
  if(st != SEC_E_OK) return ssl_error = gxSSL_LIB_ERROR;
  int tx_needed = (int)(ssl->sizes.cbHeader + ssl->sizes.cbMaximumMessage +
			ssl->sizes.cbTrailer);
  if(gxsc_grow(ssl->tx, ssl->tx_size, 0, tx_needed) != 0) {
    return ssl_error = gxSSL_BUFOVER_ERROR;
  }
  if(gxsc_grow(ssl->rx, ssl->rx_size, ssl->rx_len, tx_needed) != 0) {
    return ssl_error = gxSSL_BUFOVER_ERROR;
  }
  ssl->handshake_done = 1;
  ssl->is_accept = is_accept;
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::ConnectSSLSocket()
// Function used to connect an SSL socket.
// Returns 0 if successful.
// Returns a non-zero value if the function fails.
{
  if(is_ssl_null()) return ssl_error;
  if(is_ctx_null()) return ssl_error;
  if(ctx->is_inbound) return ssl_error = gxSSL_CONNECT_ERROR;
  return sc_handshake(0, 0);
}

int gxSSL::AcceptSSLSocket()
// Function used to accept SSL client connections.
// Returns 0 if successful.
// Returns a non-zero value if the function fails.
{
  if(is_ssl_null()) return ssl_error;
  if(is_ctx_null()) return ssl_error;
  if(!ctx->is_inbound) return ssl_error = gxSSL_ACCEPT_ERROR;
  if(sc_handshake(1, 0) != gxSSL_NO_ERROR) {
    return ssl_error = gxSSL_ACCEPT_ERROR;
  }
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::sc_shutdown()
// Internal function used to send the TLS close notify message.
// Returns 0 if successful.
{
  if(!ssl || !ctx || !ssl->has_ctxt || !ssl->handshake_done) {
    return ssl_error = gxSSL_SHUTDOWN_ERROR;
  }
  DWORD type = SCHANNEL_SHUTDOWN;
  SecBuffer buf;
  SecBufferDesc desc;
  buf.BufferType = SECBUFFER_TOKEN;
  buf.cbBuffer = sizeof(type);
  buf.pvBuffer = &type;
  desc.ulVersion = SECBUFFER_VERSION;
  desc.cBuffers = 1;
  desc.pBuffers = &buf;
  SECURITY_STATUS st = ApplyControlToken(&ssl->ctxt, &desc);
  sc_set_status((long)st);
  if(st != SEC_E_OK) return ssl_error = gxSSL_SHUTDOWN_ERROR;

  SecBuffer out_buf;
  SecBufferDesc out_desc;
  out_buf.BufferType = SECBUFFER_TOKEN;
  out_buf.cbBuffer = 0;
  out_buf.pvBuffer = 0;
  out_desc.ulVersion = SECBUFFER_VERSION;
  out_desc.cBuffers = 1;
  out_desc.pBuffers = &out_buf;
  DWORD out_flags = 0;
  TimeStamp expiry;
  if(ssl->is_accept) {
    st = AcceptSecurityContext(&ctx->cred, &ssl->ctxt, 0,
			       ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT |
			       ASC_REQ_CONFIDENTIALITY | ASC_REQ_EXTENDED_ERROR |
			       ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM, 0,
			       &ssl->ctxt, &out_desc, &out_flags, &expiry);
  }
  else {
    SEC_CHAR *target = host_name.is_null() ? 0 :
      (SEC_CHAR *)host_name.c_str();
    st = InitializeSecurityContextA(&ctx->cred, &ssl->ctxt, target,
				    ISC_REQ_SEQUENCE_DETECT |
				    ISC_REQ_REPLAY_DETECT |
				    ISC_REQ_CONFIDENTIALITY |
				    ISC_REQ_EXTENDED_ERROR |
				    ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM,
				    0, 0, 0, 0, &ssl->ctxt, &out_desc,
				    &out_flags, &expiry);
  }
  sc_set_status((long)st);
  int rv = sc_send_token(out_buf);
  ssl->handshake_done = 0;
  if(FAILED(st) || rv != 0) return ssl_error = gxSSL_SHUTDOWN_ERROR;
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::CloseSSLSocket(int destroy_ssl)
// Function used to close an SSL socket connection.
// Returns 0 if successful.
// Returns a non-zero value if the function fails.
{
  if(!ssl) return ssl_error;
  int rv = sc_shutdown();
  if(destroy_ssl) DestroySSL();
  if(rv != gxSSL_NO_ERROR) return ssl_error = gxSSL_SHUTDOWN_ERROR;
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::sc_write_all(const void *buf, int bytes)
// Internal function used to encrypt and send all the bytes in buf.
// Returns the number of bytes sent or -1 if an error occurs.
{
  if(is_ssl_null()) return -1;
  if(!ssl->handshake_done) {
    ssl_error = ssl->closed ? gxSSL_CLOSE_ERROR : gxSSL_SSL_INIT_ERROR;
    return -1;
  }
  const char *p = (const char *)buf;
  int sent = 0;
  int header = (int)ssl->sizes.cbHeader;
  int max_msg = (int)ssl->sizes.cbMaximumMessage;
  while(sent < bytes) {
    int chunk = bytes - sent;
    if(chunk > max_msg) chunk = max_msg;
    memmove(ssl->tx + header, p + sent, chunk);
    SecBuffer bufs[4];
    SecBufferDesc desc;
    bufs[0].BufferType = SECBUFFER_STREAM_HEADER;
    bufs[0].cbBuffer = ssl->sizes.cbHeader;
    bufs[0].pvBuffer = ssl->tx;
    bufs[1].BufferType = SECBUFFER_DATA;
    bufs[1].cbBuffer = (unsigned long)chunk;
    bufs[1].pvBuffer = ssl->tx + header;
    bufs[2].BufferType = SECBUFFER_STREAM_TRAILER;
    bufs[2].cbBuffer = ssl->sizes.cbTrailer;
    bufs[2].pvBuffer = ssl->tx + header + chunk;
    bufs[3].BufferType = SECBUFFER_EMPTY;
    bufs[3].cbBuffer = 0;
    bufs[3].pvBuffer = 0;
    desc.ulVersion = SECBUFFER_VERSION;
    desc.cBuffers = 4;
    desc.pBuffers = bufs;
    SECURITY_STATUS st = EncryptMessage(&ssl->ctxt, 0, &desc, 0);
    if(st != SEC_E_OK) {
      sc_set_status((long)st);
      ssl_error = gxSSL_WRITE_ERROR;
      return -1;
    }
    int total = (int)(bufs[0].cbBuffer + bufs[1].cbBuffer + bufs[2].cbBuffer);
    if(gxsc_send_all(ssl->s, ssl->tx, total) != 0) {
      sc_set_status((long)WSAGetLastError());
      ssl_error = gxSSL_DISCONNECTED_ERROR;
      return -1;
    }
    sent += chunk;
  }
  ssl_error = gxSSL_NO_ERROR;
  return sent;
}

int gxSSL::sc_read_some(void *buf, int bytes, int use_timeout, int seconds,
			int useconds)
// Internal function used to return up to bytes of decrypted data.
// Returns the number of bytes read, 0 if the peer closed the
// connection, -1 if an error occurs, or -2 on a timeout.
{
  if(is_ssl_null()) return -1;
  if(bytes <= 0) return 0;

  while(1) {
    // Return the decrypted data left from the last record first
    if(ssl->pt_pos < ssl->pt_len) {
      int n = ssl->pt_len - ssl->pt_pos;
      if(n > bytes) n = bytes;
      memmove(buf, ssl->pt + ssl->pt_pos, n);
      ssl->pt_pos += n;
      ssl_error = gxSSL_NO_ERROR;
      return n;
    }
    if(ssl->closed) {
      ssl_error = gxSSL_CLOSE_ERROR;
      return 0;
    }
    if(!ssl->handshake_done) {
      ssl_error = gxSSL_SSL_INIT_ERROR;
      return -1;
    }

    if(ssl->rx_len > 0) {
      SecBuffer bufs[4];
      SecBufferDesc desc;
      bufs[0].BufferType = SECBUFFER_DATA;
      bufs[0].cbBuffer = (unsigned long)ssl->rx_len;
      bufs[0].pvBuffer = ssl->rx;
      for(int i = 1; i < 4; i++) {
	bufs[i].BufferType = SECBUFFER_EMPTY;
	bufs[i].cbBuffer = 0;
	bufs[i].pvBuffer = 0;
      }
      desc.ulVersion = SECBUFFER_VERSION;
      desc.cBuffers = 4;
      desc.pBuffers = bufs;
      SECURITY_STATUS st = DecryptMessage(&ssl->ctxt, &desc, 0, 0);

      if(st == SEC_E_OK || st == SEC_I_RENEGOTIATE ||
	 st == SEC_I_CONTEXT_EXPIRED) {
	SecBuffer *data = 0, *extra = 0;
	for(int i = 1; i < 4; i++) {
	  if(!data && bufs[i].BufferType == SECBUFFER_DATA) data = &bufs[i];
	  if(!extra && bufs[i].BufferType == SECBUFFER_EXTRA) extra = &bufs[i];
	}
	if(data && data->cbBuffer > 0) {
	  if(gxsc_grow(ssl->pt, ssl->pt_size, 0, (int)data->cbBuffer) != 0) {
	    ssl_error = gxSSL_BUFOVER_ERROR;
	    return -1;
	  }
	  memmove(ssl->pt, data->pvBuffer, data->cbBuffer);
	  ssl->pt_len = (int)data->cbBuffer;
	  ssl->pt_pos = 0;
	}
	if(extra && extra->cbBuffer > 0) {
	  int n = (int)extra->cbBuffer;
	  memmove(ssl->rx, ssl->rx + (ssl->rx_len - n), n);
	  ssl->rx_len = n;
	}
	else {
	  ssl->rx_len = 0;
	}
	if(st == SEC_I_CONTEXT_EXPIRED) { // The peer sent close notify
	  ssl->closed = 1;
	  ssl->handshake_done = 0;
	  continue;
	}
	if(st == SEC_I_RENEGOTIATE) {
	  // TLS 1.3 sends handshake messages, such as session tickets,
	  // after the handshake. Pass them back to Schannel.
	  ssl->handshake_done = 0;
	  if(sc_handshake(ssl->is_accept, ssl->rx_len > 0) !=
	     gxSSL_NO_ERROR) {
	    return -1;
	  }
	}
	continue;
      }
      if(st != SEC_E_INCOMPLETE_MESSAGE) {
	sc_set_status((long)st);
	ssl_error = gxSSL_READ_ERROR;
	return -1;
      }
    }

    // Read more encrypted bytes
    int n = sc_recv_raw(use_timeout, seconds, useconds);
    if(n == 0) {
      // The socket closed without a close notify message
      ssl->closed = 1;
      ssl_error = gxSSL_DISCONNECTED_ERROR;
      return 0;
    }
    if(n < 0) return n;
  }
}

int gxSSL::RawWriteSSLSocket(const void *buf, int bytes)
// Send a block of data to the bound socket.
// Returns the total number of bytes sent or -1 if an error occurs.
{
  bytes_moved = 0;
  int rv = sc_write_all(buf, bytes);
  if(rv > 0) bytes_moved = rv;
  return rv;
}

int gxSSL::RawReadSSLSocket(void *buf, int bytes)
// Receive a block of data from a specified socket. NOTE: A
// raw read may return before all the bytes have been read.
// Returns the total number of bytes received, 0 if the connection
// has closed, or -1 if an error occurs.
{
  bytes_read = 0; // Reset the byte counter
  int rv = sc_read_some(buf, bytes, 0, 0, 0);
  if(rv > 0) bytes_read = rv;
  if(rv < 0) return -1;
  return rv;
}

int gxSSL::SendSSLSocket(const void *buf, int bytes)
// Send a block of data to the bound socket and do not return
// until all the bytes have been written. Returns the total number
// of bytes sent or -1 if an error occurs.
{
  bytes_moved = 0; // Reset the byte counter
  int rv = sc_write_all(buf, bytes);
  if(rv > 0) bytes_moved = rv;
  return rv;
}

int gxSSL::RecvSSLSocket(void *buf, int bytes)
// Receive a block of data from a specified socket and do not return
// until all the bytes have been read. Returns the total number of
// bytes received or -1 if an error occurs.
{
  return RecvSSLSocket(buf, bytes, 0, 0, 0);
}

int gxSSL::RecvSSLSocket(void *buf, int bytes, int seconds, int useconds)
// Receive a block of data from a specified socket and do not return
// until all the bytes have been read or the timeout value has been
// exceeded. Returns the total number of bytes received or -1 if an
// error occurs.
{
  return RecvSSLSocket(buf, bytes, 1, seconds, useconds);
}

int gxSSL::RecvSSLSocket(void *buf, int bytes, int useTimeout, int seconds,
			 int useconds)
// Receive a block of data from a specified socket and do not return
// until all the bytes have been read. Returns the total number of
// bytes received or -1 if an error occurs and 0 bytes read.
{
  bytes_read = 0; // Reset the byte counter
  if(is_ssl_null()) return -1;
  char *p = (char *)buf;
  while(bytes_read < bytes) {
    int n = sc_read_some(p + bytes_read, bytes - bytes_read, useTimeout,
			 seconds, useconds);
    if(n > 0) {
      bytes_read += n;
      continue;
    }
    if(n == 0 || n == -2) { // Closed or timed out
      if(bytes_read > 0) return bytes_read;
      return n == 0 ? 0 : -1;
    }
    return -1;
  }
  ssl_error = gxSSL_NO_ERROR;
  return bytes_read;
}

int gxSSL::VerifyCert(const char *hostname, long *X509_return_code)
// Function used to verify a certificate from the specified hostname.
// Checks the certificate chain against the CA list set by SetCAList(),
// or the Windows trusted root certificates if no CA list is set, and
// checks that the certificate is for hostname. The Windows CERT_E_*
// code is passed back in X509_return_code, 0 if the certificate is
// valid. Returns 0 if successful.
// Returns a non-zero value if the function fails.
{
  if(X509_return_code) *X509_return_code = 0;
  if(!hostname) return ssl_error = gxSSL_NULL_PTR_ERROR;
  if(is_ssl_null()) return ssl_error;
  if(!ssl->has_ctxt) return ssl_error = gxSSL_SSL_INIT_ERROR;

  PCCERT_CONTEXT cert = 0;
  SECURITY_STATUS st = QueryContextAttributesA(&ssl->ctxt,
					       SECPKG_ATTR_REMOTE_CERT_CONTEXT,
					       &cert);
  sc_set_status((long)st);
  if(st != SEC_E_OK || !cert) return ssl_error = gxSSL_PEERCERT_ERROR;

  // Build the chain with the certificates the server sent and the CA
  // list, if one was loaded
  HCERTSTORE extra_store = cert->hCertStore;
  HCERTSTORE collection = 0;
  if(ctx && ctx->ca_store) {
    collection = CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, 0, 0, 0);
    if(collection) {
      CertAddStoreToCollection(collection, cert->hCertStore, 0, 0);
      CertAddStoreToCollection(collection, ctx->ca_store, 0, 0);
      extra_store = collection;
    }
  }

  CERT_CHAIN_PARA chain_para;
  memset(&chain_para, 0, sizeof(chain_para));
  chain_para.cbSize = sizeof(chain_para);
  PCCERT_CHAIN_CONTEXT chain = 0;
  long code = 0;
  if(!CertGetCertificateChain(0, cert, 0, extra_store, &chain_para, 0, 0,
			      &chain)) {
    code = (long)GetLastError();
  }
  else {
    DWORD policy_flags = 0;
    if(ctx && ctx->ca_store) {
      // With a CA list, the same as the OpenSSL version, only the CAs in
      // the list are trusted. The root of the chain must be in the list.
      code = (long)CERT_E_UNTRUSTEDROOT;
      if(chain->cChain > 0 && chain->rgpChain[0]->cElement > 0) {
	PCERT_SIMPLE_CHAIN simple = chain->rgpChain[0];
	PCCERT_CONTEXT root = 
	  simple->rgpElement[simple->cElement-1]->pCertContext;
	PCCERT_CONTEXT found = CertFindCertificateInStore(ctx->ca_store,
          X509_ASN_ENCODING, 0, CERT_FIND_EXISTING, root, 0);
	if(found) {
	  CertFreeCertificateContext(found);
	  code = 0;
	  policy_flags = CERT_CHAIN_POLICY_ALLOW_UNKNOWN_CA_FLAG;
	}
      }
    }
    if(code == 0) {
      wchar_t *whost = gxsc_to_wide(hostname);
      SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl_para;
      memset(&ssl_para, 0, sizeof(ssl_para));
      ssl_para.cbSize = sizeof(ssl_para);
      ssl_para.dwAuthType = AUTHTYPE_SERVER;
      ssl_para.pwszServerName = whost;
      CERT_CHAIN_POLICY_PARA policy_para;
      memset(&policy_para, 0, sizeof(policy_para));
      policy_para.cbSize = sizeof(policy_para);
      policy_para.dwFlags = policy_flags;
      policy_para.pvExtraPolicyPara = &ssl_para;
      CERT_CHAIN_POLICY_STATUS status;
      memset(&status, 0, sizeof(status));
      status.cbSize = sizeof(status);
      if(!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chain,
					   &policy_para, &status)) {
	code = (long)GetLastError();
      }
      else {
	code = (long)status.dwError;
      }
      if(whost) delete[] whost;
    }
    CertFreeCertificateChain(chain);
  }
  if(collection) CertCloseStore(collection, 0);
  CertFreeCertificateContext(cert);

  if(X509_return_code) *X509_return_code = code;
  if(code != 0) {
    sc_set_status(code);
    if((DWORD)code == (DWORD)CERT_E_CN_NO_MATCH) {
      return ssl_error = gxSSL_PEERCN_ERROR;
    }
    return ssl_error = gxSSL_CERT_VER_ERROR;
  }
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::GetPeerCertInfo(gxString &subject, gxString &issuer)
// Passes back the subject and issuer names of the peer's
// certificate. Returns 0 if successful or a non-zero value if the peer
// did not present a certificate.
{
  subject.Clear(); issuer.Clear();
  if(is_ssl_null()) return ssl_error;
  if(!ssl->has_ctxt) return ssl_error = gxSSL_SSL_INIT_ERROR;
  PCCERT_CONTEXT cert = 0;
  SECURITY_STATUS st = QueryContextAttributesA(&ssl->ctxt,
					       SECPKG_ATTR_REMOTE_CERT_CONTEXT,
					       &cert);
  sc_set_status((long)st);
  if(st != SEC_E_OK || !cert) return ssl_error = gxSSL_PEERCERT_ERROR;
  char buf[4096];
  memset(buf, 0, sizeof(buf));
  CertNameToStrA(X509_ASN_ENCODING, &cert->pCertInfo->Subject,
		 CERT_X500_NAME_STR, buf, sizeof(buf));
  subject = buf;
  memset(buf, 0, sizeof(buf));
  CertNameToStrA(X509_ASN_ENCODING, &cert->pCertInfo->Issuer,
		 CERT_X500_NAME_STR, buf, sizeof(buf));
  issuer = buf;
  CertFreeCertificateContext(cert);
  return ssl_error = gxSSL_NO_ERROR;
}

const char *gxSSL::CertVerifyErrorString(long X509_return_code)
// Returns a string for a VerifyCert() return code
{
  static char sbuf[512];
  if(X509_return_code == 0) return "ok";
  memset(sbuf, 0, sizeof(sbuf));
  if(FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
		    0, (DWORD)X509_return_code, 0, sbuf, sizeof(sbuf)-1,
		    0) > 0) {
    size_t n = strlen(sbuf);
    while(n > 0 && (sbuf[n-1] == '\r' || sbuf[n-1] == '\n')) sbuf[--n] = 0;
    return sbuf;
  }
  sprintf(sbuf, "Certificate error 0x%08lX",
	  (unsigned long)(DWORD)X509_return_code);
  return sbuf;
}

int gxSSL::LoadDHParms(const char * /* fname */)
// Schannel picks the key exchange groups itself, so DH parameter
// files are not used. Returns 0.
{
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::MakeRSAEphKey(int /* key_len */)
// Not used with Schannel. Returns 0.
{
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::InitPRNG(const char *r_file, int /* check_prng_init */)
// Schannel uses the Windows random number generator. Returns 0.
{
  if(r_file) random_file = r_file;
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::InitPRNG(int /* check_prng_init */)
// Schannel uses the Windows random number generator. Returns 0.
{
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::MakeDHParms(const char * /* dh_fname */,
		       unsigned char * /* dh1024_p */, int /* dh1024_p_len */,
		       unsigned char * /* dh1024_g */, int /* dh1024_g_len */)
// Not supported with Schannel. Returns a non-zero value.
{
  return SetAppError("SSL exception: MakeDHParms() is not supported with \
Schannel, which does not need DH parameter files");
}

int gxSSL::MakeRSAPrivateKey(const char * /* key_fname */,
			     int /* key_len */,
			     KeyGenCallbackFunc /* cb_func */,
			     void * /* cb_arg */)
// Not supported with Schannel. Returns a non-zero value.
{
  return SetAppError("SSL exception: MakeRSAPrivateKey() is not supported \
with Schannel, use openssl genpkey to make a key");
}

int gxSSL::MakeCertReq(const char * /* key_fname */,
		       gxCertIssuerInfo & /* ci */,
		       const char * /* req_fname */)
// Not supported with Schannel. Returns a non-zero value.
{
  return SetAppError("SSL exception: MakeCertReq() is not supported with \
Schannel, use openssl req to make a certificate");
}

#endif // __GX_SSL_SCHANNEL_SOURCE__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
