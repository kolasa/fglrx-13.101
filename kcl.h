/****************************************************************************
 *                                                                          *
 * Copyright 2010-2015 ATI Technologies Inc., Markham, Ontario, CANADA.     *
 * All Rights Reserved.                                                     *
 *                                                                          *
 * Your use and or redistribution of this software in source and \ or       *
 * binary form, with or without modification, is subject to: (i) your       *
 * ongoing acceptance of and compliance with the terms and conditions of    *
 * the ATI Technologies Inc. software End User License Agreement; and (ii)  *
 * your inclusion of this notice in any version of this software that you   *
 * use or redistribute.  A copy of the ATI Technologies Inc. software End   *
 * User License Agreement is included with this software and is also        *
 * available by contacting ATI Technologies Inc. at http://www.ati.com      *
 *                                                                          *
 ****************************************************************************/

/** \brief KCL OS independent generic interface declarations */

#ifndef KCL_H
#define KCL_H

#include "kcl_config.h"
#include "kcl_type.h"

/** KCL declarations */

//copy from linux kernel signal.h
typedef enum{
    KCL_SIG_KILL = 9,     //force kill
    KCL_SIG_SEGV = 11,     //segmentation fault 
    KCL_SIG_TERM = 15,     //termination 
}KCL_SIG;

extern void ATI_API_CALL KCL_SEND_SIG(int pid, KCL_SIG sig);


#endif

