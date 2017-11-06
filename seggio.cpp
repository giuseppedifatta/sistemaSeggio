#include <iostream>
#include <thread>
#include <mutex>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include "tinyxml2.h"

#include "seggio.h"
#include "sslclient.h"
#include "sslserver.h"
#include "risultatiSeggio.h"
#include "conf.h"

using namespace std;
using namespace tinyxml2;

Seggio::Seggio(QObject *parent):
    QThread(parent){
    ipUrna = getConfig("ipUrna");

    for(unsigned int i = 0; i < NUM_PV; i++){
        idPostazioniVoto.at(i)=i+1;
        //busyPV[i]=true;
        PV_lastUsedHT.at(i) = "";
        statoPV.at(i)=0;
    }

    //    for(unsigned int i = 0; i < NUM_HT_ATTIVI; i++){
    //        //il primo HT del vettore non è quello con id "1", ma quello con id memorizzato nella prima posizione del vettore idHTAttivi
    //        busyHT[i]=false;
    //    }
    nuovaAssociazione = NULL;

    //calcolare usando come indirizzo base l'IP pubblico del seggio

    for (uint i = 0; i < NUM_PV; i++){

        IP_PV.push_back(this->calcolaIP_PVbyID(i+1));
    }
    cout << "IP delle postazioni voto, dentro il costruttore" << endl;
    for (uint i = 0; i < IP_PV.size(); i++){
        cout << IP_PV.at(i) << endl;
    }

}

Seggio::~Seggio(){

    delete nuovaAssociazione;

    //    this->mutex_stdout.lock();
    //    cout << "Seggio: join del thread_server" << endl;
    //    this->mutex_stdout.unlock();
}

void Seggio::run(){
    this->stopServer = false;
    this->associazioniRimovibili.clear();
    this->listAssociazioni.clear();
    for (uint i = 0; i < busyHT.size(); i++){
        busyHT.at(i) = false;
    }

    cout << "IP delle postazioni voto, dentro la run" << endl;
    for (uint i = 0; i < IP_PV.size(); i++){
        cout << IP_PV.at(i) << endl;
    }
    this->nuovaAssociazione = NULL;

    thread_server = std::thread(&Seggio::runServerPV, this);

    logged = true;
    thread_pull = std::thread(&Seggio::aggiornaPVs,this);
    thread_pull.detach();//se imposto a join, bisogna aspettare che il thread pull rilevi che siamo in fase di logout

    thread_server.join();
    cout << "Seggio: thread server ha terminato, esco dalla mia run()" << endl;
}

uint Seggio::getIdSessione() const
{
    return idSessione;
}

void Seggio::setIdSessione(const uint &value)
{
    idSessione = value;
}

uint Seggio::getStatoProcedura() const
{
    return statoProcedura;
}

void Seggio::setStatoProcedura(const uint &value)
{
    statoProcedura = value;
}

string Seggio::getDescrizioneProcedura() const
{
    return descrizioneProcedura;
}

void Seggio::setDescrizioneProcedura(const string &value)
{
    descrizioneProcedura = value;
}

unsigned int Seggio::getIdProceduraVoto() const
{
    return idProceduraVoto;
}

void Seggio::setIdProceduraVoto(unsigned int &value)
{
    idProceduraVoto = value;
}

QDateTime Seggio::getDtTermineProcedura() const
{
    return dtTermineProcedura;
}

void Seggio::setDtTermineProcedura(const string &value)
{
    QString str = QString::fromStdString(value);
    dtTermineProcedura = QDateTime::fromString(str,"dd-MM-yyyy hh:mm:ss");

}

QDateTime Seggio::getDtInizioProcedura() const
{
    return dtInizioProcedura;
}

void Seggio::setDtInizioProcedura(const string &value)
{
    QString str = QString::fromStdString(value);
    dtInizioProcedura = QDateTime::fromString(str,"dd-MM-yyyy hh:mm:ss");
}

QDateTime Seggio::getDtChiusuraSessione() const
{
    return dtChiusuraSessione;
}

void Seggio::setDtChiusuraSessione(const string &value)
{
    QString str = QString::fromStdString(value);
    dtChiusuraSessione = QDateTime::fromString(str,"dd-MM-yyyy hh:mm");
}

QDateTime Seggio::getDtAperturaSessione() const
{
    return dtAperturaSessione;
}

void Seggio::setDtAperturaSessione(const string &value)
{
    QString str = QString::fromStdString(value);
    dtAperturaSessione = QDateTime::fromString(str,"dd-MM-yyyy hh:mm");
}

//void Seggio::setstopServer(bool b) {
//    this->stopServer = b;
//}

void Seggio::setBusyHT_PV(){
    unsigned int idPV=this->nuovaAssociazione->getIdPV();
    string snHT=this->nuovaAssociazione->getSnHT();

    for(unsigned int index = 0; index < this->busyHT.size(); index++){
        if(this->snHTAttivi[index] == snHT){ //se l'id dell'HTAttivo corrente corrisponde con l'id dell'HT da impegnare
            this->busyHT[index] = true; //lo settiamo ad occupato nell'indice degli HT occupati
            break;
        }
    }
    this->PV_lastUsedHT[idPV-1]=snHT;
    this->mutex_stdout.lock();
    cout << "Seggio: Hardware token: " << snHT << ", utilizzo su PV: " << idPV << endl;
    this->mutex_stdout.unlock();


    this->busyPV[idPV-1] = true;

    if(this->anyPostazioneLibera()){
        cout << "Seggio: almeno una postazione libera" << endl;
        emit anyPVFree(true);
        //mainWindow->disableCreaAssociazioneButton();
    }
    else{
        emit anyPVFree(false);
    }

    if(anyAssociazioneEliminabile()){
        cout << "Seggio: almeno una associazione eliminabile" << endl;
        emit anyAssociationRemovable(true);
    }
    else{
        emit anyAssociationRemovable(false);
    }

}

