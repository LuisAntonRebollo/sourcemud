/*
 * AweMUD NG - Next Generation AwesomePlay MUD
 * Copyright (C) 2000-2005  AwesomePlay Productions, Inc.
 * See the file COPYING for license details
 * http://www.awemud.net
 */

#include <vector>

#include <ctype.h>

#include "mud/macro.h"
#include "common/streams.h"
#include "mud/gametime.h"
#include "common/strbuf.h"

#define MACRO_OUT_SIZE 2048
#define MACRO_BUFFER_SIZE 4096
#define MAX_COMM_NAME 32
#define MAX_COMM_DATA 128
#define MACRO_MAX_DEPTH 8

#define CHUNK_LEN(start, end) ((end)-(start)+1)

// macro value
namespace {
	// if statements
	enum if_state {
		IF_DONE = 0,	// this if group has already had a true value
		IF_TRUE,		// this if group is currently in a true value
		IF_FALSE,		// this group has not yet had a true value
	};
	enum if_type {
		IF_NONE = 0,
		IF_EQUAL,
		IF_NOTEQUAL,
		IF_MATCH,
		IF_LESSTHAN,
		IF_LESSTHANOREQUAL,
		IF_GREATERTHAN,
		IF_GREATERTHANOREQUAL
	};
	enum token_type {
		TOK_ERROR,
		TOK_NAME,
		TOK_STRING,
		TOK_BEGIN,
		TOK_END,
		TOK_IF,
		TOK_ELIF,
		TOK_ELSE,
		TOK_ENDIF,
		TOK_BANG,
		TOK_VAR,
		TOK_EQ,
		TOK_NEQ,
		TOK_METHOD
	};

	typedef std::vector<char> IfStack;

	struct MacroState {
		IfStack if_stack;
		const MacroArgs& argv;
		int disable;

		inline MacroState (const MacroArgs& s_argv) : if_stack(), argv(s_argv), disable(0) {}
	};

	class MacroIn {
		public:
		MacroIn (const char* s_cur, const char* s_end) : cur(s_cur), end(s_end) {}

		char get () { if (cur == end) return 0; else return *cur++; }
		char peek () const { if (cur == end) return 0; else return *cur; }

		bool eof () const { return cur == end; }

		protected:
		const char* cur;
		const char* end;
	};

	token_type get_token (const char** in, const char* end, StringBuffer& namebuf);
	void skip (const char** in, const char* end);
	String get_arg (MacroArgs& argv, uint index);
	int invoke_method (const StreamControl& stream, MacroValue self, String method, MacroList& argv);
	int do_macro (const char** in, const char* end, MacroState& state, const StreamControl& stream, int depth, bool if_allowed);
	int do_text(const StreamControl& stream, String in, MacroState& state, int depth);
}

// externally defined in macro_macros.cc, generated from gen/macro-intr.xml
namespace macro {
	extern int exec_macro (const StreamControl& stream, String macro, MacroList& argv);
}

// function definitions
namespace {
	// get a token
	token_type get_token (MacroIn& in, StringBuffer& namebuf)
	{
		// clear buffer
		namebuf.reset();

		// skip whitespace
		while (!in.eof() && isspace(in.peek()))
			in.get();

		// no more tokens - error, expect }
		if (in.eof()) {
			return TOK_ERROR;
		}

		char c = in.get();

		// ! is a bang
		if (c == '!') {
			return TOK_BANG;
		// == is an equals comparison
		} else if (c == '=' && in.peek() == '=') {
			in.get(); // consume second =
			return TOK_EQ;
		// != is a not-equals comparison
		} else if (c == '!' && in.peek() == '=') {
			in.get(); // consume =
			return TOK_NEQ;
		// { is a begin
		} else if (c == '{') {
			return TOK_BEGIN;
		// } is an end
		} else if (c == '}') {
			return TOK_END;
		// $ is a variable
		} else if (c == '$') {
			return TOK_VAR;
		// . is method syntactic sugar
		} else if (c == '.') {
			return TOK_METHOD;

		// alphanumeric is a name
		} else if (isalnum(c) || c == '_') {
			namebuf << c;
			while (!in.eof() && (isalnum(in.peek()) || in.peek() == '-' || in.peek() == '_')) {
				c = in.get();
				namebuf << c;
			}
			// if, elif, else, and endif are special
			if (!strcasecmp(namebuf.c_str(), "if"))
				return TOK_IF;
			else if (!strcasecmp(namebuf.c_str(), "elif"))
				return TOK_ELIF;
			else if (!strcasecmp(namebuf.c_str(), "else"))
				return TOK_ELSE;
			else if (!strcasecmp(namebuf.c_str(), "endif"))
				return TOK_ENDIF;
			return TOK_NAME;

		// ' or " is a string
		} else if (c == '"' || c == '\'') {
			// find end of string
			while (!in.eof() && in.peek() != c)
				namebuf << in.get();
			// eat quote
			if (in.peek() == c)
				in.get();
			return TOK_STRING;
		}

		// unknown
		namebuf << c;
		return TOK_ERROR;
	}

