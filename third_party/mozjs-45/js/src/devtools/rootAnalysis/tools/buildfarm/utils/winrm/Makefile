winrm.exe: rm.obj
	link rm.obj -out:winrm.exe

rm.obj: rm.cpp
	cl -c rm.cpp -out:rm.obj

test: winrm.exe
	@echo Testing single file operations
	touch norm ro rosys sys
	attrib +s sys
	attrib +r ro
	attrib +r +s rosys
	attrib
	winrm.exe -v -f norm ro rosys sys
	@echo Testing single and empty directory operations
	mkdir testdir
	winrm.exe -r -v testdir
	@echo Testing a single level of recursive delete
	mkdir test
	touch test/file1 test/file2
	winrm.exe -r -f -v test
	@echo Testing arbitrary levels of recursive delete
	mkdir -p dir1/dir2/dir3
	touch dir1/file1 dir1/dir2/file2 dir1/dir2/dir3/file3
	winrm.exe -v -r dir1
	@echo Completed successfully
	

clean:
	rm -rf rm.obj winrm.exe

.PHONY: clean test
