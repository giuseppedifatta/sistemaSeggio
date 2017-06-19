#ifndef SSLCLIENT_H
#define SSLCLIENT_H
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>

#include "seggio.h"

class Seggio;
class SSLClient
{
private:
    //membri privati

    int server_sock;
    BIO *outbio;

    //metodi privati
    void ShowCerts(SSL *ssl);

    void verify_ServerCert(const char * hostIP /*hostname*/,SSL *ssl);
    int myssl_getFile(SSL *ssl);
public:
    SSLClient(Seggio * s);
    ~SSLClient();
    SSL * ssl;

    Seggio *seggioChiamante;
    void stopLocalServer(const char * localhost);
    unsigned int getStatoPV();
    int create_socket(const char * hostIP/*hostname*/,const char * port);
    SSL* connectTo(const char* hostIP/*hostname*/);
};


#endif // SSLCLIENT_H
