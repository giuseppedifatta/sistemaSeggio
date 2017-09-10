#include "sslserver.h"

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



# define LOCALHOST "localhost"

#define PORT "4433"

using namespace std;

SSLServer::SSLServer(Seggio *s){


    this->stopServer = false;
    this->seggioChiamante = s;
    seggioChiamante->mutex_stdout.lock();
    cout << "ServerSeggio: Costruttore!" << endl;
    seggioChiamante->mutex_stdout.unlock();

    this->init_openssl_library();

    this->createServerContext();

    char certFile[] =
            "/home/giuseppe/myCA/intermediate/certs/localhost.cert.pem";
    char keyFile[] =
            "/home/giuseppe/myCA/intermediate/private/localhost.key.pem";
    char chainFile[] =
            "/home/giuseppe/myCA/intermediate/certs/ca-chain.cert.pem";

    configure_context(certFile, keyFile, chainFile);

    seggioChiamante->mutex_stdout.lock();
    cout << "ServerSeggio: Context configured" << endl;
    seggioChiamante->mutex_stdout.unlock();
    this->openListener(atoi(PORT));
    this->outbio = BIO_new_fp(stdout, BIO_NOCLOSE);

}

SSLServer::~SSLServer(){
    //nel distruttore
    cout << "ServerSeggio: Distruttore!" << endl;
    if(close(this->listen_sock) != 0)
        cerr << "ServerSeggio: errore di chiusura listen socket" << endl;

    BIO_free_all(this->outbio);
    SSL_CTX_free(this->ctx);

    //pericolosa, cancella gli algoritmi e non funziona più nulla
    this->cleanup_openssl();



}

void SSLServer::ascoltoNuovoStatoPV(){

    //inizializza una socket per il client
    struct sockaddr_in client_addr;
    uint len = sizeof(client_addr);

    seggioChiamante->mutex_stdout.lock();
    cout << "ServerSeggio: in ascolto sulla porta " << PORT << ", attesa connessione da un client PV...\n";
    seggioChiamante->mutex_stdout.unlock();

    // accept restituisce un valore negativo in caso di insuccesso
    int client_sock = accept(this->listen_sock, (struct sockaddr*) &client_addr, &len);

    if (client_sock < 0) {
        perror("Unable to accept");
        exit(EXIT_FAILURE);
    }
    else{
        seggioChiamante->mutex_stdout.lock();
        cout << "ServerSeggio: Un client ha iniziato la connessione su una socket con fd:" << client_sock << endl;
        cout<< "ServerSeggio: Client's Port assegnata: "<< ntohs(client_addr.sin_port)<< endl;
        seggioChiamante->mutex_stdout.unlock();

    }

    if(!(this->stopServer)){

        //se non è stata settata l'interruzione del server, lancia il thread per servire la richiesta
        thread t (&SSLServer::Servlet, this, client_sock, servizi::aggiornamentoPV);
        t.detach();
        seggioChiamante->mutex_stdout.lock();
        cout << "ServerSeggio: start a thread..." << endl;
        seggioChiamante->mutex_stdout.unlock();
    }
    else{
        //termina l'ascolto
        seggioChiamante->mutex_stdout.lock();
        cout << "ServerSeggio: interruzione del server in corso..." << endl;
        seggioChiamante->mutex_stdout.unlock();
        int ab = close(client_sock);
        if(ab ==0){
            cout << "ServerSeggio: successo chiusura socket per il client" << endl;
        }
        //        ab = close(this->listen_sock);
        //        if(ab ==0){
        //            cout << "ServerSeggio: successo chiusura socket del listener" << endl;
        //        }
        return;
    }

}

