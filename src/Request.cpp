#include "Request.hpp"
#include "Server.hpp"

Request::Request(ListenSocket* ls):
	  _pos(0),
	  _contentLength(0),
	  _errorCode(0),
	  _maxBodySize(ls->Config->clientMaxBody),
	  _state(PARSE_REQUEST_LINE),
	  _config(ls->Config) {}

Request::~Request() {}

void	Request::reset()
{
	_raw.erase(0, _pos);
	_pos = 0;
	_method.clear();
	_uri.clear();
	_path.clear();
	_query.clear();
	_version.clear();
	_headers.clear();
	_body.clear();
	_contentLength = 0;
	_errorCode = 0;
	_state = PARSE_REQUEST_LINE;
}

bool	Request::feed(const std::string& data)
{
	_raw += data;

	if (_state == PARSE_REQUEST_LINE)
	{
		if (!_parseRequestLine())
			return (false);
	}
	if (_state == PARSE_HEADERS)
	{
		if (!_parseHeaders())
			return (false);
	}
	if (_state == PARSE_BODY)
	{
		if (!_parseBody())
			return (false);
	}
	if (_state == PARSE_CHUNKED)
	{
		if (!_parseChunked())
			return (false);
	}
	if (_state == PARSE_TRAILER)
	{
		if (!_parseTrailer())
			return (false);
	}

	return (_state == PARSE_COMPLETE || _state == PARSE_ERROR);
}

bool	Request::_setError(int error_code)
{
	_state = PARSE_ERROR;
	_errorCode = error_code;
	return (true);
}

// /login?user=abc&pass=123
bool	Request::_parseUri()
{
	size_t qpos = _uri.find('?');

	std::string pathPart = (qpos == std::string::npos) ? _uri : _uri.substr(0, qpos);
	std::string queryPart = (qpos == std::string::npos) ? "" : _uri.substr(qpos + 1);
	_query = queryPart;

	std::string path;
	if (!Utils::decodePath(pathPart, path))
		return (false);
	std::string query;
	if (!Utils::decodeQuery(queryPart, query))
		return (false);
	_path = Utils::sanitizePath(path);
	
	return (true);
}

// POST /login?user=abc&pass=123 HTTP/1.1
bool	Request::_parseRequestLine()
{
	size_t end = _raw.find("\r\n");
	if (end == std::string::npos)
	{
		if (_raw.size() > MAX_REQUEST_LINE)
    		return _setError(414);						// URI Too Long
		return (false);
	}
	if (end - _pos + 1 > MAX_REQUEST_LINE)
    	return _setError(414);							// URI Too Long

	std::string line = _raw.substr(_pos, end - _pos);
	_pos = end + 2;

	size_t sp1 = line.find(' ');
	if (sp1 == std::string::npos)
    	return _setError(400);							// Bad request

	_method = line.substr(0, sp1);

	size_t sp2 = line.find(' ', sp1 + 1);
	if (sp2 == std::string::npos)
    	return _setError(400);							// Bad request

	_uri = line.substr(sp1 + 1, sp2 - sp1 - 1);

	_version = line.substr(sp2 + 1);

	if (_method.empty() || _uri.empty() || _uri[0] != '/' || _version.empty())
    	return _setError(400);							// Bad request

	if (_method != "GET" && _method != "POST" && _method != "DELETE" && _method != "HEAD")
    	return _setError(501);							// Not Implemented

	if (_version != "HTTP/1.0" && _version != "HTTP/1.1")
    	return _setError(505);							//  HTTP Version Not Supported

	if (_uri.size() > MAX_URI_LENTH)
    	return _setError(414);							// URI Too Long

	if(!_parseUri())
    	return _setError(400);							// Bad request

	_state = PARSE_HEADERS;
	return (true);
}

// 1. check Transfer-Encoding header first (chunked body)
// 2. if not chunked, check Content-Length header (known body size)
void	Request::_getBodyType()
{
	const LocationConfig* location = _config->findLocation(_path);
	if (location)
		_maxBodySize = location->clientMaxBody;

	std::string te = Utils::toLower(getHeader("Transfer-Encoding"));
	if (te.size() >= 7 && te.substr(te.size() - 7) == "chunked")
		_state = PARSE_CHUNKED;
	else
	{
		_state = PARSE_BODY;
		std::string cl = getHeader("Content-Length");
		if (cl.empty())
		{
			_state = PARSE_COMPLETE;
			return ;
		}
		size_t	contentLength;
		if (!Utils::toSizeT(cl, contentLength))
		{
			_state =  PARSE_ERROR;
			_errorCode = 400;							// Bad request
			return ;
		}
		_contentLength = contentLength;
		if (_contentLength == 0)
			_state = PARSE_COMPLETE;
		else if (_contentLength > _maxBodySize)
		{
			_state =  PARSE_ERROR;
			_errorCode = 413;							// Payload too large
			return ;
		}
		else
			_state = PARSE_BODY;
	}
}

