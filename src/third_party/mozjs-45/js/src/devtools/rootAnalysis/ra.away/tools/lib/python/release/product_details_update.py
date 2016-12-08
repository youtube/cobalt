from jinja2 import Environment, FileSystemLoader
import os
import re
import time
from build.versions import ANY_VERSION_REGEX


PRODUCT_VERSION_TEMPLATES = {
    "mobile": {
        "aurora": {
            "version_template": "mobile_alpha_version.php.tpl",
        },
        "beta": {
            "version_template": "mobile_beta_version.php.tpl",
            "history_info": {
                "template": "mobileHistory.class.php",
                "marker": "NEXT_DEVELOPMENT",
            },
        },
        "major": {
            "version_template": "mobile_latest_version.php.tpl",
            "history_info": {
                "template": "mobileHistory.class.php",
                "marker": "NEXT_MAJOR"
            },
        },
        "minor": {
            "version_template": "mobile_latest_version.php.tpl",
            "history_info": {
                "template": "mobileHistory.class.php",
                "marker": "NEXT_STABILITY"
            },
        },
    },

    "firefox": {
        "aurora": {
            "version_template": "FIREFOX_AURORA.php.tpl",
        },
        "beta": {
            "version_template": "LATEST_FIREFOX_DEVEL_VERSION.php.tpl",
            "version_template_2": "LATEST_FIREFOX_RELEASED_DEVEL_VERSION.php.tpl",
            "history_info": {
                "template": "firefoxHistory.class.php",
                "marker": "NEXT_DEVELOPMENT",
            },
        },
        "major": {
            "version_template": "LATEST_FIREFOX_VERSION.php.tpl",
            "version_template_2": "LATEST_FIREFOX_RELEASED_VERSION.php.tpl",
            "history_info": {
                "template": "firefoxHistory.class.php",
                "marker": "NEXT_MAJOR"
            },
        },
        "minor": {
            "version_template": "LATEST_FIREFOX_VERSION.php.tpl",
            "version_template_2": "LATEST_FIREFOX_RELEASED_VERSION.php.tpl",
            "history_info": {
                "template": "firefoxHistory.class.php",
                "marker": "NEXT_STABILITY"
            },
        },
        "esr": {
            "version_template": "FIREFOX_ESR.php.tpl",
            "version_template_2": "FIREFOX_ESR_NEXT.php.tpl",
            "history_info": {
                "template": "firefoxHistory.class.php",
                "marker": "NEXT_STABILITY"
            },
        },
    },

    "thunderbird": {
        # Thundebird is not doing anything about beta
        "major": {
            "version_template": "LATEST_THUNDERBIRD_VERSION.php.tpl",
            "history_info": {
                "template": "thunderbirdHistory.class.php",
                "marker": "NEXT_MAJOR"
            },
        },
        "minor": {
            "version_template": "LATEST_THUNDERBIRD_VERSION.php.tpl",
            "history_info": {
                "template": "thunderbirdHistory.class.php",
                "marker": "NEXT_STABILITY"
            },
        },
    },

}


def updateVersionTemplate(targetSVNDirectory, templateFile, version):
    """
    Templates are stored in dedicated directory. use jinja to update them
    """
    templatesDir = os.path.join(targetSVNDirectory, "templates")

    env = Environment(loader=FileSystemLoader(templatesDir))

    if not os.path.isfile(os.path.join(templatesDir, templateFile)):
        raise Exception("Could not find templateFile: " + templateFile)

    template = env.get_template(templateFile)
    output_from_parsed_template = template.render(VERSION=version)

    # Save the results
    targetFile = os.path.join(targetSVNDirectory, templateFile.replace(".tpl", ""))
    with open(targetFile, "wb") as fh:
        fh.write(output_from_parsed_template)
    fh.close()


def addNewVersion(targetSVNDirectory, category, filename, version):
    """
    We cannot use a template mechanism for history file
    because we need to keep the markers
    Do it by hand
    """
    sourceFile = os.path.join(targetSVNDirectory, "history", filename)
    f = open(sourceFile, 'r')
    filedata = f.read()
    f.close()

    indent = "            "
    toreplace = "// " + category
    newVersion = "'" + version + "' => '" + time.strftime("%Y-%m-%d") + "',"

    if newVersion in filedata:
        print "New version '" + newVersion + "' already existing in the file " + filename
        return

    newdata = filedata.replace(toreplace, newVersion + "\n" + indent + toreplace)
    targetFile = os.path.join(targetSVNDirectory, "history", filename)
    f = open(targetFile, 'w')
    f.write(newdata)
    f.close()


def getTypeFromVersion(version):
    """
    From the version, return the code name of the version
    This is going to be used to search in the datastructure
    """
    m = re.match(ANY_VERSION_REGEX, version)
    if m and m.groups()[2] == "a":
        return "aurora"
    if m and m.groups()[2] == "b":
        return "beta"
    if m and m.groups()[3] == "esr":
        return "esr"

    if m and all(e is None for e in m.groups()[1:]):
        # only groups()[0] is not None
        # either major or minor release
        if m.groups()[0].count(".") > 1:
            return "minor"
        else:
            return "major"


def updateProductDetailFiles(targetSVNDirectory, product, version):
    """
    Update the various files
    """
    type_ = getTypeFromVersion(version)

    info = PRODUCT_VERSION_TEMPLATES.get(product, {}).get(type_)
    template = info["version_template"]

    history_info = PRODUCT_VERSION_TEMPLATES.get(product, {}).get(type_).get("history_info")

    if template:
        updateVersionTemplate(targetSVNDirectory, template, version)

    if "version_template_2" in info:
        # Some variables are stored in two files, update the second file
        template_2 = info["version_template_2"]
        if template_2:
            updateVersionTemplate(targetSVNDirectory, template_2, version)

    if type_ == "esr":
        # Strip the 'esr' from the version. It is not used in the history
        version = version.replace("esr", "")

    if history_info:
        addNewVersion(targetSVNDirectory, history_info["marker"], history_info["template"], version)
