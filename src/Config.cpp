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
    host("0.0.0.0"), port(8080), clientMaxBody(1024 * 1024) {}  // default 1MB (= 1024KB; 1KB = 1024 bytes)

const LocationConfig* ServerConfig::findLocation(const std::string& uri) const
{

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

}

const ServerConfig* Config::findServer(const std::string& host, int port) const
{

}

void Config::_removeComments()
{

}

void Config::_skipWhitespace()
{

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

void Config::_parseLocation(ServerConfig& server){}

void Config::_vlidateConfig(){}