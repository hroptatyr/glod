/* This is the Porter stemming algorithm, coded up in ANSI C by the
   author. It may be be regarded as canonical, in that it follows the
   algorithm presented in

   Porter, 1980, An algorithm for suffix stripping, Program, Vol. 14,
   no. 3, pp 130-137,

   only differing from it at the points maked --DEPARTURE-- below.

   See also http://www.tartarus.org/~martin/PorterStemmer

   The algorithm as described in the paper could be exactly replicated
   by adjusting the points of DEPARTURE, but this is barely necessary,
   because (a) the points of DEPARTURE are definitely improvements, and
   (b) no encoding of the Porter stemmer I have seen is anything like
   as exact as this version, even with the points of DEPARTURE!

   You can compile it on Unix with 'gcc -O3 -o stem stem.c' after which
   'stem' takes a list of inputs and sends the stemmed equivalent to
   stdout.

   The algorithm as encoded here is particularly fast.

   Release 1: was many years ago
   Release 2: 11 Apr 2013
       fixes a bug noted by Matt Patenaude <matt@mattpatenaude.com>,

       case 'o': if (ends("\03" "ion") && (b[j] == 's' || b[j] == 't')) break;
           ==>
       case 'o': if (ends("\03" "ion") && j >= 0 && (b[j] == 's' || b[j] == 't')) break;

       to avoid accessing b[-1] when the word in b is "ion".
*/
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* The main part of the stemming algorithm starts here. b is a buffer
   holding a word to be stemmed. The letters are in b[k0], b[k0+1] ...
   ending at b[k]. In fact k0 = 0 in this demo program. k is readjusted
   downwards as the stemming progresses. Zero termination is not in fact
   used in the algorithm.

   Note that only lower case sequences are stemmed. Forcing to lower case
   should be done before stem(...) is called.
*/

/* buffer for word to be stemmed */
static char *b;
/* j is a general offset into the string */
static ssize_t k;
static ssize_t k0;
static ssize_t j;

/* cons(i) is TRUE <=> b[i] is a consonant. */
static bool
cons(ssize_t i)
{
	switch (b[i]) {
	case 'a':
	case 'e':
	case 'i':
	case 'o':
	case 'u':
		return false;
	case 'y':
		return (i == k0) ? true : !cons(i - 1);
	default:
		return true;
	}
}

/* m() measures the number of consonant sequences between k0 and j. if c is
   a consonant sequence and v a vowel sequence, and <..> indicates arbitrary
   presence,

      <c><v>       gives 0
      <c>vc<v>     gives 1
      <c>vcvc<v>   gives 2
      <c>vcvcvc<v> gives 3
      ....
*/
static size_t
m(void)
{
	size_t n = 0U;
	ssize_t i = k0;

	while (true) {
		if (i > j) {
			return n;
		} else if (!cons(i)) {
			break; 
		}
		i++;
	}
	i++;
	while (true) {
		while (true) {
			if (i > j) {
				return n;
			} else if (cons(i)) {
				break;
			}
			i++;
		}
		i++;
		n++;
		while (true) {
			if (i > j) {
				return n;
			} else if (!cons(i)) {
				break;
			}
			i++;
		}
		i++;
	}
	/* not reached */
}

/* vowelinstem() is TRUE <=> k0,...j contains a vowel */
static bool
vowelinstem(void)
{
	int i;
	for (i = k0; i <= j; i++) {
		if (!cons(i)) {
			return true;
		}
	}
	return false;
}

/* doublec(l) is TRUE <=> l,(l-1) contain a double consonant. */
static bool
doublec(ssize_t l)
{
	if (l < k0 + 1) {
		return false;
	} else if (b[l] != b[l - 1]) {
		return false;
	}
	return cons(l);
}

/* cvc(i) is TRUE <=> i-2,i-1,i has the form consonant - vowel - consonant
   and also if the second c is not w,x or y. this is used when trying to
   restore an e at the end of a short word. e.g.

      cav(e), lov(e), hop(e), crim(e), but
      snow, box, tray.

*/
static bool
cvc(ssize_t i)
{
	if (i < k0 + 2U || !cons(i) || cons(i - 1) || !cons(i - 2)) {
		return false;
	} else {
		int ch = b[i];
		if (ch == 'w' || ch == 'x' || ch == 'y') {
			return false;
		}
	}
	return true;
}

/* ends(s) is TRUE <=> k0,...k ends with the string s. */
static bool
ends(const char *s)
{
	ssize_t length = s[0];

	if (s[length] != b[k]) {
		return false;
	} else if (length > k - k0 + 1) {
		return false;
	} else if (memcmp(b + k - length + 1, s + 1, length) != 0) {
		return false;
	}
	j = k - length;
	return true;
}

