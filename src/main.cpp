#include "Webserv.hpp"
#include "Config.hpp"
#include "Server.hpp"

int main(int argc, char** argv) 
{
	std::string configPath = DEFAULT_CONFIG;

	if (argc > 2)
	{
		std::cerr << "Usage: " << argv[0] << " [config_path]" << std::endl;
		return 1;
	}
	if (argc == 2)
		configPath = argv[1];

    try
    {
        Config config;                                                      
        config.parse(configPath);
        LOG_INFO("Configuration loaded successfully from " + configPath);
        
    ////test config
    //    for (size_t i = 0; i < config.getServers().size(); i++)
    //     {
    //         const ServerConfig& server = config.getServers()[i];
    //         LOG_INFO("Server " << i << ": " << server.host << ":" << server.port);
    //         for (size_t j = 0; j < server.serverNames.size(); j++)
    //             LOG_INFO("  Server name: " << server.serverNames[j]);
    //         LOG_INFO("  Client max body size: " << server.clientMaxBody);
    //         for (std::map<int, std::string>::const_iterator it = server.errorPages.begin(); it != server.errorPages.end(); ++it)
    //             LOG_INFO("  Error page: " << it->first << " -> " << it->second);
    //         for (size_t k = 0; k < server.locations.size(); k++)
    //         {
    //             const LocationConfig& location = server.locations[k];
    //             LOG_INFO("  Location " << k << ": " << location.path);
    //             LOG_INFO("    Root: " << location.root);
    //             for (std::set<std::string>::const_iterator it = location.methods.begin(); it != location.methods.end(); ++it)
    //                 LOG_INFO("    Method: " << *it);
    //             LOG_INFO("    Index: " << location.index);
    //             LOG_INFO("    Autoindex: " << (location.autoindex ? "on" : "off"));
    //             if (!location.redirect.empty())
    //                 LOG_INFO("    Redirect: " << location.redirectCode << " -> " << location.redirect);
    //             if (!location.uploadPath.empty())
    //                 LOG_INFO("    Upload path: " << location.uploadPath);
    //             if (!location.cgiExtension.empty())
    //                 LOG_INFO("    CGI extension: " << location.cgiExtension);
    //             if (!location.cgiPath.empty())
    //                 LOG_INFO("    CGI path: " << location.cgiPath);
    //         }
    //     }

        // Server server(config);
        // server.run();
    }
    catch(const std::exception& e)
    {
        LOG_ERROR(e.what());
        return 1;
    }
    
    return 0;
}