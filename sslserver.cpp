#include "sslserver.h"
/*
 * server.h
 *
 *  Created on: 02/apr/2017
 *      Author: giuseppe
 */

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <mutex>
#include <vector>
#include <thread>
#include <queue>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>



# define HOST_NAME "localhost"

#define PORT "4433"

using namespace std;

SSLServer::SSLServer(Seggio *s){
    this->stop = false;
    this->seggioChiamante = s;

    this->init_openssl_library();
    this->create_context();

    char certFile[] =
            "/home/giuseppe/myCA/intermediate/certs/localhost.cert.pem";
    char keyFile[] =
            "/home/giuseppe/myCA/intermediate/private/localhost.key.pem";
    char chainFile[] =
            "/home/giuseppe/myCA/intermediate/certs/ca-chain.cert.pem";

    configure_context(certFile, keyFile, chainFile);


    cout << "Cert and key configured" << endl;

    this->server_sock = this->openListener(atoi(PORT));
    this->outbio = BIO_new_fp(stdout, BIO_NOCLOSE);


}

SSLServer::~SSLServer(){
    //nel distruttore
    BIO_free_all(outbio);
    close(server_sock);
    SSL_CTX_free(this->ctx);
    cleanup_openssl();
}

void SSLServer::ascoltoNuovoStatoPV(){
    /* Handle connections */
    SSL * ssl = nullptr;

    //inizializza una socket per il client
    struct sockaddr_in client_addr;
    uint len = sizeof(client_addr);

    seggioChiamante->mutex_stdout.lock();
    cout << "server_Seggio in ascolto sulla porta " << PORT <<", attesa connessione da un client PV...\n";
    seggioChiamante->mutex_stdout.unlock();

    // accept restituisce un valore negativo in caso di insuccesso
    int client_sock = accept(this->server_sock, (struct sockaddr*) &client_addr,
                             &len);

    if (client_sock < 0) {
        perror("Unable to accept");
        exit(EXIT_FAILURE);
    }
    else{
        cout << "Un client ha iniziato la connessione su una socket con fd:" << client_sock << endl;
    }

    seggioChiamante->mutex_stdout.lock();
    cout<<"Server: Client's Port assegnata: "<< ntohs(client_addr.sin_port)<< endl;
    seggioChiamante->mutex_stdout.unlock();




    if(!this->stop){
        ssl = SSL_new(ctx);
        //se non è stata settata l'interruzione del thread lancia il thread per servire la richiesta
        thread t (&SSLServer::Servlet,this, ssl, client_sock, servizi::aggiornamentoPV);
        t.detach();
        cout << "Server: start a thread..." << endl;
    }
    else{
        //termina l'ascolto
        cout << "Server: interruzione del server in corso..." << endl;
        return;
    }

    //cout << ssl << endl;
    //Servlet(ssl, client_sock, servizi::aggiornamentoPV);

}

void SSLServer::Servlet(SSL * ssl,int client_sock ,servizi servizio) {/* Serve the connection -- threadable */
    seggioChiamante->mutex_stdout.lock();
    cout << "Servlet: inizio servlet" << endl;
    seggioChiamante->mutex_stdout.unlock();
    //cout << ssl << endl;
    //configurara ssl per collegarsi sulla socket indicata
    SSL_set_fd(ssl, client_sock);
    int sd;

    if (SSL_accept(ssl) <= 0) {/* do SSL-protocol handshake */
        cout << "error in handshake" << endl;
        ERR_print_errors_fp(stderr);

    }
    else {
        seggioChiamante->mutex_stdout.lock();
        cout << "handshake ok!" << endl;
        seggioChiamante->mutex_stdout.unlock();
        this->ShowCerts(ssl);
        this->verify_ClientCert(ssl);

        seggioChiamante->mutex_stdout.lock();
        cout << "service starting..." << endl;
        seggioChiamante->mutex_stdout.unlock();
        this->service(ssl,servizio);
    }

    sd = SSL_get_fd(ssl); /* get socket connection */
    //cout << "What's up?" << endl;
    close(sd); /* close connection */
    SSL_free(ssl);

    seggioChiamante->mutex_stdout.lock();
    cout << "fine servlet" << endl;
    seggioChiamante->mutex_stdout.unlock();
    close(client_sock);
}

