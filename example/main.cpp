#include "lasote/async_http_request/http_request_call.h"
#include "lasote/json11/json11.hpp"
#include "lasote/httpmodels/request.h"
#include <string.h>
#include <iostream>
#include <stdio.h>

using namespace httpmodels;
using namespace lasote;
using namespace json11;


class HttpJSONRequest : public HttpRequestCall{
	public:
		HttpJSONRequest(int chunk_size=0): HttpRequestCall(chunk_size){}
    //You can also override the following methods
    /*
      virtual void on_body(const char *p, size_t len);
      virtual void on_message_begin();
      virtual void on_url(const char* url, size_t length);
      virtual void on_header_field(const char* name, size_t length);
      virtual void on_header_value(const char* value, size_t length);
      virtual void on_headers_complete(void);
    */
		void on_message_complete(int status){
			 string err;
       //By default, method on_body will append to response_buffer received response body
	 		 auto json = Json::parse(this->response_buffer, err);
	 		 cout << json.dump();
		};
};


int main() {

  Request request;
  Method method("GET",  "https://maps.googleapis.com/maps/api/geocode/json?address=1600+Amphitheatre+Parkway,+Mountain+View,+CA");  
  request.method = &method;
  HttpJSONRequest request_call;
  request_call.send(request);

  HttpJSONRequest request_call2;
  request_call2.send(request);

  //Run libuv loop
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
