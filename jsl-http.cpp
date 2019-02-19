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



#include <esp_wifi.h>
#include <esp_log.h>

#include "jsl-http.h"

#define SERVER_LOGTAG "SERVER :"
#include <esp_log.h>

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
	if(s_event_group != NULL)
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

		// timeval tv1;
		// gettimeofday(&tv1,NULL);

		if (ret == ERR_OK && newconn != NULL)
		{
			// ESP_LOGI(SERVER_LOGTAG,"HTTP Incoming request 0x%x",(uint)newconn);

			// uint32_t tcks = xthal_get_ccount(); //xTaskGetTickCount

			req request(*newconn);
			res response(*newconn);

			dispatch(request,response);

			netconn_close(newconn);
			netconn_delete(newconn);

			// ESP_LOGI(SERVER_LOGTAG,"HTTP Server timing [%8d]",(uint)xthal_get_ccount() - tcks);
		}

		// timeval tv2;
		// gettimeofday(&tv2,NULL);

		// int32_t sec = tv2.tv_sec - tv1.tv_sec;
		// int32_t usec = tv2.tv_usec - tv1.tv_usec;

		// ESP_LOGV(SERVER_LOGTAG,"HTTP Server timing [%08d:%06d]",sec,usec);

		vTaskDelay( (TickType_t)1); /* breathe */
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

	if(target == NULL)
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

	netbuf *inbuf = NULL;
	ret = netconn_recv(m_con, &inbuf);
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

	// Parse URI and headers

	std::string line;
	size_t p1 = 0, p2 = 0;
	while(std::getline(stream, line, '\n'))
	{
		if(m_uri == "") // first line => METHOD + URI
		{
			p1 = line.find(' ');
			p2 = line.rfind(' ');

			m_method = line.substr(0,p1);
			m_uri = line.substr(p1 + 1,p2 - (p1 + 1));
		}
		else // headers
		{
			p1 = line.find(':');
			if(p1 != std::string::npos)
			{
				m_headers[line.substr(0,p1)] = line.substr(p1 + 2,line.size() - (p1 + 3));
			}
		}
	}

	p1 = m_uri.find('?');
	p2 = m_uri.find('#');

	jsl_router::splitPath(m_uri.substr(0,p1),m_path);

	stream.clear();
	stream.str(m_uri.substr(p1 + 1,p2 - p1));
	while(std::getline(stream, line, '&'))
	{
		p1 = line.find('=');
		if(p1 != std::string::npos)
		{
			m_query[line.substr(0,p1)] = line.substr(p1 + 1,line.size() - (p1 + 1));
		}
	}

	return ERR_OK;
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
	netconn_write(m_con, headr.str().c_str(), hlength, NETCONN_COPY );
	netconn_write(m_con, m_out.str().c_str(), clength, NETCONN_COPY );
}
