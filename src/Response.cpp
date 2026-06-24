#include "Response.hpp"
#include "Router.hpp"
#include "Utils.hpp"

Response::Response(): _ready(false) {}

Response::~Response() {}

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

void Response::build(const Request& request, const ServerConfig& server,
                     const LocationConfig& location)
{


    if (request.getMethod() == "GET")
        handleGet(request, server, location);
    else if (request.getMethod() == "POST")
        handlePost(request, server, location);
    else if (request.getMethod() == "DELETE")
        handleDelete(request, server, location);
    else
        buildError(501, server);                //501 = Not Implemented（未实现） 服务器理解客户端发送的请求，但不支持完成该请求所需的功能或方法。
}

std::string Response::_resolvePath(const Request& request, const LocationConfig& location)
{ 
    return Router::resolvePath(request.getPath(), location);
}

void Response::handleGet(const Request& request, const ServerConfig& server, const LocationConfig& location)
{
    // GET / 
    // GET /www
    // path = "./www"
    std::string path = _resolvePath(request, location);

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
            _serveAutoIndex(path, request.getPath());   // 构建一个自动生成的目录列表页面，返回给客户端
            return ;
        }

        // index.html不存在，也没有开启autoindex / 权限不足禁止访问
        buildError(403, server);  // 403 forbidden
        return ;
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

void Response::handlePost(const Request& request, const ServerConfig& server, const LocationConfig& location)
{

}

void Response::handleDelete(const Request& request, const ServerConfig& server, const LocationConfig& location)
{

}

void Response::buildError(int code, const ServerConfig& server)
{

}

// 构建一个HTTP响应body+状态码，返回给客户端
void Response::_serveFile(const std::string&path, const ServerConfig& server)
{
    // 检查文件是否被移动，替换，删除
    if (!Utils::fileExists(path))
    {
        buildError(404, server); // 404 not found
        return ;
    }

    // 检查文件权限
    // R_OK: read permission
    // access success 0 failure -1
    if (access(path.c_str(), R_OK) != 0)
    {
        buildError(403, server); // 403 forbidden
        return ;
    }

    std::string body = Utils::readFile(path);
    _header["Content-Type"] = Utils::getMimeType(path);    //调用 _header!!!
    _buildResponse(200, body); // 200 OK

}

void Response::_serveAutoIndex(const std::string& path, const std::string& uri)
{

}