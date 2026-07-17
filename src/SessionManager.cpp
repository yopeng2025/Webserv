#include "SessionManager.hpp"

SessionManager::SessionManager() { std::srand(std::time(NULL)); }

SessionManager::~SessionManager() {}

std::string	SessionManager::generateId()
{
	std::ostringstream oss;
	oss << std::hex << time(NULL);
	for (int i = 0; i < 4; ++i)
		oss << std::hex << std::rand();
	return (oss.str());
}

std::string	SessionManager::createSession()
{
	std::string id = generateId();
	while(_sessions.find(id) != _sessions.end())
		id = generateId();
	_sessions[id] = Session();
	_sessions[id].visitCount++;

	return (id);
}

Session*	SessionManager::getSession(const std::string& id)
{
	if (id.empty())
		return (NULL);
	std::map<std::string, Session>::iterator it =  _sessions.find(id);
	if (it == _sessions.end())
		return (NULL);
	it->second.lastAccess = std::time(NULL);
	return (&it->second);
}

// void		SessionManager::destroySession(const std::string& id)
// {
// 	_sessions.erase(id);
// }

void		SessionManager::cleanExpiredSession()
{
	time_t now = std::time(NULL);
	std::map<std::string, Session>::iterator it =  _sessions.begin();
	while (it != _sessions.end())
	{

		if (now - it->second.lastAccess > SESSION_TIMEOUT)
			_sessions.erase(it++);
		else
			++it;
	}
}