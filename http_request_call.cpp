#include "http_request_call.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace lasote{

	void run_loop(){
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	HttpRequestCall::HttpRequestCall(int chunk_size) : chunk_size(chunk_size), handle_to_close(NULL), message_to_send(NULL){
		parser = new http_parser();
	  	http_parser_init(parser, HTTP_RESPONSE);
	  	settings = new http_parser_settings();
	  	settings->on_message_complete = _on_message_complete_cb;
	  	settings->on_body = _on_body_cb;
	  	settings->on_header_field = _on_header_field_cb;
	    settings->on_header_value = _on_header_value_cb;
	    settings->on_headers_complete = _on_headers_complete_cb;
	    settings->on_message_begin = _on_message_begin_cb;
      	settings->on_url = _on_url_cb;
	}	


	void HttpRequestCall::send(Request& request){

		if(request.method==NULL){
			error("Set a method to the request");
			return;
		}
		debug("SEND REQUEST: " << request.to_string());

		//Fill default arguments if needed
		if ( request.headers.find("Host") == request.headers.end()) {
		   debug("Filling host");
		   std::pair<string,string> hosth("Host", request.method->uri->host);
		   request.headers.insert(hosth); 
		}

		if ( request.headers.find("User-Agent") == request.headers.end()) {
		   debug("Filling user agent");
		   std::pair<string,string> hosth("User-Agent", "lasote/libuv_http_client");
		   request.headers.insert(hosth); 
		}

		if ( request.headers.find("Accept") == request.headers.end()) {
		   debug("Filling Accept");
		   std::pair<string,string> hosth("Accept", "text/html,application/json,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
		   request.headers.insert(hosth); 
		}


		int port = request.method->uri->port;
		string host(request.method->uri->host);
		void* request_data = (void*) this;

		string tmp(request.to_string() + "\r\n");
		int len = tmp.length() + 1;
		this->message_to_send = (char*) malloc(len * sizeof(char));
		strcpy(this->message_to_send, tmp.c_str());

		resolve_dns_and_send_request(host, port, request_data);
	}

	// OVERRIDABLE METHODS
	void HttpRequestCall::on_body(const char *p, size_t len){
		//Just append response to buffer 
		response_buffer.append(p, len);
	}

	void HttpRequestCall::on_message_complete(int status){
		debug("STATUS: " << status << endl);
		debug(response_buffer);
	}

	void HttpRequestCall::on_message_begin(){}
	void HttpRequestCall::on_url(const char* url, size_t length){}
    void HttpRequestCall::on_header_field(const char* name, size_t length){}
    void HttpRequestCall::on_header_value(const char* value, size_t length){}
    void HttpRequestCall::on_headers_complete(void){}


	/* LIBUV STATIC METHODS */

	int resolve_dns_and_send_request(string host_name, int port, void* request_data){
		struct addrinfo hints;
	    hints.ai_family = PF_INET;
	    hints.ai_socktype = SOCK_STREAM;
	    hints.ai_protocol = IPPROTO_TCP;
	    hints.ai_flags = 0;

		uv_getaddrinfo_t* resolver = (uv_getaddrinfo_t*) malloc(sizeof(uv_getaddrinfo_t));
	    resolver->data = request_data;
	    char str_port[10];

	    sprintf(str_port, "%d", port);
	    debug("GET ADDRESS: " << host_name.c_str() << " PORT: " << str_port);
    	int r = uv_getaddrinfo(uv_default_loop(), resolver, on_dns_resolved, host_name.c_str(), str_port, &hints);
	    return r;
	}


	void on_dns_resolved(uv_getaddrinfo_t *resolver, int status, struct addrinfo *res) {
	    
	    if (status < 0) {
	       error("Getaddrinfo callback error: " << status);
	       return;
	    }
	    char addr[17] = {'\0'};
	    uv_ip4_name((struct sockaddr_in*) res->ai_addr, addr, 16);
	    debug("Resolved!");
		debug(addr);

		void* data = resolver->data;
		send_request_to_ip4((const struct sockaddr*) res->ai_addr, data);	    
		free(resolver);
	}

	void send_request_to_ip4(const struct sockaddr* addr, void* request_data){
		//TODO Free client and connect_req. Where?
		uv_tcp_t* client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
		uv_tcp_init(uv_default_loop(), client);
		uv_connect_t* connect_req = (uv_connect_t*) malloc(sizeof(uv_connect_t));	
		//Pass data object	
		connect_req->data = request_data;
		uv_tcp_connect(connect_req, client, addr, on_connect);		 
	} 

	void on_connect(uv_connect_t *connection, int status) {

		HttpRequestCall* client = static_cast<HttpRequestCall*>(connection->data);

	    if (status == -1) {
	    	fprintf(stderr, "error on_connect");
	        return;
	    }
    	debug("Connected!");

	    uv_buf_t buf = uv_buf_init(client->message_to_send, strlen(client->message_to_send));
		uv_stream_t* tcp = connection->handle;
	    write_req_t* wr = (write_req_t*) malloc(sizeof(write_req_t));
	    wr->buf = buf;
	    //Pass our http_parser again
	    wr->req.data = (void*) client;

	 	int buf_count = 1;
	 	debug("Writing...");
	 	debug(client->message_to_send);
	    uv_write(&wr->req, tcp, &wr->buf, buf_count, after_write);
	}

	void after_write(uv_write_t* req, int status) {	  
	  debug("after write");
	  debug(req->handle);
	  write_req_t* wr;

	  /* Free the read/write buffer and the request */
	  wr = (write_req_t*) req;
	  free(wr->buf.base);

	  //Pass http_parser one more time
	  req->handle->data = req->data;
	  
	  if (status == 0) {    
	    uv_read_start(req->handle, alloc_buffer, receive_response);
	    debug("Write ok!");
	  }
	  else{
	  	debug("write error");
	    fprintf(stderr, "uv_write error: %s - %s\n", uv_err_name(status), uv_strerror(status));
	    if (!uv_is_closing((uv_handle_t*) req->handle)){
	    	uv_close((uv_handle_t*) req->handle, on_close);
	    }
	  }
	}

	void receive_response(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
		debug("RECEIVE RESPONSE");
		debug(handle);
		HttpRequestCall* client = static_cast<HttpRequestCall*>(handle->data);
		debug(nread);

		if (nread < 0) {
		  /* Error or EOF */
		  if (buf->base)  free(buf->base);		
		  uv_close((uv_handle_t*) handle, on_close);
		  return;
		}

		if (nread == 0) {
		  /* Everything OK, nothing read (all readed now). */
		  if (buf->base) free(buf->base);
	      uv_close((uv_handle_t*) handle, NULL);
		  return;
		}
		client->parser->data = client;
		client->handle_to_close = (uv_handle_t*) handle;
		size_t nparsed = http_parser_execute(client->parser, client->settings, buf->base, nread);
		if (buf->base) free(buf->base);
		return;
	}

	void on_close(uv_handle_t* peer) {
	  free(peer);
	  debug("Closed ok!");
	}

	void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {  
	  HttpRequestCall* client = static_cast<HttpRequestCall*>(handle->data);
	  size_t size = suggested_size;
	  if(client->chunk_size > 0){
	  	size = client->chunk_size;
	  }
	  debug("ALLOC: " << size);
	  buf->base = (char*) malloc(size);
	  buf->len = size;
	}


	/* PARSER CALLBACKS */

	int _on_body_cb (http_parser *parser, const char *p, size_t len){
	  HttpRequestCall* client = static_cast<HttpRequestCall*>(parser->data);
	  client->on_body(p, len);
	  return 0;
	}

	int _on_message_complete_cb(http_parser* parser) {
	  HttpRequestCall* client = static_cast<HttpRequestCall*>(parser->data);
	  debug("ON MESSAGE COMPLETE INTERNAL!");
	  
	  if(client->handle_to_close){
	  	uv_close(client->handle_to_close, NULL);
	  	client->handle_to_close = NULL;
	  }
	  client->on_message_complete(parser->status_code);
      return 0;
	}

	int _on_header_field_cb(http_parser* parser, const char* header, size_t len){
		HttpRequestCall* client = static_cast<HttpRequestCall*>(parser->data);
		client->on_header_field(header, len);
		return 0;
	}

	int _on_header_value_cb(http_parser* parser, const char* header_value, size_t len){
		HttpRequestCall* client = static_cast<HttpRequestCall*>(parser->data);
		client->on_header_value(header_value, len);
		return 0;
	}

	int _on_headers_complete_cb(http_parser* parser){
		HttpRequestCall* client = static_cast<HttpRequestCall*>(parser->data);
		client->on_headers_complete();
		return 0;
	}

	int _on_message_begin_cb(http_parser* parser){
		HttpRequestCall* client = static_cast<HttpRequestCall*>(parser->data);
		client->on_message_begin();
		return 0;
	}

    int _on_url_cb(http_parser* parser, const char* url, size_t len){
    	HttpRequestCall* client = static_cast<HttpRequestCall*>(parser->data);
    	client->on_url(url, len);
    	return 0;
    }

	
		

}
