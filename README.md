
# jsl-esp32-http
## A lean C++ http server and app router. Handcrafted for esp32. 

The server is based on LWIP and has very small codebase (< 1K loc).

Yet it has a very nice app router that can gather arguments along the url according to possible regexes in the route definition.

### Use

```cpp
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
	jsl_http::addRoute("GET","/res/{file}",static_target);
	jsl_http::addRoute("GET","/ok/this/is/a/long/{address:\\d+(?:\.\d*)?}/with/some/regexes/{along:\\d+}",test_target);

	jsl_http::start();
}
```

The above code snippet
- declares a callback for serving hypothetical static files
- declares two possible routes for static content 
- declare a parametric route with regexes

### How it works

The router works as follows:
- When a route is declared, the router splits the path segments and arranges a tree of routes branches according to plain or regex segments, and stores the callback as the leaf.
- When the server receives a request it splits the url segments, the query arguments and handles the data to the router.
- When the router processes the material form the server it walks the routes branches recursively matching from "most defined" to "least defined" (a matching plain segment is more defined than a matching regex, a longer match is more defined than a shorter match)
    - plain segments: if the incoming segment matches a child name search the branch for a matching leaf, if no leaf is returned test regexes
    - regex segments: if the incoming segment matches a regex search the branch for a matching leaf, if no leaf is returned possibly return the leaf (the actual callback)
    - leaf
    
The regexes have a simple sytax : `{argname:regex}` where the match from the regex (Ecmascript idiom) will be stored in argname.

### Install

```bash
git clone https://github.com/oxomoxo/jsl-esp32-http.git server
```
Or
```bash
git submodule add https://github.com/oxomoxo/jsl-esp32-http.git server
```
In component.mk add the folder to the `COMPONENT_ADD_INCLUDEDIRS` and `COMPONENT_SRCDIRS`

```mk
COMPONENT_ADD_INCLUDEDIRS := . \
	...
	server \

COMPONENT_SRCDIRS := . \
	...
	server \
```

Build :

```bash
make -j4 flash monitor
```

And, Voila !

Neat isn't it ?
