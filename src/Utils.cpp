#include "Webserv.hpp"
#include "Utils.hpp"

bool	Utils::toSizeT(const std::string& str, size_t& n)
{
	if (str.empty() || str[0] == '-')
		return (false);

	size_t tmp;
	char c;
	std::istringstream iss(str);
	if (!(iss >> tmp) || iss >> c)
		return (false);
	n = tmp;
	return (true);
}

bool	Utils::toSizeTHex(const std::string& str, size_t& n)
{
	if (str.empty())
		return (false);

	size_t tmp;
	char c;
	std::istringstream iss(str);
	// 切换成16进制
	// 10 = 1 x 16 + 0 = 16
	iss >> std::hex;
	if (!(iss >> tmp) || iss >> c)
		return (false);
	n =  tmp;
	return (true);
}

int Utils::toInt(const std::string& s) {
	std::istringstream iss(s);
	int n = 0;
	iss >> n;
	return n;
}

std::string     Utils::toString(int n)
{
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

std::string Utils::toUpper(const std::string& str)
{
    if (str.empty())
        return str;
    std::string s = str;
    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] >= 'a' && s[i] <= 'z')
            s[i] = s[i] - ('a' - 'A');
    }
    return s;
}

std::string	Utils::toLower(const std::string& str)
{
	if (str.empty())
		return (str);
	std::string s = str;
	for (size_t i = 0; i < s.size(); i++)
	{
		if (s[i] >= 'A' && s[i] <= 'Z')
			s[i] = s[i] + ('a' - 'A');
	}
	return (s);
}

bool Utils::startsWith(const std::string& str, const std::string& prefix)
{
    if (prefix.size() > str.size())
        return false;
    // compare( size_t pos, size_t len, const string& str) 
    // Compare the substring of length len starting at position pos with the string str
    return (str.compare(0, prefix.size(), prefix) == 0);
}

std::string	Utils::trim(const std::string& str)
{
	size_t start = str.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return ("");
	size_t end = str.find_last_not_of(" \t\r\n");
	return (str.substr(start, end - start + 1));
}

