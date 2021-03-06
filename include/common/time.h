/*
 * Source MUD
 * Copyright (C) 2000-2005  Sean Middleditch
 * See the file COPYING for license details
 * http://www.sourcemud.org
 */

#ifndef SOURCEMUD_COMMON_TIME_H
#define SOURCEMUD_COMMON_TIME_H

// legacy cruft
std::string timeToStr(time_t time);
time_t strToTime(const std::string& str);

namespace Time
{

// RFC 822 time format
extern const std::string RFC_822_FORMAT;

// format time using the given string
std::string format(const std::string& format, time_t time = time(NULL));

} // namespace Time

#endif
