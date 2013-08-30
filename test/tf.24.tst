#!/usr/bin/clitoris ## -*- shell-script -*-

## retake on tf.10.tst, idf should be there now
$ terms "${root}/test/12abrdg.txt" | tf idf -f foo.tcb --top 3 --augmented
3	0.405465
4	0.405465
8	0.405465

$
