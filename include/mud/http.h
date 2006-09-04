/*
 * AweMUD NG - Next Generation AwesomePlay MUD
 * Copyright (C) 2000-2005  AwesomePlay Productions, Inc.
 * See the file COPYING for license details
 * http://www.awemud.net
 */

#ifndef AWEMUD_MUD_HTTP_H
#define AWEMUD_MUD_HTTP_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "common/gcmap.h"
#include "common/types.h"
#include "common/gcbase.h"
#include "common/strbuf.h"
#include "common/streams.h"
#include "mud/network.h"
#include "common/imanager.h"
#include "scriptix/native.h"
#include "scriptix/function.h"
#include "mud/account.h"
#include "mud/parse.h"

class HTTPSession : public GC
{
	public:
	HTTPSession (Account* s_account);

	String get_id () const { return id; }
	Account* get_account () const { return account; }

	String get_var (String id) const;
	void set_var (String id, String value);

	void update_timestamp();
	bool check_timestamp();

	void clear ();

	private:
	String id;
	time_t timestamp;
	Account* account;
	GCType::map<String,String> vars;
};

class HTTPHandler : public Scriptix::Native, public SocketUser, public IStreamSink, public Parsable
{
	public:
	HTTPHandler (int s_sock, const SockStorage& s_netaddr);

	// output
	virtual void stream_put (const char*, size_t len);
	virtual IStreamSink* get_stream () { return this; }

	// processing
	void process ();
	void check_headers ();
	void execute ();

	// hard-coded pages
	void page_index ();
	void page_login ();
	void page_logout ();
	void page_account ();

	// get post data
	String get_post (String name) const;

	// get user account
	HTTPSession* get_session () const { return session; }
	Account* get_account () const { return session ? session->get_account() : NULL; }

	// low-level IO
	void disconnect ();
	virtual void in_handle (char* buffer, size_t size);
	virtual char get_poll_flags ();
	virtual void out_ready ();
	virtual void hangup ();
	virtual void prepare ();

	// parse values
	int parse_property (const StreamControl& stream, String method, const ParseArgs& argv) const;
	void parse_default (const StreamControl& stream) const;

	protected:
	~HTTPHandler () {}

	protected:
	SockStorage addr;

	// HTTP parsing
	StringBuffer line;
	StringBuffer output;
	String url;
	enum { GET, POST } reqtype;
	enum { REQ, HEADER, BODY, DONE, ERROR } state;
	size_t content_length;
	time_t timeout;
	GCType::map<String, String> headers;

	// POST data
	GCType::map<String, String> post;

	// the session
	HTTPSession* session;
};

class SHTTPPageManager : public IManager
{
	public:
	virtual int initialize (void);
	virtual void shutdown (void);

	String get_template (String id);

	Scriptix::ScriptFunction get_page (String id);

	void register_page (String id, Scriptix::ScriptFunction func);

	HTTPSession* create_session (Account* account);
	void destroy_session (HTTPSession* session);
	HTTPSession* get_session (String id);

	void check_timeouts ();

	private:
	typedef GCType::map<String, String> TemplateMap;
	TemplateMap templates;

	typedef GCType::map<String, Scriptix::ScriptFunction> PageMap;
	PageMap pages;

	typedef GCType::map<String, HTTPSession*> SessionMap;
	SessionMap sessions;
};
extern SHTTPPageManager HTTPPageManager;

#endif