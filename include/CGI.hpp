#ifndef CGI_HPP
# define CGI_HPP

# include "Webserv.hpp"
# include "Config.hpp"
# include "Request.hpp" 

class CGI
{
	public:
		CGI();
		~CGI();

		bool	execute(const Request& request, const LocationConfig& location,
						const ServerConfig& server, const std::string& resolvePath);

		int		getOutputFd();
		int		getInputFd();
	
	private:
		int		_inputFd;
		int		_outputFd;

		CGI(const CGI&);
		CGI& operator=(const CGI&);
};

#endif