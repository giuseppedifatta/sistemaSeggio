#include "mainwindowseggio.h"
#include "ui_mainwindowseggio.h"
#include "associazione.h"

#include <iostream>
#include <algorithm>
#include <vector>
using namespace std;

MainWindowSeggio::MainWindowSeggio(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindowSeggio)
{

    this->logged = false;

    ui->setupUi(this);
    ui->stackedWidget->setCurrentIndex(loginSeggio);

    //impostazione interfaccia inserimento password
    ui->wrongPassword_label->hide();



    //setWindowFlags(  Qt::WindowMinMaxButtonsHint);

    //initTableRV();

    setWindowFlags(Qt::FramelessWindowHint);

    setWindowTitle("Voto digitale UNIPA");



    //ui->password_lineEdit->setEchoMode(QLineEdit::Password);


    //inizializzazione del model
    seggio = new Seggio(this);
    emit seggioLogged(false);

    //inserire qui le connect tra model e view

    qRegisterMetaType <string> ("string");
    qRegisterMetaType <std::vector <string>>("<std::vector <string>");
    qRegisterMetaType< std::vector<Associazione>>( "std::vector<Associazione>" );

    QObject::connect(seggio,SIGNAL(anyPVFree(bool)),this,SLOT(updateCreaAssociazioneButton(bool)),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(anyAssociationRemovable(bool)),this,SLOT(updateRimuoviAssociazioneButton(bool)),Qt::QueuedConnection);
    QObject::connect(this,SIGNAL(associationToRemove(uint)),seggio,SLOT(removeAssociazioneHT_PV(uint)),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(stateChanged(uint,uint)),this,SLOT(updatePVButtons(uint,uint)),Qt::QueuedConnection);//parametri: idPV, statoPV
    QObject::connect(this,SIGNAL(needNewAssociation()),seggio,SLOT(createAssociazioneHT_PV()),Qt::QueuedConnection);

    QObject::connect(this,SIGNAL(seggioLogged(bool)),seggio,SLOT(setLogged(bool)),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(associationReady(string,uint)),this,SLOT(showNewAssociation(string,uint)),Qt::QueuedConnection);//parametri:idHT, idPV

    QObject::connect(this,SIGNAL(abortAssociation()),seggio,SLOT(eliminaNuovaAssociazione()),Qt::QueuedConnection);
    QObject::connect(this,SIGNAL(checkPassKey(QString)),seggio,SLOT(validatePassKey(QString)),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(validPass()),this,SLOT(initSeggio()),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(wrongPass()),this,SLOT(showErrorPass()),Qt::QueuedConnection);
    QObject::connect(this,SIGNAL(logoutRequest()),seggio,SLOT(tryLogout()),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(forbidLogout()),this,SLOT(showErrorLogout()),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(grantLogout()),this,SLOT(doLogout()),Qt::QueuedConnection);
    QObject::connect(this, SIGNAL(needRemovableAssociations()),seggio,SLOT(calculateRemovableAssociations()),Qt::QueuedConnection);
    QObject::connect(this, SIGNAL(confirmVotazioneCompleta(uint)),seggio,SLOT(completaOperazioneVoto(uint)),Qt::QueuedConnection);


    QObject::connect(seggio,SIGNAL(removableAssociationsReady(std::vector<Associazione>)),this,SLOT(showRemovableAssociations(std::vector<Associazione>)),Qt::QueuedConnection);

    QObject::connect(seggio,SIGNAL(sessionEnded()),SLOT(showMessageSessioneEnded()),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(sessionNotYetStarted()),SLOT(showMessageSessionNotStarted()),Qt::QueuedConnection);
    QObject::connect(this, SIGNAL(wantVote(uint)),seggio,SLOT(tryVote(uint)),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(allowVote()),this,SLOT(hideCreaAssociazione()),Qt::QueuedConnection);


    QObject::connect(seggio,SIGNAL(forbidVote(string)),this,SLOT(showMessageForbidVote(string)),Qt::QueuedConnection);

    QObject::connect(this,SIGNAL(needMatricolaInfo(uint)),seggio,SLOT(matricolaState(uint)),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(matricolaStateReceived(QString)),SLOT(showInfoMatricola(QString)),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(errorAbortVoting(uint)),this,SLOT(showErrorAbortVoting(uint)),Qt::QueuedConnection);
    QObject::connect(this,SIGNAL(tryRemoveStateMatricola(uint)),seggio,SLOT(abortVoting(uint)),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(successAbortVoting()),this,SLOT(showMessageAssociationRemoved()),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(urnaNonRaggiungibile()),this,SLOT(showErrorUrnaUnreachable()),Qt::QueuedConnection);

    //gestione hardware token


    QObject::connect(this,SIGNAL(needStatoGeneratori()),seggio,SLOT(calcolaHTdisattivabili()),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(readyHTDisattivabili(std::vector<string>,string)),this,SLOT(showManageToken(std::vector<string>,string)));
    QObject::connect(this,SIGNAL(disattivaHT(string)),seggio,SLOT(disattivaHT(string)),Qt::QueuedConnection);
    QObject::connect(seggio,SIGNAL(scambiati(string,string)),this,SLOT(showTokenScambiati(string,string)),Qt::QueuedConnection);
}
MainWindowSeggio::~MainWindowSeggio()
{
    cout << "View: distruttore della GUI" << endl;
    if(seggio->isRunning()){
        //se non è stato fatto l'accesso, il thread seggio non sarà stato avviato e non bisogna attendere che termini la sua esecuzione

        cout << "View: attendo che il thread del seggio termini la sua esecuzione" << endl;
        seggio->wait();
    }

    delete seggio;
    delete ui;
}
void MainWindowSeggio::sessioneDiVotoTerminata(){
    ui->stackedWidget->setCurrentIndex(sessioneConclusa);
}

