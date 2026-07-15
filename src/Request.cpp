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
	// 1. 寻找换行符，如果没有找到就继续读；如果字符超出最大值，返回错误代码
	size_t end = _raw.find("\r\n");
	if (end == std::string::npos)
	{
		if (_raw.size() > MAX_REQUEST_LINE)
    		return _setError(414);	// Bad request

		// 没读到一整行 下次再读
		return (false);
	}
	if (end - _pos + 1 > MAX_REQUEST_LINE)
    	return _setError(414);	// Bad request

	// 拷贝这一行
	std::string line = _raw.substr(_pos, end - _pos);
	// ❗把_pos指向到下一行（end指向换行符 +2跳过换行符） 解析完整后统一erase_raw
	_pos = end + 2;

	// 2. 寻找第一个空格 填进method
	size_t sp1 = line.find(' ');
	if (sp1 == std::string::npos)
    	return _setError(400);	// Bad request

	_method = line.substr(0, sp1);

	// 3. 寻找第二个空格 填进uri
	size_t sp2 = line.find(' ', sp1 + 1);
	if (sp2 == std::string::npos)
    	return _setError(400);	// Bad request

	_uri = line.substr(sp1 + 1, sp2 - sp1 - 1);

	// 4. 剩余填进version
	_version = line.substr(sp2 + 1);

	// 5. 查看各部分合法性
	if (_method.empty() || _uri.empty() || _uri[0] != '/' || _version.empty())
    	return _setError(400);	// Bad request

	if (_method != "GET" && _method != "POST" && _method != "DELETE" && _method != "HEAD")
    	return _setError(501);	// Not Implemented

	if (_version != "HTTP/1.0" && _version != "HTTP/1.1")
    	return _setError(505);	//  HTTP Version Not Supported

	if (_uri.size() > MAX_URI_LENTH)
    	return _setError(414);	// URI Too Long

	if(!_parseUri())
    	return _setError(400);	// Bad request

	_state = PARSE_HEADERS;
	return (true);
}

