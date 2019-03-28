/*
	jsl-http.cpp

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

#define LOG_LOCAL_LEVEL ESP_LOG_NONE
// #define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
constexpr char SERVER_LOGTAG[] = "HTTP :";
#include <esp_log.h>

#include "utils/jsl-str.h"
#include "jsl-http.h"

EventGroupHandle_t jsl_http::s_event_group;
jsl_router jsl_http::m_router;

esp_err_t jsl_http::start(const EventGroupHandle_t _evgr)
{
	s_event_group = _evgr;
	return ESP_OK;
}



void jsl_http::run(void* _ctx)
{
	ESP_LOGI(SERVER_LOGTAG, "Server Task Executing on core %d\n", xPortGetCoreID());

	// This one is necessary if wifi is in station mode
	if(s_event_group != nullptr)
	{
		xEventGroupWaitBits(s_event_group, 0x01, pdFALSE, pdTRUE, portMAX_DELAY );
	}

	err_t ret;

	netconn *conn;
	netconn *newconn;

	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn, IP_ADDR_ANY, 80);
	netconn_listen(conn);

	ESP_LOGI(SERVER_LOGTAG,"HTTP Server listening...");

	netconn_set_nonblocking(conn,1);

	do
	{
		ret = netconn_accept(conn, &newconn);
		if (ret == ERR_OK && newconn != nullptr)
		{
			req request(*newconn);
			res response(*newconn);

			dispatch(request,response);

			netconn_close(newconn);
			netconn_delete(newconn);
		}

		vTaskDelay(pdMS_TO_TICKS(1)); /* breathe */
	}
	while(ret == ERR_OK);

	netconn_close(conn);
	netconn_delete(conn);
}

esp_err_t jsl_http::stop()
{
	return ESP_OK;
}

void jsl_http::addRoute(const char* _method, const char* _pattern, jsl_router::target_t _target)
{
	m_router.addRoute(_method, _pattern, _target);
}

void jsl_http::dispatch(req& _request, res& _response)
{
	ESP_LOGI(SERVER_LOGTAG,"[%s] Dispatch URI [%s]",_request.method().c_str(),_request.uri().c_str());
	jsl_router::target_t target = m_router.dispatch(_request.method(),_request.path(),_request.args());

	if(target == nullptr)
	{
		ESP_LOGW(SERVER_LOGTAG,"[%s] Target NOT FOUND",_request.method().c_str());
		_response.write_error(jsl_http_common::STATUS_NOT_FOUND);
		return;
	}

	ESP_LOGD(SERVER_LOGTAG,"Dispatch - Executing target");
	target(_request,_response);
}



err_t jsl_http::req::parse()
{
	err_t ret;

	// Pull request data

	netbuf *inbuf = nullptr;
	ret = netconn_recv(m_conn, &inbuf);
	if (ret != ERR_OK) return ret;

	std::stringstream stream;
	do
	{
		char *bufptr;
		u16_t buflen;
		netbuf_data(inbuf, (void**)&bufptr, &buflen);

		char t = bufptr[buflen-1];
		bufptr[buflen-1] = '\0';
		stream << bufptr;
		bufptr[buflen-1] = t;
	}
	while (netbuf_next(inbuf) == 0);

	netbuf_delete(inbuf);

	ESP_LOGI(SERVER_LOGTAG,"%s",stream.str().c_str());

	// Parse URI and headers

	parse_head(stream);

	// Parse request body

	parse_body(stream);

	// Parse path

	size_t p1 = 0, p2 = 0;

	p1 = m_uri.find('?');
	p2 = m_uri.find('#');

	jsl_str::splitv(m_path,m_uri.substr(0,p1),'/');

	stream.clear();
	stream.str(m_uri.substr(p1 + 1,p2 - p1));

	// Parse url encoded query string
	parse_nval(m_query,stream,'&','=');
	for(auto i : m_query) i.second = jsl_str::url_decode(i.second);

	return ERR_OK;
}

void jsl_http::req::parse_head(std::stringstream& _stream)
{
	std::string line, name, val;
	while(std::getline(_stream, line, '\n'))
	{
		line.pop_back(); // remove '\r'

		// ESP_LOGI(SERVER_LOGTAG,"Parse Headers Line [%s]",jsl_str::escape(line).c_str());

		if(line.size() == 0) // body start
		{
			break;
		}
		if(m_uri == "") // first line => METHOD + URI + HTTP/?
		{
			size_t p1 = 0, p2 = 0;

			p1 = line.find(' ');
			p2 = line.rfind(' ');

			m_method = line.substr(0,p1);
			m_uri = line.substr(p1 + 1,p2 - (p1 + 1));
		}
		else // headers
		{
			if(jsl_str::split(line,':',name,val))
			{
				m_headers[name] = jsl_str::trim(val);
			}
		}
	}
}

