/*
 * AweMUD NG - Next Generation AwesomePlay MUD
 * Copyright (C) 2000-2005  AwesomePlay Productions, Inc.
 * See the file COPYING for license details
 * http://www.awemud.net
 */


#include <stdlib.h>
#include <stdio.h>
#include <typeinfo>
#include <algorithm>

#include "mud/entity.h"
#include "common/error.h"
#include "common/awestr.h"
#include "mud/server.h"
#include "mud/parse.h"
#include "common/streams.h"
#include "mud/color.h"
#include "mud/player.h"
#include "mud/clock.h"
#include "mud/hooks.h"

String EntityArticle::names[] = {
	"normal",
	"proper",
	"unique",
	"plural",
	"vowel"
};

EntityArticle
EntityArticle::lookup (StringArg name)
{
	for (uint i = 0; i < COUNT; ++i)
		if (str_eq(name, names[i]))
			return i;
	return NORMAL;
}

// ----- Entity -----

Entity::Entity (const Scriptix::TypeInfo* type) : Scriptix::Native(type)
{
	flags.active = false;
}

EventHandler*
Entity::get_event (EventID name)
{
	// try ours
	for (EventList::iterator i = events.begin(); i != events.end(); ++i) {
		if ((*i)->get_event() == name)
			return *i;
	}

	// FIXME: need to let sub-classes handle blueprints...

	// nope
	return NULL;
}

void
Entity::activate (void)
{
	// must not already be active
	assert(!is_active());

	// assign unique ID if it has none
	if (!uid)
		UniqueIDManager.create(uid);

	// insert into unique ID table
	EntityManager.id_map.insert(std::pair<UniqueID, Entity*>(uid, this));

	// find best heartbeat
	eheartbeat = 0;
	for (uint8 i = 1; i < TICKS_PER_ROUND; ++i)
		if (EntityManager.elist[i].size() < EntityManager.elist[eheartbeat].size())
			eheartbeat = i;

	// add to manager's big list
	EntityManager.elist[eheartbeat].push_back(this);
	eself = --EntityManager.elist[eheartbeat].end();

	// register tags
	for (TagList::iterator i = tags.begin(); i != tags.end(); ++i) 
		EntityManager.tag_map.insert(std::pair<TagID, Entity*> (*i, this));

	flags.active = true;
}

void
Entity::deactivate (void)
{
	// must be active
	assert(is_active());

	// ID MAP
	UniqueIDMap::iterator i = EntityManager.id_map.find(uid);
	if (i != EntityManager.id_map.end())
		EntityManager.id_map.erase(i);

	// BIG LIST
	if (eself == EntityManager.ecur)
		EntityManager.ecur = EntityManager.elist[eheartbeat].erase(eself);
	else
		EntityManager.elist[eheartbeat].erase(eself);

	// TAG MAP
	for (TagList::iterator i = tags.begin(); i != tags.end(); ++i) {
		std::pair<TagTable::iterator, TagTable::iterator> mi = EntityManager.tag_map.equal_range(*i);
		while (mi.first != mi.second) {
			if (mi.first->second == this)
				EntityManager.tag_map.erase(mi.first++);
			else
				++mi.first;
		}
	}

	flags.active = false;
}

void
Entity::destroy (void)
{
	Entity* owner = get_owner();
	if (owner != NULL)
		owner->owner_release(this);
	if (is_active())
		deactivate();
}

bool
Entity::name_match (StringArg name) const
{
	if (phrase_match (get_name(), name))
		return true;

	// no match
	return false;
}


