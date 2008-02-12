/*

tini - download tracklogs from Brauniger and Flytec flight recorders
Copyright (C) 2007-8  Tom Payne

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

%%{

machine manufacturer;

action flytec    { manufacturer = "FLY"; fbreak; }
action brauniger { manufacturer = "BRA"; fbreak; }

main :=
    "5020"       0 @flytec    |
    "5030"       0 @flytec    |
    "6020"       0 @flytec    |
    "6030"       0 @flytec    |
    "COMPEO"     0 @brauniger |
    "COMPEO+"    0 @brauniger |
    "COMPETINO " 0 @brauniger |
    "COMPETINO+" 0 @brauniger |
    "GALILEO"    0 @brauniger ;

}%%

const char *manufacturer_new(const char *instrument_id)
{
    const char *p = instrument_id;
    %% write data;
    int cs;
    const char *manufacturer = 0;
    %% write init;
    %% write exec noend;
    if (cs == manufacturer_error || cs < manufacturer_first_final)
	return "XXX";
    return manufacturer;
}

/* vim: set filetype=ragel: */
