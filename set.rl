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

machine set;

action first { i = fc - '0'; }
action next  { i = 10 * i + fc - '0'; }
int = ( digit $first digit* @next );

action new {
    set_t *node = alloc(sizeof(set_t));
    node->next = set;
    set = node;
}
action one { set->first = set->last = i; }
action all { set->first = set->last = -1; }
action upwards { set->last = -1; }
action last { set->last = i; }

set = ( int %one ( '-' %upwards ( int %last )? )? | '-' %all ( int %last )? ) >new;
main := set ( ',' set )* 0 @{ fbreak; };

}%%

set_t *set_merge(set_t *set, const char *list)
{
    %% write data;
    int cs = 0;
    int i = 0;
    %% write init;
    const char *p = list;
    %% write exec noend;
    if (cs == set_error || cs < set_first_final)
	error("invalid list '%s'", list);
    return set;
}

void set_delete(set_t *set)
{
    while (set) {
	set_t *next = set->next;
	free(set);
	set = next;
    }
}

int set_include(set_t *set, int element)
{
    for (; set; set = set->next) {
	if (set->first == -1 && set->last == -1) {
	    return 1;
	} else if (set->first == -1 && set->last != -1) {
	    if (element <= set->last)
		return 1;
	} else if (set->first != -1 && set->last == -1) {
	    if (set->first <= element)
		return 1;
	} else {
	    if (set->first <= element && element <= set->last)
		return 1;
	}
    }
    return 0;
}

/* vim: set filetype=ragel: */
