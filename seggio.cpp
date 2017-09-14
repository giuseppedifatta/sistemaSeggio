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

#include "seggio.h"
#include "sslclient.h"
#include "sslserver.h"

using namespace std;

Seggio::Seggio(QObject *parent):
    QThread(parent){
    ipUrna = "192.168.19.130";
    //    idProceduraVoto = 1;
    //    dataAperturaSessione.tm_mday = 15;
    //    dataAperturaSessione.tm_mon = 6;
    //    dataAperturaSessione.tm_year = 2017;
    //    dataAperturaSessione.tm_hour = 9;

    //    dataChiusuraSessione.tm_mday = 15;
    //    dataChiusuraSessione.tm_mon = 6;
    //    dataChiusuraSessione.tm_year = 2017;
    //    dataChiusuraSessione.tm_hour = 17;


    //solo per test, gli ht attivi per un certo seggio saranno impostati all'atto della creazione del seggio
    //non può essere il seggio a decidere, al momento dell'avvio del seggio stesso, quali sono gli HT attivi e quale quello di riserva
    //prevedere
    idHTAttivi[0]=1;
    idHTAttivi[1]=2;
    idHTAttivi[2]=3;
    idHTAttivi[3]=4;
    idHTRiserva = 5;

    for(unsigned int i = 0; i < NUM_PV; i++){
        idPostazioniVoto[i]=i+1;
        //busyPV[i]=true;
        PV_lastUsedHT[i] = 0;
        statoPV[i]=0;
    }

    //    for(unsigned int i = 0; i < NUM_HT_ATTIVI; i++){
    //        //il primo HT del vettore non è quello con id "1", ma quello con id memorizzato nella prima posizione del vettore idHTAttivi
    //        busyHT[i]=false;
    //    }
    nuovaAssociazione = NULL;

    //TODO calcolare usando come indirizzo base l'IP pubblico del seggio
    ////const char* IP_PV = this->calcolaIP_PVbyID(idPV);
    IP_PV[idPostazioniVoto[0]-1] = "192.168.56.101";
    IP_PV[idPostazioniVoto[1]-1] = "192.168.56.102";
    IP_PV[idPostazioniVoto[2]-1] = "192.168.56.103";

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

void Seggio::setIdProceduraVoto(unsigned int value)
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
    unsigned int idHT=this->nuovaAssociazione->getIdHT();

    for(unsigned int index = 0; index < this->busyHT.size(); index++){
        if(this->idHTAttivi[index] == idHT){ //se l'id dell'HTAttivo corrente corrisponde con l'id dell'HT da impegnare
            this->busyHT[index] = true; //lo settiamo ad occupato nell'indice degli HT occupati
        }
    }
    this->PV_lastUsedHT[idPV-1]=idHT;
    this->mutex_stdout.lock();
    cout << "Seggio: Hardware token: " << idHT << ", utilizzo su PV: " << idPV << endl;
    this->mutex_stdout.unlock();


    this->busyPV[idPV-1] = true;

    if(this->anyPostazioneLibera()){
        cout << "almeno una postazione libera" << endl;
        emit anyPVFree(true);
        //mainWindow->disableCreaAssociazioneButton();
    }
    else{
        emit anyPVFree(false);
    }

    if(anyAssociazioneEliminabile()){
        cout << "almeno una associazione eliminabile" << endl;
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
    unsigned int idHT = 0;
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

                        if(this->PV_lastUsedHT[indexPV]!=this->idHTAttivi[indexHT]){ //e l'HT non è stato utilizzato l'ultima volta su questa postazione, si può selezionare
                            cout << "Seggio: sulla postazione PV " << indexPV+1 << " l'ultimo token utilizzato è stato il " << this->PV_lastUsedHT[indexPV] << endl;
                            idHT=this->idHTAttivi[indexHT]; //seleziona l'i-esimo HT tra quelli attivi
                            cout << "Seggio: HT " << idHT << " selezionato" << endl;
                            associationFound = true; //associazione trovata

                            //creazione della nuova associazione
                            this->nuovaAssociazione = new Associazione(idPV,idHT);
                            cout << "Seggio: nuova associazione creata" << endl;
                            break; //interrompe il ciclo for
                        }
                    }
                }
            }
        }
    }

    if(idHT !=0 && idPV != 0){
        emit associationReady(idHT,idPV);
    }
}

