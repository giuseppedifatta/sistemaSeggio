#include "listaelettorale.h"

ListaElettorale::ListaElettorale()
{

}

ListaElettorale::ListaElettorale(string nome)
{
    this->nome = nome;
}

string ListaElettorale::getNome() const
{
    return nome;
}

void ListaElettorale::setNome(const string &value)
{
    nome = value;
}

void ListaElettorale::addCandidato(Candidato &candidato)
{
    candidati.push_back(candidato);
}

bool ListaElettorale::removeCandidato(string matrCandidato)
{
    for (unsigned int index = 0; index < candidati.size();index++){
        if(matrCandidato == candidati.at(index).getMatricola()){
            candidati.erase(candidati.begin()+index);
            return true;
        }
    }
    return false;
}

vector<Candidato> ListaElettorale::getCandidati() const
{
    return candidati;
}

vector<Candidato>* ListaElettorale::getPointerCandidati() {
    return &candidati;
}
