#ifndef UTILS_HPP
# define UTILS_HPP

# include "Webserv.hpp"

namespace Utils
{
    // Type conversions
	bool			toSizeT(const std::string& str, size_t& n);
	bool			toSizeTHex(const std::string& str, size_t& n);
	bool			toInt(const std::string& str, int& n);
    std::string     toString(int n);
    std::string     toUpper(const std::string& str);
	std::string		toLower(const std::string& str);
	std::string		trim(const std::string& str);

    // string
    bool            startsWith(const std::string& str, const std::string& prefix);
    
	bool			decodePath(const std::string& str, std::string& out);
	bool			decodeQuery(const std::string& str, std::string& out);
	std::string		sanitizePath(const std::string& uri);

	std::string		getDate();


}

#endif