//KEY: 				VALUE
//Host: 			example.com
//Content-Type: 	application/x-www-form-urlencoded
//Content-Length: 	29
bool	Request::_parseHeaders()
{
	while (1)
	{
		size_t end = _raw.find("\r\n", _pos);
		if (end == std::string::npos)
		{
			if (_raw.size() > MAX_HEADER_SIZE)
    			return _setError(400);					// Bad request

			return (false);
		}

		if (end == _pos)
		{
			_getBodyType();
			_pos += 2;
			return (true);
		}
		
		std::string line = _raw.substr(_pos, end - _pos);
		_pos = end + 2;

		size_t colon =  line.find(':');
		if (colon == std::string::npos)
    		return _setError(400);						// Bad request

		std::string key = Utils::toLower(Utils::trim(line.substr(0, colon)));
		std::string value = Utils::trim(line.substr(colon + 1));

		if (key == "cookie")
			_cookies = _parseCookies(value);

		_headers[key] = value;
	}
}

// POST /login HTTP/1.1
// Host: localhost
// Content-Type: application/x-www-form-urlencoded
// Content-Length: 27
// \r\n
// user=abc&password=123   <- body
bool	Request::_parseBody()
{
	size_t size = _raw.size() - _pos;
	if (size > _maxBodySize)
	{
		_pos += size;
    	return _setError(413);							// Payload too large
	}
	
	if (size >= _contentLength)
	{
		_body = _raw.substr(_pos, _contentLength);
		_pos += _contentLength;
		_state = PARSE_COMPLETE;
		return (true);
	}
	
	return (false);
}

// POST /upload HTTP/1.1

// 4\r\n
// Wiki\r\n
// 5\r\n
// pedia\r\n
// 0\r\n
// Header-After: value\r\n      <- trailer (optional)
// \r\n\n
bool	Request::_parseTrailer()
{
	// 跳过所有 trailer 直到空行
	while (1)
	{
		size_t end = _raw.find("\r\n", _pos);
		if (end == std::string::npos)
		{
			if (_raw.size() - _pos > _maxBodySize)
    			return _setError(413);					// Payload too large
			return (false);
		}
		if (end + 1 - _pos > _maxBodySize)
    		return _setError(413);						// Payload too large
		else if (end == _pos)
		{
			_pos = end + 2;
			break ;
		}
		_pos = end + 2;		
	}
	_state = PARSE_COMPLETE;
	return (true);
}

// POST /upload HTTP/1.1
// Host: localhost
// Transfer-Encoding: chunked
//
// 4\r\n    		<-_pos指向4
// Wiki\r\n
// 5\r\n
// pedia\r\n
// 0\r\n
// \r\n
bool	Request::_parseChunked()
{
	while (1)
	{
		size_t end = _raw.find("\r\n", _pos);
		if (end == std::string::npos)
		{
			if (_raw.size() - _pos > _maxBodySize)
				return _setError(413);					// Payload too large
			return (false);
		}
		std::string chunkSize = _raw.substr(_pos, end - _pos);
		size_t size;
		if (!Utils::toSizeTHex(chunkSize, size))
			return _setError(400);						// Bad request

		if (size == 0)
		{
			_pos = end + 2;
			_state = PARSE_TRAILER;
			return (true);
		}

		if (_body.size() + size > _maxBodySize)
			return _setError(413);						// Payload too large

		if (_raw.size() < end + 2 + size + 2)
			return (false);
	
		_body += _raw.substr(end + 2, size);;
		_pos = end + 2 + size + 2; 
	}
}

std::string	Request::getHeader(const std::string& str) const
{
	if (str.empty())
		return ("");
	std::string tmp = Utils::toLower(str);
	std::map<std::string, std::string>::const_iterator it = _headers.find(tmp);
	if (it != _headers.end())
		return (it->second);
	return ("");
}

// Cookie: username=Bob; session_id=12345; theme=dark
std::map<std::string, std::string> Request:: _parseCookies(const std::string& cookieHeader)
{
    std::map<std::string, std::string>  cookies;
    std::istringstream                  cookieStream(cookieHeader);
    std::string                         cookiePair;

    while (std::getline(cookieStream, cookiePair, ';'))                 // [ ]username=Bob
    {
        size_t      start = cookiePair.find_first_not_of(' ');          // ->u
		if (start == std::string::npos)
			continue;
        size_t      equalPos = cookiePair.find('=');                    // ->=
        if (equalPos == std::string::npos)                              // usernameBob -> skip
            continue;
        std::string key = cookiePair.substr(start, equalPos - start);   // username
        std::string value = cookiePair.substr(equalPos + 1);            // Bob
        cookies[key] = value;
    }
    // cookies: { "username": "Bob", "session_id": "12345", "theme": "dark" }
    return cookies;
}


const std::string Request::getCookie(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it =
        _cookies.find(key);

    if (it == _cookies.end())
        return "";

    return it->second;
}

const std::map<std::string, std::string>& Request::getHeaders()  const { return _headers; }
const std::string& Request::getMethod() const { return _method; }
const std::string& Request::getRaw() const { return _raw; }
const std::string& Request::getVersion() const { return _version; }
const std::string& Request::getQuery() const { return _query; }
const std::string& Request::getPath() const { return _path; }
const std::string& Request::getBody() const { return _body; }
Request::State Request::getState() const { return _state; }
int	Request::getErrorCode() const { return _errorCode; }
std::map<std::string, std::string> Request::getCookies() const {std::cout << "[get cookies] size = " << _cookies.size() << std::endl; return _cookies;}