	// skip to end of processing
	void skip (MacroIn& in)
	{
		int nest = 0;
		char quote = 0;

		// iterate
		while (!in.eof()) {
			// in quote?
			if (quote) {
				// de-quote?
				if (in.peek() == quote)
					quote = 0;
			// not in quote
			} else {
				// nest on {
				if (in.peek() == '{') {
					++nest;
				// ignore \\ sequence
				} else if (in.peek() == '\\') {
					in.get(); // eat character
				// handle quote
				} else if (in.peek() == '"' || in.peek() == '\'') {
					quote = in.peek();
				// un-nest
				} else if (in.peek() == '}') {
					if (nest == 0) {
						in.get(); // consume
						break;
					}
					--nest;
				}
			}
			// consume
			in.get();
		}
	}

	// get an argument as a string
	String get_arg (MacroList& argv, uint index)
	{
		// bounds check
		if (index >= argv.size())
			return String();

		// return string (empty if type is not a string)
		return argv[index].get_string();
	}

	// invoke a method
	int invoke_method (const StreamControl& stream, MacroValue self, String method, MacroList& argv)
	{
		// if it's an object, invoke Entity::macro_property();
		if (self.is_object()) {
			return self.get_object()->macro_property(stream, method, argv);
		}
		
		// it's a string, so process it ourself
		if (self.is_string()) {
			String string = self.get_string();

			// LENGTH
			if (!strcasecmp(method, "length")) {
				stream << string.size();
				return 0;
			}
		}

		return -1;
	}