void SSLServer::service(SSL *ssl, servizi servizio) {
    seggioChiamante->mutex_stdout.lock();
    cout << "Seggio Server: service started: " << servizio << endl;
    seggioChiamante->mutex_stdout.unlock();
    char buf[1024];
    char reply[1024];
    memset(buf, '\0', sizeof(buf));
    //memset(cod_service, '\0', sizeof(cod_service));

    int bytes;

    switch (servizio) {


    case servizi::aggiornamentoPV:{
        unsigned int idPV;
        bytes = SSL_read(ssl, buf, sizeof(buf));
        if (bytes > 0){
            buf[bytes] = 0;
            idPV = atoi(buf);
            seggioChiamante->mutex_stdout.lock();
            cout << "Seggio Server: idPV: " << idPV << endl;
            seggioChiamante->mutex_stdout.unlock();
        }
        else if(bytes == 0){
            seggioChiamante->mutex_stdout.lock();
            cout << "Seggio Server: error connection" << endl;
            int ret = SSL_get_error(ssl,bytes);
            cout << "Seggio Server: Error code: " << ret << endl;
            seggioChiamante->mutex_stdout.unlock();
        }
        else {
            seggioChiamante->mutex_stdout.lock();
            cout << "Seggio Server: error read" << endl;
            int ret = SSL_get_error(ssl,bytes);
            cout << " Seggio Server: Error code: " << ret << endl;
            seggioChiamante->mutex_stdout.unlock();
        }

        memset(buf, '\0', sizeof(buf));

        unsigned int nuovoStato;
        SSL_read(ssl, buf, sizeof(buf));
        if (bytes>0){
            buf[bytes] = 0;
            nuovoStato = atoi(buf);
            seggioChiamante->mutex_stdout.lock();
            cout << "nuovoStato: " << nuovoStato << endl;
            seggioChiamante->mutex_stdout.unlock();
        }

        seggioChiamante->setPVstate(idPV,nuovoStato);
        break;
    }

    case 'a':
    case 'A': {
        const char * temp_string = "Inserire del testo prova per il server: %d";
        int i = 1;
        memset(reply, '\0', sizeof(reply));
        sprintf(reply, temp_string, i);
        SSL_write(ssl, reply, sizeof(reply));

        bytes = SSL_read(ssl, buf, sizeof(buf));  //get message from client
        if (bytes > 0) {

            cout << "num. bytes: " << bytes << endl;

            //buf[bytes] = 0; //mette uno 0 terminatore dopo l'ultimo byte ricevuto
            const char* HTMLecho = "<html><body><pre>%s</pre></body></html>\n";
            printf("Client msg: %s \n", buf);
            memset(reply, '\0', sizeof(reply));
            sprintf(reply, HTMLecho, buf);   // construct reply
            SSL_write(ssl, reply, strlen(reply));  //send reply
        } else
            ERR_print_errors_fp(stderr);



        break;
    }
        //richiesta file
    case 'b':
    case 'B': {
        myssl_fwrite(ssl,"cifrato.cbc");
        break;

    }
    default:
        seggioChiamante->mutex_stdout.lock();
        cout << "servizio non esistente" << endl;
        seggioChiamante->mutex_stdout.unlock();
    }


    return;

}

int SSLServer::openListener(int s_port) {

    // non è specifico per openssl, crea una socket in ascolto su una porta passata come argomento
    int listen_sock, r;

    struct sockaddr_in sa_serv;
    listen_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_sock <= 0) {
        perror("Unable to create socket");
        abort();
    }

    memset(&sa_serv, 0, sizeof(sa_serv));
    sa_serv.sin_family = AF_INET;
    sa_serv.sin_addr.s_addr = INADDR_ANY;
    sa_serv.sin_port = htons(s_port); /* Server Port number */
    cout<<"Server: Server's Port: "<< ntohs(sa_serv.sin_port)<<endl;

    r = bind(listen_sock, (struct sockaddr*) &sa_serv, sizeof(sa_serv));
    if (r < 0) {
        perror("Unable to bind");
        exit(EXIT_FAILURE);
    }

    // Receive a TCP connection.
    r = listen(listen_sock, 10);

    if (r < 0) {
        perror("Unable to listen");
        exit(EXIT_FAILURE);
    }
    return listen_sock;
}

