/*
 * risultatiSeggio.h
 *
 *  Created on: 17/set/2017
 *      Author: giuseppe
 */

#ifndef RISULTATISEGGIO_H_
#define RISULTATISEGGIO_H_
#include <vector>
#include "schedavoto.h"
using namespace std;
class RisultatiSeggio {
public:
    RisultatiSeggio();
    RisultatiSeggio(unsigned int idSeggio);
    virtual ~RisultatiSeggio();
    unsigned int getIdSeggio() const;
    void setIdSeggio(unsigned int idSeggio);

    vector<SchedaVoto>* getPointerSchedeVotoRisultato();
    void setSchedeVotoRisultato(const vector<SchedaVoto>& schedeVotoRisultato);
    const vector<SchedaVoto>& getSchedeVotoRisultato() const;

private:
    vector <SchedaVoto> schedeVotoRisultato;
    unsigned int idSeggio;
};

#endif /* RISULTATISEGGIO_H_ */
