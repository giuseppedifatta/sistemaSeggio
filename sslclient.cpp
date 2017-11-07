#include "sslclient.h"

/*
 * sslClient.h
 *
 *  Created on: 02/apr/2017
 *      Author: giuseppe
 */



#define SERVER_PORT "4433"
#define LOCALHOST "127.0.0.1"

SSLClient::SSLClient(Seggio * s){
    //this->hostname = IP_PV;
    this->seggioChiamante = s;

    /* ---------------------------------------------------------- *
     * Create the Input/Output BIO's.                             *
     * ---------------------------------------------------------- */
    this->outbio = BIO_new_fp(stdout, BIO_NOCLOSE);
    this->ssl = nullptr;

    /* ---------------------------------------------------------- *
     * Function that initialize openssl for correct work.		  *
     * ---------------------------------------------------------- */
    this->init_openssl_library();

    createClientContext();

    string pathCertFilePem = getConfig("clientCertPem");
    const char * certFile = pathCertFilePem.c_str();

    string pathKeyFilePem = getConfig("clientKeyPem");
    const char * keyFile = pathKeyFilePem.c_str();

    string pathChainFilePem = getConfig("chainFilePem");
    const char * chainFile = pathChainFilePem.c_str();

    this->configure_context(certFile, keyFile, chainFile);
    seggioChiamante->mutex_stdout.lock();
    cout << "ClientSeggio: context configured" << endl;
    seggioChiamante->mutex_stdout.unlock();
}

SSLClient::~SSLClient(){
    /* ---------------------------------------------------------- *
     * Free the structures we don't need anymore                  *
     * -----------------------------------------------------------*/
    BIO_free_all(this->outbio);
    SSL_CTX_free(this->ctx);

    //usare con cura, cancella gli algoritmi e non funziona più nulla
    // this->cleanup_openssl();

}

int SSLClient::create_socket(/*const char * hostIP*//*hostname*/const char * port) {
    /* ---------------------------------------------------------- *
     * create_socket() creates the socket & TCP-connect to server *
     * non specifica per SSL                                      *
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
    /* decommentare la sezione se si passa all'hostname invece dell'ip dell'host
    dest_addr.sin_addr.s_addr = *(long*) (host->h_addr);
    */

    // commentare la riga sotto se non si vuole usare l'ip dell'host, ma l'hostname
    dest_addr.sin_addr.s_addr = inet_addr(this->PV_IPaddress);

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

    if (res  == -1) {
        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio, "ClientSeggio: Error: Cannot connect to host %s on port %d.\n",
                    address_printable, portCod);
        seggioChiamante->mutex_stdout.unlock();


    }
    else {
        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio, "ClientSeggio: Successfully connect to host %s on port %d.\n",
                   address_printable, portCod);
        cout << "ClientSeggio: Descrittore socket: " << this->server_sock << endl;
        seggioChiamante->mutex_stdout.unlock();

    }

    return res;
}

