/*
 * AweMUD NG - Next Generation AwesomePlay MUD
 * Copyright (C) 2000-2005  AwesomePlay Productions, Inc.
 * See the file COPYING for license details
 * http://www.awemud.net
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdarg.h>

#include "common/error.h"
#include "mud/server.h"
#include "mud/network.h"
#include "mud/parse.h"
#include "common/streams.h"
#include "mud/http.h"
#include "mud/settings.h"
#include "common/log.h"
#include "mud/fileobj.h"

SHTTPPageManager HTTPPageManager;

SCRIPT_TYPE(HTTP);
HTTPHandler::HTTPHandler (int s_sock, const SockStorage& s_netaddr) : Scriptix::Native(AweMUD_HTTPType), SocketUser(s_sock)
{
	addr = s_netaddr;
	state = REQ;
}

// disconnect
void
HTTPHandler::disconnect ()
{
	// don't log this; FIXME: we should ahve one log line per request, noting usrl/user/etc
	// Log::Network << "HTTP client disconnected: " << Network::get_addr_name(addr);

	// reduce count
	NetworkManager.connections.remove(addr);

	// close socket
	if (sock != -1) {
		close(sock);
		sock = -1;
	}
}

/* output a data of text -
 * deal with formatting new-lines and such, and also
 * escaping/removing/translating AweMUD commands
 */
void
HTTPHandler::stream_put (const char *text, size_t len) 
{
	output.write(text, len);
}

// process input
void
HTTPHandler::in_handle (char* buffer, size_t size)
{
	while (size > 0) {
		// if we're in the body of a POST, we need to get content_length bytes
		if (state == BODY) {
			size_t remain = content_length - line.size();
			if (size >= remain) {
				line.write(buffer, remain);
				size -= remain;
				buffer += remain;
				if (line.size() >= content_length) {
					process();
					line.clear();
				}
			} else {
				line.write(buffer, size);
				size = 0;
			}
		// if we're in REQ or HEADER, we need to read in lines
		} else if (state == REQ || state == HEADER) {
			char* c;
			if ((c = strchr(buffer, '\n')) != NULL) {
				// put line into buffer; hack to ignore \r
				if (c > buffer && *(c - 1) == '\r')
					line.write(buffer, c - buffer - 1);
				else
					line.write(buffer, c - buffer);
				// process the line
				process();
				line.clear();
				// update input data
				size -= (c - buffer) + 1;
				buffer = c + 1;
			} else {
				// append remaining data to our line buffer
				line.write(buffer, size);
			}
		// any other state, just ignore it
		} else {
			// ignore
		}
	}
}

// flush out the output, write prompt
void
HTTPHandler::prepare ()
{
}

char
HTTPHandler::get_poll_flags ()
{
	char flags = POLLSYS_READ;
	if (!output.empty())
		flags |= POLLSYS_WRITE;
	return flags;
}

void
HTTPHandler::out_ready ()
{
	// FIXME: do this right
	send(sock, output.c_str(), output.size(), 0);
	output.clear();

	// disconnect if that we are done
	if (state == DONE || state == ERROR)
		disconnect();
}

void
HTTPHandler::hangup ()
{
	disconnect();
}

void
HTTPHandler::process ()
{
	switch (state) {
		case REQ:
		{
			String text = line.str();

			// parse
			StringList parts = explode(text, ' ');

			// check size
			if (parts.size() != 3) {
				*this << "HTTP/1.0 400 Bad Request\n\nInvalid request\n";
				state = ERROR;
				return;
			}

			// request type
			if (parts[0] == "GET")
				reqtype = GET;
			else if (parts[0] == "POST")
				reqtype = POST;
			else {
				*this << "HTTP/1.0 405 Method Not Allowed\n\nUnauthorized request type " << parts[0] << "\n";
				state = ERROR;
				return;
			}

			// get URL
			StringList url_parts = explode(parts[1], '/');
			if (url_parts.size() != 2 && url_parts.size() != 3) {
				*this << "HTTP/1.0 404 Not Found\n\nUnknown URL " << parts[1] << "\n";
				state = ERROR;
				return;
			}
			url = url_parts[1];
			if (url_parts.size() == 3)
				reqid = url_parts[2];

			state = HEADER;
			break;
		}
		case HEADER:
		{
			// no more headers
			if (line.empty()) {
				// do header check
				check_headers();
				if (state == ERROR)
					return;

				// a GET request is now processed immediately
				if (reqtype == GET) {
					execute();
					state = DONE;
				// a POST request requires us to parse the body first
				} else {
					state = BODY;
				}
				break;
			}

			// parse the header
			const char* c = strchr(line.c_str(), ':');
			// require ': ' after header name
			if (c == NULL || *(c + 1) != ' ') {
				*this << "HTTP/1.0 400 Bad Request\n\nInvalid header:\n" << line.c_str() << "\n";
				state = ERROR;
				return;
			}
			headers[strlower(String(line.c_str(), c - line.c_str()))] = String(c + 2);
			break;
		}
		case BODY:
		{
			// parse the data
			const char* begin;
			const char* end;
			const char* sep;

			begin = line.c_str();
			do {
				// find the end of the current pair
				end = strchr(begin, '&');
				if (end == NULL)
					end = line.c_str() + line.size();

				// find the = separator
				sep = strchr(begin, '=');
				if (sep == NULL || sep > end) {
					*this << "HTTP/1.0 400 Bad Request\n\nMalformed form data.\n";
					state = ERROR;
					return;
				}

				// store the data
				post[strlower(String(begin, sep - begin))] = String(sep + 1, end - sep - 1);

				// set begin to next token
				begin = end;
				if (*begin == '&')
					++begin;
			} while (*begin != 0);

			// execute
			execute();
			state = DONE;
			break;
		}
		case DONE:
		case ERROR:
			break;
	}
}

