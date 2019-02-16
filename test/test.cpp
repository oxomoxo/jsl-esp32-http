
#include "../jsl-http.h"

bool load_file(const char* _fname, std::ostringstream& _dest)
{
	std::ifstream file(_fname, std::ios::binary);
	if(file.is_open())
	{
		ESP_LOGI(LOGTAG, "File open [%s]",_fname);

		_dest << file.rdbuf();
		file.close();

		return true;
	}

	ESP_LOGE(LOGTAG, "Failed to open [%s] for reading",_fname);

	return false;
}

bool load_file(const char* _fname, std::string& _dest)
{
	std::ostringstream buf;
	if(!load_file(_fname,buf)) return false;
	_dest = buf.str();
	return true;
}

void static_target(const jsl_http_common::req_t& _req, jsl_http_common::res_t& _res)
{
	ESP_LOGI(LOGTAG, "static_target called.");
	std::ostringstream& out = _res;

	std::string fname = "/static";
	if(_req.path().size() > 1)
	{
		fname += "/res";
	}
	fname += "/" + _req.args().at("file");

	ESP_LOGI(LOGTAG, "Opening file : %s",fname.c_str());

	if(!load_file(fname.c_str(),out))
	{
		out << jsl_http_common::dump_path("Path",_req.path());
		out << jsl_http_common::dump_pmap("Args",_req.args());
		out << jsl_http_common::dump_pmap("Query",_req.query());
		_res.write_file("text/plain");
		return;
	}

	_res.write_file(jsl_http_common::mime.at(fname.substr(fname.find('.'))).c_str());
}

void app_main()
{
	jsl_http::addRoute("GET","/{file}",static_target);
	jsl_http::addRoute("GET","/base/{file}",static_target);
	jsl_http::addRoute("GET","/ok/this/is/a/long/{address:\\d+(?:\.\d*)?}/with/some/regexes/{along:\\d+}",test_target);

	jsl_http::start();
}
