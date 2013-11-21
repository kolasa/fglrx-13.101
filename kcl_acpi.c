/****************************************************************************
 *                                                                          *
 * Copyright 1999-2005 ATI Technologies Inc., Markham, Ontario, CANADA.     *
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

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
#include <generated/autoconf.h>
#else
#include <linux/autoconf.h>
#endif
#include <linux/acpi.h>
#include <linux/pci.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
#include <acpi/button.h>
#endif

#include "kcl_config.h"
#include "kcl_type.h"
#include "kcl_acpi.h"
#include "kcl_debug.h"

#include <linux/vt_kern.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)

#if defined(CONFIG_ACPI_INTERPRETER) && defined(CONFIG_ACPI_BOOT) && defined(CONFIG_ACPI_BUS)
#define KCL_ACPI_CONFIGURED
#endif

#else

#if defined(CONFIG_ACPI)
#define KCL_ACPI_CONFIGURED
#endif

#endif

/*
 * NOTE: When adding new KCL ACPI functions below, remember to add an appropriate
 *       stub function in the #else part of KCL_ACPI_CONFIGURED to handle those
 *       cases where ACPI support is not built within the kernel.  If the function
 *       should be implemented the same way regardless of whether ACPI support is
 *       available, put it after the #endif KCL_ACPI_CONFIGURED section by the end
 *       of the file.
 */

#if defined(KCL_ACPI_CONFIGURED)

struct firegl_acpi_pci_data {
    struct acpi_pci_id      id;
    struct pci_bus          *bus;
    struct pci_dev          *dev;
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)

/** \brief kernel call back function when acpi video events happen
 * \param nb Notifier block from kernel. Not used in driver.
 * \param val Value passed from notifer. Not used in driver.
 * \param data Pointer passed unmodified to notifier function. Here it is the acpi_bus_event.
 * \return NOTIFY_OK to indicate the event has been handled. 
 * */
static int firegl_acpi_video_event(struct notifier_block *nb, unsigned long val,
                               void *data)
{
    struct acpi_bus_event *entry;
    KCL_ACPI_ContextHandle ctx_handle;

    entry = (struct acpi_bus_event *)data;
    if ( !strcmp(entry->device_class, KCL_ACPI_VIDEO_CLASS) )
    {            
        ctx_handle = firegl_query_busid(entry->bus_id, KCL_ACPI_VIDEO_CLASS);
        if (ctx_handle == NULL)
        {
            KCL_DEBUG_ERROR ("Could not find private acpi context by video busid: %s\n", entry->bus_id);
        }
        else
        {
            libip_video_notify(NULL, entry->type, ctx_handle);
        }
    }
    return NOTIFY_OK;
}

static struct notifier_block firegl_acpi_video_notifier = {
        .notifier_call = firegl_acpi_video_event,
};

/** \brief kernel call back function when acpi ac events happen
 * \param nb Notifier block from kernel. Not used in driver.
 * \param val Value passed from notifer. Not used in driver.
 * \param data Pointer passed unmodified to notifier function. Here it is the acpi_bus_event.
 * \return NOTIFY_OK to indicate the event has been handled. 
 * */
static int firegl_acpi_ac_event(struct notifier_block *nb, unsigned long val,
                               void *data)
{
    struct acpi_bus_event *entry;
    KCL_ACPI_ContextHandle ctx_handle;
    unsigned int newstate;

    entry = (struct acpi_bus_event *)data;
    if ( !strcmp(entry->device_class, KCL_ACPI_AC_CLASS) )
    {            
        ctx_handle = firegl_query_busid(entry->bus_id, KCL_ACPI_AC_CLASS);
        if (ctx_handle == NULL)
        {
            KCL_DEBUG_ERROR ("Could not find private acpi context by ac_adpater busid: %s\n", entry->bus_id);
        }
        else
        {
            newstate = entry->data;
            libip_ac_notify(NULL, entry->type, ctx_handle, &newstate);
        }
    }
    return NOTIFY_OK;
}