void MainWindowSeggio::initGestioneSeggio(){
    ui->pushButton_infoMatricola->setEnabled(false);

    //sezione nuove associazioni
    hideCreaAssociazione();

    //sezione rimozione associazioni
    //ui->rimuoviAssociazione_button->setEnabled(false);
    ui->associazioniRimovibili_label->hide();
    ui->associazioneRimovibili_comboBox->hide();
    ui->rimuovi_button->hide();
    ui->annullaRimozione_button->hide();

    //sezione feedback postazioni di voto
    ui->pv1_button->setEnabled(false);
    ui->pv2_button->setEnabled(false);
    ui->pv3_button->setEnabled(false);
    ui->liberaPValert1_label->setText("");
    ui->liberaPValert2_label->setText("");
    ui->liberaPValert3_label->setText("");

    ui->associazioneRimovibili_comboBox->clear();
}

void MainWindowSeggio::on_loginCS_button_clicked()
{
    ui->password_lineEdit->setFocus();
    ui->stackedWidget->setCurrentIndex(loginPassword);

}

void MainWindowSeggio::on_quitSystem_button_clicked()
{
    //this->~MainWindowSeggio();
    QApplication::quit();
}

void MainWindowSeggio::on_backToLogin_button_clicked()
{
    ui->stackedWidget->setCurrentIndex(loginSeggio);
    ui->wrongPassword_label->hide();
    ui->password_lineEdit->setText("");
}

void MainWindowSeggio::on_accediGestioneSeggio_button_clicked(){
    ui->wrongPassword_label->hide();
    //int numeroSeggio = ui->numeroSeggio_comboBox->currentText().toInt();
    QString pass;
    pass = ui->password_lineEdit->text();

    emit checkPassKey(pass);


}

void MainWindowSeggio::initSeggio(){

    this->logged = true;
    emit seggioLogged(true);

    //inizializzazione interfaccia gestioneSeggio
    this->initGestioneSeggio();

    ui->stackedWidget->setCurrentIndex(gestioneSeggio);


    ui->wrongPassword_label->hide();
    ui->password_lineEdit->setText("");

    seggio->mutex_stdout.lock();
    cout << "View: loggato" << endl;
    seggio->mutex_stdout.unlock();

    //avvio del thread del model
    seggio->start();

    //il segnale segnala la necessità di aggiornamento dello stato delle postazioni di voto
    //emit needStatePVs();


}

void MainWindowSeggio::showErrorPass(){
    ui->wrongPassword_label->show();
}


