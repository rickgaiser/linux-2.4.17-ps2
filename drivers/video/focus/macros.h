//	macros.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines some common comparison macros.

#ifndef __MACROS_H__
#define __MACROS_H__


#undef		max
#undef		min
#define		max(a, b)		((a) > (b) ? (a) : (b))
#define		min(a, b)		((a) < (b) ? (a) : (b))

#undef range_limit
#define range_limit(val,min_val,max_val) (max((min_val),min((val),(max_val))))


#endif
