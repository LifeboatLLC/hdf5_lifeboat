/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://www.hdfgroup.org/licenses.               *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Purpose:	Generic Property Functions
 */

/****************/
/* Module Setup */
/****************/

#include "H5Pmodule.h" /* This source code file is part of the H5P module */

/***********/
/* Headers */
/***********/
#include "H5private.h"   /* Generic Functions	   */
#include "H5CXprivate.h" /* API Contexts           */
#include "H5Eprivate.h"  /* Error handling		   */
#include "H5Iprivate.h"  /* IDs			  		   */
#include "H5Ppkg.h"      /* Property lists		   */

#ifdef H5_HAVE_MULTITHREAD
#include "H5Ppkg_mt.h" /* MT Safe Property List structures */
#endif

/****************/
/* Local Macros */
/****************/

/******************/
/* Local Typedefs */
/******************/

#ifdef H5_HAVE_MULTITHREAD

typedef H5P_mt_list_t  H5P_genplist_t;
typedef H5P_mt_class_t H5P_genclass_t;

#endif

/* Typedef for property iterator callback */
typedef struct {
    H5P_iterate_t iter_func; /* Iterator callback */
    hid_t         id;        /* Property list or class ID */
    void         *iter_data; /* Iterator callback pointer */
} H5P_iter_ud_t;

/********************/
/* Local Prototypes */
/********************/

/*********************/
/* Package Variables */
/*********************/

/*****************************/
/* Library Private Variables */
/*****************************/

/*******************/
/* Local Variables */
/*******************/

#ifdef H5_HAVE_MULTITHREAD

/****************************************************************************************
 * Function:    H5Pcopy
 *
 * Purpose:     Multithread version of H5Pcopy() which is a routine to copy a property
 *              list or class.
 *
 * Return:      SUCCEED / FAIL
 *
 ****************************************************************************************
 */
hid_t
H5Pcopy(hid_t id)
{
    H5P_mt_class_t              *copy_class; /* Copy of the class */
    H5P_mt_active_thread_count_t thrd;
    void                        *obj;                         /* Property object to copy */
    hid_t                        ret_value = H5I_INVALID_HID; /* return value */

    FUNC_ENTER_API(H5I_INVALID_HID)
    H5TRACE1("i", "i", id);

    if (H5P_DEFAULT == id) {
        HGOTO_DONE(H5P_DEFAULT);
    }

    /* Check arguments. */
    if (H5I_GENPROP_LST != H5I_get_type(id) && H5I_GENPROP_CLS != H5I_get_type(id)) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5I_INVALID_HID, "not property object");
    }
    if (NULL == (obj = H5I_object(id))) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, H5I_INVALID_HID, "property object doesn't exist");
    }

    /* Compare property lists */
    if (H5I_GENPROP_LST == H5I_get_type(id)) {
        if ((ret_value = H5P_copy_plist((H5P_mt_list_t *)obj, TRUE)) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, H5I_INVALID_HID, "can't copy MT property list");
        }
    }
    /* Compare property list classes */
    else {
        /* Copy the class */
        if ((copy_class = H5P__mt_copy_class((H5P_mt_class_t *)obj)) == NULL) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, H5I_INVALID_HID, "can't copy MT property class");
        }

        /* Get an ID for the copied class */
        if ((ret_value = H5I_register(H5I_GENPROP_CLS, copy_class, TRUE)) < 0) {
            /* If getting an ID for the copied class fails */
            H5P__mt_close_class(copy_class);

            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, H5I_INVALID_HID,
                        "unable to register MT property list class");
        }

        /* Now that the copied class is registered, opening process is complete */
        thrd.count   = 0;
        thrd.opening = FALSE;
        thrd.closing = FALSE;

        atomic_store(&(copy_class->thrd), thrd);
    }

done:

    FUNC_LEAVE_API(ret_value)

} /* H5Pcopy() MT safe version */

#else

/*--------------------------------------------------------------------------
 NAME
    H5Pcopy
 PURPOSE
    Routine to copy a property list or class
 USAGE
    hid_t H5Pcopy(id)
        hid_t id;           IN: Property list or class ID to copy
 RETURNS
    Success: valid property list ID on success (non-negative)
    Failure: H5I_INVALID_HID (negative)
 DESCRIPTION
    Copy a property list or class and return the ID.  This routine calls the
    class 'copy' callback after any property 'copy' callbacks are called
    (assuming all property 'copy' callbacks return successfully).

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
hid_t
H5Pcopy(hid_t id)
{
    H5P_genclass_t *copy_class;                  /* Copy of class */
    void           *obj;                         /* Property object to copy */
    hid_t           ret_value = H5I_INVALID_HID; /* return value */

    FUNC_ENTER_API(H5I_INVALID_HID)
    H5TRACE1("i", "i", id);

    if (H5P_DEFAULT == id)
        HGOTO_DONE(H5P_DEFAULT);

    /* Check arguments. */
    if (H5I_GENPROP_LST != H5I_get_type(id) && H5I_GENPROP_CLS != H5I_get_type(id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5I_INVALID_HID, "not property object");
    if (NULL == (obj = H5I_object(id)))
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, H5I_INVALID_HID, "property object doesn't exist");

    /* Compare property lists */
    if (H5I_GENPROP_LST == H5I_get_type(id)) {

        if ((ret_value = H5P_copy_plist((H5P_genplist_t *)obj, TRUE)) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, H5I_INVALID_HID, "can't copy property list");
    } /* end if */
    /* Must be property classes */
    else {
        /* Copy the class */
        if ((copy_class = H5P__copy_pclass((H5P_genclass_t *)obj)) == NULL)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, H5I_INVALID_HID, "can't copy property class");

        /* Get an ID for the copied class */
        if ((ret_value = H5I_register(H5I_GENPROP_CLS, copy_class, TRUE)) < 0) {

            /* If getting an ID for the copied class fails */
            H5P__close_class(copy_class);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, H5I_INVALID_HID,
                        "unable to register property list class");

        } /* end if */

    } /* end else */

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pcopy() */

#endif

#ifdef H5_HAVE_MULTITHREAD
/****************************************************************************************
 * Function:    H5Pcreate_class
 *
 * Purpose:     Multithread version of H5Pcreate_class() which creates a new property
 *              list class.
 *
 * Return:      Success: Returns a pointer to the new H5P_mt_class_t structure
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
hid_t
H5Pcreate_class(hid_t parent, const char *name, H5P_cls_create_func_t cls_create, void *create_data,
                H5P_cls_copy_func_t cls_copy, void *copy_data, H5P_cls_close_func_t cls_close,
                void *close_data)
{
    H5P_mt_class_t              *par_class = NULL;
    H5P_mt_class_t              *pclass    = NULL;
    H5P_mt_active_thread_count_t thrd;
    H5P_mt_active_thread_count_t update_thrd;
    bool                         done = FALSE;

    hid_t ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_API(H5I_INVALID_HID)
    H5TRACE8("i", "i*sPc*xPo*xPl*x", parent, name, cls_create, create_data, cls_copy, copy_data, cls_close,
             close_data);

    /* Check arguments. */
    if (H5P_DEFAULT != parent && (H5I_GENPROP_CLS != H5I_get_type(parent))) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5I_INVALID_HID, "not a property list class");
    }
    if (!name || !*name) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, H5I_INVALID_HID, "invalid class name");
    }
    if ((create_data != NULL && cls_create == NULL) || (copy_data != NULL && cls_copy == NULL) ||
        (close_data != NULL && cls_close == NULL)) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, H5I_INVALID_HID, "data specified, but no callback provided");
    }

    /* Get the pointer to the MT parent class */
    if (parent == H5P_DEFAULT) {
        par_class = NULL;
    }
    else if (NULL == (par_class = (H5P_mt_class_t *)H5I_object(parent))) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5I_INVALID_HID, "can't retrieve MT parent class");
    }

    /* Create the new MT property list class */
    if (NULL == (pclass = H5P__mt_create_class(par_class, name, H5P_TYPE_USER, 0, cls_create, create_data,
                                               cls_copy, copy_data, cls_close, close_data))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, H5I_INVALID_HID, "unable to create MT property list class");
    }

    /* Get an ID for the new class */
    if ((ret_value = H5I_register(H5I_GENPROP_CLS, pclass, TRUE)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to register property list class");
    }

    atomic_store(&(pclass->id), ret_value);

    /**
     * Now that the class has an ID atomically update
     * thrd.opening to FALSE so other threads can access it
     */
    do {
        thrd = atomic_load(&(pclass->thrd));

        assert(thrd.opening);
        assert(!thrd.closing);

        update_thrd.count   = thrd.count;
        update_thrd.opening = FALSE;
        update_thrd.closing = FALSE;

        if (!atomic_compare_exchange_strong(&(pclass->thrd), &thrd, update_thrd)) {
            /* attempt failed, update stats and try again */
            atomic_fetch_add(&(pclass->num_thrd_update_cols), 1);

            /* assert is to not get stuck in an infinite loop while testing */
            assert(H5P_MT_ASSERT_FAIL);
        }
        else {
            /* attempt succeded update stats and set done */
            atomic_fetch_add(&(pclass->num_thrd_count_update), 1);

            done = TRUE;
        }

    } while (!done);

done:

    if (H5I_INVALID_HID == ret_value && pclass) {
        H5P__mt_close_class(pclass);
    }

    FUNC_LEAVE_API(ret_value)

} /* H5Pcreate_class() MT safe version */

#else

