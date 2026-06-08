#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "Webserv.hpp"
#include "Server.hpp"

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

		State	getState() const;
		CGI*	getCGI() const;
		bool	hasTimeout() const;

	private:
		int						_fd;
		const ListenSocket*		_listen;
		const ServerConfig*		_server;
		State					_state;
		Request					_request;
		Response				_response;
		CGI*					_cgi;
		size_t					_sendOffset;
		time_t					_lastActivity;

	Client(const Client& other);
	Client& operator=(const Client&);
};

#endif