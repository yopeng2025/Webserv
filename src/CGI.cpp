#include "CGI.hpp"
#include "Utils.hpp"

CGI::CGI(): _pid(-1), _inputFd(-1), _outputFd(-1), _startTime(0), _done(false), _bodyWritten(false),
			_bodyWriteOffset(0) {}

CGI::~CGI()
{
	closeFds();
	if (_pid > 0)
	{
		kill(_pid, SIGKILL);
		waitpid(_pid, NULL, 0);
	}
}

void CGI::closeFds()
{
	if (_inputFd >= 0)
	{
		close(_inputFd);
		_inputFd = -1;
	}
	if (_outputFd >= 0)
	{
		close(_outputFd);
		_outputFd = -1;
	}
}

bool CGI::execute(const Request& request, const LocationConfig& location,
					const ServerConfig& server, const std::string& scriptPath)
{
	// current working directory
	// 4096: common maximum path length in Linux
	// /home/login/webserv + / +  cgi-bin/script.py -> /home/login/webserv/cgi-bin/script.py
	std::string absoluteScriptPath = scriptPath;
	if (!scriptPath.empty() && scriptPath[0] != '/')
	{
		char cwd[4096];
		if (getcwd(cwd, sizeof(cwd)) != 0)
		    absoluteScriptPath = std::string(cwd) + "/" + scriptPath;
	}

	// Unix pipe() 单向通信机制，父进程和子进程之间通过管道进行数据传输
	// pipefd[1] 管道的写端 | pipefd[0] 管道的读端

	// CGI：父进程和子进程之间的双向通信（full duplex）
	// 父进程写入数据，子进程从管道读取数据 inputPipe[1]父 -> inputPipe[0]子
	// 子进程写入数据，父进程从管道读取数据 outputPipe[1]子 -> outputPipe[0]父
	// 父（写） -> |子（读-写）| -> 父（读）
	int inputPipe[2];  
	int outputPipe[2];

	if (pipe(inputPipe) < 0)
	{
		LOG_ERROR("CGI: Failed to create input pipe");
		return false;
	}
	if (pipe(outputPipe) < 0)
	{
		LOG_ERROR("CGI: Failed to create output pipe");
		close(inputPipe[1]);
		close(inputPipe[0]);
		return false;
	}
	// fork() -1 fail, 0 child, >0 parent
	_pid = fork();
	if (_pid < 0)
	{
		LOG_ERROR("CGI: Failed to fork process");
		close(inputPipe[1]);
		close(inputPipe[0]);
		close(outputPipe[1]);
		close(outputPipe[0]);
		return false;
	}
	// 子进程 child process
	else if (_pid == 0) 
	{
		// 关闭父进程的写端和读端
		close(inputPipe[1]);
		close(outputPipe[0]);

		// dup2(oldfd, newfd);
		dup2(inputPipe[0], STDIN_FILENO);
		dup2(outputPipe[1], STDOUT_FILENO);

		// close(oldfd)
		close(inputPipe[0]);
		close(outputPipe[1]);

		// Build Environment
		// env = ["pwd=/home/login/webserv", "REQUEST_METHOD=GET", ...]
		// envptr = ["pwd=/home/login/webserv", "REQUEST_METHOD=GET", ... , NULL]
		std::vector<std::string> env = _buildEnvironment(request, server, absoluteScriptPath);
		std::vector<char*> envptr;
		for (size_t i = 0; i < env.size(); i++)
			// std::string -> char* -> const char*
			envptr.push_back(const_cast<char*>(env[i].c_str()));
		envptr.push_back(NULL);
		
		// change directory to the script's directory
		std::string scriptDir = absoluteScriptPath;
		size_t lastSlash = scriptDir.rfind('/');
		if (lastSlash != std::string::npos)
		{
			scriptDir = scriptDir.substr(0, lastSlash);
			chdir(scriptDir.c_str());
		}

		// Execute CGI
		if (location.cgiPath.empty())
			_exit(1);							// exit child process & return 1 to parent process
		std::string cgiPath = location.cgiPath;
		char *argv[3];
		argv[0] = const_cast<char*>(cgiPath.c_str());
		argv[1] = const_cast<char*>(absoluteScriptPath.c_str());
		argv[2] = NULL;
		
		execve(cgiPath.c_str(), argv, envptr.data());
		std::cerr << "CGI: Failed to execute CGI script" << std::endl;
		_exit(1);
	}
	// 父进程 parent process
	else
	{
		close(inputPipe[0]);
		close(outputPipe[1]);

		_inputFd = inputPipe[1];
		_outputFd = outputPipe[0];

		int flag1 = fcntl(_inputFd, F_GETFL);
		int flag2 = fcntl(_outputFd, F_GETFL);
		if (fcntl(inputPipe[1], F_SETFL, flag1 | O_NONBLOCK) < 0 || \
			fcntl(outputPipe[0], F_SETFL, flag2 | O_NONBLOCK) < 0)
		{
			LOG_ERROR("CGI: Failed to set input pipe non-blocking");
			// closeFds();
			// return false; // ！！！原本没有return false， 会导致non-blocking设置失败时，仍然继续执行，后面的读写操作阻塞
		}
	}
	_startTime = time(NULL);
	_done = false;
	_bodyWritten = false;
	return true;
}

