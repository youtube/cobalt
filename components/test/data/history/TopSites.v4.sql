-- components_unittests --gtest_filter=TopSitesDatabaseTest.Version4
--
-- .dump of a version 4 "Top Sites" database.
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','4');
INSERT INTO "meta" VALUES('last_compatible_version','4');
CREATE TABLE top_sites (url LONGVARCHAR PRIMARY KEY,url_rank INTEGER ,title LONGVARCHAR,redirects LONGVARCHAR);
INSERT INTO "top_sites" VALUES('http://www.google.com/chrome/intl/en/welcome.html',1,'Welcome to Chromium','http://www.google.com/chrome/intl/en/welcome.html');
INSERT INTO "top_sites" VALUES('https://chrome.google.com/webstore?hl=en',2,'Chrome Web Store','https://chrome.google.com/webstore?hl=en');
INSERT INTO "top_sites" VALUES('http://www.google.com/',0,'Google','https://www.google.com/ http://www.google.com/');
COMMIT;
