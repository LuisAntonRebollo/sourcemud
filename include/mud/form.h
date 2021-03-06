/*
 * Source MUD
 * Copyright (C) 2000-2005  Sean Middleditch
 * See the file COPYING for license details
 * http://www.sourcemud.org
 */

#ifndef SOURCEMUD_MUD_FORM_H
#define SOURCEMUD_MUD_FORM_H

#include "common/types.h"

class FormColor
{
public:
	explicit FormColor(uint8_t s_value) : value(s_value) {}
	FormColor() : value(0) {}
	FormColor(const FormColor& c) : value(c.value) {}

	std::string getName() const;

	uint8_t getValue() const { return value; }

	static FormColor lookup(const std::string&);
	static FormColor create(const std::string&);

	bool valid() const { return value > 0 && value < names.size(); }

	bool operator == (const FormColor& c) const { return value == c.value; }
	bool operator < (const FormColor& c) const { return value < c.value; }

private:
	static std::vector<std::string> names;
	uint8_t value;
};

class FormBuild
{
public:
	typedef enum {
		NONE = 0,
		HUNCHED,
		GAUNT,
		LEAN,
		AVERAGE,
		HUSKY,
		STOCKY,
		POWERFUL,
		ATHLETIC,
		COUNT,
	} type_t;

	explicit FormBuild(int s_value) : value((type_t)s_value) {}
	FormBuild(type_t s_value) : value(s_value) {}
	FormBuild(const FormBuild& form) : value(form.value) {}
	FormBuild() : value(NONE) {}

	std::string getName() const { return valid() ? names[value] : std::string(); }

	type_t getValue() const { return value; }

	static FormBuild lookup(const std::string& name);

	bool valid() const { return value > 0 && value < COUNT; }

	bool operator == (FormBuild dir) const { return dir.value == value; }
	bool operator < (FormBuild dir) const { return value < dir.value; }

private:
	type_t value;

	static std::string names[];
};

class FormHeight
{
public:
	typedef enum {
		NONE = 0,
		TINY,
		SHORT,
		TYPICAL,
		TALL,
		COUNT,
	} type_t;

	explicit FormHeight(int s_value) : value((type_t)s_value) {}
	FormHeight(type_t s_value) : value(s_value) {}
	FormHeight(const FormHeight& form) : value(form.value) {}
	FormHeight() : value(NONE) {}

	std::string getName() const { return valid() ? names[value] : std::string(); }

	type_t getValue() const { return value; }

	static FormHeight lookup(const std::string& name);

	bool valid() const { return value > 0 && value < COUNT; }

	bool operator == (FormHeight dir) const { return dir.value == value; }
	bool operator < (FormHeight dir) const { return value < dir.value; }

private:
	type_t value;

	static std::string names[];
};

class FormHairStyle
{
public:
	typedef enum {
		NONE = 0,
		LONG_STRAIGHT,
		LONG_WAVY,
		LONG_CURLY,
		SHORT_STRAIGHT,
		SHORT_WAVY,
		SHORT_CURLY,
		COUNT,
	} type_t;

	explicit FormHairStyle(int s_value) : value((type_t)s_value) {}
	FormHairStyle(type_t s_value) : value(s_value) {}
	FormHairStyle(const FormHairStyle& form) : value(form.value) {}
	FormHairStyle() : value(NONE) {}

	std::string getName() const { return valid() ? names[value] : std::string(); }

	type_t getValue() const { return value; }

	static FormHairStyle lookup(const std::string& name);

	bool valid() const { return value > 0 && value < COUNT; }

	bool operator == (FormHairStyle dir) const { return dir.value == value; }
	bool operator < (FormHairStyle dir) const { return value < dir.value; }

private:
	type_t value;

	static std::string names[];
};

#endif