void
HTTPHandler::check_headers()
{
	// if we're a POST, we must have the proper headers/values
	if (reqtype == POST) {
		// must be application/x-www-form-urlencoded
		if (headers[S("content-type")] != "application/x-www-form-urlencoded") {
			*this << "HTTP/1.0 406 Not Acceptable\n\nContent-type must be application/x-www-form-urlencoded.\n";
			state = ERROR;
			return;
		}

		// must have a content-length
		String len = headers[S("content-length")];
		if (len.empty()) {
			*this << "HTTP/1.0 406 Not Acceptable\n\nNo Content-length was provided.\n";
			state = ERROR;
			return;
		}
		content_length = strtoul(len.c_str(), NULL, 10);
	}
}

void
HTTPHandler::execute()
{
	if (url.empty())
		page_index();
	else if (url == "login")
		page_login();
	else if (url == "logout")
		page_logout();
	else if (url == "account")
		page_account();
	else {
		Scriptix::ScriptFunction func = HTTPPageManager.get_page(url);
		if (func) {
			*this << "HTTP/1.0 200 OK\nContent-type: text/html\n\n";
		} else {
			*this << "HTTP/1.0 404 Not Found\n\nPage not found\n\n";
		}
	}
}

void
HTTPHandler::page_index()
{
	*this << "HTTP/1.0 200 OK\nContent-type: text/html\n\n"
		<< StreamParse(HTTPPageManager.get_template(S("header")))
		<< StreamParse(HTTPPageManager.get_template(S("index")))
		<< StreamParse(HTTPPageManager.get_template(S("footer")));
}

void
HTTPHandler::page_login()
{
	String msg;

	// attempt login?
	if (post[S("action")] == "Login") {
		account = AccountManager.get(post[S("username")]);
		if (account != NULL && account->check_passphrase(post[S("password")])) {
			msg = S("Login successful!");
		} else {
			msg = S("Incorrect username or passphrase.");
		}
	}

	*this << "HTTP/1.0 200 OK\nContent-type: text/html\n\n"
		<< StreamParse(HTTPPageManager.get_template(S("header")), S("msg"), msg)
		<< StreamParse(HTTPPageManager.get_template(S("login")))
		<< StreamParse(HTTPPageManager.get_template(S("footer")));
}

void
HTTPHandler::page_logout()
{
	*this << "HTTP/1.0 200 OK\nContent-type: text/html\nSet-cookie: AWEMUD_SESSION=\n\n"
		<< StreamParse(HTTPPageManager.get_template(S("header")))
		<< StreamParse(HTTPPageManager.get_template(S("logout")))
		<< StreamParse(HTTPPageManager.get_template(S("footer")));
}

void
HTTPHandler::page_account()
{
	*this << "HTTP/1.0 200 OK\nContent-type: text/html\n\n"
		<< StreamParse(HTTPPageManager.get_template(S("header")))
		<< StreamParse(HTTPPageManager.get_template(S("account")))
		<< StreamParse(HTTPPageManager.get_template(S("footer")));
}

String
HTTPHandler::get_post (String id) const
{
	GCType::map<String,String>::const_iterator i = post.find(id);
	if (i == post.end())
		return String();
	return i->second;
}

int
SHTTPPageManager::initialize (void)
{
	File::Reader reader;

	// open templates file
	if (reader.open(SettingsManager.get_misc_path() + "/html"))
		return -1;

	// read said file
	FO_READ_BEGIN
		FO_WILD("html")
			templates[node.get_key()] = node.get_data();
	FO_READ_ERROR
		// damnable errors!
		return -1;
	FO_READ_END

	// all good
	return 0;
}

void
SHTTPPageManager::shutdown (void)
{
	templates.clear();
}

String
SHTTPPageManager::get_template (String id)
{
	TemplateMap::iterator i = templates.find(id);
	return i != templates.end() ? i->second : String();
}

Scriptix::ScriptFunction
SHTTPPageManager::get_page (String id)
{
	PageMap::iterator i = pages.find(id);
	return i != pages.end() ? i->second : Scriptix::ScriptFunction();
}

void
SHTTPPageManager::register_page (String id, Scriptix::ScriptFunction func)
{
	pages[id] = func;
}
