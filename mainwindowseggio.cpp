#include "mainwindowseggio.h"
#include "ui_mainwindowseggio.h"

#include "associazione.h"
#include <iostream>
#include <algorithm>
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
    ui->password_lineEdit->setEchoMode(QLineEdit::Password);


    //setWindowFlags(  Qt::WindowMinMaxButtonsHint);

    //initTableRV();

    setWindowFlags(Qt::FramelessWindowHint);

    //setWindowTitle("Voto digitale UNIPA");



    ui->password_lineEdit->setEchoMode(QLineEdit::Password);
    //    ui->pinInsertCS_lineEdit->setEchoMode(QLineEdit::Password);
    //    ui->pinInsertRP_lineEdit->setEchoMode(QLineEdit::Password);


    seggio = NULL;
}
MainWindowSeggio::~MainWindowSeggio()
{

    delete ui;
}
void MainWindowSeggio::sessioneDiVotoTerminata(){
    ui->stackedWidget->setCurrentIndex(sessioneConclusa);
}

void MainWindowSeggio::initGestioneSeggio(){
    //sezione nuove associazioni
    ui->annullaAssociazione_button->hide();
    ui->associazioneDisponibile_label->hide();
    ui->confermaAssociazione_button->hide();

    //sezione rimozione associazioni
    ui->rimuoviAssociazione_button->setEnabled(false);
    ui->associazioniRimovibili_label->hide();
    ui->associazioneRimovibili_comboBox->hide();
    ui->rimuovi_button->hide();
    ui->annullaRimozione_button->hide();

    //sezione feedback postazioni di voto
    ui->pv1_button->setEnabled(false);
    ui->pv2_button->setEnabled(false);
    ui->pv3_button->setEnabled(false);
    ui->liberaPValert1_label->hide();
    ui->liberaPValert2_label->hide();
    ui->liberaPValert3_label->hide();
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


    //TODO contattare il dbms degli accessi per controllare se la password è esatta

    if(pass=="qwerty"){
        this->logged = true;


        ui->stackedWidget->setCurrentIndex(gestioneSeggio);
        ui->password_lineEdit->setText("");

        //inizializzazione interfaccia gestioneSeggio
        initGestioneSeggio();

        this->seggio = new Seggio(this);

        seggio->mutex_stdout.lock();
        cout << "View: loggato" << endl;
        seggio->mutex_stdout.unlock();




        if(seggio->anyPostazioneLibera()){
            ui->creaAssociazioneHTPV_button->setEnabled(true);
        }
        else{
            ui->creaAssociazioneHTPV_button->setEnabled(false);

        }
        if(seggio->anyAssociazioneEliminabile()){
            ui->rimuoviAssociazione_button->setEnabled(true);
        }
        else{
            ui->rimuoviAssociazione_button->setEnabled(false);
        }
    }
    else{
        ui->wrongPassword_label->show();
    }


}

