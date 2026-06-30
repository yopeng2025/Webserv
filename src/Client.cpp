#include "Client.hpp"
#include "Request.hpp"

Client::Client(int fd, ListenSocket* ls)
	: 	_fd(fd),
		_listen(ls), 
		_server(NULL), 
		_state(STATE_READING),
		_request(ls),
		_response(),
		_cgi(NULL),
		_sendOffset(0),
		_matchedServer(NULL),
		_lastActivity(time(NULL)),
    	_keepAlive(false) {}

Client::~Client()
{
  if (_cgi)
  {
    delete _cgi;
    _cgi = NULL;
  }
  if (_fd >= 0)
  {
    close(_fd);
    _fd = -1;
  }
}

time_t Client::getLastActivity() const {return _lastActivity;}

Client::State Client::getState() const {return _state;}

CGI* Client::getCGI() const {return _cgi;}

int Client::getFd() const {return _fd;}

bool	Client::getKeepAlive() const { return (_keepAlive); }

bool Client::hasTimeout() const
{
  // time(NULL): returns seconds since the Unix epoch (January 1, 1970).
  return ((time(NULL) - _lastActivity) >= CLIENT_TIMEOUT);
}

void	Client::_checkKeepAlive()
{
	std::string connection = Utils::toLower(_request.getHeader("Connection"));
	if (connection == "keep-alive")
		_keepAlive = true;
	else if (connection == "close")
		_keepAlive = false;
	else
		// HTTP/1.1默认keep-alive HTTP/1.0默认close
		_keepAlive = (_request.getVersion() == "HTTP/1.1");
}

bool Client::readData()
{
	char buffer[BUFFER_SIZE];

	// 1. 从clicent socket读取数据（HTTP请求的原始文本数据）到缓冲区 （GET /index.html HTTP/1.1\r\n Host: localhost:8080 ...）
	// = 0 client closed connection, < 0 error occurred, > 0 bytes read successfully
	// recv() 多次调用才能读取1个完整的HTTP request，尤其是当请求体较大时
	// TCP 把 request 切成很多块，服务器server必须自己用feed()将_raw拼回来
	ssize_t bytesRead = recv(_fd, buffer, sizeof(buffer), 0);
	if (bytesRead <= 0)
		return false;

	// 2. 更新时间戳
	_lastActivity = time(NULL);

	// 3. 将数据追加到请求对象中，并检查请求是否完整
	std::string data(buffer, bytesRead);
	bool isComplete = _request.feed(data);
	if (isComplete)
	{
		// 4. 解析请求失败，构建错误响应 ❗错误响应之后应该关闭该客户连接
		if (_request.getState() == Request::PARSE_ERROR)
		{
			ServerConfig defaultServerConfig;
			_response.buildError(_request.getErrorCode(), defaultServerConfig);
			_state = STATE_SENDING;
		}
		else
		{
			_checkKeepAlive();
			_state = STATE_PROCESSING;
			// [DEBUG]
			// std::cout << "Request complete\n";
			// std::cout << _request.getRaw();
		}
	}
	return true;
}

bool Client::sendData()
{
	// 1.  如果响应还没有准备好，继续等待
	if (!_response.isReady())
		return true;

	// 2. 从响应对象获取要发送的数据
	const std::string& data = _response.getData();
	size_t remainData = data.size() - _sendOffset;  //没发送 = 总数据 - 已发送

	// 发送完成， 没有剩余的数据了
	if (remainData == 0)
	{
		_state = STATE_DONE;
		return true;  
	}

	// 3. 发送数据
	// data.c_str() + _sendOffset: 发送数据的起始位置
	// remainData: 还需要发送的数据长度
	// send() returns the number of bytes actually sent, which may be less than remainData
	ssize_t bytesSent = send(_fd, data.c_str() + _sendOffset, remainData, 0);
	if (bytesSent <= 0)
		return false;

	_sendOffset += bytesSent;     // 更新已发送的字节数
	_lastActivity = time(NULL);   // 更新时间戳

	// 全部数据发送完成 根据keep-alive flag决定是否关闭客户
	if (_sendOffset >= data.size())
	{
		if (_keepAlive)
		{
			_request.reset();
			_response.reset();
			_sendOffset = 0;
			_state = STATE_READING;
		}
		else
			_state = STATE_DONE; 
	}
	// [DEBUG]
	// std::cout << "Data sent\n";
	// std::cout << data << std::endl;
	return true;
}

// Routing
// 1. 从Host header中提取host部分
// 2. 根据host和监听端口在配置中找到匹配的server config
void Client::process(const Config& config)
{
  if (_state != STATE_PROCESSING)
    return;
  
  // Host: example.com:8080 -> example.com:8080
  std::string hostHeader = _request.getHeader("Host");
  std::string host;

  size_t colon_index = hostHeader.find(':');
  if (colon_index != std::string::npos)
    host = hostHeader.substr(0, colon_index); //example.com
  
  const ServerConfig* server = config.findServer(host, _listen->port); 
  if (!server)
  {
    if (!config.getServers().empty())    // 如果没有匹配的server config，使用配置中的第一个server config作为默认
      server = &config.getServers()[0];  
    else                                 // 如果给出的.conf文件没有任何server config，构建500错误响应
    {
      ServerConfig defaultServer;
      _response.buildError(500, defaultServer);
      _state = STATE_SENDING;
      return;
    }
  }

  const LocationConfig* location = server->findLocation(_request.getPath());
  if (!location)
  {
    _response.buildError(404, *server); // 如果没有匹配的location config，构建404错误响应
    _state = STATE_SENDING;
    return;
  }

  std::string resolvePath = Router::resolvePath(_request.getPath(), *location); // 根据location config的root和index以及请求的URI，解析出要访问的文件路径
  if (Router::isCGI(*location, resolvePath))
  {
    _cgi = new CGI();
    if (!_cgi->execute(_request, *location, *server, resolvePath))
    {
      delete _cgi;
      _cgi = NULL;
      _response.buildError(500, *server); // 如果CGI执行失败，构建500错误响应
      _state = STATE_SENDING;
      return;
    }
    _cgi->setBody(_request.getBody());
    _matchedServer = server;
    _state = STATE_CGI_RUNNING;
    return ;
  }
  _response.setKeepAlive(_keepAlive);
  _response.build(_request, *server, *location);
  _state = STATE_SENDING;
}

void Client::finalizeCGI()
{
	if (!_cgi || !_cgi->isDone())
		return;

	ServerConfig defaultServer;
	const ServerConfig* server = _matchedServer ? _matchedServer : &defaultServer;

	if (_cgi->getOutput().empty())
		_response.buildError(502, *server);
	else
		_response.setCGIResponse(_cgi->getOutput(), *server);

	delete _cgi;
	_cgi = NULL;
	_matchedServer = NULL;
	_state = STATE_SENDING;
}