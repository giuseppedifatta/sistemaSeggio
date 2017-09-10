#ifndef SSLCLIENT_H
#define SSLCLIENT_H
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>
#include <string>

#include "seggio.h"


using namespace std;
class Seggio;
class SSLClient
{
private:

    //dati membro per la connessione SSL
    SSL_CTX *ctx;
    BIO* outbio;
    int server_sock;
    SSL * ssl;
    const char * PV_IPaddress;

    Seggio *seggioChiamante;

    //metodi privati
    //funzioni per l'inizializzazione di SSL e la configurazione del contesto SSL
    void init_openssl_library();
    void cleanup_openssl();
    void createClientContext();
    void configure_context(char* CertFile, char* KeyFile, char * ChainFile);
    int create_socket(const char * port);

    //elaborazione certificati
    void ShowCerts();
    void verify_ServerCert();



    void sendString_SSL(SSL *ssl, string s);
    int receiveString_SSL(SSL *ssl, string &s);
public:
    SSLClient(Seggio * s);
    ~SSLClient();

    //connessione all'host
    SSL* connectTo(const char* hostIP/*hostname*/);

    //richieste per le Postazioni di Voto
    bool querySetAssociation(unsigned int idHT, unsigned int ruolo, uint matricola);
    int queryPullPVState();
    bool queryRemoveAssociation();
    bool queryFreePV();

    //richieste per l'Urna
    bool queryAttivazioneSeggio(string sessionKey);
    bool queryRisultatiVoto();
    uint queryTryVote(uint matricola,uint &ruolo);
    bool queryInfoMatricola(uint matricola, string &nome, string &cognome, uint &statoVoto);

    //utility per far terminare la funzione di esecuzione del thread server del seggio
    void stopLocalServer();

    enum serviziUrna { //richiedente
        //attivazionePV = 0, //postazionevoto
        attivazioneSeggio = 1, //seggio
        //infoProcedura, //seggio
        //infoSessione, //seggio
        risultatiVoto  = 4, //seggio
        //invioSchedeCompilate = 5//, //postazionevoto
        //scrutinio = 6, //responsabile procedimento
        //autenticazioneRP = 7, //responsabile procedimento
        tryVoteElettore = 8,
        infoMatricola = 9
    };

};


#endif // SSLCLIENT_H
