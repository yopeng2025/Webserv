#include "Server.hpp"

static bool g_running = true;

bool	setNonBlocking(int clientFd)
{
	// 查看并修改fd属性 设置non-blocking模式
	int flag = fcntl(clientFd, F_GETFL, 0);
	if (flag == -1)
		return (false);
	if (fcntl(clientFd, F_SETFL, flag | O_NONBLOCK) == -1)
		return (false);
	return (true);
}

void	Server::_addPollFd(int fd, short event)
{
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = event;
	pfd.revents = 0;
	_pollfds.push_back(pfd);
}

void	Server::_removePollFd(int fd)
{
	for (std::vector<struct pollfd>::iterator it = _pollfds.begin(); it != _pollfds.end(); ++it)
	{
		if (it->fd == fd)
		{
			_pollfds.erase(it);
			return ;
		}
	}
}

void	Server::_acceptConnection(int listenFd)
{
	// int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) 后两个可选
	int clientFd = accept(listenFd, NULL, NULL);
	if (clientFd < 0)
		return ;

	// 设置non-blocking模式
	if (!setNonBlocking(clientFd))
	{
		LOG_ERROR("fcntl() failed on client fd");
		return ;
	}

	// ❗找到对应server（结构有修改）
	std::map<int, ListenSocket*>::iterator it = _listenSocket.find(listenFd);
	ListenSocket* ls;
	if (it != _listenSocket.end())
		ls = it->second;
	
	Client* client = new Client(clientFd, ls);
	_clients[clientFd] = client;
	// 在发送respnse之前 先只允许POLLIN
	_addPollFd(clientFd, POLLIN);
}

void	Server::_pollLoop()
{
	while (_running && g_running)
	{
		// 更新client状态 决定监听的动作
		for (size_t i = 0; i < _pollfds.size(); ++i)
		{
			int fd = _pollfds[i].fd;
			std::map<int, Client*>::iterator it = _clients.find(fd);
			if (it != _clients.end())
			{
				short events = 0;
				if (it->second->getState() == Client::STATE_READING)
					events |= POLLIN;
				if (it->second->getState() == Client::STATE_SENDING)
					events |= POLLOUT;
				if (it->second->getState() == Client::STATE_CGI_RUNNING)
					events = 0;
				_pollfds[i].events = events;
			}
		}

		// int poll(struct pollfd *fds, nfds_t nfds, int timeout)
		int	ready = poll(&_pollfds[0], _pollfds.size(), 1000);

		// 返回错误或者无事发生
		if (ready <= 0)
			continue ;
		
		// 处理事件
		for (size_t i = 0; i < _pollfds.size(); ++i)
		{
			// 判断是那个socket的事件
			int socket = _pollfds[i].fd;
			short revent = _pollfds[i].revents;

			// 1. 看是不是Listen socket有新的连接
			bool isListen = false;
			for (size_t j = 0; j < _listenFds.size(); ++i)
			{
				if (socket == _listenFds[j] && (revent & POLLIN))
				{
					_acceptConnection(socket);
					isListen = true;
					break ;
				}
			}
			if (isListen)
				continue ;

			// 2. 看是不是CGI pipe
			bool isCGI = false;
			for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
			{
				CGI* cgi = it->second->getCGI();
				if (cgi)
				{
					if (cgi->getOutputFd() == socket)
					{

					}
					if (cgi->getInputFd() == socket)
					{
						
					}
				}
			}

		}
	}



}