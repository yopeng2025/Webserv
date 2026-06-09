#ifndef UTILS_HPP
# define UTILS_HPP

# include "Webserv.hpp"

namespace Utils
{
    // Type conversions
    size_t          toSizeT(const std::string& str);
    int             toInt(const std::string& str);
    std::string     toString(int n);
    std::string     toUpper(const std::string& str);

    // string
    bool            startsWith(const std::string& str, const std::string& prefix);
    
}

#endif