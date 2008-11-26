/*
 * Source MUD
 * Copyright (C) 2000-2005  Sean Middleditch
 * See the file COPYING for license details
 * http://www.sourcemud.org
 */

#include <dirent.h>

#include "common/file.h"
#include "common/log.h"

StringList file::getFileList(String path, String ext)
{
	StringList list;

	// open our directory
	DIR* dir = opendir(path.c_str());
	if (dir == NULL) {
		Log::Error << "Failed to open directory: " << path;
		return list;
	}

	// read all entries
	dirent* d;
	while ((d = readdir(dir)) != NULL) {
		Log::Info << path << "/" << d->d_name;

		// skip entries with a leading .
		if (d->d_name[0] == '.')
			continue;

		Log::Info << "no dot";

		// length must be long enough for the extension and a .
		// and an actual name
		if (strlen(d->d_name) < ext.size() + 2)
			continue;

		Log::Info << "long enough";

		// must be a . before the extension
		if (d->d_name[strlen(d->d_name) - ext.size() - 1] != '.')
			continue;

		Log::Info << "ext dot";

		// and of course must have the actual extension
		if (ext != &d->d_name[strlen(d->d_name) - ext.size()])
			continue;

		Log::Info << "ext";

		// add file to our result list!
		list.push_back(path + S("/") + String(d->d_name));
	}

	closedir(dir);
	return list;
}