// display
void
Entity::display_name (const StreamControl& stream, EntityArticleType atype, bool capitalize) const
{
	StringArg name = get_name();
	EntityArticle article = get_article();

	// proper names, no articles
	if (article == EntityArticle::PROPER || atype == NONE) {
		// specialize output
		stream << ncolor();
		if (capitalize && name) {
			stream << (char)toupper(name[0]) << name.c_str() + 1;
		} else {
			stream << name;
		}
		stream << CNORMAL;
		return;
	// definite articles - uniques
	} else if (atype == DEFINITE || article == EntityArticle::UNIQUE) {
		if (capitalize)
			stream << "The ";
		else
			stream << "the ";
	// pluralized name
	} else if (article == EntityArticle::PLURAL) {
		if (capitalize)
			stream << "Some ";
		else
			stream << "some ";
	// starts with a vowel-sound
	} else if (article == EntityArticle::VOWEL) {
		if (capitalize)
			stream << "An ";
		else
			stream << "an ";
	// normal-type name, nifty.
	} else {
		if (capitalize)
			stream << "A ";
		else
			stream << "a ";
	}
	stream << ncolor() << name << CNORMAL;
}

void
Entity::display_desc (const StreamControl& stream) const
{
	stream << StreamParse(get_desc(), "self", this);
}

void
Entity::save (File::Writer& writer)
{
	writer.attr("uid", uid);

	// event handler list
	for (EventList::const_iterator i = events.begin (); i != events.end (); i ++) {
		writer.begin("event");
		(*i)->save(writer);
		writer.end();
	}

	// save tags
	for (TagList::const_iterator i = tags.begin(); i != tags.end(); ++i)
		writer.attr("tag", TagID::nameof(*i));

	// call save hook
	ScriptRestrictedWriter* swriter = new ScriptRestrictedWriter(&writer);
	save_hook(swriter);
	swriter->release();
	swriter = NULL;
}

void
Entity::save_hook (ScriptRestrictedWriter* writer)
{
	Hooks::save_entity(this, writer);
}

// load
int
Entity::load (File::Reader& reader)
{
	// catch errors
	try {
		File::Node node;
		
		// read loop
		while (reader.get(node)) {
			if (node.is_end())
				break;

			if (load_node(reader, node) != FO_SUCCESS_CODE) {
				if (node.is_attr()) Log::Error << "Unrecognized attribute '" << node.get_name() << "' at " << reader.get_filename() << ':' << node.get_line();
				else if (node.is_custom()) Log::Error << "Unexpected custom attribute '@" << node.get_name() << "' at " << reader.get_filename() << ':' << node.get_line();
				else if (node.is_begin()) Log::Error << "Unrecognized object '" << node.get_name() << "' at " << reader.get_filename() << ':' << node.get_line();
				return -1;
			}
		}

	// show errors
	} catch (File::Error& error) {
		Log::Error << error.get_what() << " at " << reader.get_filename() << ':' << reader.get_line();
		return -1;
	}

	// finish up
	return load_finish();
}

int
Entity::load_node (File::Reader& reader, File::Node& node)
{
	FO_NODE_BEGIN
		FO_ATTR("uid")
			FO_TYPE_ASSERT(ID);
			uid = UniqueIDManager.decode(node.get_data());
		FO_CUSTOM
			if (node.get_datatype() == File::TYPE_INT)
				set_property(node.get_name(), tolong(node.get_data()));
			else if (node.get_datatype() == File::TYPE_STRING)
				set_property(node.get_name(), node.get_data());
			else if (node.get_datatype() == File::TYPE_BOOL)
				set_property(node.get_name(), str_is_true(node.get_data()));
			else {
				Log::Error << "Invalid data type for script attribute at " << reader.get_filename() << ':' << node.get_line();
				return -1;
			}
		FO_ATTR("tag")
			add_tag(TagID::create(node.get_data()));
		FO_OBJECT("event")
			EventHandler* event = new EventHandler();
			if (!event->load(reader))
					events.push_back(event);
	FO_NODE_END
}