void SSLServer::create_context() {
    const SSL_METHOD *method;


    //method = SSLv23_server_method();
    method = TLSv1_2_server_method();

    this->ctx = SSL_CTX_new(method);
    if (!this->ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    //https://www.openssl.org/docs/man1.1.0/ssl/SSL_CTX_set_options.html
    const long flags = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3
            | SSL_OP_NO_COMPRESSION;
    long old_opts = SSL_CTX_set_options(this->ctx, flags);
    seggioChiamante->mutex_stdout.lock();
    cout << "bitmask options: " << old_opts << endl;
    seggioChiamante->mutex_stdout.unlock();
    //    return ctx;
}

void SSLServer::configure_context(char* CertFile, char* KeyFile, char* ChainFile) {
    SSL_CTX_set_ecdh_auto(this->ctx, 1);

    SSL_CTX_load_verify_locations(this->ctx, ChainFile, ChainFile);
    //SSL_CTX_use_certificate_chain_file(ctx,"/home/giuseppe/myCA/intermediate/certs/ca-chain.cert.pem");

    /*The final step of configuring the context is to specify the certificate and private key to use.*/
    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(this->ctx, CertFile, SSL_FILETYPE_PEM) < 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(this->ctx, KeyFile, SSL_FILETYPE_PEM) < 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    //SSL_CTX_set_default_passwd_cb(ctx,"password"); // cercare funzionamento con reference

    if (!SSL_CTX_check_private_key(this->ctx)) {
        fprintf(stderr, "Private key does not match the public certificate\n");
        abort();
    }
    //substitute NULL with the name of the specific verify_callback
    SSL_CTX_set_verify(this->ctx, SSL_VERIFY_PEER, NULL);

}

void SSLServer::init_openssl_library() {
    /* https://www.openssl.org/docs/ssl/SSL_library_init.html */
    SSL_library_init();
    /* Cannot fail (always returns success) ??? */

    /* https://www.openssl.org/docs/crypto/ERR_load_crypto_strings.html */
    SSL_load_error_strings();
    /* Cannot fail ??? */

    /* SSL_load_error_strings loads both libssl and libcrypto strings */
    ERR_load_crypto_strings();
    /* Cannot fail ??? */

    /* OpenSSL_config may or may not be called internally, based on */
    /*  some #defines and internal gyrations. Explicitly call it    */
    /*  *IF* you need something from openssl.cfg, such as a         */
    /*  dynamically configured ENGINE.                              */
    OPENSSL_config(NULL);

}

void SSLServer::print_cn_name(const char* label, X509_NAME* const name) {
    int idx = -1, success = 0;
    unsigned char *utf8 = NULL;

    do {
        if (!name)
            break; /* failed */

        idx = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
        if (!(idx > -1))
            break; /* failed */

        X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, idx);
        if (!entry)
            break; /* failed */

        ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
        if (!data)
            break; /* failed */

        int length = ASN1_STRING_to_UTF8(&utf8, data);
        if (!utf8 || !(length > 0))
            break; /* failed */


        cout << "  " << label << ": " << utf8 << endl;
        success = 1;

    } while (0);

    if (utf8)
        OPENSSL_free(utf8);
    seggioChiamante->mutex_stdout.lock();
    if (!success){

        cout << "  " << label << ": <not available>" << endl;
    }
    seggioChiamante->mutex_stdout.unlock();
}

