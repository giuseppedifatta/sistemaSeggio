/*
 * Associazione.h
 *
 *  Created on: 18/gen/2017
 *      Author: giuse
 */

#ifndef ASSOCIAZIONE_H_
#define ASSOCIAZIONE_H_
#include <string>
using namespace std;
class Associazione {
private:
	unsigned int idPV;
    string snHT;
    unsigned int IdTipoVotante;
    unsigned int matricola;


public:
    Associazione(unsigned int idPostazioneVoto, string snHT);
	virtual ~Associazione();

	unsigned int getIdPV();
    unsigned int getMatricola() const;
    void setMatricola(unsigned int value);
    unsigned int getIdTipoVotante() const;
    void setIdTipoVotante(unsigned int value);
    string getSnHT() const;
    void setSnHT(const string &value);
};

#endif /* ASSOCIAZIONE_H_ */
