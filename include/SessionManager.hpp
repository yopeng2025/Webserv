#ifndef SESSIONMANAGER_HPP
#define SESSIONMANAGER_HPP

#include "Webserv.hpp"

struct Session
{
	int				visitCount;
	std::string 	userName;
	time_t			lastAccess;

	Session(): visitCount(0), userName(""), lastAccess(time(NULL)) {}
};

class SessionManager
{
	private:
		SessionManager(const SessionManager&);
		SessionManager& operator=(const SessionManager&);

		std::map<std::string, Session>	_sessions;
		
	public:
		SessionManager();
		~SessionManager();

		std::string	generateId();
		std::string	createSession();
		Session*	getSession(const std::string& id);
		void		destroySession(const std::string& id);
		void		cleanExpired();
};



#endif