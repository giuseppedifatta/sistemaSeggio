#ifndef LISTAELETTORALE_H
#define LISTAELETTORALE_H
#include "candidato.h"
#include <vector>
using namespace std;

class ListaElettorale
{
public:
    ListaElettorale();
    ListaElettorale(string nome);
    string getNome() const;
    void setNome(const string &value);
    void addCandidato(Candidato &candidato);
    bool removeCandidato(string matrCandidato);

    vector<Candidato> getCandidati() const;
    vector<Candidato> *getPointerCandidati();
private:
    string nome;
    vector <Candidato> candidati;
};

#endif // LISTAELETTORALE_H
