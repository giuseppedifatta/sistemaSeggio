#include "seggio.h"

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
#include "sslclient.h"
#include "sslserver.h"

using namespace std;

Seggio::Seggio(MainWindowSeggio * m)
{
    mainWindow = m;
    idProceduraVoto = 1;
    dataAperturaSessione.tm_mday = 15;
    dataAperturaSessione.tm_mon = 6;
    dataAperturaSessione.tm_year = 2017;
    dataAperturaSessione.tm_hour = 9;

    dataChiusuraSessione.tm_mday = 15;
    dataChiusuraSessione.tm_mon = 6;
    dataChiusuraSessione.tm_year = 2017;
    dataChiusuraSessione.tm_hour = 17;


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
        busyPV[i]=true;
        PV_lastUsedHT[i] = 0;
    }

    for(unsigned int i = 0; i < NUM_HT_ATTIVI; i++){
        //il primo HT del vettore non è quello con id "1", ma quello con id memorizzato nella prima posizione del vettore idHTAttivi
        busyHT[i]=false;
    }
    nuovaAssociazione = NULL;

    //    this->IP_PV[0] = "localhost";
    //    this->IP_PV[1] = "localhost";
    //    this->IP_PV[2] = "localhost";

    //le postazioni devono essere attivate, quindi in fase di inizializzazione non sono disponibili per essere associate ad un hardware token
    this->statoPV[0] = statiPV::attesa_attivazione;
    this->statoPV[1] = statiPV::attesa_attivazione;
    this->statoPV[2] = statiPV::attesa_attivazione;


    this->stopThreads = false;
    thread_server = std::thread(&Seggio::runServerUpdatePV, this);


    //TODO calcolare usando come indirizzo base l'IP pubblico del seggio
    IP_PV[idPostazioniVoto[0]-1] = "192.168.56.101";
    IP_PV[idPostazioniVoto[1]-1] = "192.168.56.102";
    IP_PV[idPostazioniVoto[2]-1] = "192.168.56.103";


    this->seggio_client = new SSLClient(this);
}

Seggio::~Seggio(){

    delete this->seggio_client;
    delete nuovaAssociazione;

    this->mutex_stdout.lock();
    cout << "Seggio: join del thread_server" << endl;
    this->mutex_stdout.unlock();

    thread_server.join();


}

//void Seggio::setStopThreads(bool b) {
//    this->stopThreads = b;
//}

void Seggio::setBusyHT_PV(){
    unsigned int idPV=this->nuovaAssociazione->getIdPV();
    unsigned int idHT=this->nuovaAssociazione->getIdHT();

    //comunicare alla postazione di competenza la nuova associazione
    //const char* IP_PV = this->calcolaIP_PVbyID(idPV);

    //se il push dell'associazioe è andato a buon fine...
    //segna come occupate le postazioni corrente, e memorizza la postazione di ultimo utilizzo per l'HT

    for(unsigned int index = 0; index < this->busyHT.size(); index++){
        if(this->idHTAttivi[index] == idHT){ //se l'id dell'HTAttivo corrente corrisponde con l'id dell'HT da impegnare
            this->busyHT[index] = true; //lo settiamo ad occupato nell'indice degli HT occupati
        }
    }
    this->PV_lastUsedHT[idPV-1]=idHT;
    this->mutex_stdout.lock();
    cout << "Seggio: Hardware token: " << idHT << ", ultimo utilizzo su PV: " << idPV << endl;
    this->mutex_stdout.unlock();
    if(this->busyPV[idPV-1] == false){
        this->busyPV[idPV-1]=true;
    }
    else {
        cerr << "postazione già impegnata, runtime error" << endl;
        exit(EXIT_FAILURE);
    }





    if(!this->anyPostazioneLibera()){
        mainWindow->disableCreaAssociazioneButton();
    }

}

