glod (grokking lots of data)
============================

Taming copious amounts of data has become daily routine for many people
in various disciplines.  This toolbox (while still trying to find its
niche) focusses on preparing the data for further processing using other
tools or frameworks.

The toolset consists of various little command line utilities.

Rationale
---------
The glod suite serves as an umbrella for tools that were/are actually
needed in a production environment but which are yet too trivial or
small to justify a full-fledged repository.

This is the primary reason for the seemingly odd coverage of problems,
and might be the reason for tools to appear and disappear willy-nilly.

All tools deliberately ignore system-wide or user-specific localisation
settings (locales)!  This (and of course speed) sets glod apart from
tools like [PRETO][1], [JPreText][3] or [OpenRefine][2].

Moreover, most of the tools deliberately sacrifice portability for speed
on the actual production platform (which is 64bit AVX2 Intel).  This
goes as far as using every trick in the book, e.g. Cilk, nested
functions, assembler-backed co-routines, automatic CPU dispatch, and
more.  The downside, obviously, is that *underfeatured* compilers (yes,
clang, looking at you) won't be able to build half the tools.


glep
----
A multi-pattern grep.
Report matching patterns in files.
All patterns are looked for in parallel in all of the specified files.

Matching files and patterns are printed to stdout (separated by tabs):

    $ cat pats
    "virus"
    "glucose"
    "pound"
    "shelf"
    $ glep -f pats testfile1 testfile2
    virus	testfile1
    shelf	testfile2
    pound	testfile2


terms
-----
A fast text file tokeniser.
Output terms occurring in the specified files, one term per line, and
different files separated by a form feed.

A term (by our definition) is a sequence of alphanumerical characters
that can be interluded (but not prefixed or suffixed) by punctuation
characters.

    $ terms testfile1
    New
    virus
    found

Output of the `terms` utility can be fed into other tools that follow
the bag-of-words approach.  For instance to get a frequency vector in no
time:

    $ cat testfile1 | terms | sort | uniq -c
          1 New
          1 found
          1 virus

Or to assign a numeric mapping:

    $ cat testfile1 | terms | sort -u | nl
         1  New
         2  found
         3  virus

The `terms` utility is meant for bulk operations on corpora of utf8
encoded text files without language labels or other forms of
preclustering.

System-wide or local i18n settings are explicitly ignored!  This might
lead to complications when mixing glod tools with other preprocessing
tools.


enum
----

Enumerate terms from stdin.  This tool reads strings, one per line, and
assigns them an integer.  Much like an SQL SERIAL.  Consider

    $ cat testfile
    this
    is
    this
    test
    $

and now enumerating the lines

    $ enum < testfile
    1
    2
    1
    3
    $


  [1]: http://code.google.com/p/preto/
  [2]: http://openrefine.org/
  [3]: http://sites.labic.icmc.usp.br/torch/msd2011/jpretext/