void Seggio::createAssociazioneHT_PV(){
    if(QDateTime::currentDateTime() < this->dtAperturaSessione){
        emit sessionNotYetStarted();
        return;
    }
    if(QDateTime::currentDateTime() > this->dtChiusuraSessione){
        emit sessionEnded();
        return;
    }

    //crea un associazione in base alle risorse disponibili
    //lo stesso HT non viene assegnato due volte consecutive alla stessa PV
    unsigned int idPV = 0;
    string snHT = "";
    bool associationFound = false;
    for(unsigned int indexPV = 0; indexPV < this->busyPV.size();indexPV++){
        if (associationFound==true){
            break; //interrompe il ciclo for
        }
        else {
            bool pv_occupata = this->busyPV[indexPV];

            unsigned int pv_stato = this->stateInfoPV(indexPV+1);

            if((!pv_occupata) && (pv_stato!=statiPV::attesa_attivazione) && (pv_stato==statiPV::libera)){ // se la postazione non è occupata
                //seleziona la postazione se è attivata e libera(disponibile per una associazione)
                this->mutex_stdout.lock();
                cout << "Seggio: PV " << indexPV+1 << " candidata alla associazione trovata" << endl;
                this->mutex_stdout.unlock();
                idPV=indexPV+1; //postazione selezionata
                for (unsigned int indexHT = 0; indexHT < this->busyHT.size();indexHT++){
                    if (this->busyHT[indexHT]==false){ // se l'HT non è impegnato

                        if(this->PV_lastUsedHT[indexPV]!=this->snHTAttivi[indexHT]){ //e l'HT non è stato utilizzato l'ultima volta su questa postazione, si può selezionare
                            cout << "Seggio: sulla postazione PV " << indexPV+1 << " l'ultimo token utilizzato è stato il " << this->PV_lastUsedHT[indexPV] << endl;
                            snHT=this->snHTAttivi[indexHT]; //seleziona l'i-esimo HT tra quelli attivi
                            cout << "Seggio: HT " << snHT << " selezionato" << endl;
                            associationFound = true; //associazione trovata

                            //creazione della nuova associazione
                            this->nuovaAssociazione = new Associazione(idPV,snHT);
                            cout << "Seggio: nuova associazione creata" << endl;
                            break; //interrompe il ciclo for
                        }
                    }
                }
            }
        }
    }

    if(snHT !="" && idPV != 0){
        emit associationReady(snHT,idPV);
    }
}

bool Seggio::addAssociazioneHT_PV(uint matricola){
    bool pushed = false;
    //aggiunge un'associazione alla lista e aggiorna i flag su PV e HT occupati

    //bug nel caso in cui non si riesca a fare il push della associazione, risolvere!!!

    unsigned int idPV=this->nuovaAssociazione->getIdPV();
    string snHT=this->nuovaAssociazione->getSnHT();
    unsigned int IdTipoVotante = this->nuovaAssociazione->getIdTipoVotante();
    nuovaAssociazione->setMatricola(matricola);

    uint indexHT = this->getIndexHTBySN(snHT);
    string usernameHT = generatoriOTP.at(indexHT).getUsername();
    string passwordHT = generatoriOTP.at(indexHT).getPassword();

    //comunicare alla postazione di competenza la nuova associazione
    //const char* IP_PV = this->calcolaIP_PVbyID(idPV);
    if(pushAssociationToPV(idPV,snHT, IdTipoVotante,matricola, usernameHT,passwordHT)){ //se la comunicazione dell'associazione alla PV è andata a buon fine
        //aggiungiamo la nuova associazione alla lista delle associazioni correnti
        pushed = true;
        this->listAssociazioni.push_back(*this->nuovaAssociazione);

        //settiamo ad occupati ht e pv
        this->setBusyHT_PV();
        //liberiamo la memoria della nuova associazione, nel caso in cui sia stata aggiunta alla lista delle associazioni ne è stata creata una copia nel vettore
        delete this->nuovaAssociazione;
        //resettiamo il puntatore membro di classe
        this->nuovaAssociazione = NULL;
    }
    else{
        this->abortVoting(matricola, motivoAbort::erroreConfigurazioneAssociazione);
    }

    return pushed;

}



void Seggio::eliminaNuovaAssociazione(){
    delete this->nuovaAssociazione;
    this->nuovaAssociazione = NULL;
}

Associazione* Seggio::getNuovaAssociazione(){
    return this->nuovaAssociazione;
}

unsigned int Seggio::getNumberAssCorrenti(){
    //non utilizzata al momento
    unsigned int elementiHeap = this->listAssociazioni.size();
    return elementiHeap;
}

//void Seggio::setNumeroSeggio(int number){
//    this->numeroSeggio=number;
//}

vector< Associazione > Seggio::getListAssociazioni(){
    return this->listAssociazioni;
}

