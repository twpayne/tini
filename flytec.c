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

#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "tini.h"

static const char base36[36] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

flytec_t *flytec_new(const char *device, FILE *logfile)
{
    flytec_t *flytec = alloc(sizeof(flytec_t));
    flytec->device = device;
    flytec->fd = open(flytec->device, O_NOCTTY | O_RDWR);
    if (flytec->fd == -1)
	error("open: %s: %s", flytec->device, strerror(errno));
    if (tcflush(flytec->fd, TCIOFLUSH) == -1)
	error("tcflush: %s: %s", flytec->device, strerror(errno));
    struct termios termios;
    memset(&termios, 0, sizeof termios);
    termios.c_iflag = IGNPAR;
    termios.c_cflag = CLOCAL | CREAD | CS8;
    cfsetispeed(&termios, B57600);
    cfsetospeed(&termios, B57600);
    if (tcsetattr(flytec->fd, TCSANOW, &termios) == -1)
	error("tcsetattr: %s: %s", flytec->device, strerror(errno));
    flytec->logfile = logfile;
    return flytec;
}

void flytec_delete(flytec_t *flytec)
{
    if (flytec) {
	snp_delete(flytec->snp);
	if (flytec->trackv) {
	    track_t **track;
	    for (track = flytec->trackv; *track; ++track)
		track_delete(*track);
	    free(flytec->trackv);
	}
	if (close(flytec->fd) == -1)
	    DIE("close", errno);
	free(flytec->pilot_name);
	free(flytec);
    }
}

static void flytec_read(flytec_t *flytec)
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(flytec->fd, &readfds);
    int rc;
    do {
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 250 * 1000;
	rc = select(flytec->fd + 1, &readfds, 0, 0, &timeout);
    } while (rc == -1 && errno == EINTR);
    if (rc == -1)
	DIE("select", errno);
    else if (rc == 0)
	error("%s: timeout waiting for data", flytec->device);
    else if (!FD_ISSET(flytec->fd, &readfds))
	DIE("select", 0);
    int n;
    do {
	n = read(flytec->fd, flytec->buf, sizeof flytec->buf);
    } while (n == -1 && errno == EINTR);
    if (n == -1)
	DIE("read", errno);
    else if (n == 0)
	DIE("read", 0);
    flytec->next = flytec->buf;
    flytec->end = flytec->buf + n;
}

int flytec_getc(flytec_t *flytec)
{
    if (flytec->next == flytec->end)
	flytec_read(flytec);
    if (flytec->next == flytec->end)
	return EOF;
    return *flytec->next++;
}

void flytec_expectc(flytec_t *flytec, char c)
{
    if (flytec_getc(flytec) != c)
	error("%s: unexpected character", flytec->device);
}

void flytec_puts_nmea(flytec_t *flytec, char *s)
{
    int checksum = 0;
    char *p;
    for (p = s; *p; ++p)
	checksum ^= (unsigned char) *p;
    int len = strlen(s) + 7;
    char *buf = alloc(len);
    if (snprintf(buf, len, "$%s*%02X\r\n", s, checksum) != len - 1)
	DIE("snprintf", 0);
    if (flytec->logfile)
	fprintf(flytec->logfile, "> %s", buf);
    int rc;
    do {
	rc = write(flytec->fd, buf, len);
    } while (rc == -1 && errno == EINTR);
    if (rc == -1)
	DIE("write", errno);
    free(buf);
}

char *flytec_gets(flytec_t *flytec, char *buf, int size)
{
    if (flytec->next == flytec->end)
	flytec_read(flytec);
    if (*flytec->next == XON)
	return 0;
    int len = size;
    char *p = buf;
    while (1) {
	if (--len <= 0)
	    DIE(__FUNCTION__, 0);
	if ((*p++ = *flytec->next++) == '\n') {
	    *p = '\0';
	    if (flytec->logfile)
		fprintf(flytec->logfile, "< %s", buf);
	    return buf;
	}
	if (flytec->next == flytec->end)
	    flytec_read(flytec);
    }
}

char *flytec_gets_nmea(flytec_t *flytec, char *buf, int size)
{
    buf = flytec_gets(flytec, buf, size);
    if (!buf)
	return 0;
    int len = strlen(buf);
    if (len < 6)
	goto _error;
    if (buf[0] != '$' || buf[len - 5] != '*' || buf[len - 2] != '\r' || buf[len - 1] != '\n')
	goto _error;
    int checksum = 0;
    char *p;
    for (p = buf + 1; p != buf + len - 5; ++p)
	checksum ^= (unsigned char) *p;
    int result = 0;
    char xdigit = buf[len - 4];
    if ('0' <= xdigit && xdigit <= '9')
	result = (xdigit - '0') << 4;
    else if ('A' <= xdigit && xdigit <= 'F')
	result = (xdigit - 'A' + 0xa) << 4;
    else
	goto _error;
    xdigit = buf[len - 3];
    if ('0' <= xdigit && xdigit <= '9')
	result += xdigit - '0';
    else if ('A' <= xdigit && xdigit <= 'F')
	result += xdigit - 'A' + 0xa;
    else
	goto _error;
    if (checksum != result)
	goto _error;
    memmove(buf, buf + 1, len - 5);
    buf[len - 6] = '\0';
    return buf;
_error:
    error("%s: invalid NMEA response", flytec->device);
}

