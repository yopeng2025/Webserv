#ifndef WEBSERV_HPP
# define WEBSERV_HPP

// c++ header files
# include <iostream>
# include <string>
# include <vector>
# include <map>
# include <set>
# include <fstream>
# include <sstream>
# include <limits>              // std::numeric_limits, SOMAXCONN
# include <cctype>              // std::isspace
# include <cstring>             // std::strerror


// c header files
# include <unistd.h>
# include <poll.h>              // poll()
# include <signal.h>            // signal()
# include <sys/socket.h>        // socket(), bind(), listen(), accept()
# include <sys/types.h>         // socket(), bind(), listen(), accept()
# include <fcntl.h>             // fcntl()
# include <netinet/in.h>        // sockaddr_in, htons(), INADDR_ANY 
# include <arpa/inet.h>         // inet_addr()
# include <netdb.h>             // addrinfo(), getaddrinfo()   
# include <unistd.h>
// marcos
# define DEFAULT_CONFIG "config/default.conf"

# define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl;
# define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl;
# define LOG_WARN(msg) std::cerr << "[WARN] " << msg << std::endl;


#endif