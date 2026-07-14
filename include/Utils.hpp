#ifndef UTILS_HPP
# define UTILS_HPP

# include "Webserv.hpp"

namespace Utils
{
    // Type conversions
	bool			toSizeT(const std::string& str, size_t& n);
	bool			toSizeTHex(const std::string& str, size_t& n);
	int				toInt(const std::string& str);
    std::string     toString(int n);
    std::string     toUpper(const std::string& str);
	std::string		toLower(const std::string& str);
	std::string		trim(const std::string& str);

    // string
    bool            startsWith(const std::string& str, const std::string& prefix);
	bool			decodePath(const std::string& str, std::string& out);
	bool			decodeQuery(const std::string& str, std::string& out);
	std::string		sanitizePath(const std::string& uri);
	std::string		htmlEscape(const std::string& str);
	std::string     trimSpace(const std::string& str);

	// file system
	std::string		getDate();
	std::string     joinPath(const std::string& a, const std::string& b);
	std::string 	addAbsolutePath(const std::string& path);
	// std::string		readFile(const std::string& path);
	bool 			readFile(const std::string& path, std::string& body);
	bool			isDirectory(const std::string& path);
	bool			isRegularFile(const std::string& path);
	bool            pathExists(const std::string& path);

	// HTTP & MIME
	std::string		getMimeType(const std::string& path);
	std::string		getExtension(const std::string& path);
	std::string     defaultErrorPage(int code);
	std::string		getStatusText(int code);
}

#endif