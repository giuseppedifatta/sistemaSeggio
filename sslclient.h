#ifndef SSLCLIENT_H
#define SSLCLIENT_H
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>
#include <string>
#include <ifaddrs.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include "seggio.h"
#include "conf.h"


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
    void configure_context(const char *CertFile, const char *KeyFile, const char *ChainFile);
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
    bool querySetAssociation(string snHT, unsigned int idTipoVotante, uint matricola, string usernameHT, string passwordHT);
    int queryPullPVState();
    bool queryRemoveAssociation();
    bool queryFreePV();

    //richieste per l'Urna
    bool queryAttivazioneSeggio(string sessionKey);
    bool queryRisultatiVoto(uint idProcedura, string &risultatiScrutinioXML, string &encodedSignRP);
    bool queryNextSessione(uint idProcedura, uint &esito);
    uint queryTryVote(uint matricola, uint &idTipoVotante);
    bool queryInfoMatricola(uint matricola, string &nome, string &cognome, uint &statoVoto);
    bool queryResetMatricolaState(uint matricola);
    //utility per far terminare la funzione di esecuzione del thread server del seggio
    void stopLocalServer();

    enum serviziUrna { //richiedente
        //attivazionePV = 0, //postazionevoto
        attivazioneSeggio = 1, //seggio
        //infoProcedura, //seggio
        nextSessione = 3, //seggio
        risultatiVoto  = 4, //seggio
        //invioSchedeCompilate = 5//, //postazionevoto
        //scrutinio = 6, //responsabile procedimento
        //autenticazioneRP = 7, //responsabile procedimento
        tryVoteElettore = 8,
        infoMatricola = 9,
        //setMatricolaVoted = 10,
        checkConnection = 11,
        resetMatricolaStatoVoto = 12
    };

    enum serviziPV{
        setAssociation,
        pullPVState,
        removeAssociation,
        freePV
    };

};


#endif // SSLCLIENT_H
