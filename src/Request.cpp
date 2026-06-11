#include "Request.hpp"

Request::Request(ListenSocket* ls)
	: _maxBodySize(ls->Config->clientMaxBody)
{
}

Request::~Request()
{
}
