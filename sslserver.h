#ifndef SSLSERVER_H
#define SSLSERVER_H

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/conf.h>
#include <openssl/x509.h>
#include <openssl/buffer.h>
#include <openssl/x509v3.h>
#include <openssl/opensslconf.h>

#include <vector>
#include <mutex>
#include "aggiornamentostatopv.h"
#include <thread>
#include <queue>
#include "seggio.h"
using namespace std;

class Seggio;

class SSLServer
{
public:

    SSLServer(Seggio * s);
    ~SSLServer();
    void ascoltoNuovoStatoPV();
    void setStopServer(bool b);

    enum servizi {
        aggiornamentoPV
    };

    //mutex per l'accesso al vettore
    //mutex mtx_vector;
    //vector <AggiornamentoStatoPV*> aggiornamentiVector;
    //queue<thread> threads_q;

    int getListenSocketFD();
private:

    //std::thread test_thread;
    BIO* outbio;
    SSL_CTX * ctx;
    SSL * ssl;
    int listen_sock;
    Seggio * seggioChiamante;
    bool stopServer;

    int openListener(int s_port);
    void init_openssl_library();
    void cleanup_openssl();

    void createServerContext();
    void configure_context(char* CertFile, char* KeyFile, char* ChainFile);
    void ShowCerts();
    void verify_ClientCert();
    void Servlet(int client_sock, servizi servizio);
    void service(servizi servizio);
    int myssl_fwrite(const char * infile);

    void print_error_string(unsigned long err, const char* const label);
    int verify_callback(int preverify, X509_STORE_CTX* x509_ctx);
    void print_san_name(const char* label, X509* const cert);
    void print_cn_name(const char* label, X509_NAME* const name);
};

#endif // SSLSERVER_H
