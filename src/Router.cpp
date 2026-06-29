#include "Router.hpp"
#include "Utils.hpp"
// uri = /images/logo.png
// location path = /images/
// location root = /www
// resolved path = /www/logo.png
std::string Router::resolvePath(const std::string& uri, const LocationConfig& location)
{
    std::string relativePath = uri;
    if (Utils::startsWith(relativePath, location.path))
        relativePath = relativePath.substr(location.path.length()); // images/logo.png -> /logo.png
    
    while (!relativePath.empty() && relativePath[0] == '/')
        relativePath = relativePath. substr(1);                     // /logo.png -> logo.png 去除/     
    
    std::string resolvedPath = Utils::joinPath(location.root, relativePath);
    
    return resolvedPath;
}

bool    Router::isCGI(const LocationConfig& location, const std::string& resolvedPath)
{
    if (location.cgiExtension.empty() || location.cgiPath.empty())
        return false;
    
    // cgiExtension: .py
    // resolvedPath: /www/cgi-bin/script.py
    if (location.cgiExtension.length() > resolvedPath.length())
        return false;

    size_t start = resolvedPath.size() - location.cgiExtension.size();
    int    match = resolvedPath.compare(start, location.cgiExtension.size(), location.cgiExtension);
    return (match == 0);
}
