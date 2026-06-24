#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include "Webserv.hpp"
# include "Request.hpp"
# include "Config.hpp"

class Response
{
	public:
		Response();
		~Response();

		void build(const Request& request, const ServerConfig& server, const LocationConfig& location);
		void buildError(int code, const ServerConfig& server);

	private:
		bool 	_ready;
		void	handleGet(const Request& request, const ServerConfig& server, const LocationConfig& location);
		void	handlePost(const Request& request, const ServerConfig& server, const LocationConfig& location);
		void	handleDelete(const Request& request, const ServerConfig& server, const LocationConfig& location);
		std::string _resolvePath(const Request& request, const LocationConfig& location);
		void 	_serveFile(const std::string&path, const ServerConfig& server);
		void 	_serveAutoIndex(const std::string& path, const std::string& uri);
		void 	_buildResponse(int code, const std::string& body);
};

#endif