void jsl_http::req::parse_body(std::stringstream& _stream)
{
	std::string ctype = header("Content-Type");

	// ESP_LOGI(SERVER_LOGTAG,"Parse Request BODY [%s]",ctype.c_str());

	if(ctype == "application/x-www-form-urlencoded")
	{
		// ESP_LOGD(SERVER_LOGTAG,"Urlencoded");
		// Parse url encoded request body
		parse_nval(m_form,_stream,'&','=');
		for(auto i : m_form) i.second = jsl_str::url_decode(i.second);
	}
	else
	{
		// ESP_LOGD(SERVER_LOGTAG,"Not urlencoded [%s]\n%s",ctype.c_str(),_stream.str().c_str());

		std::string bound;
		if(jsl_str::split(ctype,';',ctype,bound) && ctype == "multipart/form-data")
		{
			// ESP_LOGV(SERVER_LOGTAG,"Found multipart");
			ctype = bound;
			jsl_str::split(ctype,'=',ctype,bound);

			bound = jsl_str::trim(bound,' ');
			bound = jsl_str::trim(bound,'"');
			// ESP_LOGV(SERVER_LOGTAG,"Found multipart bound [%s]",bound.c_str());

			parse_mpart(_stream,bound);
		}
	}
}

void jsl_http::req::parse_nval(pmap_t& _map, std::stringstream& _stream, char _c, char _e)
{
	std::string line, name, val;
	while(std::getline(_stream, line, _c))
	{
		if(jsl_str::split(line,_e,name,val))
		{
			name = jsl_str::trim(name,' ');
			name = jsl_str::trim(name,'"');

			val = jsl_str::trim(val,' ');
			val = jsl_str::trim(val,'"');

			_map[name] = val;
		}
		else
		{
			line = jsl_str::trim(line,' ');
			line = jsl_str::trim(line,'"');

			_map[""] = line;
		}
	}
}

void jsl_http::req::parse_mpart(std::stringstream& _stream, std::string _boundary)
{
	// ESP_LOGI(SERVER_LOGTAG,"Parse multipart [%s]", _boundary.c_str());

	_boundary = "--" + _boundary; // bake-in boundary prefix

	pmap_t headers;
	bool data = false;
	std::string line, pname, fname, name, val;
	while(std::getline(_stream, line, '\n'))
	{
		line.pop_back(); // remove '\r'

		// ESP_LOGV(SERVER_LOGTAG,"Multipart raw line [%s]", line.c_str());

		if(jsl_str::trim(line) == _boundary + "--") // end of form
		{
			// ESP_LOGV(SERVER_LOGTAG,"Multipart END");
			break;
		}

		if(jsl_str::trim(line) == _boundary) // new field
		{
			// ESP_LOGV(SERVER_LOGTAG,"Multipart boundary [%s]", _boundary.c_str());

			headers.clear();
			data = false;
			pname = "";
			fname = "";

			continue;
		}

		if(data == false && line.size() == 0) // end of part header
		{
			// ESP_LOGV(SERVER_LOGTAG,"Multipart data start");
			std::cout << jsl_http_common::dump_pmap("Multipart",headers);

			data = true;

			continue;
		}

		if(data == false && jsl_str::split(line,':',name,val))
		{
			// ESP_LOGV(SERVER_LOGTAG,"Multipart header line\n\t%s",line.c_str());

			name = jsl_str::trim(name,' ');
			name = jsl_str::trim(name,'"');

			val = jsl_str::trim(val,' ');
			val = jsl_str::trim(val,'"');

			headers[name] = val;

			if(
				name == "Content-Disposition"
			){
				// parse directives

				pmap_t pval;
				std::stringstream sval(val);
				parse_nval(pval,sval,';','=');

				pname = pval["name"];
				fname = pval["filename"];

				if(pval.find("filename*") != pval.end())
				{
					fname = pval["filename*"];
				}
			}

			continue;
		}

		if(data == true)
		{
			// ESP_LOGV(SERVER_LOGTAG,"Multipart form line [%s]\n\t%s",pname.c_str(),line.c_str());
			m_form[pname] += line;
		}
	}
}

std::string jsl_http::res::headers()
{
	std::string ret;
	for(auto i = m_headers.begin(); i != m_headers.end(); ++i)
	{
		ret += i->first + ": " + i->second + "\n";
	}
	return ret;
}

void jsl_http::res::write(status_t _status)
{
	if(_status >= jsl_http_common::STATUS_MAX) return; // invalid status

	u32_t clength;
	std::ostringstream headr;

	// Compute Content-Length
	headr << (clength = size());
	m_headers["Content-Length"] = headr.str();
	// Reset stream
	headr.str(std::string());
	headr.clear();
	// Build response headers
	jsl_http_common::statinfo_t status = jsl_http_common::statcm[_status];
	headr << "HTTP/1.1 " << status.code << " " << status.msg << "\n" << headers() << "\n";
	// Compute stream length
	headr.seekp(0, std::ios::end);
	u32_t hlength = headr.tellp();
	// Flush to netconn
	netconn_write(m_conn, headr.str().c_str(), hlength, NETCONN_COPY );
	netconn_write(m_conn, m_out.str().c_str(), clength, NETCONN_COPY );
}