SSL * SSLClient::connectTo(const char* hostIP/*hostname*/){
    seggioChiamante->mutex_stdout.lock();
    cout << "ClientSeggio: Connecting to " << hostIP << endl;
    seggioChiamante->mutex_stdout.unlock();

    this->PV_IPaddress = hostIP;

    const char  port[] = SERVER_PORT;

    /* ---------------------------------------------------------- *
     * Create new SSL connection state object                     *
     * ---------------------------------------------------------- */
    this->ssl = SSL_new(this->ctx);
    //    seggioChiamante->mutex_stdout.lock();
    //    cout << "ClientSeggio: ssl pointer: " << this->ssl << endl;
    //    seggioChiamante->mutex_stdout.unlock();


    /* ---------------------------------------------------------- *
     * Make the underlying TCP socket connection                  *
     * ---------------------------------------------------------- */
    int ret = create_socket(/*this->PV_IPaddress *//*hostname,*/port);


    if (ret == 0){

        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio,"ClientSeggio: Successfully create the socket for TCP connection to: %s \n",
                   this->PV_IPaddress /*hostname*/);
        seggioChiamante->mutex_stdout.unlock();
    }
    else {


        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio,"ClientSeggio: Unable to create the socket for TCP connection to: %s \n",
                   this->PV_IPaddress /*hostname*/);
        seggioChiamante->mutex_stdout.unlock();
        SSL_free(this->ssl);
        this->ssl = nullptr;
        if(close(this->server_sock) == 0){
            cerr << "ClientSeggio: chiusura 1 socket server" << endl;
        }
        return this->ssl;
    }

    /* ---------------------------------------------------------- *
     * Attach the SSL session to the socket descriptor            *
     * ---------------------------------------------------------- */

    if (SSL_set_fd(this->ssl, this->server_sock) != 1) {

        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio, "ClientSeggio: Error: Connection to %s failed \n", this->PV_IPaddress /*hostname*/);
        seggioChiamante->mutex_stdout.unlock();

        SSL_free(this->ssl);
        this->ssl = nullptr;
        if(close(this->server_sock) == 0){
            cerr << "ClientSeggio: chiusura 2 socket server" << endl;
        }
        return this->ssl;
    }
    else
        seggioChiamante->mutex_stdout.lock();
    BIO_printf(this->outbio, "ClientSeggio: Ok: Connection to %s \n", this->PV_IPaddress /*hostname*/);
    seggioChiamante->mutex_stdout.unlock();
    /* ---------------------------------------------------------- *
     * Try to SSL-connect here, returns 1 for success             *
     * ---------------------------------------------------------- */
    ret = SSL_connect(this->ssl);
    if (ret != 1){ //SSL handshake

        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio, "ClientSeggio: Error: Could not build a SSL session to: %s\n",
                   this->PV_IPaddress /*hostname*/);
        seggioChiamante->mutex_stdout.unlock();

        SSL_free(this->ssl);
        this->ssl = nullptr;
        if(close(this->server_sock) == 0){
            cerr << "ClientSeggio: chiusura 3 socket server" << endl;
        }
        return this->ssl;
    }
    else{
        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio, "ClientSeggio: Successfully enabled SSL/TLS session to: %s\n",
                   this->PV_IPaddress /*hostname*/);
        seggioChiamante->mutex_stdout.unlock();
    }
    this->ShowCerts();
    this->verify_ServerCert(/*this->PV_IPaddress *//*hostname*/);

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

void SSLClient::verify_ServerCert(/*const char * hostIPhostname*/) {
    // Declare X509 structure
    X509 *error_cert = NULL;
    X509 *peer_cert = NULL;
    X509_NAME *certsubject = NULL;
    X509_STORE *store = NULL;
    X509_STORE_CTX *vrfy_ctx = NULL;
    //X509_NAME *certname = NULL;
    int ret;
    //BIO *certbio = NULL;
    //certbio = BIO_new(BIO_s_file());


    // Get the remote certificate into the X509 structure

    peer_cert = SSL_get_peer_certificate(this->ssl);
    if (peer_cert == NULL){
        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio, "ClientSeggio: Error: Could not get a certificate from: %s.\n",
                   this->PV_IPaddress/*hostname*/);
        seggioChiamante->mutex_stdout.unlock();
    }
    else{
        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio, "ClientSeggio: Retrieved the server's certificate from: %s.\n",
                   this->PV_IPaddress/*hostname*/);
        seggioChiamante->mutex_stdout.unlock();
    }

    // extract various certificate information

    //certname = X509_NAME_new();
    //certname = X509_get_subject_name(peer_cert);

    // display the cert subject here

    //    BIO_printf(this->outbio, "ClientSeggio: Displaying the certificate subject data:\n");
    //    X509_NAME_print_ex(this->outbio, certname, 0, 0);
    //    BIO_printf(this->outbio, "\n");

    //Initialize the global certificate validation store object.
    if (!(store = X509_STORE_new())){
        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio, "ClientSeggio: Error creating X509_STORE_CTX object\n");
        seggioChiamante->mutex_stdout.unlock();
    }
    // Create the context structure for the validation operation.
    vrfy_ctx = X509_STORE_CTX_new();

    // Load the certificate and cacert chain from file (PEM).
    /*
         ret = BIO_read_filename(certbio, certFile);
         if (!(cert = PEM_read_bio_X509(certbio, NULL, 0, NULL)))
         BIO_printf(this->outbio, "ClientSeggio: Error loading cert into memory\n");
         */
    char chainFile[] =
            "/home/giuseppe/myCA/intermediate/certs/ca-chain.cert.pem";

    ret = X509_STORE_load_locations(store, chainFile, NULL);
    if (ret != 1){
        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio, "ClientSeggio: Error loading CA cert or chain file\n");
        seggioChiamante->mutex_stdout.unlock();
    }
    /* Initialize the ctx structure for a verification operation:
      Set the trusted cert store, the unvalidated cert, and any  *
     * potential certs that could be needed (here we set it NULL) */
    X509_STORE_CTX_init(vrfy_ctx, store, peer_cert, NULL);

    /* Check the complete cert chain can be build and validated.  *
     * Returns 1 on success, 0 on verification failures, and -1   *
     * for trouble with the ctx object (i.e. missing certificate) */
    ret = X509_verify_cert(vrfy_ctx);

    seggioChiamante->mutex_stdout.lock();
    BIO_printf(this->outbio, "ClientSeggio: Verification return code: %d\n", ret);
    seggioChiamante->mutex_stdout.unlock();

    if (ret == 0 || ret == 1){
        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio, "ClientSeggio: Verification result text: %s\n",
                   X509_verify_cert_error_string(vrfy_ctx->error));
        seggioChiamante->mutex_stdout.unlock();
    }

    /* The error handling below shows how to get failure details  *
     * from the offending certificate.                            */
    if (ret == 0) {
        //get the offending certificate causing the failure
        error_cert = X509_STORE_CTX_get_current_cert(vrfy_ctx);
        certsubject = X509_NAME_new();
        certsubject = X509_get_subject_name(error_cert);
        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio, "ClientSeggio: Verification failed cert:\n");
        X509_NAME_print_ex(this->outbio, certsubject, 0, XN_FLAG_MULTILINE);
        BIO_printf(this->outbio, "\n");
        seggioChiamante->mutex_stdout.unlock();
    }

    // Free up all structures need for verify certs
    X509_STORE_CTX_free(vrfy_ctx);
    X509_STORE_free(store);
    X509_free(peer_cert);
    //X509_NAME_free(certname);

    //BIO_free_all(certbio);

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
    if ((this->ctx = SSL_CTX_new(method)) == NULL){
        seggioChiamante->mutex_stdout.lock();
        BIO_printf(this->outbio, "ClientSeggio: Unable to create a new SSL context structure.\n");
        seggioChiamante->mutex_stdout.unlock();
    }
    /* ---------------------------------------------------------- *
     * Disabling SSLv2 and SSLv3 will leave TSLv1 for negotiation    *
     * ---------------------------------------------------------- */
    SSL_CTX_set_options(this->ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
}

