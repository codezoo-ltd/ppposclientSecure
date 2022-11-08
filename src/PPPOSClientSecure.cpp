#include "PPPOSClientSecure.h"
#include "esp_crt_bundle.h"
#include <errno.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

#undef connect
#undef write
#undef read

static const char *TAG = "status";

PPPOSClientSecure::PPPOSClientSecure() {
  _connected = false;
  _timeout = 50000; // Same default as ssl_client

  sslclient = new sslclient_context;
  ssl_init(sslclient);
  sslclient->socket = -1;
  sslclient->handshake_timeout = 120000;
  _use_insecure = false;
  _CA_cert = NULL;
  _cert = NULL;
  _private_key = NULL;
  _pskIdent = NULL;
  _psKey = NULL;
  next = NULL;
  _alpn_protos = NULL;
  _use_ca_bundle = false;
}

PPPOSClientSecure::PPPOSClientSecure(int sock) {
  _connected = false;
  _timeout = 50000; // Same default as ssl_client

  sslclient = new sslclient_context;
  ssl_init(sslclient);
  sslclient->socket = sock;
  sslclient->handshake_timeout = 180000;

  if (sock >= 0) {
    _connected = true;
  }

  _CA_cert = NULL;
  _cert = NULL;
  _private_key = NULL;
  _pskIdent = NULL;
  _psKey = NULL;
  next = NULL;
  _alpn_protos = NULL;
}

PPPOSClientSecure::~PPPOSClientSecure() {
  log_v("-----------------------------111");
  stop();
  delete sslclient;
}

PPPOSClientSecure &
PPPOSClientSecure::operator=(const PPPOSClientSecure &other) {
  log_v("-----------------------------222");
  stop();
  sslclient->socket = other.sslclient->socket;
  _connected = other._connected;
  return *this;
}

void PPPOSClientSecure::stop() {
  if (sslclient->socket >= 0) {
    close(sslclient->socket);
    sslclient->socket = -1;
    _connected = false;
    _peek = -1;
  }

  log_v("-----------------------------333");
  stop_ssl_socket(sslclient, _CA_cert, _cert, _private_key);
}

int PPPOSClientSecure::connect(IPAddress ip, uint16_t port) {
  if (_pskIdent && _psKey)
    return connect(ip, port, _pskIdent, _psKey);
  return connect(ip, port, _CA_cert, _cert, _private_key);
}

int PPPOSClientSecure::connect(const char *host, uint16_t port) {
  if (_pskIdent && _psKey)
    return connect(host, port, _pskIdent, _psKey);
  return connect(host, port, _CA_cert, _cert, _private_key);
}

int PPPOSClientSecure::connect(IPAddress ip, uint16_t port, const char *CA_cert,
                               const char *cert, const char *private_key) {
  return connect(ip.toString().c_str(), port, CA_cert, cert, private_key);
}

int PPPOSClientSecure::connect(const char *host, uint16_t port,
                               const char *CA_cert, const char *cert,
                               const char *private_key) {
  log_v("start_ssl_client with CA, Cert, Private Key");
  int ret = start_ssl_client(sslclient, host, port, _timeout, CA_cert,
                             _use_ca_bundle, cert, private_key, NULL, NULL,
                             _use_insecure, _alpn_protos);
  _lastError = ret;
  if (ret < 0) {
    log_e("start_ssl_client: %d", ret);
    log_v("-----------------------------444");
    stop();
    return 0;
  }
  _connected = true;
  return 1;
}

int PPPOSClientSecure::connect(IPAddress ip, uint16_t port,
                               const char *pskIdent, const char *psKey) {
  return connect(ip.toString().c_str(), port, pskIdent, psKey);
}

int PPPOSClientSecure::connect(const char *host, uint16_t port,
                               const char *pskIdent, const char *psKey) {
  log_v("start_ssl_client with PSK");
  int ret =
      start_ssl_client(sslclient, host, port, _timeout, NULL, false, NULL, NULL,
                       pskIdent, psKey, _use_insecure, _alpn_protos);
  _lastError = ret;
  if (ret < 0) {
    log_e("start_ssl_client: %d", ret);
    log_v("-----------------------------555");
    stop();
    return 0;
  }
  _connected = true;
  return 1;
}

int PPPOSClientSecure::peek() {
  if (_peek >= 0) {
    return _peek;
  }
  _peek = timedRead();
  return _peek;
}

size_t PPPOSClientSecure::write(uint8_t data) { return write(&data, 1); }

int PPPOSClientSecure::read() {
  uint8_t data = -1;
  int res = read(&data, 1);
  if (res < 0) {
    return res;
  }
  return data;
}

size_t PPPOSClientSecure::write(const uint8_t *buf, size_t size) {
  if (!_connected) {
    return 0;
  }
  int res = send_ssl_data(sslclient, buf, size);
  log_v("send_ssl_data with %d return", res);

  if (res < 0) {
    log_v("send_ssl_data stop!!!");
    log_v("-----------------------------666");
    stop();
    res = 0;
  }
  return res;
}

