#!/usr/bin/clitoris ## -*- shell-script -*-

## retake on tf.10.tst, idf should be there now
$ terms "${root}/test/12abrdg.txt" | tf idf -f foo.tcb --top 5
3	0.81093
4	0.81093
8	0.81093
5	0.405465
6	0.405465

$
