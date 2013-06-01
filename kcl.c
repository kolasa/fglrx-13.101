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

/** \brief KCL OS independent generic interface implementation  */

#include <linux/kernel.h>
#include <linux/sched.h>
#include "kcl_config.h"
#include "kcl_type.h"
#include "kcl.h"
/** \brief Send signal to a specified pid
 *  \param pid   thread group leader id 
 *  \param sig   signal to be send 
 *  \return None
 */
void ATI_API_CALL KCL_SEND_SIG(int pid, KCL_SIG sig)
{
    struct task_struct *group_leader = current->group_leader;
    if(pid == group_leader->pid)
    {
        send_sig((int)sig, group_leader, 1);   //1 -> SEND_SIG_PRIC 
    }    
}    



