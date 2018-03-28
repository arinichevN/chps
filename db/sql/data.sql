CREATE TABLE "prog"
(
    "id" INTEGER PRIMARY KEY NOT NULL,
    "description" TEXT NOT NULL,
    "check_interval_sec" INTEGER NOT NULL,
    "enable" INTEGER NOT NULL,
    "load" INTEGER NOT NULL
);
CREATE TABLE "peer" (
    "id" TEXT NOT NULL,
    "port" INTEGER NOT NULL,
    "ip_addr" TEXT NOT NULL
);
CREATE TABLE "prog_peer" (
    "prog_id" INTEGER NOT NULL,
    "peer_id" TEXT NOT NULL
);
