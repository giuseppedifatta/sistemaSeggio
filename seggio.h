/*
 * Seggio.h
 *
 *  Created on: 18/gen/2017
 *      Author: giuse
 */

#ifndef SEGGIO_H_
#define SEGGIO_H_

#include <time.h>
#include <vector>
#include <array>
#include "associazione.h"
#include <thread>
#include <string>
#include <mutex>
#include "sslserver.h"
#include "sslclient.h"
#include "mainwindowseggio.h"

#define NUM_PV 3
#define NUM_HT_ATTIVI 4
class SSLServer;
class MainWindowSeggio;
class SSLClient;

class Seggio {
    //allo stato attuale un seggio prevede una composizione di 3 postazioni di voto e 4 hardware token attivi
public:
    Seggio(MainWindowSeggio * m);
    virtual ~Seggio();

    //il contesto SSL è condivisibile
    SSL_CTX * getCTX();

    void setNumeroSeggio(int number);
    Associazione *getNuovaAssociazione();


    //metodi pubblici
    bool isBusyHT(unsigned int idHT);
    void disattivaHT(unsigned int tokenAttivo);

    void setPVstate(unsigned int idPV, unsigned int nuovoStatoPV);

    std::vector< Associazione > getListAssociazioni();
    std::array<unsigned int, NUM_HT_ATTIVI>& getArrayIdHTAttivi();
    unsigned int getIdHTRiserva();
    const char * getIP_PV(unsigned int idPV);

    bool anyAssociazioneEliminabile();
    unsigned int stateInfoPV(unsigned int idPV);
    bool anyPostazioneLibera();


    //funzionalità dei componenti del seggio
    bool createAssociazioneHT_PV();
    void addAssociazioneHT_PV();
    void eliminaNuovaAssociazione();
    void removeAssociazioneHT_PV(unsigned int idPV);
    //void liberaHT_PV(unsigned int idPV);
    //bool feedbackFreeBusy(unsigned int idPV);

    //servizio da richiedere all'urna virtuale
    //void readVoteResults(unsigned int idProceduraVoto);


    unsigned int getNumberAssCorrenti();

    //dati membro pubblici
    enum statiPV{
        attesa_attivazione,
        libera,
        attesa_abilitazione,
        votazione_in_corso,
        votazione_completata,
        errore,
        offline
    };
    std::array <std::string,6> patternSS;


    std::mutex mutex_stati;
    std::mutex mutex_stdout;

    //questa funzione verrà chiamata dal thread che si mette in ascolto di aggiornamenti delle postazioni di voto

    void stopServerUpdatePV();
    void setStopThreads(bool b);
private:
    void runServerUpdatePV();
    //strutture condivisibili di SSL

    SSL_CTX *ctx;
    BIO* outbio;

    SSLServer * seggio_server;
    std::thread thread_server;
    bool stopThreads;
    SSLClient * seggio_client;
    MainWindowSeggio *mainWindow;

    //questi due arrey tengono traccia delle postazioni di voto e degli hardaware token attualmente impegnati in associazioni PV_HT
    std::array <bool,NUM_HT_ATTIVI> busyHT;
    std::array <bool,NUM_PV> busyPV;

    std::array <unsigned int, NUM_PV> statoPV; //usare mutex per accedere al dato, accedono stateInfoPV (per ottenere lo stato della singola postazione e il thread che aggiorna i valori con quelli che riceve dalla i-esima PV
    std::array <const char *, NUM_PV> IP_PV;

    //Dati da ottenere dall'urna centrale
    unsigned int idProceduraVoto;
    tm dataAperturaSessione;
    tm dataChiusuraSessione;
    //gli id degli HT non vanno da 1 a 5, ma sono relativi agli identificativi propri HT
    std::array <unsigned int,NUM_HT_ATTIVI> idHTAttivi;
    unsigned int idHTRiserva;
    unsigned int numeroSeggio;

    //idPostazioniVoto vanno da 1 in poi, al massimo 7

    std::array <unsigned int,NUM_PV> idPostazioniVoto;
    std::array <unsigned int,NUM_PV> PV_lastUsedHT;
    std::vector< Associazione > listAssociazioni;
    Associazione *nuovaAssociazione;

    //funzione che setta gli ht e le pv inmpegnate e chiama la funzione che comunica l'associazione alla postazione di voto relativa
    void setBusyHT_PV();
    void pushAssociationToPV(const char *ip_pv, unsigned int idHT);

    const char * calcolaIP_PVbyID(unsigned int idPV);

    //funzioni per l'inizializzazione di SSL e la configurazione del contesto SSL
    void init_openssl_library();
    void cleanup_openssl();
    void create_context();
    void configure_context(char* CertFile, char* KeyFile, char * ChainFile);

};

#endif /* SEGGIO_H_ */
