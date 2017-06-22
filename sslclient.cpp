#include "sslclient.h"

/*
 * sslClient.h
 *
 *  Created on: 02/apr/2017
 *      Author: giuseppe
 */

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;

#define SERVER_PORT "4433"
#define LOCALHOST "127.0.0.1"

SSLClient::SSLClient(Seggio * s){
    //this->hostname = IP_PV;
    this->seggioChiamante = s;

    /* ---------------------------------------------------------- *
     * Create the Input/Output BIO's.                             *
     * ---------------------------------------------------------- */
    outbio = BIO_new_fp(stdout, BIO_NOCLOSE);
    ssl = nullptr;

    /* ---------------------------------------------------------- *
     * Function that initialize openssl for correct work.		  *
     * ---------------------------------------------------------- */
    this->init_openssl_library();

    createClientContext();

    char certFile[] = "/home/giuseppe/myCA/intermediate/certs/client.cert.pem";
    char keyFile[] = "/home/giuseppe/myCA/intermediate/private/client.key.pem";
    char chainFile[] =
            "/home/giuseppe/myCA/intermediate/certs/ca-chain.cert.pem";

    this->configure_context(certFile, keyFile, chainFile);
    s->mutex_stdout.lock();
    cout << "Cert and key configured" << endl;
    s->mutex_stdout.unlock();
}

SSLClient::~SSLClient(){
    /* ---------------------------------------------------------- *
     * Free the structures we don't need anymore                  *
     * -----------------------------------------------------------*/
    BIO_free_all(this->outbio);
    SSL_CTX_free(this->ctx);
    this->cleanup_openssl();

}

void SSLClient::stopLocalServer(const char* localhost/*hostname*/){
    //questa funzione contatta il server locale, ma non deve fare alcuna operazione se non quella
    //di sbloccare il server locale dallo stato di attesa di una nuova connessione, così da portare
    //al ricontrollo della condizione del while che se falsa, porta
    //all'interruzione del thread chiamante
    const char * port = SERVER_PORT;
    create_socket(localhost/*hostname*/, port);
    seggioChiamante->mutex_stdout.lock();
    cout << "Client: niente da fare qui...chiudo la socket per il server" << endl;
    seggioChiamante->mutex_stdout.unlock();
    close(this->server_sock);
}

int SSLClient::create_socket(const char * hostIP/*hostname*/,const char * port) {
    /* ---------------------------------------------------------- *
     * create_socket() creates the socket & TCP-connect to server *
     * ---------------------------------------------------------- */

    const char *address_printable = NULL;

    /* decomentare per usare l'hostname
     * struct hostent *host;
     */
    struct sockaddr_in dest_addr;

    unsigned int portCod = atoi(port);
    /* decommentare la sezione se si passa alla funzione l'hostname invece dell'ip dell'host
    if ((host = gethostbyname(hostname)) == NULL) {
        BIO_printf(this->outbio, "Error: Cannot resolve hostname %s.\n", hostname);
        abort();
    }
    */

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(portCod);
    /* decommentare la sezione se si passa alla funzione l'hostname invece dell'ip dell'host
    dest_addr.sin_addr.s_addr = *(long*) (host->h_addr);
    */

    // commentare la riga sotto se non si vuole usare l'ip dell'host, ma l'hostname
    dest_addr.sin_addr.s_addr = inet_addr(hostIP);

    /* ---------------------------------------------------------- *
     * create the basic TCP socket                                *
     * ---------------------------------------------------------- */
    this->server_sock = socket(AF_INET, SOCK_STREAM, 0);
    /* ---------------------------------------------------------- *
     * Zeroing the rest of the struct                             *
     * ---------------------------------------------------------- */
    memset(&(dest_addr.sin_zero), '\0', 8);

    address_printable = inet_ntoa(dest_addr.sin_addr);

    /* ---------------------------------------------------------- *
     * Try to make the host connect here                          *
    * ---------------------------------------------------------- */
    int res = connect(this->server_sock, (struct sockaddr *) &dest_addr, sizeof(struct sockaddr));

    seggioChiamante->mutex_stdout.lock();
    if (res  == -1) {
        BIO_printf(this->outbio, "Client: Error: Cannot connect to host %s [%s] on port %d.\n",
                   hostIP/*hostname*/, address_printable, portCod);
    } else {
        BIO_printf(this->outbio, "Client: Successfully connect to host %s [%s] on port %d.\n",
                   hostIP/*hostname*/, address_printable, portCod);

    }
    seggioChiamante->mutex_stdout.unlock();

    seggioChiamante->mutex_stdout.lock();
    cout << "Client: Descrittore socket: "<< this->server_sock << endl;
    seggioChiamante->mutex_stdout.unlock();

    return res;
}