static struct notifier_block firegl_acpi_ac_notifier = {
        .notifier_call = firegl_acpi_ac_event,
};


static int firegl_acpi_lid_event(struct notifier_block *nb, unsigned long val,
                               void *data)
{
    unsigned int status;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
    status = acpi_lid_open();
    libip_lid_open_notify(status);
#endif
    return NOTIFY_OK;
}

static struct notifier_block firegl_acpi_lid_notifier = {
        .notifier_call = firegl_acpi_lid_event,
};
#endif
/** \brief search for acpi namespace node with specified name
 *  \param parent specify the search scope
 *  \param pathname pointer to a null terminated ascii string
 *  \param ret_handle where to put the return handle
 *  \return status
 */
unsigned int ATI_API_CALL KCL_ACPI_GetDevHandle(KCL_ACPI_DevHandle parent,
                                                const char *pathname,
                                                KCL_ACPI_DevHandle *ret_handle)
{
    return acpi_get_handle(parent, (acpi_string)pathname, ret_handle);
}

/** \brief evaluate acpi namespace object, handle or pathname must be valid
 *  \param handle namespace object handle
 *  \param info input/output arguments for the control method
 *  \return status
 */
unsigned int ATI_API_CALL KCL_ACPI_EvalObject(KCL_ACPI_DevHandle handle,
                                              struct KCL_ACPI_MethodInputInfo *info)
{
    struct acpi_object_list input;
    struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
    union acpi_object *params = NULL;
    union acpi_object *obj = NULL;
    char name[5] = {'\0'};
    struct KCL_ACPI_MethodArgument *argument = NULL;
    unsigned int i, count;
    acpi_status status;
    unsigned int funcNo = 0xFFFFFFFF;

    if (handle == NULL)
    {
        return KCL_ACPI_ERROR;
    }

    memset(&input, 0, sizeof(struct acpi_object_list));

    /* validate input info */
    if (info->size != sizeof(struct KCL_ACPI_MethodInputInfo))
    {
        return KCL_ACPI_ERROR;
    }

    input.count = info->inputCount;
    if (info->inputCount > 0)
    {
        if (info->pInputArgument == NULL)
        {
            return KCL_ACPI_ERROR;
        }
        argument = info->pInputArgument;
        funcNo = argument->value;
        for (i = 0; i < info->inputCount; i++)
        {
            if (((argument->type == ACPI_TYPE_STRING) || (argument->type == ACPI_TYPE_BUFFER))
                && (argument->pointer == NULL))
            {
                return KCL_ACPI_ERROR;
            }
            argument++;
        }
    }

    if (info->outputCount > 0)
    {
        if (info->pOutputArgument == NULL)
        {
            return KCL_ACPI_ERROR;
        }
        argument = info->pOutputArgument;
        for (i = 0; i < info->outputCount; i++)
        {
            if (((argument->type == ACPI_TYPE_STRING) || (argument->type == ACPI_TYPE_BUFFER))
                && (argument->pointer == NULL))
            {
                return KCL_ACPI_ERROR;
            }
            argument++;
        }
    }


    /* The path name passed to acpi_evaluate_object should be null terminated */
    if ((info->field & KCL_ACPI_FIELD_METHOD_NAME) != 0)
    {
        strncpy(name, (char *)&(info->name), sizeof(unsigned int));
        name[4] = '\0';
    }

    /* parse input parameters */
    if (input.count > 0)
    {
        input.pointer = params = (union acpi_object *)kmalloc(sizeof(union acpi_object) * input.count, GFP_KERNEL);
        if (params == NULL)
        {
            return KCL_ACPI_ERROR;
        }
        memset(params, 0, sizeof(union acpi_object) * input.count);
        argument = info->pInputArgument;

        for (i = 0; i < input.count; i++)
        {
            params->type = argument->type;
            switch(params->type)
            {
                case ACPI_TYPE_INTEGER:
                    params->integer.value = argument->value;
                    break;
                case ACPI_TYPE_STRING:
                    params->string.length = argument->methodLength;
                    params->string.pointer = argument->pointer;
                    break;
                case ACPI_TYPE_BUFFER:
                    params->buffer.length = argument->methodLength;
                    params->buffer.pointer = argument->pointer;
                    break;
                default:
                    break;
            }
            params++;
            argument++;
        }
    }