void SSLClient::configure_context(const char* CertFile,const char* KeyFile,const char * ChainFile) {
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
        fprintf(stderr, "ClientSeggio: Private key does not match the public certificate\n");
        abort();
    }

}

void SSLClient::cleanup_openssl() {
    EVP_cleanup();
}

void SSLClient::stopLocalServer(/*const char* localhosthostname*/){
    //questa funzione contatta il server locale, ma non deve fare alcuna operazione se non quella
    //di sbloccare il server locale dallo stato di attesa di una nuova connessione, così da portare
    //al ricontrollo della condizione del while che se falsa, porta
    //all'interruzione del thread chiamante
    const char * port = SERVER_PORT;

    //la creazione della socket sblocca il server locale dall'accept della connessione tcp
    this->PV_IPaddress="127.0.0.1"; //forziamo l'host a cui collegarsi a localhost
    create_socket(/*hostname,*/ port);

    // avendo impostato a true la variabile bool stopServer, non verrà inizializzata la connessione ssl
    // si passa direttamente alla chiusura delle socket
    seggioChiamante->mutex_stdout.lock();
    cout << "ClientSeggio: niente da fare... chiudo la socket per il server" << endl;
    seggioChiamante->mutex_stdout.unlock();
    if(close(this->server_sock) != 0)
    {
        cerr << "ClientSeggio: errore chiusura socket server" << endl;
    }

}

