#include "Server.hpp"
#include "Utils.hpp"

// Global variable to control server running state
static bool g_running = true;

Server::Server(const Config& config): _config(config), _running(false) {}

Server::~Server() 
{
	for (std::map<int, Client*>::iterator it; it != _clients.end(); ++it)
		delete (it->second);
	_clients.clear();

	for (std::map<int, ListenSocket*>::iterator it; it != _listenSocket.end(); ++ it)
	{
		close(it->first);
		delete (it->second);
	}
	_listenSocket.clear();
	_pollfds.clear();
}

void signalHandler(int sig)
{
    std::cout << "Signal " << sig << " received, shutting down server..." << std::endl;
    g_running = false;
}

void Server::run()
{
    // SIGPIPE： pipe broken signal
    //           signal generated when a process tries to write to a socket that has been closed by the peer.
    //           By default, this signal will terminate the process. 
    // ignore SIGPIPE to prevent crashes when writing to closed sockets
    signal(SIGPIPE, SIG_IGN);
    // SIGINT： signal generated when the user interrupts the process (e.g., by pressing Ctrl+C; signal 2).
    signal(SIGINT, signalHandler);
    // SIGTERM： signal generated to request the termination of the process (e.g., by the "kill" command; signal 15).
    signal(SIGTERM, signalHandler);

    _createListenSockets();

    if (_listenSocket.empty())
     throw std::runtime_error("No valid listen sockets created");

    _running = true;
    LOG_INFO("Server started successfully, entering main loop...");

    _pollLoop();

    LOG_INFO("Server shutting down...");
}

void Server::_createListenSockets()
{
    const std::vector<ServerConfig>& servers = _config.getServers();
    // save created host:port pairs to avoid duplicates
    // set: unique keys, sorted by alphabet
    std::set<std::pair<std::string, int>> createdSockets;

    for (size_t i = 0; i < servers.size(); i++)
    {
        // localhost:8080 or 127.0.0.1:8080
        std::pair<std::string, int> address(servers[i].host, servers[i].port);

        // != .end(): same host:port is already created, skip this iteration to avoid duplicate sockets  （这里对应validateConfig里, 2个同样的host:port只处理第一个host:port）
        // == .end(): address haven't been in createcSockets yet
        if (createdSockets.find(address) != createdSockets.end())
            continue;
        
        int fd = _createSocket(servers[i].host, servers[i].port);
        // fd < 0: fail
        // fd = 0: stdin*
        // fd = 1: stdout
        // fd = 2: stderr
        if (fd >= 0)
        {
			ListenSocket* ls = new ListenSocket();
            ls->fd = fd;
            ls->host = servers[i].host;
            ls->port = servers[i].port;
            ls->Config = &servers[i];
			_listenSocket[fd] = ls;

            // POLLIN： tell POLL to monitor the fd for incoming data (e.g., new connections or data to read)
            //          binary mask e.g. 0x0001
			// POLLIN  = server收请求（recv HTTP request）
			// POLLOUT = server发响应（send HTTP response）
            _addPollFd(fd, POLLIN);
 
            createdSockets.insert(address);
            LOG_INFO("Listening on " << servers[i].host << ":" << servers[i].port);
        }
    }
}

