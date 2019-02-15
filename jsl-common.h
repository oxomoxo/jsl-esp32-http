
#ifndef JSL_SERVER_COMMON_H
#define JSL_SERVER_COMMON_H

#include <map>
#include <vector>
#include <string>
#include <sstream>

#ifndef LWIP_HDR_ARCH_H
#include <stdint.h>
typedef uint8_t   u8_t;
typedef int8_t    s8_t;
typedef uint16_t  u16_t;
typedef int16_t   s16_t;
typedef uint32_t  u32_t;
typedef int32_t   s32_t;
#endif

class jsl_http_common
{
public:

	typedef std::vector<std::string> path_t;
	typedef std::map<std::string,std::string> pmap_t;

	static std::string dump_path(const char* _name, const jsl_http_common::path_t& _vec)
	{
		std::stringstream out;
		out << _name << " : " << std::endl;
		out << "[" << std::endl;
		for(auto i = _vec.begin(); i != _vec.end(); ++i)
		{
			out << "\t" << *i << std::endl;
		}
		out << "]" << std::endl;
		return out.str();
	}

	static std::string dump_pmap(const char* _name, const jsl_http_common::pmap_t& _map)
	{
		std::stringstream out;
		out << _name << " : " << std::endl;
		out << "{" << std::endl;
		for(auto i = _map.begin(); i != _map.end(); ++i)
		{
			out << "\t" << i->first << ": " << i->second << std::endl;
		}
		out << "}" << std::endl;
		return out.str();
	}

	static bool param(const pmap_t& _pmap, const char* _name, uint32_t& _val)
	{
		if(_pmap.find(_name) != _pmap.end())
		{
			std::stringstream s(_pmap.at(_name)); s >> _val;
			return true;
		}
		return false;
	}

	static bool param(const pmap_t& _pmap, const char* _name, double& _val)
	{
		if(_pmap.find(_name) != _pmap.end())
		{
			std::stringstream s(_pmap.at(_name)); s >> _val;
			return true;
		}
		return false;
	}

	static bool param(const pmap_t& _pmap, const char* _name, bool& _val)
	{
		if(_pmap.find(_name) != _pmap.end())
		{
			std::stringstream s(_pmap.at(_name)); s >> _val;
			return true;
		}
		return false;
	}

	static bool param(const pmap_t& _pmap, const char* _name, std::string& _val)
	{
		if(_pmap.find(_name) != _pmap.end())
		{
			std::stringstream s(_pmap.at(_name)); s >> _val;
			return true;
		}
		return false;
	}


	typedef enum
	{
		STATUS_OK,
		STATUS_MULTIPLE_CHOICES,
		STATUS_BAD_REQUEST,
		STATUS_UNAUTHORIZED,
		STATUS_FORBIDDEN,
		STATUS_NOT_FOUND,
		STATUS_METHOD_NOT_ALLOWED,
		STATUS_REQUEST_URI_TOO_LONG,
		STATUS_UNSUPPORTED_MEDIA_TYPE,
		STATUS_INTERNAL_SERVER_ERROR,
		STATUS_NOT_IMPLEMENTED,
		STATUS_HTTP_VERSION_NOT_SUPPORTED,
		STATUS_MAX
	} status_t;

	typedef struct {
		uint code;
		const char* msg;
	} statinfo_t;

	constexpr static const statinfo_t statcm[STATUS_MAX] {
		{200,"Ok"},
		{300,"Multiple Choices"},
		{400,"Bad Request"},
		{401,"Unauthorized"},
		{403,"Forbidden"},
		{404,"Not Found"},
		{405,"Method Not Allowed"},
		{414,"Request Uri Too Long"},
		{415,"Unsupported Media Type"},
		{500,"Internal Server Error"},
		{501,"Not Implemented"},
		{505,"Http Version Not Supported"}
	};

	typedef struct _req_t // read only capsule
	{
	public:

		const std::string& method() const { return m_method; }
		const std::string& uri() const { return m_uri; }
		const path_t& path() const { return m_path; }
		const pmap_t& args() const { return m_args; }
		const pmap_t& query() const { return m_query; }
		const pmap_t& headers() const { return m_headers; }
		std::string header(const char* _header) const
		{
			auto h = m_headers.find(_header);
			if(h != m_headers.end())
			{
				return h->second;
			}
			return "";
		}

	protected:

		std::string m_method;
		std::string m_uri;
		path_t m_path;
		pmap_t m_args;
		pmap_t m_query;
		pmap_t m_headers;

	} req_t;

	typedef struct _res_t // glorified output buffer
	{
	public:

		operator std::ostringstream& () { return m_out; }

		inline u32_t size()
		{
			m_out.seekp(0, std::ios::end);
			return m_out.tellp();
		}

		std::string header(const char* _header)
		{
			auto h = m_headers.find(_header);
			if(h != m_headers.end())
			{
				return h->second;
			}
			return "";
		}

		void header(const char* _name, const char* _value)
		{
			m_headers[_name] = _value;
		}

		void write_error(status_t _status)
		{
			if(size() > 0) m_headers["Content-type"] = "text/html";
			write(_status);
		}

		void write_file(const char* _type)
		{
			m_headers["Content-type"] = _type;
			write(jsl_http_common::STATUS_OK);
		}

		void write_gzip(const char* _type)
		{
			m_headers["Content-type"] = _type;
			m_headers["Accept-Ranges"] = "bytes";
			m_headers["Content-Encoding"] = "gzip";
			write(jsl_http_common::STATUS_OK);
		}

		void write_json()
		{
			m_headers["Content-type"] = "application/json";
			m_headers["Cache-Control"] = "no-store, no-cache, must-revalidate, max-age=0";
			m_headers["Pragma"] = "no-cache";
			write(jsl_http_common::STATUS_OK);
		}

		void write_cached(const char* _type)
		{
			m_headers["Content-type"] = _type;
			m_headers["Cache-Control"] = "public, max-age=31536000";
			write(jsl_http_common::STATUS_OK);
		}

		virtual void write(status_t _status) = 0;

	protected:

		pmap_t m_headers;
		std::ostringstream m_out;
	} res_t;

	typedef void (*target_t) (const req_t& _req, res_t& _res);

	static const pmap_t mime;
};

#endif // #ifndef JSL_SERVER_COMMON_H

/*


*/