    /* parse output info */
    count = info->outputCount;
    argument = info->pOutputArgument;

    /* evaluate the acpi method */
    status = acpi_evaluate_object(handle, name, &input, &output);
    KCL_DEBUG1(FN_FIREGL_ACPI, "Evaluated method: %s, handle: %p, function: %d, status: 0x%x  \n", name, handle, funcNo, (unsigned int)status);

    if (ACPI_FAILURE(status))
    {
        status = KCL_ACPI_ERROR;
        goto error;
    }

    /* return the output info */
    obj = output.pointer;

    if (count > 1)
    {
        if ((obj->type != ACPI_TYPE_PACKAGE) || (obj->package.count != count))
        {
            status = KCL_ACPI_ERROR;
            goto error;
        }
        params = obj->package.elements;
    }
    else
    {
        params = obj;
    }

    if (params == NULL)
    {
        status = KCL_ACPI_ERROR;
        goto error;
    }

    for (i = 0; i < count; i++)
    {
        if (argument->type != params->type)
        {
            status = KCL_ACPI_ERROR;
            goto error;
        }
        switch(params->type)
        {
            case ACPI_TYPE_INTEGER:
                argument->value = params->integer.value;
                break;
            case ACPI_TYPE_STRING:
                if ((params->string.length != argument->dataLength) || (params->string.pointer == NULL))
                {
                    status = KCL_ACPI_ERROR;
                    goto error;
                }
                strncpy(argument->pointer, params->string.pointer, params->string.length);
                break;
            case ACPI_TYPE_BUFFER:
                if (params->buffer.pointer == NULL)
                {
                    status = KCL_ACPI_ERROR;
                    goto error;
                }
                memcpy(argument->pointer, params->buffer.pointer, argument->dataLength);
                break;
            default:
                break;
        }
        argument++;
        params++;
    }

error:
    if (obj != NULL)
    {
        kfree(obj);
    }
    kfree((void *)input.pointer);
    return status;
}

void* KCL_ACPI_GetVfctBios(unsigned long *size)
{
    struct acpi_table_header *hdr;
    acpi_size tbl_size ;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,3)    
    if (!ACPI_SUCCESS(acpi_get_table_with_size("VFCT", 1, &hdr, &tbl_size)))
#else
    tbl_size = 0x7fffffff;
    if (!ACPI_SUCCESS(acpi_get_table("VFCT", 1, &hdr)))
#endif
    {
        return NULL;
    }
    *size = tbl_size;
    return hdr;
}

/** \brief install notify handler for acpi device
 *  \param device acpi device
 *  \param handler_type type of handler
 *  \param handler pointer of notify handler
 *  \param context context for the handler
 *  \return status
 */
unsigned int ATI_API_CALL KCL_ACPI_InstallHandler(KCL_ACPI_DevHandle device,
                                                  unsigned int handler_type,
                                                  KCL_ACPI_CallbackHandle handler,
                                                  KCL_ACPI_ContextHandle context)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
    return acpi_install_notify_handler(device, handler_type, (acpi_notify_handler)handler, context);
#else
    KCL_ACPI_DevHandle dummy_handle;

    //register notifier chain for video switching events
    if ((KCL_ACPI_GetDevHandle(device, "_DOD", &dummy_handle) == KCL_ACPI_OK)
        || (KCL_ACPI_GetDevHandle(device, "_DOS", &dummy_handle) == KCL_ACPI_OK))
    {
        return register_acpi_notifier(&firegl_acpi_video_notifier);
    }

    //register notifier chain for ac adapter events
    if (KCL_ACPI_GetDevHandle(device, "_PSR", &dummy_handle) == KCL_ACPI_OK)
    {
        return register_acpi_notifier(&firegl_acpi_ac_notifier);
    }

    return 0;
