from release.product_details_update import updateProductDetailFiles, getTypeFromVersion
#from util.commands import run_cmd
#from util.svn import exportJSON
import os
import tempfile
import unittest
import mock
import shutil


class TestProductDetails(unittest.TestCase):

    targetSVNDirectory = ""
    tempDir = ""

    def setUp(self):
        self.tempDir = tempfile.mkdtemp()
        self.targetSVNDirectory = os.path.join(self.tempDir, "product-details.svn")
        testDir = os.path.join(os.path.dirname(__file__), "product-details.svn")
        shutil.copytree(testDir, self.targetSVNDirectory)

    # Disable because it requires PHP 5.5, and Travis only has 5.3.
#    def test_export_json(self):
#        with mock.patch('time.strftime') as t:
#            t.return_value = "2014-09-06"
#
#            updateProductDetailFiles(self.targetSVNDirectory, "fennec", "32.0b3")
#            updateProductDetailFiles(self.targetSVNDirectory, "fennec", "42.0a2")
#            exportJSON(self.targetSVNDirectory)
#            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "json/mobile_details.json"), '"alpha_version": "42.0a2",')
#            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "json/mobile_details.json"), '"beta_version": "32.0b3",')
#            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "json/mobile_history_development_releases.json"), '"32.0b3": "2014-09-06"')

    def ContentCheckfile(self, filename, value):
        assert value in open(filename).read()

    def test_product_detail_fennec_aurora(self):
        updateProductDetailFiles(self.targetSVNDirectory, "mobile", "42.0a2")
        self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "mobile_alpha_version.php"), "'42.0a2'")

    def test_product_detail_firefox_aurora(self):
        updateProductDetailFiles(self.targetSVNDirectory, "firefox", "43.0a2")
        self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "FIREFOX_AURORA.php"), "'43.0a2'")

    def test_product_detail_fennec_beta(self):
        with mock.patch('time.strftime') as t:
            t.return_value = "2014-09-05"
            updateProductDetailFiles(self.targetSVNDirectory, "mobile", "32.0b3")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "mobile_beta_version.php"), "'32.0b3'")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "history/mobileHistory.class.php"), "'32.0b3' => '2014-09-05',")

    def test_product_detail_firefox_beta(self):
        with mock.patch('time.strftime') as t:
            t.return_value = "2014-09-06"
            updateProductDetailFiles(self.targetSVNDirectory, "firefox", "33.0b12")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "LATEST_FIREFOX_DEVEL_VERSION.php"), "'33.0b12'")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "LATEST_FIREFOX_RELEASED_DEVEL_VERSION.php"), "'33.0b12'")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "history/firefoxHistory.class.php"), "'33.0b12' => '2014-09-06',")


# Thunderbird is not doing anything with beta
# def test_product_detail_thunderbird_beta(self):
#     updateProductDetailFiles(self.targetSVNDirectory, "thunderbird", "32.0b3")
#     self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "/mobile_beta_version.php", "'32.0b3'")
#     self.ContentCheckfileRegexp(os.path.join(self.targetSVNDirectory, "/history/thunderbirdHistory.class.php", "'32.0b3' => '\d+-\d+-\d+',")

    def test_product_detail_fennec_release(self):
        with mock.patch('time.strftime') as t:
            t.return_value = "2014-09-07"

            updateProductDetailFiles(self.targetSVNDirectory, "mobile", "32.0")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "mobile_latest_version.php"), "'32.0'")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "history/mobileHistory.class.php"), "'32.0' => '2014-09-07',")
            updateProductDetailFiles(self.targetSVNDirectory, "mobile", "31.0.1")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "mobile_latest_version.php"), "'31.0.1'")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "history/mobileHistory.class.php"), "'31.0.1' => '2014-09-07',")

    def test_product_detail_firefox_release(self):
        with mock.patch('time.strftime') as t:
            t.return_value = "2014-09-08"

            updateProductDetailFiles(self.targetSVNDirectory, "firefox", "33.0")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "LATEST_FIREFOX_RELEASED_VERSION.php"), "'33.0'")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "LATEST_FIREFOX_VERSION.php"), "'33.0'")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "history/firefoxHistory.class.php"), "'33.0' => '2014-09-08',")
            updateProductDetailFiles(self.targetSVNDirectory, "firefox", "34.0.2")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "LATEST_FIREFOX_RELEASED_VERSION.php"), "'34.0.2'")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "LATEST_FIREFOX_VERSION.php"), "'34.0.2'")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "history/firefoxHistory.class.php"), "'34.0.2' => '2014-09-08',")

    def test_product_detail_thunderbird_release(self):
        with mock.patch('time.strftime') as t:
            t.return_value = "2014-09-10"

            updateProductDetailFiles(self.targetSVNDirectory, "thunderbird", "33.0")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "LATEST_THUNDERBIRD_VERSION.php"), "'33.0'")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "history/thunderbirdHistory.class.php"), "'33.0' => '2014-09-10',")
            updateProductDetailFiles(self.targetSVNDirectory, "thunderbird", "34.0.2")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "LATEST_THUNDERBIRD_VERSION.php"), "'34.0.2'")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "history/thunderbirdHistory.class.php"), "'34.0.2' => '2014-09-10',")

    def test_product_detail_firefox_esr(self):
        with mock.patch('time.strftime') as t:
            t.return_value = "2014-09-11"

            updateProductDetailFiles(self.targetSVNDirectory, "firefox", "51.1.0esr")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "FIREFOX_ESR.php"), "'51.1.0esr'")
            self.ContentCheckfile(os.path.join(self.targetSVNDirectory, "history/firefoxHistory.class.php"), "'51.1.0' => '2014-09-11',")

    def test_getTypeFromVersion(self):
        assert getTypeFromVersion("23.0a2") == "aurora"
        assert getTypeFromVersion("32.0b3") == "beta"
        assert getTypeFromVersion("54.0") == "major"
        assert getTypeFromVersion("33.0.1") == "minor"
        assert getTypeFromVersion("54.0esr") == "esr"
        assert getTypeFromVersion("54.1.0esr") == "esr"

    # Disable because PHP is too old on Travis.
    #def test_product_detail_sanity_check(self):
    #    run_cmd(["php", os.path.join(self.targetSVNDirectory, "sanity_check.php")])

    def tearDown(self):
        shutil.rmtree(self.tempDir)
