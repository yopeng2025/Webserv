#ifndef SERVER_HPP
#define SERVER_HPP

#include "Webserv.hpp"
#include "Config.hpp"
#include "Client.hpp"
#include "Utils.hpp"
#include "CGI.hpp"

class Client;

struct ListenSocket
{
	int fd;
	std::string host;
	int port;
	const ServerConfig* Config;
};

class Server
{
	public:
		Server(const Config& config);
		~Server();

		void	run();

	private:
		const Config&										_config;
		bool												_running;
		std::vector<struct pollfd>							_pollfds;
		// Client fd + CGI i/o fd + client
		std::map<int, Client*>								_clients;
		// Listen fd + ListenSocket
		std::map<int, ListenSocket*>						_listenSocket;
		
		void	_createListenSockets();
		int		_createSocket(const std::string& host, int port);

		void	_pollLoop();
		void	_updatePollEvent();
		void	_handlePollEvent();
		void	_processClient();
		void	_checkCGI();
		void	_checkTimeouts();
		void	_removeDoneClient();
		void	_acceptConnection(int listenFd);
		void	_handleClientRead(int fd);
		void	_handleClientWrite(int fd);
		void	_handleCGIRead(int fd);
		void	_handleCGIWrite(int fd);
		void	_removeClient(int fd);

		void	_addPollFd(int fd, short events);
		void	_removePollFd(int fd);
};

#endif