void Seggio::removeAssociazioneHT_PV(unsigned int idPV){
    //elimina un'associazione per la postazione di voto indicata

    unsigned int indexAss = 0;
    unsigned int currentPV = 0;
    string snHT = "";
    for (unsigned int i = 0; this->listAssociazioni.size();i++){
        currentPV = this->listAssociazioni.at(i).getIdPV();

        //trova l'associazione che corrisponde alla postazione di voto per cui rimuovere l'associazione
        if(currentPV==idPV){
            //salva l'id dell'hardware token da rendere libero
            snHT = this->listAssociazioni.at(i).getSnHT();
            //salva l'indice corrispondente per la successiva eliminazione
            indexAss = i;
            break; //interrompe l'esecuzione del for in cui è immediatemente contenuto
            // quindi in currentPV resta l'id della postazione per cui rimuovere l'associazione
        }
    }

    //comunica alla postazione voto la rimozione dell'associazione
    if(removeAssociationFromPV(idPV)){
        //se la rimozione sulla postazione è andata a buon fine, prosegui con l'aggiornamento sul seggio
        this->busyPV[currentPV-1]=false;

        this->mutex_stdout.lock();
        cout << "Seggio: postazione: " << currentPV <<" -> liberata" << endl;
        this->mutex_stdout.unlock();

        for(unsigned int index = 0; index < this->busyHT.size(); index++){
            if(this->snHTAttivi[index] == snHT){ //se l'idHTAttivo corrente corrisponde con l'id dell'HT da liberare
                this->busyHT[index] = false; //lo settiamo a libero nell'indice degli HT occupati
                this->mutex_stdout.lock();
                cout << "Seggio: token: " << snHT <<" -> liberato" << endl;
                this->mutex_stdout.unlock();
                break;
            }
        }
        uint matricolaToReset;
        matricolaToReset = this->listAssociazioni.at(indexAss).getMatricola();

        //comunica all'urna l'annullamento dell'operazione di voto per la matricola estratta dall'associazione da eliminare
        this->abortVoting(matricolaToReset,motivoAbort::rimozioneAssociazione);

        //elimino dall'heap vector l'istanza dell'associazione rimossa
        //l'eliminazione dal vettore di un elemento libera anche la memoria
        //perchè il vettore listAssociazioni è un vettore di oggetti Associazione e non un vettore di puntatori di tipo Associazione
        this->listAssociazioni.erase(this->listAssociazioni.begin()+indexAss);

        //prova di pulizia
        this->associazioniRimovibili.clear();

        emit anyPVFree(this->anyPostazioneLibera());

        emit anyAssociationRemovable(this->anyAssociazioneEliminabile());
    }
    else{
        cout << "Seggio: Unable to remove association from PV" << endl;
    }
}

void Seggio::completaOperazioneVoto(uint idPV)
{
    //libera la postazione di voto indicata

    unsigned int indexAss = 0;
    unsigned int currentPV = 0;
    string snHT = "";
    for (unsigned int i = 0; this->listAssociazioni.size();i++){
        currentPV = this->listAssociazioni.at(i).getIdPV();

        //trova l'associazione che corrisponde alla postazione di voto da liberare
        if(currentPV==idPV){
            //salva l'id dell'hardware token da rendere libero
            snHT = this->listAssociazioni.at(i).getSnHT();
            //salva l'indice corrispondente per l'operazione di liberazione da fare dopo aver comunicato alla postazione di voto l'azione da intraprendere
            indexAss = i;
            break; //interrompe l'esecuzione del for in cui è immediatemente contenuto
            // quindi in currentPV resta l'id della postazione per cui rimuovere l'associazione
        }
    }

    //comunica alla postazione voto la rimozione dell'associazione
    if(freePVpostVotazione(idPV)){
        //se la liberazione sulla postazione è andata a buon fine, prosegui con l'aggiornamento sul seggio

        //setta a libera la postazione di voto
        this->busyPV[currentPV-1]=false;

        this->mutex_stdout.lock();
        cout << "Seggio: postazione: " << currentPV <<" -> liberata" << endl;
        this->mutex_stdout.unlock();

        bool relaesedHT = false;
        for(unsigned int index = 0; index < this->busyHT.size(); index++){
            if(this->snHTAttivi[index] == snHT){ //se il sn dell'HTAttivo corrente corrisponde con il sn dell'HT da liberare
                this->busyHT[index] = false; //lo settiamo a libero nell'indice degli HT occupati
                this->mutex_stdout.lock();
                cout << "Seggio: token: " << snHT <<" -> liberato" << endl;
                this->mutex_stdout.unlock();
                relaesedHT = true;
                break;
            }
        }

        if(!relaesedHT){
            cerr << "non è stato possibile rilasciare il token";
            exit(1);
        }


        //elimino dall'heap vector l'istanza dell'associazione rimossa
        //l'eliminazione dal vettore di un elemento libera anche la memoria
        //perchè il vettore listAssociazioni è un vettore di oggetti Associazione e non un vettore di puntatori di tipo Associazione
        this->listAssociazioni.erase(this->listAssociazioni.begin()+indexAss);

        //prova di pulizia
        this->associazioniRimovibili.clear();

        emit anyPVFree(this->anyPostazioneLibera());

        emit anyAssociationRemovable(this->anyAssociazioneEliminabile());

    }
    else{
        cout << "Seggio: Unable to remove association from PV" << endl;
    }
}

void Seggio::tryVote(uint matricola)
{
    SSLClient * seggio_client = new SSLClient(this);

    if(seggio_client->connectTo(ipUrna.c_str())!=nullptr){
        uint IdTipoVotante;
        uint esito = seggio_client->queryTryVote(matricola,IdTipoVotante);
        if(esito == esitoLock::locked){
            nuovaAssociazione->setIdTipoVotante(IdTipoVotante);
            emit allowVote();
            //chiamo la funzione che aggiunge l'associazione alla lista delle associazione attuali e invia i dati alla postazione di voto
            if(addAssociazioneHT_PV(matricola)){
                emit allowVote();
            }
            else{
                uint idPV = nuovaAssociazione->getIdPV();
                setPVstate(idPV,statiPV::non_raggiungibile);
                emit errorPV(idPV);
            }
        }
        else{
            switch(esito){
            case esitoLock::alredyLocked:
                emit forbidVote("sta già votando");
                break;
            case esitoLock::alredyVoted:
                emit forbidVote("ha già votato");
                break;
            case esitoLock::notExist:
                emit forbidVote("non presente nell'anagrafica");
                break;
            case esitoLock::errorLocking:
                emit forbidVote("prenotazione di votazione non riuscita, riprovare");
                break;
            }

        }
    }
    else{
        emit urnaNonRaggiungibile();
    }
    delete seggio_client;
}