SSL * SSLClient::connectTo(const char* hostIP/*hostname*/){
    this->PV_IPaddress = hostIP;

    const char * port = SERVER_PORT;

    /* ---------------------------------------------------------- *
     * Create new SSL connection state object                     *
     * ---------------------------------------------------------- */

    this->ssl = SSL_new(this->ctx);
seggioChiamante->mutex_stdout.lock();
    cout << "ConnectTo - ssl pointer: " << this->ssl << endl;
    seggioChiamante->mutex_stdout.unlock();
    /* ---------------------------------------------------------- *
     * Make the underlying TCP socket connection                  *
     * ---------------------------------------------------------- */


    int res = create_socket(hostIP /*hostname*/,port);
    if (res == 0){
        BIO_printf(this->outbio,
                   "Successfully create the socket for TCP connection to: %s.\n",
                   hostIP /*hostname*/);
    }
    else {
        BIO_printf(this->outbio,
                   "Unable to create the socket for TCP connection to: %s.\n",
                   hostIP /*hostname*/);
    }

    /* ---------------------------------------------------------- *
     * Attach the SSL session to the socket descriptor            *
     * ---------------------------------------------------------- */

    if (SSL_set_fd(this->ssl, this->server_sock) != 1) {
        BIO_printf(this->outbio, "Error: Connection to %s failed", hostIP /*hostname*/);
    }
    else
        BIO_printf(this->outbio, "Ok: Connection to %s ", hostIP /*hostname*/);
    /* ---------------------------------------------------------- *
     * Try to SSL-connect here, returns 1 for success             *
     * ---------------------------------------------------------- */
    if (SSL_connect(this->ssl) != 1) //SSL handshake
        BIO_printf(this->outbio, "Error: Could not build a SSL session to: %s.\n",
                   hostIP /*hostname*/);
    else
        BIO_printf(this->outbio, "Successfully enabled SSL/TLS session to: %s.\n",
                   hostIP /*hostname*/);
    ShowCerts();
    verify_ServerCert(hostIP /*hostname*/);
    //SSL_set_connect_state(ssl);
    return this->ssl;
}

void SSLClient::ShowCerts() {
    X509 *peer_cert;
    char *line;

    peer_cert = SSL_get_peer_certificate(this->ssl); /* Get certificates (if available) */

    //ERR_print_errors_fp(stderr);
    if (peer_cert != NULL) {
        printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(peer_cert), 0, 0);
        printf("Subject: %s\n", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(peer_cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);
        X509_free(peer_cert);
    } else
        printf("No certificates.\n");
}

