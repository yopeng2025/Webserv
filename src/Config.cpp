#include "Webserv.hpp"
#include "Config.hpp"
#include "Utils.hpp"

// ─── LocationConfig ──────────────────────────────────────────────────────────

LocationConfig::LocationConfig():
    path("/"), root(""), index(""), autoindex(false),
    redirectCode(0), cgiExtension(""), cgiPath("")
{
    methods.insert("GET");
    methods.insert("POST");
    methods.insert("DELETE");
}

// ─── ServerConfig ────────────────────────────────────────────────────────────

ServerConfig::ServerConfig():
    host("0.0.0.0"), port(80), clientMaxBody(1024 * 1024) {}  // default 1MB (= 1024KB; 1KB = 1024 bytes)

// GET /images/logo.png HTTP/1.1
// uri = "/images/logo.png"
const LocationConfig* ServerConfig::findLocation(const std::string& uri) const
{
    const LocationConfig* bestMatch = NULL;
    size_t bestMatchLength = 0;

    for (size_t i = 0; i < locations.size(); i++)
    {
        const std::string& locationPath = locations[i].path;

        // Check if the location path is a prefix of the URI
        // e.g. locationPath = "/" or "/images" or "/images/logo"
        if (Utils::startsWith(uri, locationPath))  
        {
            if (locationPath.length() > bestMatchLength) 
            {
                bestMatch = &locations[i];
                bestMatchLength = locationPath.length(); // update the best match with the longest matching prefix (/images/logo > /images > /)
            }
        }
    }
    return bestMatch;
}

// ─── Config ──────────────────────────────────────────────────────────────────

Config::Config(): _pos(0) {}

Config::~Config() {}

void Config::parse(const std::string& filepath)
{
    std::ifstream ifs(filepath.c_str());                                        // Input File Stream （"config.conf\0"）
    if (!ifs.is_open())
        throw std::runtime_error("Failed to open config file: " + filepath);    // if file cannot be opened, throw an exception

    std::ostringstream oss;                                                     // Output String Stream
    oss << ifs.rdbuf();                                                         // Read the entire file content into the string stream
    _content = oss.str();                                                       // Get the string from the string stream
    _pos = 0;                                                                   // Reset position for parsing
    ifs.close();                                                                // Close the file stream

    _removeComments();                                                          // Remove comments from the content

    while (_pos < _content.size())
    {
        _skipWhitespace();                                                      // Skip any leading whitespace
        if (_pos >= _content.size())
            break;

        std::string token = _nextToken();
        if (token == "server")
            _parseServer();                                                     // Parse a server block
        else if (!token.empty())                                                // If an unexpected token is found (not empty, nor "server"),
            throw std::runtime_error("Unexpected token: '" + token + "'");      // throw an exception
    }

    if (_servers.empty())
        throw std::runtime_error("No server block found in config file");
    
    _vlidateConfig();
}

const std::vector<ServerConfig>& Config::getServers() const
{
    return _servers;
}

const ServerConfig* Config::findServer(const std::string& host, int port) const
{
    const ServerConfig* default_server = NULL;
    for (size_t i = 0; i < _servers.size(); i++)
    {
        if (_servers[i].port == port)
        {
            if (default_server == NULL)
                default_server = &_servers[i];
            for (size_t j = 0; j < _servers[i].serverNames.size(); j++)
            {
                if (_servers[i].serverNames[j] == host)
                    return &_servers[i];
            }
        }
    }
    return default_server;
}

std::string Config::_nextToken()
{
    _skipWhitespace();
    
    if (_pos >= _content.size())
        return "";
    
    // If the current character is a special character(e.g. { ; }), return it as a token
    if (_content[_pos] == '{' || _content[_pos] == '}' || _content[_pos] == ';')
    {
        return std::string(1, _content[_pos++]);  
             //std::string(size_t count, char ch)  Return the single character as a token and move to the next position
    }

    // Otherwise, read until the next whitespace or special character to form a token (e.g. "server", "listen", "location")
    size_t start = _pos;
    while (_pos < _content.size() && \
        _content[_pos] != ' ' && _content[_pos] != '\t' && \
        _content[_pos] != '\n' && _content[_pos] != '\r' && \
        _content[_pos] != '{' && _content[_pos] != '}' && \
        _content[_pos] != ';')
    {
        _pos++;
    }
    return _content.substr(start, _pos - start);
}

