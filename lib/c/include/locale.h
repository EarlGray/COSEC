#ifndef __COSEC_LOCALE_H__
#define __COSEC_LOCALE_H__

#define LC_ALL      2000
#define LC_COLLATE  2001
#define LC_CTYPE    2002
#define LC_MESSAGES 2003
#define LC_MONETARY 2004
#define LC_NUMERIC  2005
#define LC_TIME     2006

struct lconv {
    /* Numeric (nonmonetary) information */
    char *decimal_point;     /* Radix character */
    char *thousands_sep;     /* Separator for digit groups to left
                                of radix character */
    char *grouping; /* Each element is the number of digits in a
                       group; elements with higher indices are
                       further left.  An element with value CHAR_MAX
                       means that no further grouping is done.  An
                       element with value 0 means that the previous
                       element is used for all groups further left. */

    /* Remaining fields are for monetary information */

    char *int_curr_symbol;   /* First three chars are a currency symbol
                                from ISO 4217.  Fourth char is the
                                separator.  Fifth char is '\0'. */
    char *currency_symbol;   /* Local currency symbol */
    char *mon_decimal_point; /* Radix character */
    char *mon_thousands_sep; /* Like thousands_sep above */
    char *mon_grouping;      /* Like grouping above */
    char *positive_sign;     /* Sign for positive values */
    char *negative_sign;     /* Sign for negative values */
    char  int_frac_digits;   /* International fractional digits */
    char  frac_digits;       /* Local fractional digits */
    char  p_cs_precedes;     /* 1 if currency_symbol precedes a
                                positive value, 0 if succeeds */
    char  p_sep_by_space;    /* 1 if a space separates currency_symbol
                                from a positive value */
    char  n_cs_precedes;     /* 1 if currency_symbol precedes a
                                negative value, 0 if succeeds */
    char  n_sep_by_space;    /* 1 if a space separates currency_symbol
                                from a negative value */
    /* Positive and negative sign positions:
       0 Parentheses surround the quantity and currency_symbol.
       1 The sign string precedes the quantity and currency_symbol.
       2 The sign string succeeds the quantity and currency_symbol.
       3 The sign string immediately precedes the currency_symbol.
       4 The sign string immediately succeeds the currency_symbol. */
    char  p_sign_posn;
    char  n_sign_posn;
};

struct lconv *localeconv(void);

char *setlocale(int category, const char *locale);

#endif // __COSEC_LOCALE_H__
