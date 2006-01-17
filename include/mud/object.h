/*
 * AweMUD NG - Next Generation AwesomePlay MUD
 * Copyright (C) 2000-2005  AwesomePlay Productions, Inc.
 * See the file COPYING for license details
 * http://www.awemud.net
 */

#ifndef OBJECT_H
#define OBJECT_H

#include "mud/entity.h"
#include "common/awestr.h"
#include "common/error.h"
#include "mud/body.h"
#include "mud/elist.h"
#include "mud/container.h"
#include "mud/server.h"
#include "common/imanager.h"
#include "common/gcmap.h"
#include "scriptix/native.h"
#include "scriptix/function.h"

// WEIGHT:
//  cost is in hundredths of 1 unit of current (ex: US dollar)
//  so a cost of 2100 would possibly equal 21 US$.  In most
//  fantasy settings, gold pieces or silver pieces are the standard
//  unit of currency, so 2100 might equal 21 gold or 21 silver.

//  weight is measured in decagrams.  There are 100 decagrams to a
//  kilogram.  For the imperialists out there (such as myself),
//  there are roughly 4.453 kilograms to each pound.  Most fantasy
//  games might want to convert all weights in stones; there are
//  14 pounds to a stone, or 16.350 kilograms to a stone.  To
//  convert weight to pounds, divide by 445.3.  To convert weight
//  to stones, divide weigth by 6234.2.

// TRASH:
//  the trash timer should be configurable...

#define OBJECT_TRASH_TICKS ROUNDS_TO_TICKS(60 * 20)
  // trash objects after 20 minutes
#define OBJECT_ROT_TICKS ROUNDS_TO_TICKS(60 * 1)
  // rotting objects wither away after 1 minute

// ACTIONS:
//  Script usage return codes follow.  Scripted actions must return
//  one of the codes.  Default return is nil (0).
enum ObjectActionCode {
	OBJECT_ACTION_OK_NORMAL = 0,		// do normal processing (may send event)
	OBJECT_ACTION_OK_QUIET = 1,			// just send an event
	OBJECT_ACTION_CANCEL_NORMAL = 2,	// error message, no event
	OBJECT_ACTION_CANCEL_QUIET = 3		// no further processing at all
};

// Object blueprint
class
ObjectBlueprint : public Scriptix::Native
{
	public:
	typedef GCType::set<ContainerType> ContainerList;
	typedef GCType::map<String,Scriptix::ScriptFunctionSource> ActionList;

	ObjectBlueprint (void);

	// blueprint id
	inline String get_id (void) const { return id; }

	// name
	inline const String& get_name (void) const { return name.name; }
	inline void set_name (StringArg s_name) { name.name = s_name; set_flags.name = true; }
	inline EntityArticle get_article (void) const { return name.article; }
	inline void set_article (EntityArticle s_type) { name.article = s_type; set_flags.article = true; }
	void reset_name (void);

	inline const StringList& get_keywords (void) const { return keywords; }

	// description
	inline const String& get_desc (void) const { return desc; }
	inline void set_desc (StringArg s_desc) { desc = s_desc; set_flags.desc = true; }
	void reset_desc (void);

	// weight
	inline uint get_weight (void) const { return weight; }
	inline void set_weight (uint s_weight) { weight = s_weight; set_flags.weight = true; }
	void reset_weight (void);

	// cost
	inline uint get_cost (void) const { return cost; }
	inline void set_cost (uint s_cost) { cost = s_cost; set_flags.cost = true; }
	void reset_cost (void);

	// equip location
	inline EquipLocation get_equip (void) const { return equip; }
	inline void set_equip (EquipLocation s_equip) { equip = s_equip; set_flags.equip = true; }
	void reset_equip (void);

	// flags
	inline bool is_hidden (void) const { return flags.hidden; }
	inline void set_hidden (bool v) { flags.hidden = v; set_flags.hidden = true; }
	void reset_hidden (void);
	inline bool is_touchable (void) const { return flags.touchable; }
	inline void set_touchable (bool v) { flags.touchable = v; set_flags.touchable = true; }
	void reset_touchable (void);
	inline bool is_gettable (void) const { return flags.gettable; }
	inline void set_gettable (bool v) { flags.gettable = v; set_flags.gettable = true; }
	void reset_gettable (void);
	inline bool is_dropable (void) const { return flags.dropable; }
	inline void set_dropable (bool v) { flags.dropable = v; set_flags.dropable = true; }
	void reset_dropable (void);
	inline bool is_trashable (void) const { return flags.trashable; }
	inline void set_trashable (bool v) { flags.trashable = v; set_flags.trashable = true; }
	void reset_trashable (void);
	inline bool is_rotting (void) const { return flags.rotting; }
	inline void set_rotting (bool v) { flags.rotting = v; set_flags.rotting = true; }
	void reset_rotting (void);

	// update inherited data
	void refresh (void);

	// containers
	inline const ContainerList& get_containers (void) const { return containers; }
	bool set_container_exist (ContainerType type, bool);
	bool has_container (ContainerType type) const;

	// actions
	inline const ActionList& get_actions (void) const { return actions; }

	// load
	int load (File::Reader& reader);
	void save (File::Writer& writer);

	// parent blueprint
	virtual ObjectBlueprint* get_parent (void) const { return parent; }

	private:
	String id;
	ObjectBlueprint* parent;
	
	ContainerList containers;
	ActionList actions;

	EntityName name;
	String desc;
	uint weight;
	uint cost;
	EquipLocation equip;
	StringList keywords;

	// flags
	struct Flags {
		char hidden:1, touchable:1, gettable:1, dropable:1,
			trashable:1, rotting:1;
		inline Flags (void) : hidden(false), touchable(true),
			gettable(true), dropable(true), trashable(true),
			rotting(false) {}
	} flags;

	// set flags
	struct SetFlags {
		int	name:1,
			article:1,
			desc:1,
			weight:1,
			cost:1,
			equip:1,
			hidden:1,
			touchable:1,
			gettable:1,
			dropable:1,
			trashable:1,
			attack:1,
			rotting:1;
		inline SetFlags (void) : name(false), article(false), desc(false),
			weight(false), cost(false), equip(false),
			hidden(false), touchable(false), gettable(false), dropable(false),
			trashable(false), attack(false), rotting(false) {}
	} set_flags;

	void set_parent (ObjectBlueprint* blueprint);

	virtual Scriptix::Value get_undefined_property (Scriptix::Atom id) const;
};