void Config::_expectToken(const std::string& expected)  // 返回值改为void, 如果token不匹配就抛出异常，不需要返回token
{
    std::string token = _nextToken();
    if (token != expected)
        throw std::runtime_error("Expected '" + expected + "', but got '" + token + "'");
}

void Config::_parseServer()
{
    _expectToken("{");

    ServerConfig    server;
    std::string     token;

    while (true)
    {
        token = _nextToken();
        if (token == "}")
            break;
        if (token.empty())
            throw std::runtime_error("Unexpected end of config in server block");
        if (token == "listen")
        {
            std::string port = _nextToken();                   
            _parseListen(server, port);
            _expectToken(";");
        }
        else if (token == "server_name")
        {
            while (true)
            {
                std::string name = _nextToken();
                if (name == ";")
                    break;
                server.serverNames.push_back(name);
            }
        }
        else if (token  == "client_max_body_size")
        {
            std::string size = _nextToken();
            size_t      multiplier = 1;
            if (size[size.size() - 1] == 'k' || size[size.size() - 1] == 'K')
            {
                multiplier = 1024;
                size = size.substr(0, size.size() - 1);             // Remove the 'k' or 'K' suffix before converting to number
            }
            else if (size[size.size() - 1] == 'm' || size[size.size() - 1] == 'M')
            {
                multiplier = 1024 * 1024;
                size = size.substr(0, size.size() - 1);
            }
            else if (size[size.size() - 1] == 'g' || size[size.size() - 1] == 'G')
            {
                multiplier = 1024 * 1024 * 1024;
                size = size.substr(0, size.size() - 1);
            }
            else if (size[size.size() - 1] >= '0' && size[size.size() - 1] <= '9')
                multiplier = 1;
            else
                throw std::runtime_error("Invalid client_max_body_size value: " + size);
            size_t sizeNum = Utils::toSizeT(size);
            if (sizeNum > std::numeric_limits<size_t>::max() / multiplier)
                throw std::runtime_error("client_max_body_size value is too large: " + size);
            server.clientMaxBody = sizeNum * multiplier;
            _expectToken(";");
        }
        else if (token == "error_page")
        {
            std::string code = _nextToken();
            std::string path = _nextToken();
            int codeInt = Utils::toInt(code);
            server.errorPages[codeInt] = path;
            _expectToken(";");
        }
        else if (token == "location")
            _parseLocation(server);
        else
            throw std::runtime_error("Unknown directive in server block: '" + token + "'");
    }
    if (server.locations.empty())
    {
        LocationConfig defaultLocation;
        defaultLocation.path = "/";
        defaultLocation.index = "index.html";
        server.locations.push_back(defaultLocation);
    }
    _servers.push_back(server);
}

void Config::_parseListen(ServerConfig& server,const std::string& value)
{
	if (value == ";")
		throw (std::runtime_error("invalid number of arguments in \"listen\" directive"));
    size_t colon = value.rfind(':');            //reverse find； 127.0.0.1:8080   [::1]:8080   localhost:8080   8080
    if (colon != std::string::npos)             // If a colon is found, split the value into host and port
    {
        server.host = value.substr(0, colon);                   // 127.0.0.1
        server.port = Utils::toInt(value.substr(colon + 1));    // 8080
    }
    else                                        // If no colon is found, treat the entire value as the port and use the default host
    {
        bool isNumber = true;
        for (size_t i = 0; i < value.size(); i++)
        {
            if (value[i] < '0' || value[i] > '9')
            {
                isNumber = false;
                break;
            }
        }
        if (isNumber)
            server.port = Utils::toInt(value);    // 8080
        else
            server.host = value;                  // "127.0.0.1" or "localhost"
    }

    if (server.port == 0 || server.port > 65535)
        throw std::runtime_error("Invalid port " + Utils::toString(server.port));
}