bool SSLClient::querySetAssociation(string snHT, unsigned int idTipoVotante, uint matricola , string usernameHT, string passwordHT){
    bool res = false;
    //invia codice del servizio richiesto al PV_Server
    int serviceCod = serviziPV::setAssociation;
    stringstream ssCod;
    ssCod << serviceCod;
    string strCod = ssCod.str();
    const char * charCod = strCod.c_str();
    seggioChiamante->mutex_stdout.lock();
    cout << "ClientSeggio: richiedo il servizio: " << charCod << endl;
    seggioChiamante->mutex_stdout.unlock();
    SSL_write(ssl,charCod,strlen(charCod));


    //invio snHT da associare alla postazione di voto per l'avvio della funzionalità di abilitazione al voto
    this->sendString_SSL(ssl,snHT);
    seggioChiamante->mutex_stdout.lock();
    cout << "ClientSeggio: Id Hardware token da associare alla PV: " << snHT << endl;
    seggioChiamante->mutex_stdout.unlock();


    //invio idTipoVotante
    this->sendString_SSL(ssl,to_string(idTipoVotante));

    //invio matricola votante
    this->sendString_SSL(ssl,to_string(matricola));

    //TODO invio authenticationUsernameHT
    this->sendString_SSL(ssl,usernameHT);

    //TODO invio authenticationPasswordHT
    this->sendString_SSL(ssl,passwordHT);

    //ricevi esito dell'operazione
    // 0 -> success
    // 1 -> error
    string buffer;
    if(receiveString_SSL(ssl,buffer)!=0){
        int result = atoi(buffer.c_str());
        seggioChiamante->mutex_stdout.lock();
        cout << "ClientSeggio: Risultato associazione: " << result << endl;
        seggioChiamante->mutex_stdout.unlock();
        if (result == 0){
            res = true;
        }
        else{

            res = false; //configurazione associazione non riuscita
        }

    }else{
        cerr << "ClientSeggio: Errore ricezione del risultato associazione" << endl;
    }


    seggioChiamante->mutex_stdout.lock();
    BIO_printf(this->outbio, "ClientSeggio: Finished SSL/TLS connection with server: %s.\n",
               this->PV_IPaddress);
    seggioChiamante->mutex_stdout.unlock();

    if(SSL_shutdown(this->ssl) == 0){ // valore di ritorno uguale a 0 indica che lo shutdown non si è concluso , bisogna richiamare la SSL_shutdown una seconda volta
        SSL_shutdown(this->ssl);
    }

    SSL_free(this->ssl);

    if(close(this->server_sock) != 0){
        cerr << "ClientSeggio: errore chiusura socket per il server" << endl;
    }

    return res;
}

int SSLClient::queryPullPVState(){
    //invia codice del servizio richiesto al PV_Server
    int serviceCod = serviziPV::pullPVState;
    stringstream ssCod;
    ssCod << serviceCod;
    string strCod = ssCod.str();
    const char * charCod = strCod.c_str();
    seggioChiamante->mutex_stdout.lock();
    cout << "ClientSeggio: richiedo il servizio: " << charCod << endl;
    seggioChiamante->mutex_stdout.unlock();
    SSL_write(ssl,charCod,strlen(charCod));

    //do stuff
    int statoCorrente = 0;
    char buf[16];
    memset(buf, '\0', sizeof(buf));
    int bytes = SSL_read(ssl, buf, sizeof(buf));
    if (bytes>0){
        //cout << "ClientSeggio: Received buffer: " << buf << endl;
        statoCorrente = atoi(buf);

        seggioChiamante->mutex_stdout.lock();
        cout << "ClientSeggio: statoCorrente: " << statoCorrente << endl;
        seggioChiamante->mutex_stdout.unlock();
    }
    else{
        cerr << "ClientSeggio: Errore ricezione dello stato corrente" << endl;
        statoCorrente = seggioChiamante->statiPV::non_raggiungibile;
    }


    //end do stuff

    //chiusura connessione
    seggioChiamante->mutex_stdout.lock();
    BIO_printf(this->outbio, "ClientSeggio: Finished SSL/TLS connection with server: %s.\n",
               this->PV_IPaddress);
    seggioChiamante->mutex_stdout.unlock();
    SSL_shutdown(this->ssl);
    SSL_free(this->ssl);
    if(close(this->server_sock) != 0){
        cerr << "ClientSeggio: errore chiusura socket per il server" << endl;
    }

    return statoCorrente;
}

