all: hgtool-nodeps.py gittool-nodeps.py retry-nodeps.py

hgtool-nodeps.py: hgtool.py
	PYTHONPATH=$(PWD)/../../lib/python python package-script.py -m util -m util.commands -m util.retry -m util.hg $< $@
	chmod +x $@

gittool-nodeps.py: gittool.py
	PYTHONPATH=$(PWD)/../../lib/python python package-script.py -m util -m util.file -m util.commands -m util.retry -m util.git $< $@
	chmod +x $@

retry-nodeps.py: retry.py
	PYTHONPATH=$(PWD)/../../lib/python:$(PWD) python package-script.py -m util -m util.file -m util.commands -m util.retry -m unix_util $< $@
	chmod +x $@
