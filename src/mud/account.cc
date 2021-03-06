/*
 * Source MUD
 * Copyright (C) 2000-2005  Sean Middleditch
 * See the file COPYING for license details
 * http://www.sourcemud.org
 */

#include "common.h"
#include "common/md5.h"
#include "common/time.h"
#include "common/string.h"
#include "mud/account.h"
#include "mud/fileobj.h"
#include "mud/settings.h"

_MAccount MAccount;

Account::Account(const std::string& s_id) : id(s_id), active(0), maxcharacters(0), maxactive(0), timeout(0)
{
	flags.disabled = false;
	time_created = time(NULL);
	time_lastlogin = (time_t)0;
}

Account::~Account()
{
	// remove from account list
	_MAccount::AccountList::iterator i = MAccount.accounts.find(id);
	if (i != MAccount.accounts.end())
		MAccount.accounts.erase(i);
}

int Account::save() const
{
	std::string path = MSettings.getAccountPath() + "/" + strlower(id) + ".acct";

	// open
	File::Writer writer;
	if (writer.open(path))
		return -1;

	// save it out
	writer.attr("account", "name", name);
	writer.attr("account", "email", email);
	writer.attr("account", "passphrase", pass);
	for (std::vector<std::string>::const_iterator i = characters.begin(); i != characters.end(); ++i)
		writer.attr("account", "character", *i);
	if (flags.disabled)
		writer.attr("account", "disabled", "yes");
	if (maxcharacters > 0)
		writer.attr("account", "maxcharacters", maxcharacters);
	if (maxactive > 0)
		writer.attr("account", "maxactive", maxactive);
	if (timeout > 0)
		writer.attr("account", "maxactive", timeout);
	writer.attr("account", "created", timeToStr(time_created));
	writer.attr("account", "lastlogin", timeToStr(time_lastlogin));
	for (AccessList::const_iterator i = access.begin(); i != access.end(); ++i)
		writer.attr("account", "access", AccessID::nameof(*i));

	// done
	writer.close();
	return 0;
}

// password management
void Account::setPassphrase(const std::string& s_pass)
{
	// encrypt
	char enc_pass[MD5_BUFFER_SIZE];
	MD5::encrypt(s_pass.c_str(), enc_pass);

	// store
	pass = std::string(enc_pass);

	// force save
	save();
}

// check password
bool Account::checkPassphrase(const std::string& s_pass) const
{
	// empty?  auto-fail
	if (s_pass.empty())
		return false;

	// do compare
	return MD5::compare(pass.c_str(), s_pass.c_str());
}

// add a new character
void Account::addCharacter(const std::string& name)
{
	// not already in list?
	if (find(characters.begin(), characters.end(), name) != characters.end())
		return;

	// ok then, add it
	characters.push_back(name);
}

// remove a character
void Account::delCharacter(const std::string& name)
{
	// find in list
	std::vector<std::string>::iterator i;
	if ((i = find(characters.begin(), characters.end(), name)) == characters.end())
		return;

	// ok then, remove it
	characters.erase(i);
}

// get max characters allowed
uint Account::getMaxCharacters() const
{
	// explicit?
	if (maxcharacters > 0)
		return maxcharacters;

	// default
	return MSettings.getCharactersPerAccount();
}

// get max active characters allowed
uint Account::getMaxActive() const
{
	// explicit?
	if (maxactive > 0)
		return maxactive;

	// default
	return MSettings.getActivePerAccount();
}

// update login time
void Account::updateTimeLogin()
{
	time_lastlogin = time(NULL);
}

// access
bool Account::hasAccess(AccessID id) const
{
	return access.find(id) != access.end();
}

bool Account::grantAccess(AccessID id)
{
	// grant it
	access.insert(id);
	return true;
}

bool Account::revokeAccess(AccessID id)
{
	// find it
	AccessList::iterator i = access.find(id);
	if (i == access.end())
		return false;
	// remove it
	access.erase(i);
	return true;
}

int Account::macroProperty(const StreamControl& stream, const std::string& method, const MacroList& argv) const
{
	if (method == "id") {
		stream << id;
		return 0;
	} else if (method == "name") {
		stream << name;
		return 0;
	} else if (method == "email") {
		stream << email;
		return 0;
	} else {
		return -1;
	}
}

