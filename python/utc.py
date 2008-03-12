import datetime

class UTC(datetime.tzinfo):
  """UTC"""

  def utcoffset(self, dt):
    return datetime.timedelta(0)

  def tzname(self):
    return "UTC"

  def dst(self, dt):
    return datetime.timedelta(0)