//void Seggio::readVoteResults(unsigned int idProceduraVoto){
//    //servizio da richiedere all'urna virtuale
//    //TODO contattare l'urna per ottenere i risultati di voto
//}

unsigned int Seggio::stateInfoPV(unsigned int idPV){
    //restituisce lo stato della postazione di voto con l'id indicato
    unsigned int stato = 0 ;

    this->mutex_stati.lock();
    stato = this->statoPV[idPV -1];
    this->mutex_stati.unlock();

    return stato;
}

//bool Seggio::feedbackFreeBusy(unsigned int idPV){
//    idPV = 1;
//    return true;
//}

bool Seggio::anyPostazioneLibera(){
    for(unsigned int i = 0; i < busyPV.size(); ++i){
        if (busyPV[i] == false){
            return true;
        }
    }
    return false;
}

bool Seggio::anyAssociazioneEliminabile(){
    for(unsigned int i=0; i< this->listAssociazioni.size(); ++i){
        unsigned int idPV = this->listAssociazioni.at(i).getIdPV();

        unsigned int statoPV = stateInfoPV(idPV);

        cout << "Seggio: pv #" << idPV << ": stato " << statoPV << endl;
        if(( statoPV == attesa_abilitazione) || ( statoPV == errore) || ( statoPV == offline)){
            cout << "Seggio: trovata almeno una associazione eliminabile" << endl;
            return true;
        }

    }

    return false;
}

void Seggio::disattivaHT(string snHTdaDisattivare){
    //disattiva l'HT passato come parametro e attiva quello di riserva
    for (uint i = 0; i < snHTAttivi.size(); i++){
        if(snHTAttivi.at(i) == snHTdaDisattivare){
            snHTAttivi.at(i) = snHTRiserva;
            snHTRiserva = snHTdaDisattivare;
            cout << "Token: " << snHTRiserva << " -> disattivato" << endl;
            cout << "Token: " << snHTAttivi.at(i) << " -> attivato" << endl;
            emit scambiati(snHTdaDisattivare,snHTAttivi.at(i));
            break;
        }
    }
    emit
}

bool Seggio::isBusyHT(string  snHT){
    for(unsigned int index = 0; index < this->busyHT.size(); index++){
        if(this->snHTAttivi[index] == snHT){

            return this->busyHT[index];
        }
    }
    cerr << "Seggio: isBusyHT: verifica stato HT fallita" << endl;
    return false;
}

void Seggio::calcolaHTdisattivabili(){
    vector <string> htDisattivabili;

    for (unsigned int i  = 0; i < snHTAttivi.size();i++){

        if(!isBusyHT(snHTAttivi.at(i))){
            htDisattivabili.push_back(snHTAttivi.at(i));
        }
    }

    emit readyHTDisattivabili(htDisattivabili,snHTRiserva);
}

void Seggio::setPVstate(unsigned int idPV, unsigned int nuovoStatoPV){

    if(idPV > 0 && idPV <= NUM_PV){ //NUM_PV = 3
        this->mutex_stati.lock();
        this->statoPV[idPV-1] = nuovoStatoPV;
        this->mutex_stati.unlock();
        cout << "Seggio: stato " <<  nuovoStatoPV << " impostato alla postazione " << idPV << endl;
    }
    else{
        cerr << "id Postazione di Voto non valido" << endl;
        exit(EXIT_FAILURE);
    }


    switch(nuovoStatoPV) {

    case statiPV::libera :
        this->busyPV[idPV-1]=false;
        break;
    default:
        this->busyPV[idPV-1]=true;
        break;
    }
    //richiama la funzione della view che si occupa di aggiornare la grafica delle postazioni
    //e la conseguente abilitazione o disabilitazione di bottoni(ovvero attivazione o
    //disattivazione di funzionalità relazionate allo stato delle postazioni di voto
    cout << "Seggio: emetto il segnale di cambio stato"  << endl;

    //emettere un segnale
    emit stateChanged(idPV,nuovoStatoPV);
    //mainWindow->updatePVbuttons(idPV);

    if (this->anyPostazioneLibera()){
        emit anyPVFree(true);
    }
    else{
        emit anyPVFree(false);
    }

    if(this->anyAssociazioneEliminabile()){
        emit anyAssociationRemovable(true);
    }
    else{
        emit anyAssociationRemovable(false);
    }

}

string Seggio::getIP_PV(unsigned int idPV){
    return this->IP_PV.at(idPV-1);
}

void Seggio::runServerPV(){
    //funzione eseguita da un thread
    this->seggio_server = new SSLServer(this);
    this->mutex_stdout.lock();
    cout << "Seggio: avviato il ServerSeggio per ricevere gli update dello stato delle Postazioni di Voto" << endl;
    this->mutex_stdout.unlock();

    while(!(this->stopServer)){//se stopServer ha valore vero il while termina

        //attesa di una richiesta
        this->seggio_server->ascoltoNuovoStatoPV();

        //prosegue rimettendosi in ascolto al ciclo successivo, se stopServer è falso
    }

    this->mutex_stdout.lock();
    cout << "Seggio: runServerPV: exit!" << endl;
    this->mutex_stdout.unlock();


    //delete this->seggio_server;
    // il thread che eseguiva la funzione termina se la funzione arriva alla fine
    delete this->seggio_server;
    return;
}


void Seggio::stopServerPV(){

    //creo client per contattare il server
    SSLClient * seggio_client = new SSLClient(this);

    //predispongo il thread all'uscita del loop while
    this->stopServer = true;

    //predispongo il server per l'interruzione
    this->seggio_server->setStopServer(true);

    this->mutex_stdout.lock();
    cout << "Seggio: il server sta per essere fermato" << endl;
    this->mutex_stdout.unlock();

    //mi connetto al server locale per sbloccare l'ascolto e portare alla terminazione della funzione eseguita dal thread che funge da serve in ascolto
    //const char * localhost = "127.0.0.1";
    seggio_client->stopLocalServer(/*localhost*/);

    delete seggio_client;
}