// Object control
class
Object : public Entity
{
	public:
	Object (void);
	Object (ObjectBlueprint* s_blueprint);

	// name info
	virtual String get_name (void) const;
	virtual EntityArticle get_article (void) const;
	virtual bool name_match (StringArg name) const;

	// description
	virtual String get_desc (void) const;

	// not supported - blueprint
	inline void set_name (StringArg s_name) { name.name = s_name; }
	inline void set_article (EntityArticle s_article) { name.article = s_article; }

	// save/load
	virtual int load_node (File::Reader& reader, File::Node& node);
	virtual int load_finish (void);
	virtual void save (File::Writer& writer);
	virtual void save_hook (class ScriptRestrictedWriter* writer);

	// weight
	inline uint get_weight (void) const { return calc_weight + get_real_weight(); }
	uint get_real_weight (void) const;

	// owner tracking - see entity.h
	virtual inline Entity* get_owner (void) const { return owner; }
	virtual void set_owner (Entity* s_owner);
	virtual void owner_release (Entity* child);

	// returns the room the object is in (tracing through parents) or the
	// character holding the object (again, tracing through parenst)
	class Character* get_holder (void) const;
	class Room* get_room (void) const;

	// actions
	Scriptix::ScriptFunction get_action (StringArg action);
	ObjectActionCode do_action (StringArg action, Entity* user, Scriptix::Value data = Scriptix::Value());

	// name color
	virtual const char *ncolor (void) const { return CITEM; }

	// for parsing, pull a property based on a char*
	virtual int parse_property (const class StreamControl& stream, StringArg method, const ParseArgs& argv) const;

	// object properties
	uint get_cost (void) const;
	EquipLocation get_equip (void) const;

	// check flags
	bool is_hidden (void) const;
	bool is_touchable (void) const;
	bool is_gettable (void) const;
	bool is_dropable (void) const;
	bool is_trashable (void) const;
	bool is_rotting (void) const;

	// return ture if we derive from the named blueprint
	bool is_blueprint (StringArg blueprint) const;

	// (de)activate children
	virtual void activate (void);
	virtual void deactivate (void);

	// heartbeat
	void heartbeat (void);

	// blueprint information
	virtual ObjectBlueprint* get_blueprint (void) const { return blueprint; }
	void set_blueprint (ObjectBlueprint* blueprint);
	static Object* load_blueprint (StringArg name);

	// containers
	bool has_container (ContainerType type) const;
	bool add_object (Object *sub, ContainerType type);
	void remove_object (Object *sub, ContainerType type);
	Object *find_object (const char *name, uint index, ContainerType type, uint *matches = NULL) const;
	void show_contents (class Player *player, ContainerType type) const;

	// data
	private:
	EntityName name;
	Entity *owner;
	ObjectBlueprint* blueprint;
	ContainerType location;
	uint calc_weight; // calculated weight of children objects
	uint trash_timer; // ticks until trashed

	EList<Object> children; // child objects

	// weight tracking
	void recalc_weight (void);

	protected:
	virtual ~Object (void);

	virtual Scriptix::Value get_undefined_property (Scriptix::Atom id) const;

	E_TYPE(Object)
};

class SObjectBlueprintManager : public IManager
{
	typedef GCType::map<String,ObjectBlueprint*> BlueprintMap;

	public:
	int initialize (void);

	void shutdown (void);

	ObjectBlueprint* lookup (StringArg id);

	private:
	BlueprintMap blueprints;
};

extern SObjectBlueprintManager ObjectBlueprintManager;

#define OBJECT(ent) E_CAST(ent,Object)

#endif