#endif
}

/** \brief uninstall notify handler for acpi device
 *  \param device acpi device
 *  \param handler_type type of handler
 *  \param handler pointer of notify handler
 *  \return status
 */
unsigned int ATI_API_CALL KCL_ACPI_RemoveHandler(KCL_ACPI_DevHandle device,
                                                 unsigned int handler_type,
                                                 KCL_ACPI_CallbackHandle handler)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
    return acpi_remove_notify_handler(device, handler_type, (acpi_notify_handler)handler);
#else
    KCL_ACPI_DevHandle dummy_handle;

    //unregister notifier chain for video switching events
    if ((KCL_ACPI_GetDevHandle(device, "_DOD", &dummy_handle) == KCL_ACPI_OK)
        || (KCL_ACPI_GetDevHandle(device, "_DOS", &dummy_handle) == KCL_ACPI_OK))
    {
        return unregister_acpi_notifier(&firegl_acpi_video_notifier);
    }

    //unregister notifier chain for ac adapter events
    if (KCL_ACPI_GetDevHandle(device, "_PSR", &dummy_handle) == KCL_ACPI_OK)
    {
        return unregister_acpi_notifier(&firegl_acpi_ac_notifier);
    }

    return 0;
#endif
}

unsigned int ATI_API_CALL KCL_ACPI_InstallLidHandler(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
    if (acpi_lid_notifier_register(&firegl_acpi_lid_notifier))
    {
        KCL_DEBUG_ERROR("lid notifier registration failed\n");
        firegl_acpi_lid_notifier.notifier_call = NULL;
    }
#endif
    return 0;
}

unsigned int ATI_API_CALL KCL_ACPI_RemoveLidHandler(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
    if (firegl_acpi_lid_notifier.notifier_call)
    {
        acpi_lid_notifier_unregister(&firegl_acpi_lid_notifier);
    }
#endif
    return 0;
}
/** \brief get the global variable acpi_disabled in the kernel
 *  \return the value of acpi_disabled
 */
int ATI_API_CALL KCL_ACPI_Disabled(void)
{
    return acpi_disabled;
}

/** \brief a function of acpi notify handler type, called when state of ac adapter changed
 *  \param handle      acpi handle
 *  \param event       acpi event type
 *  \param data        context for the notify handler
 *  \return void
 */
void KCL_ACPI_AcNotify(KCL_ACPI_DevHandle handle, unsigned int event, KCL_ACPI_ContextHandle data)
{
    libip_ac_notify(handle, event, data, NULL);
}

/** \brief execute a specified acpi notify handler
 *  \param handler     pointer to the notify handler
 *  \param handle      acpi handle
 *  \param event       acpi event type
 *  \param data        context for the notifyhandler
 *  \return void
 */
void ATI_API_CALL KCL_ACPI_ExecHandler(KCL_ACPI_CallbackHandle handler, KCL_ACPI_DevHandle handle, unsigned int event, KCL_ACPI_ContextHandle data)
{
    if (handler != NULL)
    {
        ((acpi_notify_handler)handler)(handle, event, data);
    }
}

/** \brief get notify handler for the device notify operand object
 *  \param handle acpi handle
 *  \return pointer to notify handler
 */
KCL_ACPI_CallbackHandle ATI_API_CALL KCL_ACPI_GetNotifyHandler(KCL_ACPI_DevHandle handle)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
    struct acpi_namespace_node *node = NULL;

    node = (struct acpi_namespace_node *)handle;

    if ((node != NULL) && (node->object != NULL))
    {
        if (node->object->common_notify.device_notify != NULL)
        {
            return (KCL_ACPI_CallbackHandle)node->object->common_notify.device_notify->notify.handler;
        }
        else
        {
            return NULL;
        }
    }
    else
