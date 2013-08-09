glod (grokking lots of data)
============================

Taming copious amounts of data has become daily routine for many people
in various disciplines.  This toolbox (while still trying to find its
niche) focusses on preparing the data for further processing using other
tools or frameworks.

The toolset consists of various little command line utilities:

glep
----
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
Output terms occurring in the specified files, one term per line, and
different files separated by a form feed.

By default a term is made up of consecutive alphanumerical characters
but the precise alphabet can be specified:

    $ terms testfile1
    New
    virus
    found
    $ terms -c 'a-z.' testfile1
    ew
    virus
    found.

tf
--
Return the term frequency within a document (file or stdin).  When used
with a corpus it can turn text terms into numerical terms which is
useful for systems that are number based primarily.

Since `tf` does not massage the input in any way it needs the terms to
be counted one per line (like `terms` would produce).

    $ terms testfile1 | tf
    New	1
    virus	1
    found	1


Rationale
---------
The glod suite serves as an umbrella for tools that were/are actually
needed in a production environment but which are yet too trivial or
small to justify a full-fledged repository.

This is the primary reason for the seemingly odd coverage of problems,
and might be the reason for tools to appear and disappear willy-nilly.