bool Seggio::pushAssociationToPV(unsigned int idPV, string snHT, unsigned int idTipoVotante, uint matricola, string usernameHT, string passwordHT){


    cout << this->IP_PV.at(idPV - 1);
    string ip_pv = this->IP_PV.at(idPV - 1);
    bool res = false;
    SSLClient *seggio_client = new SSLClient(this);
    if(seggio_client->connectTo(ip_pv.c_str())!= nullptr){
        //richiede il settaggio della associazione alla postazione di voto a cui il client del seggio si è connesso
        res = seggio_client->querySetAssociation(snHT,idTipoVotante,matricola,usernameHT,passwordHT);
    }
    else{

        this->setPVstate(idPV,statiPV::non_raggiungibile);

    }
    delete seggio_client;

    return res;
}

bool Seggio::removeAssociationFromPV(unsigned int idPV){
    string ip_pv = this->IP_PV.at(idPV-1);
    bool res = false;
    SSLClient *seggio_client = new SSLClient(this);
    if(seggio_client->connectTo(ip_pv.c_str())!= nullptr){

        //richiede la rimozione della associazione alla postazione di voto a cui il client del seggio si è connesso
        res = seggio_client->queryRemoveAssociation();
    }
    delete seggio_client;
    return res;
}

bool Seggio::freePVpostVotazione(unsigned int idPV)
{
    string ip_pv = this->IP_PV.at(idPV-1);
    bool res = false;
    SSLClient *seggio_client = new SSLClient(this);
    if(seggio_client->connectTo(ip_pv.c_str())!= nullptr){

        //richiede la rimozione della associazione alla postazione di voto a cui il client del seggio si è connesso
        res = seggio_client->queryFreePV();
    }
    delete seggio_client;
    return res;
}

void Seggio::pullStatePV(unsigned int idPV){
    const char* ip_pv = this->calcolaIP_PVbyID(idPV).c_str();

    //bool res = false;
    this->mutex_stdout.lock();
    cout << "Seggio: tento la connessione alla PV " << idPV << endl;
    this->mutex_stdout.unlock();
    SSLClient *seggio_client = new SSLClient(this);
    if(seggio_client->connectTo(ip_pv)!= nullptr){
        this->mutex_stdout.lock();
        cout << "Seggio: PV " << idPV << " raggiungibile, chiedo lo stato" << endl;
        this->mutex_stdout.unlock();
        //richiede lo stato della postazione di voto a cui il client del seggio si è connesso
        int stato = seggio_client->queryPullPVState();
        this->setPVstate(idPV,stato);
        //res = true;
    }
    else{
        this->mutex_stdout.lock();
        cout << "Seggio: PV " << idPV << " non raggiungibile!" << endl;
        this->mutex_stdout.unlock();

        this->setPVstate(idPV,statiPV::non_raggiungibile);


    }

    this->mutex_stdout.lock();
    cout << "Seggio: Fine Pull della PV" << idPV << endl;
    this->mutex_stdout.unlock();

    delete seggio_client;
    //return res;
}
void Seggio::aggiornaPVs(){


    //funzione eseguita da un thread

    this->mutex_stdout.lock();
    cout << "Seggio: avviato il thread che richiede periodicamente lo stato delle postazioni di voto" << endl;
    this->mutex_stdout.unlock();

    while(this->logged){
        for (unsigned int i = 1; i <= NUM_PV; i++){
            this->pullStatePV(i);
        }

        std::this_thread::sleep_for (std::chrono::seconds(10));
    }

    this->mutex_stdout.lock();
    cout << "Seggio: pullClient: exit!" << endl;
    this->mutex_stdout.unlock();
    return;
}

string Seggio::getPublicKeyRP() const
{
    return publicKeyRP;
}

void Seggio::setPublicKeyRP(const string &value)
{
    publicKeyRP = value;
}

vector<HardwareToken> Seggio::getGeneratoriOTP() const
{
    return generatoriOTP;
}

void Seggio::setGeneratoriOTP(const vector<HardwareToken> &value)
{
    generatoriOTP = value;
}

void Seggio::addGeneratoreOTP(HardwareToken &ht)
{
    this->generatoriOTP.push_back(ht);
}

uint Seggio::getIndexHTBySN(string sn)
{
    uint indexHT = 0;
    for (uint i = 0; i < generatoriOTP.size(); i++){
        if(sn == generatoriOTP.at(i).getSN()){
            //indice ht token trovato
            indexHT = i;
            break;
        }
    }
    return indexHT;
}

string Seggio::getSnHTRiserva() const
{
    return snHTRiserva;
}

void Seggio::setSnHTRiserva(const string &value)
{
    snHTRiserva = value;
}

string Seggio::getSessionKey_Seggio_Urna() const
{
    return sessionKey_Seggio_Urna;
}

void Seggio::setSessionKey_Seggio_Urna(const string &value)
{
    sessionKey_Seggio_Urna = value;
}

void Seggio::validatePassKey(QString pass)
{
    SSLClient * seggio_client = new SSLClient(this);

    if(seggio_client->connectTo(ipUrna.c_str())!=nullptr){

        if(seggio_client->queryAttivazioneSeggio(pass.toStdString())){
            emit validPass();
            sessionKey_Seggio_Urna = pass.toStdString();
            for (uint i = 0; i < NUM_HT_ATTIVI; i++){
                this->snHTAttivi.at(i) = this->generatoriOTP.at(i).getSN();
            }
            this->snHTRiserva = this->generatoriOTP.at(NUM_HT_ATTIVI).getSN();
        }
        else{
            emit wrongPass();
        }
    }
    else{
        emit urnaNonRaggiungibile();
    }
    delete seggio_client;
}