int
Entity::parse_property (const StreamControl& stream, StringArg comm, const ParseArgs& argv) const
{
	// SPECIAL: one-letter name commands
	if (comm.size() == 1) {
		switch (comm[0]) {
			case 'D':
				stream << StreamName(this, DEFINITE, true);
				return 0;
			case 'd':
				stream << StreamName(this, DEFINITE, false);
				return 0;
			case 'C':
			case 'I':
				stream << StreamName(this, INDEFINITE, true);
				return 0;
			case 'c':
			case 'i':
				stream << StreamName(this, INDEFINITE, false);
				return 0;
			case 'N':
				stream << StreamName(this, NONE, true);
				return 0;
			case 'n':
				stream << StreamName(this, NONE, false);
				return 0;
			default:
				return -1;
		}
	}

	// ENTITY's NAME
	if (str_eq(comm, "name")) {
		stream << StreamName(this);
		return 0;
	// ENTITY'S DESC
	} else if (str_eq(comm, "desc")) {
		display_desc(stream);
		return 0;
	}

	return -1;
}

bool
Entity::has_tag (TagID tag) const
{
	return tags.find(tag) != tags.end();
}

int
Entity::add_tag (TagID tag)
{
	// no duplicates
	if (has_tag(tag))
		return 1;

	// add tag
	tags.insert(tag);

	// register with entity manager
	// FIXME: check for error, maybe?
	if (is_active())
		EntityManager.tag_map.insert(std::pair<TagID, Entity*> (tag, this));

	return 0;
}

int
Entity::remove_tag (TagID tag)
{
	// find
	TagList::iterator ti = std::find(tags.begin(), tags.end(), tag);
	if (ti == tags.end())
		return 1;

	// remove
	tags.erase(ti);

	// unregister with entity manager
	if (is_active()) {
		std::pair<TagTable::iterator, TagTable::iterator> mi = EntityManager.tag_map.equal_range(tag);
		while (mi.first != mi.second) {
			if (mi.first->second == this) {
				EntityManager.tag_map.erase(mi.first);
				return 0;
			}
			++mi.first;
		}
		return 2; // failed to find in manager
	} else {
		// no active - no need to unregister
		return 0;
	}
}

void
Entity::set_owner (Entity* owner)
{
	assert(owner != NULL);

	Entity* old_owner = get_owner();
	if (old_owner != NULL)
		old_owner->owner_release(this);

	if (is_active() && !owner->is_active())
		deactivate();
	else if (!is_active() && owner->is_active())
		activate();
}

bool
Entity::operator< (const Entity& ent) const
{
	return strcasecmp(get_name().c_str(), ent.get_name().c_str()) < 0;
}

// ----- SEntityManager -----

SEntityManager EntityManager;

SEntityManager::SEntityManager (void) : elist(), ecur(), eheartbeat(0)
{
}

SEntityManager::~SEntityManager (void)
{
}

int
SEntityManager::initialize (void)
{
	return 0; // no error
}

void
SEntityManager::shutdown (void)
{
	for (uint8 i = 0; i < TICKS_PER_ROUND; ++i)
		elist[i].clear();
	tag_map.clear();
}

void
SEntityManager::heartbeat (void)
{
	// loop
	ecur = elist[eheartbeat].begin();
	Entity* cur;
	while (ecur != elist[eheartbeat].end()) {
		// get current
		cur = *ecur;

		// move ptr
		++ecur;

		// update entity
		cur->heartbeat();
	}

	// increment heartbeat
	if (++eheartbeat == TICKS_PER_ROUND)
		eheartbeat = 0;
}

size_t
SEntityManager::tag_count (TagID tag) const
{
	return tag_map.count(tag);
}

std::pair<TagTable::const_iterator, TagTable::const_iterator>
SEntityManager::tag_list (TagID tag) const
{
	return tag_map.equal_range(tag);
}

Entity*
SEntityManager::get (const UniqueID& uid) const
{
	UniqueIDMap::const_iterator i = id_map.find(uid);
	if (i != id_map.end())
		return i->second;
	return NULL;
}