/*--------------------------------------------------------------------------
 NAME
    H5Pcreate_class
 PURPOSE
    Create a new property list class.
 USAGE
    hid_t H5Pcreate_class(parent, name, cls_create, create_data,
                cls_close, close_data)
        hid_t parent;       IN: Property list class ID of parent class
        const char *name;   IN: Name of class we are creating
        H5P_cls_create_func_t cls_create;   IN: The callback function to call
                                    when each property list in this class is
                                    created.
        void *create_data;  IN: Pointer to user data to pass along to class
                                    creation callback.
        H5P_cls_copy_func_t cls_copy;   IN: The callback function to call
                                    when each property list in this class is
                                    copied.
        void *copy_data;  IN: Pointer to user data to pass along to class
                                    copy callback.
        H5P_cls_close_func_t cls_close;     IN: The callback function to call
                                    when each property list in this class is
                                    closed.
        void *close_data;   IN: Pointer to user data to pass along to class
                                    close callback.
 RETURNS
    Returns a valid property list class ID on success, H5I_INVALID_HID on failure.
 DESCRIPTION
    Allocates memory and attaches a class to the property list class hierarchy.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
hid_t
H5Pcreate_class(hid_t parent, const char *name, H5P_cls_create_func_t cls_create, void *create_data,
                H5P_cls_copy_func_t cls_copy, void *copy_data, H5P_cls_close_func_t cls_close,
                void *close_data)
{
    H5P_genclass_t *par_class = NULL;            /* Pointer to the parent class */
    H5P_genclass_t *pclass    = NULL;            /* Property list class created */
    hid_t           ret_value = H5I_INVALID_HID; /* Return value		   */

    FUNC_ENTER_API(H5I_INVALID_HID)
    H5TRACE8("i", "i*sPc*xPo*xPl*x", parent, name, cls_create, create_data, cls_copy, copy_data, cls_close,
             close_data);

    /* Check arguments. */
    if (H5P_DEFAULT != parent && (H5I_GENPROP_CLS != H5I_get_type(parent)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5I_INVALID_HID, "not a property list class");
    if (!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, H5I_INVALID_HID, "invalid class name");
    if ((create_data != NULL && cls_create == NULL) || (copy_data != NULL && cls_copy == NULL) ||
        (close_data != NULL && cls_close == NULL))
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, H5I_INVALID_HID, "data specified, but no callback provided");

    /* Get the pointer to the parent class */
    if (parent == H5P_DEFAULT)
        par_class = NULL;
    else if (NULL == (par_class = (H5P_genclass_t *)H5I_object(parent)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5I_INVALID_HID, "can't retrieve parent class");

    /* Create the new property list class */
    if (NULL == (pclass = H5P__create_class(par_class, name, H5P_TYPE_USER, cls_create, create_data, cls_copy,
                                            copy_data, cls_close, close_data)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, H5I_INVALID_HID, "unable to create property list class");

    /* Get an ID for the class */
    if ((ret_value = H5I_register(H5I_GENPROP_CLS, pclass, TRUE)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to register property list class");

done:
    if (H5I_INVALID_HID == ret_value && pclass)
        H5P__close_class(pclass);

    FUNC_LEAVE_API(ret_value)
} /* H5Pcreate_class() */

#endif

#ifdef H5_HAVE_MULTITHREAD
/****************************************************************************************
 * Function:    H5Pcreate
 *
 * Purpose:     Multithread version of H5Pcreate() which creates a new property
 *              list derived from a property list class.
 *
 * Return:      Success: Returns a pointer to the new H5P_mt_class_t structure
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
hid_t
H5Pcreate(hid_t cls_id)
{
    H5P_mt_class_t *pclass;

    hid_t ret_value = H5I_INVALID_HID; /* return value */

    FUNC_ENTER_API(H5I_INVALID_HID)
    H5TRACE1("i", "i", cls_id);

    /* Check arguments, and get class to derive the list from */
    if (NULL == (pclass = (H5P_mt_class_t *)H5I_object_verify(cls_id, H5I_GENPROP_CLS))) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5I_INVALID_HID, "not a MT property list class");
    }

    /* Create the new property list */
    if ((ret_value = H5P_create_id(pclass, TRUE)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, H5I_INVALID_HID, "unable to create property list");
    }

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pcreate() */

#else

/*--------------------------------------------------------------------------
 NAME
    H5Pcreate
 PURPOSE
    Routine to create a new property list of a property list class.
 USAGE
    hid_t H5Pcreate(cls_id)
        hid_t cls_id;       IN: Property list class create list from
 RETURNS
    Returns a valid property list ID on success, H5I_INVALID_HID  on failure.
 DESCRIPTION
        Creates a property list of a given class.  If a 'create' callback
    exists for the property list class, it is called before the
    property list is passed back to the user.  If 'create' callbacks exist for
    any individual properties in the property list, they are called before the
    class 'create' callback.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
hid_t
H5Pcreate(hid_t cls_id)
{
    H5P_genclass_t *pclass;                      /* Property list class to modify */
    hid_t           ret_value = H5I_INVALID_HID; /* return value */

    FUNC_ENTER_API(H5I_INVALID_HID)
    H5TRACE1("i", "i", cls_id);

    /* Check arguments. */
    if (NULL == (pclass = (H5P_genclass_t *)H5I_object_verify(cls_id, H5I_GENPROP_CLS)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5I_INVALID_HID, "not a property list class");

    /* Create the new property list */
    if ((ret_value = H5P_create_id(pclass, TRUE)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, H5I_INVALID_HID, "unable to create property list");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pcreate() */

#endif

#ifdef H5_HAVE_MULTITHREAD

/****************************************************************************************
 * Function:    H5Pregister2
 *
 * Purpose:     Multithread version of H5Pregister2() which registers a new property into
 *              a property list class.
 *
 *              NOTE: see the original version below this version for a description of
 *              the parameters.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5Pregister2(hid_t cls_id, const char *name, size_t size, void *def_value, H5P_prp_create_func_t prp_create,
             H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get, H5P_prp_delete_func_t prp_delete,
             H5P_prp_copy_func_t prp_copy, H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close)
{
    H5P_mt_class_t *pclass;
    // H5P_mt_prop_t  * prop;

    herr_t ret_value; /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE11("e", "i*sz*xPCPSPGPDPOPMPL", cls_id, name, size, def_value, prp_create, prp_set, prp_get,
              prp_delete, prp_copy, prp_cmp, prp_close);

    /* Check arguments. */
    if (NULL == (pclass = (H5P_mt_class_t *)H5I_object_verify(cls_id, H5I_GENPROP_CLS))) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list class");
    }
    if (!name || !*name) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid class name");
    }
    if (size > 0 && def_value == NULL) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "properties >0 size must have default");
    }

    if ((ret_value = H5P__register(&pclass, name, size, def_value, prp_create, prp_set, prp_get, NULL, NULL,
                                   prp_delete, prp_copy, prp_cmp, prp_close)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to register property in class");
    }

done:

    FUNC_LEAVE_API(ret_value)

} /* H5Pregister2() MT safe version */

#else

