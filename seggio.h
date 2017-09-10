/*
 * Seggio.h
 *
 *  Created on: 18/gen/2017
 *      Author: giuse
 */

#ifndef SEGGIO_H_
#define SEGGIO_H_

#include <time.h>
#include <iostream>
#include <vector>
#include <array>
#include "associazione.h"
#include <thread>
#include <string>
#include <mutex>
#include <chrono>
#include "sslserver.h"
#include "sslclient.h"
#include "mainwindowseggio.h"

#include "sslclient.h"
#include "sslserver.h"


#include "cryptopp/osrng.h"
#include "cryptopp/cryptlib.h"
#include "cryptopp/hmac.h"
#include "cryptopp/sha.h"
#include "cryptopp/hex.h"
#include "cryptopp/filters.h"
#include "cryptopp/secblock.h"
#include "cryptopp/aes.h"
#include "cryptopp/modes.h"
#include <cryptopp/rsa.h>

#include <QtCore>
#include <QThread>

#define NUM_PV 3
#define NUM_HT_ATTIVI 4

using namespace CryptoPP;
using namespace std;

class SSLServer;
class SSLClient;

class Seggio : public QThread{
    Q_OBJECT
signals:
    void stateChanged(unsigned int idPV,unsigned int stato);
    void anyPVFree(bool);
    void anyAssociationRemovable(bool);
    void associationReady(unsigned int idHT, unsigned int idPV);
    void validPass();
    void wrongPass();
    void forbidLogout();
    void grantLogout();
    void removableAssociationsReady(std::vector <Associazione> associazioniRimovibili);
    void sessionEnded();
    void sessionNotYetStarted();
    void allowVote();
    void forbidVote(std::string);
    void errorPV(uint idPV);
    void matricolaStateReceived(QString infoMatricola);
    void urnaNonRaggiungibile();

public slots:
    void createAssociazioneHT_PV();
    void eliminaNuovaAssociazione();

    void tryLogout();
    void setLogged(bool value);
    void calculateRemovableAssociations();

    //funzioni per richiedere servizi alle postazioni voto

    void removeAssociazioneHT_PV(unsigned int idPV);
    void completaOperazioneVoto(uint idPV);

    //funzioni per richiedere servizi all'urna
    void tryVote(uint matricola);
    void validatePassKey(QString pass);
    void matricolaState(uint matricola);

    //void liberaHT_PV(unsigned int idPV);
    //bool feedbackFreeBusy(unsigned int idPV);
    //servizio da richiedere all'urna virtuale
    //void readVoteResults(unsigned int idProceduraVoto);

public:
    Seggio(QObject *parent = 0);
    ~Seggio();

    //void setNumeroSeggio(int number);
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

    unsigned int getNumberAssCorrenti();

    //dati membro pubblici
    enum statiPV : unsigned int{
        attesa_attivazione,
        libera,
        attesa_abilitazione,
        votazione_in_corso,
        votazione_completata,
        errore,
        offline,
        non_raggiungibile
    };

    enum statiProcedura: unsigned int{
            creazione,
            programmata,
            in_corso,
            conclusa,
            scrutinata,
            undefined
        };
    enum esitoLock{
            locked,
            alredyLocked,
            alredyVoted,
            notExist,
            errorLocking
        };
    enum matricolaExist{
            si,
            no
        };
    enum statoVoto{
            non_espresso,
            votando,
            espresso
        };
    std::mutex mutex_stati;
    std::mutex mutex_stdout;

    //questa funzione verrà chiamata dal thread che si mette in ascolto di aggiornamenti delle postazioni di voto

    void stopServerPV();

    QDateTime getDtAperturaSessione() const;
    void setDtAperturaSessione(const string &value);

    QDateTime getDtChiusuraSessione() const;
    void setDtChiusuraSessione(const string &value);

    QDateTime getDtInizioProcedura() const;
    void setDtInizioProcedura(const string &value);

    QDateTime getDtTermineProcedura() const;
    void setDtTermineProcedura(const string &value);

    unsigned int getIdProceduraVoto() const;
    void setIdProceduraVoto(unsigned int value);

    string calcolaMAC(string encodedSessionKey, string plainText);
    int verifyMAC(string encodedSessionKey, string data, string macEncoded);
    string getDescrizioneProcedura() const;
    void setDescrizioneProcedura(const string &value);

    uint getStatoProcedura() const;
    void setStatoProcedura(const uint &value);

    uint getIdSessione() const;
    void setIdSessione(const uint &value);

private:
    void run();

    //thread in ascolto degli aggiornamenti dalle postazioni di voto
    SSLServer * seggio_server;
    std::thread thread_server;
    void runServerPV();

    //thread che ogni tot di secondi esegue il pull degli stati delle postazioni di voto
    SSLClient * pull_client;
    std::thread thread_pull;
    void aggiornaPVs();

    bool stopServer;
    bool logged;
    const char * ipUrna;
    //questi due arrey tengono traccia delle postazioni di voto e degli hardware token attualmente impegnati in associazioni PV_HT
    std::array <bool,NUM_HT_ATTIVI> busyHT;
    std::array <bool,NUM_PV> busyPV;

    std::array <unsigned int, NUM_PV> statoPV; //usare mutex per accedere al dato, accedono stateInfoPV (per ottenere lo stato della singola postazione e il thread che aggiorna i valori con quelli che riceve dalla i-esima PV
    std::array <const char *, NUM_PV> IP_PV;

    string sessionKey_Seggio_Urna;
    //Dati da ottenere dall'urna centrale
    unsigned int idProceduraVoto;
    QDateTime dtAperturaSessione;
    QDateTime dtChiusuraSessione;
    QDateTime dtInizioProcedura;
    QDateTime dtTermineProcedura;
    string descrizioneProcedura;
    uint statoProcedura;
    uint idSessione;
    //gli id degli HT non vanno da 1 a 5, ma sono relativi agli identificativi propri HT
    std::array <unsigned int,NUM_HT_ATTIVI> idHTAttivi;
    std::array <string, NUM_HT_ATTIVI> authenticationUsernameHT;
    unsigned int idHTRiserva;
    unsigned int numeroSeggio;

    //idPostazioniVoto vanno da 1 in poi, al massimo 7

    std::array <unsigned int,NUM_PV> idPostazioniVoto;
    std::array <unsigned int,NUM_PV> PV_lastUsedHT;
    std::vector< Associazione > listAssociazioni;
    Associazione *nuovaAssociazione;

    //funzioni a solo uso del seggio
    void setBusyHT_PV();
    bool pushAssociationToPV(unsigned int idPV, unsigned int idHT, unsigned int ruolo, uint matricola);
    void pullStatePV(unsigned int idPV);
    bool removeAssociationFromPV(unsigned int idPV);
    bool freePVpostVotazione(unsigned int idPV);
    bool addAssociazioneHT_PV(uint matricola);

    const char * calcolaIP_PVbyID(unsigned int idPV);


    //allo stato attuale un seggio prevede una composizione di 3 postazioni di voto e 4 hardware token attivi


    std::vector <Associazione> associazioniRimovibili;
};

#endif /* SEGGIO_H_ */