void Seggio::matricolaState(uint matricola)
{
    SSLClient * seggio_client = new SSLClient(this);

    if(seggio_client->connectTo(ipUrna.c_str())!=nullptr){
        string nome, cognome;
        uint stato;
        if(seggio_client->queryInfoMatricola(matricola,nome, cognome,stato)){
            string s;
            switch(stato){
            case statoVoto::non_espresso:
                s = "non ha votato";
                break;
            case statoVoto::votando:
                s = "sta votando";
                break;
            case statoVoto::espresso:
                s = "ha votato";

            }

            string info = nome + " " + cognome + ", "+ s;
            emit matricolaStateReceived(QString::fromStdString(info));
        }
        else{
            emit matricolaStateReceived("assente");
        }
    }
    else{
        emit urnaNonRaggiungibile();
    }

    delete seggio_client;
}
void Seggio::abortVoting(uint matricola, uint situazione)
{
    SSLClient * seggio_client = new SSLClient(this);

    if(seggio_client->connectTo(ipUrna.c_str())!=nullptr){
        if (seggio_client->queryResetMatricolaState(matricola)){
            emit successAbortVoting(situazione);
        }
        else{
            emit errorAbortVoting(matricola);
        }

    }
    else{
        emit errorAbortVoting(matricola);
    }

    delete seggio_client;
}

void Seggio::risultatiVoto()
{
    vector <RisultatiSeggio> risultatiSeggi;
    SSLClient * seggio_client = new SSLClient(this);
    string risultatiVotoXML, encodedSignRP;
    if(seggio_client->connectTo(ipUrna.c_str())!=nullptr){
        if (seggio_client->queryRisultatiVoto(this->idProceduraVoto,risultatiVotoXML, encodedSignRP)){
            int verifica = this->verifySignString_RP(risultatiVotoXML, encodedSignRP,this->publicKeyRP);
            if(verifica == 0){
                this->parsingScrutinioXML(risultatiVotoXML, &risultatiSeggi);
                emit readyRisultatiSeggi(risultatiSeggi);
            }
        }
        else{
            emit notScrutinio();
            //scrutinio non ancora eseguito
        }
    }

    delete seggio_client;
}

void Seggio::tryLogout(){
    bool allowLogout = true;
    unsigned int statoPV;
    for (unsigned int idPV = 1; idPV <= NUM_PV; idPV++){
        statoPV = this->stateInfoPV(idPV);

        switch(statoPV){

        case this->statiPV::attesa_abilitazione:
        case this->statiPV::votazione_in_corso:
        case this->statiPV::votazione_completata:
            allowLogout = false;
            break;

        default:

            //in tutti gli altri casi
            //il valore di allowLogout resta a true, e si può procedere con il logout
            break;
        }
        if(!allowLogout){
            //logout vietato, emissione segnale
            emit forbidLogout();
            //non bisogna controllare stati di altre postazioni di voto  se ve ne è già uno che impedisce il logout
            //quindi il compito della funzione è concluso
            return;

        }

    }
    //se siamo arrivati qui, è possibile effettuare il logout

    //interruzione del thread server
    this->stopServerPV();

    //verificare se la sessione conclusa è l'ultima
    SSLClient * seggio_client = new SSLClient(this);

    if(seggio_client->connectTo(ipUrna.c_str())!=nullptr){
        uint esito = 0;
        if(seggio_client->queryNextSessione(this->idProceduraVoto,esito)){
            if(esito == 1){
                //altra sessione
                emit grantLogout(this->dtAperturaSessione,this->dtChiusuraSessione);
            }
            else if(esito == 2){
                emit toPageRisultati();
            }
        }
        else{
            emit grantLogout();
        }
    }
    else{
        emit urnaNonRaggiungibile();
    }
    delete seggio_client;



}

void Seggio::setLogged(bool value)
{
    logged = value;
}

void Seggio::calculateRemovableAssociations()
{
    //così mi sa che non funziona
    //std::vector <Associazione> associazioniRimovibili;
    associazioniRimovibili.clear();

    for(unsigned i=0; i < listAssociazioni.size(); ++i){
        //cout << i << endl;
        //ottengo id postazione di voto dell'associazione corrente
        unsigned int idPV = listAssociazioni.at(i).getIdPV();
        //ottengo lo stato della postazione di voto relativa
        unsigned int statoPV = this->stateInfoPV(idPV);


        if(( statoPV == attesa_abilitazione) || ( statoPV == errore)||( statoPV == offline)){
            //se la postazione di voto ha uno stato che consente la rimozione dell'associazione
            //aggiungo l'associazione al vettore delle associazioni rimovibili
            cout << "Seggio: associazione rimovibile alla PV: "<< idPV << endl;
            associazioniRimovibili.push_back(listAssociazioni.at(i));

        }
    }

    emit removableAssociationsReady(associazioniRimovibili);
}

string Seggio::calcolaMAC(string encodedSessionKey, string plainText){


    //"11A47EC4465DD95FCD393075E7D3C4EB";

    cout << "Seggio: Session key: " << encodedSessionKey << endl;
    string decodedKey;
    StringSource (encodedSessionKey,true,
                  new HexDecoder(
                      new StringSink(decodedKey)
                      ) // HexDecoder
                  ); // StringSource

    SecByteBlock key(reinterpret_cast<const byte*>(decodedKey.data()), decodedKey.size());


    string macCalculated, encoded;

    /*********************************\
    \*********************************/

    // Pretty print key
    encoded.clear();
    StringSource(key, key.size(), true,
                 new HexEncoder(
                     new StringSink(encoded)
                     ) // HexEncoder
                 ); // StringSource
    cout << "Seggio: key encoded: " << encoded << endl;

    cout << "Seggio: plain text: " << plainText << endl;

    /*********************************\
    \*********************************/

    try
    {
        CryptoPP::HMAC< CryptoPP::SHA256 > hmac(key, key.size());

        StringSource(plainText, true,
                     new HashFilter(hmac,
                                    new StringSink(macCalculated)
                                    ) // HashFilter
                     ); // StringSource
    }
    catch(const CryptoPP::Exception& e)
    {
        cerr << e.what() << endl;

    }

    /*********************************\
    \*********************************/

    // Pretty print MAC
    string macEncoded;
    StringSource(macCalculated, true,
                 new HexEncoder(
                     new StringSink(macEncoded)
                     ) // HexEncoder
                 ); // StringSource
    cout << "Seggio: hmac encoded: " << macEncoded << endl;

    verifyMAC(encodedSessionKey,plainText, macEncoded);

    return macEncoded;
}

