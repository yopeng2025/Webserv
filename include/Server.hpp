#ifndef SERVER_HPP
#define SERVER_HPP

#include "Webserv.hpp"
#include "Config.hpp"
#include "Client.hpp"
#include "Utils.hpp"

#include <poll.h> // poll()

class Server
{
	public:
		Server(const Config& config);
		~Server();

		void	run();

	private:
		const Config&										_config;
		bool												_running;
		std::vector<struct pollfd>							_pullfds;
		std::vector<int>									_listenFds;
		// Fd + client
		std::map<int, Client*>								_clients;
		// Fd + host:port (pair)
		std::map<int, std::pair<std::string, int>>			_listenSocket;
		
		void	_createListenSockets();
		int		_createSocket(const std::string& host, int port);

		void	_pollLoop();
		void	_acceptConnection(int listenFd);
		void	_handleClientRead(int fd);
		void	_handleClientWrite(int fd);
		void	_handleCGIRead(int fd);
		void	_handleCGIWrite(int fd);
		void	_removeClient(int fd);
		void	_checkTimeouts();

		void	_addPollFd(int fd, short events);
		void	_removePollFd(int fd);
};

#endif