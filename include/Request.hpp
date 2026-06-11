#ifndef REQUEST_HPP
# define REQUEST_HPP

#include "Webserv.hpp"
#include "Client.hpp"

class Request
{	
	public:
		Request(ListenSocket* ls);
		~Request();

	private:
		std::string							_raw;
		std::string							_method;
		std::string							_uri;
		std::string							_path;
		std::string							_query;
		std::string							_version;
		std::map<std::string, std::string>	_headers;
		std::string							_body;

		size_t								_maxBodySize;
	

};

#endif