/* setto(s) sets (j+1),...k to the characters in the string s, readjusting
   k. */
static void
setto(const char *s)
{
	int length = s[0];

	memmove(b + j + 1, s + 1, length);
	k = j + length;
	return;
}

/* r(s) is used further down. */
static void
r(char *s)
{
	if (m() > 0U) {
		setto(s);
	}
	return;
}

/* step1ab() gets rid of plurals and -ed or -ing. e.g.

       caresses  ->  caress
       ponies    ->  poni
       ties      ->  ti
       caress    ->  caress
       cats      ->  cat

       feed      ->  feed
       agreed    ->  agree
       disabled  ->  disable

       matting   ->  mat
       mating    ->  mate
       meeting   ->  meet
       milling   ->  mill
       messing   ->  mess

       meetings  ->  meet

*/
static void
step1ab(void)
{
	if (b[k] == 's') {
		if (ends("\04" "sses")) {
			k -= 2;
		} else if (ends("\03" "ies")) {
			setto("\01" "i"); 
		} else if (b[k-1] != 's') {
			k--;
		}
	}
	if (ends("\03" "eed")) {
		if (m() > 0U) {
			k--;
		}
	} else if ((ends("\02" "ed") || ends("\03" "ing")) && vowelinstem()) {
		k = j;
		if (ends("\02" "at")) {
			setto("\03" "ate");
		} else if (ends("\02" "bl")) {
			setto("\03" "ble");
		} else if (ends("\02" "iz")) {
			setto("\03" "ize");
		} else if (doublec(k)) {
			k--;
			{
				int ch = b[k];
				if (ch == 'l' || ch == 's' || ch == 'z') {
					k++;
				}
			}
		} else if (m() == 1U && cvc(k)) {
			setto("\01" "e");
		}
	}
	return;
}

/* step1c() turns terminal y to i when there is another vowel in the stem. */
static void
step1c(void)
{
	if (ends("\01" "y") && vowelinstem()) {
		b[k] = 'i';
	}
	return;
}

/* step2() maps double suffices to single ones. so -ization ( = -ize plus
   -ation) maps to -ize etc. note that the string before the suffix must give
   m() > 0. */
static void
step2(void)
{
	switch (b[k-1]) {
	case 'a':
		if (ends("\07" "ational")) {
			r("\03" "ate");
			break;
		}
		if (ends("\06" "tional")) {
			r("\04" "tion");
			break;
		}
		break;
	case 'c':
		if (ends("\04" "enci")) {
			r("\04" "ence");
			break;
		}
		if (ends("\04" "anci")) {
			r("\04" "ance");
			break;
		}
		break;
	case 'e':
		if (ends("\04" "izer")) {
			r("\03" "ize");
			break;
		}
		break;
	case 'l':
		if (ends("\03" "bli")) {
			r("\03" "ble");
			break;
		}
		/*-DEPARTURE-*/
 
		if (ends("\04" "alli")) {
			r("\02" "al");
			break;
		}
		if (ends("\05" "entli")) {
			r("\03" "ent");
			break;
		}
		if (ends("\03" "eli")) {
			r("\01" "e");
			break;
		}
		if (ends("\05" "ousli")) {
			r("\03" "ous");
			break;
		}
		break;
	case 'o':
		if (ends("\07" "ization")) {
			r("\03" "ize");
			break;
		}
		if (ends("\05" "ation")) {
			r("\03" "ate");
			break;
		}
		if (ends("\04" "ator")) {
			r("\03" "ate");
			break;
		}
		break;
	case 's':
		if (ends("\05" "alism")) {
			r("\02" "al");
			break;
		}
		if (ends("\07" "iveness")) {
			r("\03" "ive");
			break;
		}
		if (ends("\07" "fulness")) {
			r("\03" "ful");
			break;
		}
		if (ends("\07" "ousness")) {
			r("\03" "ous");
			break;
		}
		break;
	case 't':
		if (ends("\05" "aliti")) {
			r("\02" "al");
			break;
		}
		if (ends("\05" "iviti")) {
			r("\03" "ive");
			break;
		}
		if (ends("\06" "biliti")) {
			r("\03" "ble");
			break;
		}
		break;
	case 'g':
		if (ends("\04" "logi")) {
			r("\03" "log");
			break;
		}
		/*-DEPARTURE-*/

	default:
		break;
	}
	return;
}