// 1. Create scoket
// 2. Set socket options (SO_REUSEADDR)
// 3. Set non-blocking mode
// 4. Bind this socket to address and port
// 5. Listen for incoming connections
int Server::_createSocket(const std::string& host, int port)
{
    // AF_INET:     IPv4 Internet protocols
    // SOCK_STREAM: provides sequenced, reliable, two-way, connection-based byte streams.
    // 0:           default protocol for the given socket type (TCP for SOCK_STREAM)
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        LOG_ERROR("Failed to create socket for " << host << ":" << port << " - " << strerror(errno));
        return -1;
    }
    
    // Set socket options to reuse address and port
    int opt = 1;
    // SOL_SOCKET:      level for socket options; sol: socket level
    // SO_REUSEADDR:    allow reuse of local addresses (e.g., allow binding to the same port immediately after the server is restarted)
    // &opt:            pointer to the option value (1 to enable)
    // setsockopt:      0 = success, -1 = failure
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        LOG_ERROR("Failed to set SO_REUSEADDR for " << host << ":" << port << " - " << strerror(errno));
        close(fd);
        return -1;
    }

    // Put the socket into non-blocking mode.
    // Operations such as accept(), recv(), send(), connect() return immediately instead of waiting for completion.
    // fcntl:           file control; manipulate file descriptor; 0 = success, -1 = failure
    // F_SETFL:         F: file; SETFL: set flags
    // O_NONBLOCK:      set non-blocking mode for the socket
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
    {
        LOG_ERROR("Failed to set non-blocking mode for " << host << ":" << port << " - " << strerror(errno));
        close(fd);
        return -1;
    }

    // Turn host & port into sockaddr_in structure for bind() or connect()
    // sockaddr_in: structure to hold an [IPv4!] socket address, in means internet
    //              (sin_family, sin_port, sin_addr)
    // sockaddr:    [generic] socket address structure; used for bind(), connect(), accept() etc. (sa_family, sa_data)
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    // sin_family: sin: socket internet; address family (AF_INET for IPv4)
    // sin_port:   port number in network byte order (htons converts from host to network byte order)
    // htons:      host to network short; converts a 16-bit number from host byte order to network byte order (big-endian)
    //             8080(decimal) = 0x1F90 (hex) -> 1F 90 Network Byte Oder bytes(big-endian) {！= 90 1F Host Byte Oder bytes(little-endian)}
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    // sin_addr:    IP address in network byte order
    // s_addr:      socket address; s: socket; addr: address
    // INADDR_ANY:  allows the server to accept connections on any of the host's IP addresses
    if (host == "0.0.0.0" || host.empty())
        addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    
    else    // host = "192.168.1.100"
    {
        // inet_addr: convert IPv4 address from dotted-decimal string format to binary form in network byte order
        //            e.g. 192.168.1.100(decimal) = 0xC0A80164 (hex) -> C0 A8 01 64 Network Byte Oder bytes(big-endian) {！= 64 01 A8 C0 Host Byte Oder bytes(big-endian)(little-endian)}
        //            (! cannot deal with IPv6, inet_pton() can be used instead for both IPv4 and IPv6)
        addr.sin_addr.s_addr = inet_addr(host.c_str());

        // host = google.com
        // inet_addr returns INADDR_NONE (0xFFFFFFFF = 255.255.255.255), which is also a valid IPv4 broadcast address.
        if (addr.sin_addr.s_addr == INADDR_NONE)
        {
            // addrinfo： structure used for address resolution
            //            (ai_family, ai_socktype, ai_protocol, ai_addrlen, ai_addr, ai_canonname, ai_next ...)
            // hints:     input parameter to specify criteria for selecting socket address structures returned by getaddrinfo()
            // res:       output parameter to hold the linked list of addrinfo structures returned by getaddrinfo() on success
            struct addrinfo  hints;
            struct addrinfo  *res;
            std::memset(&hints, 0, sizeof(hints));

            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            // getaddrinfo: resolve the hostname to an IP address; returns 0 on success, non-zero on failure
            // success:     res points to a linked list of addrinfo structures containing the resolved addresses for the hostname
            //              Use the first resolved IPv4 address returned by getaddrinfo().
            if (getaddrinfo(host.c_str(), NULL, &hints, &res) == 0)
            {
                addr.sin_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;
                freeaddrinfo(res);
            }
            // failure
            else
            {
                LOG_ERROR("Failed to resolve host " << host);
                close(fd);
                return (-1);
            }
        }
    }

    // Bind the socket to the specified address and port
    // bind():  -1 failure; =0 success
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        LOG_ERROR("Failed to bind socket for " << host << ":" << port << " - " << strerror(errno));
        close(fd);
        return (-1);
    }

    // Listen for incoming connections
    // SOMAXCONN: maximum number of pending connections in the listen queue
    // -1: failure; =0 success
    if (listen(fd, SOMAXCONN) < 0)
    {
        LOG_ERROR("Failed to listen on socket for " << host << ":" << port << " - " << strerror(errno));
        close(fd);
        return (-1);
    }

    return fd;
}

