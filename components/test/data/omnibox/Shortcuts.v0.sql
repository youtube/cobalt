PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE omni_box_shortcuts ( id VARCHAR PRIMARY KEY, text VARCHAR, url VARCHAR, contents VARCHAR, contents_class VARCHAR, description VARCHAR, description_class VARCHAR, last_access_time INTEGER, number_of_hits INTEGER, fill_into_edit VARCHAR, transition INTEGER, type INTEGER, keyword VARCHAR);
INSERT INTO "omni_box_shortcuts" VALUES('34A7401D-0DC5-4A8F-A6B5-3FA4FF786C42','sha','http://shacknews.com/','shacknews.com','0,1','Video Game News and Features - Video Game News, Videos, and File Downloads for PC and Console Games at Shacknews.com','0,0',13024194201806179,3,'http://shacknews.com',1,2,NULL);
INSERT INTO "omni_box_shortcuts" VALUES('9EA31BB8-8528-4AE0-A6B1-FD06458DFC31','shacknews','http://shacknews.com/','shacknews.com','0,1','Video Game News and Features - Video Game News, Videos, and File Downloads for PC and Console Games at Shacknews.com','0,0',13024456716613368,4,'http://shacknews.com',1,2,NULL);
INSERT INTO "omni_box_shortcuts" VALUES('CD853DC4-7C4E-4E64-903D-A4BBCAA410F9','shacknews.com','http://shacknews.com/','shacknews.com','0,1','Video Game News and Features - Video Game News, Videos, and File Downloads for PC and Console Games at Shacknews.com','0,0',13024196901413541,1,'http://shacknews.com',1,2,NULL);
INSERT INTO "omni_box_shortcuts" VALUES('377314F7-E3AB-4264-8F3F-3404BE48DADB','echo echo','chrome-extension://cedabbhfglmiikkmdgcpjdkocfcmbkee/?q=echo','Run Echo command: echo','0,0','Echo','0,4',13025401559133998,2,'http://shacknews.com',1,2,NULL);
INSERT INTO "omni_box_shortcuts" VALUES('BCE200CA-01E9-4A2F-B5EC-2D561DDFBE41','echo','chrome-extension://cedabbhfglmiikkmdgcpjdkocfcmbkee/?q=frobber','Run Echo command: frobber','0,0','Echo','0,4',13025413423801769,2,'http://shacknews.com',1,2,NULL);
INSERT INTO "omni_box_shortcuts" VALUES('B07F4744-F008-3C87-37E4-9D284AD9A697','yahoo.com other keyword','http://search.yahoo.com/search?ei=UTF-8&fr=crmas&p=other+keyword','other keyword','0,0','Yahoo! Search','0,4',13036572535614255,1,'yahoo.com other keyword',9,9,'yahoo.com');
-- BOOKMARK_TITLE (type = 12)
INSERT INTO "omni_box_shortcuts" VALUES('9CBB669D-B7F0-5AFF-D808-EFFF7028B159','rdt','http://www.reddit.com/','www.reddit.com/','0,1','rdt','0,0',13036574229966419,1,'www.reddit.com/',1,12,'');
COMMIT;
