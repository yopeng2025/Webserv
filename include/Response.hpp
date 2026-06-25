#ifndef RESPONSE_HPP
# define RESPONSE_HPP

#include "Webserv.hpp"
#include "Request.hpp"
#include "Config.hpp"

class Response
{
	public:
		Response();
		~Response();

		void	build(const Request& req, const ServerConfig& server, const LocationConfig&);
		void	buildError(int code, const ServerConfig& server);
		bool	isReady() const;
		
	private:
		bool								_ready;
		std::string							_data;
		std::string							_statusLine;
		std::map<std::string, std::string>	_headers;
		std::string							_body;
		int									_statusCode;

		bool		_checkMethod(const Request& req, const LocationConfig& location);
		void		_buildResponse();
		void		_handleRedirect(int code, const std::string& url);
		void		_handleGet(const Request& req, const ServerConfig& server, const LocationConfig& location);
		void		_handlePost(const Request& req, const ServerConfig& server, const LocationConfig& location);
		void		_handleDelete(const Request& req, const ServerConfig& server, const LocationConfig& location);


};



#endif
