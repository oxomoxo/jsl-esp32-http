/*
	jsl-router.cpp

	This scource file is part of the jsl-esp32 project.

	Author: Lorenzo Pastrana
	Copyright © 2019 Lorenzo Pastrana

	This program is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the
	Free Software Foundation, either version 3 of the License, or (at your
	option) any later version.

	This program is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
	or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
	for more details.

	You should have received a copy of the GNU General Public License along
	with this program. If not, see http://www.gnu.org/licenses/.

*/


#include <iostream>

#include "server/jsl-router.h"

// #define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#define ROUTER_LOGTAG "ROUTER :"
#include <esp_log.h>


void jsl_router::addRoute(const char* _method, const char* _pattern, target_t _target)
{
	std::string method, m(_method);

	std::locale loc;
	for(auto elem : m)
	{
		method += std::tolower(elem,loc);
	}

	path_t path;
	jsl_str::splitv(path,_pattern,'/');

	ESP_LOGI(ROUTER_LOGTAG,"Adding route : [%s] => %s",method.c_str(),_pattern);
	m_routes[method].settle(_target,path);
}

jsl_router::target_t jsl_router::dispatch(const std::string& _method, const path_t& _path, pmap_t& _args)
{
	std::string method, m(_method);

	std::locale loc;
	for(auto elem : m)
	{
		method += std::tolower(elem,loc);
	}

	if(m_routes.find(method) != m_routes.end())
	{
		// ESP_LOGI(ROUTER_LOGTAG,"Dispatching URI [%s]",method.c_str());
		return m_routes[method].dispatch(_args,_path);
	}

	ESP_LOGE(ROUTER_LOGTAG,"Method Not Supported [%s]",method.c_str());
	return NULL;
}

void jsl_router::branch::settle(target_t _target, const path_t& _path, u16_t _pos)
{
	if(_pos >= _path.size()) // Leaf !
	{
		// ESP_LOGI(ROUTER_LOGTAG,"Settele - Leaf Attained");
		m_leaf = _target;
		return;
	}

	std::string segt = _path[_pos++];

	// ESP_LOGD(ROUTER_LOGTAG,"Settle - segment is : %s",segt.c_str());

	if(m_childs.find(segt) == m_childs.end())
	{
		// ESP_LOGD(ROUTER_LOGTAG,"Settele - Creating branch");
		m_childs[segt] = branch(this);
		addReg(segt,m_childs[segt]);
	}

	// ESP_LOGD(ROUTER_LOGTAG,"Settle - Diving branch");
	m_childs[segt].settle(_target, _path, _pos);
	// ESP_LOGV(ROUTER_LOGTAG,"Settle - Popping branch");
}

jsl_router::target_t jsl_router::branch::dispatch(pmap_t& _args, const path_t& _path, u16_t _pos)
{
	if((_path.size() - _pos) < 1) // early out no dive
	{
		ESP_LOGD(ROUTER_LOGTAG,"Dispatch - End of path (%d)",_pos);
		return m_leaf; // return possible match
	}

	std::string segt = _path[_pos++];

	ESP_LOGD(ROUTER_LOGTAG,"Dispatch - segment is : %s",segt.c_str());

	if(m_childs.find(segt) != m_childs.end())
	{
		ESP_LOGD(ROUTER_LOGTAG,"Dispatch - Diving branch");
		target_t ret = m_childs[segt].dispatch(_args,_path,_pos);
		ESP_LOGV(ROUTER_LOGTAG,"Dispatch - Popping branch");
		if(ret != NULL)
		{
			return ret;
		}
		// else continue to regexes
	}

	if(m_regs.size())
	{
		for(auto i = m_regs.begin(); i != m_regs.end(); ++i)
		{
			ESP_LOGD(ROUTER_LOGTAG,"Dispatch - Testing regex : %s",i->first.c_str());
			std::smatch m;
			if(std::regex_match(segt,m,i->second.first))
			{
				ESP_LOGD(ROUTER_LOGTAG,"Dispatch - Regex MATCH");
				_args[i->first] = m[0];
				target_t ret = i->second.second->dispatch(_args,_path,_pos);
				if(ret != NULL)
				{
					return ret;
				}
				// else continue to m_leaf
			}
		}
	}

	ESP_LOGD(ROUTER_LOGTAG,"Dispatch - return possible match");
	return m_leaf; // return possible match
}

void jsl_router::branch::addReg(const std::string& _segt, jsl_router::branch& _child)
{
	size_t o = 0, p = 0, c = 0;

	o = _segt.find('{');

	if(o == std::string::npos)
	{
		// ESP_LOGE(ROUTER_LOGTAG,"Settle - No Regex");
		return; // No RegEx
	}

	p = _segt.find(':');
	c = _segt.find('}');

	std::string pname,regex;

	if(p > o && p < c) // name and regex
	{
		pname = _segt.substr(o + 1,p - (o + 1));
		regex = _segt.substr(p + 1,c - (p + 1));
	}
	else if(c > o) // name but no regex
	{
		pname = _segt.substr(o + 1,c - (o + 1));
		regex = ".*"; // match all
	}
	else
	{
		// ESP_LOGE(ROUTER_LOGTAG,"Settle - Ill Formed Regex");
		return; // Ill formed
	}

	// ESP_LOGD(ROUTER_LOGTAG,"Settle - Regex Stored : %s",regex.c_str());
	m_regs[pname] = regref_t(
		std::regex(regex, std::regex_constants::ECMAScript),
		&_child
	);
}