void Account::macroDefault(const StreamControl& stream) const
{
	stream << id;
}

int _MAccount::initialize()
{

	return 0;
}

void _MAccount::shutdown()
{
	// save all accounts
	for (AccountList::iterator i = accounts.begin(); i != accounts.end(); ++i) {
		i->second->save();
		i->second.reset();
	}
}

bool _MAccount::validName(const std::string& name)
{
	// length
	if (name.size() < ACCOUNT_NAME_MIN_LEN || name.size() > ACCOUNT_NAME_MAX_LEN)
		return false;

	// check characters
	for (uint i = 0; i < name.size(); ++i)
		if (!isalnum(name[i]))
			return false;

	// must be good
	return true;
}

bool _MAccount::validPassphrase(const std::string& pass)
{
	// length
	if (pass.size() < ACCOUNT_PASS_MIN_LEN)
		return false;

	// must be both letters and numbers
	bool let = false;
	bool num = false;
	for (uint i = 0; i < pass.size(); ++i)
		if (isalpha(pass[i]))
			let = true;
		else if (isdigit(pass[i]))
			num = true;

	// true if both let and num are now true
	return let && num;
}

std::tr1::shared_ptr<Account> _MAccount::get(const std::string& in_name)
{
	// force lower-case
	std::string name = strlower(in_name);

	// check validity
	if (!validName(name))
		return std::tr1::shared_ptr<Account>();

	// search loaded list
	AccountList::iterator i = accounts.find(name);
	if (i != accounts.end())
		return i->second;

	// try load
	File::Reader reader;

	// open
	if (reader.open(MSettings.getAccountPath() + "/" + name + ".acct"))
		return std::tr1::shared_ptr<Account>();

	// create
	std::tr1::shared_ptr<Account> account(new Account(name));
	if (!account)
		return std::tr1::shared_ptr<Account>();

	// read it in
	FO_READ_BEGIN
	FO_ATTR("account", "name")
	account->name = node.getString();
	FO_ATTR("account", "email")
	account->email = node.getString();
	FO_ATTR("account", "passphrase")
	account->pass = node.getString();
	FO_ATTR("account", "character")
	account->characters.push_back(node.getString());
	FO_ATTR("account", "maxcharacters")
	account->maxcharacters = node.getInt();
	FO_ATTR("account", "maxactive")
	account->maxactive = node.getInt();
	FO_ATTR("account", "timeout")
	account->timeout = node.getInt();
	FO_ATTR("account", "disabled")
	account->flags.disabled = node.getBool();
	FO_ATTR("account", "access")
	account->access.insert(AccessID::create(node.getString()));
	FO_ATTR("account", "created")
	account->time_created = strToTime(node.getString());
	FO_ATTR("account", "lastlogin")
	account->time_lastlogin = strToTime(node.getString());
	FO_READ_ERROR
	return std::tr1::shared_ptr<Account>();
	FO_READ_END

	// add to list
	accounts[account->getId()] = account;

	return account;
}

std::tr1::shared_ptr<Account> _MAccount::create(const std::string& name)
{
	// check validity
	if (!validName(name))
		return std::tr1::shared_ptr<Account>();

	// check if account exists?
	if (get(name) != NULL)
		return std::tr1::shared_ptr<Account>();

	// create
	std::tr1::shared_ptr<Account> account(new Account(name));
	if (account == NULL)
		return std::tr1::shared_ptr<Account>();

	// save
	account->save();

	// add to list
	accounts[account->getId()] = account;

	return account;
}

bool
_MAccount::exists(const std::string& name)
{
	// must be lower-case
	strlower(name);

	// must be a valid name
	if (!validName(name))
		return false;

	// look thru list for valid and/or connected players
	AccountList::iterator i = accounts.find(name);
	if (i != accounts.end())
		return true;

	// check if player file exists
	std::string path = MSettings.getAccountPath() + "/" + name + ".acct";
	struct stat st;
	int res = stat(path.c_str(), &st);
	if (res == 0)
		return true;
	if (res == -1 && errno == ENOENT)
		return false;
	Log::Error << "stat() failed for " << path << ": " << strerror(errno);
	return true;
}
