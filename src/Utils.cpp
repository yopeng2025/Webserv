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

std::string Utils::toUpper(const std::string& str)
{
    if (str.empty())
        return str;
    std::string s = str;
    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] >= 'a' && s[i] <= 'z')
            s[i] = s[i] - ('a' - 'A');
    }
    return s;
}

bool Utils::startsWith(const std::string& str, const std::string& prefix)
{
    if (prefix.size() > str.size())
        return false;
    // compare( size_t pos, size_t len, const string& str) 
    // Compare the substring of length len starting at position pos with the string str
    return (str.compare(0, prefix.size(), prefix) == 0);
}