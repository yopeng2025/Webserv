#include "Response.hpp"
#include "Router.hpp"
#include "Utils.hpp"


/*
1. 如果有重定向， 构建一个HTTP响应body+状态码，Location头部指向重定向的URL。

build-redirect

HTTP/1.1 302 Found
Location: https://www.google.com
Content-Type: text/html
Content-Length: 115
\r\n
<html>
<body>
<h1>302 Found</h1>
<p>Redirecting to
<a href="https://www.google.com">   //<a> anchor; href hytertext reference
https://www.google.com
</a></p>
</body>
</html>
\r\n

2. 检查HTTP请求方法（GET、POST、DELETE 等）是否被location允许，不允许返回 405 Method Not Allowed

build-response

HTTP/1.1 405 Method Not Allowed
Date: Tue, 24 Jun 2025 18:20:00 GMT
Server: Webserv/1.0
Content-Type: text/html
Content-Length: xxx
Connetction: close
\r\n
<html>
<body>
<h1>405 Method Not Allowed</h1>
</body>
</html>
\r\n

3. 根据HTTP请求方法method和location的配置，构建一个HTTP响应body+状态码

*/

Response::Response(): _ready(false), _statusCode(0), _keepAlive(false), _head(false) {}

Response::~Response() {}

void	Response::reset()
{
	_ready = false;
	_data.clear();
	_headers.clear();
	_body.clear();
	_statusCode = 0;;
	_keepAlive = false;
    _head = false;
}

bool	Response::isReady() const { return (_ready); }

void	Response::setKeepAlive(bool keepAlive)	{ _keepAlive = keepAlive; }

std::string	Response::getData() const { return (_data); }

