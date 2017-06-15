#ifndef MAINWINDOWSEGGIO_H
#define MAINWINDOWSEGGIO_H

#include <QMainWindow>
#include "seggio.h"
#include "associazione.h"
#include <thread>

class Seggio;

namespace Ui {
class MainWindowSeggio;
}

class MainWindowSeggio : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindowSeggio(QWidget *parent = 0);
    ~MainWindowSeggio();
    void sessioneDiVotoTerminata();
    Ui::MainWindowSeggio* getUiPointer();
    bool logged;
    void updatePVbuttons();
    void updateCreaAssociazioneButton();

private slots:
    void on_loginCS_button_clicked();

    void on_quitSystem_button_clicked();

    void on_gestisci_HT_button_clicked();

    void on_accediGestioneSeggio_button_clicked();

    void on_logout_button_clicked();

    void on_annullaAssociazione_button_clicked();

    void on_sostituisci_button_clicked();

    void on_logoutCS_button_clicked();

    void on_creaAssociazioneHTPV_button_clicked();

    void on_letturaRisultatiCS_button_clicked();

    void on_goBackFromRisultatiVoto_button_clicked();

    void on_schedaSuccessiva_button_clicked();

    void on_rimuoviAssociazione_button_clicked();

    void on_rimuovi_button_clicked();

    void on_confermaAssociazione_button_clicked();

    void on_backToLogin_button_clicked();

    void on_logout2_button_clicked();

    void on_annullaRimozione_button_clicked();

    void on_goBackToGestioneSeggio_button_clicked();


private:
    Ui::MainWindowSeggio *ui;
    Seggio *seggio;
    enum InterfaccieSeggio{
        loginSeggio,
        loginPassword,
        gestioneSeggio,
        gestioneHT,
        sessioneConclusa,
        risultatiVoto
    };
    //std::thread thread_1;
protected:
    //void initTableHT();
    void initTableRV();
    void initGestioneSeggio();
};

#endif // MAINWINDOWSEGGIO_H