void MainWindowSeggio::updatePVbuttons(){
    //Questa funzione si occupa di aggiornare la grafica delle postazioni
    //e la conseguente abilitazione o disabilitazione di bottoni, ovvero attivazione o
    //disattivazione di funzionalità relazionate allo stato delle postazioni di voto

    std::array <int,3> statoPV;
    std::array <QString, 3> messaggioPV;




    //aggiornamento del testo
    for (int i = 0;i< 3; i++){

        seggio->mutex_stati.lock();
        statoPV[i] = seggio->stateInfoPV(i+1);
        seggio->mutex_stati.unlock();

        switch(statoPV[i]){
        case seggio->statiPV::attesa_attivazione :
            messaggioPV[i] = "attivare postazione";
            break;
        case seggio->statiPV::libera  :
            messaggioPV[i] = "postazione libera";
            break;
        case seggio->statiPV::attesa_abilitazione  :
            messaggioPV[i] = "attesa abilitazione";
            break;
        case seggio->statiPV::votazione_in_corso  :
            messaggioPV[i] = "votazione in corso";
            break;
        case seggio->statiPV::votazione_completata  :
            messaggioPV[i] = "votazione completata";
            break;
        case seggio->statiPV::errore  :
            messaggioPV[i] = "errore";
            break;

        }
    }

    seggio->mutex_stdout.lock();
    cout << "View: Aggiorno bottoni Postazioni Voto" << endl;
    seggio->mutex_stdout.unlock();

    //
    //        QString nameButton = "QPushButton#pv1_button";
    //        QString qstr = QString::fromStdString(seggio->patternSS[statoPV[0]]);
    //        QString newStyle = QString("%1 { %2 }").arg(nameButton).arg(qstr);

    //        seggio->mutex_stdout.lock();
    //        cout << "View: PV1, nuovo stile: " << newStyle.toUtf8().constData() << endl;
    //        seggio->mutex_stdout.unlock();
    //        //setStyleSheet(newStyle);
    //        ui->pv1_button->setStyleSheet(newStyle);
    //        if(statoPV[0]==seggio->statiPV::votazione_completata){
    //            ui->pv1_button->setEnabled(true);
    //        }

    //aggiornamento bottone PV1
    ui->pv1_button->setText(messaggioPV[0]);
    if(statoPV[0] == seggio->statiPV::libera){
        cout << "View: PV1: set green border" << endl;
        ui->pv1_button->setProperty("free",true);
        ui->pv1_button->style()->unpolish(ui->pv1_button);
        ui->pv1_button->style()->polish(ui->pv1_button);
        ui->pv1_button->update();

        ui->creaAssociazioneHTPV_button->setEnabled(true);
    }
    else if (statoPV[0] == seggio->statiPV::votazione_completata){
        cout << "View: PV1: set green background" << endl;
        ui->pv1_button->setProperty("freeable",true);
        ui->pv1_button->style()->unpolish(ui->pv1_button);
        ui->pv1_button->style()->polish(ui->pv1_button);
        ui->pv1_button->update();

        ui->pv1_button->setEnabled(true);
    }
    else{
        ui->pv1_button->setProperty("free",false);
        ui->pv1_button->style()->unpolish(ui->pv1_button);
        ui->pv1_button->style()->polish(ui->pv1_button);
        ui->pv1_button->update();
    }

    //aggiornamento bottone PV2
    ui->pv2_button->setText(messaggioPV[1]);
    if(statoPV[1] == seggio->statiPV::libera){
        cout << "View: PV2: set green border" << endl;
        ui->pv2_button->setProperty("free",true);
        ui->pv2_button->style()->unpolish(ui->pv2_button);
        ui->pv2_button->style()->polish(ui->pv2_button);
        ui->pv2_button->update();

        ui->creaAssociazioneHTPV_button->setEnabled(true);
    }
    else if (statoPV[1] == seggio->statiPV::votazione_completata){
        cout << "View: PV2: set green background" << endl;
        ui->pv2_button->setProperty("freeable",true);
        ui->pv2_button->style()->unpolish(ui->pv2_button);
        ui->pv2_button->style()->polish(ui->pv2_button);
        ui->pv2_button->update();

        ui->pv2_button->setEnabled(true);
    }
    else{
        ui->pv2_button->setProperty("free",false);
        ui->pv2_button->style()->unpolish(ui->pv2_button);
        ui->pv2_button->style()->polish(ui->pv2_button);
        ui->pv2_button->update();
    }

    //aggiornamento bottone PV3
    ui->pv3_button->setText(messaggioPV[2]);
    if(statoPV[2] == seggio->statiPV::libera){
        cout << "View: PV3: set green border" << endl;
        ui->pv3_button->setProperty("free",true);
        ui->pv3_button->style()->unpolish(ui->pv3_button);
        ui->pv3_button->style()->polish(ui->pv3_button);
        ui->pv3_button->update();

        ui->creaAssociazioneHTPV_button->setEnabled(true);
    }
    else if (statoPV[2] == seggio->statiPV::votazione_completata){
        cout << "View: PV3: set green background" << endl;
        ui->pv3_button->setProperty("freeable",true);
        ui->pv3_button->style()->unpolish(ui->pv3_button);
        ui->pv3_button->style()->polish(ui->pv3_button);
        ui->pv3_button->update();

        ui->pv3_button->setEnabled(true);
    }
    else{
        ui->pv3_button->setProperty("free",false);
        ui->pv3_button->style()->unpolish(ui->pv3_button);
        ui->pv3_button->style()->polish(ui->pv3_button);
        ui->pv3_button->update();
    }

    //    ui->pv2_button->setText(messaggioPV[1]);
    //    QString nameButton = "QPushButton#pv2_button";
    //    QString qstr = QString::fromStdString(seggio->patternSS[statoPV[1]]);
    //    QString newStyle = QString("%1 { %2 }").arg(nameButton).arg(qstr);
    //    seggio->mutex_stdout.lock();
    //    cout << "View: PV2, nuovo stile: " << newStyle.toUtf8().constData() << endl;
    //    seggio->mutex_stdout.unlock();
    //    ui->pv2_button->setStyleSheet(newStyle);
    //    if(statoPV[1]==seggio->statiPV::votazione_completata){
    //        ui->pv2_button->setEnabled(true);
    //    }
    //    else if(statoPV[1]==seggio->statiPV::libera){
    //        ui->creaAssociazioneHTPV_button->setEnabled(true);
    //    }

    //    ui->pv3_button->setText(messaggioPV[2]);
    //    nameButton = "QPushButton#pv3_button";
    //    qstr = QString::fromStdString(seggio->patternSS[statoPV[2]]);
    //    newStyle = QString("%1 { %2 }").arg(nameButton).arg(qstr);
    //    seggio->mutex_stdout.lock();
    //    cout << "View: PV3, nuovo stile: " << newStyle.toUtf8().constData() << endl;
    //    seggio->mutex_stdout.unlock();
    //    ui->pv3_button->setStyleSheet(newStyle);
    //    if(statoPV[2]==seggio->statiPV::votazione_completata){
    //        ui->pv3_button->setEnabled(true);
    //    }
    //    else if(statoPV[2]==seggio->statiPV::libera){
    //        ui->creaAssociazioneHTPV_button->setEnabled(true);
    //    }


    //aggiornamento bottoni crea_associazione e rimuovi associazione
    if(seggio->anyPostazioneLibera()){
        ui->creaAssociazioneHTPV_button->setEnabled(true);
    }
    if(seggio->anyAssociazioneEliminabile()){
        ui->rimuoviAssociazione_button->setEnabled(true);
    }
    seggio->mutex_stdout.lock();
    cout << "View: executed the function that updates states of PVs "  << endl;
    seggio->mutex_stdout.unlock();

}

