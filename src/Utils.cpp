#include "Webserv.hpp"
#include "Utils.hpp"

size_t Utils::toSizeT(const std::string& str)
{
    std::istringstream iss(str);
    size_t n;

    if (!(iss >> n))
        throw std::runtime_error("Invalide size_t value: " + str);
    return n;
}

int Utils::toInt(const std::string& str)
{
    std::istringstream iss(str);
    int n;

    if (!(iss >> n))
        throw std::runtime_error("Invalid integer value: " + str);
    return n;
}

std::string     Utils::toString(int n)
{
    std::ostringstream oss;
    oss << n;
    return oss.str();
}