void	Response::buildError(int code, const ServerConfig& server)
{
	_statusCode = code;
	_keepAlive = false;

	// 查看自定义错误页
	std::map<int, std::string>::const_iterator it = server.errorPages.find(code);
	if (it != server.errorPages.end())
	{
		std::string body;
		if (!Utils::readFile(it->second, body))
			_body = Utils::defaultErrorPage(_statusCode);
		else
			_body = body;
		// _body = Utils::readFile(it->second);
		// if (_body.empty())
		// 	_body = Utils::defaultErrorPage(_statusCode);
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

	// HEAD Method 的body是空的
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
	// 理论上不会到这里，_parseRequestLine 已经过滤
		buildError(501, server);
}

void Response::_handleGet(const Request& request, const ServerConfig& server, const LocationConfig& location)
{
    // GET / 
    // GET /www
    // path = "./www"
    std::string path = Router::resolvePath(request.getPath(), location);
    std::cout << "[debug handleGet] Resolved path: " << path << std::endl;
    
    // 是目录 /www
    if (Utils::isDirectory(path))
    {
        // index.html存在
        if (!location.index.empty())
        {
            // indexPath = "./www/index.html" 
            std::string indexPath = Utils::joinPath(path, location.index);
            
            // 是文件 (检查文件类型是否为常规文件)
            if (Utils::isRegularFile(indexPath))
            {
                _serveFile(indexPath, server);
                return ;
            }
        }
        // autoindex = on/true
        if (location.autoindex)
        {
            _serveAutoindex(path, request.getPath());   // 构建一个自动生成的目录列表页面，返回给客户端
            return ;
        }

        // index.html不存在，也没有开启autoindex / 权限不足禁止访问
        return  buildError(404, server);                        // ❓原本是403 forbidden， 但是如果是目录不存在，返回404 not found更合理
    }

    // 是文件 /www/index.html
    if (Utils::isRegularFile(path))
    {
        _serveFile(path, server);
        return ;
    }

    // 不是现有目录， 也不是现有文件 /abc
    buildError(404, server); // 404 not found
}

//处理 HTTP POST 请求：要么当上传处理，要么当普通 GET 处理
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

/*
POST 请求
获取 content-type
获取 body
获取 上传目录
multipart/form-data？ 解析文件名&文件内容 ： body作为文件
清理文件名
保存到磁盘
201 Created
*/
void Response::_handleUpload(const Request& request, const ServerConfig& server, const LocationConfig& location)
{
    std::string contentType = request.getHeader("Content-Type");
    const std::string& body = request.getBody();
    if (body.empty())
        return buildError(400, server);

    std::string uploadPath = location.uploadPath;
    if (!Utils::isDirectory(uploadPath))
    {
        if (mkdir(uploadPath.c_str(), 0755) != 0) // 创建目录，权限为 rwxr-xr-x
        return buildError(500, server); // 500 Internal Server Error
    }
    
    std::string filename;
    std::string fileContent;
    // POST /upload HTTP/1.1
    // Content-Type: multipart/form-data; boundary=abc123 
    // *boundary是和header的分割线（--abc123）
    if (contentType.find("multipart/form-data") != std::string::npos)
    {
        // 找出boundary分割线
        size_t bpos = contentType.find("boundary=");
        if (bpos == std::string::npos)
            return buildError(400, server);
        std::string boundary = "--" + contentType.substr(bpos + 9); // boundary=abc123 -> --abc123

        // 找出boundary分割线以下的内容
        size_t startPart = body.find(boundary);
        if (startPart == std::string::npos)
            return buildError(400, server);
        startPart += boundary.size() + 2; // 跳过boundary和\r\n

        // --abc123
        // Content-Disposition: form-data;\r\n
        size_t dispPos = body.find("Content-Disposition:", startPart);
        if (dispPos == std::string::npos)
            return buildError(400, server);
        
        // 找出文件名filename
        //--abc123
        //Content-Disposition: form-data; name="file"; filename="test.txt"\r\n
        size_t filenamePos = body.find("filename=\"", dispPos);
        if (filenamePos == std::string::npos)
            return buildError(400, server);
        filenamePos += 10; // 跳过 filename="
        size_t filenameEnd = body.find("\"", filenamePos);
        if (filenameEnd == std::string::npos)
            return buildError(400, server);
        filename = body.substr(filenamePos, filenameEnd - filenamePos);
        if (filename.empty())
            filename = "upload_" + Utils::toString(static_cast<int>(time(NULL)));

        // 找出文件内容content
        // \r\n
        // hello world\r\n
        // --abc123
        size_t bodyStart = body.find("\r\n\r\n", filenameEnd);
        if (bodyStart == std::string::npos)
            return buildError(400, server);
        bodyStart += 4; // 跳过 \r\n\r\n, 指向hello world\r\n

        size_t bodyEnd = body.find(boundary, bodyStart);
        if (bodyEnd == std::string::npos)
            bodyEnd = body.size();  //没有分割线，说明是结尾
        else
            bodyEnd -= 2; // 跳过 \r\n, 指向hello world的d

        fileContent = body.substr(bodyStart, bodyEnd - bodyStart);
    }
    else
    {
        filename = "upload_" + Utils::toString(static_cast<int>(time(NULL)));
        fileContent = body;
    }

    // 清理文件名，防止目录遍历攻击
    for (size_t i = 0; i < filename.size(); i++)
    {
        if (filename[i] == '/' || filename[i] == '\\')                      // ❓需要继续过滤：*?"<>| 吗？
            filename[i] = '_';
    }

    std::string filePath = Utils::joinPath(uploadPath, filename);
    // 以二进制方式打开（覆盖）/创建文件（二进制模式可以保证数据原样写入）
    std::ofstream ofs(filePath.c_str(), std::ios::binary);
    // 检查文件是否成功打开
    if (!ofs.is_open())
        return buildError(500, server); // 500 Internal Server Error
    else
    {
        // 将文件内容写入磁盘
        ofs.write(fileContent.c_str(), fileContent.size());
        ofs.close();
    }

    _headers["Content-Type"] = "text/html";                               
    _headers["Location"] = "/" + filename;       // 允许通过URI访问：http://localhost:8080/test.txt
    _body = "<html><body><h1>201 Created</h1><p>File '" + filename +
                       "' uploaded successfully.</p></body></html>";
	_statusCode = 201;
    _buildResponse(); // 201 Created
}

void Response::_handleDelete(const Request& request, const ServerConfig& server, const LocationConfig& location)
{
    std::string path = Router::resolvePath(request.getPath(), location);

    // 检查文件/文件夹是否存在
    if (!Utils::pathExists(path))
        return buildError(404, server); // 404 not found
    // 检查是否是目录: 不允许删除目录
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

// 构建一个HTTP响应body+状态码，返回给客户端
void Response::_serveFile(const std::string&path, const ServerConfig& server)
{
    // 检查文件是否被移动，替换，删除
    if (!Utils::pathExists(path))
        return buildError(404, server); // 404 not found

    // 检查文件权限
    // R_OK: read permission
    // access success 0 failure -1
	if (access(path.c_str(), R_OK) != 0)
    	return buildError(403, server); // 403 forbidden

	std::string body;
	if (!Utils::readFile(path, body))
        return buildError(500,server);
	_body = body;
    // _body = Utils::readFile(path);
    // if (_body.empty())
        // return buildError(500,server);
    _headers["Content-Type"] = Utils::getMimeType(path);  
	_statusCode = 200;           
    _buildResponse();
}

void Response::_serveAutoindex(const std::string& path, const std::string& uri)
{
    // 打开本地目录
    DIR* dir = opendir(path.c_str());
    // 目录不存在或无法打开
    if (!dir)
    {
        _headers["Content-Type"] = "text/html";   
		_statusCode = 403;
		_body = Utils::defaultErrorPage(403);                 
        _buildResponse();
        return ;
    }

    // XSS (Cross-Site Scripting) 防护
    std::string safeUri = Utils::htmlEscape(uri);
    _body = "<!DOCTYPE html>\n<html>\n<head><title>INdex of " + safeUri +
                       "</title></head>\n<body>\n<h1>Index of " + safeUri +
                       "</h1>\n<hr>\n<pre>\n";
                       // <hr> = Horizontal Rule（水平分隔线）
                       // <pre> = Preformatted Text（预格式化文本） 保留空格和换行符
    
    // 给uri加上斜杠/
    std::string Uri = uri;
    if (Uri.empty() || Uri[Uri.size() - 1] != '/')
        Uri += '/';
    
    // 读取目录内容entry
    // . .. index.html dog.png
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
        std::string fullPath = Utils::joinPath(path, entries[i]); // ./www/dog.png 用于判断是否是目录
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

// 把CGI程序输出的“原始文本”解析成HTTP Response
// Content-Type: text/html\r\n
// Status: 200 OK\r\n
// \r\n
//<html>...</html>\r\n
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
        // 跳过空行
        if (line.empty())
            continue;
        // 去掉行尾的 \r
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
        // if (!line.empty() && line.back() == '\r')
        //     line.pop_back();
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
