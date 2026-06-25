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
            _serveAutoindex(path, request.getPath());   // 构建一个自动生成的目录列表页面，返回给客户端
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

//处理 HTTP POST 请求：要么当上传处理，要么当普通 GET 处理
void Response::handlePost(const Request& request, const ServerConfig& server, const LocationConfig& location)
{
    if (!location.uploadPath.empty())
    {
        handleUpload(request, server, location);
        return ;
    }
    else
        handleGet(request, server, location);
}

/*
POST 请求
获取 content-type
获取 body （body为空->403）
获取 上传目录
multipart/form-data？ 解析文件名&文件内容 ： body作为文件
清理文件名
保存到磁盘
201 Created
*/
void Response::handleUpload(const Request& request, const ServerConfig& server, const LocationConfig& location)
{
    std::string contentType = request.getHeader("Content-Type");
    const std::string& body = request.getBody();
    if (body.empty())
    {
        buildError(400, server);
        return ;
    }

    std::string uploadPath = location.uploadPath;
    if (!Utils::isDirectory(uploadPath))
        mkdir(uploadPath.c_str(), 0755); // 创建目录，权限为 rwxr-xr-x
    
    std::string filename;
    std::string fileContent;
    // POST /upload HTTP/1.1
    // Content-Type: multipart/form-data; boundary=abc123 
    // *boundary是和header的分割线（--abc123）
    if (contentType.find("mltipart/form-data") != std::string::npos)
    {
        size_t bpos = contentType.find("boundary=");
        if (bpos == std::string::npos)
        {
            buildError(400, server);
            return ;
        }
        std::string boundary = "--" + contentType.substr(bpos + 9); // boundary=abc123 -> --abc123

        //找出boundary分割线以下的body内容
        size_t startPart = body.find(boundary);
        if (startPart == std::string::npos)
        {
            buildError(400, server);
            return ;
        }
        startPart += boundary.size() + 2; // 跳过boundary和\r\n

        // --abc123
        // Content-Disposition: form-data; name="username"
        size_t dispPos = body.find("Content-Disposition:", startPart);
        if (dispPos == std::string::npos)
        {
            buildError(400, server);
            return ;
        }
        

    }


    
}

void Response::handleDelete(const Request& request, const ServerConfig& server, const LocationConfig& location)
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
    _header["Content-Type"] = Utils::getMimeType(path);             //调用 _header structure !!!
    _buildResponse(200, body); // 200 OK

}

void Response::_serveAutoindex(const std::string& path, const std::string& uri)
{
    // 打开本地目录
    DIR* dir = opendir(path.c_str());
    // 目录不存在或无法打开
    if (!dir)
    {
        _header["Content-Type"] = "text/html";                      //调用 _header structure !!!
        _buildResponse(403, Utils::defaultErrorPage(403));
        return ;
    }

    // XSS (Cross-Site Scripting) 防护
    std::string safeUri = Utils::htmlEscape(uri);
    std::string body = "<!DOCTYPE html>\n<html>\n<head><title>INdex of " + safeUri +
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
        body += "<a href=\"" + Uri + entries[i];
        if (Utils::isDirectory(fullPath))
            body += "/";
        body += "\">" + display + "</a>\n";
    }

    body += "</pre>\n</hr>\n</body>\n</html>\n";
    _header["Content-Type"] = "text/html";
    _buildResponse(200, body);                                      //调用 _header structure !!!
}