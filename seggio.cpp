#include "seggio.h"

#include <iostream>
#include <thread>
#include <unistd.h>
#include "sslclient.h"
#include "sslserver.h"
#include <mutex>
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

    idHTAttivi[0]=1;
    idHTAttivi[1]=2;
    idHTAttivi[2]=3;
    idHTAttivi[3]=4;
    idHTRiserva = 5;

    idPostazioniVoto[0]=1;
    idPostazioniVoto[1]=2;
    idPostazioniVoto[2]=3;

    nuovaAssociazione = NULL;

    this->IP_PV[0] = "localhost";
    this->IP_PV[1] = "localhost";
    this->IP_PV[2] = "localhost";

    //le postazioni devono essere attivate, quindi in fase di inizializzazione non sono disponibili per essere associate ad un hardware token
    this->statoPV[0] = statiPV::attesa_attivazione;
    this->statoPV[1] = statiPV::attesa_attivazione;
    this->statoPV[2] = statiPV::attesa_attivazione;
    busyPV={true, true,true};

    busyHT={false,false, false,false};


    this->stopThreads = false;
    thread_server = std::thread(&Seggio::runServerUpdatePV, this);


    patternSS[statiPV::attesa_abilitazione] = "width:180; height:120;border: 7px solid red;background-color:white;color:black;font-size:18px;";
    patternSS[statiPV::attesa_attivazione] = "width:180; height:120;border: 7px solid red;background-color:white;color:black;font-size:18px;";
    patternSS[statiPV::libera] = "width:180; height:120;border: 7px solid rgb(85,255,0);background-color:white;color:black;font-size:18px;";
    patternSS[statiPV::votazione_completata] = " width:180; height:120; border:7px solid red; background-color:rgb(85,255,0);color:black;font-size:18px;";
    patternSS[statiPV::votazione_in_corso] = "width:180; height:120;border: 7px solid red;background-color:white;color:black;font-size:18px;";
    patternSS[statiPV::errore] = "width:180; height:120;border: 7px solid red;background-color:white;color:black;font-size:18px;";
}

Seggio::~Seggio(){

    //thread_server.join();
    //thread_1.join();
    //thread_2.join();
    //thread_3.join();

}

void Seggio::setBusyHT_PV(){
    //segna come occupate le postazioni corrente, e memorizza la postazione di ultimo utilizzo per l'HT
    unsigned int idPV=this->nuovaAssociazione->getIdPV();
    unsigned int idHT=this->nuovaAssociazione->getIdHT();

    for(unsigned int index = 0; index < this->busyHT.size(); index++){
        if(this->idHTAttivi[index] == idHT){ //se l'id dell'HTAttivo corrente corrisponde con l'id dell'HT da impegnare
            this->busyHT[index] = true; //lo settiamo ad occupato nell'indice degli HT occupati
        }
    }
    this->PV_lastUsedHT[idPV-1]=idHT;
    //lock_guard<std::mutex> guard(this->mutex_stdout);
    cout << "Hardware token: " << idHT << ", ultimo utilizzo su PV: " << idPV << endl;
    this->busyPV[idPV-1]=true;

    if(!this->anyPostazioneLibera()){
        mainWindow->updateCreaAssociazioneButton();
    }

}

