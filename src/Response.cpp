#include "Response.hpp"
#include "Router.hpp"
#include "Utils.hpp"
#include "SessionManager.hpp"

Response::Response(): _ready(false), _statusCode(0), _keepAlive(false), _head(false) {}

Response::~Response() {}

void	Response::reset()
{
	_ready = false;
	_data.clear();
	_headers.clear();
	_body.clear();
	_statusCode = 0;
	_keepAlive = false;
    _head = false;
}

bool	Response::isReady() const { return (_ready); }

void	Response::setKeepAlive(bool keepAlive)	{ _keepAlive = keepAlive; }

bool	Response::getKeepAlive() const { return (_keepAlive); }

std::string	Response::getData() const { return (_data); }
int Response::getCode() const { return (_statusCode); }


void	Response::buildError(int code, const ServerConfig& server)
{
	_statusCode = code;
	_keepAlive = false;

	std::map<int, std::string>::const_iterator it = server.errorPages.find(code);
	if (it != server.errorPages.end())
	{
		std::string body;
		if (!Utils::readFile(it->second, body))
			_body = Utils::defaultErrorPage(_statusCode);
		else
			_body = body;
	}
	else
		_body = Utils::defaultErrorPage(_statusCode);
	_headers["Content-Type"] = "text/html";
	_buildResponse();
}

void    Response::_buildResponse()
{
	_data = "HTTP/1.1 ";
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
		_data += "Content-Length: ";
		_data += Utils::toString(_body.size());
		_data += "\r\n";
	}

	if (_keepAlive)
		_data += "Connection: keep-alive\r\n";
	else
		_data += "Connection: close\r\n";

	_data += "\r\n";

	if (!_head)
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
			"</h1><p>Redirecting to <a href=\"" +
			url +
			"\">" +
			url +
			"</a></p></body></html>";
	_buildResponse();
}

bool	Response::_checkMethod(const Request& req, const LocationConfig& location)
{
	std::string rMethod = req.getMethod();
	if (location.methods.find(rMethod) == location.methods.end())
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
		_keepAlive = false;
		_statusCode = 405;
		_body = Utils::defaultErrorPage(_statusCode);
        _buildResponse();
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
    else if (req.getMethod() == "HEAD")
    {
        _head = true;
		_handleGet(req, server, location);
    }
	else if (req.getMethod() == "POST")
		_handlePost(req, server, location);
	else if (req.getMethod() == "DELETE")
		_handleDelete(req, server, location);
	else
		buildError(501, server);
}

void Response::_handleGet(const Request& request, const ServerConfig& server, const LocationConfig& location)
{
    std::string path = Router::resolvePath(request.getPath(), location);
    
    if (Utils::isDirectory(path))
    {
        if (!location.index.empty())
        {
            std::string indexPath = Utils::joinPath(path, location.index);
            
            if (Utils::isRegularFile(indexPath))
            {
                _serveFile(indexPath, server);
                return ;
            }
        }
        if (location.autoindex)
        {
            _serveAutoindex(path, request.getPath());
            return ;
        }

        return  buildError(404, server);
    }

    if (Utils::isRegularFile(path))
    {
        _serveFile(path, server);
        return ;
    }

    buildError(404, server);
}

void Response::_handlePost(const Request& request, const ServerConfig& server, const LocationConfig& location)
{
    if (!location.uploadPath.empty())
    {
        _handleUpload(request, server, location);
        return ;
    }
    else
        _handleGet(request, server, location);
}

void Response::_handleUpload(const Request& request, const ServerConfig& server, const LocationConfig& location)
{
    std::string contentType = request.getHeader("Content-Type");
    const std::string& body = request.getBody();

    std::string uploadPath = location.uploadPath;
    if (!Utils::isDirectory(uploadPath))
    {
        if (mkdir(uploadPath.c_str(), 0755) != 0)
        return buildError(500, server);
    }
    
    std::string filename;
    std::string fileContent;
    if (contentType.find("multipart/form-data") != std::string::npos)
    {
        size_t bpos = contentType.find("boundary=");
        if (bpos == std::string::npos)
            return buildError(400, server);
        std::string boundary = "--" + contentType.substr(bpos + 9);

        size_t startPart = body.find(boundary);
        if (startPart == std::string::npos)
            return buildError(400, server);
        startPart += boundary.size() + 2;

        size_t dispPos = body.find("Content-Disposition:", startPart);
        if (dispPos == std::string::npos)
            return buildError(400, server);
        
        size_t filenamePos = body.find("filename=\"", dispPos);
        if (filenamePos == std::string::npos)
            return buildError(400, server);
        filenamePos += 10;
        size_t filenameEnd = body.find("\"", filenamePos);
        if (filenameEnd == std::string::npos)
            return buildError(400, server);
        filename = body.substr(filenamePos, filenameEnd - filenamePos);
        if (filename.empty())
            filename = "upload_" + Utils::toString(static_cast<int>(time(NULL)));

        size_t bodyStart = body.find("\r\n\r\n", filenameEnd);
        if (bodyStart == std::string::npos)
            return buildError(400, server);
        bodyStart += 4;

        size_t bodyEnd = body.find(boundary, bodyStart);
        if (bodyEnd == std::string::npos)
            bodyEnd = body.size();
        else
            bodyEnd -= 2;

        fileContent = body.substr(bodyStart, bodyEnd - bodyStart);
    }
    else
    {
        filename = "upload_" + Utils::toString(static_cast<int>(time(NULL)));
        fileContent = body;
    }

    for (size_t i = 0; i < filename.size(); i++)
    {
        if (filename[i] == '/' || filename[i] == '\\')
            filename[i] = '_';
    }

    std::string filePath = Utils::joinPath(uploadPath, filename);
    std::ofstream ofs(filePath.c_str(), std::ios::binary);
    if (!ofs.is_open())
        return buildError(500, server); // 500 Internal Server Error
    else
    {
        ofs.write(fileContent.c_str(), fileContent.size());
        ofs.close();
    }

    _headers["Content-Type"] = "text/html";                               
    _headers["Location"] = "/" + filename;
    _body = "<html><body><h1>201 Created</h1><p>File '" + filename +
                       "' uploaded successfully.</p></body></html>";
	_statusCode = 201;
    _buildResponse(); // 201 Created
}