void SSLServer::print_san_name(const char* label, X509* const cert) {
    int success = 0;
    GENERAL_NAMES* names = NULL;
    unsigned char* utf8 = NULL;

    do {
        if (!cert)
            break; // failed

        names = (GENERAL_NAMES*) X509_get_ext_d2i(cert, NID_subject_alt_name, 0,
                                                  0);
        if (!names)
            break;

        int i = 0, count = sk_GENERAL_NAME_num(names);
        if (!count)
            break; // failed

        for (i = 0; i < count; ++i) {
            GENERAL_NAME* entry = sk_GENERAL_NAME_value(names, i);
            if (!entry)
                continue;

            if (GEN_DNS == entry->type) {
                int len1 = 0, len2 = -1;
                //ASN1_STRING_to_UTF8 restiruisce la lunghezza del buffer di out o un valore negativo
                len1 = ASN1_STRING_to_UTF8(&utf8, entry->d.dNSName);
                if (utf8) {
                    len2 = (int) strlen((const char*) utf8);
                }

                if (len1 != len2) {
                    cerr
                            << "  Strlen and ASN1_STRING size do not match (embedded null?): "
                            << len2 << " vs " << len1 << endl;
                }

                // If there's a problem with string lengths, then
                // we skip the candidate and move on to the next.
                // Another policy would be to fails since it probably
                // indicates the client is under attack.
                if (utf8 && len1 && len2 && (len1 == len2)) {
                    //lock_guard<std::mutex> guard(seggioChiamante->mutex_stdout);
                    cout << "  " << label << ": " << utf8 << endl;
                    success = 1;
                }

                if (utf8) {
                    OPENSSL_free(utf8), utf8 = NULL;
                }
            } else {
                cerr << "  Unknown GENERAL_NAME type: " << entry->type << endl;
            }
        }

    } while (0);

    if (names)
        GENERAL_NAMES_free(names);

    if (utf8)
        OPENSSL_free(utf8);

    if (!success){
        seggioChiamante->mutex_stdout.lock();
        cout << "  " << label << ": <not available>\n" << endl;
        seggioChiamante->mutex_stdout.unlock();
    }
}

int SSLServer::verify_callback(int preverify, X509_STORE_CTX* x509_ctx) {

    /*cout << "preverify value: " << preverify <<endl;*/
    int depth = X509_STORE_CTX_get_error_depth(x509_ctx);
    int err = X509_STORE_CTX_get_error(x509_ctx);

    X509* cert = X509_STORE_CTX_get_current_cert(x509_ctx);
    X509_NAME* iname = cert ? X509_get_issuer_name(cert) : NULL;
    X509_NAME* sname = cert ? X509_get_subject_name(cert) : NULL;

    seggioChiamante->mutex_stdout.lock();
    cout << "verify_callback (depth=" << depth << ")(preverify=" << preverify
         << ")" << endl;

    /* Issuer is the authority we trust that warrants nothing useful */
    print_cn_name("Issuer (cn)", iname);

    /* Subject is who the certificate is issued to by the authority  */
    print_cn_name("Subject (cn)", sname);

    if (depth == 0) {
        /* If depth is 0, its the server's certificate. Print the SANs */
        print_san_name("Subject (san)", cert);
    }



    if (preverify == 0) {
        if (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY){

            cout << "  Error = X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY\n";
        }
        else if (err == X509_V_ERR_CERT_UNTRUSTED){

            cout << "  Error = X509_V_ERR_CERT_UNTRUSTED\n";
        }
        else if (err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN){

            cout << "  Error = X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN\n";}
        else if (err == X509_V_ERR_CERT_NOT_YET_VALID) {

            cout << "  Error = X509_V_ERR_CERT_NOT_YET_VALID\n";
        }
        else if (err == X509_V_ERR_CERT_HAS_EXPIRED){

            cout << "  Error = X509_V_ERR_CERT_HAS_EXPIRED\n";
        }
        else if (err == X509_V_OK){

            cout << "  Error = X509_V_OK\n";
        }
        else{

            cout << "  Error = " << err << "\n";
        }
    }
    seggioChiamante->mutex_stdout.unlock();

    return 1;
}

void SSLServer::print_error_string(unsigned long err, const char* const label) {
    const char* const str = ERR_reason_error_string(err);
    if (str)
        fprintf(stderr, "%s\n", str);
    else
        fprintf(stderr, "%s failed: %lu (0x%lx)\n", label, err, err);
}

