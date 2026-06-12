#include "CGI.hpp"

CGI::CGI(/* args */)
{
}

CGI::~CGI()
{
}

bool CGI::execute(const Request& request, const LocationConfig& location,
					const ServerConfig& server, const std::string& resolvePath)
{
	std::string absolutePath = resolvePath;
	if (!resolvePath.empty() && resolvePath[0] != '/')
	{
		// current working directory
		// 4096: common maximum path length in Linux
		// /home/login/webserv + / +  cgi-bin/script.py -> /home/login/webserv/cgi-bin/script.py
		char cwd[4096];
		if (getcwd(cwd, sizeof(cwd)) != 0)
		    absolutePath = std::string(cwd) + "/" + resolvePath;
	}

	
}

int CGI::getInputFd()
{
	return (_inputFd);
}

int CGI::getOutputFd()
{
	return (_outputFd);
}