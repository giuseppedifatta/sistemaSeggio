/*
 * risultatiSeggio.cpp
 *
 *  Created on: 17/set/2017
 *      Author: giuseppe
 */

#include "risultatiSeggio.h"

RisultatiSeggio::RisultatiSeggio() {
    idSeggio = 0;

}

RisultatiSeggio::~RisultatiSeggio() {
    // TODO Auto-generated destructor stub
}

unsigned int RisultatiSeggio::getIdSeggio() const {
    return idSeggio;
}

void RisultatiSeggio::setIdSeggio(unsigned int idSeggio) {
    this->idSeggio = idSeggio;
}



RisultatiSeggio::RisultatiSeggio(unsigned int idSeggio) {
    this->idSeggio = idSeggio;
}



vector<SchedaVoto>* RisultatiSeggio::getPointerSchedeVotoRisultato() {
    return &schedeVotoRisultato;
}

void RisultatiSeggio::setSchedeVotoRisultato(
        const vector<SchedaVoto>& schedeVotoRisultato) {
    this->schedeVotoRisultato = schedeVotoRisultato;
}

const vector<SchedaVoto>& RisultatiSeggio::getSchedeVotoRisultato() const {
    return schedeVotoRisultato;
}
