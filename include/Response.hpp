#ifndef RESPONSE_HPP
# define RESPONSE_HPP

#include "Webserv.hpp"

class Request;
class ServerConfig;
class LocationConfig;
class SessionManager;

class Response
{
	public:
		Response();
		~Response();

		void	reset();
		bool	isReady() const;
		std::string	getData() const;
		int		getCode() const;
		void	build(const Request& req, const ServerConfig& server, const LocationConfig&);
		void	buildError(int code, const ServerConfig& server);
		void    setCGIResponse(const std::string& cgiOutput, const ServerConfig& server);
		void	setKeepAlive(bool keepAlive);
		bool	getKeepAlive () const;
		void    addHeader(const std::string& key, const std::string& value);
		
	private:
		bool								_ready;
		std::string							_data;
		std::map<std::string, std::string>	_headers;
		std::string							_body;
		int									_statusCode;
		bool								_keepAlive;
		bool								_head;

		bool		_checkMethod(const Request& req, const LocationConfig& location);
		void		_buildResponse();
		void		_handleRedirect(int code, const std::string& url);
		void		_handleGet(const Request& req, const ServerConfig& server, const LocationConfig& location);
		void		_handlePost(const Request& req, const ServerConfig& server, const LocationConfig& location);
		void 		_handleUpload(const Request& request, const ServerConfig& server, const LocationConfig& location);
		void		_handleDelete(const Request& req, const ServerConfig& server, const LocationConfig& location);
		void 		_serveFile(const std::string&path, const ServerConfig& server);
		void 		_serveAutoindex(const std::string& path, const std::string& uri);
		void		_handleSession(const Request& req, SessionManager& sessionManager);
};

#endif