int Seggio::verifyMAC(string encodedSessionKey,string data, string macEncoded){
    int success = 0;
    cout << "Seggio: Dati da verificare: " << data << endl;
    cout << "Seggio: mac da verificare: " << macEncoded << endl;

    string decodedKey;
    cout << "Seggio: Session key: " << encodedSessionKey << endl;

    StringSource (encodedSessionKey,true,
                  new HexDecoder(
                      new StringSink(decodedKey)
                      ) // HexDecoder
                  ); // StringSource

    SecByteBlock key2(reinterpret_cast<const byte*>(decodedKey.data()), decodedKey.size());

    string macDecoded;
    StringSource(macEncoded, true,
                 new HexDecoder(
                     new StringSink(macDecoded)
                     ) // HexDecoder
                 ); // StringSource
    cout << "Seggio: hmac decoded: " << macDecoded << endl;

    try
    {
        CryptoPP::HMAC< CryptoPP::SHA256 > hmac(key2, key2.size());
        const int flags = HashVerificationFilter::THROW_EXCEPTION | HashVerificationFilter::HASH_AT_END;



        StringSource(data + macDecoded, true,
                     new HashVerificationFilter(hmac, NULL, flags)
                     ); // StringSource

        cout << "Seggio: Verified message" << endl;
        success = 0;
    }
    catch(const CryptoPP::Exception& e)
    {
        success = 1;
        cerr << e.what() << endl;
        exit(1);
    }
    return success;
}


string Seggio::getIPbyInterface(const char * interfaceName){
    struct ifaddrs *ifaddr, *ifa;
    int /*family,*/ s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }


    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        s=getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

        if((strcmp(ifa->ifa_name,interfaceName)==0)&&(ifa->ifa_addr->sa_family==AF_INET))
        {
            if (s != 0)
            {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            printf("\tInterface : <%s>\n",ifa->ifa_name );
            printf("\t  Address : <%s>\n", host);
        }
    }

    freeifaddrs(ifaddr);
    string ip_host = host;
    return ip_host;
}

string Seggio::calcolaIP_PVbyID(uint idPostazione)
{
    string ipSeggio = this->getIPbyInterface("enp0s8");
    int byte1, byte2, byte3, byte4;
    char dot;
    istringstream s(ipSeggio);  // input stream that now contains the ip address string

    s >> byte1 >> dot >> byte2 >> dot >> byte3 >> dot >> byte4 >> dot;



    //aggiungendo l'idPostazione all byte meno significativo dell'ipSeggio si ottiene l'ip della postazione di voto con un certo id
    int byte4PV = byte4 + idPostazione;

    string ipPV = to_string(byte1) + "." + to_string(byte2) + "." + to_string(byte3) + "." +  to_string(byte4PV);
    cout << "PV: ip postazione voto " << idPostazione << ": " << ipPV << endl;
    return ipPV;
}

int Seggio::verifySignString_RP(string data, string encodedSignature,
                                string encodedPublicKey) {

    int success = 1; //non verificato
    string signature;
    StringSource(encodedSignature,true,
                 new HexDecoder(
                     new StringSink(signature)
                     )//HexDecoder
                 );//StringSource
    cout << "Signature encoded: " << encodedSignature << endl;
    cout << "Signature decoded: " << signature << endl;

    string decodedPublicKey;
    StringSource(encodedPublicKey,true,
                 new HexDecoder(
                     new StringSink(decodedPublicKey)
                     )//HexDecoder
                 );//StringSource

    ////------ verifica signature
    StringSource ss(decodedPublicKey,true /*pumpAll*/);
    CryptoPP::RSA::PublicKey publicKey;
    publicKey.Load(ss);

    cout << "PublicKey encoded: " << encodedPublicKey << endl;
    ////////////////////////////////////////////////
    try{
        // Verify and Recover
        CryptoPP::RSASS<CryptoPP::PSS, CryptoPP::SHA256>::Verifier verifier(publicKey);
        cout << "Data to sign|signature: " << data + signature << endl;
        StringSource(data + signature, true,
                     new SignatureVerificationFilter(verifier, NULL,
                                                     SignatureVerificationFilter::THROW_EXCEPTION) // SignatureVerificationFilter
                     );// StringSource

        cout << "Verified signature on message" << endl;
        success = 0; //verificato
    } // try

    catch (CryptoPP::Exception& e) {
        cerr << "Error: " << e.what() << endl;
        success = 1;
    }
    return success;
}

