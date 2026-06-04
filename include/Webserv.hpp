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
# include <limits>
# include <cctype> // std::isspace


// c header files
# include <unistd.h>
# include <poll.h> // poll()
# include <sys/types.h> // accept / socket / bind / listen
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <fcntl.h> // fcntl()

// marcos
# define DEFAULT_CONFIG "config/default.conf"

# define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl;
# define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl;

#endif