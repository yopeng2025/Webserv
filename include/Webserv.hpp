#ifndef WEBSERV_HPP
# define WEBSERV_HPP

// c++ header files
#include <iostream>
#include <string>

// c header files
# include <unistd.h>

// marcos
# define DEFAULT_CONFIG "config/default.conf"

# define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl;
# define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl;

#endif