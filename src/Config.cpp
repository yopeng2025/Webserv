#include "Webserv.hpp"
#include "Config.hpp"

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
{}

const ServerConfig* Config::findServer(const std::string& host, int port) const
{}

void Config::_removeComments(){}
void Config::_skipWhitespace(){}
std::string Config::_nextToken(){}
std::string Config::_expectToken(const std::string& expected){}
void Config::_parseServer(){}
void Config::_parseLocation(ServerConfig& server){}
void Config::_parseListen(ServerConfig& server,const std::string& value){}
void Config::_vlidateConfig(){}