#endif
    /* No alternative on 2.6.29 and later for now. The internal ACPI data
     * structures are no longer included in the kernel headers. 
     * For 2.6.29 and later, this is not used. Return null here.
     * And for 2.6.9, there is no ACPI video module, so also return null
     */
    {
        return NULL;
    }
}

/** \brief get notify context for the device notify operand object
 *  \param handle acpi handle
 *  \return pointer to notify context
 */
KCL_ACPI_ContextHandle ATI_API_CALL KCL_ACPI_GetNotifyContext(KCL_ACPI_DevHandle handle)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
    struct acpi_namespace_node *node = NULL;

    node = (struct acpi_namespace_node *)handle;

    if ((node != NULL) && (node->object != NULL))
    {
        if (node->object->common_notify.device_notify != NULL)
        {
            return (KCL_ACPI_ContextHandle)node->object->common_notify.device_notify->notify.context;
        }
        else
        {
            return NULL;
        }
    }
    else
#endif
    /* No alternative on 2.6.29 and later for now. The internal ACPI data
     * structures are no longer included in the kernel headers. 
     * For 2.6.29 and later, this is not used. Return null here.
     * And for 2.6.9, there is no ACPI video module, so also return null
     */
    {
        return NULL;
    }
}

/** \brief update notify handler for the device notify operand object
 *  \param handle acpi handle
 *  \param handler new notify handler
 *  \return
 */
void ATI_API_CALL KCL_ACPI_UpdateNotifyHandler(KCL_ACPI_DevHandle handle, KCL_ACPI_CallbackHandle handler)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
    struct acpi_namespace_node *node = NULL;

    node = (struct acpi_namespace_node *)handle;

    if ((node != NULL) && (node->object != NULL) && (node->object->common_notify.device_notify != NULL))
    {
        node->object->common_notify.device_notify->notify.handler = (acpi_notify_handler)handler;
    }
#endif
    /* No alternative on 2.6.29 and later for now. The internal ACPI data
     * structures are no longer included in the kernel headers. 
     * For 2.6.29 and later, this is not used.  */
    return;
}

/** \brief update notify context for the device notify operand object
 *  \param handle acpi handle
 *  \param context new notify context
 *  \return
 */
void ATI_API_CALL KCL_ACPI_UpdateNotifyContext(KCL_ACPI_DevHandle handle, KCL_ACPI_ContextHandle context)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
    struct acpi_namespace_node *node = NULL;

    node = (struct acpi_namespace_node *)handle;

    if ((node != NULL) && (node->object != NULL) && (node->object->common_notify.device_notify != NULL))
    {
        node->object->common_notify.device_notify->notify.context = context;
    }
#endif
    /* No alternative on 2.6.29 and later for now. The internal ACPI data
     * structures are no longer included in the kernel headers. 
     * For 2.6.29 and later, this is not used.  */
    return;
}

/** \brief get child device in ACPI namespace
 * \param handle acpi handle for parent device
 * \return acpi handle for child device
 */
KCL_ACPI_DevHandle ATI_API_CALL KCL_ACPI_GetChildDevice(KCL_ACPI_DevHandle handle)
{
    KCL_ACPI_DevHandle child_handle;
    acpi_status status = acpi_get_next_object (ACPI_TYPE_ANY, handle, NULL, &child_handle);
    if (ACPI_SUCCESS(status))
    {
        return child_handle;
    }
    else
    {
        return NULL;
    }
}

/** \brief get peer device in ACPI namespace
 * \param handle acpi handle
 * \return acpi handle for peer device
 */
KCL_ACPI_DevHandle ATI_API_CALL KCL_ACPI_GetPeerDevice(KCL_ACPI_DevHandle handle)
{
    KCL_ACPI_DevHandle peer_handle;
    acpi_status status = acpi_get_next_object (ACPI_TYPE_ANY, NULL, handle, &peer_handle);
    if (ACPI_SUCCESS(status))
    {
        return peer_handle;
    }
    else
    {
        return NULL;
    }
}

