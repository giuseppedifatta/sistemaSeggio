/*
 * Associazione.h
 *
 *  Created on: 18/gen/2017
 *      Author: giuse
 */

#ifndef ASSOCIAZIONE_H_
#define ASSOCIAZIONE_H_

class Associazione {
private:
	unsigned int idPV;
    unsigned int idHT;


public:
    Associazione(unsigned int idPostazioneVoto, unsigned int idTokenOTP);
	virtual ~Associazione();

	unsigned int getIdPV();
    unsigned int getIdHT();

};

#endif /* ASSOCIAZIONE_H_ */
