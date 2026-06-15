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
						const ServerConfig& server, const std::string& scriptPath);

		int		getOutputFd() const;
		int		getInputFd() const;
		pid_t	getPid() const;
	
	private:
		pid_t	_pid;
		int		_inputFd;
		int		_outputFd;
		time_t	_startTime;
		bool	_done;
		bool	_bodyWriten;

		std::vector<std::string>	_buildEnvironment(const Request& request,
													  const ServerConfig& server,
													  const std::string& absoluteScriptPath);

		CGI(const CGI&);
		CGI& operator=(const CGI&);
};

#endif