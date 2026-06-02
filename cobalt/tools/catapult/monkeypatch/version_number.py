import re


def MonkeyPatchVersionNumber(self):
  print(f'Monkey-patching GetChromeMajorNumber')
  resp = self.GetVersion()
  if 'Protocol-Version' in resp:
    major_number_match = re.search(r'Cobalt/(\d+)', resp['User-Agent'])
    if major_number_match:
      # Simply return something believable
      return 114
  return 0