void MainWindowSeggio::updatePVButtons(unsigned int idPVtoUpdate, unsigned int statoPV){
    //Questa funzione si occupa di aggiornare la grafica delle postazioni
    //e la conseguente abilitazione o disabilitazione di bottoni, ovvero attivazione o
    //disattivazione di funzionalità relazionate allo stato delle postazioni di voto

    QString messaggioPV;

    cout << "View: Aggiorno lo stato della postazione " << idPVtoUpdate << endl;

    switch(statoPV){

    case seggio->statiPV::attesa_attivazione :
        messaggioPV = "da attivare";
        break;
    case seggio->statiPV::libera  :
        messaggioPV = "libera";
        break;
    case seggio->statiPV::attesa_abilitazione  :
        messaggioPV = "attesa abilitazione";
        break;
    case seggio->statiPV::votazione_in_corso  :
        messaggioPV = "votazione in corso";
        break;
    case seggio->statiPV::votazione_completata  :
        messaggioPV = "votazione completata";
        break;
    case seggio->statiPV::errore  :
        messaggioPV = "errore";
        break;
    case seggio->statiPV::offline  :
        messaggioPV = "offline";
        break;
    case seggio->statiPV::non_raggiungibile :
        messaggioPV = "non raggiungibile";
        break;

    }

    seggio->mutex_stdout.lock();
    cout << "View: Aggiorno bottoni" << endl;
    seggio->mutex_stdout.unlock();


    switch(idPVtoUpdate){
    case 1:{
        //aggiornamento bottone PV1
        ui->pv1_button->setText(messaggioPV);
        if(statoPV == seggio->statiPV::libera){
            cout << "View: PV1: set green border" << endl;
            ui->pv1_button->setProperty("free",true);
            ui->pv1_button->setProperty("freeable", false);
            ui->pv1_button->style()->unpolish(ui->pv1_button);
            ui->pv1_button->style()->polish(ui->pv1_button);
            ui->pv1_button->update();

            ui->creaAssociazioneHTPV_button->setEnabled(true);
        }
        else if (statoPV == seggio->statiPV::votazione_completata){
            cout << "View: PV1: set green background" << endl;
            ui->pv1_button->setProperty("freeable",true);
            ui->pv1_button->style()->unpolish(ui->pv1_button);
            ui->pv1_button->style()->polish(ui->pv1_button);
            ui->pv1_button->update();

            ui->pv1_button->setEnabled(true);
            ui->liberaPValert1_label->setText("clicca per liberare");
        }
        else{
            ui->pv1_button->setProperty("free",false);
            ui->pv1_button->style()->unpolish(ui->pv1_button);
            ui->pv1_button->style()->polish(ui->pv1_button);
            ui->pv1_button->update();
        }
        break;
    }
    case 2:{
        //aggiornamento bottone PV2
        ui->pv2_button->setText(messaggioPV);
        if(statoPV == seggio->statiPV::libera){
            cout << "View: PV2: set green border" << endl;
            ui->pv2_button->setProperty("free",true);
            ui->pv2_button->style()->unpolish(ui->pv2_button);
            ui->pv2_button->style()->polish(ui->pv2_button);
            ui->pv2_button->update();

            ui->creaAssociazioneHTPV_button->setEnabled(true);
        }
        else if (statoPV == seggio->statiPV::votazione_completata){
            cout << "View: PV2: set green background" << endl;
            ui->pv2_button->setProperty("freeable",true);
            ui->pv2_button->style()->unpolish(ui->pv2_button);
            ui->pv2_button->style()->polish(ui->pv2_button);
            ui->pv2_button->update();

            ui->pv2_button->setEnabled(true);
            ui->liberaPValert2_label->setText("clicca per liberare");
        }
        else{
            ui->pv2_button->setProperty("free",false);
            ui->pv2_button->style()->unpolish(ui->pv2_button);
            ui->pv2_button->style()->polish(ui->pv2_button);
            ui->pv2_button->update();
        }
        break;
    }
    case 3:{
        //aggiornamento bottone PV3
        ui->pv3_button->setText(messaggioPV);
        if(statoPV == seggio->statiPV::libera){
            cout << "View: PV3: set green border" << endl;
            ui->pv3_button->setProperty("free",true);
            ui->pv3_button->style()->unpolish(ui->pv3_button);
            ui->pv3_button->style()->polish(ui->pv3_button);
            ui->pv3_button->update();

            ui->creaAssociazioneHTPV_button->setEnabled(true);
        }
        else if (statoPV == seggio->statiPV::votazione_completata){
            cout << "View: PV3: set green background" << endl;
            ui->pv3_button->setProperty("freeable",true);
            ui->pv3_button->style()->unpolish(ui->pv3_button);
            ui->pv3_button->style()->polish(ui->pv3_button);
            ui->pv3_button->update();

            ui->pv3_button->setEnabled(true);
            ui->liberaPValert3_label->setText("clicca per liberare");
        }
        else{
            ui->pv3_button->setProperty("free",false);
            ui->pv3_button->style()->unpolish(ui->pv3_button);
            ui->pv3_button->style()->polish(ui->pv3_button);
            ui->pv3_button->update();
        }
        break;
    }
    }

    seggio->mutex_stdout.lock();
    cout << "View: bottoni aggiornati "  << endl;
    seggio->mutex_stdout.unlock();
}



