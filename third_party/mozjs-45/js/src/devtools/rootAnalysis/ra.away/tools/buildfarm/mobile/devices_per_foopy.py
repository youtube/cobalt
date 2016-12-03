import json

STAGING_FOOPIES = ("foopy109", "foopy110", "foopy25", "foopy26")

f = open('devices.json', 'r')
j = json.load(f)
foopies = {}
nofoopy = {}
for key in j:
    foopy = j[key]["foopy"]
    if foopy != "None":
        if foopy in foopies:
            foopies[foopy].append(key)
        else:
            foopies[foopy] = [key]
    else:
        if "_comment" in j[key]:
            newkey = j[key]["_comment"]
        else:
            newkey = "None"
        if newkey in nofoopy:
            nofoopy[newkey] += 1
        else:
            nofoopy[newkey] = 1

prod_devices = 0
prod_foopies = 0
prod_text = []
stage_text = []
stage_devices = 0
stage_foopies = 0
for foopy in sorted(foopies.keys()):
    # exclude staging foopies
    if not foopy in STAGING_FOOPIES:
        prod_text.append(
            "  %s contains %s devices" % (foopy, len(foopies[foopy])))
        prod_devices += len(foopies[foopy])
        prod_foopies += 1
    if foopy in STAGING_FOOPIES:
        stage_text.append(
            "  %s contains %s devices" % (foopy, len(foopies[foopy])))
        stage_devices += len(foopies[foopy])
        stage_foopies += 1

unassigned_text = []
for unassigned in sorted(nofoopy.keys()):
    if unassigned is "None":
        unassigned_text.append("%4ld (With no Comment)" % nofoopy[unassigned])
    else:
        unassigned_text.append(
            "%4ld With Comment: %s" % (nofoopy[unassigned], unassigned))

print "PRODUCTION:"
print "\n".join(prod_text)
print "We have %s devices in %s foopies which means a ratio of %s devices per foopy" % \
      (prod_devices, prod_foopies, prod_devices / prod_foopies)
print
print "STAGING:"
print "\n".join(stage_text)
print "We have %s devices in %s foopies which means a ratio of %s devices per foopy" % \
      (stage_devices, stage_foopies, stage_devices / stage_foopies)
print
print "UNASSIGNED"
print "\n".join(unassigned_text)