bool Seggio::createAssociazioneHT_PV(){
    //crea un associazione in base alle risorse disponibili
    //lo stesso HT non viene assegnato due volte consecutive alla stessa PV
    unsigned int idPV;
    unsigned int idHT;
    bool associationFound = false;
    for(unsigned int indexPV = 0; indexPV < this->busyPV.size();indexPV++){
        if (associationFound==true){
            break; //interrompe il ciclo for
        }
        else {
            bool pv_occupata = this->busyPV[indexPV];
            this->mutex_stati.lock();
            unsigned int pv_stato = this->stateInfoPV(indexPV+1);
            this->mutex_stati.unlock();

            if((!pv_occupata) && (pv_stato!=statiPV::attesa_attivazione) && (pv_stato==statiPV::libera)){ // se la postazione non è occupata
                //seleziona la postazione se è attivata e libera(disponibile per una associazione)
                this->mutex_stdout.lock();
                cout << "Seggio: PV " << indexPV+1 << " candidata alla associazione trovata" << endl;
                this->mutex_stdout.unlock();
                idPV=indexPV+1; //postazione selezionata
                for (unsigned int indexHT = 0; indexHT < this->busyHT.size();indexHT++){
                    if (this->busyHT[indexHT]==false){ // se l'HT non è impegnato
                        if(this->PV_lastUsedHT[indexPV]!=this->idHTAttivi[indexHT]){ //e l'HT non è stato utilizzato l'ultima volta su questa postazione, si può selezionare
                            idHT=this->idHTAttivi[indexHT]; //seleziona l'i-esimo HT tra quelli attivi
                            cout << "Seggio: HT " << idHT << " selezionato" << endl;
                            associationFound = true; //associazione trovata

                            //creazione della nuova associazione
                            this->nuovaAssociazione = new Associazione(idPV,idHT);

                            break; //interrompe il ciclo for
                        }
                    }
                }
            }
        }
    }
    return associationFound;
}

void Seggio::addAssociazioneHT_PV(){
    //aggiunge un'associazione alla lista e aggiorna i flag su PV e HT occupati

    //back nel caso in cui non si riesca a fare il push della associazione, risolvere!!!
    //
    unsigned int idPV=this->nuovaAssociazione->getIdPV();
    unsigned int idHT=this->nuovaAssociazione->getIdHT();

    //comunicare alla postazione di competenza la nuova associazione
    //const char* IP_PV = this->calcolaIP_PVbyID(idPV);
    if(pushAssociationToPV(idPV,idHT)){
        //aggiungiamo la nuova associazione alla lista delle associazioni correnti
        this->listAssociazioni.push_back(*this->nuovaAssociazione);

        //settiamo ad occupati ht e pv
        this->setBusyHT_PV();
    }

    //liberiamo la memoria della nuova associazione, nel caso in cui sia stata aggiunta alla lista delle associazioni ne è stata creata una copia nel vettore
    delete this->nuovaAssociazione;
    //libera il puntatore membro di classe
    this->nuovaAssociazione = NULL;
}

void Seggio::eliminaNuovaAssociazione(){
    delete this->nuovaAssociazione;
    this->nuovaAssociazione = NULL;
}

Associazione* Seggio::getNuovaAssociazione(){
    return this->nuovaAssociazione;
}

unsigned int Seggio::getNumberAssCorrenti(){
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

    unsigned int index = 0;
    unsigned int currentPV = 0;
    unsigned int idHT = 0;
    for (unsigned int i = 0; this->listAssociazioni.size();i++){
        currentPV = this->listAssociazioni[i].getIdPV();

        //trova l'associazione che corrisponde alla postazione di voto per cui rimuovere l'associazione
        if(currentPV==idPV){
            //salva l'id dell'hardware token da rendere libero
            idHT = this->listAssociazioni[i].getIdHT();
            //salva l'indice corrispondente per la successiva eliminazione
            index = i;
            break; //interrompe l'esecuzione del for in cui è immediatemente contenuto
            // quindi in currentPV resta l'id della postazione per cui rimuovere l'associazione
        }
    }

    //TODO comunica alla postazione voto la rimozione dell'associazione
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


        //elimino dall'heap vector l'istanza dell'associazione rimossa
        //l'eliminazione dal vettore di un elemento libera anche la memoria
        //perchè il vettore listAssociazioni è un vettore di oggetti Associazione e non un vettore di puntatori di tipo Associazione
        this->listAssociazioni.erase(this->listAssociazioni.begin()+index);
    }
    else{
        cout << "Seggio: Unable to remove association from PV" << endl;
    }
}

//void Seggio::liberaHT_PV(unsigned int idPV){
//    //cliccando sulla postazione in cui il voto è completato, rimuove l'associazione relativa dalla listaAssociazioni
//    //TODO libera le postazioni di voto alla conclusione di una operazione di voto, chiamata dal click sul bottone relativo alla postazione di voto, quando è verde
//}