/*--------------------------------------------------------------------------
 NAME
    H5Pregister2
 PURPOSE
    Routine to register a new property in a property list class.
 USAGE
    herr_t H5Pregister2(class, name, size, default, prp_create, prp_set, prp_get, prp_close)
        hid_t class;            IN: Property list class to close
        const char *name;       IN: Name of property to register
        size_t size;            IN: Size of property in bytes
        void *def_value;        IN: Pointer to buffer containing default value
                                    for property in newly created property lists
        H5P_prp_create_func_t prp_create;   IN: Function pointer to property
                                    creation callback
        H5P_prp_set_func_t prp_set; IN: Function pointer to property set callback
        H5P_prp_get_func_t prp_get; IN: Function pointer to property get callback
        H5P_prp_delete_func_t prp_delete; IN: Function pointer to property delete callback
        H5P_prp_copy_func_t prp_copy; IN: Function pointer to property copy callback
        H5P_prp_compare_func_t prp_cmp; IN: Function pointer to property compare callback
        H5P_prp_close_func_t prp_close; IN: Function pointer to property close
                                    callback
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Registers a new property with a property list class.  The property will
    exist in all property list objects of that class after this routine is
    finished.  The name of the property must not already exist.  The default
    property value must be provided and all new property lists created with this
    property will have the property value set to the default provided.  Any of
    the callback routines may be set to NULL if they are not needed.

        Zero-sized properties are allowed and do not store any data in the
    property list.  These may be used as flags to indicate the presence or
    absence of a particular piece of information.  The 'default' pointer for a
    zero-sized property may be set to NULL.  The property 'create' & 'close'
    callbacks are called for zero-sized properties, but the 'set' and 'get'
    callbacks are never called.

        The 'create' callback is called when a new property list with this
    property is being created.  H5P_prp_create_func_t is defined as:
        typedef herr_t (*H5P_prp_create_func_t)(hid_t prop_id, const char *name,
                size_t size, void *initial_value);
    where the parameters to the callback function are:
        hid_t prop_id;      IN: The ID of the property list being created.
        const char *name;   IN: The name of the property being modified.
        size_t size;        IN: The size of the property value
        void *initial_value; IN/OUT: The initial value for the property being created.
                                (The 'default' value passed to H5Pregister2)
    The 'create' routine may modify the value to be set and those changes will
    be stored as the initial value of the property.  If the 'create' routine
    returns a negative value, the new property value is not copied into the
    property and the property list creation routine returns an error value.

        The 'set' callback is called before a new value is copied into the
    property.  H5P_prp_set_func_t is defined as:
        typedef herr_t (*H5P_prp_set_func_t)(hid_t prop_id, const char *name,
            size_t size, void *value);
    where the parameters to the callback function are:
        hid_t prop_id;      IN: The ID of the property list being modified.
        const char *name;   IN: The name of the property being modified.
        size_t size;        IN: The size of the property value
        void *new_value;    IN/OUT: The value being set for the property.
    The 'set' routine may modify the value to be set and those changes will be
    stored as the value of the property.  If the 'set' routine returns a
    negative value, the new property value is not copied into the property and
    the property list set routine returns an error value.

        The 'get' callback is called before a value is retrieved from the
    property.  H5P_prp_get_func_t is defined as:
        typedef herr_t (*H5P_prp_get_func_t)(hid_t prop_id, const char *name,
            size_t size, void *value);
    where the parameters to the callback function are:
        hid_t prop_id;      IN: The ID of the property list being queried.
        const char *name;   IN: The name of the property being queried.
        size_t size;        IN: The size of the property value
        void *value;        IN/OUT: The value being retrieved for the property.
    The 'get' routine may modify the value to be retrieved and those changes
    will be returned to the calling function.  If the 'get' routine returns a
    negative value, the property value is returned and the property list get
    routine returns an error value.

        The 'delete' callback is called when a property is deleted from a
    property list.  H5P_prp_del_func_t is defined as:
        typedef herr_t (*H5P_prp_del_func_t)(hid_t prop_id, const char *name,
            size_t size, void *value);
    where the parameters to the callback function are:
        hid_t prop_id;      IN: The ID of the property list the property is deleted from.
        const char *name;   IN: The name of the property being deleted.
        size_t size;        IN: The size of the property value
        void *value;        IN/OUT: The value of the property being deleted.
    The 'delete' routine may modify the value passed in, but the value is not
    used by the library when the 'delete' routine returns.  If the
    'delete' routine returns a negative value, the property list deletion
    routine returns an error value but the property is still deleted.

        The 'copy' callback is called when a property list with this
    property is copied.  H5P_prp_copy_func_t is defined as:
        typedef herr_t (*H5P_prp_copy_func_t)(const char *name, size_t size,
            void *value);
    where the parameters to the callback function are:
        const char *name;   IN: The name of the property being copied.
        size_t size;        IN: The size of the property value
        void *value;        IN: The value of the property being copied.
    The 'copy' routine may modify the value to be copied and those changes will be
    stored as the value of the property.  If the 'copy' routine returns a
    negative value, the new property value is not copied into the property and
    the property list copy routine returns an error value.

        The 'compare' callback is called when a property list with this
    property is compared to another property list.  H5P_prp_compare_func_t is
    defined as:
        typedef int (*H5P_prp_compare_func_t)( void *value1, void *value2,
            size_t size);
    where the parameters to the callback function are:
        const void *value1; IN: The value of the first property being compared.
        const void *value2; IN: The value of the second property being compared.
        size_t size;        IN: The size of the property value
    The 'compare' routine may not modify the values to be compared.  The
    'compare' routine should return a positive value if VALUE1 is greater than
    VALUE2, a negative value if VALUE2 is greater than VALUE1 and zero if VALUE1
    and VALUE2 are equal.

        The 'close' callback is called when a property list with this
    property is being destroyed.  H5P_prp_close_func_t is defined as:
        typedef herr_t (*H5P_prp_close_func_t)(const char *name, size_t size,
            void *value);
    where the parameters to the callback function are:
        const char *name;   IN: The name of the property being closed.
        size_t size;        IN: The size of the property value
        void *value;        IN: The value of the property being closed.
    The 'close' routine may modify the value passed in, but the value is not
    used by the library when the 'close' routine returns.  If the
    'close' routine returns a negative value, the property list close
    routine returns an error value but the property list is still closed.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
        The 'set' callback function may be useful to range check the value being
    set for the property or may perform some transformation/translation of the
    value set.  The 'get' callback would then [probably] reverse the
    transformation, etc.  A single 'get' or 'set' callback could handle
    multiple properties by performing different actions based on the property
    name or other properties in the property list.

        I would like to say "the property list is not closed" when a 'close'
    routine fails, but I don't think that's possible due to other properties in
    the list being successfully closed & removed from the property list.  I
    suppose that it would be possible to just remove the properties which have
    successful 'close' callbacks, but I'm not happy with the ramifications
    of a mangled, un-closable property list hanging around...  Any comments? -QAK

 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5Pregister2(hid_t cls_id, const char *name, size_t size, void *def_value, H5P_prp_create_func_t prp_create,
             H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get, H5P_prp_delete_func_t prp_delete,
             H5P_prp_copy_func_t prp_copy, H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close)
{
    H5P_genclass_t *pclass;      /* Property list class to modify */
    H5P_genclass_t *orig_pclass; /* Original property class */
    herr_t          ret_value;   /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE11("e", "i*sz*xPCPSPGPDPOPMPL", cls_id, name, size, def_value, prp_create, prp_set, prp_get,
              prp_delete, prp_copy, prp_cmp, prp_close);

    /* Check arguments. */
    if (NULL == (pclass = (H5P_genclass_t *)H5I_object_verify(cls_id, H5I_GENPROP_CLS)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list class");
    if (!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid class name");
    if (size > 0 && def_value == NULL)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "properties >0 size must have default");

    /* Create the new property list class */
    orig_pclass = pclass;
    if ((ret_value = H5P__register(&pclass, name, size, def_value, prp_create, prp_set, prp_get, NULL, NULL,
                                   prp_delete, prp_copy, prp_cmp, prp_close)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to register property in class");

    /* Check if the property class changed and needs to be substituted in the ID */
    if (pclass != orig_pclass) {
        H5P_genclass_t *old_pclass; /* Old property class */

        /* Substitute the new property class in the ID */
        if (NULL == (old_pclass = (H5P_genclass_t *)H5I_subst(cls_id, pclass)))
            HGOTO_ERROR(H5E_PLIST, H5E_CANTSET, FAIL, "unable to substitute property class in ID");
        assert(old_pclass == orig_pclass);

        /* Close the previous class */
        if (H5P__close_class(old_pclass) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCLOSEOBJ, FAIL,
                        "unable to close original property class after substitution");
    } /* end if */

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pregister2() */

#endif

/*--------------------------------------------------------------------------
 NAME
    H5Pinsert2
 PURPOSE
    Routine to insert a new property in a property list.
 USAGE
    herr_t H5Pinsert2(plist, name, size, value, prp_set, prp_get, prp_close)
        hid_t plist;            IN: Property list to add property to
        const char *name;       IN: Name of property to add
        size_t size;            IN: Size of property in bytes
        void *value;            IN: Pointer to the value for the property
        H5P_prp_set_func_t prp_set; IN: Function pointer to property set callback
        H5P_prp_get_func_t prp_get; IN: Function pointer to property get callback
        H5P_prp_delete_func_t prp_delete; IN: Function pointer to property delete callback
        H5P_prp_copy_func_t prp_copy; IN: Function pointer to property copy callback
        H5P_prp_compare_func_t prp_cmp; IN: Function pointer to property compare callback
        H5P_prp_close_func_t prp_close; IN: Function pointer to property close
                                    callback
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Inserts a temporary property into a property list.  The property will
    exist only in this property list object.  The name of the property must not
    already exist.  The value must be provided unless the property is zero-
    sized.  Any of the callback routines may be set to NULL if they are not
    needed.

        Zero-sized properties are allowed and do not store any data in the
    property list.  These may be used as flags to indicate the presence or
    absence of a particular piece of information.  The 'value' pointer for a
    zero-sized property may be set to NULL.  The property 'close' callback is
    called for zero-sized properties, but the 'set' and 'get' callbacks are
    never called.

        The 'set' callback is called before a new value is copied into the
    property.  H5P_prp_set_func_t is defined as:
        typedef herr_t (*H5P_prp_set_func_t)(hid_t prop_id, const char *name,
            size_t size, void *value);
    where the parameters to the callback function are:
        hid_t prop_id;      IN: The ID of the property list being modified.
        const char *name;   IN: The name of the property being modified.
        size_t size;        IN: The size of the property value
        void *new_value;    IN/OUT: The value being set for the property.
    The 'set' routine may modify the value to be set and those changes will be
    stored as the value of the property.  If the 'set' routine returns a
    negative value, the new property value is not copied into the property and
    the property list set routine returns an error value.

        The 'get' callback is called before a value is retrieved from the
    property.  H5P_prp_get_func_t is defined as:
        typedef herr_t (*H5P_prp_get_func_t)(hid_t prop_id, const char *name,
            size_t size, void *value);
    where the parameters to the callback function are:
        hid_t prop_id;      IN: The ID of the property list being queried.
        const char *name;   IN: The name of the property being queried.
        size_t size;        IN: The size of the property value
        void *value;        IN/OUT: The value being retrieved for the property.
    The 'get' routine may modify the value to be retrieved and those changes
    will be returned to the calling function.  If the 'get' routine returns a
    negative value, the property value is returned and the property list get
    routine returns an error value.

        The 'delete' callback is called when a property is deleted from a
    property list.  H5P_prp_del_func_t is defined as:
        typedef herr_t (*H5P_prp_del_func_t)(hid_t prop_id, const char *name,
            size_t size, void *value);
    where the parameters to the callback function are:
        hid_t prop_id;      IN: The ID of the property list the property is deleted from.
        const char *name;   IN: The name of the property being deleted.
        size_t size;        IN: The size of the property value
        void *value;        IN/OUT: The value of the property being deleted.
    The 'delete' routine may modify the value passed in, but the value is not
    used by the library when the 'delete' routine returns.  If the
    'delete' routine returns a negative value, the property list deletion
    routine returns an error value but the property is still deleted.

        The 'copy' callback is called when a property list with this
    property is copied.  H5P_prp_copy_func_t is defined as:
        typedef herr_t (*H5P_prp_copy_func_t)(const char *name, size_t size,
            void *value);
    where the parameters to the callback function are:
        const char *name;   IN: The name of the property being copied.
        size_t size;        IN: The size of the property value
        void *value;        IN: The value of the property being copied.
    The 'copy' routine may modify the value to be copied and those changes will be
    stored as the value of the property.  If the 'copy' routine returns a
    negative value, the new property value is not copied into the property and
    the property list copy routine returns an error value.

        The 'compare' callback is called when a property list with this
    property is compared to another property list.  H5P_prp_compare_func_t is
    defined as:
        typedef int (*H5P_prp_compare_func_t)( void *value1, void *value2,
            size_t size);
    where the parameters to the callback function are:
        const void *value1; IN: The value of the first property being compared.
        const void *value2; IN: The value of the second property being compared.
        size_t size;        IN: The size of the property value
    The 'compare' routine may not modify the values to be compared.  The
    'compare' routine should return a positive value if VALUE1 is greater than
    VALUE2, a negative value if VALUE2 is greater than VALUE1 and zero if VALUE1
    and VALUE2 are equal.

        The 'close' callback is called when a property list with this
    property is being destroyed.  H5P_prp_close_func_t is defined as:
        typedef herr_t (*H5P_prp_close_func_t)(const char *name, size_t size,
            void *value);
    where the parameters to the callback function are:
        const char *name;   IN: The name of the property being closed.
        size_t size;        IN: The size of the property value
        void *value;        IN: The value of the property being closed.
    The 'close' routine may modify the value passed in, but the value is not
    used by the library when the 'close' routine returns.  If the
    'close' routine returns a negative value, the property list close
    routine returns an error value but the property list is still closed.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
        The 'set' callback function may be useful to range check the value being
    set for the property or may perform some transformation/translation of the
    value set.  The 'get' callback would then [probably] reverse the
    transformation, etc.  A single 'get' or 'set' callback could handle
    multiple properties by performing different actions based on the property
    name or other properties in the property list.

        There is no 'create' callback routine for temporary property list
    objects, the initial value is assumed to have any necessary setup already
    performed on it.

        I would like to say "the property list is not closed" when a 'close'
    routine fails, but I don't think that's possible due to other properties in
    the list being successfully closed & removed from the property list.  I
    suppose that it would be possible to just remove the properties which have
    successful 'close' callbacks, but I'm not happy with the ramifications
    of a mangled, un-closable property list hanging around...  Any comments? -QAK

 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5Pinsert2(hid_t plist_id, const char *name, size_t size, void *value, H5P_prp_set_func_t prp_set,
           H5P_prp_get_func_t prp_get, H5P_prp_delete_func_t prp_delete, H5P_prp_copy_func_t prp_copy,
           H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close)
{
    H5P_genplist_t *plist;     /* Property list to modify */
    herr_t          ret_value; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE10("e", "i*sz*xPSPGPDPOPMPL", plist_id, name, size, value, prp_set, prp_get, prp_delete, prp_copy,
              prp_cmp, prp_close);

    /* Check arguments. */
    if (NULL == (plist = (H5P_genplist_t *)H5I_object_verify(plist_id, H5I_GENPROP_LST)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");
    if (!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property name");
    if (size > 0 && value == NULL)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "properties >0 size must have default");

    /* Create the new property list class */
    if ((ret_value = H5P_insert(plist, name, size, value, prp_set, prp_get, NULL, NULL, prp_delete, prp_copy,
                                prp_cmp, prp_close)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to register property in plist");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pinsert2() */

/*--------------------------------------------------------------------------
 NAME
    H5Pset
 PURPOSE
    Routine to set a property's value in a property list.
 USAGE
    herr_t H5Pset(plist_id, name, value)
        hid_t plist_id;         IN: Property list to find property in
        const char *name;       IN: Name of property to set
        void *value;            IN: Pointer to the value for the property
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Sets a new value for a property in a property list.  The property name
    must exist or this routine will fail.  If there is a 'set' callback routine
    registered for this property, the 'value' will be passed to that routine and
    any changes to the 'value' will be used when setting the property value.
    The information pointed at by the 'value' pointer (possibly modified by the
    'set' callback) is copied into the property list value and may be changed
    by the application making the H5Pset call without affecting the property
    value.

        If the 'set' callback routine returns an error, the property value will
    not be modified.  This routine may not be called for zero-sized properties
    and will return an error in that case.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5Pset(hid_t plist_id, const char *name, const void *value)
{
    H5P_genplist_t *plist;               /* Property list to modify */
    herr_t          ret_value = SUCCEED; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE3("e", "i*s*x", plist_id, name, value);

    /* Check arguments. */
    if (NULL == (plist = (H5P_genplist_t *)H5I_object_verify(plist_id, H5I_GENPROP_LST)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");
    if (!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property name");
    if (value == NULL)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property value");

    /* Go set the value */
    if (H5P_set(plist, name, value) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to set value in plist");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pset() */

#ifdef H5_HAVE_MULTITHREAD

/****************************************************************************************
 * Function:    H5Pexist
 *
 * Purpose:     Multithread safe version of H5Pexist() which is a routine to query the
 *              existance of a property in a property list or property list class.
 *
 * Return:      Success: 1 (TRUE) if it exists,
 *                       0 (FALSE) if not
 *
 *              Failure: -1
 *
 ****************************************************************************************
 */
htri_t
H5Pexist(hid_t id, const char *name)
{
    H5P_mt_list_t  *plist;  /* Property list to query */
    H5P_mt_class_t *pclass; /* Property class to query */

    htri_t ret_value; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE2("t", "i*s", id, name);

    /* Check arguments. */
    if (H5I_GENPROP_LST != H5I_get_type(id) && H5I_GENPROP_CLS != H5I_get_type(id)) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");
    }
    if (!name || !*name) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property name");
    }

    /* Check for the existence of the property in the list or class */
    if (H5I_GENPROP_LST == H5I_get_type(id)) {
        if (NULL == (plist = (H5P_genplist_t *)H5I_object(id))) {
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");
        }

        if ((ret_value = H5P_exist_plist(plist, name)) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "property does not exist in list");
        }
    }
    else if (H5I_GENPROP_CLS == H5I_get_type(id)) {
        if (NULL == (pclass = (H5P_genclass_t *)H5I_object(id))) {
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property class");
        }

        if ((ret_value = H5P__exist_pclass(pclass, name)) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "property does not exist in list");
        }

    } /* end if */
    else {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");
    }

done:

    FUNC_LEAVE_API(ret_value)

} /* H5Pexist() MT safe version */

#else

/*--------------------------------------------------------------------------
 NAME
    H5Pexist
 PURPOSE
    Routine to query the existence of a property in a property object.
 USAGE
    htri_t H5P_exist(id, name)
        hid_t id;           IN: Property object ID to check
        const char *name;   IN: Name of property to check for
 RETURNS
    Success: Positive if the property exists in the property object, zero
            if the property does not exist.
    Failure: negative value
 DESCRIPTION
        This routine checks if a property exists within a property list or
    class.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
htri_t
H5Pexist(hid_t id, const char *name)
{
    H5P_genplist_t *plist;  /* Property list to query */
    H5P_genclass_t *pclass; /* Property class to query */

    htri_t ret_value; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE2("t", "i*s", id, name);

    /* Check arguments. */
    if (H5I_GENPROP_LST != H5I_get_type(id) && H5I_GENPROP_CLS != H5I_get_type(id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");
    if (!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property name");

    /* Check for the existence of the property in the list or class */
    if (H5I_GENPROP_LST == H5I_get_type(id)) {
        if (NULL == (plist = (H5P_genplist_t *)H5I_object(id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");
        if ((ret_value = H5P_exist_plist(plist, name)) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "property does not exist in list");
    } /* end if */
    else if (H5I_GENPROP_CLS == H5I_get_type(id)) {
        if (NULL == (pclass = (H5P_genclass_t *)H5I_object(id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property class");
        if ((ret_value = H5P__exist_pclass(pclass, name)) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "property does not exist in class");
    } /* end if */
    else
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pexist() */

#endif

#ifdef H5_HAVE_MULTITHREAD

/****************************************************************************************
 * Function:    H5Pget_size
 *
 * Purpose:     Multithread version of H5Pget_size() which queries the size of a property
 *              in a property list or property list class.
 *
 *              NOTE: The only real difference in the MT (multithread) version of this
 *              function is instead of calling H5P__get_size_plist() or
 *              H5P__get_size_pclass(), it calls H5P__mt_search__list() or
 *              H5P__mt_search__class() to get the property and then returns the size
 *              respectively.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5Pget_size(hid_t id, const char *name, size_t *size /*out*/)
{
    H5P_mt_class_t *pclass;
    H5P_mt_list_t  *plist;

    herr_t ret_value = SUCCEED; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE3("e", "i*sx", id, name, size);

    /* Check arguments. */
    if (H5I_GENPROP_LST != H5I_get_type(id) && H5I_GENPROP_CLS != H5I_get_type(id)) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");
    }
    if (!name || !*name) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property name");
    }
    if (size == NULL) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property size");
    }

    if (H5I_GENPROP_LST == H5I_get_type(id)) {
        if (NULL == (plist = (H5P_mt_list_t *)H5I_object(id))) {
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a MT property list");
        }

        /* Check the property size */
        if ((ret_value = H5P__get_size_plist(plist, name, size)) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to query size in plist");
    }
    else if (H5I_GENPROP_CLS == H5I_get_type(id)) {
        if (NULL == (pclass = (H5P_mt_class_t *)H5I_object(id))) {
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a MT property list class");
        }

        /* Check the property size */
        if ((ret_value = H5P__get_size_pclass(pclass, name, size)) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to query size in plist");
    }
    else {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");
    }

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pget_size() */

#else
/*--------------------------------------------------------------------------
 NAME
    H5Pget_size
 PURPOSE
    Routine to query the size of a property in a property list or class.
 USAGE
    herr_t H5Pget_size(id, name)
        hid_t id;               IN: ID of property list or class to check
        const char *name;       IN: Name of property to query
        size_t *size;           OUT: Size of property
 RETURNS
    Success: non-negative value
    Failure: negative value
 DESCRIPTION
        This routine retrieves the size of a property's value in bytes.  Zero-
    sized properties are allowed and return a value of 0.  This function works
    for both property lists and classes.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5Pget_size(hid_t id, const char *name, size_t *size /*out*/)
{
    H5P_genclass_t *pclass; /* Property class to query */
    H5P_genplist_t *plist;  /* Property list to query */

    herr_t ret_value; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE3("e", "i*sx", id, name, size);

    /* Check arguments. */
    if (H5I_GENPROP_LST != H5I_get_type(id) && H5I_GENPROP_CLS != H5I_get_type(id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");
    if (!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property name");
    if (size == NULL)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property size");

    if (H5I_GENPROP_LST == H5I_get_type(id)) {
        if (NULL == (plist = (H5P_genplist_t *)H5I_object(id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");

        /* Check the property size */
        if ((ret_value = H5P__get_size_plist(plist, name, size)) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to query size in plist");
    } /* end if */
    else if (H5I_GENPROP_CLS == H5I_get_type(id)) {
        if (NULL == (pclass = (H5P_genclass_t *)H5I_object(id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");

        /* Check the property size */
        if ((ret_value = H5P__get_size_pclass(pclass, name, size)) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to query size in plist");
    } /* end if */
    else
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pget_size() */

#endif

/*--------------------------------------------------------------------------
 NAME
    H5Pencode2
 PURPOSE
    Routine to convert the property values in a property list into a binary buffer.
    The encoding of property values will be done according to the file format
    setting in fapl_id.
 USAGE
    herr_t H5Pencode(plist_id, buf, nalloc, fapl_id)
        hid_t plist_id;         IN: Identifier to property list to encode
        void *buf:              OUT: buffer to gold the encoded plist
        size_t *nalloc;         IN/OUT: size of buffer needed to encode plist
        hid_t fapl_id;          IN: File access property list ID
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
    Encodes a property list into a binary buffer. If the buffer is NULL, then
    the call will set the size needed to encode the plist in nalloc. Otherwise
    the routine will encode the plist in buf.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5Pencode2(hid_t plist_id, void *buf, size_t *nalloc, hid_t fapl_id)
{
    H5P_genplist_t *plist;               /* Property list to query */
    herr_t          ret_value = SUCCEED; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE4("e", "i*x*zi", plist_id, buf, nalloc, fapl_id);

    /* Check arguments. */
    if (NULL == (plist = (H5P_genplist_t *)H5I_object_verify(plist_id, H5I_GENPROP_LST)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");

    /* Verify access property list and set up collective metadata if appropriate */
    if (H5CX_set_apl(&fapl_id, H5P_CLS_FACC, H5I_INVALID_HID, TRUE) < 0)
        HGOTO_ERROR(H5E_FILE, H5E_CANTSET, H5I_INVALID_HID, "can't set access property list info");

    /* Call the internal encode routine */
    if ((ret_value = H5P__encode(plist, TRUE, buf, nalloc)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL, "unable to encode property list");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pencode2() */

/*--------------------------------------------------------------------------
 NAME
    H5Pdecode
 PURPOSE
    API routine to decode a property list from a binary buffer.
 USAGE
    hid_t H5Pdecode(buf)
        void *buf;    IN: buffer that holds the encoded plist
 RETURNS
    Success: ID of new property list object
    Failure: H5I_INVALID_HID (negative)
 DESCRIPTION
     Decodes a property list from a binary buffer. The contents of the buffer
     contain the values for the corresponding properties of the plist. The decode
     callback of a certain property decodes its value from the buffer and sets it
     in the property list.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
     Properties in the property list that are not encoded in the serialized
     form retain their default value.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
hid_t
H5Pdecode(const void *buf)
{
    hid_t ret_value = H5I_INVALID_HID; /* return value */

    FUNC_ENTER_API(H5I_INVALID_HID)
    H5TRACE1("i", "*x", buf);

    /* Call the internal decode routine */
    if ((ret_value = H5P__decode(buf)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTDECODE, H5I_INVALID_HID, "unable to decode property list");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pdecode() */

#ifdef H5_HAVE_MULTITHREAD

/****************************************************************************************
 * Function:    H5Pget_class
 *
 * Purpose:     Multithread version of H5Pget_class() which queries the class of a
 *              property list.
 *
 *              NOTE: The original version of this function would create a new ID in the
 *              index for the class, regardless of whether it already had one or not.
 *              In this multithread version we are trying to avoid that, and are instead
 *              simply incrementing the ref count of the ID in the index and returning
 *              the existing ID.
 *              However, it is possible that the class's ID gets decrement to 0 via a
 *              call to H5Pclose_class(). Generally this won't happen for the standard
 *              hdf5 library property list classes, however, it does occur in testing
 *              due to a new unique class being created to perform the tests on. Meaning
 *              that a user designed class could potentially have this happen as well.
 *              Since the property list class structure will still be available, due to
 *              having a derived property list that still has a pointer to the class
 *              (the class structure will only be closed and added to the free list if it
 *              has no derived property lists or property list classes, for more info on
 *              this process see the typedef H5P_mt_class_t struct comment description in
 *              H5Pint_mt.c), a new ID will be assigned to the class since it does not
 *              have an existing ID in the index. This prevents a property list class
 *              from ever having two IDs but still able to be removed and re-added to the
 *              index as needed.
 *
 * Return:      Success: Returns the ID of the parent class
 *
 *              Failure: H5I_INVALID_HID (-1)
 *
 ****************************************************************************************
 */
hid_t
H5Pget_class(hid_t plist_id)
{
    H5P_mt_class_t           *pclass;
    H5P_mt_class_ref_counts_t ref_count;
    H5P_mt_class_ref_counts_t update_ref;
    H5P_mt_list_t            *plist;
    hid_t                     pclass_id;

    hid_t ret_value = H5I_INVALID_HID; /* return value */

    FUNC_ENTER_API(H5I_INVALID_HID)
    H5TRACE1("i", "i", plist_id);

    /* Check arguments. */
    if (NULL == (plist = (H5P_mt_list_t *)H5I_object_verify(plist_id, H5I_GENPROP_LST))) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5I_INVALID_HID, "not a MT property list");
    }

    /* Retrieve the MT parent property list class */
    if (NULL == (pclass = H5P_get_class(plist))) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, H5I_INVALID_HID, "unable to query class of property list");
    }

    /* Get the ID of the parent class */
    pclass_id = pclass->id;

    /* Increment the ref count of the parent class in the index */
    if (0 >= H5I_inc_ref(pclass_id, TRUE)) {

        /**
         * NOTE: The original H5P code didn't use H5I_inc_ref(), instead using H5I_register() to
         * assign a new ID to the class regardless of whether the class had an existing ID or not.
         * Meaning that multiple IDs in the index could point to the same class instance. To
         * prevent this, the use of H5I_inc_ref() is implemented to simply increment the index's
         * reference count on the ID for that class.
         *
         * However, if an ID's ref_count for a class were to drop to 0 the ID would be removed
         * from the index, but if the class has existing derived classes or lists the structure
         * would not be deleted. Then if H5Pget_class() is called on a derived list or class
         * H5I_inc_ref() would fail (which is most likely why the original H5P code used
         * H5I_register() ), so H5I_register_using_existing_id() is called to attempt to re-insert
         * a class's ID back into the index (H5P_mt_class_t structs have a field for their ID).
         * There is a chance H5I_register_using _existing_id() could fail if the ID has been
         * assigned to another instance of H5P_mt_class_t (which is unlikely to occur). But if it
         * does fail H5I_register() is called to register a new ID in the index for the class.
         *
         * This is something that should not occur during regular use of the library, however, does
         * occur during existing H5P tests. This design is mostly done to handle that test, and to
         * handle the off chance that a class is closed enough times it is removed from the index before
         * it's derived classes and lists are closed.
         */

        /* If the class's ID isn't in the index, register it back in */
        if (0 > H5I_register_using_existing_id(H5I_GENPROP_CLS, pclass, TRUE, pclass_id)) {
            /**
             * If registering the class's id back into the index fails,
             * register it with a new id.
             */
            if ((pclass_id = H5I_register(H5I_GENPROP_CLS, pclass, TRUE)) < 0) {
                HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, H5I_INVALID_HID,
                            "unable to register property list class");
            }

            /* Update the class's ID to the new assigned ID */
            atomic_store(&(pclass->id), pclass_id);
        }

        ref_count = atomic_load(&(pclass->ref_count));

        /* If marked deleted, unmark it, since it's ID is back in the index */
        if (ref_count.deleted) {
            update_ref         = ref_count;
            update_ref.deleted = FALSE;

            if (!atomic_compare_exchange_strong(&(pclass->ref_count), &ref_count, update_ref)) {
                atomic_fetch_add(&(pclass->num_ref_count_cols), 1);
            }
            else {
                atomic_fetch_add(&(pclass->num_ref_count_update), 1);
                atomic_fetch_add(&(pclass->num_ref_count_unmarked_deleted), 1);
                atomic_fetch_add(&(H5P_mt_g.class_un_marked_as_deleted), 1);
            }
        }
    }

    /* Return the parent's ID*/
    ret_value = pclass_id;

done:

    FUNC_LEAVE_API(ret_value)
} /* H5Pget_class() */

#else

/*--------------------------------------------------------------------------
 NAME
    H5Pget_class
 PURPOSE
    Routine to query the class of a generic property list
 USAGE
    hid_t H5Pget_class(plist_id)
        hid_t plist_id;         IN: Property list to query
 RETURNS
    Success: ID of class object
    Failure: H5I_INVALID_HID (negative)
 DESCRIPTION
    This routine retrieves the class of a property list.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
    Change the name of this function to H5Pget_class (and remove old H5Pget_class)
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
hid_t
H5Pget_class(hid_t plist_id)
{
    H5P_genplist_t *plist;                       /* Property list to query */
    H5P_genclass_t *pclass    = NULL;            /* Property list class */
    hid_t           ret_value = H5I_INVALID_HID; /* return value */

    FUNC_ENTER_API(H5I_INVALID_HID)
    H5TRACE1("i", "i", plist_id);

    /* Check arguments. */
    if (NULL == (plist = (H5P_genplist_t *)H5I_object_verify(plist_id, H5I_GENPROP_LST)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5I_INVALID_HID, "not a property list");

    /* Retrieve the property list class */
    if ((pclass = H5P_get_class(plist)) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, H5I_INVALID_HID, "unable to query class of property list");

    /* Increment the outstanding references to the class object */
    if (H5P__access_class(pclass, H5P_MOD_INC_REF) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, H5I_INVALID_HID, "Can't increment class ID ref count");

    /* Get an ID for the class */
    if ((ret_value = H5I_register(H5I_GENPROP_CLS, pclass, TRUE)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to register property list class");

done:

    if (H5I_INVALID_HID == ret_value && pclass)
        H5P__close_class(pclass);

    FUNC_LEAVE_API(ret_value)
} /* H5Pget_class() */

#endif

#ifdef H5_HAVE_MULTITHREAD

/****************************************************************************************
 * Function:    H5Pget_nprops
 *
 * Purpose:     Multithread version of H5Pget_nprops() which is a routine to query the
 *              number of properties in a property list or class.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5Pget_nprops(hid_t id, size_t *nprops /*out*/)
{
    H5P_mt_class_t *pclass;
    H5P_mt_list_t  *plist;

    herr_t ret_value = SUCCEED; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE2("e", "ix", id, nprops);

    /* Check arguments. */
    if (H5I_GENPROP_LST != H5I_get_type(id) && H5I_GENPROP_CLS != H5I_get_type(id)) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");
    }
    if (nprops == NULL) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property nprops pointer");
    }

    if (H5I_GENPROP_LST == H5I_get_type(id)) {
        if (NULL == (plist = (H5P_mt_list_t *)H5I_object(id))) {
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a MT property list");
        }

        /**
         * The multithread safe list structs us nprops to track the total number of
         * properties in the most current version of the lkup_tbl plus LFSLL of the
         * list. For more information on the multhithread structures see the description
         * in H5Ppkg_mt.h
         */
        *nprops = atomic_load(&(plist->nprops));
    }
    else if (H5I_GENPROP_CLS == H5I_get_type(id)) {
        if (NULL == (pclass = (H5P_mt_class_t *)H5I_object(id))) {
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a MT property class");
        }

        /**
         * The original H5P class structures don't inherite the properties from their
         * parent class, while the MT H5P class structures do. To account for this the
         * field nprops_added only tracks the number of properties that were added and
         * not inherited.
         */
        *nprops = atomic_load(&(pclass->nprops_added));
    }
    else {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");
    }

done:

    FUNC_LEAVE_API(ret_value)
} /* H5Pget_nprops() */

#else
/*--------------------------------------------------------------------------
 NAME
    H5Pget_nprops
 PURPOSE
    Routine to query the number of properties in a property list or class.
 USAGE
    herr_t H5Pget_nprops(id, nprops)
        hid_t id;               IN: ID of Property list or class to check
        size_t *nprops;         OUT: Number of properties in the property object
 RETURNS
    Success: non-negative value
    Failure: negative value
 DESCRIPTION
        This routine retrieves the number of properties in a property list or
    class.  If a property class ID is given, the number of registered properties
    in the class is returned in NPROPS.  If a property list ID is given, the
    current number of properties in the list is returned in NPROPS.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5Pget_nprops(hid_t id, size_t *nprops /*out*/)
{
    H5P_genplist_t *plist;  /* Property list to query */
    H5P_genclass_t *pclass; /* Property class to query */

    herr_t ret_value = SUCCEED; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE2("e", "ix", id, nprops);

    /* Check arguments. */
    if (H5I_GENPROP_LST != H5I_get_type(id) && H5I_GENPROP_CLS != H5I_get_type(id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");
    if (nprops == NULL)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property nprops pointer");

    if (H5I_GENPROP_LST == H5I_get_type(id)) {
        if (NULL == (plist = (H5P_genplist_t *)H5I_object(id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");
        if (H5P__get_nprops_plist(plist, nprops) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to query # of properties in plist");
    } /* end if */
    else if (H5I_GENPROP_CLS == H5I_get_type(id)) {
        if (NULL == (pclass = (H5P_genclass_t *)H5I_object(id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property class");
        if (H5P_get_nprops_pclass(pclass, nprops, FALSE) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to query # of properties in pclass");
    } /* end if */
    else
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pget_nprops() */

#endif

#ifdef H5_HAVE_MULTITHREAD

/****************************************************************************************
 * Function:    H5Pequal
 *
 * Purpose:     Multithread version of H5Pequal() which is a routine to query whether
 *              two property lists or two property list classes are equal.
 *
 *              NOTE: The only difference in the multithread version of this function
 *              and the original is const is removed from H5P_genplist_t and
 *              H5P_genclass_t in the parameters of the functions H5P__cmp_plist() and
 *              H5P__cmp_class() respectively. This was done because the multithread
 *              structures track the number of threads that currently are accessing them,
 *              thus a field will always be modified.
 *
 *
 * Return:      Success: TRUE if equal, FALSE if unequal
 *
 *              Failure: negative
 *
 ****************************************************************************************
 */
htri_t
H5Pequal(hid_t id1, hid_t id2)
{
    void  *obj1, *obj2; /* Property objects to compare */
    int    cmp_ret   = 0;
    htri_t ret_value = FALSE; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE2("t", "ii", id1, id2);

    /* Check arguments. */
    if ((H5I_GENPROP_LST != H5I_get_type(id1) && H5I_GENPROP_CLS != H5I_get_type(id1)) ||
        (H5I_GENPROP_LST != H5I_get_type(id2) && H5I_GENPROP_CLS != H5I_get_type(id2))) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not property objects");
    }
    if (H5I_get_type(id1) != H5I_get_type(id2)) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not the same kind of property objects");
    }
    if (NULL == (obj1 = H5I_object(id1)) || NULL == (obj2 = H5I_object(id2))) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property object doesn't exist");
    }

    /* Compare property lists */
    if (H5I_GENPROP_LST == H5I_get_type(id1)) {
        if (H5P__cmp_plist((H5P_genplist_t *)obj1, (H5P_genplist_t *)obj2, &cmp_ret) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOMPARE, FAIL, "can't compare property lists");
        }

        /* Set return value */
        ret_value = cmp_ret == 0 ? TRUE : FALSE;

    } /* end if */
    /* Must be property classes */
    else {
        if (H5P__cmp_class((H5P_genclass_t *)obj1, (H5P_genclass_t *)obj2) == 0) {
            ret_value = TRUE;
        }

    } /* end else */

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pequal() */

#else

/*--------------------------------------------------------------------------
 NAME
    H5Pequal
 PURPOSE
    Routine to query whether two property lists or two property classes are equal
 USAGE
    htri_t H5Pequal(id1, id2)
        hid_t id1;         IN: Property list or class ID to compare
        hid_t id2;         IN: Property list or class ID to compare
 RETURNS
    Success: TRUE if equal, FALSE if unequal
    Failure: negative
 DESCRIPTION
    Determines whether two property lists or two property classes are equal.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
htri_t
H5Pequal(hid_t id1, hid_t id2)
{
    void  *obj1, *obj2; /* Property objects to compare */
    int    cmp_ret   = 0;
    htri_t ret_value = FALSE; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE2("t", "ii", id1, id2);

    /* Check arguments. */
    if ((H5I_GENPROP_LST != H5I_get_type(id1) && H5I_GENPROP_CLS != H5I_get_type(id1)) ||
        (H5I_GENPROP_LST != H5I_get_type(id2) && H5I_GENPROP_CLS != H5I_get_type(id2)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not property objects");
    if (H5I_get_type(id1) != H5I_get_type(id2))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not the same kind of property objects");
    if (NULL == (obj1 = H5I_object(id1)) || NULL == (obj2 = H5I_object(id2)))
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property object doesn't exist");

    /* Compare property lists */
    if (H5I_GENPROP_LST == H5I_get_type(id1)) {
        if (H5P__cmp_plist((const H5P_genplist_t *)obj1, (const H5P_genplist_t *)obj2, &cmp_ret) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOMPARE, FAIL, "can't compare property lists");

        /* Set return value */
        ret_value = cmp_ret == 0 ? TRUE : FALSE;
    } /* end if */
    /* Must be property classes */
    else {
        if (H5P__cmp_class((const H5P_genclass_t *)obj1, (const H5P_genclass_t *)obj2) == 0)
            ret_value = TRUE;
    } /* end else */

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pequal() */

#endif

/*--------------------------------------------------------------------------
 NAME
    H5Pisa_class
 PURPOSE
    Routine to query whether a property list is a certain class
 USAGE
    hid_t H5Pisa_class(plist_id, pclass_id)
        hid_t plist_id;         IN: Property list to query
        hid_t pclass_id;        IN: Property class to query
 RETURNS
    Success: TRUE (1) or FALSE (0)
    Failure: negative
 DESCRIPTION
    This routine queries whether a property list is a member of the property
    list class.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
    What about returning a value indicating that the property class is further
    up the class hierarchy?
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
htri_t
H5Pisa_class(hid_t plist_id, hid_t pclass_id)
{
    htri_t ret_value; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE2("t", "ii", plist_id, pclass_id);

    /* Check arguments. */
    if (H5I_GENPROP_LST != H5I_get_type(plist_id)) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");
    }
    if (H5I_GENPROP_CLS != H5I_get_type(pclass_id)) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property class");
    }

    /* Compare the property list's class against the other class */
    if ((ret_value = H5P_isa_class(plist_id, pclass_id)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to compare property list classes");
    }

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pisa_class() */

/*--------------------------------------------------------------------------
 NAME
    H5P__iterate_cb
 PURPOSE
    Internal callback routine when iterating over properties in property list
    or class
 USAGE
    int H5P__iterate_cb(prop, udata)
        H5P_genprop_t *prop;        IN: Pointer to the property
        void *udata;                IN/OUT: Pointer to iteration data from user
 RETURNS
    Success: Returns the return value of the last call to ITER_FUNC
    Failure: negative value
 DESCRIPTION
    This routine calls the actual callback routine for the property in the
property list or class.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static int
H5P__iterate_cb(H5P_genprop_t *prop, void *_udata)
{
    H5P_iter_ud_t *udata     = (H5P_iter_ud_t *)_udata; /* Pointer to user data */
    int            ret_value = 0;                       /* Return value */

    FUNC_ENTER_PACKAGE_NOERR

    /* Sanity check */
    assert(prop);
    assert(udata);

    /* Call the user's callback routine */
    ret_value = (*udata->iter_func)(udata->id, prop->name, udata->iter_data);

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P__iterate_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5Piterate
 PURPOSE
    Routine to iterate over the properties in a property list or class
 USAGE
    int H5Piterate(pclass_id, idx, iter_func, iter_data)
        hid_t id;                   IN: ID of property object to iterate over
        int *idx;                   IN/OUT: Index of the property to begin with
        H5P_iterate_t iter_func;    IN: Function pointer to function to be
                                        called with each property iterated over.
        void *iter_data;            IN/OUT: Pointer to iteration data from user
 RETURNS
    Success: Returns the return value of the last call to ITER_FUNC if it was
                non-zero, or zero if all properties have been processed.
    Failure: negative value
 DESCRIPTION
    This routine iterates over the properties in the property object specified
with ID.  The properties in both property lists and classes may be iterated
over with this function.  For each property in the object, the ITER_DATA and
some additional information, specified below, are passed to the ITER_FUNC
function.  The iteration begins with the IDX property in the object and the
next element to be processed by the operator is returned in IDX.  If IDX is
NULL, then the iterator starts at the first property; since no stopping point
is returned in this case, the iterator cannot be restarted if one of the calls
to its operator returns non-zero.  The IDX value is 0-based (ie. to start at
the "first" property, the IDX value should be 0).

The prototype for H5P_iterate_t is:
    typedef herr_t (*H5P_iterate_t)(hid_t id, const char *name, void *iter_data);
The operation receives the property list or class identifier for the object
being iterated over, ID, the name of the current property within the object,
NAME, and the pointer to the operator data passed in to H5Piterate, ITER_DATA.

The return values from an operator are:
    Zero causes the iterator to continue, returning zero when all properties
        have been processed.
    Positive causes the iterator to immediately return that positive value,
        indicating short-circuit success. The iterator can be restarted at the
        index of the next property.
    Negative causes the iterator to immediately return that value, indicating
        failure. The iterator can be restarted at the index of the next
        property.

H5Piterate assumes that the properties in the object identified by ID remains
unchanged through the iteration.  If the membership changes during the
iteration, the function's behavior is undefined.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
int
H5Piterate(hid_t id, int *idx, H5P_iterate_t iter_func, void *iter_data)
{
    H5P_iter_ud_t udata;        /* User data for internal iterator callback */
    int           fake_idx = 0; /* Index when user doesn't provide one */
    void         *obj;          /* Property object to copy */
    int           ret_value;    /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE4("Is", "i*IsPi*x", id, idx, iter_func, iter_data);

    /* Check arguments. */
    if (H5I_GENPROP_LST != H5I_get_type(id) && H5I_GENPROP_CLS != H5I_get_type(id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");
    if (NULL == (obj = H5I_object(id)))
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property object doesn't exist");
    if (iter_func == NULL)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid iteration callback");

    /* Set up user data */
    udata.iter_func = iter_func;
    udata.id        = id;
    udata.iter_data = iter_data;

    if (H5I_GENPROP_LST == H5I_get_type(id)) {
        /* Iterate over a property list */
        if ((ret_value = H5P__iterate_plist((H5P_genplist_t *)obj, TRUE, (idx ? idx : &fake_idx),
                                            H5P__iterate_cb, &udata)) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to iterate over list");
    } /* end if */
    else if (H5I_GENPROP_CLS == H5I_get_type(id)) {
        /* Iterate over a property class */
        if ((ret_value = H5P__iterate_pclass((H5P_genclass_t *)obj, (idx ? idx : &fake_idx), H5P__iterate_cb,
                                             &udata)) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to iterate over class");
    } /* end if */
    else
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property object");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Piterate() */

/*--------------------------------------------------------------------------
 NAME
    H5Pget
 PURPOSE
    Routine to query the value of a property in a property list.
 USAGE
    herr_t H5Pget(plist_id, name, value)
        hid_t plist_id;         IN: Property list to check
        const char *name;       IN: Name of property to query
        void *value;            OUT: Pointer to the buffer for the property value
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Retrieves a copy of the value for a property in a property list.  The
    property name must exist or this routine will fail.  If there is a
    'get' callback routine registered for this property, the copy of the
    value of the property will first be passed to that routine and any changes
    to the copy of the value will be used when returning the property value
    from this routine.
        If the 'get' callback routine returns an error, 'value' will not be
    modified and this routine will return an error.  This routine may not be
    called for zero-sized properties.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5Pget(hid_t plist_id, const char *name, void *value /*out*/)
{
    H5P_genplist_t *plist; /* Property list pointer */

    herr_t ret_value = SUCCEED; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE3("e", "i*sx", plist_id, name, value);

    /* Check arguments. */
    if (NULL == (plist = (H5P_genplist_t *)H5I_object_verify(plist_id, H5I_GENPROP_LST)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");
    if (!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property name");
    if (value == NULL)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property value");

    /* Go get the value */
    if (H5P_get(plist, name, value) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "unable to query property value");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pget() */

/*--------------------------------------------------------------------------
 NAME
    H5Premove
 PURPOSE
    Routine to remove a property from a property list.
 USAGE
    herr_t H5Premove(plist_id, name)
        hid_t plist_id;         IN: Property list to modify
        const char *name;       IN: Name of property to remove
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Removes a property from a property list.  Both properties which were
    in existence when the property list was created (i.e. properties registered
    with H5Pregister2) and properties added to the list after it was created
    (i.e. added with H5Pinsert2) may be removed from a property list.
    Properties do not need to be removed a property list before the list itself
    is closed, they will be released automatically when H5Pclose is called.
    The 'close' callback for this property is called before the property is
    release, if the callback exists.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5Premove(hid_t plist_id, const char *name)
{
    H5P_genplist_t *plist;     /* Property list to modify */
    herr_t          ret_value; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE2("e", "i*s", plist_id, name);

    /* Check arguments. */
    if (NULL == (plist = (H5P_genplist_t *)H5I_object_verify(plist_id, H5I_GENPROP_LST)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");
    if (!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property name");

    /* Create the new property list class */
    if ((ret_value = H5P_remove(plist, name)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTDELETE, FAIL, "unable to remove property");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Premove() */

/*--------------------------------------------------------------------------
 NAME
    H5Pcopy_prop
 PURPOSE
    Routine to copy a property from one list or class to another
 USAGE
    herr_t H5Pcopy_prop(dst_id, src_id, name)
        hid_t dst_id;               IN: ID of destination property list or class
        hid_t src_id;               IN: ID of source property list or class
        const char *name;           IN: Name of property to copy
 RETURNS
    Success: non-negative value.
    Failure: negative value.
 DESCRIPTION
    Copies a property from one property list or class to another.

    If a property is copied from one class to another, all the property
    information will be first deleted from the destination class and then the
    property information will be copied from the source class into the
    destination class.

    If a property is copied from one list to another, the property will be
    first deleted from the destination list (generating a call to the 'close'
    callback for the property, if one exists) and then the property is copied
    from the source list to the destination list (generating a call to the
    'copy' callback for the property, if one exists).

    If the property does not exist in the destination class or list, this call
    is equivalent to calling H5Pregister2 or H5Pinsert2 (for a class or list, as
    appropriate) and the 'create' callback will be called in the case of the
    property being copied into a list (if such a callback exists for the
    property).

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5Pcopy_prop(hid_t dst_id, hid_t src_id, const char *name)
{
    H5I_type_t src_id_type, dst_id_type; /* ID types */
    herr_t     ret_value = SUCCEED;      /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE3("e", "ii*s", dst_id, src_id, name);

    /* Check arguments. */
    if ((src_id_type = H5I_get_type(src_id)) < 0)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid source ID");
    if ((dst_id_type = H5I_get_type(dst_id)) < 0)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid destination ID");
    if ((H5I_GENPROP_LST != src_id_type && H5I_GENPROP_CLS != src_id_type) ||
        (H5I_GENPROP_LST != dst_id_type && H5I_GENPROP_CLS != dst_id_type))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not property objects");
    if (src_id_type != dst_id_type)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not the same kind of property objects");
    if (!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name given");

    /* Compare property lists */
    if (H5I_GENPROP_LST == src_id_type) {
        if (H5P__copy_prop_plist(dst_id, src_id, name) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "can't copy property between lists");
    } /* end if */
    /* Must be property classes */
    else {
        if (H5P__copy_prop_pclass(dst_id, src_id, name) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "can't copy property between classes");
    } /* end else */

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pcopy_prop() */

#ifdef H5_HAVE_MULTITHREAD

/****************************************************************************************
 * Function:    H5Punregister
 *
 * Purpose:     Multithread version of H5Punregister() which is a routine to remove a
 *              property from a property list class.
 *
 *              NOTE: The process of removing a property list or property list class in
 *              the multithread safe version of H5P differs from the original version of
 *              H5P. The multithread safe H5P structures use a versioning system to
 *              ensure threads don't have stuff deleted out from under them. Thus the
 *              multithread process is to set the delete_version of the property at the
 *              next version of the class, and then to increment the class's version to
 *              that next version. With the delete_version set that property will no
 *              longer be a valid property and will not be accessed in the future,
 *              effectively removing the property from the class.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5Punregister(hid_t pclass_id, const char *name)
{
    H5P_mt_class_t *pclass;

    herr_t ret_value; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE2("e", "i*s", pclass_id, name);

    /* Check arguments. */
    if (NULL == (pclass = (H5P_mt_class_t *)H5I_object_verify(pclass_id, H5I_GENPROP_CLS))) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list class");
    }

    if (!name || !*name) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property name");
    }

    /* Set the delete version of the property */
    if ((ret_value = H5P__mt_delete_prop__class(pclass, name)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to remove property from class");
    }

done:

    FUNC_LEAVE_API(ret_value)

} /* H5Punregister() */

#else

/*--------------------------------------------------------------------------
 NAME
    H5Punregister
 PURPOSE
    Routine to remove a property from a property list class.
 USAGE
    herr_t H5Punregister(pclass_id, name)
        hid_t pclass_id;         IN: Property list class to modify
        const char *name;       IN: Name of property to remove
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Removes a property from a property list class.  Future property lists
    created of that class will not contain this property.  Existing property
    lists containing this property are not affected.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5Punregister(hid_t pclass_id, const char *name)
{
    H5P_genclass_t *pclass;    /* Property list class to modify */
    herr_t          ret_value; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE2("e", "i*s", pclass_id, name);

    /* Check arguments. */
    if (NULL == (pclass = (H5P_genclass_t *)H5I_object_verify(pclass_id, H5I_GENPROP_CLS)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list class");
    if (!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid property name");

    /* Remove the property list from class */
    if ((ret_value = H5P__unregister(pclass, name)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to remove property from class");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Punregister() */

#endif

/*--------------------------------------------------------------------------
 NAME
    H5Pclose
 PURPOSE
    Routine to close a property list.
 USAGE
    herr_t H5Pclose(plist_id)
        hid_t plist_id;       IN: Property list to close
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Closes a property list.  If a 'close' callback exists for the property
    list class, it is called before the property list is destroyed.  If 'close'
    callbacks exist for any individual properties in the property list, they are
    called after the class 'close' callback.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5Pclose(hid_t plist_id)
{
    herr_t ret_value = SUCCEED; /* return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE1("e", "i", plist_id);

    /* Allow default property lists to pass through without throwing an error */
    if (H5P_DEFAULT != plist_id) {
        /* Check arguments. */
        if (H5I_GENPROP_LST != H5I_get_type(plist_id)) {
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");
        }

        /* Close the property list */
        if (H5I_dec_app_ref(plist_id) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTFREE, FAIL, "can't close");
        }

    } /* end if */

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pclose() */

#ifdef H5_HAVE_MULTITHREAD

/****************************************************************************************
 * Function:    H5Pget_class_name
 *
 * Purpose:     Multithread version of H5Pget_class_name() which is a routine to query
 *              the name of a property list class
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
char *
H5Pget_class_name(hid_t pclass_id)
{
    H5P_mt_class_t *pclass;

    char *ret_value; /* return value */

    FUNC_ENTER_API(NULL)
    H5TRACE1("*s", "i", pclass_id);

    /* Check arguments. */
    if (NULL == (pclass = (H5P_mt_class_t *)H5I_object_verify(pclass_id, H5I_GENPROP_CLS))) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a MT property class");
    }

    ret_value = strdup(pclass->name);

    if (!ret_value) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, NULL, "unable to query name of MT class");
    }

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pget_class_name() */

#else

/*--------------------------------------------------------------------------
 NAME
    H5Pget_class_name
 PURPOSE
    Routine to query the name of a generic property list class
 USAGE
    char *H5Pget_class_name(pclass_id)
        hid_t pclass_id;         IN: Property class to query
 RETURNS
    Success: Pointer to a malloc'ed string containing the class name
    Failure: NULL
 DESCRIPTION
        This routine retrieves the name of a generic property list class.
    The pointer to the name must be free'd by the user for successful calls.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
char *
H5Pget_class_name(hid_t pclass_id)
{
    H5P_genclass_t *pclass;    /* Property class to query */
    char           *ret_value; /* return value */

    FUNC_ENTER_API(NULL)
    H5TRACE1("*s", "i", pclass_id);

    /* Check arguments. */
    if (NULL == (pclass = (H5P_genclass_t *)H5I_object_verify(pclass_id, H5I_GENPROP_CLS)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a property class");

    /* Get the property list class name */
    if ((ret_value = H5P_get_class_name(pclass)) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, NULL, "unable to query name of class");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pget_class_name() */

#endif

#ifdef H5_HAVE_MULTITHREAD

/****************************************************************************************
 * Function:    H5Pget_class_parent
 *
 * Purpose:     Multithread safe version of H5Pget_class_parent() which is a routine to
 *              query the parent class of a property list class.
 *
 *              NOTE: The original version of this function would create a new ID in the
 *              index for the class, regardless of whether it already had one or not.
 *              In this multithread version we are trying to avoid that, and are instead
 *              simply incrementing the ref count of the ID in the index and returning
 *              the existing ID.
 *              However, it is possible that the class's ID gets decrement to 0 via a
 *              call to H5Pclose_class(). Generally this won't happen for the standard
 *              hdf5 library property list classes, however, it does occur in testing
 *              due to a new unique class being created to perform the tests on. Meaning
 *              that a user designed class could potentially have this happen as well.
 *              Since the property list class structure will still be available, due to
 *              having a derived property list that still has a pointer to the class
 *              (the class structure will only be closed and added to the free list if it
 *              has no derived property lists or property list classes, for more info on
 *              this process see the typedef H5P_mt_class_t struct comment description in
 *              H5Pint_mt.c), a new ID will be assigned to the class since it does not
 *              have an existing ID in the index. This prevents a property list class
 *              from ever having two IDs but still able to be removed and re-added to the
 *              index as needed.
 *
 * Return:      Success: Pointer to a property class object
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
hid_t
H5Pget_class_parent(hid_t pclass_id)
{
    H5P_mt_class_t           *pclass;
    H5P_mt_class_t           *parent;
    H5P_mt_class_ref_counts_t ref_count;
    H5P_mt_class_ref_counts_t update_ref;
    hid_t                     parent_id;

    hid_t ret_value = H5I_INVALID_HID; /* return value */

    FUNC_ENTER_API(H5I_INVALID_HID)
    H5TRACE1("i", "i", pclass_id);

    /* Check arguments. */
    if (NULL == (pclass = (H5P_mt_class_t *)H5I_object_verify(pclass_id, H5I_GENPROP_CLS))) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5I_INVALID_HID, "not a MT property class");
    }

    /* Retrieve the ID for the property class's parent */
    parent_id = pclass->parent_id;

    /* Increment the ref count of the parent class in the index */
    if (0 >= H5I_inc_ref(parent_id, TRUE)) {
        /* Get an ID for the class if it doesn't have one */
        if ((0 > H5I_register_using_existing_id(H5I_GENPROP_CLS, pclass, TRUE, parent_id))) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, H5I_INVALID_HID,
                        "unable to register property list class");
        }

        parent = pclass->parent_ptr;

        ref_count = atomic_load(&(parent->ref_count));

        /* If marked deleted, unmark it, since it's ID is back in the index */
        if (ref_count.deleted) {
            update_ref         = ref_count;
            update_ref.deleted = FALSE;

            if (!atomic_compare_exchange_strong(&(pclass->ref_count), &ref_count, update_ref)) {
                atomic_fetch_add(&(pclass->num_ref_count_cols), 1);
            }
            else {
                atomic_fetch_add(&(pclass->num_ref_count_update), 1);
                atomic_fetch_add(&(pclass->num_ref_count_unmarked_deleted), 1);
                atomic_fetch_add(&(H5P_mt_g.class_un_marked_as_deleted), 1);
            }
        }
    }

    ret_value = parent_id;

done:

    FUNC_LEAVE_API(ret_value)

} /* H5Pget_class_parent() */

#else
/*--------------------------------------------------------------------------
 NAME
    H5Pget_class_parent
 PURPOSE
    routine to query the parent class of a generic property class
 USAGE
    hid_t H5Pget_class_parent(pclass_id)
        hid_t pclass_id;         IN: Property class to query
 RETURNS
    Success: ID of parent class object
    Failure: H5I_INVALID_HID (negative)
 DESCRIPTION
    This routine retrieves an ID for the parent class of a property class.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
hid_t
H5Pget_class_parent(hid_t pclass_id)
{
    H5P_genclass_t *pclass;        /* Property class to query */
    H5P_genclass_t *parent = NULL; /* Parent's property class */

    hid_t ret_value = H5I_INVALID_HID; /* return value */

    FUNC_ENTER_API(H5I_INVALID_HID)
    H5TRACE1("i", "i", pclass_id);

    /* Check arguments. */
    if (NULL == (pclass = (H5P_genclass_t *)H5I_object_verify(pclass_id, H5I_GENPROP_CLS)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5I_INVALID_HID, "not a property class");

    /* Retrieve the property class's parent */
    if (NULL == (parent = H5P__get_class_parent(pclass)))
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, H5I_INVALID_HID, "unable to query class of property list");

    /* Increment the outstanding references to the class object */
    if (H5P__access_class(parent, H5P_MOD_INC_REF) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, H5I_INVALID_HID, "Can't increment class ID ref count");

    /* Get an ID for the class */
    if ((ret_value = H5I_register(H5I_GENPROP_CLS, parent, TRUE)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to register property list class");

done:

    if (H5I_INVALID_HID == ret_value && parent)
        H5P__close_class(parent);

    FUNC_LEAVE_API(ret_value)

} /* H5Pget_class_parent() */

#endif

#ifdef H5_HAVE_MULTITHREAD

/****************************************************************************************
 * Function:    H5Pclose_class
 *
 * Purpose:     Multithread safe version of H5Pclose_class() which is a routine to close
 *              a property list class.
 *
 *              NOTE: In this iteration of H5P.c where the multithread structures and
 *              functions have been implemented, there is not a difference in the
 *              original and multithread version of H5Pclose_class(), however they are
 *              still separated for explaination sake.
 *
 *              NOTE: The original version of H5P would generate new IDs for classes in
 *              some functions (i.e. H5Pget_class_parent() ), without checking if they
 *              already have an existing ID. Thus this class would have two IDs. This
 *              could cause some problems in the future during multithread operations,
 *              so instead those functions were changed to increment the reference count
 *              of those IDs in the index and only generate a new ID if that class did
 *              not have one already. Due to this change, H5Pclose_class is able to
 *              remain identical.
 *
 * Return:      Success: Non-negative
 *
 *              Failure: Negative
 *
 ****************************************************************************************
 */
herr_t
H5Pclose_class(hid_t cls_id)
{
    herr_t ret_value = SUCCEED; /* Return value			*/

    FUNC_ENTER_API(FAIL)
    H5TRACE1("e", "i", cls_id);

    /* Check arguments */
    if (H5I_GENPROP_CLS != H5I_get_type(cls_id)) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list class");
    }

    if (H5I_dec_app_ref(cls_id) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTFREE, FAIL, "can't close");
    }

done:

    FUNC_LEAVE_API(ret_value)

} /* H5Pclose_class() */

#else
/*--------------------------------------------------------------------------
 NAME
    H5Pclose_class
 PURPOSE
    Close a property list class.
 USAGE
    herr_t H5Pclose_class(cls_id)
        hid_t cls_id;       IN: Property list class ID to class

 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
    Releases memory and de-attach a class from the property list class hierarchy.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5Pclose_class(hid_t cls_id)
{
    herr_t ret_value = SUCCEED; /* Return value			*/

    FUNC_ENTER_API(FAIL)
    H5TRACE1("e", "i", cls_id);

    /* Check arguments */
    if (H5I_GENPROP_CLS != H5I_get_type(cls_id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list class");

    /* Close the property list class */
    if (H5I_dec_app_ref(cls_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTFREE, FAIL, "can't close");

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Pclose_class() */

#endif