void MainWindowSeggio::disableCreaAssociazioneButton(){
    ui->creaAssociazioneHTPV_button->setEnabled(false);
}

void MainWindowSeggio::on_gestisci_HT_button_clicked()
{
    ui->token_attivi_comboBox->clear();
    array <unsigned int, 4> idHtAttivi = seggio->getArrayIdHTAttivi();
    seggio->mutex_stdout.lock();
    cout << "View: numero ht attivi: "  << idHtAttivi.size() << endl;
    seggio->mutex_stdout.unlock();
    for (unsigned int i  = 0; i < idHtAttivi.size();i++){
        if(!(seggio->isBusyHT(idHtAttivi[i]))){
            QString t = QString::number(idHtAttivi[i]);
            ui->token_attivi_comboBox->addItem(t);
        }
    }
    QString s = QString::number(seggio->getIdHTRiserva());
    ui->id_token_attivabile_label->setText(s);

    ui->stackedWidget->setCurrentIndex(gestioneHT);
}

void MainWindowSeggio::on_logout_button_clicked()
{

    this->logged=false;
    ui->stackedWidget->setCurrentIndex(loginSeggio);
    //setto il seggio affinchè vengano fermati i thread alla successiva esecuzione del costrutto while in essi contenuto
    //seggio->setStopThreads(true);

    //chiedo l'arresto del server che è in ascolto per aggiornare lo stato delle postazioni di voto
    seggio->stopServerUpdatePV();

    delete seggio;
}

//void MainWindowSeggio::on_aggiungiHT_button_clicked()
//{
//    QString codHT = ui->codHT_lineEdit->text();
//    ui->codHT_lineEdit->setText("");
//    ui->token_tableWidget->insertRow(ui->token_tableWidget->rowCount());
//    int rigaAggiunta = ui->token_tableWidget->rowCount()-1;
//    ui->token_tableWidget->setItem(rigaAggiunta,0,new QTableWidgetItem(codHT));
//    QString elimina = "Elimina";
//    ui->token_tableWidget->setItem(rigaAggiunta,1,new QTableWidgetItem(elimina));
//}

