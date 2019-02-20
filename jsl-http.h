/*
	jsl-http.h

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



#ifndef JSL_http_H
#define JSL_http_H


#include <esp_err.h>
#include <esp_event_loop.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <lwip/api.h>
#include <lwip/err.h>

#include "jsl-common.h"
#include "jsl-router.h"

class jsl_http
{
public:

	using req_t = jsl_http_common::req_t;
	using res_t = jsl_http_common::res_t;
	using pmap_t = jsl_http_common::pmap_t;
	using target_t = jsl_http_common::target_t;
	using status_t = jsl_http_common::status_t;

	static esp_err_t start(const EventGroupHandle_t _evgr = NULL);
	static void run(void* _ctx);
	static esp_err_t stop();

	static void addRoute(const char* _method, const char* _pattern, target_t _target);

protected:

	class req :
		public req_t
	{
	public:

		req(netconn& _con) : m_con(&_con) { parse(); }
		pmap_t& args() { return m_args; } // non const, needed for router dispatch

	protected:

		err_t parse();

		void parse_head(std::stringstream& _stream);
		void parse_body(std::stringstream& _stream);
		void parse_nval(pmap_t& _map, std::stringstream& _stream, char _split);
		void parse_mpart(std::stringstream& _stream, const std::string& _boundary);

		static std::string url_decode(const std::string& _src);
		static std::string url_encode(const std::string& _src);

		static std::string b64_decode(const std::string& _src);
		static std::string b64_encode(const std::string& _src);

		netconn* m_con;
	};

	class res :
		public res_t
	{
	public:

		res(netconn& _con) : m_con(&_con) {}
		virtual void write(status_t _status);

	protected:

		std::string headers();

		netconn* m_con;
	};

	static void dispatch(req& _request, res& _response);

	static jsl_router m_router;
	static EventGroupHandle_t s_event_group;
};

#endif // #ifndef JSL_http_H
