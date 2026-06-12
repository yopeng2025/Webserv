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
# include <ctime>               // time()


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
# define CLIENT_TIMEOUT 60      // seconds
# define BUFFER_SIZE 4096       // 1KB = 1024 bytes 4KB = 4096 bytes

# define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl;
# define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl;
# define LOG_WARN(msg) std::cerr << "[WARN] " << msg << std::endl;


#endif