bool SSLClient::queryRemoveAssociation() {
    bool res = false;
    //invia codice del servizio richiesto al PV_Server
    int serviceCod = serviziPV::removeAssociation;
    stringstream ssCod;
    ssCod << serviceCod;
    string strCod = ssCod.str();
    const char * charCod = strCod.c_str();
    seggioChiamante->mutex_stdout.lock();
    cout << "ClientSeggio: richiedo il servizio: " << charCod << endl;
    seggioChiamante->mutex_stdout.unlock();
    SSL_write(ssl,charCod,strlen(charCod));

    //ricevi esito dell'operazione
    // 0 -> success
    // 1 -> error
    int bytes;
    char buffer[8];
    memset(buffer, '\0', sizeof(buffer));
    bytes = SSL_read(ssl,buffer,sizeof(buffer));
    if(bytes > 0){
        buffer[bytes] = 0;
        int result = atoi(buffer);

        seggioChiamante->mutex_stdout.lock();
        cout << "ClientSeggio: Risultato rimozione: " << result << endl;
        seggioChiamante->mutex_stdout.unlock();

        if (result == 0){
            res = true;
        }

    }
    else{
        cerr << "ClientSeggio: Errore ricezione del risultato rimozione associazione" << endl;

    }

    //chiusura connessione
    seggioChiamante->mutex_stdout.lock();
    BIO_printf(this->outbio, "ClientSeggio: Finished SSL/TLS connection with server: %s.\n",
               this->PV_IPaddress);
    seggioChiamante->mutex_stdout.unlock();

    if(SSL_shutdown(this->ssl) == 0){ // valore di ritorno uguale a 0 indica che lo shutdown non si è concluso , bisogna richiamare la SSL_shutdown una seconda volta
        SSL_shutdown(this->ssl);
    }


    SSL_free(this->ssl);

    if(close(this->server_sock) != 0) {
        cerr << "ClientSeggio: errore chiusura socket per il server" << endl;
    }

    return res;
}

bool SSLClient::queryFreePV(){
    //invia codice del servizio richiesto al PV_Server
    int serviceCod = serviziPV::freePV;
    stringstream ssCod;
    ssCod << serviceCod;
    string strCod = ssCod.str();
    const char * charCod = strCod.c_str();

    seggioChiamante->mutex_stdout.lock();
    cout << "ClientSeggio:richiedo il servizio: " << charCod << endl;
    seggioChiamante->mutex_stdout.unlock();

    SSL_write(ssl,charCod,strlen(charCod));

    //do stuff
    //ricevi esito dell'operazione
    // 0 -> success
    // 1 -> error
    bool res;
    int bytes;
    char buffer[8];
    memset(buffer, '\0', sizeof(buffer));
    bytes = SSL_read(ssl,buffer,sizeof(buffer));
    if(bytes > 0){
        buffer[bytes] = 0;
        int result = atoi(buffer);

        seggioChiamante->mutex_stdout.lock();
        cout << "ClientSeggio: Risultato postazione liberata: " << result << endl;
        seggioChiamante->mutex_stdout.unlock();

        if (result == 0){
            res = true;
        }

    }
    else{
        cerr << "ClientSeggio: Errore ricezione del risultato postazione liberata" << endl;

    }


    //end do stuff


    seggioChiamante->mutex_stdout.lock();
    BIO_printf(this->outbio, "ClientSeggio: Finished SSL/TLS connection with server: %s.\n",
               this->PV_IPaddress);
    seggioChiamante->mutex_stdout.unlock();

    if(SSL_shutdown(this->ssl) == 0){ // valore di ritorno uguale a 0 indica che lo shutdown non si è concluso , bisogna richiamare la SSL_shutdown una seconda volta
        SSL_shutdown(this->ssl);
    }
    if(close(this->server_sock) != 0) {
        cerr << "ClientSeggio: errore chiusura socket per il server" << endl;
    }
    SSL_free(this->ssl);

    return res;
}

