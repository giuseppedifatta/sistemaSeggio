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

    SSL_CTX *ctx;
    BIO* outbio;
    int server_sock;
    SSL * ssl;
    const char * PV_IPaddress;
    //metodi privati
    void ShowCerts();

    void verify_ServerCert(const char * hostIP /*hostname*/);
    int myssl_getFile();
public:
    SSLClient(Seggio * s);
    ~SSLClient();

    Seggio *seggioChiamante;
    void stopLocalServer(const char * localhost);
    unsigned int getStatoPV();
    int create_socket(const char * hostIP/*hostname*/,const char * port);
    SSL* connectTo(const char* hostIP/*hostname*/);

    //funzioni per l'inizializzazione di SSL e la configurazione del contesto SSL
    void init_openssl_library();
    void cleanup_openssl();
    void createClientContext();
    void configure_context(char* CertFile, char* KeyFile, char * ChainFile);

    //richieste per le Postazioni di Voto
    void querySetAssociation(unsigned int idHT);
    void queryPullPVState();
    void queryRemoveAssociation();
    void queryFreePV();
};


#endif // SSLCLIENT_H
