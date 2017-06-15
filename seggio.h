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

class SSLServer;
class MainWindowSeggio;

class Seggio {
    //allo stato attuale un seggio prevede una composizione di 3 postazioni di voto e 4 hardware token attivi
public:
    Seggio(MainWindowSeggio * m);
    virtual ~Seggio();
    void setNumeroSeggio(int number);


    Associazione *getNuovaAssociazione();


    //metodi pubblici
    bool isBusyHT(unsigned int idHT);
    void disattivaHT(unsigned int tokenAttivo);
    void setPVstate(unsigned int idPV, unsigned int nuovoStatoPV);
    std::vector< Associazione > getListAssociazioni();
    std::array<unsigned int, 4>& getArrayIdHTAttivi();
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
    void liberaHT_PV(unsigned int idPV);



    //servizio da richiedere all'urna virtuale
    void readVoteResults(unsigned int idProceduraVoto);



    bool feedbackFreeBusy(unsigned int idPV);
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
    bool stopThreads;

    std::mutex mutex_stati;
    std::mutex mutex_stdout;

    //questa funzione verrà chiamata dal thread che si mette in ascolto di aggiornamenti delle postazioni di voto
    void runServerUpdatePV();
    void stopServerUpdatePV();
private:
    //questi due arrey tengono traccia delle postazioni di voto e degli hardaware token attualmente impegnati in associazioni PV_HT
    std::array <bool,4> busyHT;
    std::array <bool,3> busyPV;


    std::array <unsigned int, 3> statoPV; //usare mutex per accedere al dato, accedono stateInfoPV (per ottenere lo stato della singola postazione e il thread che aggiorna i valori con quelli che riceve dalla i-esima PV
    std::array <const char *, 3> IP_PV;
    unsigned int idProceduraVoto;
    tm dataAperturaSessione;
    tm dataChiusuraSessione;
    std::array <unsigned int,4> idHTAttivi;
    unsigned int idHTRiserva;
    unsigned int idPostazioniVoto[3];
    unsigned int numeroSeggio;
    std::array <unsigned int,3> PV_lastUsedHT={0,0,0};
    std::vector< Associazione > listAssociazioni;
    Associazione *nuovaAssociazione;
    void setBusyHT_PV();

    SSLServer * seggio_server;
    std::thread thread_server;
    //std::thread thread_1;
    //std::thread thread_2;
    //std::thread thread_3;

    SSLClient * seggio_client;

    MainWindowSeggio *mainWindow;

};

#endif /* SEGGIO_H_ */
