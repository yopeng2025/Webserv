#ifndef CGI_HPP
#define CGI_HPP

class CGI
{
	private:
		int		_inputFd;
		int		_outputFd;

	public:
		CGI();
		~CGI();

		int	getOutputFd();
		int	getInputFd();
};




#endif