void Config::_removeComments()
{
	std::string result;
	result.reserve(_content.size());
	for (size_t i = 0; i < _content.size(); ++i)
	{
		if (_content[i] == '#')													// When meet #, jump until the end of the line(keeping the '\n')
		{
			while (i < _content.size() && _content[i] != '\n')
				++i;
		}
		else
			result += _content[i];
	}
	_content = result;
}

void Config::_skipWhitespace()
{
	while (_pos < _content.size() && std::isspace(static_cast<unsigned char>(_content[_pos])))
		++_pos;
}

void Config::_parseLocation(ServerConfig& server)
{
    LocationConfig location;
    std::string path = _nextToken();
    if (path[0] && path[0] != '/')
        throw std::runtime_error("Unvalid location path: " + path);
    location.path = path;
    _expectToken("{");

    while (true)
    {
        std::string token = _nextToken();
        if (token == "}")
            break;
        if (token.empty())
            throw std::runtime_error("Unexpected end of config in location block");
        else if (token == "root")
        {
            location.root = _nextToken();
            _expectToken(";");
        }
        else if (token == "index")
        {
            location.index = _nextToken();
            _expectToken(";");
        }
        else if (token == "methods" || token == "allow_methods" || token == "limit_except")
        {
            location.methods.clear();
            while (true)
            {
                std::string method = _nextToken();
                if (method == ";")
                    break;
                location.methods.insert(Utils::toUpper(method));
            }
        }
        else if (token == "autoindex")
        {
            std::string value = _nextToken();
            location.autoindex = (value == "on");
            _expectToken(";");
        }
        else if (token == "upload_path")
        {
            location.uploadPath = _nextToken();
            _expectToken(";");
        }
        else if (token == "cgi_ext")
        {
            location.cgiExtension = _nextToken();
            _expectToken(";");
        }
        else if (token == "cgi_path")
        {
            location.cgiPath = _nextToken();
            _expectToken(";");
        }
        else if (token == "return")
        {
            std::string code = _nextToken();
            std::string url = _nextToken();
            int codeInt = Utils::toInt(code);
            if (codeInt < 300 || codeInt >= 400)
                throw std::runtime_error("Invalid redirect code: " + code);
            location.redirectCode = codeInt;
            location.redirect = url;
            _expectToken(";");
        }
        else
            throw std::runtime_error("Unknown directive in location block: '" + token + "'");
    }
    server.locations.push_back(location);
}

void Config::_vlidateConfig()
{
    for (size_t i = 0; i < _servers.size(); i++)
    {
        ServerConfig& prev_server = _servers[i];

        for (size_t j = i + 1; j < _servers.size(); j++)
        {
            ServerConfig& next_server = _servers[j];

            // server1: localhost:8080 server_name=localhost
            // server2: localhost:8080 server_name=empty
            if (prev_server.host == next_server.host && \
                prev_server.port == next_server.port)
            {
                bool hasdifferentnames = false;
                if (prev_server.serverNames.empty() || next_server.serverNames.empty())
                    hasdifferentnames = false;

                else
                {
                    for (size_t k = 0; k < prev_server.serverNames.size(); k++)
                    {
                        for (size_t l = 0; l < next_server.serverNames.size(); l++)
                        {
                            if (prev_server.serverNames[k] != next_server.serverNames[l])
                                hasdifferentnames = true;
                        }
                    }

                if (!hasdifferentnames)
                    LOG_INFO("Duplicate server on " << prev_server.host << ":" << prev_server.port
                            << " (same server_name). Second block will be ignored.");
                }
            }
        }

        for (size_t j = 0; j < prev_server.locations.size(); j++)
        {
            LocationConfig& loc = prev_server.locations[j];
            if (loc.root.empty())
                loc.root = "www";
        }
    }
}