#ifndef MAINWINDOWSEGGIO_H
#define MAINWINDOWSEGGIO_H

#include <QMainWindow>
#include <QMessageBox>
#include "seggio.h"
#include "associazione.h"
#include "risultatiSeggio.h"
#include <thread>
#include <vector>
#include <QObject>
Q_DECLARE_METATYPE(std::vector <Associazione>)
class Seggio;

namespace Ui {
class MainWindowSeggio;
}

class MainWindowSeggio : public QMainWindow
{
    Q_OBJECT
    
signals:
    void needNewAssociation();
    void needStatePVs();
    void confirmAssociation();
    void abortAssociation();
    void needRemovableAssociations();
    void checkPassKey(QString pass);
    void logoutRequest();
    void associationToRemove(uint pvToFree);
    void confirmVotazioneCompleta(uint pvToFree);
    void wantVote(uint matricola);
    void needMatricolaInfo(uint matricola);
    void seggioLogged(bool);
    void tryRemoveStateMatricola(uint matricola, uint motivoAbort);
    void needStatoGeneratori();
    void disattivaHT(string snHT);
    void needRisultatiVoto();
public slots:
    //aggiornamento bottoni crea_associazione, rimuovi associazione e postazioni voto
    void updateCreaAssociazioneButton(bool b);
    void updateRimuoviAssociazioneButton(bool b);
    void updatePVButtons(unsigned int idPVtoUpdate, unsigned int statoPV); 
    void showNewAssociation(string ht, unsigned int idPV);
    void initSeggio();
    void showErrorPass();
    void doLogout();
    void showErrorLogout();
    void showRemovableAssociations(std::vector<Associazione> associazioniRimovibili);
    void showMessageSessioneEnded();
    void showMessageSessionNotStarted();
    void showMessageForbidVote(string esitoLock);
    void showInfoMatricola(QString info);
    void showErrorUrnaUnreachable();
    void showErrorAbortVoting(uint matricola);
    void hideCreaAssociazione();
    void showMessageAssociationRemoved(uint motivoAbort);
    void showManageToken(std::vector<string> snHTdisattivabili, string snHTdisattivo);
    void showTokenScambiati(string disativato, string attivato);
    void showMessageNextSessione(QDateTime inizioProssimaSessione,QDateTime fineProssimaSessione);
    void showViewAskRisultati();
    void showRisultatiProcedura(vector<RisultatiSeggio> risultatiSeggi);
    void showMessageNotScrutinio();
private:
    Ui::MainWindowSeggio *ui;
    Seggio *seggio;
    bool logged;
    enum InterfacceSeggio{
        loginSeggio,
        loginPassword,
        gestioneSeggio,
        gestioneHT,
        proceduraConclusa,
        risultatiVoto
    };

    enum columnRisultatiVoto{
        SEGGIO,
        CANDIDATO,
        DATA,
        LUOGO,
        NUM_VOTI,
        LISTA
    };

    vector <RisultatiSeggio> risultatiSeggioOttenuti;
    uint indexSchedaRisultatoDaMostrare;
    void showSchedaRisultato(uint indexScheda, vector<RisultatiSeggio> &risultatiSeggi);
public:
    explicit MainWindowSeggio(QWidget *parent = 0);
    ~MainWindowSeggio();

    
    //void initTableHT();
    void initTableRV();
    void initGestioneSeggio();

private slots:
    void on_loginCS_button_clicked();

    void on_quitSystem_button_clicked();

    void on_gestisci_HT_button_clicked();

    void on_accediGestioneSeggio_button_clicked();

    void on_logout_button_clicked();

    void on_annullaAssociazione_button_clicked();

    void on_sostituisci_button_clicked();

    void on_logoutRisultatiVoto_button_clicked();

    void on_creaAssociazioneHTPV_button_clicked();

    void on_letturaRisultatiCS_button_clicked();

    void on_goBackFromRisultatiVoto_button_clicked();

    void on_rimuoviAssociazione_button_clicked();

    void on_rimuovi_button_clicked();

    void on_confermaAssociazione_button_clicked();

    void on_backToLogin_button_clicked();

    void on_logoutRV_button_clicked();

    void on_annullaRimozione_button_clicked();

    void on_goBackToGestioneSeggio_button_clicked();

    void on_pv1_button_clicked();
    void on_pv2_button_clicked();
    void on_pv3_button_clicked();
    void on_pushButton_letVote_clicked();
    void on_pushButton_infoMatricola_clicked();
    void on_lineEdit_matricolaElettore_textChanged(const QString &arg1);
    void on_pushButton_schedaSuccessiva_clicked();
};

#endif // MAINWINDOWSEGGIO_H
