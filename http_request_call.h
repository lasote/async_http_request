#include "lasote/simple_logger/logger.h"
#include "lasote/http_parser/http_parser.h"
#include "lasote/httpmodels/request.h"
#include "lasote/libuv/include/uv.h"
#include <iostream>

using namespace httpmodels;

namespace lasote{

    typedef struct {
    	uv_write_t req;
    	uv_buf_t buf;
    } write_req_t;

	class HttpRequestCall{

		public:
			HttpRequestCall(int chunk_size=0);
			void send(Request& request);

			virtual void on_body(const char *p, size_t len);
			virtual void on_message_complete(int status);
			virtual void on_message_begin();
            virtual void on_url(const char* url, size_t length);
            virtual void on_header_field(const char* name, size_t length);
            virtual void on_header_value(const char* value, size_t length);
            virtual void on_headers_complete(void);
     			
			int chunk_size;
			string response_buffer;
			http_parser* parser;
			http_parser_settings* settings;
			uv_handle_t* handle_to_close;
			char* message_to_send;

	};

	//Static methods from libuv
	void send_request_to_ip4(const struct sockaddr* addr, void* request_data);
	void on_dns_resolved(uv_getaddrinfo_t *resolver, int status, struct addrinfo *res);
	int resolve_dns_and_send_request(string host_name, int port, void* request_data);
	void on_connect(uv_connect_t *connection, int status);
	void after_write(uv_write_t* req, int status);
	void receive_response(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf);
	void on_close(uv_handle_t* peer);
	void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
	
	//Parser callbacks
	int _on_message_complete_cb(http_parser*);
	int _on_body_cb(http_parser*, const char*, size_t);
	int _on_header_field_cb(http_parser*, const char*, size_t);
	int _on_header_value_cb(http_parser*, const char*, size_t);
	int _on_headers_complete_cb(http_parser*);
	int _on_message_begin_cb(http_parser*) ;
    int _on_url_cb(http_parser*, const char*, size_t);

}
