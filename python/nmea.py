import re

ENCODE_RE = re.compile('\\A[\x20-\x7f]{1,79}\\Z')
DECODE_RE = re.compile('\\A\\$(.{1,79})\\*([0-9A-F]{2})\r\n\\Z')

class Error(RuntimeError):
  pass

class EncodeError(Error):
  pass

class DecodeError(Error):
  pass

def encode(input):
  if not ENCODE_RE.match(input): raise EncodeError(input)
  checksum = 0
  for c in input: checksum ^= ord(c)
  return '$%s*%02X\r\n' % (input, checksum)

def decode(input):
  m = DECODE_RE.match(input)
  if not m: raise DecodeError(input)
  checksum = 0
  for c in m.group(1): checksum ^= ord(c)
  if checksum != ord(m.group(2).decode('hex')): raise DecodeError(input)
  return m.group(1)
