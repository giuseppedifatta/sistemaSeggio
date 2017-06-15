#ifndef SSLCLIENT_H
#define SSLCLIENT_H
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>

class SSLClient
{
private:
    //membri privati
    SSL_CTX *ctx;
    int server_sock;
    BIO *outbio;

    //metodi privati
    void init_openssl_library();
    int create_socket(const char *hostname, const char * port);
    void ShowCerts(SSL *ssl);
    void configure_context(char* CertFile, char* KeyFile, char * ChainFile);
    void verify_ServerCert(const char * hostname,SSL *ssl);
    int myssl_getFile(SSL *ssl);
public:
    SSLClient();
    ~SSLClient();



    void connectToStop(SSL *ssl, const char * hostname);
    unsigned int getStatoPV();
};


#endif // SSLCLIENT_H