void Seggio::parsingScrutinioXML(string &risultatiVotoXML, vector <RisultatiSeggio> *risultatiSeggi)
{
    XMLDocument xmlDoc;
    xmlDoc.Parse(risultatiVotoXML.c_str());

    //nodo Scrutinio
    XMLNode *rootNode = xmlDoc.FirstChild();

    XMLNode *risultatiNode = rootNode->FirstChild();

    XMLElement * risultatoElement = risultatiNode->FirstChildElement("risultatoSeggio");
    bool oneMoreRisultatoElement = true;
    while(oneMoreRisultatoElement){
        oneMoreRisultatoElement = false;
        RisultatiSeggio rs;

        XMLText * textNodeIdSeggio = risultatoElement->FirstChildElement("idSeggio")->FirstChild()->ToText();
        uint idSeggio = atoi(textNodeIdSeggio->Value());
        cout << "Risultati seggio " << idSeggio << endl;
        rs.setIdSeggio(idSeggio);

        XMLElement *schedeElement = risultatoElement->FirstChildElement("schede");

        XMLElement *schedaRisultatoElement = schedeElement->FirstChildElement("schedaRisultato");
        bool oneMoreSchedaRisultato = true;
        vector <SchedaVoto> * schedeRisultato = rs.getPointerSchedeVotoRisultato();
        while(oneMoreSchedaRisultato){
            oneMoreSchedaRisultato = false;
            SchedaVoto svr;

            XMLText * textNodeIdScheda = schedaRisultatoElement->FirstChildElement("id")->FirstChild()->ToText();
            uint id = atoi(textNodeIdScheda->Value());
            svr.setId(id);

            XMLText * textNodeDesc = schedaRisultatoElement->FirstChildElement("descrizioneElezione")->FirstChild()->ToText();
            string descrizioneElezione = textNodeDesc->Value();
            svr.setDescrizioneElezione(descrizioneElezione);

            cout << "scheda " << id << ": " << descrizioneElezione <<  endl;

            XMLElement * listeElement = schedaRisultatoElement->FirstChildElement("liste");

            XMLElement * firstListaElement = listeElement->FirstChildElement("lista");
            XMLElement * lastListaElement = listeElement->LastChildElement("lista");

            XMLElement *listaElement = firstListaElement;
            bool lastLista = false;
            do{

                int idLista = listaElement->IntAttribute("id");
                cout << "PV:  --- lista trovata" << endl;
                cout << "PV: id Lista: " << idLista << endl;
                string nomeLista = listaElement->Attribute("nome");
                cout << "PV: nome: " << nomeLista << endl;

                XMLElement * firstCandidatoElement  = listaElement->FirstChildElement("candidato");
                XMLElement * lastCandidatoElement = listaElement->LastChildElement("candidato");

                XMLElement * candidatoElement = firstCandidatoElement;
                //ottengo tutti i candidati della lista
                bool lastCandidato = false;
                do{

                    XMLElement * matricolaElement = candidatoElement->FirstChildElement("matricola");
                    XMLNode * matricolaInnerNode = matricolaElement->FirstChild();
                    string matricola;
                    if(matricolaInnerNode!=nullptr){
                        matricola = matricolaInnerNode->ToText()->Value();
                    }

                    int numVoti = candidatoElement->IntAttribute("votiRicevuti");
                    //cout << matricola << endl;

                    XMLElement *nomeElement = matricolaElement->NextSiblingElement("nome");
                    XMLNode * nomeInnerNode = nomeElement->FirstChild();
                    string nome;
                    if(nomeInnerNode!=nullptr){
                        nome = nomeInnerNode->ToText()->Value();
                    }
                    //cout << nome << endl;

                    XMLElement *cognomeElement = nomeElement->NextSiblingElement("cognome");
                    XMLNode * cognomeInnerNode = cognomeElement->FirstChild();
                    string cognome;
                    if(cognomeInnerNode!=nullptr){
                        cognome = cognomeInnerNode->ToText()->Value();
                    }
                    //cout << cognome << endl;

                    XMLElement *luogoNascitaElement = cognomeElement->NextSiblingElement("luogoNascita");
                    XMLNode * luogoNascitaInnerNode = luogoNascitaElement->FirstChild();
                    string luogoNascita;
                    if(luogoNascitaInnerNode!=nullptr){
                        luogoNascita = luogoNascitaInnerNode->ToText()->Value();
                    }
                    //cout << luogoNascita << endl;

                    XMLElement *dataNascitaElement = luogoNascitaElement->NextSiblingElement("dataNascita");
                    XMLNode * dataNascitaInnerNode = dataNascitaElement->FirstChild();
                    string dataNascita;
                    if(dataNascitaInnerNode!=nullptr){
                        dataNascita = dataNascitaInnerNode->ToText()->Value();
                    }
                    //cout << dataNascita << endl;

                    cout << "PV: Estratti i dati del candidato id " << id;
                    //creo il candidato
                    cout << ": " << matricola << ", "<< nome << " " << cognome << ", " << luogoNascita << ", " << dataNascita << " ->" << numVoti << " voti." << endl;
                    Candidato c(nome,nomeLista,cognome,dataNascita,luogoNascita,matricola,numVoti);

                    svr.addCandidato(c);

                    //accesso al successivo candidato
                    if(candidatoElement == lastCandidatoElement){
                        lastCandidato = true;
                    }else {
                        candidatoElement = candidatoElement->NextSiblingElement("candidato");
                        cout << "PV: ottengo il puntatore al successivo candidato" << endl;
                    }
                }while(!lastCandidato);

                cout << "PV: non ci sono altri candidati nella lista: " << nomeLista << endl;


                if(listaElement == lastListaElement){
                    lastLista = true;
                }
                else{
                    //accediamo alla successiva lista nella scheda di voto
                    listaElement = listaElement->NextSiblingElement("lista");
                    cout << "PV: ottengo il puntatore alla successiva lista" << endl;
                }
            }while(!lastLista);
            cout << "PV: non ci sono altre liste" << endl;


            schedeRisultato->push_back(svr);

            if(schedaRisultatoElement->NextSiblingElement("schedaRisultato")!=nullptr){
                schedaRisultatoElement = schedaRisultatoElement->NextSiblingElement("schedaRisultato");
                oneMoreSchedaRisultato = true;
            }
        }

        risultatiSeggi->push_back(rs);

        if(risultatoElement->NextSiblingElement("risultatoSeggio")!=nullptr){
            risultatoElement = risultatoElement->NextSiblingElement("risultatoSeggio");
            oneMoreRisultatoElement = true;
        }
    }
}
