/*
	jsl-router.h

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



#ifndef JSL_ROUTER_H
#define JSL_ROUTER_H

#include <regex>

#include "jsl-common.h"


class jsl_router
{
public:

	using pmap_t = jsl_http_common::pmap_t;
	using path_t = jsl_http_common::path_t;
	using target_t = jsl_http_common::target_t;

	void addRoute(const char* _method, const char* _pattern, target_t _target);
	target_t dispatch(const std::string& _method, const path_t& _path, pmap_t& _args);

	static void splitPath(const std::string& _pattern, path_t& _path);

protected:

	class branch
	{
	public:

		branch(branch* _parent = NULL) : m_parent(_parent), m_leaf(NULL) {}

		void settle(target_t _target, const path_t& _path, u16_t _pos = 0);
		target_t dispatch(pmap_t& _args, const path_t& _path, u16_t _pos = 0);

	protected:

		branch* m_parent;
		target_t m_leaf;

		void addReg(const std::string& _segt, branch& _child);

		typedef std::pair<std::regex,branch*> regref_t;

		std::map<std::string,branch> m_childs;
		std::map<std::string,regref_t> m_regs;
	};

protected:

	std::map<std::string,branch> m_routes;

};

#endif // #ifndef JSL_ROUTER_H
