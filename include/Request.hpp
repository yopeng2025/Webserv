#ifndef REQUEST_HPP
# define REQUEST_HPP

#include "Webserv.hpp"
#include "Config.hpp"

struct ListenSocket;

class Request
{	
	public:

		enum State
			{
				PARSE_REQUEST_LINE,
				PARSE_HEADERS,
				PARSE_BODY,
				PARSE_CHUNKED,
				PARSE_TRAILER,
				PARSE_COMPLETE,
				PARSE_ERROR
			};

		Request(ListenSocket* ls);
		~Request();

		bool	feed(const std::string& data);
		void	reset();

		const std::map<std::string, std::string>& getHeaders() const;
		std::string			getHeader(const std::string& str) const;
		const std::string&	getMethod() const;
		const std::string&	getRaw() const;
		const std::string&	getVersion() const;
		const std::string&	getPath() const;
		const std::string&	getQuery() const;
		const std::string&	getBody() const;
		State 				getState() const;
		int 				getErrorCode() const;



	private:
		std::string							_raw;
		size_t								_pos;
		std::string							_method;
		std::string							_uri;
		std::string							_path;
		std::string							_query;
		std::string							_version;
		std::map<std::string, std::string>	_headers;
		std::string							_body;
		size_t								_contentLength;
		int									_errorCode;
		size_t								_maxBodySize;
		State								_state;
		const ServerConfig*					_config;
		std::map<std::string, std::string>  _cookies;

	
		bool								_parseRequestLine();
		bool								_parseHeaders();
		bool								_parseBody();
		bool								_parseChunked();
		bool								_parseTrailer();
		bool								_parseUri();
		void								_getBodyType();
		bool								_setError(int error_code);
		std::map<std::string, std::string> 	_parseCookies(const std::string& cookieHeader);

};

#endif