/** \brief called for Notify(VGA, 0x80)
 * \param handle acpi handle
 * \param event  acpi event type
 * \param data   context for the notify handler
 * \return void
 * */
void KCL_ACPI_VideoNotify(KCL_ACPI_DevHandle handle, unsigned int event, KCL_ACPI_ContextHandle data)
{
    libip_video_notify(handle, event, data);
}

/** \brief Compare specified busid with busid of specified acpi handle
 * \param handle acpi handle
 * \param bus_id busid from acpi event
 * \return 0 if equal; 1 otherwise
 * */
int ATI_API_CALL KCL_ACPI_Cmp_BusID(KCL_ACPI_DevHandle handle, KCL_ACPI_BusId bus_id)
{
    acpi_status status;
    struct acpi_device *tdev;
    int ret = 1;

    status = acpi_bus_get_device(handle, &tdev);
    if ( ACPI_SUCCESS(status) ) 
    {
        if ( !strcmp(tdev->pnp.bus_id, bus_id) )            
        {
            ret = 0;
        }
    }
    else
    {
        KCL_DEBUG_ERROR ("Could not find device for handle: %p, status: 0x%x.\n", handle, status);
    }
    return (ret);
}

static unsigned int KCL_ACPI_videoDevice(KCL_ACPI_DevHandle handle)
{
    KCL_ACPI_DevHandle dummy_handle;

    if ((KCL_ACPI_GetDevHandle(handle, "_DOD", &dummy_handle) == KCL_ACPI_OK)
       || (KCL_ACPI_GetDevHandle(handle, "_DOS", &dummy_handle) == KCL_ACPI_OK))
    {
        return KCL_ACPI_OK;
    }
    else
    {
        return KCL_ACPI_ERROR;
    }
}

static unsigned int KCL_ACPI_acDevice(KCL_ACPI_DevHandle handle)
{
    KCL_ACPI_DevHandle dummy_handle;

    if (KCL_ACPI_GetDevHandle(handle, "_PSR", &dummy_handle) == KCL_ACPI_OK)
    {
        return KCL_ACPI_OK;
    }
    else
    {
        return KCL_ACPI_ERROR;
    }
}

static unsigned int KCL_ACPI_PowerXpressDevice(KCL_ACPI_DevHandle handle)
{
    KCL_ACPI_DevHandle dummy_handle;

    if (KCL_ACPI_GetDevHandle(handle, "ATPX", &dummy_handle) == KCL_ACPI_OK)
    {
        return KCL_ACPI_OK;
    }
    else
    {
        return KCL_ACPI_ERROR;
    }
}

/* call back function for acpi_get_devices */
static unsigned int KCL_ACPI_SearchHandles(KCL_ACPI_DevHandle handle, unsigned int level, void* context, void **retval)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)
    acpi_status status;
    struct acpi_device *tdev;
    struct firegl_acpi_pci_data    *pdata = NULL;
    struct acpi_namespace_node     *node;
    union acpi_operand_object      *obj_desc;