// Add fd to pollfd structure & set events to monitor for this fd (e.g., POLLIN for incoming data, POLLOUT for ready to send data)
// struct pollfd ()
// {
//      int     fd;
//      short   events;
//      short   revents;    // returned events
// }
void    Server::_addPollFd(int fd, short event)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = event;
    pfd.revents = 0;            // do not return any events yet (only set by poll() when events occur)
    _pollfds.push_back(pfd);
}

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

void	Server::_removeClient(int fd)
{
	std::map<int, Client*>::iterator it =_clients.find(fd);
	if (it != _clients.end())
	{
		CGI* cgi = it->second->getCGI();
		if (cgi)
		{
			if (cgi->getInputFd() >= 0)
				_removePollFd(cgi->getInputFd());
			if (cgi->getOutputFd() >= 0)
				_removePollFd(cgi->getOutputFd());
		}
		_removePollFd(fd);
		delete (it->second);
		_clients.erase(it);
	}
}

void	Server::_acceptConnection(int listenFd)
{
	// int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) 后两个可选
	// create a new socket for the accepted connection and return its fd
	// failure -1 success >=0
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

void	Server::_handleCGIRead(int fd)
{
	std::map<int, Client*>::iterator it = _clients.find(fd);
	Client* c = it->second;
	CGI* cgi = c->getCGI();
	if (cgi)
	{
		bool done = cgi->readOutput();
		if (done)
			_removePollFd(cgi->getOutputFd());
	}
}

void	Server::_handleCGIWrite(int fd)
{
	std::map<int, Client*>::iterator it = _clients.find(fd);
	Client* c = it->second;
	CGI* cgi = c->getCGI();
	if (cgi)
	{
		bool done = cgi->readIntput();
		if (done)
			_removePollFd(cgi->getInputFd());
	}
}

void	Server::_handleClientRead(int fd)
{
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it != _clients.end())
	{
		// 去到client里读数据
		if (!it->second->readData())
			_removeClient(fd);
	}
}

void	Server::_handleClientWrite(int fd)
{
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it != _clients.end())
	{
		// 去到client里发数据
		if (!it->second->sendData())
			_removeClient(fd);
	}
}

void	Server::_updatePollEvent()
{
	for (size_t i = 0; i < _pollfds.size(); ++i)
	{
		int fd = _pollfds[i].fd;
		std::map<int, Client*>::iterator it = _clients.find(fd);
		if (it == _clients.end())
			return ;
		
		Client* c = it->second;
		short events = 0;

		if (c->getState() == Client::STATE_READING)
			events |= POLLIN;
		if (c->getState() == Client::STATE_SENDING)
			events |= POLLOUT;
		if (c->getState() == Client::STATE_CGI_RUNNING)
			events = 0;
		_pollfds[i].events = events;
	}
}

void	Server::_handlePollEvent()
{
	for (size_t i = 0; i < _pollfds.size(); ++i)
	{
		// 判断是那个socket的事件
		int socket = _pollfds[i].fd;
		short revent = _pollfds[i].revents;

		// 1. 看是不是Listen socket有新的连接
		bool isListen = false;
		for (std::map<int, ListenSocket*>::iterator it = _listenSocket.begin(); it != _listenSocket.end(); ++it)
		{
			int fd = it->first;
			if (socket == fd && (revent & POLLIN))		// bitwise AND
			{
				// 新建连接之后，会在_client里新建client对象 并且在pollfd里添加这个client的fd和POLLIN事件
				_acceptConnection(socket);
				isListen = true;
				break ;
			}
		}
		if (isListen)
			continue ;

		// 2. 看是不是CGI pipe ❗可使用_fdToClient
		bool isCGI = false;
		for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		{
			CGI* cgi = it->second->getCGI();
			if (cgi)
			{
				if (socket == cgi->getOutputFd())
				{
					// 不管是读还是挂断 都去读完然后看是否finish
					if (revent & POLLIN || revent & POLLHUP)
						_handleCGIRead(it->first);
				}
				else if (socket == cgi->getInputFd())
				{
					if (revent & POLLOUT)
						_handleCGIWrite(it->first);
				}
				isCGI = true;							//？？？ 这个判断是否要放在handleCGIRead/Write里？ 还是说只要这个socket是CGI的输入输出管道 就不处理client的读写事件了？
														//（因为CGI的输入输出管道和client的读写事件是分开的） 目前放在外面 只要这个socket是CGI的输入输出管道 就不处理client的读写事件了
			}
		}
		if (isCGI)
			continue ;

		// 3. 检查是否有错误、挂断或无效等 并清除client
		if (revent & (POLLERR | POLLHUP | POLLNVAL))
		{
			_removeClient(socket);
			continue ;
		}

		// 4. Client Socket的读和写 (前面已经处理好listen和CGI pipe，所以剩下的就是client socket)
		if (revent & POLLIN)
			_handleClientRead(socket);
			//检查 client是否还存在
		if (_clients.find(socket) == _clients.end())
			continue ;
		if (revent & POLLOUT)
			_handleClientWrite(socket);
	}
}