/* step3() deals with -ic-, -full, -ness etc. similar strategy to step2. */
static void
step3(void)
{
	switch (b[k]) {
	case 'e':
		if (ends("\05" "icate")) {
			r("\02" "ic");
			break;
		}
		if (ends("\05" "ative")) {
			r("\00" "");
			break;
		}
		if (ends("\05" "alize")) {
			r("\02" "al");
			break;
		}
		break;
	case 'i':
		if (ends("\05" "iciti")) {
			r("\02" "ic");
			break;
		}
		break;
	case 'l':
		if (ends("\04" "ical")) {
			r("\02" "ic");
			break;
		}
		if (ends("\03" "ful")) {
			r("\00" "");
			break;
		}
		break;
	case 's':
		if (ends("\04" "ness")) {
			r("\00" "");
			break;
		}
		break;
	default:
		break;
	}
	return;
}

/* step4() takes off -ant, -ence etc., in context <c>vcvc<v>. */
static void
step4(void)
{
	switch (b[k-1]) {
	case 'a':
		if (ends("\02" "al")) {
			break;
		}
		return;
	case 'c':
		if (ends("\04" "ance")) {
			break;
		}
		if (ends("\04" "ence")) {
			break;
		}
		return;
	case 'e':
		if (ends("\02" "er")) {
			break;
		}
		return;
	case 'i':
		if (ends("\02" "ic")) {
			break;
		}
		return;
	case 'l':
		if (ends("\04" "able")) {
			break;
		}
		if (ends("\04" "ible")) {
			break;
		}
		return;
	case 'n':
		if (ends("\03" "ant")) {
			break;
		}
		if (ends("\05" "ement")) {
			break;
		}
		if (ends("\04" "ment")) {
			break;
		}
		if (ends("\03" "ent")) {
			break;
		}
		return;
	case 'o':
		if (ends("\03" "ion") &&
		    j >= 0 && (b[j] == 's' || b[j] == 't')) {
			break;
		}
		if (ends("\02" "ou")) {
			break;
		}
		return;
		/* takes care of -ous */
	case 's':
		if (ends("\03" "ism")) {
			break;
		}
		if (ends("\03" "ise")) {
			break;
		}
		return;
	case 't':
		if (ends("\03" "ate")) {
			break;
		}
		if (ends("\03" "iti")) {
			break;
		}
		return;
	case 'u':
		if (ends("\03" "ous")) {
			break;
		}
		return;
	case 'v':
		if (ends("\03" "ive")) {
			break;
		}
		return;
	case 'z':
		if (ends("\03" "ize")) {
			break;
		}
		return;
	default:
		return;
	}
	if (m() > 1U) {
		k = j;
	}
	return;
}

/* step5() removes a final -e if m() > 1, and changes -ll to -l if
   m() > 1. */
static void
step5(void)
{
	j = k;

	if (b[k] == 'e') {
		size_t a = m();

		if (a > 1U || a == 1U && !cvc(k - 1)) {
			k--;
		}
	}
	if (b[k] == 'l' && doublec(k) && m() > 1U) {
		k--;
	}
	return;
}

/* In stem(p,i,j), p is a char pointer, and the string to be stemmed is from
   p[i] to p[j] inclusive. Typically i is zero and j is the offset to the last
   character of a string, (p[j+1] == '\0'). The stemmer adjusts the
   characters p[i] ... p[j] and returns the new end-point of the string, k.
   Stemming never increases word length, so i <= k <= j. To turn the stemmer
   into a module, declare 'stem' as extern, and delete the remainder of this
   file.
*/
static ssize_t
stem(char *s, size_t z)
{
	/* copy the parameters into statics */
	b = s;
	k = z - 1;
	k0 = 0;

	if (k <= k0 + 1) {
		/*-DEPARTURE-*/
		return k;
	}

	/* With this line, strings of length 1 or 2 don't go through the
	   stemming process, although no mention is made of this in the
	   published algorithm. Remove the line to match the published
	   algorithm. */
	step1ab();
	step1c();
	step2();
	step3();
	step4();
	step5();
	return k;
}


int
main(int argc, char *argv[])
{
	static char *line;
	static size_t llen;
	ssize_t nrd;

	/* just read the words from stdin */
	while ((nrd = getline(&line, &llen, stdin)) > 0) {
		ssize_t s;

		/* lower them */
		for (char *lp = line; lp < line + nrd - 1; lp++) {
			*lp = (char)tolower(*lp);
		}
		if ((s = stem(line, nrd - 1)) < 0) {
			continue;
		}
		line[s + 1U] = '\0';
		puts(line);
	}
	free(line);
	return 0;
}

/* porter-stemmer.c ends here */