//void MainWindowSeggio::on_token_tableWidget_clicked(const QModelIndex &index)
//{
//    int colonnaCliccata=index.column();
//    if(colonnaCliccata==1)
//            ui->token_tableWidget->removeRow(index.row());
//}


//void MainWindowSeggio::initTableHT(){
//ui->token_tableWidget->verticalHeader()->setVisible(false);
//ui->token_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
//ui->token_tableWidget->setSelectionMode(QAbstractItemView::NoSelection);
//QStringList tableHeaders;
//tableHeaders << "S/N hardware token" << "Azione";
//ui->token_tableWidget->setColumnCount(2);
//ui->token_tableWidget->setHorizontalHeaderLabels(tableHeaders);

//ui->token_tableWidget->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
//ui->token_tableWidget->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
//QFont font = ui->token_tableWidget->horizontalHeader()->font();
//font.setPointSize(18);
//ui->token_tableWidget->horizontalHeader()->setFont( font );
//ui->token_tableWidget->horizontalHeader()->setStyleSheet(".QHeaderView{}");

//QFont fontItem("Sans Serif",18);
//ui->token_tableWidget->setFont(fontItem);
//}

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

void MainWindowSeggio::on_logoutCS_button_clicked()
{
    this->logged=false;
    ui->stackedWidget->setCurrentIndex(loginSeggio);
    //setto il seggio affinchè vengano fermati i thread alla successiva esecuzione del costrutto while in essi contenuto
    //seggio->setStopThreads(true);

    //chiedo l'arresto del server che è in ascolto per aggiornare lo stato delle postazioni di voto
    seggio->stopServerUpdatePV();

    delete seggio;
}

void MainWindowSeggio::on_annullaAssociazione_button_clicked()
{
    ui->gestisci_HT_button->setEnabled(true);

    ui->annullaAssociazione_button->hide();
    ui->associazioneDisponibile_label->hide();
    ui->confermaAssociazione_button->hide();
    ui->creaAssociazioneHTPV_button->setEnabled(true);

    seggio->eliminaNuovaAssociazione();
}

void MainWindowSeggio::on_creaAssociazioneHTPV_button_clicked()
{
    ui->creaAssociazioneHTPV_button->setEnabled(false);
    ui->gestisci_HT_button->setEnabled(false);
    seggio->createAssociazioneHT_PV();

    unsigned int ht = seggio->getNuovaAssociazione()->getIdHT();
    unsigned int idPV  = seggio->getNuovaAssociazione()->getIdPV();
    QString s = "HT %1 - PV %2";
    ui->confermaAssociazione_button->setText(s.arg(ht).arg(idPV));


    ui->annullaAssociazione_button->show();

    ui->associazioneDisponibile_label->show();
    ui->confermaAssociazione_button->show();
}

void MainWindowSeggio::on_confermaAssociazione_button_clicked()
{
    ui->gestisci_HT_button->setEnabled(true);

    //completare associazione HT-PV
    seggio->addAssociazioneHT_PV();


    ui->annullaAssociazione_button->hide();
    ui->associazioneDisponibile_label->hide();
    ui->confermaAssociazione_button->hide();


    //controllo

    if(seggio->anyPostazioneLibera()){
        cout << "View: almeno una postazione libera, abilito il bottone di creazione associazione" << endl;
        ui->creaAssociazioneHTPV_button->setEnabled(true);
    }
    else {
        cout << "View: non ci sono altre postazioni libere" << endl;
    }

    //abilitazione bottone per la rimozione delle associazioni
    if(seggio->anyAssociazioneEliminabile()){
        cout << "View: almeno un'associazione eliminabile, attivo il bottone rimuovi associazione" <<endl;
        ui->rimuoviAssociazione_button->setEnabled(true);
    }
    else{
        cout << "View: non ci sono associazioni rimovibili" << endl;
    }
    on_annullaRimozione_button_clicked();
}