bool SSLClient::queryAttivazioneSeggio(string sessionKey)
{
    bool attivata = false;

    //richiesta servizio
    int serviceCod = serviziUrna::attivazioneSeggio;
    stringstream ssCod;
    ssCod << serviceCod;
    string strCod = ssCod.str();
    const char * charCod = strCod.c_str();
    seggioChiamante->mutex_stdout.lock();
    cout << "ClientSeggio: richiedo il servizio: " << charCod << endl;
    seggioChiamante->mutex_stdout.unlock();
    SSL_write(ssl,charCod,strlen(charCod));

    //invio mio ip
    string interfaceIP = getConfig("interfaceIP");
    string my_ip = seggioChiamante->getIPbyInterface(interfaceIP.c_str());
    sendString_SSL(ssl,my_ip);

    //ricevo idProceduraVoto
    uint idProcedura;
    char cod_idProcedura[128];
    memset(cod_idProcedura, '\0', sizeof(cod_idProcedura));
    int bytes = SSL_read(ssl, cod_idProcedura, sizeof(cod_idProcedura));
    if (bytes > 0) {
        cod_idProcedura[bytes] = 0;
        idProcedura = atoi(cod_idProcedura);
        //pvChiamante->setIdProceduraVoto(idProcedura);
    }
    else{
        cerr << "ClientSeggio: non sono riuscito a ricevere l'idProcedura" << endl;
    }

    //SE L'ID PROCEDURA RICEVUTO È 0, NON C'È UNA PROCEDURA IN CORSO, ABORTISCO L'ATTIVAZIONE
    if(idProcedura == 0){
        return attivata; //valore corrente "false"

    }
    else{
        seggioChiamante->setIdProceduraVoto(idProcedura);
    }

    string idProceduraMAC = seggioChiamante->calcolaMAC(sessionKey, to_string(idProcedura));

    //invio MAC all'URNA

    const char * charIdProceduraMAC = idProceduraMAC.c_str();
    //uvChiamante->mutex_stdout.lock();
    cout << "ClientSeggio: Invio il MAC all'Urna: " << charIdProceduraMAC << endl;
    //uvChiamante->mutex_stdout.unlock();
    SSL_write(ssl,charIdProceduraMAC,strlen(charIdProceduraMAC));


    //ricevi esito verifica della chiave di sessione
    // 0 -> success
    // 1 -> error

    char buffer[16];
    memset(buffer, '\0', sizeof(buffer));
    bytes = SSL_read(ssl,buffer,sizeof(buffer));
    if(bytes > 0){
        buffer[bytes] = 0;
        int result = atoi(buffer);

        if (result == 0){
            attivata = true;
        }

    }
    else{
        cerr << "ClientSeggio: Errore ricezione esito attivazione" << endl;

    }



    //----se l'attivazione è andata a buon fine----
    if(attivata){
        //ricevi infoProcedura: descrizione,dtInizio,dtTermine,stato
        string descrizione;
        receiveString_SSL(ssl,descrizione);
        cout << "ClientSeggio: descrizione: " << descrizione << endl;
        seggioChiamante->setDescrizioneProcedura(descrizione);

        string dtInizio;
        receiveString_SSL(ssl,dtInizio);
        cout << "ClientSeggio: inizio Procedura: " << dtInizio << endl;
        seggioChiamante->setDtInizioProcedura(dtInizio);

        string dtTermine;
        receiveString_SSL(ssl,dtTermine);
        cout << "ClientSeggio: termine Procedura: " << dtTermine << endl;
        seggioChiamante->setDtTermineProcedura(dtTermine);

        string stato;
        receiveString_SSL(ssl,stato);
        cout << "ClientSeggio: stato Procedura: " << stato << endl;
        seggioChiamante->setStatoProcedura(atoi(stato.c_str()));


        //ricevi infoSessione:idSessione,dataSessione, oraApertura, oraChiusura
        string idSessione;
        receiveString_SSL(ssl,idSessione);
        cout << "ClientSeggio: id Sessione: " << idSessione << endl;
        seggioChiamante->setIdSessione(atoi(idSessione.c_str()));

        string dataSessione;
        receiveString_SSL(ssl,dataSessione);
        cout << "ClientSeggio: data Sessione: " << dataSessione << endl;

        string oraApertura;
        receiveString_SSL(ssl,oraApertura);
        cout << "ClientSeggio: ora Apertura: " << oraApertura << endl;

        string oraChiusura;
        receiveString_SSL(ssl,oraChiusura);
        cout << "ClientSeggio: ora Chiusura: " << oraChiusura << endl;

        seggioChiamante->setDtAperturaSessione(dataSessione + " " + oraApertura);
        seggioChiamante->setDtChiusuraSessione(dataSessione + " " + oraChiusura);

        //ricevi info HT associati al seggio

        for (uint i = 0; i < 5; i++){
            string sn;
            string user;
            string pass;
            receiveString_SSL(ssl,sn);
            receiveString_SSL(ssl,user);
            receiveString_SSL(ssl,pass);
            string encodedMAC;
            receiveString_SSL(ssl, encodedMAC);

            if(seggioChiamante->verifyMAC(sessionKey,sn+user+pass,encodedMAC) != 0){
                attivata = false;
                break;
            }
            else{
                HardwareToken ht;
                ht.setSN(sn);
                ht.setUsername(user);
                ht.setPassword(pass);
                seggioChiamante->addGeneratoreOTP(ht);
            }

        }

        //ricevi chiave publica RP
        string publicKeyRP;
        receiveString_SSL(ssl,publicKeyRP);
        string encodedMAC;
        receiveString_SSL(ssl,encodedMAC);

        if(seggioChiamante->verifyMAC(sessionKey,publicKeyRP,encodedMAC) != 0 ){
            attivata = false;
        }
        else{
            seggioChiamante->setPublicKeyRP(publicKeyRP);
        }



    }
    return attivata;
}

