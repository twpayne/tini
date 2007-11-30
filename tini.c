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

#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>

#include "tini.h"

const char *program_name = 0;
const char *device = 0;
igc_filename_format_t igc_filename_format = igc_filename_format_long;
FILE *logfile = 0;
int overwrite = 0;
int quiet = 0;

void error(const char *message, ...)
{
    fprintf(stderr, "%s: ", program_name);
    va_list ap;
    va_start(ap, message);
    vfprintf(stderr, message, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

void die(const char *file, int line, const char *function, const char *message, int _errno)
{
    if (_errno)
	error("%s:%d: %s: %s: %s", file, line, function, message, strerror(_errno));
    else
	error("%s:%d: %s: %s", file, line, function, message);
}

void *alloc(int size)
{
    void *p = malloc(size);
    if (!p)
	DIE("malloc", errno);
    memset(p, 0, size);
    return p;
}

static void usage(void)
{
    printf("%s - download tracklogs from Brauniger and Flytec flight recorders\n"
	    "Usage: %s [options] [command]\n"
	    "Options:\n"
	    "\t-h, --help\t\t\tshow some help\n"
	    "\t-d, --device=DEVICE\t\tselect device (default is /dev/ttyS0)\n"
	    "\t-D, --directory=DIR\t\tdownload tracklogs to DIR\n"
	    "\t-l, --log=FILENAME\t\tlog communication to FILENAME\n"
	    "\t-m, --manufacturer=STRING\toverride manufacturer\n"
	    "\t-s, --short-filenames\t\tuse short filename style\n"
	    "\t-o, --overwrite\t\t\toverwrite existing IGC files\n"
	    "\t-q, --quiet\t\t\tdon't output aything\n"
	    "Commands:\n"
	    "\tid\t\t\t\tidentify flight recorder\n"
	    "\tli, list\t\t\tlist tracklogs\n"
	    "\tdo, download [LIST]\t\tdownload tracklogs (default is all)\n"
	    "\tig, igc\t\t\t\twrite currently selected tracklog to stdout\n"
	    "Supported flight recorders:\n"
	    "\tBrauniger Compeo and Competino\n"
	    "\tFlytec 5020 and 5030\n",
	    program_name, program_name);
}

typedef struct {
    track_t *track;
    FILE *file;
    int percentage;
    struct tm tm;
    int _sc_clk_tck;
    clock_t clock;
    int remaining_sec;
} download_data_t;

static void download_callback(void *data, const char *line)
{
    download_data_t *download_data = data;
    if (fputs(line, download_data->file) == EOF)
	DIE("fputs", errno);
    if (!quiet && igc_tm_update(&download_data->tm, line) && line[0] == 'B') {
	time_t time = mktime(&download_data->tm);
	int percentage = 100 * (time - download_data->track->time) / (download_data->track->duration ? download_data->track->duration : 1);
	if (percentage < 0)
	    percentage = 0;
	else if (percentage > 99)
	    percentage = 99;
	struct tms tms;
	clock_t clock = times(&tms);
	if (clock == (clock_t) -1)
	    DIE("times", errno);
	int divisor = download_data->_sc_clk_tck * (download_data->track->time - time);
	if (divisor == 0)
	    divisor = 1;
	int remaining_sec = ((clock - download_data->clock) * (time - download_data->track->time - download_data->track->duration) + download_data->_sc_clk_tck / 2) / divisor;
	if (remaining_sec < 1)
	    remaining_sec = 1;
	else if (remaining_sec > 99 * 60 + 59)
	    remaining_sec = 99 * 60 + 59;
	if (percentage != download_data->percentage || remaining_sec < download_data->remaining_sec) {
	    fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b%3d%%  %02d:%02d ETA", percentage, remaining_sec / 60, remaining_sec % 60);
	    download_data->percentage = percentage;
	    download_data->remaining_sec = remaining_sec;
	}
    }
}

static void tini_download(flytec_t *flytec, set_t *indexes, const char *manufacturer, igc_filename_format_t igc_filename_format)
{
    int count = 0;
    track_t **ptrack;
    for (ptrack = flytec_pbrtl(flytec, manufacturer, igc_filename_format); *ptrack; ++ptrack) {
	track_t *track = *ptrack;
	if (indexes && !set_include(indexes, track->index + 1))
	    continue;
	if (!overwrite) {
	    struct stat buf;
	    if (stat(track->igc_filename, &buf) == 0)
		continue;
	    if (errno != ENOENT)
		DIE("stat", errno);
	}
	if (!quiet)
	    fprintf(stderr, "%s: downloading %s  ", program_name, track->igc_filename);
	download_data_t download_data;
	memset(&download_data, 0, sizeof download_data);
	download_data.track = track;
	download_data.file = fopen(track->igc_filename, "w");
	if (!download_data.file)
	    error("fopen: %s: %s", track->igc_filename, strerror(errno));
	download_data._sc_clk_tck = sysconf(_SC_CLK_TCK);
	if (download_data._sc_clk_tck == -1)
	    DIE("sysconf", errno);
	struct tms tms;
	download_data.clock = times(&tms);
	if (download_data.clock == (clock_t) -1)
	    DIE("times", errno);
	if (!quiet)
	    fprintf(stderr, "  0%%           ");
	flytec_pbrtr(flytec, track, download_callback, &download_data);
	if (fclose(download_data.file) == EOF)
	    DIE("fclose", errno);
	if (!quiet) {
	    struct tms tms;
	    clock_t clock = times(&tms);
	    if (clock == (clock_t) -1)
		DIE("times", errno);
	    int sec = (clock - download_data.clock + download_data._sc_clk_tck / 2) / download_data._sc_clk_tck;
	    if (sec > 99 * 60 + 59)
		sec = 99 * 60 + 59;
	    fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b100%%  %02d:%02d    \n", sec / 60, sec % 60);
	}
	++count;
    }
    if (!quiet) {
	if (count)
	    fprintf(stderr, "%s: %d tracklog%s downloaded\n", program_name, count, count == 1 ? "" : "s");
	else if (*flytec_pbrtl(flytec, manufacturer, igc_filename_format) == 0)
	    fprintf(stderr, "%s: no tracklogs to download\n", program_name);
	else
	    fprintf(stderr, "%s: no new tracklogs to download\n", program_name);
    }
}

static void tini_id(flytec_t *flytec)
{
    flytec_pbrsnp(flytec);
    printf("--- \n");
    printf("instrument_id: \"%s\"\n", flytec->snp->instrument_id);
    printf("pilot_name: \"%s\"\n", flytec->pilot_name);
    printf("serial_number: %d\n", flytec->serial_number);
    printf("software_version: \"%s\"\n", flytec->snp->software_version);
}

static void igc_callback(void *data, const char *line)
{
    FILE *file = data;
    if (!fputs(line, file))
	DIE("fputs", errno);
}

static void tini_igc(flytec_t *flytec)
{
    flytec_pbrigc(flytec, igc_callback, stdout);
}

static void tini_list(flytec_t *flytec, const char *manufacturer, igc_filename_format_t igc_filename_format)
{
    track_t **ptrack;
    printf("--- \n");
    for (ptrack = flytec_pbrtl(flytec, manufacturer, igc_filename_format); *ptrack; ++ptrack) {
	track_t *track = *ptrack;
	char time[128];
	if (!strftime(time, sizeof time, "%Y-%m-%d %H:%M:%S +00:00", gmtime(&track->time)))
	    DIE("strftime", errno);
	int duration = track->duration;
	printf("- index: %d\n", track->index + 1);
	printf("  time: %s\n", time);
	printf("  duration: \"%02d:%02d:%02d\"\n", duration / 3600, (duration / 60) % 60, duration % 60);
	printf("  igc_filename: %s\n", track->igc_filename);
    }
    if (!quiet && *flytec_pbrtl(flytec, manufacturer, igc_filename_format) == 0)
	fprintf(stderr, "%s: no tracklogs\n", program_name);
}

int main(int argc, char *argv[])
{
    program_name = strrchr(argv[0], '/');
    program_name = program_name ? program_name + 1 : argv[0];

    const char *manufacturer = 0;

    device = getenv("TINI_DEVICE");
    if (!device)
	device = "/dev/ttyS0";

    setenv("TZ", "UTC", 1);
    tzset();

    opterr = 0;
    while (1) {
	static struct option options[] = {
	    { "device",          required_argument, 0, 'd' },
	    { "directory",       required_argument, 0, 'D' },
	    { "help",            no_argument,       0, 'h' },
	    { "overwrite",       no_argument,       0, 'o' },
	    { "quiet",           no_argument,       0, 'q' },
	    { "manufacturer",    required_argument, 0, 'm' },
	    { "short-filenames", no_argument,       0, 's' },
	    { "log",             required_argument, 0, 'l' },
	    { 0,                 0,                 0, 0 },
	};
	int c = getopt_long(argc, argv, ":D:d:hl:m:oqs", options, 0);
	if (c == -1)
	    break;
	switch (c) {
	    case 'D':
		if (chdir(optarg) == -1)
		    error("chdir: %s: %s", optarg, strerror(errno));
		break;
	    case 'd':
		device = optarg;
		break;
	    case 'h':
		usage();
		exit(EXIT_SUCCESS);
	    case 'l':
		if (strcmp(optarg, "-") == 0)
		    logfile = stdout;
		else {
		    logfile = fopen(optarg, "a");
		    if (!logfile)
			error("fopen: %s: %s", optarg, strerror(errno));
		}
		break;
	    case 'm':
		manufacturer = optarg;
		break;
	    case 'o':
		overwrite = 1;
		break;
	    case 'q':
		quiet = 1;
		break;
	    case 's':
		igc_filename_format = igc_filename_format_short;
		break;
	    case ':':
		error("option '%c' requires an argument", optopt);
	    case '?':
		error("invalid option '%c'", optopt);
	}
    }

    flytec_t *flytec = flytec_new(device, logfile);
    if (!manufacturer) {
	flytec_pbrsnp(flytec);
	manufacturer = flytec->manufacturer;
    }
    if (optind == argc || strcmp(argv[optind], "do") == 0 || strcmp(argv[optind], "download") == 0) {
	++optind;
	set_t *indexes = 0;
	for (; optind < argc; ++optind)
	    indexes = set_merge(indexes, argv[optind]);
	tini_download(flytec, indexes, manufacturer, igc_filename_format);
	set_delete(indexes);
    } else {
	if (optind + 1 != argc)
	    error("excess argument%s on command line", argc - optind == 1 ? "" : "s");
	if (strcmp(argv[optind], "id") == 0) {
	    tini_id(flytec);
	} else if (strcmp(argv[optind], "ig") == 0 || strcmp(argv[optind], "igc") == 0) {
	    tini_igc(flytec);
	} else if (strcmp(argv[optind], "li") == 0 || strcmp(argv[optind], "list") == 0) {
	    tini_list(flytec, manufacturer, igc_filename_format);
	} else {
	    error("invalid command '%s'", argv[optind]);
	}
    }

    flytec_delete(flytec);
    if (logfile && logfile != stdout)
	fclose(logfile);

    return EXIT_SUCCESS;
}