void	Server::_processClient()
{
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		Client* c = it->second;
		if (c->getState() == Client::STATE_PROCESSING)
		{
			c->process(_config);

			CGI* cgi = c->getCGI();
			if (cgi)
			{
				if (cgi->getInputFd() >= 0)
					_addPollFd(cgi->getInputFd(), POLLOUT);
				if (cgi->getOutputFd() >= 0)
					_addPollFd(cgi->getOutputFd(), POLLIN);
			}
		}
	}
}

void	Server::_checkCGI()
{
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		Client* c = it->second;
		CGI* cgi = c->getCGI();
		
		if (cgi && c->getState() == Client::STATE_CGI_RUNNING)
		{
			cgi->reapChild();
			if (cgi->isDone())
				c->finalizeCGI();
			else if (cgi->checkTimeout())
			{
				if (cgi->getOutputFd() >= 0)
					_removePollFd(cgi->getOutputFd());
				if (cgi->getInputFd() >= 0)
					_removePollFd(cgi->getInputFd());
				cgi->kill();
				c->finalizeCGI();
			}
		}
	}
}

void	Server::_checkTimeouts()
{
	std::vector<int> timeOut;
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second->getState() == Client::STATE_CGI_RUNNING && it->second->hasTimeout())
			timeOut.push_back(it->first);
	}
	for (size_t i = 0; i < timeOut.size(); ++i)
	{
		LOG_WARN("Client timeout, disconnecting fd " << timeOut[i]);
		_removeClient(timeOut[i]);
	}
}

void	Server::_removeDoneClient()
{
	std::vector<int> toRemove;
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second->getState() == Client::STATE_DONE)
			toRemove.push_back(it->first);
	}
	for (size_t i = 0; i < toRemove.size(); ++i)
		_removeClient(toRemove[i]);
}

void	Server::_pollLoop()
{
	while (_running && g_running)
	{
		// 1. 更新client状态 决定监听的动作
		_updatePollEvent();

		// int poll(struct pollfd *fds, nfds_t nfds, int timeout)
		// 1. pollfds -> kernel
		// 2. kernel monitors events on these fds (listen sockets / client sockets / CGI pipes)
		// 3. pollfds.revents = POLLIN/POLLOUT/POLLERR/POLLHUP/POLLNVAL
		// returns how many fds are ready for the requested
        // -1 error; 0 timeout; >0 number of fds with events
		int	ready = poll(&_pollfds[0], _pollfds.size(), 1000);

		// 返回错误或者无事发生
		if (ready <= 0)
        {
            if (!_running || !g_running)
                break;
			continue ;
        }
		
		// 2. 处理事件
		_handlePollEvent();

		// 3. 处理刚读完的 正在process状态的client
		_processClient();

		// 4. 结束CGI 查看CGI是正常结束还是卡死超时
		_checkCGI();

		// 5. 查看HTTP是否超时
		_checkTimeouts();

		// 6. 回收完成生命周期的client （正常client， 非正常的前面都已回收）
		_removeDoneClient();
	}
}
