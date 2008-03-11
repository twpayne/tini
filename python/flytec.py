import datetime
import logging
import nmea
import os
import re

# Manufacturers

MANUFACTURER = {}
for instrument in 'COMPEO COMPEO+ COMPETINO COMPETINO+ GALILEO'.split(' '):
  MANUFACTURER[instrument] = 'Br\xc3\xa4uniger'.decode('utf-8')
for instrument in '5020 5030 6020 6030'.split(' '):
  MANUFACTURER[instrument] = 'Flytec'

# Strings

XON = '\021'
XOFF = '\023'

# Regular expressions

PBRMEMR_RE = re.compile('\\APBRMEMR,([0-9A-F]+),([0-9A-F]+(?:,[0-9A-F]+)*)\\Z')
PBRRTS_RE1 = re.compile('\\APBRRTS,(\\d+),(\\d+),0+,(.*)\\Z')
PBRRTS_RE2 = re.compile('\\APBRRTS,(\\d+),(\\d+),(\\d+),([^,]*),(.*?)\\Z')
PBRSNP_RE = re.compile('\\APBRSNP,([^,]*),([^,]*),([^,]*),([^,]*)\\Z')
PBRTL_RE = re.compile('\\APBRTL,(\\d+),(\\d+),(\\d+).(\\d+).(\\d+),(\\d+):(\\d+):(\\d+),(\\d+):(\\d+):(\\d+)\\Z')
PBRWPS_RE = re.compile('\\APBRWPS,(\\d{2})(\\d{2}\\.\\d+),([NS]),(\\d{3})(\\d{2}\\.\\d+),([EW]),([^,]*),([^,]*),(\\d+)\\Z')

#
# Error
#

class Error(RuntimeError): pass
class TimeoutError(Error): pass
class ReadError(Error): pass
class WriteError(Error): pass

#
# SerialIO
#

class SerialIO:

  def __init__(self, filename):
    self.logger = logging.getLogger('%s.%s' % (__name__, filename))
    self.buffer = ''

  def __del__(self):
    self.close()

  def readline(self):
    if self.buffer == '':
      self.buffer = self.read(1024)
    if self.buffer[0] == XON or self.buffer[0] == XOFF:
      result = self.buffer[0]
      self.buffer = self.buffer[1:]
      self.logger.info('%s', result.encode('string_escape'), extra=dict(direction='read', x=True))
      return result
    else:
      result = ''
      while True:
	index = self.buffer.find('\n')
	if index == -1:
	  result += self.buffer
	  self.buffer = self.read(1024)
	else:
	  result += self.buffer[0:index + 1]
	  self.buffer = self.buffer[index + 1:]
	  self.logger.info('%s', result.encode('string_escape'), extra=dict(direction='read'))
	  return result

  def writeline(self, line):
    self.logger.info('%s', line.encode('string_escape'), extra=dict(direction='write'))
    self.write(line)

  def close(self):
    pass

  def flush(self):
    pass

  def read(self, n):
    raise NotImplementedError

  def write(self, data):
    raise NotImplementedError

#
# POSIXSerialIO
#

if os.name == 'posix':
  import select
  import tty

