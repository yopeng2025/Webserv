#include "Router.hpp"

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
