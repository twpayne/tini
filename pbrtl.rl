/*

tini - download tracklogs from Brauniger and Flytec flight recorders
Copyright (C) 2007  Tom Payne

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "tini.h"

%%{

machine pbrtl;

action int_first { i = fc - '0'; }
action int_next { i = 10 * i + fc - '0'; }
int = digit $int_first digit* @int_next;

action pbrtl_count { track->count = i; }
action pbrtl_index { track->index = i; }
action pbrtl_mday { tm.tm_mday = i; }
action pbrtl_mon { tm.tm_mon = i - 1; }
action pbrtl_year {
    tm.tm_year = i + 2000 - 1900;
    track->date = DATE_NEW(tm);
}
action pbrtl_hour { tm.tm_hour = i; }
action pbrtl_min { tm.tm_min = i; }
action pbrtl_sec {
    tm.tm_sec = i;
    track->time = mktime(&tm);
    if (track->time == -1)
	DIE("mktime", errno);
}
action pbrtl_duration_hour { track->duration = 3600 * i; }
action pbrtl_duration_min { track->duration += 60 * i; }
action pbrtl_duration_sec { track->duration += i; }

main :=
    "PBRTL" ","
    int %pbrtl_count ","
    int %pbrtl_index ","
    int %pbrtl_mday "."
    int %pbrtl_mon "."
    int %pbrtl_year ","
    int %pbrtl_hour ":"
    int %pbrtl_min ":"
    int %pbrtl_sec ","
    int %pbrtl_duration_hour ":"
    int %pbrtl_duration_min ":"
    int %pbrtl_duration_sec
    0 @{ fbreak; };

}%%

track_t *track_new(const char *p)
{
    track_t *track = alloc(sizeof(track_t));
    struct tm tm;
    memset(&tm, 0, sizeof tm);
    %% write data;
    int cs;
    int i = 0;
    %% write init;
    %% write exec noend;
    if (cs == pbrtl_error || cs < pbrtl_first_final) {
	track_delete(track);
	return 0;
    }
    return track;
}

void track_delete(track_t *track)
{
    if (track) {
	free(track->igc_filename);
	free(track);
    }
}

/* vim: set filetype=ragel: */
