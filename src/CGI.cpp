#include "CGI.hpp"

CGI::CGI(/* args */)
{
}

CGI::~CGI()
{
}

int CGI::getInputFd()
{
	return (_inputFd);
}

int CGI::getOutputFd()
{
	return (_outputFd);
}