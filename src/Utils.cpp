#include "Webserv.hpp"
#include "Utils.hpp"

bool	Utils::toSizeT(const std::string& str, size_t& n)
{
	if (str.empty() || str[0] == '-')
		return (false);

	size_t tmp;
	char c;
	std::istringstream iss(str);
	if (!(iss >> tmp) || iss >> c)
		return (false);
	n =  tmp;
	return (true);
}

bool	Utils::toInt(const std::string& str, int& n)
{
	if (str.empty())
		return (false);

	char c;
	int tmp;
	std::istringstream iss(str);
	if (!(iss >> tmp) || iss >> c)
		return (false);
	n =  tmp;
	return (true);
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

std::string	Utils::toLower(const std::string& str)
{
	if (str.empty())
		return (str);
	std::string s = str;
	for (size_t i = 0; i < s.size(); i++)
	{
		if (s[i] >= 'A' && s[i] <= 'Z')
			s[i] = s[i] + ('a' - 'A');
	}
	return (s);
}

bool Utils::startsWith(const std::string& str, const std::string& prefix)
{
    if (prefix.size() > str.size())
        return false;
    // compare( size_t pos, size_t len, const string& str) 
    // Compare the substring of length len starting at position pos with the string str
    return (str.compare(0, prefix.size(), prefix) == 0);
}

std::string	Utils::trim(const std::string& str)
{
	size_t start = str.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return ("");
	size_t end = str.find_last_not_of(" \t\r\n");
	return (str.substr(start, end - start + 1));
}