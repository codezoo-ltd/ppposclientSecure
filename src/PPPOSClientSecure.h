#ifndef _PPPOSCLIENTSECURE_H_
#define _PPPOSCLIENTSECURE_H_

//#define PPPOS_RXBUFFER_LENGTH 1024
#define PPPOS_RXBUFFER_LENGTH 2048
#define PPPOS_CLIENT_DEF_CONN_TIMEOUT_MS (3000)
#define PPPOS_CLIENT_MAX_WRITE_RETRY (10)
#define PPPOS_CLIENT_SELECT_TIMEOUT_US (1000000)
#define PPPOS_CLIENT_FLUSH_BUFFER_SIZE (1024)

#include "Client.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <Arduino.h>
#include <errno.h>
#include "IPAddress.h"
#include "ssl_client.h"

class PPPOSClientSecure : public Client {

	protected:
		sslclient_context *sslclient;

		bool _connected;
		int _lastError = 0;
		int _peek = -1; 
		int _timeout;
		bool _use_insecure;
		const char *_CA_cert;
		const char *_cert;
		const char *_private_key;
		const char *_pskIdent; // identity for PSK cipher suites
		const char *_psKey; // key in hex for PSK cipher suites
		const char **_alpn_protos;
		bool _use_ca_bundle;

	public:
		PPPOSClientSecure *next;
		PPPOSClientSecure();
		PPPOSClientSecure(int socket);
		~PPPOSClientSecure();
		int connect(IPAddress ip, uint16_t port);
		int connect(const char *host, uint16_t port);
		int connect(IPAddress ip, uint16_t port, const char *rootCABuff, const char *cli_cert, const char *cli_key);
		int connect(const char *host, uint16_t port, const char *rootCABuff, const char *cli_cert, const char *cli_key);
		int connect(IPAddress ip, uint16_t port, const char *pskIdent, const char *psKey);
		int connect(const char *host, uint16_t port, const char *pskIdent, const char *psKey);
		size_t write(uint8_t data);
		size_t write(const uint8_t *buf, size_t size);
		int available();
		int read();
		int read(uint8_t *buf, size_t size);
		int peek();
		void flush();
		void stop();
		uint8_t connected();
		int lastError(char *buf, const size_t size);
		void setInsecure(); // Don't validate the chain, just accept whatever is given.  VERY INSECURE!
		void setPreSharedKey(const char *pskIdent, const char *psKey); // psKey in Hex
		void setCACert(const char *rootCA);
		void setCertificate(const char *client_ca);
		void setPrivateKey (const char *private_key);
		bool loadCACert(Stream& stream, size_t size);
		void setCACertBundle(const uint8_t * bundle);
		bool loadCertificate(Stream& stream, size_t size);
		bool loadPrivateKey(Stream& stream, size_t size);
		bool verify(const char* fingerprint, const char* domain_name);
		void setHandshakeTimeout(unsigned long handshake_timeout);
		void setAlpnProtocols(const char **alpn_protos);
		const mbedtls_x509_crt* getPeerCertificate() { return mbedtls_ssl_get_peer_cert(&sslclient->ssl_ctx); };
		bool getFingerprintSHA256(uint8_t sha256_result[32]) { return get_peer_fingerprint(sslclient, sha256_result); };
		int setTimeout(uint32_t seconds);
		int setSocketOption(int option, char* value, size_t len);

		operator bool()
		{   
			return connected();
		}   
		PPPOSClientSecure &operator=(const PPPOSClientSecure &other);
		bool operator==(const bool value)
		{   
			return bool() == value;
		}   
		bool operator!=(const bool value)
		{   
			return bool() != value;
		}   
		bool operator==(const PPPOSClientSecure &); 
		bool operator!=(const PPPOSClientSecure &rhs)
		{   
			return !this->operator==(rhs);
		};  

		int socket()
		{   
			return sslclient->socket = -1; 
		}   

	//	using Print::write; //[maybe]

	private:
		char *_streamLoad(Stream& stream, size_t size);

		//friend class WiFiServer;
		using Print::write;  
};

#endif /* _PPPOSCLIENTSECURE_H_ */