#endif
    struct KCL_ACPI_MatchInfo *pInfo = (struct KCL_ACPI_MatchInfo *)context;

    if ( pInfo->ac_handle == NULL && KCL_ACPI_acDevice(handle) == KCL_ACPI_OK )
    {
        pInfo->ac_handle = handle;
    }

    //Hack: search video handle for old kernels.  
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)
    if ( pInfo->video_handle == NULL &&  (KCL_ACPI_videoDevice(handle) == KCL_ACPI_OK) )
    {
        status = acpi_bus_get_device(handle, &tdev);
        if ( ACPI_FAILURE(status) )
        {
            KCL_DEBUG_ERROR ("Could not find acpi device for video handle: %p, status: 0x%x.\n", handle, status);
            return KCL_ACPI_OK;
        }

        node = (struct acpi_namespace_node *)handle;
        if ( node != NULL && node->object != NULL )
        {
            obj_desc = node->object;
            while (obj_desc) 
            {
                if ( (ACPI_GET_OBJECT_TYPE (obj_desc) == ACPI_TYPE_LOCAL_DATA) &&
                     obj_desc->data.handler != NULL ) 
                {
                    // Only two kinds of data is attached to a video device: "struct acpi_device" and "struct acpi_pci_data" 
                    if ( obj_desc->data.pointer != NULL && (void *)obj_desc->data.pointer != (void *)tdev )
                    {
                        pdata = (struct firegl_acpi_pci_data *)obj_desc->data.pointer;
                        if ( pdata && pdata->dev == pInfo->pcidev )
                        {
                            pInfo->video_handle = handle;
                        }
                        break;
                    }
                }
                obj_desc = obj_desc->common.next_object;
            }
        }
    }
    if (pInfo->video_handle)
    {
#endif    
        if (pInfo->ac_handle)
        {
            return AE_CTRL_TERMINATE;
        }
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)
    }
#endif
    return KCL_ACPI_OK;
}

unsigned int ATI_API_CALL KCL_ACPI_GetHandles(kcl_match_info_t *pInfo)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0)
        pInfo->video_handle = pInfo->pcidev->dev.acpi_node.handle;
#else
        pInfo->video_handle = pInfo->pcidev->dev.acpi_node.companion;
#endif
    #elif LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19)
        pInfo->video_handle = pInfo->pcidev->dev.archdata.acpi_handle;
    #else 
        pInfo->video_handle = pInfo->pcidev->dev.firmware_data;
    #endif    
    if ( pInfo->video_handle &&
        (KCL_ACPI_videoDevice(pInfo->video_handle) != KCL_ACPI_OK) )
    {
        KCL_DEBUG1(FN_FIREGL_KCL, "Video handle doesn't have ACPI function: %p for video device: %p\n", pInfo->video_handle, pInfo->pcidev);
        pInfo->video_handle = NULL;
    }
#endif
    return acpi_get_devices(NULL, (acpi_walk_callback)KCL_ACPI_SearchHandles, pInfo, NULL);
}

static unsigned int KCL_ACPI_SearchAlternateHandle(KCL_ACPI_DevHandle handle, unsigned int level, void* context, void **retval)
{
    /*This function is only used by PowerXpress handle search for now*/
    KCL_ACPI_DevHandle pHandle = (KCL_ACPI_DevHandle) context;
    if ((KCL_ACPI_PowerXpressDevice(handle) == KCL_ACPI_OK) && handle != pHandle)
    {
        *retval = handle;
        return AE_CTRL_TERMINATE;
    }
    return AE_OK;
}

/** \brief get a different video handle in ACPI namespace
 * \param handle acpi handle for current video handle
 * \return alternate acpi video handle
 */
KCL_ACPI_DevHandle ATI_API_CALL KCL_ACPI_GetAlternateHandle(KCL_ACPI_DevHandle pHandle)
{
	acpi_status status;
    KCL_ACPI_DevHandle retHandle = NULL;
    status = acpi_get_devices(NULL, (acpi_walk_callback)KCL_ACPI_SearchAlternateHandle, pHandle, &retHandle);
    if ( ACPI_FAILURE(status) )
    {
        return NULL;
    }
    return retHandle;
}

#else

/*
 * ACPI support was never built into the kernel,
 * stub out all the KCL ACPI funtions.
 */

unsigned int ATI_API_CALL KCL_ACPI_GetDevHandle(KCL_ACPI_DevHandle parent,
                                                const char *pathname,
                                                KCL_ACPI_DevHandle *ret_handle)
{
    return KCL_ACPI_NOT_AVAILABLE;
}