void SSLServer::cleanup_openssl() {
    EVP_cleanup();
}

void SSLServer::ShowCerts(SSL *ssl) {
    BIO * outbio = BIO_new_fp(stdout, BIO_NOCLOSE);
    X509 *cert = NULL;
    char *line = NULL;
    cert = SSL_get_peer_certificate(ssl); /* Get certificates (if available) */

    seggioChiamante->mutex_stdout.lock();
    ERR_print_errors_fp(stderr);
    if (cert != NULL) {
        BIO_printf(outbio,"Client certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        BIO_printf(outbio,"Subject: %s\n", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        BIO_printf(outbio,"Issuer: %s\n", line);
        free(line);

    } else
        BIO_printf(outbio,"No certificates.\n");
    seggioChiamante->mutex_stdout.unlock();
    X509_free(cert);
    BIO_free_all(outbio);
}

void SSLServer::verify_ClientCert(SSL *ssl) {

    /* ---------------------------------------------------------- *
     * Declare X509 structure                                     *
     * ---------------------------------------------------------- */
    X509 *error_cert = NULL;
    X509 *cert = NULL;
    X509_NAME *certsubject = NULL;
    X509_STORE *store = NULL;
    X509_STORE_CTX *vrfy_ctx = NULL;
    BIO *outbio = NULL,*certbio = NULL;
    X509_NAME *certname = NULL;
    outbio = BIO_new_fp(stdout, BIO_NOCLOSE);
    certbio = BIO_new(BIO_s_file());

    /* ---------------------------------------------------------- *
     * Get the remote certificate into the X509 structure         *
     * ---------------------------------------------------------- */
    cert = SSL_get_peer_certificate(ssl);
    seggioChiamante->mutex_stdout.lock();
    if (cert == NULL){

        BIO_printf(outbio, "Error: Could not get a certificate \n"
                   /*,hostname*/);
    }
    else{

        BIO_printf(outbio, "Retrieved the client's certificate \n"
                   /*,hostname*/);
    }
    seggioChiamante->mutex_stdout.unlock();
    /* ---------------------------------------------------------- *
     * extract various certificate information                    *
     * -----------------------------------------------------------*/
    certname = X509_NAME_new();
    certname = X509_get_subject_name(cert);

    /* ---------------------------------------------------------- *
     * display the cert subject here                              *
     * -----------------------------------------------------------*/
    seggioChiamante->mutex_stdout.lock();
    BIO_printf(outbio, "Displaying the certificate subject data:\n");
    X509_NAME_print_ex(outbio, certname, 0, 0);
    BIO_printf(outbio, "\n");
    seggioChiamante->mutex_stdout.unlock();
    /* ---------------------------------------------------------- *
     * Initialize the global certificate validation store object. *
     * ---------------------------------------------------------- */
    if (!(store = X509_STORE_new())){
        seggioChiamante->mutex_stdout.lock();
        BIO_printf(outbio, "Error creating X509_STORE_CTX object\n");
        seggioChiamante->mutex_stdout.unlock();
    }

    /* ---------------------------------------------------------- *
     * Create the context structure for the validation operation. *
     * ---------------------------------------------------------- */
    vrfy_ctx = X509_STORE_CTX_new();

    /* ---------------------------------------------------------- *
     * Load the certificate and cacert chain from file (PEM).     *
     * ---------------------------------------------------------- */
    int ret;
    /*
         ret = BIO_read_filename(certbio, certFile);
         if (!(cert = PEM_read_bio_X509(certbio, NULL, 0, NULL)))
         BIO_printf(outbio, "Error loading cert into memory\n");
         */
    char chainFile[] =
            "/home/giuseppe/myCA/intermediate/certs/ca-chain.cert.pem";

    ret = X509_STORE_load_locations(store, chainFile, NULL);
    if (ret != 1){
        //lock_guard<std::mutex> guard6(seggioChiamante->mutex_stdout);
        BIO_printf(outbio, "Error loading CA cert or chain file\n");
    }
    /* ---------------------------------------------------------- *
     * Initialize the ctx structure for a verification operation: *
     * Set the trusted cert store, the unvalidated cert, and any  *
     * potential certs that could be needed (here we set it NULL) *
     * ---------------------------------------------------------- */
    X509_STORE_CTX_init(vrfy_ctx, store, cert, NULL);

    /* ---------------------------------------------------------- *
     * Check the complete cert chain can be build and validated.  *
     * Returns 1 on success, 0 on verification failures, and -1   *
     * for trouble with the ctx object (i.e. missing certificate) *
     * ---------------------------------------------------------- */
    ret = X509_verify_cert(vrfy_ctx);
    //lock_guard<std::mutex> guard7(seggioChiamante->mutex_stdout);
    BIO_printf(outbio, "Verification return code: %d\n", ret);

    if (ret == 0 || ret == 1){
        //lock_guard<std::mutex> guard8(seggioChiamante->mutex_stdout);
        BIO_printf(outbio, "Verification result text: %s\n",
                   X509_verify_cert_error_string(vrfy_ctx->error));
    }
    /* ---------------------------------------------------------- *
     * The error handling below shows how to get failure details  *
     * from the offending certificate.                            *
     * ---------------------------------------------------------- */
    if (ret == 0) {
        /*  get the offending certificate causing the failure */
        error_cert = X509_STORE_CTX_get_current_cert(vrfy_ctx);
        certsubject = X509_NAME_new();
        certsubject = X509_get_subject_name(error_cert);
        //lock_guard<std::mutex> guard9(seggioChiamante->mutex_stdout);
        BIO_printf(outbio, "Verification failed cert:\n");
        X509_NAME_print_ex(outbio, certsubject, 0, XN_FLAG_MULTILINE);
        //lock_guard<std::mutex> guard10(seggioChiamante->mutex_stdout);
        BIO_printf(outbio, "\n");
    }

    /* ---------------------------------------------------------- *
     * Free the structures we don't need anymore                  *
     * -----------------------------------------------------------*/
    X509_STORE_CTX_free(vrfy_ctx);
    X509_STORE_free(store);
    X509_free(cert);
    BIO_free_all(certbio);
    BIO_free_all(outbio);
    seggioChiamante->mutex_stdout.lock();
    cout << "Fine --Verify Client Cert --" << endl;
    seggioChiamante->mutex_stdout.unlock();
}

int SSLServer::myssl_fwrite(SSL *ssl, const char * infile) {
    /* legge in modalità binaria il file e lo strasmette sulla socket aperta
     * una SSL_write per comunicare la lunghezza dello stream da inviare
     * una SSL_write per trasmettere il file binario della lunghezza calcolata
     * */
    ifstream is(infile, std::ifstream::binary);
    if (is) {
        // get length of file:
        is.seekg(0, is.end);
        int length = is.tellg();
        is.seekg(0, is.beg);

        char * buffer = new char[length];

        //lock_guard<std::mutex> guard1(seggioChiamante->mutex_stdout);
        cout << "Reading " << length << " characters... ";
        // read data as a block:
        is.read(buffer, length);

        if (is){
            //lock_guard<std::mutex> guard2(seggioChiamante->mutex_stdout);
            cout << "all characters read successfully." << endl;
        }
        else{
            // lock_guard<std::mutex> guard3(seggioChiamante->mutex_stdout);
            cout << "error: only " << is.gcount() << " could be read";
        }
        is.close();


        // ...buffer contains the entire file...
        stringstream strs;
        strs << length;
        string temp_str = strs.str();
        const char *info = temp_str.c_str();
        cout << "bytes to send:" << info << endl;
        SSL_write(ssl, info, strlen(info));
        SSL_write(ssl, buffer, length);
        delete[] buffer;
        return 1;

    }
    else{
        //lock_guard<std::mutex> guard4(seggioChiamante->mutex_stdout);
        cout << "file unreadable" << endl;
    }
    return 0;
}

void SSLServer::stopServer(){
    this->stop=true;
}

