#!/usr/bin/clitoris ## -*- shell-script -*-

## retake on tf.10.tst, idf should be there now
$ terms "${root}/test/12abrdg.txt" | tf list -f foo.tcb --idf
5	Vectron	2	4
6	Aktien	2	4
7	verkaufen	1	4
8	Für	1	4
9	die	1	2
9	die	5	2
6	Aktien	2	4
10	der	2	2
10	der	5	2
5	Vectron	2	4
11	AG	1	4
12	gibt	1	4
13	es	1	4
14	von	1	4
15	den	1	2
15	den	2	2
16	Analysten	1	2
16	Analysten	3	2
10	der	2	2
10	der	5	2
17	Bankgesellschaft	1	4
18	Berlin	1	4
19	eine	1	2
19	eine	2	2
20	Verkaufsempfehlung	1	4
$