void flytec_pbrigc(flytec_t *flytec, void (*callback)(void *, const char *), void *data)
{
    flytec_puts_nmea(flytec, "PBRIGC,");
    flytec_expectc(flytec, XOFF);
    char line[128];
    while (flytec_gets(flytec, line, sizeof line))
	callback(data, line);
    flytec_expectc(flytec, XON);
}

snp_t *flytec_pbrsnp(flytec_t *flytec)
{
    if (flytec->snp)
	return flytec->snp;
    flytec_puts_nmea(flytec, "PBRSNP,");
    flytec_expectc(flytec, XOFF);
    char line[128];
    if (!flytec_gets_nmea(flytec, line, sizeof line))
	DIE("flytec_gets_nmea", 0);
    flytec->snp = snp_new(line);
    if (!flytec->snp)
	error("%s: invalid response", flytec->device);
    flytec_expectc(flytec, XON);
    /* determine manufacturer from instrument id */
    flytec->manufacturer = manufacturer_new(flytec->snp->instrument_id);
    /* strip leading and trailing spaces from pilot name */
    char *pilot_name = flytec->snp->pilot_name;
    while (*pilot_name == ' ')
	++pilot_name;
    char *pilot_name_end = pilot_name + 1;
    char *p;
    for (p = pilot_name; *p; ++p)
	if (*p != ' ')
	    pilot_name_end = p + 1;
    flytec->pilot_name = alloc(pilot_name_end - pilot_name + 1);
    memcpy(flytec->pilot_name, pilot_name, pilot_name_end - pilot_name);
    flytec->pilot_name[pilot_name_end - pilot_name] = '\0';
    flytec->serial_number = flytec->snp->serial_number;
    return flytec->snp;
}

track_t **flytec_pbrtl(flytec_t *flytec, const char *manufacturer, igc_filename_format_t filename_format)
{
    if (flytec->trackv)
	return flytec->trackv;
    manufacturer = manufacturer ? manufacturer : flytec->manufacturer;
    flytec_pbrsnp(flytec);
    flytec_puts_nmea(flytec, "PBRTL,");
    flytec_expectc(flytec, XOFF);
    char line[128];
    int index = 0;
    while (flytec_gets_nmea(flytec, line, sizeof line)) {
	track_t *track = track_new(line);
	if (!track)
	    error("%s: invalid response", flytec->device);
	if (track->index != index++)
	    error("%s: inconsistent data", flytec->device);
	if (flytec->trackv) {
	    if (track->count != flytec->trackc)
		error("%s: inconsistent data", flytec->device);
	} else {
	    flytec->trackc = track->count;
	    flytec->trackv = alloc((flytec->trackc + 1) * sizeof(track_t *));
	}
	flytec->trackv[track->index] = track;
    }
    if (flytec->trackc) {
	/* calculate daily flight indexes */
	int i;
	flytec->trackv[flytec->trackc - 1]->day_index = 1;
	for (i = flytec->trackc - 2; i >= 0; --i) {
	    if (flytec->trackv[i]->date == flytec->trackv[i + 1]->date)
		flytec->trackv[i]->day_index = flytec->trackv[i + 1]->day_index + 1;
	    else
		flytec->trackv[i]->day_index = 1;
	}
	/* calculate igc filenames */
	for (i = 0; i < flytec->trackc; ++i) {
	    track_t *track = flytec->trackv[i];
	    char serial_number[4];
	    int rc;
	    switch (filename_format) {
		case igc_filename_format_long:
		    track->igc_filename = alloc(128);
		    rc = snprintf(track->igc_filename, 128, "%04d-%02d-%02d-%s-%d-%02d.IGC", DATE_YEAR(track->date) + 1900, DATE_MON(track->date) + 1, DATE_MDAY(track->date), manufacturer, flytec->serial_number, track->day_index);
		    if (rc < 0 || rc > 128)
			error("snprintf");
		    break;
		case igc_filename_format_short:
		    track->igc_filename = alloc(16);
		    serial_number[0] = base36[flytec->serial_number % 36];
		    serial_number[1] = base36[(flytec->serial_number / 36) % 36];
		    serial_number[2] = base36[(flytec->serial_number / 36 / 36) % 36];
		    serial_number[3] = '\0';
		    rc = snprintf(track->igc_filename, 16, "%c%c%c%c%s%c.IGC", base36[DATE_YEAR(track->date) % 10], base36[DATE_MON(track->date) + 1], base36[DATE_MDAY(track->date)], manufacturer[0], serial_number, base36[track->day_index]);
		    if (rc < 0 || rc > 16)
			error("snprintf");
		    break;
	    }
	}

    } else {
	flytec->trackv = alloc(sizeof(track_t *));
    }
    flytec_expectc(flytec, XON);
    return flytec->trackv;
}

void flytec_pbrtr(flytec_t *flytec, track_t *track, void (*callback)(void *, const char *), void *data)
{
    char buf[9];
    if (snprintf(buf, sizeof buf, "PBRTR,%02d", track->index) != 8)
	DIE("sprintf", 0);
    flytec_puts_nmea(flytec, buf);
    flytec_expectc(flytec, XOFF);
    char line[1024];
    while (flytec_gets(flytec, line, sizeof line))
	callback(data, line);
    flytec_expectc(flytec, XON);
}