void MainWindowSeggio::updateRimuoviAssociazioneButton(bool b){
    ui->rimuoviAssociazione_button->setEnabled(b);
}

void MainWindowSeggio::updateCreaAssociazioneButton(bool b){
    ui->creaAssociazioneHTPV_button->setEnabled(b);
}

void MainWindowSeggio::on_gestisci_HT_button_clicked()
{
    emit needStatoGeneratori();
    ui->stackedWidget->setCurrentIndex(gestioneHT);
}

void MainWindowSeggio::showManageToken(std::vector <string> snHTdisattivabili,string snHTdisattivo){
    ui->token_disattivabili_comboBox->clear();
    for(uint i =  0; i < snHTdisattivabili.size();i++){
        QString item = QString::fromStdString(snHTdisattivabili.at(i));
        ui->token_disattivabili_comboBox->addItem(item);
    }
    ui->label_ht_da_attivare->setText(QString::fromStdString(snHTdisattivo));
}

void MainWindowSeggio::showTokenScambiati(string disattivato, string attivato)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Operazione completata");
    msgBox.setInformativeText("Token " + QString::fromStdString(disattivato) + " disattivato; token " +
                              QString::fromStdString(attivato) + " attivato.");
    msgBox.exec();

    ui->stackedWidget->setCurrentIndex(InterfacceSeggio::gestioneSeggio);
}

void MainWindowSeggio::on_logout_button_clicked(){
    emit logoutRequest();
}


void MainWindowSeggio::on_logoutCS_button_clicked()
{
    emit logoutRequest();
}

void MainWindowSeggio::on_logout2_button_clicked()
{
    emit logoutRequest();
}

void MainWindowSeggio::doLogout(){
    this->logged = false;
    emit seggioLogged(false);
    ui->stackedWidget->setCurrentIndex(loginSeggio);

}

void MainWindowSeggio::showErrorLogout(){
    // implementare messaggio popup a schermo che avvisa di aspettare la fine delle operazioni di voto in corso
    cout << "View: logout non consentito, delle operazioni di voto sono in corso" << endl;
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Error");
    msgBox.setInformativeText("Impossibile effettuare il logout, attendere la conclusione delle operazioni di voto in corso");
    msgBox.exec();
}

void MainWindowSeggio::on_annullaAssociazione_button_clicked()
{
    ui->creaAssociazioneHTPV_button->setEnabled(true);

    hideCreaAssociazione();
    emit abortAssociation();
}

void MainWindowSeggio::on_creaAssociazioneHTPV_button_clicked()
{
    ui->creaAssociazioneHTPV_button->setEnabled(false);
    ui->gestisci_HT_button->setEnabled(false);

    //emettere un segnale
    emit needNewAssociation();

}

void MainWindowSeggio::showNewAssociation(string ht, unsigned int idPV){
    QString s = "HT %1 - PV %2";
    ui->confermaAssociazione_button->setText(s.arg(QString::fromStdString(ht)).arg(idPV));

    ui->annullaAssociazione_button->show();
    ui->associazioneDisponibile_label->show();
    ui->confermaAssociazione_button->show();
    //ui->lineEdit_matricolaElettore->show();
    ui->pushButton_letVote->show();
}

void MainWindowSeggio::on_confermaAssociazione_button_clicked()
{
    on_pushButton_letVote_clicked();

    //    //completare associazione HT-PV
    //    emit confirmAssociation();


    //    ui->gestisci_HT_button->setEnabled(true);

    //    ui->annullaAssociazione_button->hide();
    //    ui->associazioneDisponibile_label->hide();
    //    ui->confermaAssociazione_button->hide();

}

void MainWindowSeggio::hideCreaAssociazione(){
    ui->gestisci_HT_button->setEnabled(true);


    ui->annullaAssociazione_button->hide();
    ui->associazioneDisponibile_label->hide();
    ui->confermaAssociazione_button->hide();
    //ui->lineEdit_matricolaElettore->hide();
    ui->pushButton_letVote->hide();
}