bool SSLClient::queryRisultatiVoto(uint idProcedura, string &risultatiScrutinioXML,string &encodedSignRP)
{
    //richiesta servizio
    int serviceCod = serviziUrna::risultatiVoto;
    stringstream ssCod;
    ssCod << serviceCod;
    string strCod = ssCod.str();
    const char * charCod = strCod.c_str();

    cout << "ClientPV: richiedo il servizio: " << charCod << endl;

    SSL_write(ssl,charCod,strlen(charCod));


    sendString_SSL(ssl,to_string(idProcedura));

    uint esito = 1;
    string s;
    if(receiveString_SSL(ssl,s) != 0){
        esito = atoi(s.c_str());
    }
    else {return false;}

    if (esito == 0){
        if(receiveString_SSL(ssl, risultatiScrutinioXML)==0){
            return false;
        }
        if(receiveString_SSL(ssl,encodedSignRP) == 0){
            return false;
        }
    }

    return true;
}
bool SSLClient::queryNextSessione(uint idProcedura, uint &esito)
{
    esito = 1;
    //richiesta servizio
    int serviceCod = serviziUrna::nextSessione;
    string strCod = to_string(serviceCod);
    const char * charCod = strCod.c_str();

    seggioChiamante->mutex_stdout.lock();
    cout << "ClientSeggio: richiedo il servizio: " << charCod << endl;
    seggioChiamante->mutex_stdout.unlock();
    SSL_write(ssl,charCod,strlen(charCod));

    sendString_SSL(ssl,to_string(idProcedura));

    string str;
    if(receiveString_SSL(ssl,str)==0){
        return false;
    }
    esito = atoi(str.c_str());

    string dataSessione;
    receiveString_SSL(ssl,dataSessione);
    cout << "ClientSeggio: data Sessione: " << dataSessione << endl;

    string oraApertura;
    receiveString_SSL(ssl,oraApertura);
    cout << "ClientSeggio: ora Apertura: " << oraApertura << endl;

    string oraChiusura;
    receiveString_SSL(ssl,oraChiusura);
    cout << "ClientSeggio: ora Chiusura: " << oraChiusura << endl;

    seggioChiamante->setDtAperturaSessione(dataSessione + " " + oraApertura);
    seggioChiamante->setDtChiusuraSessione(dataSessione + " " + oraChiusura);

    return true;
}

uint SSLClient::queryTryVote(uint matricola, uint &idTipoVotante)
{
    //richiesta servizio
    int serviceCod = serviziUrna::tryVoteElettore;
    stringstream ssCod;
    ssCod << serviceCod;
    string strCod = ssCod.str();
    const char * charCod = strCod.c_str();
    seggioChiamante->mutex_stdout.lock();
    cout << "ClientSeggio: richiedo il servizio: " << charCod << endl;
    seggioChiamante->mutex_stdout.unlock();
    SSL_write(ssl,charCod,strlen(charCod));

    //invio mio ip
    string interfaceIP = getConfig("interfaceIP");
    string my_ip = seggioChiamante->getIPbyInterface(interfaceIP.c_str());
    sendString_SSL(ssl,my_ip);

    //invia matricola
    sendString_SSL(ssl,std::to_string(matricola));

    //invio MAC matricola
    string encodedMac = seggioChiamante->calcolaMAC(seggioChiamante->getSessionKey_Seggio_Urna(),to_string(matricola));
    sendString_SSL(ssl,encodedMac);

    //ricevi esito
    uint esito = seggioChiamante->esitoLock::errorLocking;
    string esitoStr;
    receiveString_SSL(ssl,esitoStr);
    if(esitoStr != ""){
        esito = atoi(esitoStr.c_str());
        cout << "ClientSeggio: Matricola " << matricola <<", esito lock: " << esito << endl;
    }
    else{
        cerr << "ClientSeggio: Errore ricezione esito lock" << endl;
    }
    //ricevi eventualmente il ruolo/tipoVotante se l'esito del lock al voto è positivo
    if(esito == seggioChiamante->esitoLock::locked){
        string ruoloStr;
        receiveString_SSL(ssl,ruoloStr);
        idTipoVotante = atoi(ruoloStr.c_str());
    }

    return esito;
}