static bool	isxdigit(char c)
{
	return (std::isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

bool	Utils::decodePath(const std::string& str, std::string& out)
{
	// 百分号解码
	// %20 -> ' '
	// %2F -> '/' 以此类推
	out.clear();
	out.reserve(str.size());
	for (size_t i = 0; i < str.size(); ++i)
	{
		if (str[i] == '%')
		{
			if (i + 2 >= str.size())
				return (false);
			char c1 = str[i + 1];
			char c2 = str[i + 2];
			if (!isxdigit(c1) || !isxdigit(c2))
				return (false);
			std::string hex = str.substr(i + 1, 2);
			int value = std::strtol(hex.c_str(), NULL, 16);
			if (value == 0)
				return (false);
			out += static_cast<char>(value);
			i += 2;
		}
		else
			out += str[i];
	}
	return (true);
}

bool	Utils::decodeQuery(const std::string& str, std::string& out)
{
	// 百分号解码
	out.clear();
	out.reserve(str.size());
	for (size_t i = 0; i < str.size(); ++i)
	{
		if (str[i] == '%')
		{
			if (i + 2 >= str.size())
				return (false);
			char c1 = str[i + 1];
			char c2 = str[i + 2];
			if (!isxdigit(c1) || !isxdigit(c2))
				return (false);
			std::string hex = str.substr(i + 1, 2);
			int value = std::strtol(hex.c_str(), NULL, 16);
			//%00 -> '\0'
			if (value == 0)
				return (false);
			out += static_cast<char>(value);
			i += 2;
		}
		// Query+号变空格
		// user=Alice+Green -> user=Alice Green
		else if (str[i] == '+')
			out += ' ';
		else
			out += str[i];
	}
	return (true);
}

// 规范化路径，有效地处理 . 和 ..（回到上一个文件夹），并删除多余的斜杠
std::string Utils::sanitizePath(const std::string& path)
{
	std::vector<std::string> parts;
	std::istringstream iss(path);
	std::string part;
	while (std::getline(iss, part, '/'))
	{
		if(part.empty() || part == ".")
			continue ;
		if (part == "..")
		{
			if (!parts.empty())
				parts.pop_back();
		}
		else
			parts.push_back(part);
	}
	std::string res = "/";
	for (size_t i = 0; i < parts.size(); ++i)
	{
		res += parts[i];
		if (i + 1 < parts.size())
			res += "/";
	}
	return (res);
}

std::string Utils::getDate()
{
	time_t t = time(NULL);
	struct tm* gmt = gmtime(&t);
	char buf[128];
	strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);
	return (std::string(buf));
} 

std::string Utils::joinPath(const std::string& a, const std::string& b)
{
	if (a.empty())
		return b;
	if (b.empty())
		return a;
	if (a[a.size() - 1] == '/' && b[0] == '/')
		return a + b.substr(1);
	if (a[a.size() - 1] != '/' && b[0] != '/')
		return a + "/" + b;
	return a + b;
}

//把整个文件的内容读出来，放进一个 std::string 里返回
std::string Utils::readFile(const std::string& path)
{
	std::ifstream ifs(path.c_str(), std::ios::binary); // 以二进制模式放入文件流，防止换行符被转换/丢失图片数据
	// 检查文件是否成功打开
	if (!ifs.is_open())
		return ("");
	std::ostringstream oss;
	oss << ifs.rdbuf();
	return (oss.str());
}

bool Utils::isDirectory(const std::string& path)
{
	struct stat info;

	// 0 success, -1 error
	if (stat(path.c_str(), &info) != 0)
		return false;
	return (S_ISDIR(info.st_mode));
}

bool Utils::isRegularFile(const std::string& path)
{
	struct stat info;

	if (stat(path.c_str(), &info) != 0)
		return false;
	return (S_ISREG(info.st_mode));
}

// 检查文件或目录是否存在
//原名叫fileExists，但更改为pathExists更准确，因为它可以检查目录和文件是否存在
bool Utils::pathExists(const std::string& path)  
{
	struct stat info;
	return (stat(path.c_str(), &info) == 0);
}

// Multipurpose Internet Mail Extensions
// 表示数据类型(Content Type)
// text/html text/css application/javascript image/png video/mp4 ...
std::string Utils::getMimeType(const std::string& path) {
	std::string ext = getExtension(path);
	if (ext == ".html" || ext == ".htm")	return "text/html";
	if (ext == ".css")						return "text/css";
	if (ext == ".js")						return "application/javascript";
	if (ext == ".json")						return "application/json";
	if (ext == ".xml")						return "application/xml";
	if (ext == ".txt")						return "text/plain";
	if (ext == ".png")						return "image/png";
	if (ext == ".jpg" || ext == ".jpeg")	return "image/jpeg";
	if (ext == ".gif")						return "image/gif";
	if (ext == ".svg")						return "image/svg+xml";
	if (ext == ".ico")						return "image/x-icon";
	if (ext == ".pdf")						return "application/pdf";
	if (ext == ".zip")						return "application/zip";
	if (ext == ".mp3")						return "audio/mpeg";
	if (ext == ".mp4")						return "video/mp4";
	if (ext == ".woff")						return "font/woff";
	if (ext == ".woff2")					return "font/woff2";
	if (ext == ".ttf")						return "font/ttf";
	return "application/octet-stream"; // 应用程序的1个字节流（未知格式），浏览器看到后直接下载
}

std::string Utils::getExtension(const std::string& path)
{
	size_t dot = path.rfind('.');
	if (dot == std::string::npos || dot  == path.size() - 1)
		return "";
	size_t slash = path.rfind('/');
	if (slash != std::string::npos && slash > dot)
		return "";
	return path.substr(dot);
}

std::string Utils::defaultErrorPage(int code)
{
	std::string text = Utils::getStatusText(code);
	std::string codeStr = Utils::toString(code);
	std::string body = "<!DOCTYPE html>\n<html>\n<head><title>" + codeStr + " " + text +
	                   "</title></head>\n<body>\n<center><h1>" + codeStr + " " + text +
					   "</h1></center>\n<hr>\n<center>" + SERVER_NAME + "</center>\n</body>\n</html>\n";
	return (body);
}

std::string Utils::getStatusText(int code)
{
	switch (code)
	{
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 303: return "See Other";
		case 307: return "Temporary Redirect";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 413: return "Payload Too Large";
		case 414: return "URI Too Long";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timeout";
		case 505: return "HTTP Version Not Supported";
		default:  return "Unknown";
	}
}

std::string Utils::htmlEscape(const std::string& str)
{
	std::string escaped;
	escaped.reserve(str.size());
	for (size_t i = 0; i < str.size(); i++)
	{
		switch (str[i])
		{
			case '&':  escaped += "&amp;";  break;
			case '<':  escaped += "&lt;";   break;
			case '>':  escaped += "&gt;";   break;
			case '"':  escaped += "&quot;"; break;
			case '\'': escaped += "&#39;";  break;
			default:   escaped += str[i];   break;
		}
	}
	return escaped;
}

std::string     Utils::trimSpace(const std::string& str)
{
	size_t start = str.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return ("");
	size_t end = str.find_last_not_of(" \t\r\n");
	std::string trimmed = str.substr(start, end - start + 1);
	return trimmed;
}