//void Seggio::readVoteResults(unsigned int idProceduraVoto){
//    //servizio da richiedere all'urna virtuale
//    //TODO contattare l'urna per ottenere i risultati di voto
//}

unsigned int Seggio::stateInfoPV(unsigned int idPV){
    //restituisce lo stato della postazione di voto con l'id indicato
    return this->statoPV[idPV-1];
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
        if(( statoPV == Seggio::statiPV::attesa_abilitazione) || ( statoPV == Seggio::statiPV::errore)){
            cout << "trovata almeno una associazione eliminabile" << endl;
            return true;
        }
        cout << "Seggio: Non eliminabile" << endl;
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
    cerr << "Seggio: isBusyHT: il for ha fallito" << endl;
    return true;
}

void Seggio::setPVstate(unsigned int idPV, unsigned int nuovoStatoPV){

    if(idPV > 0 && idPV <= NUM_PV){
        this->mutex_stati.lock();
        this->statoPV[idPV-1] = nuovoStatoPV;
        this->mutex_stati.unlock();
    }
    else{
        cerr << "id Postazione di Voto non valido" << endl;
        exit(EXIT_FAILURE);
    }
    switch(nuovoStatoPV) {

    case statiPV::libera :
        this->busyPV[idPV-1]=false;
        break;
    }
    //richiama la funzione della view che si occupa di aggiornare la grafica delle postazioni
    //e la conseguente abilitazione o disabilitazione di bottoni(ovvero attivazione o
    //disattivazione di funzionalità relazionate allo stato delle postazioni di voto
    mainWindow->updatePVbuttons();
}

const char *Seggio::getIP_PV(unsigned int idPV){
    return this->IP_PV[idPV-1];
}

void Seggio::runServerUpdatePV(){
    //funzione eseguita da un thread
    this->seggio_server = new SSLServer(this);
    this->mutex_stdout.lock();
    cout << "Seggio: avviato il ServerSeggio per ricevere gli update dello stato delle Postazioni di Voto" << endl;
    this->mutex_stdout.unlock();

    while(!(this->stopThreads)){//se stopThreads ha valore vero il while termina

        //attesa di una richiesta
        this->seggio_server->ascoltoNuovoStatoPV();

        //prosegue rimettendosi in ascolto al ciclo successivo, se stopThreads è falso
    }

    this->mutex_stdout.lock();
    cout << "Seggio: runServerUpdatePV: exit!" << endl;
    this->mutex_stdout.unlock();


    //delete this->seggio_server;
    // il thread che eseguiva la funzione termina se la funzione arriva alla fine
    delete this->seggio_server;
    return;
}


void Seggio::stopServerUpdatePV(){

    //creo client per contattare il server
    //this->seggio_client = new SSLClient(this);
    this->stopThreads = true;

    //predispongo il server per l'interruzione
    this->seggio_server->setStopServer(true);

    this->mutex_stdout.lock();
    cout << "Seggio: il server sta per essere fermato" << endl;
    this->mutex_stdout.unlock();

    //mi connetto al server locale per sbloccare l'ascolto e portare alla terminazione della funzione eseguita dal thread che funge da serve in ascolto
    const char * localhost = "127.0.0.1";
    this->seggio_client->stopLocalServer(localhost);




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

bool Seggio::pushAssociationToPV(unsigned int idPV, unsigned int idHT){
    const char* ip_pv = this->IP_PV[idPV-1];

    bool res = false;
    if(seggio_client->connectTo(ip_pv)!= nullptr){
        //richiede il settaggio della associazione alla postazione di voto a cui il client del seggio si è connesso
        res = seggio_client->querySetAssociation(idHT);
    }

    return res;
}

bool Seggio::removeAssociationFromPV(unsigned int idPV){
    const char* ip_pv = this->IP_PV[idPV-1];
    bool res = false;
    if(seggio_client->connectTo(ip_pv)!= nullptr){

        //richiede la rimozione della associazione alla postazione di voto a cui il client del seggio si è connesso
        res = seggio_client->queryRemoveAssociation();
    }
    return res;
}

void Seggio::pullStatePV(unsigned int idPV){

}