bool SSLClient::queryInfoMatricola(uint matricola, string &nome, string &cognome, uint &statoVoto)
{
    //richiesta servizio
    int serviceCod = serviziUrna::infoMatricola;
    stringstream ssCod;
    ssCod << serviceCod;
    string strCod = ssCod.str();
    const char * charCod = strCod.c_str();
    seggioChiamante->mutex_stdout.lock();
    cout << "ClientSeggio: richiedo il servizio: " << charCod << endl;
    seggioChiamante->mutex_stdout.unlock();
    SSL_write(ssl,charCod,strlen(charCod));

    //invio mio ip
    string interfaceIP = getConfig("interfaceIP");
    string my_ip = seggioChiamante->getIPbyInterface(interfaceIP.c_str());
    sendString_SSL(ssl,my_ip);

    //invio matricola
    sendString_SSL(ssl,to_string(matricola));

    //inviare MAC della matricola
    string encodedMac = seggioChiamante->calcolaMAC(seggioChiamante->getSessionKey_Seggio_Urna(),to_string(matricola));
    sendString_SSL(ssl,encodedMac);

    bool matricolaPresente = false;
    //ricevo informazione sulla presenza o assenza della matricola
    string exist;
    receiveString_SSL(ssl,exist);

    if(exist != ""){
        if(atoi(exist.c_str()) == seggioChiamante->matricolaExist::si){
            matricolaPresente = true;

            //ricevo le informazioni relative alla matricola:stato, nome, cognome
            string s;
            receiveString_SSL(ssl,s);
            statoVoto = atoi(s.c_str());

            receiveString_SSL(ssl,nome);

            receiveString_SSL(ssl,cognome);
        }
    }
    else{
        cerr << "ClientSeggio: Errore ricezione valore di esistenza della matricola" << endl;
    }



    //se la matricola non è presente il valore è rimasto a false
    return matricolaPresente;


}

bool SSLClient::queryResetMatricolaState(uint matricola)
{
    //richiesta servizio
    int serviceCod = serviziUrna::resetMatricolaStatoVoto;
    stringstream ssCod;
    ssCod << serviceCod;
    string strCod = ssCod.str();
    const char * charCod = strCod.c_str();
    seggioChiamante->mutex_stdout.lock();
    cout << "ClientSeggio: richiedo il servizio: " << charCod << endl;
    seggioChiamante->mutex_stdout.unlock();
    SSL_write(ssl,charCod,strlen(charCod));

    //invio mio ip
    string interfaceIP = getConfig("interfaceIP");
    string my_ip = seggioChiamante->getIPbyInterface(interfaceIP.c_str());
    sendString_SSL(ssl,my_ip);

    //invio matricola da resettare
    sendString_SSL(ssl, to_string(matricola));

    //invio mac matricola
    string encodedMac = seggioChiamante->calcolaMAC(seggioChiamante->getSessionKey_Seggio_Urna(),to_string(matricola));
    sendString_SSL(ssl,encodedMac);

    //ricevi esito operazione

    // TODO gestire la situazione in cui sul database delle anagrafiche risulti che il voto è stato espresso
    string s;
    receiveString_SSL(ssl, s);
    if(s!=""){
        int success = atoi(s.c_str());
        if(success == 0){
            return true;
        }
        else{
            return false;
        }
    }
    else{
        return false;
    }


}

void SSLClient::sendString_SSL(SSL* ssl, string s) {
    int length = strlen(s.c_str());
    string length_str = std::to_string(length);
    const char *num_bytes = length_str.c_str();
    SSL_write(ssl, num_bytes, strlen(num_bytes));
    SSL_write(ssl, s.c_str(), length);
}

int SSLClient::receiveString_SSL(SSL* ssl, string &s){

    char dim_string[16];
    memset(dim_string, '\0', sizeof(dim_string));
    int bytes = SSL_read(ssl, dim_string, sizeof(dim_string));
    if (bytes > 0) {
        dim_string[bytes] = 0;
        //lunghezza fileScheda da ricevere
        uint length = atoi(dim_string);
        char buffer[length + 1];
        memset(buffer, '\0', sizeof(buffer));
        bytes = SSL_read(ssl, buffer, sizeof(buffer));
        if (bytes > 0) {
            buffer[bytes] = 0;
            s = buffer;
        }
    }
    return bytes; //bytes read for the string received
}


