#if !defined INCLUDED_ftab_h_
#define INCLUDED_ftab_h_

#define MAX_LENGTH	(15)
#if defined OPTIM_SIZE
# define FACC_1ST	(1)
# define FACC_LST	(127)
#else  /* !OPTIM_SIZE */
# define FACC_1ST	(' ')
# define FACC_LST	(128)
#endif	/* OPTIM_SIZE */

/* this may be variable */
typedef __facc_bmsk_t *__facc_btbl_t;

/* length table */
typedef __facc_bmsk_t __facc_ltbl_t[MAX_LENGTH + 1];

/* indirection table, for size improvements, full ascii? */
typedef unsigned char __facc_meta_t[128];

#if defined OPTIM_SIZE
# define facc_get_meta(x)	((unsigned int)(__facc_meta[(x)]))
#else	/* !OPTIM_SIZE */
# define facc_get_meta(x)	((unsigned int)((x) - FACC_1ST))
#endif	/* OPTIM_SIZE */

#define facc_get_bmsk(tbl, c)	(tbl[facc_get_meta(c)])
#define facc_get_lmsk(len)	(__facc_ltbl[(len)])

#endif	/* INCLUDED_ftab_h_ */
