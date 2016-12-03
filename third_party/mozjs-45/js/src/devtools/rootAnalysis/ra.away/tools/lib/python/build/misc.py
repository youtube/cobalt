from os import path

from util.commands import remove_path


def cleanupObjdir(sourceRepo, objdir, appName):
    remove_path(path.join(sourceRepo, objdir, "dist", "upload"))
    remove_path(path.join(sourceRepo, objdir, "dist", "update"))
    remove_path(path.join(sourceRepo, objdir, appName, "locales", "merged"))