void SSLClient::verify_ServerCert(const char * hostIP/*hostname*/) {
    // Declare X509 structure
    X509 *error_cert = NULL;
    X509 *peer_cert = NULL;
    X509_NAME *certsubject = NULL;
    X509_STORE *store = NULL;
    X509_STORE_CTX *vrfy_ctx = NULL;
    X509_NAME *certname = NULL;
    int ret;
    //BIO *certbio = NULL;
    //certbio = BIO_new(BIO_s_file());


    // Get the remote certificate into the X509 structure

    peer_cert = SSL_get_peer_certificate(this->ssl);
    if (peer_cert == NULL)
        BIO_printf(this->outbio, "Error: Could not get a certificate from: %s.\n",
                   hostIP/*hostname*/);
    else
        BIO_printf(this->outbio, "Retrieved the server's certificate from: %s.\n",
                   hostIP/*hostname*/);


    // extract various certificate information

    certname = X509_NAME_new();
    certname = X509_get_subject_name(peer_cert);

    // display the cert subject here

    BIO_printf(this->outbio, "Displaying the certificate subject data:\n");
    X509_NAME_print_ex(this->outbio, certname, 0, 0);
    BIO_printf(this->outbio, "\n");

    //Initialize the global certificate validation store object.
    if (!(store = X509_STORE_new()))
        BIO_printf(this->outbio, "Error creating X509_STORE_CTX object\n");

    // Create the context structure for the validation operation.
    vrfy_ctx = X509_STORE_CTX_new();

    // Load the certificate and cacert chain from file (PEM).
    /*
         ret = BIO_read_filename(certbio, certFile);
         if (!(cert = PEM_read_bio_X509(certbio, NULL, 0, NULL)))
         BIO_printf(outbio, "Error loading cert into memory\n");
         */
    char chainFile[] =
            "/home/giuseppe/myCA/intermediate/certs/ca-chain.cert.pem";

    ret = X509_STORE_load_locations(store, chainFile, NULL);
    if (ret != 1)
        BIO_printf(this->outbio, "Error loading CA cert or chain file\n");

    /* Initialize the ctx structure for a verification operation:
      Set the trusted cert store, the unvalidated cert, and any  *
     * potential certs that could be needed (here we set it NULL) */
    X509_STORE_CTX_init(vrfy_ctx, store, peer_cert, NULL);

    /* Check the complete cert chain can be build and validated.  *
     * Returns 1 on success, 0 on verification failures, and -1   *
     * for trouble with the ctx object (i.e. missing certificate) */
    ret = X509_verify_cert(vrfy_ctx);

    BIO_printf(this->outbio, "Verification return code: %d\n", ret);

    if (ret == 0 || ret == 1)
        BIO_printf(this->outbio, "Verification result text: %s\n",
                   X509_verify_cert_error_string(vrfy_ctx->error));

    /* The error handling below shows how to get failure details  *
     * from the offending certificate.                            */
    if (ret == 0) {
        //get the offending certificate causing the failure
        error_cert = X509_STORE_CTX_get_current_cert(vrfy_ctx);
        certsubject = X509_NAME_new();
        certsubject = X509_get_subject_name(error_cert);
        BIO_printf(this->outbio, "Verification failed cert:\n");
        X509_NAME_print_ex(this->outbio, certsubject, 0, XN_FLAG_MULTILINE);
        BIO_printf(this->outbio, "\n");
    }

    // Free up all structures need for verify certs
    X509_STORE_CTX_free(vrfy_ctx);
    X509_STORE_free(store);
    X509_free(peer_cert);
    //BIO_free_all(certbio);

}

int SSLClient::myssl_getFile(){
    //richiede ed ottiene lo stato della Postazione di voto a cui è connesso
    char buffer[1024];
    //memset(buffer, '\0', sizeof(buffer));

    //ricevo proposta dal server
    //    int bytes = SSL_read(ssl, buffer, sizeof(buffer));
    //    if (bytes > 0){
    //        cout << buffer;
    //    }

    //

    char reply[] = "b";
    cout << reply << endl;
    SSL_write(ssl, reply, strlen(reply));

    //SSL_write(ssl, reply, strlen(reply));
    //inizio test
    //char buffer[1024];
    memset(buffer, '\0', sizeof(buffer));

    //lettura numero byte da ricevere
    int bytes = SSL_read(ssl, buffer, sizeof(buffer));
    if (bytes > 0) {

        //creo un buffer della dimensione del file che sto per ricevere
        buffer[bytes] = 0;
        cout << "Client: Bytes to read: " << buffer << endl;
        char buffer2[atoi(buffer)];
        if (bytes > 0) {
            ofstream outf("./file_ricevuto.cbc", ios::out);
            if (!outf) {

                cout << "Client: unable to open file for output\n";
                exit(1);
            } else {
                cout << "Client: ricevo il file" << endl;
                SSL_read(ssl, buffer2, sizeof(buffer2));

                cout << "Client: Testo ricevuto: " << buffer2 << endl;
                outf.write(buffer2, sizeof(buffer2));
                outf.close();
                // release dynamically-allocated memory
                //delete[] buffer2;

            }
        }
    }

    return 0;
}

void SSLClient::init_openssl_library() {
    /* https://www.openssl.org/docs/ssl/SSL_library_init.html */
    SSL_library_init();
    /* Cannot fail (always returns success) ??? */

    /* https://www.openssl.org/docs/crypto/ERR_load_crypto_strings.html */
    SSL_load_error_strings();
    /* Cannot fail ??? */

    ERR_load_BIO_strings();
    /* SSL_load_error_strings loads both libssl and libcrypto strings */
    //ERR_load_crypto_strings();
    /* Cannot fail ??? */

    /* OpenSSL_config may or may not be called internally, based on */
    /*  some #defines and internal gyrations. Explicitly call it    */
    /*  *IF* you need something from openssl.cfg, such as a         */
    /*  dynamically configured ENGINE.                              */
    OPENSSL_config(NULL);

}