class POSIXSerialIO(SerialIO):

  def __init__(self, filename):
    SerialIO.__init__(self, filename)
    self.fd = os.open(filename, os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
    tty.setraw(self.fd)
    attr = tty.tcgetattr(self.fd)
    attr[tty.ISPEED] = attr[tty.OSPEED] = tty.B57600
    tty.tcsetattr(self.fd, tty.TCSAFLUSH, attr)

  def close(self):
    os.close(self.fd)

  def flush(self):
    tty.tcflush(self.fd, tty.TCIOFLUSH)

  def read(self, n):
    if select.select([self.fd], [], [], 1) == ([], [], []): raise TimeoutError()
    data = os.read(self.fd, n)
    if len(data) == 0: raise ReadError()
    return data

  def write(self, data):
    if os.write(self.fd, data) != len(data): raise WriteError()

#
# Route
#

class Route:

  def __init__(self, index, name, routepoints):
    self.index = index
    self.name = name
    self.routepoints = routepoints

#
# Routepoint
#

class Routepoint:

  def __init__(self, short_name, long_name):
    self.short_name = short_name
    self.long_name = long_name

#
# SNP
#

class SNP:

  def __init__(self, instrument, pilot_name, serial_number, software_version):
    self.instrument = instrument
    self.pilot_name = pilot_name
    self.serial_number = serial_number
    self.software_version = software_version

#
# Track
#

class Track:

  def __init__(self, count, index, datetime, duration, igc_filename=None):
    self.count = count
    self.index = index
    self.datetime = datetime
    self.duration = duration
    self.igc_filename = igc_filename

#
# Waypoint
#

class Waypoint:

  def __init__(self, lat, lon, short_name, long_name, alt):
    self.lat = lat
    self.lon = lon
    self.short_name = short_name
    self.long_name = long_name
    self.alt = alt

#
# Flytec
#

class Flytec:

  def __init__(self, io):
    self.io = io

  def ieach(self, command, re=None):
    try:
      self.io.writeline(nmea.encode(command))
      if self.io.readline() != XOFF: raise Error
      while True:
	line = self.io.readline()
	if line == XON:
	  break
	elif re is None:
	  yield line
	else:
	  m = re.match(nmea.decode(line))
	  if m is None: raise Error(line)
	  yield m
    except:
      self.io.flush()
      raise

  def none(self, command):
    for m in self.ieach(command):
      raise Error(m)

  def one(self, command, re=None):
    result = None
    for m in self.ieach(command, re):
      if not result is None: raise Error(m)
      result = m
    return result

  def pbrconf(self):
    self.none('PBRCONF,')

  def ipbrigc(self):
    return self.ieach('PBRIGC,')

  def pbrmemr(self, address, length):
    result = []
    first, last = address, address + length
    while first < last:
      m = self.one('PBRMEMR,%04X' % first, PBRMEMR_RE)
      # FIXME check returned address
      data = [i.decode('hex') for i in m.group(2).split(',')]
      result.extend(data)
      first += len(data)
    return result[:length]

  def ipbrrts(self):
    for l in self.ieach('PBRRTS,'):
      l = nmea.decode(l)
      m = PBRRTS_RE1.match(l)
      if m:
	index, count, name = int(m.group(1)), int(m.group(2)), m.group(3)
	if count == 1:
	  yield Route(index, name, [])
	else:
	  routepoints = []
      else:
	m = PBRRTS_RE2.match(l)
	if m:
	  index, count, routepoint_index = [int(i) for i in m.groups()[0:3]]
	  routepoint_short_name = m.group(4)
	  routepoint_long_name = m.group(5)
	  routepoints.append(Routepoint(routepoint_short_name, routepoint_long_name))
	  if routepoint_index == count - 1:
	    yield Route(index, name, routepoints)
	else:
	  raise Error(m)

  def pbrrts(self):
    return list(self.ipbrrts())

  def pbrsnp(self):
    return SNP(*self.one('PBRSNP,', PBRSNP_RE).groups())

  def pbrtl(self):
    tracks = []
    for m in self.ieach('PBRTL,', PBRTL_RE):
      count, index = [int(i) for i in m.groups()[0:2]]
      day, month, year, hour, minute, second = [int(i) for i in m.groups()[2:8]]
      _datetime = datetime.datetime(year + 2000, month, day, hour, minute, second)
      hours, minutes, seconds = [int(i) for i in m.groups()[8:11]]
      duration = datetime.timedelta(hours=hours, minutes=minutes, seconds=seconds)
      tracks.append(Track(count, index, _datetime, duration))
    date, index = None, 0
    for track in reversed(tracks):
      if track.datetime.date() == date:
	index += 1
      else:
	index = 1
      # FIXME calculate manufacturer code
      track.igc_filename = '%s-XXX-%s-%02d.IGC' % (track.datetime.strftime('%Y-%m-%d'), self.serial_number, index)
      date = track.datetime.date()
    return tracks

  def ipbrtr(self, index):
    return self.ieach('PBRTR,%02d' % index)

  def pbrtr(self, index):
    return list(self.ipbrtr(index))

  def ipbrwps(self):
    for m in self.ieach('PBRWPS,', PBRWPS_RE):
      lat = int(m.group(1)) + float(m.group(2)) / 60
      if m.group(3) == 'S': lat *= -1
      lon = int(m.group(4)) + float(m.group(5)) / 60
      if m.group(6) == 'W': lon *= -1
      short_name = m.group(7)
      long_name = m.group(8)
      alt = int(m.group(9))
      yield Waypoint(lat, lon, short_name, long_name, alt)

  def pbrwps(self):
    return list(self.ipbrwps())

  def pbrwpx(self, name=None):
    if name:
      self.zero('PBRWPX,%-17s' % name)
    else:
      self.zero('PBRWPX,')

  def __getattr__(self, attr):
    if attr == 'instrument':
      if not self.__dict__.has_key('_snp'): self._snp = self.pbrsnp()
      return self._snp.instrument
    elif attr == 'pilot_name':
      if not self.__dict__.has_key('_snp'): self._snp = self.pbrsnp()
      return self._snp.pilot_name.strip()
    elif attr == 'serial_number':
      if not self.__dict__.has_key('_snp'): self._snp = self.pbrsnp()
      return re.compile('\\A0+').sub('', self._snp.serial_number)
    elif attr == 'software_version':
      if not self.__dict__.has_key('_snp'): self._snp = self.pbrsnp()
      return self._snp.software_version
    elif attr == 'manufacturer':
      return MANUFACTURER[self.instrument]
    else:
      return None