bool Seggio::addAssociazioneHT_PV(uint matricola){
    bool pushed = false;
    //aggiunge un'associazione alla lista e aggiorna i flag su PV e HT occupati

    //bug nel caso in cui non si riesca a fare il push della associazione, risolvere!!!

    unsigned int idPV=this->nuovaAssociazione->getIdPV();
    unsigned int idHT=this->nuovaAssociazione->getIdHT();
    unsigned int ruolo = this->nuovaAssociazione->getRuolo();
    nuovaAssociazione->setMatricola(matricola);


    //comunicare alla postazione di competenza la nuova associazione
    //const char* IP_PV = this->calcolaIP_PVbyID(idPV);
    if(pushAssociationToPV(idPV,idHT, ruolo,matricola)){ //se la comunicazione dell'associazione alla PV è andata a buon fine
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
    unsigned int idHT = 0;
    for (unsigned int i = 0; this->listAssociazioni.size();i++){
        currentPV = this->listAssociazioni.at(i).getIdPV();

        //trova l'associazione che corrisponde alla postazione di voto per cui rimuovere l'associazione
        if(currentPV==idPV){
            //salva l'id dell'hardware token da rendere libero
            idHT = this->listAssociazioni.at(i).getIdHT();
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
            if(this->idHTAttivi[index] == idHT){ //se l'idHTAttivo corrente corrisponde con l'id dell'HT da liberare
                this->busyHT[index] = false; //lo settiamo a libero nell'indice degli HT occupati
                this->mutex_stdout.lock();
                cout << "Seggio: token: " << idHT <<" -> liberato" << endl;
                this->mutex_stdout.unlock();
                break;
            }
        }
        uint matricolaToReset;
        matricolaToReset = this->listAssociazioni.at(indexAss).getMatricola();

        //comunica all'urna l'annullamento dell'operazione di voto per la matricola estratta dall'associazione da eliminare
        abortVoting(matricolaToReset);

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
    unsigned int idHT = 0;
    for (unsigned int i = 0; this->listAssociazioni.size();i++){
        currentPV = this->listAssociazioni.at(i).getIdPV();

        //trova l'associazione che corrisponde alla postazione di voto da liberare
        if(currentPV==idPV){
            //salva l'id dell'hardware token da rendere libero
            idHT = this->listAssociazioni.at(i).getIdHT();
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
            if(this->idHTAttivi[index] == idHT){ //se l'idHTAttivo corrente corrisponde con l'id dell'HT da liberare
                this->busyHT[index] = false; //lo settiamo a libero nell'indice degli HT occupati
                this->mutex_stdout.lock();
                cout << "Seggio: token: " << idHT <<" -> liberato" << endl;
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

        //TODO comunicare al database con gli elettori attivi, che l'elettore associato alla postazione per cui si è liberata la postazione ha votato
    }
    else{
        cout << "Seggio: Unable to remove association from PV" << endl;
    }
}

void Seggio::tryVote(uint matricola)
{
    SSLClient * pv_client = new SSLClient(this);

    if(pv_client->connectTo(ipUrna)!=nullptr){
        uint ruolo;
        uint esito = pv_client->queryTryVote(matricola,ruolo);
        if(esito == esitoLock::locked){
            nuovaAssociazione->setRuolo(ruolo);
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
    delete pv_client;
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
        unsigned int idPV = this->listAssociazioni[i].getIdPV();

        unsigned int statoPV = stateInfoPV(idPV);

        cout << "Seggio: pv #" << idPV << ": stato " << statoPV << endl;
        if(( statoPV == attesa_abilitazione) || ( statoPV == errore) || ( statoPV == offline)){
            cout << "Seggio: trovata almeno una associazione eliminabile" << endl;
            return true;
        }

    }

    return false;
}

array<unsigned int, NUM_HT_ATTIVI> &Seggio::getArrayIdHTAttivi(){
    return this->idHTAttivi;
}

unsigned int Seggio::getIdHTRiserva(){
    return this->idHTRiserva;
}

void Seggio::disattivaHT(unsigned int tokenAttivo){
    //disattiva l'HT passato come parametro e attiva quello di riserva
    //MANCANTE!!! comunicazione modifica all'OTP server provider
    unsigned int temp = this->idHTRiserva;
    this->idHTRiserva=tokenAttivo;
    for (unsigned int i = 0; i < this->idHTAttivi.size(); i++){
        if(this->idHTAttivi[i] == tokenAttivo){
            this->idHTAttivi[i] = temp;
            this->mutex_stdout.lock();
            cout << "Seggio: token: " << tokenAttivo << " -> disabilitato" <<endl;

            cout << "Seggio: token: " << temp << " -> abilitato" <<endl;
            this->mutex_stdout.unlock();
            break;
        }
    }
}

bool Seggio::isBusyHT(unsigned int idHT){
    for(unsigned int index = 0; index < this->busyHT.size(); index++){
        if(this->idHTAttivi[index] == idHT){

            return this->busyHT[index];
        }
    }
    cerr << "Seggio: isBusyHT: verifica stato HT fallita" << endl;
    return true;
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

const char *Seggio::getIP_PV(unsigned int idPV){
    return this->IP_PV[idPV-1];
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

const char * Seggio::calcolaIP_PVbyID(unsigned int idPV){
    cout << idPV << endl;
    struct hostent *host;
    host = gethostbyname("localhost");
    struct sockaddr_in local_address;
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = *(long*) (host->h_addr);
    /* ---------------------------------------------------------- *
             * Zeroing the rest of the struct                             *
             * ---------------------------------------------------------- */
    memset(&(local_address.sin_zero), '\0', 8);

    const char * address_printable = inet_ntoa(local_address.sin_addr);


    this->mutex_stdout.lock();
    cout << "Seggio: IP locale del Seggio: " << address_printable << endl;


    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;

    int ret = getifaddrs(&ifAddrStruct);
    if (ret == 0){
        cout << "Seggio: getifaddrs success"<< endl;
    }
    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
        }
        /*else if (ifa->ifa_addr->sa_family == AF_INET6) { // check it is IP6
                    // is a valid IP6 Address
                    tmpAddrPtr=&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
                    char addressBuffer[INET6_ADDRSTRLEN];
                    inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
                    printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
                }*/
    }
    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
    this->mutex_stdout.unlock();


    //delete ifAddrStruct;
    //delete ifa;

    //delete host;
    return address_printable;
}

bool Seggio::pushAssociationToPV(unsigned int idPV, unsigned int idHT, unsigned int ruolo, uint matricola){


    const char* ip_pv = this->IP_PV[idPV-1];

    bool res = false;
    SSLClient *seggio_client = new SSLClient(this);
    if(seggio_client->connectTo(ip_pv)!= nullptr){
        //richiede il settaggio della associazione alla postazione di voto a cui il client del seggio si è connesso
        res = seggio_client->querySetAssociation(idHT,ruolo,matricola);
    }
    else{

        this->setPVstate(idPV,statiPV::non_raggiungibile);

    }
    delete seggio_client;
    return res;
}

bool Seggio::removeAssociationFromPV(unsigned int idPV){
    const char* ip_pv = this->IP_PV[idPV-1];
    bool res = false;
    SSLClient *seggio_client = new SSLClient(this);
    if(seggio_client->connectTo(ip_pv)!= nullptr){

        //richiede la rimozione della associazione alla postazione di voto a cui il client del seggio si è connesso
        res = seggio_client->queryRemoveAssociation();
    }
    delete seggio_client;
    return res;
}

bool Seggio::freePVpostVotazione(unsigned int idPV)
{
    const char* ip_pv = this->IP_PV[idPV-1];
    bool res = false;
    SSLClient *seggio_client = new SSLClient(this);
    if(seggio_client->connectTo(ip_pv)!= nullptr){

        //richiede la rimozione della associazione alla postazione di voto a cui il client del seggio si è connesso
        res = seggio_client->queryFreePV();
    }
    delete seggio_client;
    return res;
}

void Seggio::pullStatePV(unsigned int idPV){
    const char* ip_pv = this->IP_PV[idPV-1];

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

void Seggio::validatePassKey(QString pass)
{
    //TODO contattare il dbms degli accessi per controllare se la password è esatta
    //    if(pass == "qwerty"){
    //        cout << "Seggio: passKey corretta" << endl;
    //        emit validPass();
    //    }
    //    else {
    //        emit wrongPass();
    //    }
    //contatto l'urna per validare la password
    // 11A47EC4465DD95FCD393075E7D3C4EB


    SSLClient * pv_client = new SSLClient(this);

    if(pv_client->connectTo(ipUrna)!=nullptr){

        if(pv_client->queryAttivazioneSeggio(pass.toStdString())){
            emit validPass();
            sessionKey_Seggio_Urna = pass.toStdString();
        }
        else{
            emit wrongPass();
        }
    }
    else{
        emit urnaNonRaggiungibile();
    }
    delete pv_client;
}

void Seggio::matricolaState(uint matricola)
{
    SSLClient * pv_client = new SSLClient(this);

    if(pv_client->connectTo(ipUrna)!=nullptr){
        string nome, cognome;
        uint stato;
        if(pv_client->queryInfoMatricola(matricola,nome, cognome,stato)){
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

    delete pv_client;
}
void Seggio::abortVoting(uint matricola)
{
    SSLClient * pv_client = new SSLClient(this);

    if(pv_client->connectTo(ipUrna)!=nullptr){
        if (pv_client->queryResetMatricolaState(matricola)){
            emit successAbortVoting();
        }
        else{
            emit errorAbortVoting(matricola);
        }

    }
    else{
        emit errorAbortVoting(matricola);
    }

    delete pv_client;
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

    emit grantLogout();
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
            cout << "associazione rimovibile alla PV: "<< idPV << endl;
            associazioniRimovibili.push_back(listAssociazioni.at(i));

        }
    }

    emit removableAssociationsReady(associazioniRimovibili);
}

string Seggio::calcolaMAC(string encodedSessionKey, string plainText){


    //"11A47EC4465DD95FCD393075E7D3C4EB";

    cout << "Session key: " << encodedSessionKey << endl;
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
    cout << "key encoded: " << encoded << endl;

    cout << "plain text: " << plainText << endl;

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
    cout << "hmac encoded: " << macEncoded << endl;

    verifyMAC(encodedSessionKey,plainText, macEncoded);

    return macEncoded;
}

int Seggio::verifyMAC(string encodedSessionKey,string data, string macEncoded){
    int success = 0;
    cout << "Dati da verificare: " << data << endl;
    cout << "mac da verificare: " << macEncoded << endl;

    string decodedKey;
    cout << "Session key: " << encodedSessionKey << endl;

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
    cout << "hmac decoded: " << macDecoded << endl;

    try
    {
        CryptoPP::HMAC< CryptoPP::SHA256 > hmac(key2, key2.size());
        const int flags = HashVerificationFilter::THROW_EXCEPTION | HashVerificationFilter::HASH_AT_END;



        StringSource(data + macDecoded, true,
                     new HashVerificationFilter(hmac, NULL, flags)
                     ); // StringSource

        cout << "Verified message" << endl;
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
