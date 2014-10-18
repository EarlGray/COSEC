#ifndef __COSEC_STDBOOL_H__
#define __COSEC_STDBOOL_H__

/*
 *  bool type
 */
typedef enum { false, true } bool;
#define true 1
#define false 0
#define not(b) (1-(b))

#endif // __COSEC_STDBOOL_H__
