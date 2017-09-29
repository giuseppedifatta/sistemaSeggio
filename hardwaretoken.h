#ifndef HARDWARETOKEN_H
#define HARDWARETOKEN_H
#include <string>
using namespace std;
class HardwareToken
{
public:
    HardwareToken();

    string getSN() const;
    void setSN(const string &value);

    string getUsername() const;
    void setUsername(const string &value);

    string getPassword() const;
    void setPassword(const string &value);

private:
    string SN;
    string username;
    string password;


};

#endif // HARDWARETOKEN_H
