#include "Server.hpp"
#include "Utils.hpp"

// Global variable to control server running state
static bool g_running = true;

Server::Server(const Config& config): _config(config), _running(false) {}

Server::~Server() 
{
    // NEED TO FILL
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

    if (_listenFds.empty())
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

        // != .end(): same host:port is already created, skip this iteration to avoid duplicate sockets
        // == .end(): address havent been in createcSockets yet
        if (createdSockets.find(address) != createdSockets.end())
            continue;
        
        int fd = _createSocket(servers[i].host, servers[i].port);
        // fd < 0: fail
        // fd = 0: stdin
        // fd = 1: stdout
        // fd = 2: stderr
        if (fd >= 0)
        {
            _listenFds.push_back(fd);
            _listenSocket[fd] = address;
            // POLLIN： tell POLL to monitor the fd for incoming data (e.g., new connections or data to read)
            //          binary mask e.g. 0x0001
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


/*
poll loop:
1. update client poll events
2. poll()
3. handle events:
    a. if event on listen socket: accept new connection
    b. if event on client socket: read/write data [recev() / send()]
4. CGI handling (pipe read/write to poll)
*/