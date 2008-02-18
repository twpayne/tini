/*

   tini - download tracklogs from Brauniger and Flytec flight recorders
   Copyright (C) 2007-2008  Tom Payne

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <ctype.h>
#include "tini.h"

    static inline const char *
match_char(const char *p, char c)
{
    if (!p) return 0;
    return *p == c ? ++p : 0;
}

    static inline const char *
match_literal(const char *p, const char *s)
{
    if (!p) return 0;
    while (*p && *s && *p == *s) {
	++p;
	++s;
    }
    return *s ? 0 : p;
}

    static inline const char *
match_n_digits(const char *p, int n, int *result)
{
    if (!p) return 0;
    *result = 0;
    for (; n > 0; --n) {
	if ('0' <= *p && *p <= '9') {
	    *result = 10 * *result + *p - '0';
	    ++p;
	} else {
	    return 0;
	}
    }
    return p;
}

    static inline const char *
match_unsigned(const char *p, int *result)
{
    if (!p) return 0;
    if (!isdigit(*p)) return 0;
    *result = *p - '0';
    ++p;
    while (isdigit(*p)) {
	*result = 10 * *result + *p - '0';
	++p;
    }
    return p;
}

    static inline const char *
match_one_of(const char *p, const char *s, char *result)
{
    if (!p) return 0;
    for (; *s; ++s)
	if (*p == *s) {
	    *result = *p;
	    return ++p;
	}
    return 0;
}

    static inline const char *
match_string_until(const char *p, char c, int consume, char **result)
{
    if (!p) return 0;
    const char *start = p;
    while (*p && *p != c)
	++p;
    if (!p) return 0;
    *result = alloc(p - start + 1);
    memcpy(*result, start, p - start);
    (*result)[p - start] = '\0';
    return consume ? ++p : p;
}

    static inline const char *
match_until_eol(const char *p)
{
    if (!p) return 0;
    while (*p && *p != '\r')
	++p;
    if (*p != '\r') return 0;
    ++p;
    return *p == '\n' ? ++p : 0;
}

    static inline const char *
match_eos(const char *p)
{
    if (!p) return 0;
    return *p ? 0 : p;
}

    static const char *
match_b_record(const char *p, struct tm *tm)
{
    p = match_char(p, 'B');
    if (!p) return 0;
    int hour = 0, min = 0, sec = 0;
    p = match_n_digits(p, 2, &hour);
    p = match_n_digits(p, 2, &min);
    p = match_n_digits(p, 2, &sec);
    if (!p) return 0;
    p = match_until_eol(p);
    if (!p) return 0;
    tm->tm_hour = hour;
    tm->tm_min = min;
    tm->tm_sec = sec;
    return p;
}

    static const char *
match_hfdte_record(const char *p, struct tm *tm)
{
    int mday = 0, mon = 0, year = 0;
    p = match_literal(p, "HFDTE");
    if (!p) return 0;
    p = match_n_digits(p, 2, &mday);
    p = match_n_digits(p, 2, &mon);
    p = match_n_digits(p, 2, &year);
    p = match_literal(p, "\r\n");
    if (!p) return 0;
    tm->tm_year = year + 2000 - 1900;
    tm->tm_mon = mon - 1;
    tm->tm_mday = mday;
    return p;
}

int igc_tm_update(struct tm *tm, const char *p)
{
    switch (p[0]) {
	case 'B':
	    p = match_b_record(p, tm);
	    break;
	case 'H':
	    p = match_hfdte_record(p, tm);
	    break;
	default:
	    p = 0;
	    break;
    }
    return !!p;
}

const char *manufacturer_new(const char *instrument_id)
{
    if (
	    !strcmp(instrument_id, "5020") ||
	    !strcmp(instrument_id, "5030") ||
	    !strcmp(instrument_id, "6020") ||
	    !strcmp(instrument_id, "6030")
       )
	return "FLY";
    else if (
	    !strcmp(instrument_id, "COMPEO") ||
	    !strcmp(instrument_id, "COMPEO+") ||
	    !strcmp(instrument_id, "COMPETINO") ||
	    !strcmp(instrument_id, "COMPETINO+") ||
	    !strcmp(instrument_id, "GALILEO")
	    )
	return "BRA";
    else
	return "XXX";
}

set_t *set_merge(set_t *set, const char *p)
{
    while (*p) {
	while (*p == ',') ++p;
	int first = -1, last = -1;
	if (*p != '-') {
	    p = match_unsigned(p, &first);
	    if (!p) goto error;
	    last = first;
	}
	if (*p == '-') {
	    ++p;
	    if (*p == '\0' || *p == ',')
		last = -1;
	    else {
		p = match_unsigned(p, &last);
		if (!p) goto error;
	    }
	}
	if (*p == '\0')
	    ;
	else if (*p != ',')
	    goto error;
	set_t *node = alloc(sizeof(set_t));
	node->first = first;
	node->last = last;
	node->next = set;
	set = node;
    }
    return set;
error:
    error("invalid list");
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

snp_t *snp_new(const char *p)
{
    snp_t *snp = alloc(sizeof(snp_t));
    p = match_literal(p, "PBRSNP,");
    p = match_string_until(p, ',', 1, &snp->instrument_id);
    p = match_string_until(p, ',', 1, &snp->pilot_name);
    p = match_unsigned(p, &snp->serial_number);
    p = match_char(p, ',');
    p = match_string_until(p, '\0', 0, &snp->software_version);
    p = match_eos(p);
    if (!p) {
	snp_delete(snp);
	return 0;
    }
    return snp;
}

void snp_delete(snp_t *snp)
{
    if (snp) {
        free(snp->instrument_id);
        free(snp->pilot_name);
        free(snp->software_version);
        free(snp);
    }
}

track_t *track_new(const char *p)
{
    track_t *track = alloc(sizeof(track_t));
    p = match_literal(p, "PBRTL,");
    p = match_unsigned(p, &track->count);
    p = match_char(p, ',');
    p = match_unsigned(p, &track->index);
    p = match_char(p, ',');
    struct tm tm;
    memset(&tm, 0, sizeof tm);
    p = match_unsigned(p, &tm.tm_mday);
    p = match_char(p, '.');
    p = match_unsigned(p, &tm.tm_mon);
    p = match_char(p, '.');
    p = match_unsigned(p, &tm.tm_year);
    p = match_char(p, ',');
    p = match_unsigned(p, &tm.tm_hour);
    p = match_char(p, ':');
    p = match_unsigned(p, &tm.tm_min);
    p = match_char(p, ':');
    p = match_unsigned(p, &tm.tm_sec);
    p = match_char(p, ',');
    int duration_hour = 0, duration_min = 0, duration_sec = 0;
    p = match_unsigned(p, &duration_hour);
    p = match_char(p, ':');
    p = match_unsigned(p, &duration_min);
    p = match_char(p, ':');
    p = match_unsigned(p, &duration_sec);
    p = match_eos(p);
    if (!p) {
	track_delete(track);
	return 0;
    }
    tm.tm_mon -= 1;
    tm.tm_year += 2000 - 1900;
    track->date = DATE_NEW(tm);
    track->time = mktime(&tm);
    if (track->time == (time_t) -1)
	DIE("mktime", errno);
    track->duration = 3600 * duration_hour + 60 * duration_min + duration_sec;
    return track;
}

void track_delete(track_t *track)
{
    if (track) {
        free(track->igc_filename);
        free(track);
    }
}
