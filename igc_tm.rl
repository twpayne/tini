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

machine igc_tm;

action mday { tm->tm_mday = 10 * (fpc[-1] - '0') + fc - '0'; }
action mon  { tm->tm_mon  = 10 * (fpc[-1] - '0') + fc - '0' - 1; }
action year { tm->tm_year = 10 * (fpc[-1] - '0') + fc - '0' + 2000 - 1900; }
action hour { tm->tm_hour = 10 * (fpc[-1] - '0') + fc - '0'; }
action min  { tm->tm_min  = 10 * (fpc[-1] - '0') + fc - '0'; }
action sec  { tm->tm_sec  = 10 * (fpc[-1] - '0') + fc - '0'; }

HFDTErecord =
    "HFDTE"
    digit{2} @mday
    digit{2} @mon
    digit{2} @year;

Brecord =
    "B"
    digit{2} @hour
    digit{2} @min
    digit{2} @sec
    /[^\r]*/;

main := ( HFDTErecord | Brecord ) "\r\n" 0 @{ fbreak; };

}%%

int igc_tm_update(struct tm *tm, const char *p)
{
    %% write data noerror;
    int cs = 0;
    %% write init;
    %% write exec noend;
    return cs >= igc_tm_first_final;
}

/* vim: set filetype=ragel: */
