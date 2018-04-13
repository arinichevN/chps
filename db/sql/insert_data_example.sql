delete from "prog";
INSERT INTO prog(id,description,check_interval_sec,enable,load) VALUES 
(1,'regulyator',5,1,1);
INSERT INTO prog(id,description,check_interval_sec,enable,load) VALUES 
(2,'model veschestva',5,1,1);
INSERT INTO prog(id,description,check_interval_sec,enable,load) VALUES 
(3,'modul vvoda',5,1,1);
INSERT INTO prog(id,description,check_interval_sec,enable,load) VALUES 
(4,'kommutator',5,1,1);

delete from "peer";
INSERT INTO "peer" VALUES('gwu18_1',49161,'127.0.0.1');
INSERT INTO "peer" VALUES('gwu22_1',49162,'127.0.0.1');
INSERT INTO "peer" VALUES('gwu18_2',49161,'127.0.0.1');
INSERT INTO "peer" VALUES('gwu22_2',49162,'127.0.0.1');
INSERT INTO "peer" VALUES('gwu74_1',49163,'127.0.0.1');
INSERT INTO "peer" VALUES('gwu59_1',49164,'127.0.0.1');
INSERT INTO "peer" VALUES('lck_1',49175,'127.0.0.1');
INSERT INTO "peer" VALUES('lgr_1',49172,'127.0.0.1');
INSERT INTO "peer" VALUES('alr_1',49174,'127.0.0.1');
INSERT INTO "peer" VALUES('chp_1',49176,'127.0.0.1');
INSERT INTO "peer" VALUES('chv_1',49177,'127.0.0.1');
INSERT INTO "peer" VALUES('regsmp_1',49192,'127.0.0.1');
INSERT INTO "peer" VALUES('regonf_1',49191,'127.0.0.1');
INSERT INTO "peer" VALUES('stp_1',49179,'127.0.0.1');
INSERT INTO "peer" VALUES('obj_1',49178,'127.0.0.1');
INSERT INTO "peer" VALUES('swr_1',49183,'127.0.0.1');
INSERT INTO "peer" VALUES('swf_1',49182,'127.0.0.1');

delete from "prog_peer";
INSERT INTO "prog_peer" VALUES(1,'obj_1');
INSERT INTO "prog_peer" VALUES(1,'obj_1');
INSERT INTO "prog_peer" VALUES(1,'obj_1');
INSERT INTO "prog_peer" VALUES(2,'obj_1');
INSERT INTO "prog_peer" VALUES(2,'regsmp_1');
INSERT INTO "prog_peer" VALUES(2,'regonf_1');
INSERT INTO "prog_peer" VALUES(3,'alp_1');
INSERT INTO "prog_peer" VALUES(3,'alr_1');
INSERT INTO "prog_peer" VALUES(3,'swf_1');




