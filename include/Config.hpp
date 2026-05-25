#ifndef CONFIG_HPP
# define CONFIG_HPP

# include "Webserv.hpp"

struct LocationConfig
{
    std::string                 path;           // "/" "/upload" "/cg-bin"
    std::string                 root;
    std::string                 index;
    std::set<std::string>       methods;        // set: "GET", "POST", "DELETE"(unique keys, sorted by alphabet)
    bool                        autoindex;
    std::string                 redirect;       // "301 /new"
    int                         redirectCode;
    std::string                 uploadPath;
    std::string                 cgiExtension;   // ".py"
    std::string                 cgiPath;        // "usr/bin/python3"

    LocationConfig();
};

struct ServerConfig
{
    std::string                 host;           // "0.0.0.0"
    int                         port;           // 8080 8081
    std::vector<std::string>    serverNames;
    std::map<int, std::string>  errorPages;     // code - path
    size_t                      clientMaxBody;  // in bytes
    std::vector<LocationConfig> locations;

    ServerConfig();
    const LocationConfig*       findLocation(const std::string& uri) const;
};

class Config
{
    public:
        Config();
        ~Config();

        void                                parse(const std::string& filepath);
        const std::vector<ServerConfig>&    getServers() const;
        const ServerConfig*                 findServer(const std::string& host, int port) const;

    private:
        std::vector<ServerConfig>   _servers;
        std::string                 _content;
        size_t                      _pos;

        void            _removeComments();
        void            _skipWhitespace();
        std::string     _nextToken();
        void            _expectToken(const std::string& expected); //原本返回值是std::string，但在Config.cpp中实现时并没有使用返回值，所以改为void
        void            _parseServer();
        void            _parseListen(ServerConfig& server,const std::string& value);
        void            _parseLocation(ServerConfig& server);
        void            _vlidateConfig();

        Config(const Config&);                      // copy & assign functions, declared but not implemented (prevent copying)
        Config& operator=(const Config&);
};

#endif