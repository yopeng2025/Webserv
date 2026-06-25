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

bool	Utils::toSizeTHex(const std::string& str, size_t& n)
{
	if (str.empty())
		return (false);

	size_t tmp;
	char c;
	std::istringstream iss(str);
	// 切换成16进制
	iss >> std::hex;
	if (!(iss >> tmp) || iss >> c)
		return (false);
	n =  tmp;
	return (true);
}

bool	Utils::toInt(const std::string& str, int& n)
{
	if (str.empty())
		return (false);
	
	int tmp;
	char c;
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

static bool	isxdigit(char c)
{
	return (std::isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

bool	Utils::decodePath(const std::string& str, std::string& out)
{
	// 百分号解码
	out.clear();
	out.reserve(str.size());
	for (size_t i; i < str.size(); ++i)
	{
		if (str[i] == '%')
		{
			if (i + 2 >= str.size())
				return (false);
			char c1 = str[i + 1];
			char c2 = str[i + 2];
			if (!isxdigit(c1) || !isxdigit(c1))
				return (false);
			std::string hex = str.substr(i + 1, 2);
			int value = std::strtol(hex.c_str(), NULL, 16);
			if (value == 0)
				return (false);
			out += static_cast<char>(value);
			i += 2;
		}
		else
			out += str[i];
		
	}
}

bool	Utils::decodeQuery(const std::string& str, std::string& out)
{
	// 百分号解码
	out.clear();
	out.reserve(str.size());
	for (size_t i; i < str.size(); ++i)
	{
		if (str[i] == '%')
		{
			if (i + 2 >= str.size())
				return (false);
			char c1 = str[i + 1];
			char c2 = str[i + 2];
			if (!isxdigit(c1) || !isxdigit(c1))
				return (false);
			std::string hex = str.substr(i + 1, 2);
			int value = std::strtol(hex.c_str(), NULL, 16);
			if (value == 0)
				return (false);
			out += static_cast<char>(value);
			i += 2;
		}
		// Query+号变空格
		else if (str[i] == '+')
			out += ' ';
		else
			out += str[i];
	}
	return (true);
}

std::string Utils::sanitizePath(const std::string& path)
{
	std::vector<std::string> parts;
	std::istringstream iss(path);
	std::string part;
	while (std::getline(iss, part, '/'))
	{
		if(part.empty() || part == ".")
			continue ;
		if (part == "..")
		{
			if (!parts.empty())
				parts.pop_back();
			else
				parts.push_back(part);
		}
	}
	std::string res = "/";
	for (size_t i = 0; i < parts.size(); ++i)
	{
		res += parts[i];
		if (i + 1 < parts.size())
			res += "/";
	}
	return (res);
}

std::string Utils::getDate()
{
	time_t t = time(NULL);
	struct tm* gmt = gmtime(&t);
	char buf[128];
	strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);
	return (std::string(buf));
}