void Response::_handleDelete(const Request& request, const ServerConfig& server, const LocationConfig& location)
{
    std::string path = Router::resolvePath(request.getPath(), location);

    if (!Utils::pathExists(path))
        return buildError(404, server); // 404 not found

    if (Utils::isDirectory(path))
        return buildError(403, server); // 403 forbidden
    //移除
    if (std::remove(path.c_str()) != 0)
        return buildError(500, server); // 500 Internal Server Error

    _headers["Content-Type"] = "text/html";                                  
    _body = "<html><body><h1>200 OK</h1><p>File deleted successfully.</p></body></html>";
	_statusCode = 200;
    _buildResponse();
}

void Response::_serveFile(const std::string&path, const ServerConfig& server)
{
	if (!Utils::pathExists(path))
        return buildError(404, server); // 404 not found

	if (access(path.c_str(), R_OK) != 0)
    	return buildError(403, server); // 403 forbidden

	std::string body;
	if (!Utils::readFile(path, body))
        return buildError(500,server);
	_body = body;
    _headers["Content-Type"] = Utils::getMimeType(path);  
	_statusCode = 200;           
    _buildResponse();
}

void Response::_serveAutoindex(const std::string& path, const std::string& uri)
{

    DIR* dir = opendir(path.c_str());
    if (!dir)
    {
        _headers["Content-Type"] = "text/html";   
		_statusCode = 403;
		_body = Utils::defaultErrorPage(403);                 
        _buildResponse();
        return ;
    }

    // XSS (Cross-Site Scripting)
    std::string safeUri = Utils::htmlEscape(uri);
    _body = "<!DOCTYPE html>\n<html>\n<head><title>INdex of " + safeUri +
                       "</title></head>\n<body>\n<h1>Index of " + safeUri +
                       "</h1>\n<hr>\n<pre>\n";
    
    std::string Uri = uri;
    if (Uri.empty() || Uri[Uri.size() - 1] != '/')
        Uri += '/';
    
    struct dirent* entry;
    std::vector<std::string> entries;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;
        if (name == ".")
            continue;
        entries.push_back(name);
    }
    closedir(dir);

    std::sort(entries.begin(), entries.end());
    for (size_t i = 0; i < entries.size(); i++)
    {
        std::string fullPath = Utils::joinPath(path, entries[i]);
        std::string display = Utils::htmlEscape(entries[i]);
        if (Utils::isDirectory(fullPath))
            display += "/";
        _body += "<a href=\"" + Uri + entries[i];
        if (Utils::isDirectory(fullPath))
            _body += "/";
        _body += "\">" + display + "</a>\n";
    }

    _body += "</pre>\n<hr>\n</body>\n</html>\n";
    _headers["Content-Type"] = "text/html";
	_statusCode = 200;
    _buildResponse();                                  
}

void    Response::setCGIResponse(const std::string& cgiOutput, const ServerConfig& server)
{
    //CGI OUTPUT： HEADER \r\n \r\n BODY or HEADER \n \n BODY \n
    std::string end = "\r\n";
    size_t headerEnd = cgiOutput.find(end + end);
    if (headerEnd == std::string::npos)
    {
        end = "\n";
        headerEnd = cgiOutput.find(end + end);
        if (headerEnd == std::string::npos)
            return buildError(500, server); // 500 Internal Server Error
    }

    std::string cgiHeader = cgiOutput.substr(0, headerEnd);
    std::string cgiBody = cgiOutput.substr(headerEnd + end.size() * 2);
	
	_body = cgiBody;
    _statusCode = 200;
    std::istringstream iss(cgiHeader);
    std::string line;
    while (std::getline(iss, line))
    {
        if (line.empty())
            continue;
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
        size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        
        std::string key = Utils::trimSpace(line.substr(0, colon));
        std::string value = Utils::trimSpace(line.substr(colon + 1));
        if (Utils::toLower(key) == "status")
            _statusCode = Utils::toInt(value);
        else
            _headers[key] = value;
    }
    if (_headers.find("Content-Type") == _headers.end())
        _headers["Content-Type"] = "text/html";
    
    _buildResponse();
}

void Response::addHeader(const std::string& key, const std::string& value)
{
    _headers[key] = value;
}