bool Seggio::createAssociazioneHT_PV(){
    //crea un associazione in base alle risorse disponibili
    //lo stesso HT non viene assegnato due volte consecutive alla stessa PV
    unsigned int idPV;
    unsigned int idHT;
    bool associationFound = false;
    //cout << "e cazzo... size(busyPV): " << busyPV.size() <<endl;
    for(unsigned int indexPV = 0; indexPV < this->busyPV.size();indexPV++){
        if (associationFound==true){
            break;
        }
        else {
            bool pv_occupata = this->busyPV[indexPV];
            this->mutex_stati.lock();
            unsigned int pv_stato = this->stateInfoPV(indexPV+1);
            this->mutex_stati.unlock();
            cout << "PV " << indexPV+1 << " candidata alla associazione trovata" << endl;

            if((!pv_occupata) && (pv_stato!=statiPV::attesa_attivazione) && (pv_stato==statiPV::libera)){ // se la postazione non è occupata
                //sceglie una postazione voto libera e attivata (disponibile per una associazione)

                idPV=indexPV+1; //postazione selezionata
                for (unsigned int indexHT = 0; indexHT < this->busyHT.size();indexHT++){
                    if (this->busyHT[indexHT]==false){ // se l'HT non è impegnato
                        if(this->PV_lastUsedHT[indexPV]!=this->idHTAttivi[indexHT]){ //e l'HT non è stato utilizzato l'ultima volta su questa postazione, si può selezionare
                            idHT=this->idHTAttivi[indexHT]; //seleziona l'i-esimo HT tra quelli attivi
                            associationFound = true; //associazione possibile trovata
                            this->nuovaAssociazione = new Associazione(idPV,idHT);
                            break;
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
    this->listAssociazioni.push_back(*this->nuovaAssociazione);
    setBusyHT_PV();

    //libera il puntatore membro di classe
    this->nuovaAssociazione = NULL;
}

void Seggio::eliminaNuovaAssociazione(){
    this->nuovaAssociazione = NULL;
}

Associazione* Seggio::getNuovaAssociazione(){
    return this->nuovaAssociazione;
}

unsigned int Seggio::getNumberAssCorrenti(){
    unsigned int elementiHeap = this->listAssociazioni.size();
    return elementiHeap;
}

void Seggio::setNumeroSeggio(int number){
    this->numeroSeggio=number;
}

vector< Associazione > Seggio::getListAssociazioni(){
    return this->listAssociazioni;
}

void Seggio::removeAssociazioneHT_PV(unsigned int idPV){
    //elimina un'associazione per la postazione di voto indicata

    unsigned int index = 0;
    unsigned int currentPV;
    unsigned int idHT = 0;
    for (unsigned int i = 0; this->listAssociazioni.size();i++){
        currentPV = this->listAssociazioni[i].getIdPV();
        //lock_guard<std::mutex> guard(this->seggioMTX);
        //cout << currentPV << endl;
        if(currentPV==idPV){
            idHT = this->listAssociazioni[i].getIdHT();
            index = i;
            break; //interrompe l'esecuzione del for in cui è immediatemente contenuto
            // quindi in currentPV resta l'id della postazione per cui rimuovere l'associazione
        }
    }
    this->listAssociazioni.erase(this->listAssociazioni.begin()+index);
    this->busyPV[currentPV-1]=false;
    //lock_guard<std::mutex> guard(this->seggioMTX);
    cout << "postazione: " << currentPV <<" -> liberata" << endl;

    for(unsigned int index = 0; index < this->busyHT.size(); index++){
        if(this->idHTAttivi[index] == idHT){ //se l'idHTAttivo corrente corrisponde con l'id dell'HT da liberare
            this->busyHT[index] = false; //lo settiamo a libero nell'indice degli HT occupati
            //lock_guard<std::mutex> guard(this->seggioMTX);
            cout << "token: " << idHT <<" -> liberato" << endl;
            return;
        }
    }
    cerr << "Impossibile liberare il token!" << endl;
}

void Seggio::liberaHT_PV(unsigned int idPV){
    //cliccando sulla postazione in cui il voto è completato, rimuove l'associazione relativa dalla listaAssociazioni
    //TODO libera le postazioni di voto alla conclusione di una operazione di voto, chiamata dal click sul bottone relativo alla postazione di voto, quando è verde
}

void Seggio::readVoteResults(unsigned int idProceduraVoto){
    //servizio da richiedere all'urna virtuale
    //TODO contattare l'urna per ottenere i risultati di voto
}

unsigned int Seggio::stateInfoPV(unsigned int idPV){
    //restituisce lo stato della postazione di voto con l'id indicato
    //----penso non occorra il mutex, perché i valori vengono solo letti e non modificati
    return this->statoPV[idPV-1];
}

bool Seggio::feedbackFreeBusy(unsigned int idPV){
    idPV = 1;
    return true;
}

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
        if(( statoPV == Seggio::attesa_abilitazione) || ( statoPV == Seggio::errore)){
            return true;
        }
    }
    return false;
}

array<unsigned int, 4> &Seggio::getArrayIdHTAttivi(){
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
            lock_guard<std::mutex> guard(this->mutex_stdout);
            cout << "token: " << tokenAttivo << " -> disabilitato" <<endl;

            cout << "token: " << temp << " -> abilitato" <<endl;
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
    //return false;
}

void Seggio::setPVstate(unsigned int idPV, unsigned int nuovoStatoPV){
    this->mutex_stati.lock();
    this->statoPV[idPV-1] = nuovoStatoPV;
    this->mutex_stati.unlock();
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
    cout << "avvio del seggio_server per ricevere gli update dello stato delle Postazioni di Voto" << endl;
    this->mutex_stdout.unlock();

    while(!this->stopThreads){//se stopThreads ha valore vero il while termina

        //attesa di una richiesta
        this->seggio_server->ascoltoNuovoStatoPV();

        //prosegue rimettendosi in ascolto al ciclo successivo
    }

    this->mutex_stdout.lock();
    cout << "runServerUpdatePV: exit!" << endl;
    this->mutex_stdout.unlock();

    // il thread che eseguiva la funzione termina se la funzione arriva alla fine
    return;

}

void Seggio::stopServerUpdatePV(){
    //creo client per contattare il server
    this->seggio_client = new SSLClient(this);

    //predispongo il server per l'interruzione
    this->seggio_server->setStopServer(true);

    this->mutex_stdout.lock();
    cout << "il server sta per essere fermato" << endl;
    this->mutex_stdout.unlock();

    //mi connetto al server locale per sbloccare l'ascolto e indurre la terminazione della funzione eseguita dal thread che funge da serve in ascolto
    this->seggio_client->stopServer("localhost");


}