void SSLServer::Servlet(int client_sock_fd ,servizi servizio) {/* Serve the connection -- threadable */
    seggioChiamante->mutex_stdout.lock();
    cout << "ServerSeggioThread: Servlet: inizio servlet" << endl;
    seggioChiamante->mutex_stdout.unlock();

    SSL * ssl = SSL_new(ctx);
    if(!ssl) exit(1);

    //configurara ssl per collegarsi sulla socket indicata
    SSL_set_fd(ssl, client_sock_fd);

    int ret = SSL_accept(ssl);
    if ( ret <= 0) {/* do SSL-protocol handshake */
        cout << "ServerSeggioThread: error in handshake" << endl;
        ERR_print_errors_fp(stderr);

    }
    else {
        seggioChiamante->mutex_stdout.lock();
        cout << "ServerSeggioThread: handshake ok!" << endl;
        seggioChiamante->mutex_stdout.unlock();
        this->ShowCerts(ssl);
        this->verify_ClientCert(ssl);

        seggioChiamante->mutex_stdout.lock();
        cout << "ServerSeggioThread: service starting..." << endl;
        seggioChiamante->mutex_stdout.unlock();
        this->service(ssl,servizio);
    }




    //chiusura connessione
    //send close notify
    //    int ret = SSL_shutdown(ssl);
    //    if(ret == 0){ // valore di ritorno uguale a 0 indica che lo shutdown non si è concluso , bisogna richiamare la SSL_shutdown una seconda volta
    //        SSL_shutdown(ssl); //if not done, resent close notify
    //        cout << " richiamo la shutdown" << endl;
    //    }
    //    else if(ret == -1){
    //        cout << "Fatal error: " << SSL_get_error(ssl,ret) << endl;
    //    }


    SSL_shutdown(ssl);
    SSL_free(ssl);

    if(close(client_sock_fd) != 0) {
        cerr << "ClientSeggio: errore chiusura socket del client" << endl;
    }
    seggioChiamante->mutex_stdout.lock();
    cout << "ServerSeggioThread: fine servlet" << endl;
    seggioChiamante->mutex_stdout.unlock();
}

void SSLServer::service(SSL * ssl,servizi servizio) {
    seggioChiamante->mutex_stdout.lock();
    cout << "ServerSeggioThread: service started: " << servizio << endl;
    seggioChiamante->mutex_stdout.unlock();
    char buf[128];

    memset(buf, '\0', sizeof(buf));
    //memset(cod_service, '\0', sizeof(cod_service));

    int bytes;

    switch (servizio) {

    case servizi::aggiornamentoPV:{
        unsigned int idPV = 0;
        bytes = SSL_read(ssl, buf, sizeof(buf));
        if (bytes > 0){
            //cout << "Received buffer: " << buf << endl;
            idPV = atoi(buf);
            seggioChiamante->mutex_stdout.lock();
            cout << "ServerSeggioThread: idPV: " << idPV << endl;
            seggioChiamante->mutex_stdout.unlock();
        }


        memset(buf, '\0', sizeof(buf));

        unsigned int nuovoStato = 0;
        bytes = SSL_read(ssl, buf, sizeof(buf));
        if (bytes>0){
            //cout << "Received buffer: " << buf << endl;
            nuovoStato = atoi(buf);
            seggioChiamante->mutex_stdout.lock();
            cout << "ServerSeggioThread: nuovoStato: " << nuovoStato << endl;
            seggioChiamante->mutex_stdout.unlock();
        }


        //chiamo la funzione per aggiornare lo stato della postazione
        seggioChiamante->setPVstate(idPV,nuovoStato);
        break;
    }


    }

    return;

}

void SSLServer::openListener(int s_port) {

    // non è specifico per openssl, crea una socket in ascolto su una porta passata come argomento
    int  r;

    struct sockaddr_in sa_serv;
    this->listen_sock = socket(PF_INET, SOCK_STREAM, 0);

    //allow reuse of port without dealy for TIME_WAIT
    int iSetOption = 1;
    setsockopt(this->listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&iSetOption,
               sizeof(iSetOption));

    if (this->listen_sock <= 0) {
        perror("Unable to create socket");
        abort();
    }

    memset(&sa_serv, 0, sizeof(sa_serv));
    sa_serv.sin_family = AF_INET;
    sa_serv.sin_addr.s_addr = INADDR_ANY;
    sa_serv.sin_port = htons(s_port); /* Server Port number */
    cout<< "ServerSeggio: Server's Port: "<< ntohs(sa_serv.sin_port)<<endl;

    r = bind(this->listen_sock, (struct sockaddr*) &sa_serv, sizeof(sa_serv));
    if (r < 0) {
        perror("Unable to bind");
        exit(EXIT_FAILURE);
    }

    // Receive a TCP connection.
    r = listen(this->listen_sock, 10);

    if (r < 0) {
        perror("Unable to listen");
        exit(EXIT_FAILURE);
    }
    //return this->listen_sock;
}