void SSLClient::createClientContext(){
    const SSL_METHOD *method;
    method = TLSv1_2_client_method();

    /* ---------------------------------------------------------- *
     * Try to create a new SSL context                            *
     * ---------------------------------------------------------- */
    if ((this->ctx = SSL_CTX_new(method)) == NULL)
        BIO_printf(this->outbio, "Unable to create a new SSL context structure.\n");

    /* ---------------------------------------------------------- *
     * Disabling SSLv2 and SSLv3 will leave TSLv1 for negotiation    *
     * ---------------------------------------------------------- */
    SSL_CTX_set_options(this->ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
}

void SSLClient::configure_context(char* CertFile, char* KeyFile, char * ChainFile) {
    SSL_CTX_set_ecdh_auto(this->ctx, 1);

    //---il chainfile dovrà essere ricevuto dal peer che si connette? non so se è necessario su entrambi i peer----
    SSL_CTX_load_verify_locations(this->ctx,ChainFile, NULL);
    //SSL_CTX_use_certificate_chain_file(ctx,"/home/giuseppe/myCA/intermediate/certs/ca-chain.cert.pem");
    /*The final step of configuring the context is to specify the certificate and private key to use.*/
    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(this->ctx, CertFile, SSL_FILETYPE_PEM) < 0) {
        //ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(this->ctx, KeyFile, SSL_FILETYPE_PEM) < 0) {
        //ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (!SSL_CTX_check_private_key(this->ctx)) {
        fprintf(stderr, "Private key does not match the public certificate\n");
        abort();
    }

}

void SSLClient::cleanup_openssl() {
    EVP_cleanup();
}

void SSLClient::querySetAssociation(unsigned int idHT){
    //invia codice del servizio richiesto al PV_Server
    //setAssociation : 0
    int serviceCod = 0;
    stringstream ssCod;
    ssCod << serviceCod;
    string strCod = ssCod.str();
    const char * charCod = strCod.c_str();
    seggioChiamante->mutex_stdout.lock();
    cout << "richiedo il servizio: " << charCod << endl;
    seggioChiamante->mutex_stdout.unlock();
    SSL_write(ssl,charCod,strlen(charCod));

    //invio idHT da associare alla postazione di voto per l'avvio della funzionalità di abilitazione al voto
    stringstream ssIdHT;
    ssIdHT<< idHT;
    string strIdHT = ssIdHT.str();
    const char * charIdHT = strIdHT.c_str();
    seggioChiamante->mutex_stdout.lock();
    cout << "Id Hardware token da associare alla PV: " << charIdHT << endl;
    seggioChiamante->mutex_stdout.unlock();
    SSL_write(ssl,charIdHT,strlen(charIdHT));


    BIO_printf(outbio, "Finished SSL/TLS connection with server: %s.\n",
               this->PV_IPaddress);
    close(this->server_sock);
    SSL_shutdown(this->ssl);
    SSL_free(this->ssl);
}
void SSLClient::queryPullPVState(){
    //invia codice del servizio richiesto al PV_Server
    //pullPVState: 1


    BIO_printf(outbio, "Finished SSL/TLS connection with server: %s.\n",
               this->PV_IPaddress);
    close(this->server_sock);
    SSL_shutdown(this->ssl);
    SSL_free(this->ssl);
}
void SSLClient::queryRemoveAssociation() {
    //invia codice del servizio richiesto al PV_Server
    //removeAssociation: 2


    BIO_printf(outbio, "Finished SSL/TLS connection with server: %s.\n",
               this->PV_IPaddress);
    close(this->server_sock);
    SSL_shutdown(this->ssl);
    SSL_free(this->ssl);
}
void SSLClient::queryFreePV(){
    //invia codice del servizio richiesto al PV_Server
    //freePV: 3



    BIO_printf(outbio, "Finished SSL/TLS connection with server: %s.\n",
               this->PV_IPaddress);
    close(this->server_sock);
    SSL_shutdown(this->ssl);
    SSL_free(this->ssl);
}
