/*
 * Source MUD
 * Copyright (C) 2000-2005  Sean Middleditch
 * See the file COPYING for license details
 * http://www.sourcemud.org
 */

#include "common/streams.h"
#include "mud/macro.h"

StreamMacro::StreamMacro(const std::string& s_text)
	: text(s_text), argv()
{}

StreamMacro::StreamMacro(const std::string& s_text, const std::string& s_name, MacroValue s_value)
	: text(s_text)
{
	argv[s_name] = s_value;
}

StreamMacro::StreamMacro(const std::string& s_text, const std::string& s_name1, MacroValue s_value1, const std::string& s_name2, MacroValue s_value2)
	: text(s_text)
{
	argv[s_name1] = s_value1;
	argv[s_name2] = s_value2;
}

StreamMacro::StreamMacro(const std::string& s_text, const std::string& s_name1, MacroValue s_value1, const std::string& s_name2, MacroValue s_value2, const std::string& s_name3, MacroValue s_value3)
	: text(s_text)
{
	argv[s_name1] = s_value1;
	argv[s_name2] = s_value2;
	argv[s_name3] = s_value3;
}

StreamMacro& StreamMacro::add(const std::string& s_name, MacroValue s_value)
{
	argv[s_name] = s_value;
	return *this;
}
