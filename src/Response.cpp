#include "Response.hpp"

Response::Response(): _ready(false) {}

Response::~Response() {}

bool	Response::isReady() const { return (_ready); }


void	Response::buildError(int code, const ServerConfig& server)
{
	_statusCode = code;

	// 查看自定义错误页
	std::map<int, std::string>::const_iterator it = server.errorPages.find(code);
	if (it != server.errorPages.end())
	{
		_body = Utils::readFile(it->second);
		if (_body.empty())
			_body = Utils::defaultErrorPage(_statusCode);
	}
	else
		_body = Utils::defaultErrorPage(_statusCode);
	_headers["Content-Type"] = "text/html";
	_buildResponse();
}

void    Response::_buildResponse()
{
	_data = "HTTP/1.1";
	_data += Utils::toString(_statusCode) + " ";
	_data += Utils::getStatusText(_statusCode) + "\r\n";

	_data += "Server: ";
	_data += SERVER_NAME;
	_data += "\r\n";

	_data += "Date: " + Utils::getDate() + "\r\n";

	for (std::map<std::string, std::string>::iterator it = _headers.begin(); it != _headers.end(); ++it)
		_data += it->first + ": " + it->second + "\r\n";
	
	if (_headers.find("Content-Length") == _headers.end())
	{
		_data += "Content-Length";
		_data += _body.size();
		_data += "\r\n";
	}

	if (_headers.find("Connection") == _headers.end())
		_data += "Connection: close\r\n";
	
	_data += "\r\n";
	_data += _body;

	_ready = true;
}

void    Response::_handleRedirect(int code, const std::string& url)
{
	_statusCode = code;
	_headers["Location"] = url;
	_headers["Content-Type"] = "text/html";
	_body = 
			"<html><body><h1>" +
			Utils::toString(code) +
			" " +
			Utils::getStatusText(code) +
			// 超链接可以点击 给不支持自动跳转的客户端使用
			"</h1><p>Redirecting to <a href=\"" +
			url +
			"\">" +
			url +
			"</a></p></body></html>";
	_buildResponse();
}

bool	Response::_checkMethod(const Request& req, const LocationConfig& location)
{
	// 405 比较特殊 需要加Allow header 所以走正常的buildResponse（builderror不能加额外的header）
	std::string rMethod = req.getMethod();
	if (location.methods.find(rMethod) != location.methods.end())
	{
		std::string allow;
		for (std::set<std::string>::const_iterator it = location.methods.begin(); it != location.methods.end(); ++it)
		{
			if (!allow.empty())
				allow += ", ";
			allow += *it;
		}
		_headers["Content-Type"] = "text/html";
		_headers["Allow"] = allow;
		_statusCode = 405;
		_body = Utils::defaultErrorPage(_statusCode);
		return (false);
	}
	return (true);
}

void	Response::build(const Request& req,
						const ServerConfig& server,
						const LocationConfig& location)
{
	if (!_checkMethod(req, location))
		return ;

	if (location.redirectCode > 0 && !location.redirect.empty())
	{
		_handleRedirect(location.redirectCode, location.redirect);
		return ;
	}

	if (req.getMethod() == "GET")
		_handleGet(req, server, location);
	if (req.getMethod() == "POST")
		_handlePost(req, server, location);
	if (req.getMethod() == "DELETE")
		_handleDelete(req, server, location);
	else
	// 理论上不会到这里，_parseRequestLine 已经过滤
		buildError(501, server);
}
