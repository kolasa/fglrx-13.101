/****************************************************************************
 *                                                                          *
 * Copyright 2010-2011 ATI Technologies Inc., Markham, Ontario, CANADA.     *
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

/** \brief Implementation of KCL IOMMU supporting interfaces
 *
 * CONVENTIONS
 *
 * Public symbols:
 * - prefixed with KCL_IOMMU
 * - are not static
 * - declared in the corresponding header
 *
 * Private symbols:
 * - prefixed with kcl
 * - are static
 * - not declared in the corresponding header
 *
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
#include <generated/autoconf.h>
#else
#include <linux/autoconf.h>
#endif
#include <linux/pci.h>

#include "kcl_config.h"
#include "kcl_osconfig.h"
#include "kcl_type.h"
#include "kcl_iommu.h"
#include "kcl_debug.h"
#include "firegl_public.h"

#if defined(CONFIG_AMD_IOMMU_V2) || defined(CONFIG_AMD_IOMMU_V2_MODULE)
#include <linux/amd-iommu.h>
#include <linux/kthread.h>
#define IOMMUV2_SUPPORT  1

#endif

/** \brief Registers a peripheral device to the IOMMU driver.  
 *  \param pcidev [in] PCI device handle
 *  \param pPasids [out] num of maximum PASIDs supported.   
  * \return 0 success, failed other wise. 
 */
int ATI_API_CALL KCL_IOMMU_InitDevice( KCL_PCI_DevHandle pcidev,KCL_IOMMU_info_t* pInfo)
{
#ifdef IOMMUV2_SUPPORT 
    struct amd_iommu_device_info info;
    struct pci_dev* pdev = (struct pci_dev*)pcidev;
    memset(&info, 0 , sizeof(info));
    if(amd_iommu_device_info(pdev, &info))
    {
        return  -1;
    }
    pInfo->max_pasids = info.max_pasids;
    pInfo->flags.raw = info.flags;

    if(pInfo->flags.f.ats_sup && pInfo->flags.f.pri_sup && pInfo->flags.f.pasid_sup &&  pInfo->max_pasids)
    {
        int i ; 
        for(i = 0; i < sizeof(pInfo->erratum_mask)* 8;++i)    
        {
            if ( (pInfo->erratum_mask >> i) & 1 )
            {
                amd_iommu_enable_device_erratum(pdev, i);
            }
        }    
        return amd_iommu_init_device(pdev,info.max_pasids); 
    }    
#endif
    return -1;
}

/** \brief Unregisters a peripheral device from the IOMMU
 *  \param pcidev [in] PCI device handle
 *  
 */
void ATI_API_CALL KCL_IOMMU_FreeDevice( KCL_PCI_DevHandle pcidev)
{
#ifdef IOMMUV2_SUPPORT 
    struct pci_dev* pdev = (struct pci_dev*)pcidev;
    amd_iommu_free_device(pdev);
#endif    
}

#ifdef IOMMUV2_SUPPORT 
//Call back function from iommu driver when OS could not successfully
//resolve an IO page-fault signaled by our GPU via PRI request
static int kcl_iommu_invalid_pri_request( struct pci_dev* pdev,
                                          int  pasid,
                                          unsigned long fault_addr,
                                          unsigned int perm)
{
    KCL_IOMMU_req_perm_t kcl_perm;
    kcl_perm.all = perm; 
    return libip_iommu_invalid_pri_request( (KCL_PCI_DevHandle)pdev, pasid, fault_addr, kcl_perm); 
}

//Register a call-back for invalidating a pasid context. This call-back is
//invoked when the IOMMUv2 driver needs to invalidate a PASID context, for example
//because the task that is bound to that context is about to exit but the unbind_pasid is not been called yet
static void kcl_iommu_invalidate_ctx( struct pci_dev* pdev,int  pasid)
{
    libip_iommu_invalidate_pasid_ctx((KCL_PCI_DevHandle)pdev, pasid);
}
#endif

/** \brief Binds a peripheral's execution context identified by a PASID 
 *         to a process' virtual address space.   
 *  \param pcidev [in] PCI device handle
 *  \param pid [in] process ID of thread group leader.(First thread of the process)
 *  \param pasid [in]  specified PASID.   
  * \return 0 success, failed other wise. 
 */
int ATI_API_CALL KCL_IOMMU_BindPasid( KCL_PCI_DevHandle pcidev,int pid,int pasid)
{
    int ret = 0;
#ifdef IOMMUV2_SUPPORT
    struct pci_dev* pdev = (struct pci_dev*)pcidev;
    struct task_struct *group_leader = current->group_leader;
    if(pid == group_leader->pid)
    {
        ret =  amd_iommu_bind_pasid( pdev,
                                     pasid,
                                     group_leader );  //task of thread group leader
    }

    if(ret)
    {
        KCL_DEBUG_ERROR("pid:0x%x, group_leader id:0x%x, pasid:0x%x, ret:0x%x.\n", pid,group_leader->pid,pasid,ret); 
        return  ret;
    }    

    //register call back on invalid PPR. 
    ret = amd_iommu_set_invalid_ppr_cb( pdev,(amd_iommu_invalid_ppr_cb)kcl_iommu_invalid_pri_request);
    if(ret)
    {
        KCL_DEBUG_ERROR("register invalid PPR call back failed, pid:0x%x, group_leader id:0x%x, pasid:0x%x. ret:0x%x .\n", pid,group_leader->pid,pasid,ret); 
        return  ret;
    }    

    //register call back for invalidating a pasid contect . 
    ret = amd_iommu_set_invalidate_ctx_cb( pdev,(amd_iommu_invalidate_ctx)kcl_iommu_invalidate_ctx); 
    if(ret)
    {
        KCL_DEBUG_ERROR("register invalid pasid context call back failed, pid:0x%x, group_leader id:0x%x, pasid:0x%x. ret:0x%x .\n", pid,group_leader->pid,pasid,ret); 
        return  ret;
    }    

#endif    
    return ret;
}

/** \brief Unbinds a previously bound execution context from a device
 *  \param pcidev [in] PCI device handle
 *  \param pasid [in]  specified PASID.   
 */
void ATI_API_CALL KCL_IOMMU_UnbindPasid( KCL_PCI_DevHandle pcidev,int pasid)
{
#ifdef IOMMUV2_SUPPORT
    struct pci_dev* pdev = (struct pci_dev*)pcidev;
    amd_iommu_unbind_pasid( pdev, pasid);
#endif    
}


