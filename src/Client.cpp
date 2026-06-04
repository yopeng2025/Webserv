# include "Client.hpp"

Client::Client(int fd, ListenSocket* ls)
    : _fd(fd),
      _listen(ls), 
      _server(NULL), 
      _state(STATE_READING),
      _request(),
      _response(),
      _cgi(NULL),
      _sendOffset(0),
      _lastActivity(time(NULL))
{
}

Client::~Client()
{
  
}