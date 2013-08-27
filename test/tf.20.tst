#!/usr/bin/clitoris ## -*- shell-script -*-

## retake on tf.10.tst, idf should be there now
$ terms "${root}/test/12abrdg.txt" | tf idf -f foo.tcb --top 5
5	0.81093
6	0.81093
10	0.81093
7	0.405465
8	0.405465

$
