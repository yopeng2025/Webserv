#ifndef ROUTER_HPP
# define ROUTER_HPP

# include "Webserv.hpp"
# include "Config.hpp"
# include "Request.hpp"

namespace Router
{
    std::string resolvePath(const std::string& uri, const LocationConfig& location);
}

#endif