void SSLServer::createServerContext() {
    const SSL_METHOD *method;
    method = TLSv1_2_server_method();

    this->ctx = SSL_CTX_new(method);
    if (!this->ctx) {
        perror("ServerSeggio::Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }


    const long flags = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;
    /*long old_opts = */SSL_CTX_set_options(this->ctx, flags);
    //    seggioChiamante->mutex_stdout.lock();
    //    cout << "ServerSeggio: bitmask options: " << old_opts << endl;
    //    seggioChiamante->mutex_stdout.unlock();


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
        fprintf(stderr, "ServerSeggio: Private key does not match the public certificate\n");
        abort();
    }
    //substitute NULL with the name of the specific verify_callback
    SSL_CTX_set_verify(this->ctx, SSL_VERIFY_PEER, NULL);

}

void SSLServer::init_openssl_library() {
    //SSL_library_init();


    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();


    /* OpenSSL_config may or may not be called internally, based on */
    /*  some #defines and internal gyrations. Explicitly call it    */
    /*  *IF* you need something from openssl.cfg, such as a         */
    /*  dynamically configured ENGINE.                              */
    //OPENSSL_config(NULL);

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


        cout << "ServerSeggioThread:   " << label << ": " << utf8 << endl;
        success = 1;

    } while (0);

    if (utf8)
        OPENSSL_free(utf8);
    seggioChiamante->mutex_stdout.lock();
    if (!success){

        cout << "ServerSeggioThread:   " << label << ": <not available>" << endl;
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
                            << "ServerSeggioThread:  Strlen and ASN1_STRING size do not match (embedded null?): "
                            << len2 << " vs " << len1 << endl;
                }

                // If there's a problem with string lengths, then
                // we skip the candidate and move on to the next.
                // Another policy would be to fails since it probably
                // indicates the client is under attack.
                if (utf8 && len1 && len2 && (len1 == len2)) {
                    //lock_guard<std::mutex> guard(seggioChiamante->mutex_stdout);
                    cout << "ServerSeggioThread:   " << label << ": " << utf8 << endl;
                    success = 1;
                }

                if (utf8) {
                    OPENSSL_free(utf8), utf8 = NULL;
                }
            } else {
                cerr << "ServerSeggioThread:  Unknown GENERAL_NAME type: " << entry->type << endl;
            }
        }

    } while (0);

    if (names)
        GENERAL_NAMES_free(names);

    if (utf8)
        OPENSSL_free(utf8);

    if (!success){
        seggioChiamante->mutex_stdout.lock();
        cout << "ServerSeggioThread:   " << label << ": <not available>\n" << endl;
        seggioChiamante->mutex_stdout.unlock();
    }
}

int SSLServer::verify_callback(int preverify, X509_STORE_CTX* x509_ctx) {

    /*cout << "ServerSeggioThread: preverify value: " << preverify <<endl;*/
    int depth = X509_STORE_CTX_get_error_depth(x509_ctx);
    int err = X509_STORE_CTX_get_error(x509_ctx);

    X509* cert = X509_STORE_CTX_get_current_cert(x509_ctx);
    X509_NAME* iname = cert ? X509_get_issuer_name(cert) : NULL;
    X509_NAME* sname = cert ? X509_get_subject_name(cert) : NULL;

    seggioChiamante->mutex_stdout.lock();
    cout << "ServerSeggioThread: verify_callback (depth=" << depth << ")(preverify=" << preverify
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

            cout << "ServerSeggioThread:   Error = X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY\n";
        }
        else if (err == X509_V_ERR_CERT_UNTRUSTED){

            cout << "ServerSeggioThread:   Error = X509_V_ERR_CERT_UNTRUSTED\n";
        }
        else if (err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN){

            cout << "ServerSeggioThread:   Error = X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN\n";}
        else if (err == X509_V_ERR_CERT_NOT_YET_VALID) {

            cout << "ServerSeggioThread:   Error = X509_V_ERR_CERT_NOT_YET_VALID\n";
        }
        else if (err == X509_V_ERR_CERT_HAS_EXPIRED){

            cout << "ServerSeggioThread:   Error = X509_V_ERR_CERT_HAS_EXPIRED\n";
        }
        else if (err == X509_V_OK){

            cout << "ServerSeggioThread:   Error = X509_V_OK\n";
        }
        else{

            cout << "ServerSeggioThread:   Error = " << err << "\n";
        }
    }
    seggioChiamante->mutex_stdout.unlock();

    return 1;
}

void SSLServer::print_error_string(unsigned long err, const char* const label) {
    const char* const str = ERR_reason_error_string(err);
    if (str)
        fprintf(stderr, "ServerSeggioThread: %s\n", str);
    else
        fprintf(stderr, "ServerSeggioThread: %s failed: %lu (0x%lx)\n", label, err, err);
}

void SSLServer::cleanup_openssl() {
    EVP_cleanup();
}