	// handle a macro
	int do_macro (MacroIn& in, MacroState& state, const StreamControl& stream, int depth, bool if_allowed)
	{
		token_type token;
		bool is_if = false;
		bool is_bang = false;
		MacroValue value;
		String method;
		StringBuffer buffer;
		MacroList argv;

		// max depth
		if (depth > MACRO_MAX_DEPTH) {
			skip(in);
			stream << "{error: overflow}";
			return -1;
		}

		// grab a token
		if ((token = get_token(in, buffer)) == TOK_ERROR) {
			skip(in);
			stream << "{error: invalid token: " << buffer.c_str() << "}";
			return -1;
		}

		// if processing, depth one only
		if (if_allowed) {
			// begin new true block
			if (token == TOK_IF) {
				state.if_stack.push_back(IF_DONE);
				is_if = true;

				// if disabled, go no further
				if (state.disable)
					return 0;
			// execute another if
			} else if (token == TOK_ELIF) {
				// check we're not empty
				if (state.if_stack.empty()) {
					skip(in);
					stream << "{error: elif without if}";
					return -1;

				// already had true state - all done
				} else if (state.if_stack.back() == IF_TRUE || state.if_stack.back() == IF_DONE) {
					state.if_stack.back() = IF_DONE;
					skip(in);
					++state.disable;
					return 0;

				// we were false, so now we _may_ be true...
				} else {
					is_if = true;
				}
			// alternate to true state
			} else if (token == TOK_ELSE) {
				// chek we're not empty
				if (state.if_stack.empty()) {
					skip(in);
					stream << "{error: else without if}";
					return -1;

				// already had true state - all done
				} else if (state.if_stack.back() == IF_TRUE) {
					state.if_stack.back() = IF_DONE;
					++state.disable;

				// we were false, so now we are true
				} else if (state.if_stack.back() == IF_FALSE) {
					state.if_stack.back() = IF_TRUE;
					--state.disable;
				}

				skip(in);
				return 0;
			// end if block
			} else if (token == TOK_ENDIF) {
				// chek we're not empty
				if (state.if_stack.empty()) {
					skip(in);
					stream << "{error: endif without if}";
					return -1;

				// just end the if block
				} else {
					skip(in);
				}

				if (state.if_stack.back() == IF_DONE || state.if_stack.back() == IF_FALSE)
					--state.disable;
				state.if_stack.pop_back();
				return 0;
			}

			// if it was an if, grab another token
			if (is_if) {
				if ((token = get_token(in, buffer)) == TOK_ERROR) {
					skip(in);
					stream << "{error: invalid token: " << buffer.c_str() << "}";
					return -1;
				}
			}
		}

		// if we're disable and not in a check, exit now
		if (state.disable) {
			skip(in);
			return 0;
		}

		// if a bang, mark and grab another token
		if (token == TOK_BANG) {
			is_bang = true;

			if ((token = get_token(in, buffer)) == TOK_ERROR) {
				skip(in);
				stream << "{error: invalid token: " << buffer.c_str() << "}";
				return -1;
			}
		}

		// if a variable, get name, then another token
		if (token == TOK_VAR) {
			// expect a name token
			if ((token = get_token(in, buffer)) != TOK_NAME) {
				skip(in);
				stream << "{error: expected name}";
				return -1;
			}

			// get value of variable, error if no such variable
			value = MacroValue();
			MacroArgs::const_iterator i = state.argv.find(buffer.str());
			if (i != state.argv.end())
				value = i->second;

			// next token
			if ((token = get_token(in, buffer)) == TOK_ERROR) {
				skip(in);
				stream << "{error: invalid token: " << buffer.c_str() << "}";
				return -1;
			}

			// eat method token, syntactic sugar
			if (token == TOK_METHOD) {
				// next token
				if ((token = get_token(in, buffer)) == TOK_ERROR) {
					skip(in);
					stream << "{error: invalid token: " << buffer.c_str() << "}";
					return -1;
				}
			}
		}

		// if we have a name, then it's a method
		if (token == TOK_NAME) {
			method = buffer.str();

			// next token
			if ((token = get_token(in, buffer)) == TOK_ERROR) {
				skip(in);
				stream << "{error: invalid token: " << buffer.c_str() << "}";
				return -1;
			}

		// if we have no method, then we can have no arguments
		} else if (token != TOK_END) {
			skip(in);
			stream << "{error: arguments given to non-function}";
			return -1;
		}

		// keep pushing arguments
		while (token != TOK_END) {
			// string
			if (token == TOK_STRING) {
				argv.push_back(buffer.str());
			// sub-macro
			} else if (token == TOK_BEGIN) {
				if (do_macro(in, state, buffer, depth + 1, false)) {
					stream << "{error: unknown macro}";
					return -1;
				}
				argv.push_back(buffer.str());
			// something else
			} else {
				skip(in);
				stream << "{error: macro error}";
				return -1;
			}
			// another token
			if ((token = get_token(in, buffer)) == TOK_ERROR) {
				skip(in);
				stream << "{error: invalid token: " << buffer.c_str() << "}";
				return -1;
			}
		}

		// have a variable
		if (!value.is_null()) {
			// a method?
			if (method) {
				if (invoke_method(buffer, value, method, argv)) {
					stream << "{error: unknown method}";
					return -1;
				}
			// just a value
			} else {
				if (value.is_string())
					buffer << value.get_string();
				else if (value.is_object())
					value.get_object()->macro_default(buffer);
			}
		// just a function?
		} else if (method) {
			if (macro::exec_macro(buffer, method, argv)) {
				stream << "{error: macro failed}";
				return -1;
			}
		}

		// upper-case output if method name was upper-cased
		if (isupper(method[0])) {
			buffer[0] = toupper(buffer[0]);
		}

		// execute if statement
		if (is_if) {
			if (buffer[0] != 0) {
				state.if_stack.back() = IF_TRUE;
			} else {
				state.if_stack.back() = IF_FALSE;
				++state.disable;
			}
		// or expand bang
		} else if (is_bang) {
			MacroState bstate(state.argv);
			StringBuffer bbuffer;
			if (do_text(bbuffer, buffer.str(), bstate, depth + 1)) {
				stream << bbuffer;
				return -1;
			}
			stream << bbuffer;
		// or output text
		} else {
			stream << buffer;
		}

		// done
		return 0;
	}

	// macro text
	int
	do_text(const StreamControl& stream, String text, MacroState& state, int depth)
	{
		// declarations
		MacroIn in(text.c_str(), text.c_str() + text.size());

		// iterate over input
		while (!in.eof()) {
			char c = in.get();

			// backslash escape
			if (c == '\\') {
				// get escape operand
				c = in.get();

				if (!state.disable) {
					// another backslash
					if (c == '\\')
						stream << '\\';
					// newline
					else if (c == 'n')
						stream << '\n';
					// begin-macro
					else if (c == '{')
						stream << '{';
					// anything else is unsupported, ignore
				}

			// begin macro macro
			} else if (c == '{') {
				do_macro(in, state, stream, depth, true);
			// just text
			} else if (!state.disable) {
				stream << c;
			}
		}

		return 0;
	}
}

// parsing
namespace macro {
	// macro text
	const StreamControl&
	text(const StreamControl& stream, String in, const MacroArgs& argv)
	{
		MacroState state(argv);

		if (do_text(stream, in, state, 1))
			stream << in;

		return stream;
	}
}