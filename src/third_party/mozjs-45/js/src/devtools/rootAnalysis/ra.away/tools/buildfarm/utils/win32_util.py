import win32api
import win32con
import win32process
import win32event
import win32com.client
import ctypes
import time
import os


def which(prog):
    possible_ext = ('.cmd', '.bat', '.exe')
    for path in os.environ['PATH'].split(';'):
        full_path = os.path.join(path, prog)
        for ext in possible_ext:
            if os.path.isfile(full_path + ext):
                return prog + ext
    # Otherwise return as is
    return prog


def getChildrenPidsOfPid(pid):
    wmi = win32com.client.GetObject('winmgmts:')
    children = wmi.ExecQuery(
        'Select * from win32_process where ParentProcessId=%s' % pid)
    pids = []
    for proc in children:
        pids.append(proc.Properties_('ProcessId'))
    return pids


def kill(pid, graceperiod=5000):
    success = False
    for child in getChildrenPidsOfPid(pid):
        kill(child, graceperiod)
        time.sleep(5)
    handle = win32api.OpenProcess(win32con.PROCESS_ALL_ACCESS, False, pid)
    exitcode = win32process.GetExitCodeProcess(handle)
    if exitcode == win32con.STILL_ACTIVE:
        hKernel = win32api.GetModuleHandle("Kernel32")
        procExit = win32api.GetProcAddress(hKernel, "ExitProcess")
        hRemoteT = ctypes.windll.kernel32.CreateRemoteThread(handle.handle,
                                                             None, 0, procExit, ctypes.c_void_p(-1), 0, None)
        if hRemoteT:
            retval = win32event.WaitForSingleObject(handle, graceperiod)
            if retval != win32con.WAIT_OBJECT_0:
                win32api.TerminateProcess(handle, -1)
            win32api.CloseHandle(hRemoteT)
            success = True
    else:
        success = True
    win32api.CloseHandle(handle)
    return success