void	Request::_getBodyType()
{
	// 1. 检查Transfer-Encoding是否为chunked （未知body大小，如上传文件时边生成边发送）
	// POST /upload HTTP/1.1
	// Host: localhost
	// Transfer-Encoding: chunked
	//
	// 4\r\n
	// Wiki\r\n
	// 5\r\n
	// pedia\r\n
	// 0\r\n
	// \r\n

	const LocationConfig* location = _config->findLocation(_path);
	if (location)
		_maxBodySize = location->clientMaxBody;

	std::string te = Utils::toLower(getHeader("Transfer-Encoding")); //返回 值chunked
	if (te.size() >= 7 && te.substr(te.size() - 7) == "chunked")
		_state = PARSE_CHUNKED;
	// 2. 检查Content-Length是否存在 （已知body大小，如表单提交）
	// POST /login HTTP/1.1
	// Host: localhost
	// Content-Type: application/x-www-form-urlencoded
	// Content-Length: 29
	//
	// user=abc&pass=123&remember=on
	else
	{
		_state = PARSE_BODY;
		std::string cl = getHeader("Content-Length"); //返回 值29
		if (cl.empty())
		{
			_state = PARSE_COMPLETE;
			return ;
		}
		size_t	contentLength;
		if (!Utils::toSizeT(cl, contentLength))
		{
			_state =  PARSE_ERROR;
			_errorCode = 400;	// Bad request
			return ;
		}
		_contentLength = contentLength;
		if (_contentLength == 0)
			_state = PARSE_COMPLETE;
		else if (_contentLength > _maxBodySize)
		{
			_state =  PARSE_ERROR;
			_errorCode = 413;	// Payload too large
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
		// 跳过之前读过的内容 查找下一行
		size_t end = _raw.find("\r\n", _pos);   		// 修改：删去 _pos+1的 +1
		if (end == std::string::npos)
		{
			if (_raw.size() > MAX_HEADER_SIZE)
    			return _setError(400);	// Bad request

			// 没读到一整行 下次再读
			return (false);
		}

		// 看是否header已结束 结束返回成功（结束时有一行空行）
		if (end == _pos)
		{
			// Body形式
			_getBodyType();
			_pos += 2;
			return (true);
		}
		
		// 拷贝这一行
		std::string line = _raw.substr(_pos, end - _pos);
		_pos = end + 2;

		size_t colon =  line.find(':');
		if (colon == std::string::npos)
    		return _setError(400);	// Bad request

		std::string key = Utils::toLower(Utils::trim(line.substr(0, colon)));
		std::string value = Utils::trim(line.substr(colon + 1));
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
	// 如果size大于最大body 返回错误代码
	if (size > _maxBodySize)
	{
		_pos += size;			// 指针移动到[body\r\n]后面
    	return _setError(413);	// Payload too large
	}
	// 如果size大于标出 不返回错误 剩下的内容留在raw里 下一个request接着读
	// helloworld\r\nGET /index HTTP/1.1
	// 只读 helloworld\r\n;  留下 GET /index HTTP/1.1; 重置_pos=0 指向_raw的[G]ET
	if (size >= _contentLength)
	{
		_body = _raw.substr(_pos, _contentLength);
		_pos += _contentLength;
		_state = PARSE_COMPLETE;
		return (true);
	}
	// size < 标出的大小时 继续读
	// POST /login HTTP/1.1\r\nCont
	// ent-Length: 11\r\n\r\nhello   <-
	// world
	return (false);
}

// POST /upload HTTP/1.1

// 4\r\n
// Wiki\r\n
// 5\r\n
// pedia\r\n
// 0\r\n
// Header-After: value\r\n      <- trailer (optional)  也可以没有trailer
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
    			return _setError(413);	// Payload too large
			return (false);
		}
		if (end + 1 - _pos > _maxBodySize)
    		return _setError(413);	// Payload too large
		else if (end == _pos)
		{
			_pos = end + 2;						// 原本放在else if里面，我放出来让pos向下一行移动增量
			break ;
		}
		_pos = end + 2;		
	}
	_state = PARSE_COMPLETE;
	return (true);
}

// 未知body大小，如上传文件时边生成边发送
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
		// 1. 寻找换行符，如果没有找到就继续读；如果字符超出最大值，返回错误代码
		size_t end = _raw.find("\r\n", _pos);
		if (end == std::string::npos)
		{
			// 没读到一整行 下次再读
			if (_raw.size() - _pos > _maxBodySize)
				return _setError(413);	// Payload too large
			return (false);
		}

		// 2. 拷贝chunckSize
		std::string chunkSize = _raw.substr(_pos, end - _pos);
		size_t size;
		if (!Utils::toSizeTHex(chunkSize, size)) // ❗新function
			return _setError(400);	// Bad request

		// 3. 0为结束 最后一个chunck
		if (size == 0)
		{
			_pos = end + 2;
			_state = PARSE_TRAILER;
			return (true);
		}

		// 5. 追加前检查body大小限制
		if (_body.size() + size > _maxBodySize)
			return _setError(413);	// Payload too large

		// 4. 检查chuck内容部分是否足够
		if (_raw.size() < end + 2 + size + 2)
			return (false);
	
		// 6. 追加body 移动_pos
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

const std::map<std::string, std::string>& Request::getHeaders()  const { return _headers; }
const std::string& Request::getMethod() const { return _method; }
const std::string& Request::getRaw() const { return _raw; }
const std::string& Request::getVersion() const { return _version; }
const std::string& Request::getQuery() const { return _query; }
const std::string& Request::getPath() const { return _path; }
const std::string& Request::getBody() const { return _body; }
Request::State Request::getState() const { return _state; }
int	Request::getErrorCode() const { return _errorCode; }

