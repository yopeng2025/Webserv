#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "Webserv.hpp"
#include "Server.hpp"
#include "Request.hpp"
#include "Response.hpp"

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

		bool	hasTimeout() const;
		bool	readData();
		bool	sendData();
		void	process(const Config& config);

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

		Client(const Client& other);
		Client& operator=(const Client&);
};

#endif