void SSLServer::ShowCerts(SSL *ssl) {

    X509 *cert = NULL;
    char *line = NULL;
    cert = SSL_get_peer_certificate(ssl); /* Get certificates (if available) */

    seggioChiamante->mutex_stdout.lock();
    ERR_print_errors_fp(stderr);
    if (cert != NULL) {
        BIO_printf(this->outbio,"Client certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        BIO_printf(this->outbio,"Subject: %s\n", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        BIO_printf(this->outbio,"Issuer: %s\n", line);
        free(line);

    } else
        BIO_printf(this->outbio,"No certificates.\n");
    seggioChiamante->mutex_stdout.unlock();
    X509_free(cert);

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
    //BIO *certbio = NULL;
    //X509_NAME *certname = NULL;
    //certbio = BIO_new(BIO_s_file());

    /* ---------------------------------------------------------- *
     * Get the remote certificate into the X509 structure         *
     * ---------------------------------------------------------- */
    cert = SSL_get_peer_certificate(ssl);
    seggioChiamante->mutex_stdout.lock();
    if (cert == NULL){

        BIO_printf(this->outbio, "ServerSeggioThread: Error: Could not get a certificate \n"
                   /*,hostname*/);
    }
    else{

        BIO_printf(this->outbio, "ServerSeggioThread: Retrieved the client's certificate \n"
                   /*,hostname*/);
    }
    seggioChiamante->mutex_stdout.unlock();
    /* ---------------------------------------------------------- *
     * extract various certificate information                    *
     * -----------------------------------------------------------*/
    //certname = X509_NAME_new();
    //certname = X509_get_subject_name(cert);

    /* ---------------------------------------------------------- *
     * display the cert subject here                              *
     * -----------------------------------------------------------*/
    //    seggioChiamante->mutex_stdout.lock();
    //    BIO_printf(this->outbio, "ServerSeggioThread: Displaying the certificate subject data:\n");
    //    seggioChiamante->mutex_stdout.unlock();

    //X509_NAME_print_ex(this->outbio, certname, 0, 0);

    //    seggioChiamante->mutex_stdout.lock();
    //    BIO_printf(this->outbio, "\n");
    //    seggioChiamante->mutex_stdout.unlock();
    /* ---------------------------------------------------------- *
     * Initialize the global certificate validation store object. *
     * ---------------------------------------------------------- */
    if (!(store = X509_STORE_new())){
        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio, "ServerSeggioThread: Error creating X509_STORE_CTX object\n");
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
         BIO_printf(this->outbio, "ServerSeggioThread: Error loading cert into memory\n");
         */
    char chainFile[] =
            "/home/giuseppe/myCA/intermediate/certs/ca-chain.cert.pem";

    ret = X509_STORE_load_locations(store, chainFile, NULL);
    if (ret != 1){
        BIO_printf(this->outbio, "ServerSeggioThread: Error loading CA cert or chain file\n");
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
    BIO_printf(this->outbio, "ServerSeggioThread: Verification return code: %d\n", ret);

    if (ret == 0 || ret == 1){
        //lock_guard<std::mutex> guard8(seggioChiamante->mutex_stdout);
        BIO_printf(this->outbio, "ServerSeggioThread: Verification result text: %s\n",
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
        BIO_printf(this->outbio, "ServerSeggioThread: Verification failed cert:\n");
        X509_NAME_print_ex(this->outbio, certsubject, 0, XN_FLAG_MULTILINE);
        BIO_printf(this->outbio, "\n");
    }

    /* ---------------------------------------------------------- *
     * Free the structures we don't need anymore                  *
     * -----------------------------------------------------------*/
    X509_STORE_CTX_free(vrfy_ctx);
    X509_STORE_free(store);
    X509_free(cert);

    //BIO_free_all(certbio);

    seggioChiamante->mutex_stdout.lock();
    cout << "ServerSeggioThread: Fine --Verify Client Cert --" << endl;
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
        cout << "ServerSeggioThread: Reading " << length << " characters... ";
        // read data as a block:
        is.read(buffer, length);

        if (is){
            //lock_guard<std::mutex> guard2(seggioChiamante->mutex_stdout);
            cout << "ServerSeggioThread: all characters read successfully." << endl;
        }
        else{
            // lock_guard<std::mutex> guard3(seggioChiamante->mutex_stdout);
            cout << "ServerSeggioThread: error: only " << is.gcount() << " could be read";
        }
        is.close();


        // ...buffer contains the entire file...
        stringstream strs;
        strs << length;
        string temp_str = strs.str();
        const char *info = temp_str.c_str();
        cout << "ServerSeggioThread: bytes to send:" << info << endl;
        SSL_write(ssl, info, strlen(info));
        SSL_write(ssl, buffer, length);
        //delete[] buffer;
        return 1;
    }
    else{
        //lock_guard<std::mutex> guard4(seggioChiamante->mutex_stdout);
        cout << "ServerSeggioThread: file unreadable" << endl;
    }
    return 0;
}

void SSLServer::setStopServer(bool value){

    //proteggere con un mutex
    this->stopServer=value;
}
