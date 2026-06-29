#ifndef CGI_HPP
# define CGI_HPP

# include "Webserv.hpp"
# include "Config.hpp"
# include "Request.hpp" 

class Request;

class CGI
{
	public:
		CGI();
		~CGI();

		bool		execute(const Request& request, const LocationConfig& location,
							const ServerConfig& server, const std::string& scriptPath);

		int			getOutputFd() const;
		int			getInputFd() const;
		const std::string&	getOutput() const;
		pid_t		getPid() const;
		time_t  	getStartTime() const;
		bool		isDone() const;
		bool		checkTimeout();
		bool		readOutput();
		bool		isBodyWritten() const;

		void    	closeFds();
		void		reapChild();
		void		kill_process();
		void		setBody(const std::string& body);
		bool		writeBody();
		
	private:
		pid_t		_pid;
		int			_inputFd;
		int			_outputFd;
		time_t		_startTime;
		bool		_done;
		bool		_bodyWritten;
		std::string _bodyToWrite;
		size_t  	_bodyWriteOffset;
		std::string _output;
		

		std::vector<std::string>	_buildEnvironment(const Request& request,
													  const ServerConfig& server,
													  const std::string& absoluteScriptPath);

		CGI(const CGI&);
		CGI& operator=(const CGI&);
};

#endif