void MainWindowSeggio::on_rimuoviAssociazione_button_clicked()
{
    ui->rimuoviAssociazione_button->setEnabled(false);
    //Questa funzione riempie il combo box relativo alle associazioni eliminabili
    ui->associazioneRimovibili_comboBox->clear();


    vector < Associazione > associazioniCorrenti = seggio->getListAssociazioni();
    for(unsigned i=0; i< associazioniCorrenti.size(); ++i){
        unsigned int idPV = associazioniCorrenti[i].getIdPV();
        unsigned int idHT = associazioniCorrenti[i].getIdHT();

        seggio->mutex_stati.lock();
        unsigned int statoPV = seggio->stateInfoPV(idPV);
        seggio->mutex_stati.unlock();


        if(( statoPV == Seggio::attesa_abilitazione) || ( statoPV == Seggio::errore)){
            //creiamo l'item per l'associazione che risulta eliminabile
            QString str = "HT ";
            QString s;
            s.setNum(idHT);
            str.append(s);
            str.append(" - PV ");
            s.setNum(idPV);
            str.append(s);

            ui->associazioneRimovibili_comboBox->addItem(str);
        }
    }

    //contenuto della funzionalità pronto, rendiamolo visibile all'utente
    ui->associazioniRimovibili_label->show();
    ui->associazioneRimovibili_comboBox->show();
    ui->annullaRimozione_button->show();
    ui->rimuovi_button->show();
}

void MainWindowSeggio::on_rimuovi_button_clicked()
{
    QString selectedAss = ui->associazioneRimovibili_comboBox->currentText();
    QString pv;

    //alla posizione di indice 10 è presente l'id della PV da liberare
    //mettiamo il carattere relativo all'id della PV da liberare in una variabile QString
    //QString dovrebbe essere una struttura di tipo queue
    pv.push_back(selectedAss[10]);
    //pv = selectedAss[10];
    unsigned int pvToFree = pv.toInt();
    seggio->mutex_stdout.lock();
    cout << "View: postazione da liberare: " << pvToFree << endl;
    seggio->mutex_stdout.unlock();
    seggio->removeAssociazioneHT_PV(pvToFree);


    ui->rimuoviAssociazione_button->setEnabled(true);

    ui->associazioniRimovibili_label->hide();
    ui->associazioneRimovibili_comboBox->hide();
    ui->rimuovi_button->hide();
    ui->annullaRimozione_button->hide();


    if(/*seggio->getNumberAssCorrenti()<3 && */seggio->anyPostazioneLibera()){
        ui->creaAssociazioneHTPV_button->setEnabled(true);
    }

    if(!(seggio->anyAssociazioneEliminabile())){
        ui->rimuoviAssociazione_button->setEnabled(false);
    }
}

void MainWindowSeggio::on_annullaRimozione_button_clicked()
{
    ui->rimuoviAssociazione_button->setEnabled(true);

    ui->associazioniRimovibili_label->hide();
    ui->associazioneRimovibili_comboBox->hide();
    ui->rimuovi_button->hide();
    ui->annullaRimozione_button->hide();

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

void MainWindowSeggio::on_logout2_button_clicked()
{
    this->logged=false;
    ui->stackedWidget->setCurrentIndex(loginSeggio);
    //setto il seggio affinchè vengano fermati i thread alla successiva esecuzione del costrutto while in essi contenuto
    //seggio->setStopThreads(true);

    //chiedo l'arresto del server che è in ascolto per aggiornare lo stato delle postazioni di voto
    seggio->stopServerUpdatePV();

    delete seggio;
}

void MainWindowSeggio::on_sostituisci_button_clicked()
{
    QString strHTDaDisattivare = ui->token_attivi_comboBox->currentText();
    //attiva il token di riserva e disabilita il token selezionato
    unsigned int idHTDaDisattivare = strHTDaDisattivare.toInt();
    if(!seggio->isBusyHT(idHTDaDisattivare)){
        seggio->disattivaHT(idHTDaDisattivare);
        ui->stackedWidget->setCurrentIndex(gestioneSeggio);
    }
    else{
        //TODO aggiungere label in cui visualizzare il messaggio di errore

    }

}

void MainWindowSeggio::on_goBackToGestioneSeggio_button_clicked()
{
    ui->stackedWidget->setCurrentIndex(gestioneSeggio);
}


