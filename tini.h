/*

   tini - download tracklogs from Flytec and Brauniger flight recorders
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

#ifndef TINI_H
#define TINI_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DIE(syscall, _errno) die(__FILE__, __LINE__, __FUNCTION__, (syscall), (_errno))

#define DATE_NEW(tm) (((tm).tm_year << 9) + ((tm).tm_mon << 5) + (tm).tm_mday)
#define DATE_YEAR(date) ((date) >> 9)
#define DATE_MON(date) (((date) >> 5) & 0xf)
#define DATE_MDAY(date) ((date) & 0x1f)

enum {
    XON = '\x11',
    XOFF = '\x13',
};

void error(const char *, ...) __attribute__ ((noreturn, format(printf, 1, 2)));
void die(const char *, int, const char *, const char *, int) __attribute__ ((noreturn));
void *alloc(int);

typedef struct _set_t {
    int first;
    int last;
    struct _set_t *next;
} set_t;

set_t *set_merge(set_t *, const char *);
void set_delete(set_t *);
int set_include(set_t *, int);

typedef struct {
    char *instrument_id;
    char *pilot_name;
    int serial_number;
    char *software_version;
} snp_t;

snp_t *snp_new(const char *);
void snp_delete(snp_t *);

const char *manufacturer_new(const char *);

typedef struct {
    int count;
    int index;
    int date;
    int day_index;
    time_t time;
    int duration;
    char *igc_filename;
} track_t;

track_t *track_new(const char *);
void track_delete(track_t *);

typedef struct {
    const char *device;
    int fd;
    FILE *logfile;
    snp_t *snp;
    const char *manufacturer;
    char *pilot_name;
    int serial_number;
    int trackc;
    track_t **trackv;
    char *next;
    char *end;
    char buf[128];
} flytec_t;

typedef enum {
    igc_filename_format_long,
    igc_filename_format_short
} igc_filename_format_t;

void flytec_error(flytec_t *, const char *message, ...);
flytec_t *flytec_new(const char *, FILE *);
void flytec_delete(flytec_t *);
int flytec_getc(flytec_t *);
void flytec_expectc(flytec_t *, char);
void flytec_puts_nmea(flytec_t *, char *);
char *flytec_gets(flytec_t *, char *, int);
char *flytec_gets_nmea(flytec_t *, char *, int);
int flytec_ping(flytec_t *);
void flytec_pbrigc(flytec_t *, void (*)(void *, const char *), void *);
snp_t *flytec_pbrsnp(flytec_t *);
track_t **flytec_pbrtl(flytec_t *, const char *, igc_filename_format_t);
void flytec_pbrtr(flytec_t *, track_t *, void (*)(void *, const char *), void *);

int igc_tm_update(struct tm *, const char *);

#endif