unsigned int ATI_API_CALL KCL_ACPI_EvalObject(KCL_ACPI_DevHandle handle,
                                              struct KCL_ACPI_MethodInputInfo *info)
{
    return KCL_ACPI_NOT_AVAILABLE;
}

unsigned int ATI_API_CALL KCL_ACPI_InstallHandler(KCL_ACPI_DevHandle device,
                                                  unsigned int handler_type,
                                                  KCL_ACPI_CallbackHandle handler,
                                                  KCL_ACPI_ContextHandle context)
{
    return KCL_ACPI_NOT_AVAILABLE;
}

unsigned int ATI_API_CALL KCL_ACPI_RemoveHandler(KCL_ACPI_DevHandle device,
                                                 unsigned int handler_type,
                                                 KCL_ACPI_CallbackHandle handler)
{
    return KCL_ACPI_NOT_AVAILABLE;
}

unsigned int ATI_API_CALL KCL_ACPI_InstallLidHandler()
{
    return KCL_ACPI_NOT_AVAILABLE;
}
unsigned int ATI_API_CALL KCL_ACPI_RemoveLidHandler();
{
    return KCL_ACPI_NOT_AVAILABLE;
}

int ATI_API_CALL KCL_ACPI_Disabled(void)
{
    return (1); /* Needs to mimic acpi_disabled value when permanently disabled */
}

void KCL_ACPI_AcNotify(KCL_ACPI_DevHandle handle, unsigned int event, KCL_ACPI_ContextHandle data)
{
    return;
}

void ATI_API_CALL KCL_ACPI_ExecHandler(KCL_ACPI_CallbackHandle handler, KCL_ACPI_DevHandle handle, unsigned int event, KCL_ACPI_ContextHandle data)
{
    return;
}

KCL_ACPI_CallbackHandle ATI_API_CALL KCL_ACPI_GetNotifyHandler(KCL_ACPI_DevHandle handle)
{
    return NULL;
}

KCL_ACPI_ContextHandle ATI_API_CALL KCL_ACPI_GetNotifyContext(KCL_ACPI_DevHandle handle)
{
    return NULL;
}

void ATI_API_CALL KCL_ACPI_UpdateNotifyHandler(KCL_ACPI_DevHandle handle, KCL_ACPI_CallbackHandle handler)
{
    return;
}

void ATI_API_CALL KCL_ACPI_UpdateNotifyContext(KCL_ACPI_DevHandle handle, KCL_ACPI_ContextHandle context)
{
    return;
}

KCL_ACPI_DevHandle ATI_API_CALL KCL_ACPI_GetChildDevice(KCL_ACPI_DevHandle handle)
{
    return NULL;
}

KCL_ACPI_DevHandle ATI_API_CALL KCL_ACPI_GetPeerDevice(KCL_ACPI_DevHandle handle)
{
    return NULL;
}

void KCL_ACPI_VideoNotify(KCL_ACPI_DevHandle handle, unsigned int event, KCL_ACPI_ContextHandle data)
{
    return NULL;
}

int ATI_API_CALL KCL_ACPI_Cmp_BusID(KCL_ACPI_DevHandle handle, KCL_ACPI_BusId bus_id)
{
    return (1);
}

unsigned int ATI_API_CALL KCL_ACPI_GetHandles(struct KCL_ACPI_MatchInfo *pInfo)
{
    return KCL_ACPI_NOT_AVAILABLE;
}

KCL_ACPI_DevHandle ATI_API_CALL KCL_ACPI_GetAlternateHandle(KCL_ACPI_DevHandle pHandle)
{
    return NULL;
}

#endif

/*
 * The following KCL ACPI functions need to always work regardless
 * of whether or not ACPI support has been built into the kernel.
 */

int ATI_API_CALL KCL_ACPI_GetFgConsole(void)
{
    return fg_console;
}

int ATI_API_CALL KCL_ACPI_GetNewVt(int console)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)
    return vc_cons[console].d->vt_newvt;
#else
    return vt_cons[console]->vt_newvt;
#endif
}

