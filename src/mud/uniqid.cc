/*
 * AweMUD NG - Next Generation AwesomePlay MUD
 * Copyright (C) 2000-2005  AwesomePlay Productions, Inc.
 * See the file COPYING for license details
 * http://www.awemud.net
 */

#include <sys/time.h>
#include <unistd.h>

#include "settings.h"
#include "uniqid.h"
#include "fileobj.h"

SUniqueIDManager UniqueIDManager;

int
SUniqueIDManager::initialize ()
{
	seq = 0;
	clock = rand() % 4096;
	gettimeofday(&last_time, NULL);

	File::Reader reader;
	if (reader.open(SettingsManager.get_misc_path() + "/uniqid"))
		return -1;

	FO_READ_BEGIN
		FO_ATTR("sequence")
			FO_TYPE_ASSERT(INT)
			seq = strtoul(node.get_data(), NULL, 10);

		FO_ATTR("clock")
			FO_TYPE_ASSERT(INT)
			clock = strtoul(node.get_data(), NULL, 10);
		FO_ATTR("last_time")
			FO_TYPE_ASSERT(STRING)
			uint32 sec;
			uint32 usec;
			sscanf(node.get_data(), "%u.%u", &sec, &usec);
			last_time.tv_sec = sec;
			last_time.tv_usec = usec;
	FO_READ_ERROR
		return -1;
	FO_READ_END

	return 0;
}

void
SUniqueIDManager::shutdown ()
{
}

void
SUniqueIDManager::save ()
{
	File::Writer writer;

	if (writer.open(SettingsManager.get_misc_path() + "/uniqid"))
		return;

	writer.attr("sequence", seq);
	writer.attr("clock", clock);

	StringBuffer tstr;
	tstr << last_time.tv_sec << '.' << last_time.tv_usec;
	writer.attr("last_time", tstr.str());
}

void
SUniqueIDManager::create (UniqueID& uid)
{
	struct timeval tv;

	/* get time, shift for year 2000 base */
	gettimeofday(&tv, NULL);
	tv.tv_sec -= 946080000; /* 30 years */

	/* increment clock sequence if time moved backwards */
	if (timercmp(&tv, &last_time, <))
		if ((++clock) == 4096)
			clock = 0;

	/* record time */
	last_time = tv;

	/* set UUID */
	uid.random = rand();
	uid.usecs = tv.tv_usec;
	uid.clock = clock;
	uid.seq = ++seq;
	uid.secs = tv.tv_sec;
}

String
SUniqueIDManager::encode (const UniqueID& uid)
{
	char buffer[64];

	snprintf(buffer, sizeof(buffer), "%08x-%05x-%03x-%08x-%08x",
			uid.random,
			uid.usecs,
			uid.clock,
			uid.seq,
			uid.secs);

	return buffer;
}

UniqueID
SUniqueIDManager::decode (StringArg string)
{
	UniqueID uid;

	uint32 random, usecs, clock, seq, secs;
	sscanf(string, "%x-%x-%x-%x-%x",
			&random,
			&usecs,
			&clock,
			&seq,
			&secs);

	uid.random = random;
	uid.usecs = usecs;
	uid.clock = clock;
	uid.seq = seq;
	uid.secs = secs;

	return uid;
}