std::vector<std::string> CGI::_buildEnvironment(const Request& request,
												const ServerConfig& server,
												const std::string& absoluteScriptPath)
{
	std::vector<std::string> env;

	// 将request请求的相关信息和服务器配置提取出来， 写入CGI需要的环境变量
	env.push_back("REQUEST_METHOD=" + request.getMethod());
	env.push_back("QUERY_STRING=" + request.getQuery());
	env.push_back("CONTENT_TYPE=" + request.getHeader("Content-Type"));
	std::string contentLength = request.getHeader("Content-Length");
	if (contentLength.empty())
		contentLength = "0";
	env.push_back("CONTENT_LENGTH=" + contentLength);
	env.push_back("SCRIPT_NAME=" + request.getPath());
	env.push_back("SCRIPT_FILENAME=" + absoluteScriptPath);
	env.push_back("PATH_INFO=" + request.getPath());
	env.push_back("PATH_TRANSLATED=" + absoluteScriptPath);
	env.push_back("SERVER_NAME=" + server.host);
	env.push_back("SERVER_PORT=" + Utils::toString(server.port));
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SERVER_SOFTWARE=" + std::string(SERVER_NAME));
	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REDIRECT_STATUS=200");	// 200 = OK

	const std::map<std::string, std::string>& headers = request.getHeaders();

	// HTTP headers
	// User-agent -> HTTP_USER_AGENT
	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
		 it != headers.end();
		 it++)
	{
		std::string envName = "HTTP_" + Utils::toUpper(it->first);
		for (size_t i = 0; i < envName.size(); i++)
		{
			if (envName[i] == '-')
				envName[i] = '_';
		}
		env.push_back(envName + "=" + it->second);
	}
	return env;
}

int CGI::getInputFd() const {return (_inputFd);}

int CGI::getOutputFd() const {return (_outputFd);}

const std::string& CGI::getOutput() const {return (_output);}

pid_t CGI::getPid() const {return (_pid);}

time_t CGI::getStartTime() const {return _startTime;}

bool CGI::isDone() const {return _done;}

bool CGI::isBodyWritten() const {return _bodyWritten;}

bool CGI::checkTimeout()
{
	if (_done)
		return false;
	time_t currentTime = time(NULL);
	return (currentTime - _startTime >= CGI_TIMEOUT);
}

void CGI::reapChild()
{
	if (_pid <= 0)
		return ;
	// WNOHANG： Wait NO HANG - non-blocking wait, return immediately if no child has died
	int status;
	pid_t result = waitpid(_pid, &status, WNOHANG); // >0: child exited, 0: child still running, -1: error
	if (result > 0)
		_pid = -1;
	return ;
}

void CGI::kill_process()
{
	if (_pid > 0)
	{
		kill(_pid, SIGKILL);
		int status;
		waitpid(_pid, &status, 0);
		if (WIFEXITED(status))
		{
			int exit_code = WEXITSTATUS(status);
			LOG_INFO("CGI process " << _pid << " exited with code " << exit_code);
		}
		_pid = 0;
	}
	closeFds();
	_done = true;
}

bool CGI::readOutput()
{
	if (_outputFd < 0) // no output fd to read from / CGI process not started
		return true;
	
	char buffer[BUFFER_SIZE];
	ssize_t bytesRead = read(_outputFd, buffer, sizeof(buffer));
	if (bytesRead > 0)
	{
		_output.append(buffer, bytesRead);
		return false;		//还没读完，还要继续读
	}
	if (bytesRead == 0) //EOF
	{
		 close(_outputFd);
		 _outputFd = -1; 
		 reapChild();
		 _done = true;
		 return true;
	}
	if (bytesRead < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return false; // No data available now, try again later
		else
		{
			LOG_ERROR("CGI: Error reading output pipe");
			close(_outputFd);
			_outputFd = -1;
			_done = true;
			return true; // Treat as done on error
		}
	}
	return false; // bytesRead < 0, error occurred, but not EOF
}

void	CGI::setBody(const std::string& body)
{
	_bodyWriteOffset = 0;
	_bodyToWrite = body;
	if (body.empty())
	{
		close(_inputFd);
		_inputFd = -1;
		_bodyWritten = true; // No body to write, mark as done
	}
}

bool CGI::writeBody()
{
	if (_bodyWritten || _inputFd < 0)
		return true; // Already written or no input fd to write to

	size_t bytesToWrite = _bodyToWrite.size() - _bodyWriteOffset;
	if (bytesToWrite == 0)
	{
		close(_inputFd);
		_inputFd = -1;
		_bodyWritten = true; // All body written, mark as done
		return true;
	}

	ssize_t bytesWritten = write(_inputFd, _bodyToWrite.c_str() + _bodyWriteOffset, bytesToWrite);
	if (bytesWritten > 0)
	{
		_bodyWriteOffset += bytesWritten;
		if (_bodyWriteOffset >= _bodyToWrite.size())
		{
			close(_inputFd);
			_inputFd = -1;
			_bodyWritten = true; // All body written, mark as done
			return true;
		}
		return false; // Not all body written yet, need to write more
	}
	if (bytesWritten <= 0)
	{
		// If write fails, check if it's due to EAGAIN or EWOULDBLOCK (non-blocking write would block)
		// EAGAIN: Error， will try AGAIN later
		// EWOULDBLOCK: Error, WOULD BLOCK
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return false;
		close(_inputFd);
		_inputFd = -1;
		_bodyWritten = true; // Error occurred, mark as done to prevent further
		return true;
	}
	return false; // 逻辑上不可达，但能消除警告
}