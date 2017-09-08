/*
 * Associazione.cpp
 *
 *  Created on: 18/gen/2017
 *      Author: giuse
 */

#include "associazione.h"

unsigned int Associazione::getRuolo() const
{
    return ruolo;
}

void Associazione::setRuolo(unsigned int value)
{
    ruolo = value;
}

unsigned int Associazione::getMatricola() const
{
    return matricola;
}

void Associazione::setMatricola(unsigned int value)
{
    matricola = value;
}

Associazione::Associazione(unsigned int idPostazioneVoto, unsigned int idTokenOTP) {
    this->idPV=idPostazioneVoto;
    this->idHT=idTokenOTP;
    
}

Associazione::~Associazione() {
    // TODO Auto-generated destructor stub
}

unsigned int Associazione::getIdPV(){
    return this->idPV;
}

unsigned int Associazione::getIdHT(){
    return this->idHT;
}

