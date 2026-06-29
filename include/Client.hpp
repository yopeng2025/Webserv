#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "Webserv.hpp"
#include "Server.hpp"
#include "Router.hpp"
#include "CGI.hpp"
#include "Response.hpp"

struct ListenSocket;
class Request;

class Client
{
	public:
		// 枚举 背后是整数(0~N)
		enum State
		{
			STATE_READING,
			STATE_PROCESSING,
			STATE_SENDING,
			STATE_CGI_RUNNING,
			STATE_DONE
		};

		Client(int fd, ListenSocket* ls);
		~Client();

		time_t	getLastActivity() const;
		State	getState() const;
		CGI*	getCGI() const;
		int		getFd() const;
		bool	getKeepAlive () const;
		bool	hasTimeout() const;
		bool	readData();
		bool	sendData();
		void	process(const Config& config);
		void	finalizeCGI();


	private:
		int						_fd;
		const ListenSocket*		_listen;
		const ServerConfig*		_server;
		State					_state;
		Request					_request;
		Response				_response;
		CGI*					_cgi;
		size_t					_sendOffset;
		const ServerConfig*		_matchedServer;
		time_t					_lastActivity;
		bool					_keepAlive;

		Client(const Client& other);
		Client& operator=(const Client&);

		void	_checkKeepAlive();
};

#endif