int PPPOSClientSecure::read(uint8_t *buf, size_t size) {
  int peeked = 0;
  int avail = available();
  if ((!buf && size) || avail <= 0) {
    return -1;
  }
  if (!size) {
    return 0;
  }
  if (_peek >= 0) {
    buf[0] = _peek;
    _peek = -1;
    size--;
    avail--;
    if (!size || !avail) {
      return 1;
    }
    buf++;
    peeked = 1;
  }

  int res = get_ssl_receive(sslclient, buf, size);
  log_v("get_ssl_receive with %d return", res);
  if (res < 0) {
    log_v("-----------------------------777");
    stop();
    return peeked ? peeked : res;
  }
  return res + peeked;
}

int PPPOSClientSecure::available() {
  int peeked = (_peek >= 0);
  if (!_connected) {
    return peeked;
  }
  int res = data_to_read(sslclient);
  //	log_v("data_to_read with %d return",res);
  if (res < 0) {
    log_v("-----------------------------888");
    stop();
    return peeked ? peeked : res;
  }
  return res + peeked;
}

void PPPOSClientSecure::flush() {}

uint8_t PPPOSClientSecure::connected() {
  uint8_t dummy = 0;
  read(&dummy, 0);

  return _connected;
}

void PPPOSClientSecure::setInsecure() {
  _CA_cert = NULL;
  _cert = NULL;
  _private_key = NULL;
  _pskIdent = NULL;
  _psKey = NULL;
  _use_insecure = true;
}

void PPPOSClientSecure::setCACert(const char *rootCA) { _CA_cert = rootCA; }

void PPPOSClientSecure::setCACertBundle(const uint8_t *bundle) {
  if (bundle != NULL) {
    esp_crt_bundle_set(bundle);
    _use_ca_bundle = true;
  } else {
    esp_crt_bundle_detach(NULL);
    _use_ca_bundle = false;
  }
}

void PPPOSClientSecure::setCertificate(const char *client_ca) {
  _cert = client_ca;
}

void PPPOSClientSecure::setPrivateKey(const char *private_key) {
  _private_key = private_key;
}

void PPPOSClientSecure::setPreSharedKey(const char *pskIdent,
                                        const char *psKey) {
  _pskIdent = pskIdent;
  _psKey = psKey;
}

bool PPPOSClientSecure::verify(const char *fp, const char *domain_name) {
  if (!sslclient)
    return false;

  return verify_ssl_fingerprint(sslclient, fp, domain_name);
}

char *PPPOSClientSecure::_streamLoad(Stream &stream, size_t size) {
  char *dest = (char *)malloc(size + 1);
  if (!dest) {
    return nullptr;
  }
  if (size != stream.readBytes(dest, size)) {
    free(dest);
    dest = nullptr;
    return nullptr;
  }
  dest[size] = '\0';
  return dest;
}

bool PPPOSClientSecure::loadCACert(Stream &stream, size_t size) {
  if (_CA_cert != NULL)
    free(const_cast<char *>(_CA_cert));
  char *dest = _streamLoad(stream, size);
  bool ret = false;
  if (dest) {
    setCACert(dest);
    ret = true;
  }
  return ret;
}

bool PPPOSClientSecure::loadCertificate(Stream &stream, size_t size) {
  if (_cert != NULL)
    free(const_cast<char *>(_cert));
  char *dest = _streamLoad(stream, size);
  bool ret = false;
  if (dest) {
    setCertificate(dest);
    ret = true;
  }
  return ret;
}

bool PPPOSClientSecure::loadPrivateKey(Stream &stream, size_t size) {
  if (_private_key != NULL)
    free(const_cast<char *>(_private_key));
  char *dest = _streamLoad(stream, size);
  bool ret = false;
  if (dest) {
    setPrivateKey(dest);
    ret = true;
  }
  return ret;
}

int PPPOSClientSecure::lastError(char *buf, const size_t size) {
  if (!_lastError) {
    return 0;
  }
  mbedtls_strerror(_lastError, buf, size);
  return _lastError;
}

void PPPOSClientSecure::setHandshakeTimeout(unsigned long handshake_timeout) {
  sslclient->handshake_timeout = handshake_timeout * 1000;
}

void PPPOSClientSecure::setAlpnProtocols(const char **alpn_protos) {
  _alpn_protos = alpn_protos;
}
int PPPOSClientSecure::setTimeout(uint32_t seconds) {
  _timeout = seconds * 1000;
  if (sslclient->socket >= 0) {
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    if (setSocketOption(SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)) < 0) {
      return -1;
    }
    return setSocketOption(SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval));
  } else {
    return 0;
  }
}
int PPPOSClientSecure::setSocketOption(int option, char *value, size_t len) {
  int res = setsockopt(sslclient->socket, SOL_SOCKET, option, value, len);
  if (res < 0) {
    log_e("%X : %d", option, errno);
  }
  return res;
}
