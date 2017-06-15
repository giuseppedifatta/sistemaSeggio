#ifndef AGGIORNAMENTOSTATOPV_H
#define AGGIORNAMENTOSTATOPV_H

#endif // AGGIORNAMENTOSTATOPV_H

class AggiornamentoStatoPV {
private:
    unsigned int idPV;
    unsigned int stato;
public:
    AggiornamentoStatoPV(unsigned int idPV,unsigned int stato){
        this->idPV=idPV;
        this->stato=stato;
    }
    unsigned int getIdPV(){return this->idPV;}
    unsigned int getStato(){return this->stato;}
};
