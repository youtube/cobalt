all: compare-mozconfigs-nodeps.py

compare-mozconfigs-nodeps.py: compare-mozconfigs.py
	PYTHONPATH=$(PWD)/../../lib/python python $(PWD)/../../buildfarm/utils/package-script.py -m release -m release.paths -m release.info -m util -m util.commands -m util.retry -m util.hg -m release.sanity $< $@
	chmod +x $@

clean:
	rm -f compare-mozconfigs-nodeps.py