void MainWindowSeggio::showMessageAssociationRemoved()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Success");
    msgBox.setInformativeText("Rimozione associazione completata con successo.");
    msgBox.exec();
}

void MainWindowSeggio::on_rimuoviAssociazione_button_clicked()
{
    ui->rimuoviAssociazione_button->setEnabled(false);

    emit needRemovableAssociations();
}

void MainWindowSeggio::showRemovableAssociations(vector<Associazione> associazioniRimovibili){
    //Questa funzione riempie il combo box relativo alle associazioni eliminabili
    ui->associazioneRimovibili_comboBox->clear();

    for(unsigned i=0; i< associazioniRimovibili.size(); ++i){
        unsigned int idPV = associazioniRimovibili.at(i).getIdPV();
        string snHT = associazioniRimovibili.at(i).getSnHT();

        //creiamo l'item per l'associazione che risulta eliminabile
        QString str = "HT ";
        QString s = QString::fromStdString(snHT);
        str.append(s);
        str.append(" - PV ");
        s.setNum(idPV);
        str.append(s);

        ui->associazioneRimovibili_comboBox->addItem(str);

    }

    //contenuto della funzionalità pronto, rendiamolo visibile all'utente
    ui->associazioniRimovibili_label->show();
    ui->associazioneRimovibili_comboBox->show();
    ui->annullaRimozione_button->show();
    ui->rimuovi_button->show();
}

void MainWindowSeggio::showMessageSessioneEnded()
{
    QMessageBox msgBox(this);
    msgBox.setInformativeText("La sessione di voto si è conclusa, non è possibile creare nuove associazioni. Se non ci sono operazioni di voto in corso si prega di effettuare il Logout");
    msgBox.exec();
}

void MainWindowSeggio::showMessageSessionNotStarted()
{
    QMessageBox msgBox(this);
    msgBox.setInformativeText("La sessione di voto non è ancora iniziata, non è possibile creare nuove associazioni. Si prega di attendere...");
    msgBox.exec();
    ui->gestisci_HT_button->setEnabled(true);
    ui->creaAssociazioneHTPV_button->setEnabled(true);
}

void MainWindowSeggio::showMessageForbidVote(string esitoLock)
{
    QString matricola = ui->lineEdit_matricolaElettore->text();
    QMessageBox msgBox(this);
    msgBox.setInformativeText("Non è possibile accordare il voto alla matricola " +
                              matricola + ", motivo: " + QString::fromStdString(esitoLock));
    msgBox.exec();
}

void MainWindowSeggio::showInfoMatricola(QString info)
{
    QString matricola = ui->lineEdit_matricolaElettore->text();
    QMessageBox msgBox(this);
    msgBox.setInformativeText("Matricola " + matricola + ": " + info);
    msgBox.exec();
}

void MainWindowSeggio::showErrorUrnaUnreachable()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Error");
    msgBox.setInformativeText("Impossibile comunicare con l'urna, verificare la connessione");
    msgBox.exec();
}

void MainWindowSeggio::showErrorAbortVoting(uint matricola)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Error");
    msgBox.setInformativeText("Non è stato possibile comunicare con l'urna e completare la rimozione dell'associazione. Verificare la connessione.");
    msgBox.exec();
    emit tryRemoveStateMatricola(matricola);
}

void MainWindowSeggio::on_rimuovi_button_clicked()
{
    QString selectedAss = ui->associazioneRimovibili_comboBox->currentText();
    QString pv;

    //alla posizione di indice 10 è presente l'id della PV da liberare
    //mettiamo il carattere relativo all'id della PV da liberare in una variabile QString

    //perchè push_back? verificare se la versione commentata sotto funziona
    pv.push_back(selectedAss[10]);
    //pv = selectedAss[10];
    unsigned int pvToFree = pv.toInt();
    seggio->mutex_stdout.lock();
    cout << "View: postazione da liberare: " << pvToFree << endl;
    seggio->mutex_stdout.unlock();


    emit associationToRemove(pvToFree);

    //ui->rimuoviAssociazione_button->setEnabled(true);

    ui->associazioniRimovibili_label->hide();
    ui->associazioneRimovibili_comboBox->hide();
    ui->rimuovi_button->hide();
    ui->annullaRimozione_button->hide();

}

