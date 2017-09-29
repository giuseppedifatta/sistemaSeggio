/*
 * Associazione.cpp
 *
 *  Created on: 18/gen/2017
 *      Author: giuse
 */

#include "associazione.h"


unsigned int Associazione::getMatricola() const
{
    return matricola;
}

void Associazione::setMatricola(unsigned int value)
{
    matricola = value;
}

unsigned int Associazione::getIdTipoVotante() const
{
    return IdTipoVotante;
}

void Associazione::setIdTipoVotante(unsigned int value)
{
    IdTipoVotante = value;
}

string Associazione::getSnHT() const
{
    return snHT;
}

void Associazione::setSnHT(const string &value)
{
    snHT = value;
}

Associazione::Associazione(unsigned int idPostazioneVoto, string snHT) {
    this->idPV=idPostazioneVoto;
    this->snHT=snHT;
    
}

Associazione::~Associazione() {
    // TODO Auto-generated destructor stub
}

unsigned int Associazione::getIdPV(){
    return this->idPV;
}


