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

/*
GET /index.html HTTP/1.1
Host: 		localhost:8080
Connection: keep-alive
User-Agent: Mozilla/5.0
Accept: 	text/html
*/
void	Client::_checkKeepAlive()
{
	std::string connection = Utils::toLower(_request.getHeader("Connection"));
	if (connection == "keep-alive")
		_keepAlive = true;
	else if (connection == "close")
		_keepAlive = false;
	else
		_keepAlive = (_request.getVersion() == "HTTP/1.1");
}

bool Client::readData()
{
	char buffer[BUFFER_SIZE];

	ssize_t bytesRead = recv(_fd, buffer, sizeof(buffer), 0);
	if (bytesRead <= 0)
		return false;

	_lastActivity = time(NULL);

	std::string data(buffer, bytesRead);
	bool isComplete = _request.feed(data);
	if (isComplete)
	{
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
			// [DEBUG request]
			// std::cout << "[Request complete]--------------------\n";
			// std::map<std::string, std::string> headers = _request.getHeaders();
			// for (std::map<std::string, std::string>::iterator it = headers.begin(); it != headers.end(); ++it)
			// 	std::cout << it->first << ": "<< it->second << std::endl;
			// std::cout << _request.getRaw();
		}
	}
	return true;
}

bool Client::sendData()
{
	if (!_response.isReady())
		return true;

	const std::string& data = _response.getData();
	size_t remainData = data.size() - _sendOffset;

	if (remainData == 0)
	{
		_state = STATE_DONE;
		return true;  
	}

	// send() returns the number of bytes actually sent, which may be less than remainData
	ssize_t bytesSent = send(_fd, data.c_str() + _sendOffset, remainData, 0);
	if (bytesSent <= 0)
		return false;

	_sendOffset += bytesSent;
	_lastActivity = time(NULL);

	if (_sendOffset >= data.size())
	{
		_keepAlive = _response.getKeepAlive();
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
	// [DEBUG send]
	// std::cout << "[Data sent]>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";
	// std::cout << data << std::endl;
	// size_t statusCode_postion = data.find("\r\n"); 
	// std::cout << data.substr(0, statusCode_postion) << std::endl;
	// std::cout << "_state: " << _state << std::endl;
	// std::cout << "_keepAlive: " << _keepAlive << std::endl;

	return true;
}

void Client::process(const Config& config, SessionManager& sessionManager)
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
    if (!config.getServers().empty())
      server = &config.getServers()[0];  
    else
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
    _response.buildError(404, *server);
    _state = STATE_SENDING;
    return;
  }

  std::string resolvePath = Router::resolvePath(_request.getPath(), *location); // 根据location config的root和index以及请求的URI，解析出要访问的文件路径
	// [DEBUG path]
	// Request URI: /
	// Resolved path: www
	// std::cout << "Request URI: " << _request.getPath() << std::endl; 
	// std::cout << "Resolved path: " << resolvePath << std::endl;

  _handleSession(sessionManager);
  if (_newSession)
		_response.addHeader("Set-Cookie", "session_id=" + _sessionId + "; Path=/; HttpOnly");

  if (Router::isCGI(*location, resolvePath))
  {
    _cgi = new CGI();
    if (!_cgi->execute(_request, *location, *server, resolvePath))
    {
      delete _cgi;
      _cgi = NULL;
      _response.buildError(500, *server);
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
	ServerConfig defaultServer;
	const ServerConfig* server = _matchedServer ? _matchedServer : &defaultServer;

	if (_cgi->getTimeOut())
	{
		_response.buildError(504, *server);
		delete _cgi;
		_cgi = NULL;
		_matchedServer = NULL;
		_state = STATE_SENDING;
		return ;
	}

	if (!_cgi || !_cgi->isDone())
		return;

	if (_cgi->getOutput().empty())
		_response.buildError(502, *server);
	else
		_response.setCGIResponse(_cgi->getOutput(), *server);

	delete _cgi;
	_cgi = NULL;
	_matchedServer = NULL;
	_state = STATE_SENDING;
}
void	Client::_handleSession(SessionManager& sessionManager)
{
	_newSession = false;
	_sessionId.clear();

	Session* session;

	// Cookies exist
	std::string sessionId = _request.getCookie("session_id");
	if (!sessionId.empty())
	{
		session = sessionManager.getSession(sessionId);
		if (session)
		{
			_sessionId = sessionId;
			session->visitCount++;
			std::cout << "[SESSION]        " << sessionId.substr(0, 8) << "..."
					  << "visit #" << session->visitCount
					  << " " << _request.getMethod() << " " << _request.getPath()
					  << std::endl;
			return ;
		}
	}

	// Cookies do not exist -> create a new session
	_sessionId = sessionManager.createSession();
	session = sessionManager.getSession(_sessionId);
	std::cout << "[SESSION create] "  << _sessionId.substr(0, 8) << "..."
					  << "visit #" << session->visitCount
					  << " " << _request.getMethod() << " " << _request.getPath()
					  << std::endl;
	_newSession = true;

	return ;
}