void MainWindowSeggio::on_annullaRimozione_button_clicked()
{
    ui->rimuoviAssociazione_button->setEnabled(true);

    ui->associazioniRimovibili_label->hide();
    ui->associazioneRimovibili_comboBox->hide();
    ui->rimuovi_button->hide();
    ui->annullaRimozione_button->hide();

    //PATTERN MVC ERROR chiamata funzione del seggio
    if(!(seggio->anyAssociazioneEliminabile())){
        ui->rimuoviAssociazione_button->setEnabled(false);
    }
}

void MainWindowSeggio::on_letturaRisultatiCS_button_clicked()
{
    ui->stackedWidget->setCurrentIndex(risultatiVoto);
}

void MainWindowSeggio::on_goBackFromRisultatiVoto_button_clicked()
{
    ui->stackedWidget->setCurrentIndex(sessioneConclusa);
}

void MainWindowSeggio::on_schedaSuccessiva_button_clicked()
{
    //TODO verificare la presenza di altre schede e visualizzare i relativi risultati di scrutinio
    //se la scheda corrente è l'ultima visualizzare la prima scheda della lista

}

void MainWindowSeggio::on_sostituisci_button_clicked()
{
    QString strHTDaDisattivare = ui->token_disattivabili_comboBox->currentText();
    //attiva il token di riserva e disabilita il token selezionato
    string snHTDaDisattivare = strHTDaDisattivare.toStdString();

    emit disattivaHT(snHTDaDisattivare);

}

void MainWindowSeggio::on_goBackToGestioneSeggio_button_clicked()
{
    ui->stackedWidget->setCurrentIndex(gestioneSeggio);
}

void MainWindowSeggio::initTableRV(){
    ui->risultatiVoto_tableWidget->verticalHeader()->setVisible(false);
    ui->risultatiVoto_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->risultatiVoto_tableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    QStringList tableHeaders;
    tableHeaders << "Candidati" << "Numero voti ricevuti";
    ui->risultatiVoto_tableWidget->setColumnCount(2);
    ui->risultatiVoto_tableWidget->setHorizontalHeaderLabels(tableHeaders);

    ui->risultatiVoto_tableWidget->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    ui->risultatiVoto_tableWidget->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    QFont font = ui->risultatiVoto_tableWidget->horizontalHeader()->font();
    font.setPointSize(18);
    ui->risultatiVoto_tableWidget->horizontalHeader()->setFont( font );
    ui->risultatiVoto_tableWidget->horizontalHeader()->setStyleSheet(".QHeaderView{}");

    QFont fontItem("Sans Serif",18);
    ui->risultatiVoto_tableWidget->setFont(fontItem);
}


void MainWindowSeggio::on_pv1_button_clicked()
{
    //libera postazione 1
    emit confirmVotazioneCompleta(1);
    ui->liberaPValert1_label->setText("");

}

void MainWindowSeggio::on_pv2_button_clicked()
{
    //libera postazione 2
    emit confirmVotazioneCompleta(2);
    ui->liberaPValert2_label->hide();
}

void MainWindowSeggio::on_pv3_button_clicked()
{
    //libera postazione 3
    emit confirmVotazioneCompleta(3);
    ui->liberaPValert3_label->hide();
}

void MainWindowSeggio::on_pushButton_letVote_clicked()
{
    QString matricola = ui->lineEdit_matricolaElettore->text();
    QRegExp re("\\d*");  // a digit (\d), zero or more times (*)
    if (re.exactMatch(matricola)){
        emit wantVote(matricola.toUInt());
    }
    else {
        QMessageBox msgBox(this);
        msgBox.setInformativeText("Matricola inserita non valida. La matricola deve essere numerica");
        msgBox.exec();
    }
}

void MainWindowSeggio::on_pushButton_infoMatricola_clicked()
{
    QString matricola = ui->lineEdit_matricolaElettore->text();
    QRegExp re("\\d*");  // a digit (\d), zero or more times (*)
    if (re.exactMatch(matricola)){
        emit needMatricolaInfo(matricola.toUInt());
    }
    else {
        QMessageBox msgBox(this);
        msgBox.setInformativeText("Matricola inserita non valida. La matricola deve essere numerica");
        msgBox.exec();
    }

}

void MainWindowSeggio::on_lineEdit_matricolaElettore_textChanged(const QString &arg1)
{
    if(arg1 == ""){
        ui->pushButton_infoMatricola->setEnabled(false);
    }
    else{
        ui->pushButton_infoMatricola->setEnabled(true);

    }
}
