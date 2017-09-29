#include "hardwaretoken.h"

HardwareToken::HardwareToken()
{

}

string HardwareToken::getSN() const
{
    return SN;
}

void HardwareToken::setSN(const string &value)
{
    SN = value;
}

string HardwareToken::getUsername() const
{
    return username;
}

void HardwareToken::setUsername(const string &value)
{
    username = value;
}

string HardwareToken::getPassword() const
{
    return password;
}

void HardwareToken::setPassword(const string &value)
{
    password = value;
}
