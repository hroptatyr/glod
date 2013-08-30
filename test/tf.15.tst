#!/usr/bin/clitoris ## -*- shell-script -*-

## retake on tf.10.tst, idf should be there now
$ terms "${root}/test/12abrdg.txt" | tf list -f foo.tcb --idf
3	Vectron	2	4
4	Aktien	2	4
5	verkaufen	1	4
6	Für	1	4
7	die	1	2
7	die	5	2
4	Aktien	2	4
8	der	2	2
8	der	5	2
3	Vectron	2	4
9	AG	1	4
10	gibt	1	4
11	es	1	4
12	von	1	4
13	den	1	2
13	den	2	2
14	Analysten	1	2
14	Analysten	3	2
8	der	2	2
8	der	5	2
15	Bankgesellschaft	1	4
16	Berlin	1	4
17	eine	1	2
17	eine	2	2
18	Verkaufsempfehlung	1	4
$
