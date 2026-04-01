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
#include "H5private.h" /* Generic Functions			*/
#ifdef H5_HAVE_PARALLEL
#include "H5ACprivate.h" /* Metadata cache                       */
#endif                   /* H5_HAVE_PARALLEL */
#include "H5Eprivate.h"  /* Error handling		  	*/
#include "H5Fprivate.h"  /* File access				*/
#include "H5FLprivate.h" /* Free lists                           */
#include "H5Iprivate.h"  /* IDs			  		*/
#include "H5MMprivate.h" /* Memory management			*/
#include "H5Ppkg.h"      /* Property lists		  	*/

#ifdef H5_HAVE_MULTITHREAD
#include "H5CXprivate.h"
#include "H5Ppkg_mt.h"

typedef H5P_mt_class_t H5P_genclass_t;
typedef H5P_mt_list_t  H5P_genplist_t;
#endif /* H5_HAVE_MULTITHREAD */

/****************/
/* Local Macros */
/****************/

#define H5P_ENCODE_VERS 0

#define H5P__MAX_PROP_FL_LEN  256ULL
#define H5P__MAX_CLASS_FL_LEN 16ULL
#define H5P__MAX_LIST_FL_LEN  32ULL

/******************/
/* Local Typedefs */
/******************/

/* Typedef for checking for duplicate class names in parent class */
typedef struct {
    const H5P_genclass_t *parent;    /* Pointer to parent class */
    const char           *name;      /* Pointer to name to check */
    H5P_genclass_t       *new_class; /* Pointer to class during path traversal */
} H5P_check_class_t;

/* Typedef for property list iterator callback */
typedef struct {
    H5P_iterate_int_t     cb_func;      /* Iterator callback */
    void                 *udata;        /* Iterator callback pointer */
    const H5P_genplist_t *plist;        /* Property list pointer */
    H5SL_t               *seen;         /* Skip list to hold names of properties already seen */
    int                  *curr_idx_ptr; /* Pointer to current iteration index */
    int                   prev_idx;     /* Previous iteration index */
} H5P_iter_plist_ud_t;

/* Typedef for property list class iterator callback */
typedef struct {
    H5P_iterate_int_t cb_func;      /* Iterator callback */
    void             *udata;        /* Iterator callback pointer */
    int              *curr_idx_ptr; /* Pointer to current iteration index */
    int               prev_idx;     /* Previous iteration index */
} H5P_iter_pclass_ud_t;

/* Typedef for property list comparison callback */
typedef struct {
    const H5P_genplist_t *plist2;    /* Pointer to second property list */
    int                   cmp_value; /* Value from property comparison */
} H5P_plist_cmp_ud_t;

/* Typedef for property list set/poke callbacks */
typedef struct {
    const void *value; /* Pointer to value to set */
} H5P_prop_set_ud_t;

/* Typedef for property list get/peek callbacks */
typedef struct {
    void *value; /* Pointer for retrieved value */
} H5P_prop_get_ud_t;

/* Typedef for H5P__do_prop() callbacks */
typedef herr_t (*H5P_do_plist_op_t)(H5P_genplist_t *plist, const char *name, H5P_genprop_t *prop,
                                    void *udata);
typedef herr_t (*H5P_do_pclass_op_t)(H5P_genplist_t *plist, const char *name, H5P_genprop_t *prop,
                                     void *udata);

/********************/
/* Local Prototypes */
/********************/

/* Infrastructure routines */
static herr_t H5P__close_class_cb(void *space, void **request);
static herr_t H5P__close_list_cb(void *space, void **request);

/* General helper routines */
#ifndef H5_HAVE_MULTITHREAD
static H5P_genplist_t *H5P__create(H5P_genclass_t *pclass);
static H5P_genprop_t  *H5P__create_prop(const char *name, size_t size, H5P_prop_within_t type,
                                        const void *value, H5P_prp_create_func_t prp_create,
                                        H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get,
                                        H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode,
                                        H5P_prp_delete_func_t prp_delete, H5P_prp_copy_func_t prp_copy,
                                        H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close);
static H5P_genprop_t  *H5P__dup_prop(H5P_genprop_t *oprop, H5P_prop_within_t type);
static herr_t          H5P__free_prop(H5P_genprop_t *prop);
static int             H5P__cmp_prop(const H5P_genprop_t *prop1, const H5P_genprop_t *prop2);
static herr_t          H5P__do_prop(H5P_genplist_t *plist, const char *name, H5P_do_plist_op_t plist_op,
                                    H5P_do_pclass_op_t pclass_op, void *udata);
static int             H5P__open_class_path_cb(void *_obj, hid_t H5_ATTR_UNUSED id, void *_key);
static H5P_genprop_t  *H5P__find_prop_pclass(H5P_genclass_t *pclass, const char *name);
static herr_t          H5P__free_prop_cb(void *item, void H5_ATTR_UNUSED *key, void *op_data);
static herr_t H5P__free_del_name_cb(void *item, void H5_ATTR_UNUSED *key, void H5_ATTR_UNUSED *op_data);
#endif /* ! H5_HAVE_MULTITHREAD */

/*********************/
/* Package Variables */
/*********************/

#ifdef H5_HAVE_MULTITHREAD

/**
 * Flag to know if the context is initialized, so
 * the plist version can be grabbed from there
 */
bool H5P_H5CX_INIT_g = FALSE;

#define H5P_PLIST_CX_LOG 0

#endif /* H5_HAVE_MULTITHREAD */

/*
 * Predefined property list classes. These are initialized at runtime by
 * H5P_init() in this source file.
 */
hid_t H5P_CLS_ROOT_ID_g = H5I_INVALID_HID;

hid_t H5P_CLS_ATTRIBUTE_ACCESS_ID_g = H5I_INVALID_HID;
hid_t H5P_CLS_ATTRIBUTE_CREATE_ID_g = H5I_INVALID_HID;
hid_t H5P_CLS_DATASET_ACCESS_ID_g   = H5I_INVALID_HID;
hid_t H5P_CLS_DATASET_CREATE_ID_g   = H5I_INVALID_HID;
hid_t H5P_CLS_DATASET_XFER_ID_g     = H5I_INVALID_HID;
hid_t H5P_CLS_DATATYPE_ACCESS_ID_g  = H5I_INVALID_HID;
hid_t H5P_CLS_DATATYPE_CREATE_ID_g  = H5I_INVALID_HID;
hid_t H5P_CLS_FILE_ACCESS_ID_g      = H5I_INVALID_HID;
hid_t H5P_CLS_FILE_CREATE_ID_g      = H5I_INVALID_HID;
hid_t H5P_CLS_FILE_MOUNT_ID_g       = H5I_INVALID_HID;
hid_t H5P_CLS_GROUP_ACCESS_ID_g     = H5I_INVALID_HID;
hid_t H5P_CLS_GROUP_CREATE_ID_g     = H5I_INVALID_HID;
hid_t H5P_CLS_LINK_ACCESS_ID_g      = H5I_INVALID_HID;
hid_t H5P_CLS_LINK_CREATE_ID_g      = H5I_INVALID_HID;
hid_t H5P_CLS_MAP_ACCESS_ID_g       = H5I_INVALID_HID;
hid_t H5P_CLS_MAP_CREATE_ID_g       = H5I_INVALID_HID;
hid_t H5P_CLS_OBJECT_COPY_ID_g      = H5I_INVALID_HID;
hid_t H5P_CLS_OBJECT_CREATE_ID_g    = H5I_INVALID_HID;
hid_t H5P_CLS_REFERENCE_ACCESS_ID_g = H5I_INVALID_HID;
hid_t H5P_CLS_STRING_CREATE_ID_g    = H5I_INVALID_HID;
hid_t H5P_CLS_VOL_INITIALIZE_ID_g   = H5I_INVALID_HID;

H5P_genclass_t *H5P_CLS_ROOT_g = NULL;

H5P_genclass_t *H5P_CLS_ATTRIBUTE_ACCESS_g = NULL;
H5P_genclass_t *H5P_CLS_ATTRIBUTE_CREATE_g = NULL;
H5P_genclass_t *H5P_CLS_DATASET_ACCESS_g   = NULL;
H5P_genclass_t *H5P_CLS_DATASET_CREATE_g   = NULL;
H5P_genclass_t *H5P_CLS_DATASET_XFER_g     = NULL;
H5P_genclass_t *H5P_CLS_DATATYPE_ACCESS_g  = NULL;
H5P_genclass_t *H5P_CLS_DATATYPE_CREATE_g  = NULL;
H5P_genclass_t *H5P_CLS_FILE_ACCESS_g      = NULL;
H5P_genclass_t *H5P_CLS_FILE_CREATE_g      = NULL;
H5P_genclass_t *H5P_CLS_FILE_MOUNT_g       = NULL;
H5P_genclass_t *H5P_CLS_GROUP_ACCESS_g     = NULL;
H5P_genclass_t *H5P_CLS_GROUP_CREATE_g     = NULL;
H5P_genclass_t *H5P_CLS_LINK_ACCESS_g      = NULL;
H5P_genclass_t *H5P_CLS_LINK_CREATE_g      = NULL;
H5P_genclass_t *H5P_CLS_MAP_ACCESS_g       = NULL;
H5P_genclass_t *H5P_CLS_MAP_CREATE_g       = NULL;
H5P_genclass_t *H5P_CLS_OBJECT_COPY_g      = NULL;
H5P_genclass_t *H5P_CLS_OBJECT_CREATE_g    = NULL;
H5P_genclass_t *H5P_CLS_REFERENCE_ACCESS_g = NULL;
H5P_genclass_t *H5P_CLS_STRING_CREATE_g    = NULL;
H5P_genclass_t *H5P_CLS_VOL_INITIALIZE_g   = NULL;

/*
 * Predefined property lists for each predefined class. These are initialized
 * at runtime by H5P_init() in this source file.
 */
hid_t H5P_LST_ATTRIBUTE_ACCESS_ID_g = H5I_INVALID_HID;
hid_t H5P_LST_ATTRIBUTE_CREATE_ID_g = H5I_INVALID_HID;
hid_t H5P_LST_DATASET_ACCESS_ID_g   = H5I_INVALID_HID;
hid_t H5P_LST_DATASET_CREATE_ID_g   = H5I_INVALID_HID;
hid_t H5P_LST_DATASET_XFER_ID_g     = H5I_INVALID_HID;
hid_t H5P_LST_DATATYPE_ACCESS_ID_g  = H5I_INVALID_HID;
hid_t H5P_LST_DATATYPE_CREATE_ID_g  = H5I_INVALID_HID;
hid_t H5P_LST_FILE_ACCESS_ID_g      = H5I_INVALID_HID;
hid_t H5P_LST_FILE_CREATE_ID_g      = H5I_INVALID_HID;
hid_t H5P_LST_FILE_MOUNT_ID_g       = H5I_INVALID_HID;
hid_t H5P_LST_GROUP_ACCESS_ID_g     = H5I_INVALID_HID;
hid_t H5P_LST_GROUP_CREATE_ID_g     = H5I_INVALID_HID;
hid_t H5P_LST_LINK_ACCESS_ID_g      = H5I_INVALID_HID;
hid_t H5P_LST_LINK_CREATE_ID_g      = H5I_INVALID_HID;
hid_t H5P_LST_MAP_ACCESS_ID_g       = H5I_INVALID_HID;
hid_t H5P_LST_MAP_CREATE_ID_g       = H5I_INVALID_HID;
hid_t H5P_LST_OBJECT_COPY_ID_g      = H5I_INVALID_HID;
hid_t H5P_LST_REFERENCE_ACCESS_ID_g = H5I_INVALID_HID;
hid_t H5P_LST_VOL_INITIALIZE_ID_g   = H5I_INVALID_HID;

#ifdef H5_HAVE_MULTITHREAD
/**
 * Atomic global versions for each of the predefined property lists. These are
 * pointed to by the default property list structures to be used by the context
 * as a way to ensure that a version of list isn't changed out from under a
 * thread during API calls.
 */
_Atomic uint64_t H5P_AAPL_VER_g   = 0;
_Atomic uint64_t H5P_ACPL_VER_g   = 0;
_Atomic uint64_t H5P_DAPL_VER_g   = 0;
_Atomic uint64_t H5P_DCPL_VER_g   = 0;
_Atomic uint64_t H5P_DXPL_VER_g   = 0;
_Atomic uint64_t H5P_TAPL_VER_g   = 0;
_Atomic uint64_t H5P_TCPL_VER_g   = 0;
_Atomic uint64_t H5P_FAPL_VER_g   = 0;
_Atomic uint64_t H5P_FCPL_VER_g   = 0;
_Atomic uint64_t H5P_FMPL_VER_g   = 0;
_Atomic uint64_t H5P_GAPL_VER_g   = 0;
_Atomic uint64_t H5P_GCPL_VER_g   = 0;
_Atomic uint64_t H5P_LAPL_VER_g   = 0;
_Atomic uint64_t H5P_LCPL_VER_g   = 0;
_Atomic uint64_t H5P_MAPL_VER_g   = 0;
_Atomic uint64_t H5P_MCPL_VER_g   = 0;
_Atomic uint64_t H5P_OCPYPL_VER_g = 0;
_Atomic uint64_t H5P_RAPL_VER_g   = 0;
_Atomic uint64_t H5P_VIPL_VER_g   = 0;

#endif

/* Root property list class library initialization object */
const H5P_libclass_t H5P_CLS_ROOT[1] = {{
    "root",        /* Class name for debugging     */
    H5P_TYPE_ROOT, /* Class type                   */

    NULL, /* Parent class                 */

    &H5P_CLS_ROOT_g, /* Pointer to class             */

    &H5P_CLS_ROOT_ID_g, /* Pointer to class ID          */
    NULL,               /* Pointer to default property list ID */
    NULL,               /* Default property registration routine */

    NULL, /* Class creation callback      */
    NULL, /* Class creation callback info */
    NULL, /* Class copy callback          */
    NULL, /* Class copy callback info     */
    NULL, /* Class close callback         */
    NULL  /* Class close callback info    */
}};

/* Attribute access property list class library initialization object */
/* (move to proper source code file when used for real) */
const H5P_libclass_t H5P_CLS_AACC[1] = {{
    "attribute access",        /* Class name for debugging     */
    H5P_TYPE_ATTRIBUTE_ACCESS, /* Class type                   */

    &H5P_CLS_LINK_ACCESS_g,      /* Parent class                 */
    &H5P_CLS_ATTRIBUTE_ACCESS_g, /* Pointer to class             */

    &H5P_CLS_ATTRIBUTE_ACCESS_ID_g, /* Pointer to class ID          */
    &H5P_LST_ATTRIBUTE_ACCESS_ID_g, /* Pointer to default property list ID */
    NULL,                           /* Default property registration routine */

    NULL, /* Class creation callback      */
    NULL, /* Class creation callback info */
    NULL, /* Class copy callback          */
    NULL, /* Class copy callback info     */
    NULL, /* Class close callback         */
    NULL  /* Class close callback info    */
}};

/* Group access property list class library initialization object */
/* (move to proper source code file when used for real) */
const H5P_libclass_t H5P_CLS_GACC[1] = {{
    "group access",        /* Class name for debugging     */
    H5P_TYPE_GROUP_ACCESS, /* Class type                   */

    &H5P_CLS_LINK_ACCESS_g,  /* Parent class                 */
    &H5P_CLS_GROUP_ACCESS_g, /* Pointer to class             */

    &H5P_CLS_GROUP_ACCESS_ID_g, /* Pointer to class ID          */
    &H5P_LST_GROUP_ACCESS_ID_g, /* Pointer to default property list ID */
    NULL,                       /* Default property registration routine */

    NULL, /* Class creation callback      */
    NULL, /* Class creation callback info */
    NULL, /* Class copy callback          */
    NULL, /* Class copy callback info     */
    NULL, /* Class close callback         */
    NULL  /* Class close callback info    */
}};

/* Datatype creation property list class library initialization object */
/* (move to proper source code file when used for real) */
const H5P_libclass_t H5P_CLS_TCRT[1] = {{
    "datatype create",        /* Class name for debugging     */
    H5P_TYPE_DATATYPE_CREATE, /* Class type                   */

    &H5P_CLS_OBJECT_CREATE_g,   /* Parent class                 */
    &H5P_CLS_DATATYPE_CREATE_g, /* Pointer to class             */

    &H5P_CLS_DATATYPE_CREATE_ID_g, /* Pointer to class ID          */
    &H5P_LST_DATATYPE_CREATE_ID_g, /* Pointer to default property list ID */
    NULL,                          /* Default property registration routine */

    NULL, /* Class creation callback      */
    NULL, /* Class creation callback info */
    NULL, /* Class copy callback          */
    NULL, /* Class copy callback info     */
    NULL, /* Class close callback         */
    NULL  /* Class close callback info    */
}};

/* Datatype access property list class library initialization object */
/* (move to proper source code file when used for real) */
const H5P_libclass_t H5P_CLS_TACC[1] = {{
    "datatype access",        /* Class name for debugging     */
    H5P_TYPE_DATATYPE_ACCESS, /* Class type                   */

    &H5P_CLS_LINK_ACCESS_g,     /* Parent class                 */
    &H5P_CLS_DATATYPE_ACCESS_g, /* Pointer to class             */

    &H5P_CLS_DATATYPE_ACCESS_ID_g, /* Pointer to class ID          */
    &H5P_LST_DATATYPE_ACCESS_ID_g, /* Pointer to default property list ID */
    NULL,                          /* Default property registration routine */

    NULL, /* Class creation callback      */
    NULL, /* Class creation callback info */
    NULL, /* Class copy callback          */
    NULL, /* Class copy callback info     */
    NULL, /* Class close callback         */
    NULL  /* Class close callback info    */
}};

/* VOL initialization property list class library initialization object */
/* (move to proper source code file when used for real) */
const H5P_libclass_t H5P_CLS_VINI[1] = {{
    "VOL initialization",    /* Class name for debugging     */
    H5P_TYPE_VOL_INITIALIZE, /* Class type                   */

    &H5P_CLS_ROOT_g,           /* Parent class                 */
    &H5P_CLS_VOL_INITIALIZE_g, /* Pointer to class             */

    &H5P_CLS_VOL_INITIALIZE_ID_g, /* Pointer to class ID          */
    &H5P_LST_VOL_INITIALIZE_ID_g, /* Pointer to default property list ID */
    NULL,                         /* Default property registration routine */

    NULL, /* Class creation callback      */
    NULL, /* Class creation callback info */
    NULL, /* Class copy callback          */
    NULL, /* Class copy callback info     */
    NULL, /* Class close callback         */
    NULL  /* Class close callback info    */
}};

/* Reference access property list class library initialization object */
/* (move to proper source code file when used for real) */
const H5P_libclass_t H5P_CLS_RACC[1] = {{
    "reference access",        /* Class name for debugging     */
    H5P_TYPE_REFERENCE_ACCESS, /* Class type                   */

    &H5P_CLS_FILE_ACCESS_g,      /* Parent class                         */
    &H5P_CLS_REFERENCE_ACCESS_g, /* Pointer to class                     */

    &H5P_CLS_REFERENCE_ACCESS_ID_g, /* Pointer to class ID                  */
    &H5P_LST_REFERENCE_ACCESS_ID_g, /* Pointer to default property list ID  */
    NULL,                           /* Default property registration routine*/

    NULL, /* Class creation callback              */
    NULL, /* Class creation callback info         */
    NULL, /* Class copy callback                  */
    NULL, /* Class copy callback info             */
    NULL, /* Class close callback                 */
    NULL  /* Class close callback info            */
}};

/* Library property list classes defined in other code modules */
/* (And not present in src/H5Pprivate.h) */
H5_DLLVAR const H5P_libclass_t H5P_CLS_OCRT[1];   /* Object creation */
H5_DLLVAR const H5P_libclass_t H5P_CLS_STRCRT[1]; /* String create */
H5_DLLVAR const H5P_libclass_t H5P_CLS_GCRT[1];   /* Group create */
H5_DLLVAR const H5P_libclass_t H5P_CLS_FCRT[1];   /* File creation */
H5_DLLVAR const H5P_libclass_t H5P_CLS_DCRT[1];   /* Dataset creation */
H5_DLLVAR const H5P_libclass_t H5P_CLS_MCRT[1];   /* Map creation */
H5_DLLVAR const H5P_libclass_t H5P_CLS_DXFR[1];   /* Data transfer */
H5_DLLVAR const H5P_libclass_t H5P_CLS_FMNT[1];   /* File mount */
H5_DLLVAR const H5P_libclass_t H5P_CLS_ACRT[1];   /* Attribute creation */

/*****************************/
/* Library Private Variables */
/*****************************/

/*******************/
/* Local Variables */
/*******************/

#ifdef H5_HAVE_MULTITHREAD

/**
 * The multithread version of H5P no longer uses revision count anymore,
 * as each of the multithread structures have versions and instead track
 * the version they are currently on.
 */

#else
/* Track the revision count of a class, to make comparisons faster */
static unsigned H5P_next_rev = 0;
#define H5P_GET_NEXT_REV (H5P_next_rev++)

#endif

/* List of all property list classes in the library */
/* (order here is not important, they will be initialized in the proper
 *      order according to their parent class dependencies)
 */
static H5P_libclass_t const *const init_class[] = {
    H5P_CLS_ROOT,   /* Root */
    H5P_CLS_OCRT,   /* Object create */
    H5P_CLS_STRCRT, /* String create */
    H5P_CLS_LACC,   /* Link access */
    H5P_CLS_GCRT,   /* Group create */
    H5P_CLS_OCPY,   /* Object copy */
    H5P_CLS_GACC,   /* Group access */
    H5P_CLS_FCRT,   /* File creation */
    H5P_CLS_FACC,   /* File access */
    H5P_CLS_DCRT,   /* Dataset creation */
    H5P_CLS_DACC,   /* Dataset access */
    H5P_CLS_DXFR,   /* Data transfer */
    H5P_CLS_FMNT,   /* File mount */
    H5P_CLS_TCRT,   /* Datatype creation */
    H5P_CLS_TACC,   /* Datatype access */
    H5P_CLS_MCRT,   /* Map creation */
    H5P_CLS_MACC,   /* Map access */
    H5P_CLS_ACRT,   /* Attribute creation */
    H5P_CLS_AACC,   /* Attribute access */
    H5P_CLS_LCRT,   /* Link creation */
    H5P_CLS_VINI,   /* VOL initialization */
    H5P_CLS_RACC    /* Reference access */
};

#ifdef H5_HAVE_MULTITHREAD

/**
 * Global structure that contains the property free list,
 * plist free list, class free list, and global H5P stats.
 */
H5P_mt_t H5P_mt_g;

/**
 * Thread specific multithread callback used in testing to pass back the
 * exact version of a class or plist a function is being performed on.
 */
_Thread_local H5P_mt_cb_t H5P_mt_cb = {0};

#else
/* Declare a free list to manage the H5P_genclass_t struct */
H5FL_DEFINE_STATIC(H5P_genclass_t);

/* Declare a free list to manage the H5P_genprop_t struct */
H5FL_DEFINE_STATIC(H5P_genprop_t);

/* Declare a free list to manage the H5P_genplist_t struct */
H5FL_DEFINE_STATIC(H5P_genplist_t);
#endif

/* Generic Property Class ID class */
static const H5I_class_t H5I_GENPROPCLS_CLS[1] = {{
    H5I_GENPROP_CLS,                /* ID class value */
    0,                              /* Class flags */
    0,                              /* # of reserved IDs for class */
    (H5I_free_t)H5P__close_class_cb /* Callback routine for closing objects of this class */
}};

/* Generic Property List ID class */
static const H5I_class_t H5I_GENPROPLST_CLS[1] = {{
    H5I_GENPROP_LST,               /* ID class value */
    0,                             /* Class flags */
    0,                             /* # of reserved IDs for class */
    (H5I_free_t)H5P__close_list_cb /* Callback routine for closing objects of this class */
}};

#ifdef H5_HAVE_MULTITHREAD

/****************************************************************************************
 * Function:    H5P_init_phase1
 *
 *              Multithread safe version of H5P_init_phase1()
 *
 * Purpose:     Initialize the interface from some other layer. This should
 *              be followed with a call to H5P_init_phase2 after the H5P
 *              interface is completely setup.
 *
 *              NOTE: This multithread version sets up the default library property
 *              classes and property lists, and stores pointers to the class, parent
 *              class, and pointers to the IDs of the class and its derived default list
 *              in a H5P_libclass_t struct same as the non-multithread safe version.
 *              In addition to that though a global variable to store current version of
 *              default property lists is also initialized. These global variables are
 *              used by H5CX to store the version of default lists on public API entry to
 *              ensure that the correct version of a list is used throughout the entire
 *              public API call.
 *
 *
 * Return:      Success:    non-negative
 *              Failure:    negative
 *
 ****************************************************************************************
 */
herr_t
H5P_init_phase1(void)
{
    size_t          tot_init = 0; /* Total # of classes initialized */
    size_t          pass_init;    /* # of classes initialized in each pass */
    size_t          u;
    H5P_mt_class_t *new_class;           /* Class created this iteration */
    H5P_mt_list_t  *def_plist;           /* Default plist of new_class */
    herr_t          ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDcompile_assert(H5P_TYPE_REFERENCE_ACCESS == (H5P_TYPE_MAX_TYPE - 1));

    /*
     * Initialize the Generic Property class & object groups.
     */
    if (H5I_register_type(H5I_GENPROPCLS_CLS) < 0) {
        HGOTO_ERROR(H5E_ID, H5E_CANTINIT, FAIL, "unable to initialize ID group");
    }
    if (H5I_register_type(H5I_GENPROPLST_CLS) < 0) {
        HGOTO_ERROR(H5E_ID, H5E_CANTINIT, FAIL, "unable to initialize ID group");
    }

    /**
     * Initializes the H5P_mt_g global struct which contains the free lists for
     * classes, lists, and properties, and global stats for H5P.
     */
    if (0 > H5P_mt_init_free_lists()) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "unable to initialize H5P MT safe free lists");
    }

    /* Repeatedly pass over the list of property list classes for the library,
     * initializing each class if its parent class is initialized, until no
     * more progress is made.
     */
    tot_init = 0;
    do {
        /* Reset pass initialization counter */
        pass_init = 0;

        /* Make a pass over all the library's property list classes */
        for (u = 0; u < NELMTS(init_class); u++) {
            H5P_libclass_t const *lib_class = init_class[u]; /* Current class to operate on */

            /* Check if the current class hasn't been initialized and can be now */
            assert(lib_class->class_id);
            if (*lib_class->class_id == (-1) &&
                (lib_class->par_pclass == NULL || *lib_class->par_pclass != NULL)) {
                /* Sanity check - only the root class is not allowed to have a parent class */
                assert(lib_class->par_pclass || lib_class == H5P_CLS_ROOT);

                /* Allocate the MT safe new class */
                if (NULL == (*lib_class->pclass = H5P__create_class(
                                 lib_class->par_pclass ? *lib_class->par_pclass : NULL, lib_class->name,
                                 lib_class->type, 0, lib_class->create_func, lib_class->create_data,
                                 lib_class->copy_func, lib_class->copy_data, lib_class->close_func,
                                 lib_class->close_data))) {
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "class initialization failed");
                }

                /* Call routine to register properties for class */
                if (lib_class->reg_prop_func && (*lib_class->reg_prop_func)(*lib_class->pclass) < 0) {
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "can't register properties");
                }

                /* Register the new class */
                if ((*lib_class->class_id = H5I_register(H5I_GENPROP_CLS, *lib_class->pclass, FALSE)) < 0) {
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "can't register property list class");
                }

                /* Atomically update the index ID in the class and set opening flag to FALSE */
                new_class = *lib_class->pclass;

                atomic_store(&(new_class->id), *(lib_class->class_id));

                /* Only register the default property list if it hasn't been created yet */
                if (lib_class->def_plist_id && *lib_class->def_plist_id == (-1)) {

                    /* Register the default MT property list for the new MT class */
                    def_plist = H5P__create_list(*lib_class->pclass, FALSE);

                    if (0 > (*lib_class->def_plist_id = atomic_load(&(def_plist->plist_id)))) {
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL,
                                    "can't register default MT property list for MT class");
                    }

                    /**
                     * Set the atomic global plist version with the current version and set
                     * the default plist's def_ver_ptr to point to its associated global.
                     */
                    if (*lib_class->def_plist_id == H5P_LST_ATTRIBUTE_ACCESS_ID_g) {
                        atomic_store(&(H5P_AAPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_AAPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_ATTRIBUTE_CREATE_ID_g) {
                        atomic_store(&(H5P_ACPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_ACPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_DATASET_ACCESS_ID_g) {
                        atomic_store(&(H5P_DAPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_DAPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_DATASET_CREATE_ID_g) {
                        atomic_store(&(H5P_DCPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_DCPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_DATASET_XFER_ID_g) {
                        atomic_store(&(H5P_DXPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_DXPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_DATATYPE_ACCESS_ID_g) {
                        atomic_store(&(H5P_TAPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_TAPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_DATATYPE_CREATE_ID_g) {
                        atomic_store(&(H5P_TCPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_TCPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_FILE_ACCESS_ID_g) {
                        atomic_store(&(H5P_FAPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_FAPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_FILE_CREATE_ID_g) {
                        atomic_store(&(H5P_FCPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_FCPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_FILE_MOUNT_ID_g) {
                        atomic_store(&(H5P_FMPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_FMPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_GROUP_ACCESS_ID_g) {
                        atomic_store(&(H5P_GAPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_GAPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_GROUP_CREATE_ID_g) {
                        atomic_store(&(H5P_GCPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_GCPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_LINK_ACCESS_ID_g) {
                        atomic_store(&(H5P_LAPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_LAPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_LINK_CREATE_ID_g) {
                        atomic_store(&(H5P_LCPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_LCPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_MAP_ACCESS_ID_g) {
                        atomic_store(&(H5P_MAPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_MAPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_MAP_CREATE_ID_g) {
                        atomic_store(&(H5P_MCPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_MCPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_OBJECT_COPY_ID_g) {
                        atomic_store(&(H5P_OCPYPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_OCPYPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_REFERENCE_ACCESS_ID_g) {
                        atomic_store(&(H5P_RAPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_RAPL_VER_g;
                    }
                    else if (*lib_class->def_plist_id == H5P_LST_VOL_INITIALIZE_ID_g) {
                        atomic_store(&(H5P_VIPL_VER_g), atomic_load(&(def_plist->curr_version)));
                        def_plist->def_ver_ptr = &H5P_VIPL_VER_g;
                    }

                } /* end if */

                /* Increment class initialization counters */
                pass_init++;
                tot_init++;

            } /* end if */

        } /* end for */

    } while (pass_init > 0);

    /* Verify that all classes were initialized */
    assert(tot_init == NELMTS(init_class));

done:

    if (ret_value < 0 && tot_init > 0) {
        /* First uninitialize all default property lists */
        H5I_clear_type(H5I_GENPROP_LST, FALSE, FALSE);

        /* Then uninitialize any initialized libclass */
        for (u = 0; u < NELMTS(init_class); u++) {
            H5P_libclass_t const *lib_class = init_class[u]; /* Current class to operate on */

            assert(lib_class->class_id);
            if (*lib_class->class_id >= 0) {
                /* Close the class ID */
                if (H5I_dec_ref(*lib_class->class_id) < 0) {
                    HDONE_ERROR(H5E_PLIST, H5E_CLOSEERROR, FAIL, "unable to close property list class ID");
                }
            }
            else if (lib_class->pclass && *lib_class->pclass) {
                /* Close a half-initialized pclass */
                if (H5P__close_class(*lib_class->pclass) < 0) {
                    HDONE_ERROR(H5E_PLIST, H5E_CLOSEERROR, FAIL, "unable to close MT property list class");
                }
            }

        } /* end for () */

    } /* end if (ret_value < 0 && tot_init > 0) */

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_init_phase1() MT safe version */

/****************************************************************************************
 * Function:    H5P_init_phase2
 *
 *              No changes were made for multithread H5P.
 *
 * Purpose:     Finish initializing the interface from some other package.
 *
 * NOTE:        This is broken out as a separate routine so that the
 *              library's default VFL driver can be chosen and initialized
 *              after the entire H5P interface has been initialized.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 ****************************************************************************************
 */
herr_t
H5P_init_phase2(void)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    /* Set up the default VFL driver */
    if (H5P__facc_set_def_driver() < 0) {
        HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, "unable to set default VFL driver");
    }

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P_init_phase2() */

/****************************************************************************************
 * Function:    H5P_mt_init_free_lists
 *
 *              Multithread version only function
 *
 * Purpose:     Initializes the H5P_mt_g global struct which contains the free lists for
 *              classes, lists, and properties, and global stats for H5P.
 *
 *              NOTE: Once initialized, the free lists will always contain at least one
 *              entry of H5P_mt_prop_t for the prop free list, H5P_mt_class_t for the
 *              class free list, and one H5P_mt_list_t for the list free list. The free
 *              lists are considered empty if both the head and tail of the free list
 *              point to the same object.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P_mt_init_free_lists(void)
{
    H5P_mt_prop_aptr_t           fl_aptr = {NULL, FALSE, FALSE, FALSE, FALSE};
    H5P_mt_prop_t               *fl_prop;
    H5P_mt_prop_value_t          value    = {NULL, 0};
    H5P_mt_list_sptr_t           fl_lsptr = {NULL, 0};
    H5P_mt_list_t               *fl_list;
    H5P_mt_class_sptr_t          fl_csptr = {NULL, 0};
    H5P_mt_class_t              *fl_class;
    H5P_mt_class_ref_counts_t    ref_counts = {0, 0, TRUE, FALSE, FALSE, FALSE};
    H5P_mt_active_thread_count_t thrd       = {0, FALSE, TRUE};

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    /* Allocate and initialize a property free list node */
    if ((fl_prop = (H5P_mt_prop_t *)malloc(sizeof(H5P_mt_prop_t))) == NULL) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "property allocation failed");
    }

    atomic_init(&(fl_prop->tag), H5P_MT_PROP_VALID_ONFL_TAG);
    atomic_init(&(fl_prop->next), fl_aptr);
    fl_prop->sentinel      = FALSE;
    fl_prop->in_prop_class = FALSE;
    atomic_init(&(fl_prop->ref_count), 0);
    fl_prop->in_lkup_tbl = FALSE;
    fl_prop->chksum      = 0;
    fl_prop->name        = NULL;
    atomic_init(&(fl_prop->value), value);
    atomic_init(&(fl_prop->create_version), 0);
    atomic_init(&(fl_prop->delete_version), 0);
    fl_prop->callbacks_mt_safe = FALSE;
    fl_prop->create            = NULL;
    fl_prop->set               = NULL;
    fl_prop->get               = NULL;
    fl_prop->encode            = NULL;
    fl_prop->decode            = NULL;
    fl_prop->del               = NULL;
    fl_prop->copy              = NULL;
    fl_prop->cmp               = NULL;
    fl_prop->close             = NULL;

    fl_aptr.ptr = fl_prop;

    atomic_init(&(H5P_mt_g.prop_fl_head), fl_aptr);
    atomic_init(&(H5P_mt_g.prop_fl_tail), fl_aptr);
    atomic_init(&(H5P_mt_g.prop_fl_len), 0ULL);
    atomic_init(&(H5P_mt_g.prop_max_desired_fl_len), H5P__MAX_PROP_FL_LEN);

    /* Allocate and initialize a class free list node */
    if ((fl_class = (H5P_mt_class_t *)malloc(sizeof(H5P_mt_class_t))) == NULL) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "property list class allocation failed");
    }

    atomic_init(&(fl_class->tag), H5P_MT_CLASS_INVALID_TAG);
    fl_class->parent_id      = H5I_INVALID_HID;
    fl_class->parent_ptr     = NULL;
    fl_class->parent_version = 0;
    fl_class->name           = NULL;
    fl_class->id             = H5I_INVALID_HID;
    fl_class->type           = 0;
    atomic_init(&(fl_class->curr_version), 0);
    atomic_init(&(fl_class->next_version), 0);
    fl_class->pl_head = NULL;
    atomic_init(&(fl_class->nprops_added), 0);
    atomic_init(&(fl_class->log_pl_len), 0);
    atomic_init(&(fl_class->phys_pl_len), 0);
    atomic_init(&(fl_class->ref_count), ref_counts);
    fl_class->create_func = NULL;
    fl_class->create_data = NULL;
    fl_class->copy_func   = NULL;
    fl_class->copy_data   = NULL;
    fl_class->close_func  = NULL;
    fl_class->close_data  = NULL;
    atomic_init(&(fl_class->thrd), thrd);
    atomic_init(&(fl_class->fl_next), fl_csptr);

    H5P__init_stats_class(fl_class);

    fl_csptr.ptr = fl_class;

    atomic_init(&(H5P_mt_g.class_fl_head), fl_csptr);
    atomic_init(&(H5P_mt_g.class_fl_tail), fl_csptr);
    atomic_init(&(H5P_mt_g.class_fl_len), 0ULL);
    atomic_init(&(H5P_mt_g.class_max_desired_fl_len), H5P__MAX_CLASS_FL_LEN);

    /* Allocate and initialize a list free list node */
    if ((fl_list = (H5P_mt_list_t *)malloc(sizeof(H5P_mt_list_t))) == NULL) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "property list class allocation failed");
    }

    atomic_init(&(fl_list->tag), H5P_MT_LIST_INVALID_TAG);
    fl_list->pclass_id      = H5I_INVALID_HID;
    fl_list->pclass_ptr     = NULL;
    fl_list->pclass_version = 0;
    atomic_init(&(fl_list->plist_id), H5I_INVALID_HID);
    atomic_init(&(fl_list->curr_version), 0);
    atomic_init(&(fl_list->next_version), 0);
    fl_list->def_ver_ptr      = NULL;
    fl_list->lkup_tbl         = NULL;
    fl_list->nprops_inherited = 0;
    atomic_init(&(fl_list->nprops_added), 0);
    atomic_init(&(fl_list->nprops), 0);
    fl_list->pl_head = NULL;
    atomic_init(&(fl_list->log_pl_len), 0);
    atomic_init(&(fl_list->phys_pl_len), 0);
    atomic_init(&(fl_list->class_init), TRUE);
    atomic_init(&(fl_list->thrd), thrd);
    atomic_init(&(fl_list->fl_next), fl_lsptr);

    H5P__init_stats_list(fl_list);

    fl_lsptr.ptr = fl_list;

    atomic_init(&(H5P_mt_g.list_fl_head), fl_lsptr);
    atomic_init(&(H5P_mt_g.list_fl_tail), fl_lsptr);
    atomic_init(&(H5P_mt_g.list_fl_len), 0ULL);
    atomic_init(&(H5P_mt_g.list_max_desired_fl_len), H5P__MAX_LIST_FL_LEN);

    H5P__init_stats_global();

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_mt_init_free_lists() */

/****************************************************************************************
 * Function:    H5P_set_cx_init
 *
 *              Multithread only function
 *
 * Purpose:     Function called by H5CX to set global H5P_H5CX_INIT_g to
 *              TRUE.
 *
 * NOTE:        If H5P_H5CX_INIT_g is TRUE functions in H5P will grab the
 *              version of the plist parameter from the context, else they
 *              will use the current version of the plist.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 ****************************************************************************************
 */
herr_t
H5P_set_cx_init(void)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT_NOERR

    H5P_H5CX_INIT_g = TRUE;

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P_set_cx_init() */

/****************************************************************************************
 * Function:    H5P_unset_cx_init
 *
 *              Multithread only function
 *
 * Purpose:     Function called by H5CX to set global H5P_H5CX_INIT_g to
 *              FALSE.
 *
 * NOTE:        If H5P_H5CX_INIT_g is TRUE functions in H5P will grab the
 *              version of the plist parameter from the context, else they
 *              will use the current version of the plist.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 ****************************************************************************************
 */
herr_t
H5P_unset_cx_init(void)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT_NOERR

    H5P_H5CX_INIT_g = FALSE;

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P_unset_cx_init() */

/****************************************************************************************
 * Function:    H5P_term_package
 *
 *              Multithread safe version
 *
 * Purpose:     Terminate various H5P objects
 *
 * Return:      Success:    Positive if any action was taken that might
 *                          affect some other interface; zero otherwise.
 *
 *              Failure:    Negative
 *
 ****************************************************************************************
 */
int
H5P_term_package(void)
{
    int    n = 0;
    herr_t result;

    FUNC_ENTER_NOAPI_NOINIT_NOERR

    int64_t nlist, nclass;

    /* Destroy HDF5 library property classes & lists */

    /* Check if there are any open property list classes or lists */
    nclass = H5I_nmembers(H5I_GENPROP_CLS);
    nlist  = H5I_nmembers(H5I_GENPROP_LST);

    /* If there are any open classes or groups, attempt to get rid of them. */
    if ((nclass + nlist) > 0) {
        /* Clear the lists */
        if (nlist > 0) {
            (void)H5I_clear_type(H5I_GENPROP_LST, FALSE, FALSE);

            /* Reset the default property lists, if they've been closed */
            if (H5I_nmembers(H5I_GENPROP_LST) == 0) {
                H5P_LST_ATTRIBUTE_ACCESS_ID_g = H5I_INVALID_HID;
                H5P_LST_ATTRIBUTE_CREATE_ID_g = H5I_INVALID_HID;
                H5P_LST_DATASET_ACCESS_ID_g   = H5I_INVALID_HID;
                H5P_LST_DATASET_CREATE_ID_g   = H5I_INVALID_HID;
                H5P_LST_DATASET_XFER_ID_g     = H5I_INVALID_HID;
                H5P_LST_DATATYPE_ACCESS_ID_g  = H5I_INVALID_HID;
                H5P_LST_DATATYPE_CREATE_ID_g  = H5I_INVALID_HID;
                H5P_LST_FILE_ACCESS_ID_g      = H5I_INVALID_HID;
                H5P_LST_FILE_CREATE_ID_g      = H5I_INVALID_HID;
                H5P_LST_FILE_MOUNT_ID_g       = H5I_INVALID_HID;
                H5P_LST_GROUP_ACCESS_ID_g     = H5I_INVALID_HID;
                H5P_LST_GROUP_CREATE_ID_g     = H5I_INVALID_HID;
                H5P_LST_LINK_ACCESS_ID_g      = H5I_INVALID_HID;
                H5P_LST_LINK_CREATE_ID_g      = H5I_INVALID_HID;
                H5P_LST_MAP_ACCESS_ID_g       = H5I_INVALID_HID;
                H5P_LST_MAP_CREATE_ID_g       = H5I_INVALID_HID;
                H5P_LST_OBJECT_COPY_ID_g      = H5I_INVALID_HID;
                H5P_LST_REFERENCE_ACCESS_ID_g = H5I_INVALID_HID;
                H5P_LST_VOL_INITIALIZE_ID_g   = H5I_INVALID_HID;
            }
        }

        /* Only attempt to close the classes after all the lists are closed */
        if (nlist == 0 && nclass > 0) {
            (void)H5I_clear_type(H5I_GENPROP_CLS, FALSE, FALSE);

            /* Reset the default property classes and IDs if they've been closed */
            if (H5I_nmembers(H5I_GENPROP_CLS) == 0) {
                H5P_CLS_ROOT_g = NULL;

                H5P_CLS_ATTRIBUTE_ACCESS_g = NULL;
                H5P_CLS_ATTRIBUTE_CREATE_g = NULL;
                H5P_CLS_DATASET_ACCESS_g   = NULL;
                H5P_CLS_DATASET_CREATE_g   = NULL;
                H5P_CLS_DATASET_XFER_g     = NULL;
                H5P_CLS_DATATYPE_ACCESS_g  = NULL;
                H5P_CLS_DATATYPE_CREATE_g  = NULL;
                H5P_CLS_FILE_ACCESS_g      = NULL;
                H5P_CLS_FILE_CREATE_g      = NULL;
                H5P_CLS_FILE_MOUNT_g       = NULL;
                H5P_CLS_GROUP_ACCESS_g     = NULL;
                H5P_CLS_GROUP_CREATE_g     = NULL;
                H5P_CLS_LINK_ACCESS_g      = NULL;
                H5P_CLS_LINK_CREATE_g      = NULL;
                H5P_CLS_MAP_ACCESS_g       = NULL;
                H5P_CLS_MAP_CREATE_g       = NULL;
                H5P_CLS_OBJECT_COPY_g      = NULL;
                H5P_CLS_OBJECT_CREATE_g    = NULL;
                H5P_CLS_REFERENCE_ACCESS_g = NULL;
                H5P_CLS_STRING_CREATE_g    = NULL;
                H5P_CLS_VOL_INITIALIZE_g   = NULL;

                H5P_CLS_ROOT_ID_g = H5I_INVALID_HID;

                H5P_CLS_ATTRIBUTE_ACCESS_ID_g = H5I_INVALID_HID;
                H5P_CLS_ATTRIBUTE_CREATE_ID_g = H5I_INVALID_HID;
                H5P_CLS_DATASET_ACCESS_ID_g   = H5I_INVALID_HID;
                H5P_CLS_DATASET_CREATE_ID_g   = H5I_INVALID_HID;
                H5P_CLS_DATASET_XFER_ID_g     = H5I_INVALID_HID;
                H5P_CLS_DATATYPE_ACCESS_ID_g  = H5I_INVALID_HID;
                H5P_CLS_DATATYPE_CREATE_ID_g  = H5I_INVALID_HID;
                H5P_CLS_FILE_ACCESS_ID_g      = H5I_INVALID_HID;
                H5P_CLS_FILE_CREATE_ID_g      = H5I_INVALID_HID;
                H5P_CLS_FILE_MOUNT_ID_g       = H5I_INVALID_HID;
                H5P_CLS_GROUP_ACCESS_ID_g     = H5I_INVALID_HID;
                H5P_CLS_GROUP_CREATE_ID_g     = H5I_INVALID_HID;
                H5P_CLS_LINK_ACCESS_ID_g      = H5I_INVALID_HID;
                H5P_CLS_LINK_CREATE_ID_g      = H5I_INVALID_HID;
                H5P_CLS_MAP_ACCESS_ID_g       = H5I_INVALID_HID;
                H5P_CLS_MAP_CREATE_ID_g       = H5I_INVALID_HID;
                H5P_CLS_OBJECT_COPY_ID_g      = H5I_INVALID_HID;
                H5P_CLS_OBJECT_CREATE_ID_g    = H5I_INVALID_HID;
                H5P_CLS_REFERENCE_ACCESS_ID_g = H5I_INVALID_HID;
                H5P_CLS_STRING_CREATE_ID_g    = H5I_INVALID_HID;
                H5P_CLS_VOL_INITIALIZE_ID_g   = H5I_INVALID_HID;
            }
        }

        n++; /*H5I*/
    }
    else {
        /* Destroy the property list and class id groups */
        n += (H5I_dec_type_ref(H5I_GENPROP_LST) > 0);
        n += (H5I_dec_type_ref(H5I_GENPROP_CLS) > 0);
    } /* end else */

    /**
     * If all classes, lists, and properties have been closed, iterate the
     * free lists of the classes, lists, and properties and free any structs
     * still there.
     */
    if (n == 0) {
        result = H5P__mt_term_free_lists();
        assert(result >= 0);
    }

    FUNC_LEAVE_NOAPI(n)

} /* end H5P_term_package() MT safe version */

/****************************************************************************************
 * Function:    H5P__mt_term_free_lists
 *
 *              Multithread version only function
 *
 * Purpose:     Function for terminating all H5P structures still on their free lists.
 *
 * Return:      VOID
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_term_free_lists(void)
{
    H5P_mt_list_t               *head_list;
    H5P_mt_list_sptr_t           fl_list_head;
    H5P_mt_list_sptr_t           next_list;
    H5P_mt_list_sptr_t           fl_list_tail;
    H5P_mt_class_t              *head_class;
    H5P_mt_class_sptr_t          fl_class_head;
    H5P_mt_class_sptr_t          next_class;
    H5P_mt_class_sptr_t          fl_class_tail;
    H5P_mt_prop_t               *head_prop;
    H5P_mt_prop_aptr_t           fl_prop_head;
    H5P_mt_prop_aptr_t           next_prop;
    H5P_mt_prop_aptr_t           fl_prop_tail;
    H5P_mt_active_thread_count_t thrd;
    bool                         done = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    /* Frees all property lists by iterating the list free list */

    fl_list_head = atomic_load(&(H5P_mt_g.list_fl_head));
    fl_list_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

    while (fl_list_head.ptr) {
        do {
            head_list = fl_list_head.ptr;
            next_list = atomic_load(&(head_list->fl_next));

            /* Ensure no other threads are in this struct */
            thrd = atomic_load(&(head_list->thrd));

            assert(thrd.count == 0);
            assert(thrd.opening == FALSE);
            assert(thrd.closing == TRUE);

            if (!atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_head), &fl_list_head, next_list)) {
                /* atomic update failed, update stats and try again */
                atomic_fetch_add(&(H5P_mt_g.list_fl_head_update_cols), 1);
            }
            else {
                atomic_fetch_add(&(H5P_mt_g.list_fl_head_update), 1);
                done = TRUE;
            }

            /* If we are at the last entry in the free list update the tail */
            if (fl_list_tail.ptr == fl_list_head.ptr) {
                done = FALSE;

                assert(next_list.ptr == NULL);

                if (!atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_tail), &fl_list_tail, next_list)) {
                    /* atomic update failed, update stats and try again */
                    atomic_fetch_add(&(H5P_mt_g.list_fl_tail_update_cols), 1);
                }
                else {
                    atomic_fetch_add(&(H5P_mt_g.list_fl_tail_update), 1);
                    done = TRUE;
                }
            }

        } while (!done);

        atomic_store(&(head_list->tag), H5P_MT_LIST_FL_REALLOC_TAG);

        head_list = H5P__clear_mt_list(head_list);

        free(head_list);
        head_list = NULL;

        fl_list_head = atomic_load(&(H5P_mt_g.list_fl_head));
        fl_list_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

        if (fl_list_head.ptr) {
            assert(fl_list_tail.ptr);
            atomic_fetch_sub(&(H5P_mt_g.list_fl_len), 1);
        }

    } /* end while ( fl_list_head.ptr ) */

    /* Ensure the head and tail of the list's free list are NULL */
    assert(NULL == fl_list_head.ptr);
    assert(NULL == fl_list_tail.ptr);

    done = FALSE;

    /* Frees all property classes by iterating the class free list */

    fl_class_head = atomic_load(&(H5P_mt_g.class_fl_head));
    fl_class_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

    while (fl_class_head.ptr) {
        do {
            head_class = fl_class_head.ptr;
            next_class = atomic_load(&(head_class->fl_next));

            /* Ensure no other threads are in this struct */
            thrd = atomic_load(&(head_class->thrd));

            assert(thrd.count == 0);
            assert(thrd.opening == FALSE);
            assert(thrd.closing == TRUE);

            if (!atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_head), &fl_class_head, next_class)) {
                /* atomic update failed, update stats and try again */
                atomic_fetch_add(&(H5P_mt_g.class_fl_head_update_cols), 1);
            }
            else {
                atomic_fetch_add(&(H5P_mt_g.class_fl_head_update), 1);
                done = true;
            }

            /* If we are at the last entry in the free list update the tail */
            if (fl_class_tail.ptr == fl_class_head.ptr) {
                done = FALSE;

                assert(next_class.ptr == NULL);

                if (!atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_tail), &fl_class_tail, next_class)) {
                    /* atomic update failed, update stats and try again */
                    atomic_fetch_add(&(H5P_mt_g.class_fl_tail_update_cols), 1);
                }
                else {
                    atomic_fetch_add(&(H5P_mt_g.class_fl_tail_update), 1);
                    done = TRUE;
                }
            }

        } while (!done);

        atomic_store(&(head_class->tag), H5P_MT_CLASS_FL_REALLOC_TAG);

        head_class = H5P__clear_mt_class(head_class);

        free(head_class);
        head_class = NULL;

        fl_class_head = atomic_load(&(H5P_mt_g.class_fl_head));
        fl_class_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

        if (fl_class_head.ptr) {
            assert(fl_class_tail.ptr);
            atomic_fetch_sub(&(H5P_mt_g.class_fl_len), 1);
        }

    } /* end while ( fl_class_head.ptr ) */

    /* Ensure the head and tail of the free list are NULL */
    assert(NULL == fl_class_head.ptr);
    assert(NULL == fl_class_tail.ptr);

    done = FALSE;

    /* Frees all property classes by iterating the class free list */

    fl_prop_head = atomic_load(&(H5P_mt_g.prop_fl_head));
    fl_prop_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

    while (fl_prop_head.ptr) {
        do {
            head_prop = fl_prop_head.ptr;
            next_prop = atomic_load(&(head_prop->next));

            if (!atomic_compare_exchange_strong(&(H5P_mt_g.prop_fl_head), &fl_prop_head, next_prop)) {
                /* atomic update failed, update stats and try again */
                atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update_cols), 1);
            }
            else {
                atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update), 1);
                done = true;
            }

            /* If we are at the last entry in the free list update the tail */
            if (fl_prop_tail.ptr == fl_prop_head.ptr) {
                done = FALSE;

                assert(next_prop.ptr == NULL);

                if (!atomic_compare_exchange_strong(&(H5P_mt_g.prop_fl_tail), &fl_prop_tail, next_prop)) {
                    /* atomic update failed, update stats and try again */
                    atomic_fetch_add(&(H5P_mt_g.prop_fl_tail_update_cols), 1);
                }
                else {
                    atomic_fetch_add(&(H5P_mt_g.prop_fl_tail_update), 1);
                    done = TRUE;
                }
            }

        } while (!done);

        atomic_store(&(head_prop->tag), H5P_MT_PROP_FL_REALLOC_TAG);

        head_prop = H5P__clear_mt_prop(head_prop);

        free(head_prop);
        head_prop = NULL;

        fl_prop_head = atomic_load(&(H5P_mt_g.prop_fl_head));
        fl_prop_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

        if (fl_prop_head.ptr) {
            assert(fl_prop_tail.ptr);
            atomic_fetch_sub(&(H5P_mt_g.prop_fl_len), 1);
        }

    } /* while ( fl_prop_head.ptr ) */

    /* Ensure the head and tail of the free list are NULL */
    assert(NULL == fl_prop_head.ptr);
    assert(NULL == fl_prop_tail.ptr);

    FUNC_LEAVE_NOAPI(ret_value);

} /* H5P__mt_term_free_lists() */

/****************************************************************************************
 * Function:    H5P__close_class_cb
 *
 *              No changes were made for multithread H5P
 *
 * Purpose:     Called by H5I when the ref count reaches zero on a property class's ID.
 *
 * Return:      SUCCEED / FAIL
 *
 ****************************************************************************************
 */
static herr_t
H5P__close_class_cb(void *_pclass, void H5_ATTR_UNUSED **request)
{
    H5P_mt_class_t *pclass = (H5P_mt_class_t *)_pclass;

    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(pclass);

    atomic_fetch_add(&(H5P_mt_g.H5P__close_class_cb__num_calls), 1);

    /* Close the property list class object */
    if (H5P__close_class(pclass) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CLOSEERROR, FAIL, "unable to close property list class");
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5P__close_class_cb() MT safe version */

/****************************************************************************************
 * Function:    H5P__close_list_cb
 *
 *              No changes were made for multithread H5P
 *
 * Purpose:     Called by H5I when the ref count reaches zero on a property list's ID.
 *
 * Return:      SUCCEED / FAIL
 *
 ****************************************************************************************
 */
static herr_t
H5P__close_list_cb(void *_plist, void H5_ATTR_UNUSED **request)
{
    H5P_genplist_t *plist = (H5P_genplist_t *)_plist; /* Property list to close */

    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(plist);

    atomic_fetch_add(&(H5P_mt_g.H5P__close_list_cb__num_calls), 1);

    /* Close the property list object */
    if (H5P_close(plist) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CLOSEERROR, FAIL, "unable to close property list");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P__close_list_cb() */

/****************************************************************************************
 * Function:    H5P__create_class
 *
 *              Multithread version of H5P__create_class().
 *
 * Purpose:     Internal routine to create a new property list class.
 *
 *              NOTE: property list classes are referred to as classes and property lists
 *              are referred to as lists.
 *
 *              A multi-thread safe function to create a new class derived from an
 *              existing class. The new class creates copies of the properties from it's
 *              parent class that are valid for the version of the parent class the new
 *              class is being derived from.
 *
 * Details:     NOTE: for more information on the H5P_mt_class_t structure or specific
 *              fields, check the detailed comment for the structure in H5Ppkg_mt.h
 *
 *              Call H5P__inc_thrd_count() on the parent to check its opening and
 *              closing flags, and to increment the thrd.count field.
 *
 *              Call H5P__mt_create_class__internal() to allocate and initialize the
 *              new class structure.
 *
 *              The parent class's LFSLL is then iterated to find and create copies of
 *              the valid properties using the function H5P__mt_copy_lfsll(), if the
 *              parent class has any properties other than the sentinel nodes.
 *
 *              Call H5I_inc_ref() to increment the parent class's ID ref count.
 *
 *              Lastly, call H5P__dec_thrd_count() to decrement the parent's thrd.count
 *              field.
 *
 *
 * Parameters:  H5P_mt_class_t *parent: Property class to derive the new class from.
 *              const char *name:       Name of the class being created.
 *              H5P_plist_type_t type:  Type of the class being created.
 *              uint64_t src_version:   Version of the parent class being copied.
 *              H5P_cls_create_func_t:  Callback function for when each property list
 *                                      derived from this class is created.
 *              void *create_data:      Pointer to user data to pass along to class
 *                                      create callback.
 *              H5P_cls_copy_func_t:    Callback function for when each property list
 *                                      derived from this class is copied.
 *              void *copy_data:        Pointer to user data to pass along to class
 *                                      copy callback.
 *              H5P_cls_close_func_t:   Callback function for when each property list
 *                                      derived from this class is closed.
 *              void *close_data:       Pointer to user data to pass along to class
 *                                      close callback.
 *
 *
 * Return:      Success: Returns a pointer to the new H5P_mt_class_t struct.
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_class_t *
H5P__create_class(H5P_mt_class_t *parent, const char *name, H5P_plist_type_t type, uint64_t src_version,
                  H5P_cls_create_func_t create_func, void *create_data, H5P_cls_copy_func_t copy_func,
                  void *copy_data, H5P_cls_close_func_t close_func, void *close_data)
{
    H5P_mt_class_t              *new_class = NULL;       /* New class to be created */
    H5P_mt_active_thread_count_t thrd;                   /* thrd struct for new class */
    H5P_mt_active_thread_count_t update_thrd;            /* used to atomically update thrd */
    uint64_t                     parent_version = 0;     /* Parent's version to derive */
    bool                         inc_thrd_flag  = FALSE; /* Flag to dec parent's thrd count */

    H5P_mt_class_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__create_class__num_calls), 1);

    /**
     * If parent is NULL then this is the root class, and there isn't a parent class to
     * increment. Otherwise, increment the thrd count in the parent class.
     */
    if (parent != NULL) {
        assert((atomic_load(&(parent->tag))) == H5P_MT_CLASS_TAG);

        parent_version = atomic_load(&(parent->curr_version));

        /* Increment parent's thrd count */
        if (H5P__inc_thrd_count(parent) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, "Couldn't increment parent's thread count.");
        }

        inc_thrd_flag = TRUE;
    }
    else {
        /* update stats */
        atomic_fetch_add(&(H5P_mt_g.num_classes_created_wo_parent), 1);
    }

    if (src_version > 0) {
        /* If src_version is larger than any version of the parent, throw an error */
        if (src_version > parent_version) {
            assert(src_version <= parent_version);
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL,
                        "src_version is greater than parent's most current version.");
        }

        parent_version = src_version;
    }

    /* Allocates and initialize a new property class */
    if (NULL == (new_class = H5P__mt_create_class__internal(parent, name, type, parent_version, create_func,
                                                            create_data, copy_func, copy_data, close_func,
                                                            close_data))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create property class.");
    }

    /**
     * Copy the valid props from the parent into the new_class.
     * If root class then there is no parent.
     */
    if (parent != NULL) {
        /*If the phy_pl_len of the parent's LFSLL is 2, there are not props to copy */
        if (2 < (atomic_load(&(parent->phys_pl_len)))) {
            if (0 > H5P__mt_copy_lfsll(new_class, parent->pl_head, parent_version)) {
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, NULL, "Failed to copy parent's lfsll.");
            }

        } /* end if ( 2 < (atomic_load(&(parent->phys_pl_len))) ) */

    } /* end if ( parent != NULL ) */

    /**
     * If the parent isn't NULL (should only be NULL for root class),
     * increment the ID for the parent in the index.
     */
    if (parent) {
        if (0 >= H5I_inc_ref(atomic_load(&(parent->id)), FALSE)) {
            assert(FALSE);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, "Failed to increment parent's ID index ref count");
        }
    }

    /* Set opening flag to FALSE */
    thrd = atomic_load(&(new_class->thrd));

    assert(thrd.opening);
    assert(!thrd.closing);

    update_thrd.count   = thrd.count;
    update_thrd.opening = FALSE;
    update_thrd.closing = FALSE;

    atomic_store(&(new_class->thrd), update_thrd);

    ret_value = new_class;

done:

    /* update parent's thrd count */
    if (parent != NULL && inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(parent)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, NULL, "Failed to decrement parent's thrd_count.");
        }
    }

    /* Clean up if an error occurred */
    if ((ret_value == NULL) && (new_class)) {
        H5P__close_class(new_class);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__create_class() MT safe version*/

/****************************************************************************************
 * Function:    H5P__copy_pclass
 *
 *              Multithread safe version of H5P__copy_pclass.
 *
 * Purpose:     Internal routine to create a copy of an existing property class
 *              (H5P_mt_class_t).
 *
 *              NOTE: og_class is the original class that is being copied.
 *
 *              NOTE: This routine does not make any callbacks. Those are only made when
 *              operating on property lists.
 *
 *
 * Return:      Success: Returns a pointer to the new H5P_mt_class_t structure
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_class_t *
H5P__copy_pclass(H5P_mt_class_t *og_class)
{
    H5P_mt_class_t              *parent    = NULL;      /* Parent of original class */
    H5P_mt_class_t              *new_class = NULL;      /* Copy of the og_class */
    H5P_mt_active_thread_count_t thrd;                  /* thrd struct for new class */
    H5P_mt_active_thread_count_t update_thrd;           /* used to atomically update thrd */
    uint64_t                     ver_at_copy   = 0;     /* version of parent og_class is derived */
    bool                         inc_thrd_flag = FALSE; /* Flag to dec og_class's thrd count*/
    bool                         par_thrd_flag = FALSE; /* Flag to dec parent's thrd count */

    H5P_mt_class_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__copy_pclass__num_calls), 1);

    assert(og_class);
    assert(atomic_load(&(og_class->tag)) == H5P_MT_CLASS_TAG);

    /* Increment the original class's thrd count */
    if (H5P__inc_thrd_count(og_class) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, "Failed to increment og_class's thread count.");
    }

    inc_thrd_flag = TRUE;

    /**
     * The new class is a copy so it must be 'derived' from
     * the same version of the parent as the original class.
     */
    ver_at_copy = atomic_load(&(og_class->curr_version));

    /**
     * Callback function for testing, to pass the exact version
     * of the original class the new class is being copied from.
     */
    if (H5P_mt_cb.ver_cb) {
        (H5P_mt_cb.ver_cb)(ver_at_copy);
    }
    /**
     * end testing function
     */

    parent = og_class->parent_ptr;

    /**
     * If parent is NULL, there isn't a parent class to increment.
     * Otherwise, increment the thrd count in the parent class.
     */
    if (parent != NULL) {
        assert((atomic_load(&(parent->tag))) == H5P_MT_CLASS_TAG);

        /* Increment parent's thrd count */
        if (H5P__inc_thrd_count(parent) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, "Failed to increment parent's thread count.");
        }

        par_thrd_flag = TRUE;
    }
    else {
        /* update stats */
        atomic_fetch_add(&(H5P_mt_g.num_classes_created_wo_parent), 1);
    }

    /* Allocates and initialize a new property class */
    if (NULL == (new_class = H5P__mt_create_class__internal(
                     parent, og_class->name, og_class->type, og_class->parent_version, og_class->create_func,
                     og_class->create_data, og_class->copy_func, og_class->copy_data, og_class->close_func,
                     og_class->close_data))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create property class.");
    }

    /* Copy the valid properties from the original class's LFSLL */
    if (0 > H5P__mt_copy_lfsll(new_class, og_class->pl_head, ver_at_copy)) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, NULL, "Failed to copy og_class's lfsll.");
    }

    /**
     * If the parent isn't NULL (should only occur for root class),
     * increment the ID for the parent in the index.
     */
    if (parent) {
        if (0 >= H5I_inc_ref(atomic_load(&(parent->id)), FALSE)) {
            assert(FALSE);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, "Faile to increment parent's ID index ref count");
        }
    }

    thrd = atomic_load(&(new_class->thrd));

    assert(thrd.opening);
    assert(!thrd.closing);

    update_thrd.count   = thrd.count;
    update_thrd.opening = FALSE;
    update_thrd.closing = FALSE;

    atomic_store(&(new_class->thrd), update_thrd);

    ret_value = new_class;

done:

    /* update parent class's thrd count */
    if (parent && par_thrd_flag) {
        if (0 > H5P__dec_thrd_count(parent)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, NULL, "Failed to decrement parent's thrd_count.");
        }
    }

    /* update original class's thrd count */
    if (og_class && inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(og_class)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, NULL, "Failed to decrement og_class's thrd_count.");
        }
    }

    /* Clean up if an error occurred */
    if ((ret_value == NULL) && (new_class)) {
        H5P__close_class(new_class);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__copy_pclass() MT safe version */

/****************************************************************************************
 * Function:    H5P__mt_create_class__internal
 *
 *              Multithread version only function.
 *
 * Purpose:     Allocates and initializes a property class struct.
 *
 * Details:     Increment the parent's ref_count (if this is a root class it won't have a
 *              parent class). Allocate memory for the new class structure. Initialize
 *              the new class fields, including allocating a name buffer for the name of
 *              the class, and allocating and initializing the sentinel nodes for the
 *              LFSLL. Set thrd.opening flag to TRUE. Lastly initialize the class's
 *              statistics fields
 *
 * Return:      Success: Pointer to the new H5P_mt_class_t struct.
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_class_t *
H5P__mt_create_class__internal(H5P_mt_class_t *parent, const char *name, H5P_plist_type_t type,
                               uint64_t src_version, H5P_cls_create_func_t create_func, void *create_data,
                               H5P_cls_copy_func_t copy_func, void *copy_data,
                               H5P_cls_close_func_t close_func, void *close_data)
{
    H5P_mt_class_t              *new_class    = NULL;
    bool                         inc_ref_flag = FALSE;
    H5P_mt_class_ref_counts_t    ref_count;
    H5P_mt_active_thread_count_t thrd;
    H5P_mt_class_sptr_t          fl_next;

    H5P_mt_class_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__mt_create_class__internal__num_calls), 1);

    /* Update parent's ref count if parent is NULL. Currently can't fail */
    if (parent != NULL) {
        H5P__inc_ref_count(parent, TRUE);
        inc_ref_flag = TRUE;
    }

    /* Allocates a new property list class */
    new_class = H5P__mt_alloc_class();
    if (NULL == new_class) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to allocate property list class.");
    }

    /* Initialize class fields */
    atomic_store(&(new_class->tag), H5P_MT_CLASS_TAG);

    /* If root class then there is no parent */
    if (parent != NULL) {
        new_class->parent_id = atomic_load(&(parent->id));
        ;
    }
    else {
        new_class->parent_id = H5I_INVALID_HID;
    }

    new_class->parent_ptr     = parent;
    new_class->parent_version = src_version;

    new_class->name = strdup(name);
    if (new_class->name == NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, NULL, "Failed to copy name buffer.");
    }

    atomic_store(&(new_class->id), H5I_INVALID_HID);
    new_class->type = type;

    atomic_store(&(new_class->curr_version), 1);
    atomic_store(&(new_class->next_version), 2);

    /* Creates the sentinel nodes and sets the negative sentinel as the head */
    new_class->pl_head = H5P__create_sentinels(TRUE);
    if (new_class->pl_head == NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create sentinel nodes.");
    }

    atomic_store(&(new_class->phys_pl_len), 2);
    atomic_store(&(new_class->log_pl_len), 0);
    atomic_store(&(new_class->nprops_added), 0);

    ref_count.pl      = 0;
    ref_count.plc     = 0;
    ref_count.deleted = FALSE;
    atomic_store(&(new_class->ref_count), ref_count);

    thrd.count   = 0;
    thrd.opening = TRUE;
    thrd.closing = FALSE;
    atomic_store(&(new_class->thrd), thrd);

    fl_next.ptr = NULL;
    fl_next.sn  = 0;
    atomic_store(&(new_class->fl_next), fl_next);

    /* Set the callbacks */
    new_class->create_func = create_func;
    new_class->create_data = create_data;
    new_class->copy_func   = copy_func;
    new_class->copy_data   = copy_data;
    new_class->close_func  = close_func;
    new_class->close_data  = close_data;

    /* Initializes the class's stats fields */
    H5P__init_stats_class(new_class);

    ret_value = new_class;

done:

    /* If an error occured, properly handle the allocated memory */
    if (!ret_value) {
        if (new_class) {
            H5P__close_class(new_class);
        }
        if (inc_ref_flag) {
            H5P__dec_ref_count(parent, TRUE);
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_create_class__internal() */

/****************************************************************************************
 * Function:    H5P__mt_alloc_class
 *
 *              Multithread version only function.
 *
 * Purpose:     Gets a pointer to a valid H5P_mt_class_t struct by either reallocating
 *              one from the free list if one is reallocable, or by allocating a new one
 *              from memory.
 *
 *              NOTE: In this iteration the free list has not been tested and a new one
 *              is allocated from memory every time.
 *
 * Return:      Success: Pointer to a H5P_mt_class_t struct
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_class_t *
H5P__mt_alloc_class(void)
{
    H5P_mt_class_sptr_t fl_head; /* First node of the class free list */
    H5P_mt_class_sptr_t fl_tail;
    H5P_mt_class_sptr_t fl_next;           /* Next node of the class free list */
    H5P_mt_class_t     *head_class;        /* Class the fl_head points to */
    H5P_mt_class_t     *new_class = NULL;  /* New class to be allocated */
    bool                done      = FALSE; /* Flag to exit a loop */

    H5P_mt_class_t *ret_value = NULL;

    FUNC_ENTER_NOAPI(FAIL)

    /* Get head of the free list */
    fl_head = atomic_load(&(H5P_mt_g.class_fl_head));
    fl_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

    head_class = fl_head.ptr;

    /* If no class structs are reallocable, alloc from memory */
    if (atomic_load(&(head_class->tag)) != H5P_MT_CLASS_FL_REALLOC_TAG || (fl_head.ptr == fl_tail.ptr)) {
        new_class = (H5P_mt_class_t *)malloc(sizeof(H5P_mt_class_t));
        if (NULL == new_class) {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "Failed to allocate property class");
        }

        /* update stats */
        atomic_fetch_add(&(H5P_mt_g.num_class_structs_allocated_from_heap), 1);
    }
    /* If a struct on the free list is reallocable, clear it and return it */
    else {
        /* Remove the struct from the free list */
        do {
            fl_next = atomic_load(&(head_class->fl_next));

            fl_head = atomic_load(&(H5P_mt_g.class_fl_head));

            if (!atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_head), &fl_head, fl_next)) {
                /* failed, update stats and try again */
                atomic_fetch_add(&(H5P_mt_g.class_fl_head_update_cols), 1);

                /* assert is to not get stuck in an infinite loop while testing */
                assert(H5P_MT_ASSERT_FAIL);
            }
            else {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.class_fl_head_update), 1);
                atomic_fetch_sub(&(H5P_mt_g.class_fl_len), 1);

                done = TRUE;
            }

        } while (!done);

        /* Ensure the struct is cleared of any previous data */
        if (NULL == (new_class = H5P__clear_mt_class(fl_head.ptr))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Failed clearing class");
        }

        /* update stats */
        atomic_fetch_add(&(H5P_mt_g.num_class_structs_allocated_from_fl), 1);

    } /* end else */

done:

    ret_value = new_class;

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_alloc_class() */

/****************************************************************************************
 * Function:    H5P_create_id
 *
 *              Multithread safe version of H5P_create_id()
 *
 * Purpose:     Internal routine to create a new property list of a property list class.
 *
 *              NOTE: property list classes are referred to as classes and property lists
 *              are referred to as lists.
 *
 *              This function is basically just a pass-through function calling
 *              H5P__create_list() to actually allocate and initialize a new list derived
 *              from the provided pclass. This was done due to the need of having a
 *              function that returns the ID of the newly created list to fit with most
 *              of the existing functions. But also a function that returns a pointer to
 *              the newly created list (H5P_mt_list_t), due to the complexity of ensuring
 *              H5P be multithread safe.
 *
 *              NOTE: the primary example of needing a function to return a pointer to
 *              the list is the multithread version of H5P_init_phase1(). After creating
 *              the default list of a class it stores the ID of the list in the
 *              associated H5P_libclass_t struct, but the multithread version also sets
 *              up a global variable for the version of every main hdf5 library default
 *              list (see the description of the multithread version of H5P_init_phase1()
 *              for more details). To do this a pointer to the struct is needed and this
 *              method made more since than to search the index for the list with the
 *              returned ID.
 *
 *
 * Return:      Success: ID of the new plist
 *
 *              Failure: H5I_INVALID_HID
 *
 ****************************************************************************************
 */
hid_t
H5P_create_id(H5P_mt_class_t *pclass, hbool_t app_ref)
{
    H5P_mt_list_t *plist = NULL;

    hid_t ret_value = H5I_INVALID_HID; /* return value */

    FUNC_ENTER_NOAPI(H5I_INVALID_HID)

    assert(pclass);
    assert(atomic_load(&(pclass->tag)) == H5P_MT_CLASS_TAG);

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P_create_id__num_calls), 1);

    if (NULL == (plist = H5P__create_list(pclass, app_ref))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, H5I_INVALID_HID, "unable to create MT property list");
    }

    ret_value = atomic_load(&(plist->plist_id));

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_create_id() MT safe version */

/****************************************************************************************
 * Function:    H5P__create_list
 *
 *              Multithread version only function
 *
 * Purpose:     Internal routine to create a new property list of a property list class.
 *
 *              NOTE: property list classes are referred to as classes and property lists
 *              are referred to as lists.
 *
 *              Lists create an array of H5P_mt_list_table_entry_t of length
 *              nprops_inherited which point to the valid properties in the parent
 *              class's LFSLL.
 *
 *
 * Details:     NOTE: for more information of the H5P_mt_list_t structure or specific
 *              fields, check the detailed comment for the struture in H5Ppkg_mt.h
 *
 *              Call H5P__inc_thrd_count() on the parent to check its opening and
 *              closing flags, and to increment the thrd.count field.
 *
 *              Call H5P__mt_create_list__internal() to allocate and initialize the
 *              new list structure.
 *
 *              Call H5P__init_lkup_tbl() to allocated and initialize the new list's
 *              lkup_tbl to have an entry that points to every valid property in the
 *              parent's LFSLL at the version being derived from.
 *
 *              Register the new list in the index via H5I_register().
 *
 *              Walk up the parent inheritance tree and call each create callback, if
 *              that specific parent has one.
 *
 *              Call H5I_inc_ref to increment the parent's ID ref count.
 *
 *              Set thrd.opening flag to FALSE.
 *
 *              Lastly, call H5P__dec_thrd_count() to decrement the parent's thrd.count
 *              field.
 *
 *
 * Return:      Success: ID of the new plist
 *
 *              Failure: H5I_INVALID_HID
 *
 ****************************************************************************************
 */
H5P_mt_list_t *
H5P__create_list(H5P_mt_class_t *pclass, hbool_t app_ref)
{
    H5P_mt_list_t               *plist = NULL;
    hid_t                        plist_id;
    H5P_mt_class_t              *parent_walk   = NULL;
    bool                         inc_thrd_flag = FALSE; /* Flag to dec parent's thrd count */
    uint64_t                     version       = 0;     /* version of the original list being copied */
    H5P_mt_active_thread_count_t thrd;

    H5P_mt_list_t *ret_value = NULL; /* return value */

    FUNC_ENTER_NOAPI(H5I_INVALID_HID)

    assert(pclass);
    assert(atomic_load(&(pclass->tag)) == H5P_MT_CLASS_TAG);

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__create_list__num_calls), 1);

    /* Increment parent's thrd count */
    if (H5P__inc_thrd_count(pclass) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, "Couldn't increment parent's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    version = atomic_load(&(pclass->curr_version));

    /* Create the new MT property list */
    if (NULL == (plist = H5P__mt_create_list__internal(pclass, version))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "unable to create MT property list");
    }

    /* Allocate and intialize lkup_tbl */
    if (0 > (H5P__init_lkup_tbl(pclass, version, plist))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create lkup_tbl.");
    }

    /* Register the new list in the index and get and ID. */
    if ((plist_id = H5I_register(H5I_GENPROP_LST, plist, app_ref)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, NULL, "unable to register property list");
    }

    atomic_store(&(plist->plist_id), plist_id);

    /**
     * Call the class create callback on the parent
     * classes up the inheritance tree, if it exits.
     */
    parent_walk = pclass;

    while (parent_walk) {
        /* If the parent has a create callback, call it */
        if (parent_walk->create_func) {

            /* If the create callback fails remove the list's id from the index */
            if ((parent_walk->create_func)(plist_id, parent_walk->create_data) < 0) {
                H5I_remove(plist_id);

                assert(H5P_MT_ASSERT_FAIL);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, NULL, "Can't initialize property");
            }

        } /* end if ( parent->create_func ) */

        /* Increment up the parent tree */
        parent_walk = parent_walk->parent_ptr;

    } /* end while ( parent ) */

    /* Set the class initialization flag */
    atomic_store(&(plist->class_init), TRUE);

    if (0 >= H5I_inc_ref(atomic_load(&(pclass->id)), FALSE)) {
        assert(FALSE);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, "unable to increment parent's ID ref_count in index");
    }

    /* update thrd struct of the new_list to mark opening FALSE */
    thrd.count   = 0;
    thrd.opening = FALSE;
    thrd.closing = FALSE;

    atomic_store(&(plist->thrd), thrd);

    ret_value = plist;

done:

    /* update parent's thrd count */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(pclass)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, NULL, "Failure to decrement thrd_count.");
        }
    }

    if (NULL == ret_value && plist) {
        H5P_close(plist);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__create_list() MT safe version */

/****************************************************************************************
 * Function:    H5P_copy_plist
 *
 *              Multithread safe version of H5P__copy_plist.
 *
 * Purpose:     Internal routine to copy a property list (H5P_mt_list_t).
 *
 *              NOTE: og_list is the original plist that is being copied.
 *
 * Details:
 *
 * Return:      Success: ID of the new copy of the plist
 *
 *              Failure: H5I_INVALID_HID
 *
 ****************************************************************************************
 */
hid_t
H5P_copy_plist(H5P_mt_list_t *og_list, hbool_t app_ref)
{
    H5P_mt_class_t *parent;
    H5P_mt_class_t *parent_walk = NULL; /* The current parent when iterating the inheritance tree */
    H5P_mt_list_t  *new_list    = NULL;
    hid_t           new_plist_id;
    H5P_mt_active_thread_count_t thrd;
    uint64_t                     ver_at_copy        = 0;     /* version of the original list being copied */
    bool                         inc_thrd_flag_list = FALSE; /* flag to dec og_list thrd count */
    bool                         inc_thrd_flag      = FALSE; /* Flag to dec parent's thrd count */

    hid_t ret_value = H5I_INVALID_HID; /* return value */

    FUNC_ENTER_NOAPI(H5I_INVALID_HID)

    assert(og_list);
    assert(atomic_load(&(og_list->tag)) == H5P_MT_LIST_TAG);

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P_copy_plist__num_calls), 1);

    /* Increment the original list's thrd.count */
    if (H5P__inc_thrd_count(og_list) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, H5I_INVALID_HID, "Failed to increment og_list's thread count.");
    }
    else {
        inc_thrd_flag_list = TRUE; /* Set flag to dec thrd count */
    }

    /* If the context is initialized grab version from it */
    if (H5P_H5CX_INIT_g) {
        if (0 == (ver_at_copy = H5CX_get_plist_version(atomic_load(&(og_list->plist_id))))) {
            ver_at_copy = atomic_load(&(og_list->curr_version));
        }
        else {
            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_list_version_from_ctx), 1);
        }
    }
    else {
        ver_at_copy = atomic_load(&(og_list->curr_version));
    }

    assert(ver_at_copy > 0);

    /**
     * Callback function for testing, to pass the exact version
     * of the original list the new list is being copied from.
     */
    if (H5P_mt_cb.ver_cb) {
        (H5P_mt_cb.ver_cb)(ver_at_copy);
    }
    /**
     * end testing function
     */

    parent = og_list->pclass_ptr;
    assert(parent);
    assert((atomic_load(&(parent->tag))) == H5P_MT_CLASS_TAG);

    /* Increment parent's thrd count */
    if (H5P__inc_thrd_count(parent) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, H5I_INVALID_HID, "Failed to increment parent's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* Allocates and initialize a new property list */
    if (NULL == (new_list = H5P__mt_create_list__internal(parent, og_list->pclass_version))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, H5I_INVALID_HID, "Failed to create property list.");
    }

    /* Allocate and intialize lkup_tbl */

    /* Copy original list's lkup_tbl */
    if (0 > (H5P__init_lkup_tbl_copy(og_list, ver_at_copy, new_list))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, H5I_INVALID_HID, "Failed to copy lkup_tbl.");
    }

    /* Iterate the og_list's LFSLL and copy the valid props into the new list */
    if (0 > H5P__mt_copy_lfsll(new_list, og_list->pl_head, ver_at_copy)) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, H5I_INVALID_HID, "Failed to copy og_list's lfsll");
    }

    /* Register the new list in the index and get and ID. */
    if ((new_plist_id = H5I_register(H5I_GENPROP_LST, new_list, app_ref)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, H5I_INVALID_HID,
                    "Failed to register property list in index");
    }

    atomic_store(&(new_list->plist_id), new_plist_id);

    /**
     * Call the class copy callback on the parent classes
     * up the inheritance tree, if it exists.
     */
    parent_walk = parent;

    while (parent_walk) {
        /* If the class has a copy callback, call it */
        if (parent_walk->copy_func) {
            hid_t new_list_id = atomic_load(&(new_list->plist_id));
            hid_t og_list_id  = atomic_load(&(og_list->plist_id));

            /* If the copy callback fails remove the list's id from the index */
            if ((parent_walk->copy_func)(new_list_id, og_list_id, parent_walk->copy_data) < 0) {
                H5I_remove(new_plist_id);

                assert(H5P_MT_ASSERT_FAIL);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, H5I_INVALID_HID, "Failed to initialize property");
            }

        } /* end if ( parent0>copy_func ) */

        /* Increment up the parent tree */
        parent_walk = parent_walk->parent_ptr;

    } /* end while ( parent_walk ) */

    /* Set the class initialization flag */
    atomic_store(&(new_list->class_init), TRUE);

    if (0 >= H5I_inc_ref(atomic_load(&(parent->id)), FALSE)) {
        assert(FALSE);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, H5I_INVALID_HID,
                    "Failed to increment parent's ID index ref count");
    }

    /* update thrd struct of the new_list to mark opening FALSE */
    thrd.count   = 0;
    thrd.opening = FALSE;
    thrd.closing = FALSE;

    atomic_store(&(new_list->thrd), thrd);

    ret_value = atomic_load(&(new_list->plist_id));

done:

    /* update parent's thrd count */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(parent)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, H5I_INVALID_HID, "Failure to decrement thrd_count.");
        }
    }

    /* update original list's thrd count */
    if (inc_thrd_flag_list) {
        if (0 > H5P__dec_thrd_count(og_list)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, H5I_INVALID_HID, "Failure to decrement thrd_count.");
        }
    }

    /* Clean up if an error occurred */
    if ((ret_value == H5I_INVALID_HID) && (new_list)) {
        H5P_close(new_list);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_copy_plist() MT safe version */

/****************************************************************************************
 * Function:    H5P__mt_create_list__internal
 *
 *              Multithread version only function.
 *
 * Purpose:     Allocates and initializes a property list struct.
 *
 * Details:     Increment the parent's ref_count. Allocate memory for the new list
 *              structure. Initialize the new list's fields, including allocating and
 *              initializing the sentinel nodes for the LFSLL. Set thrd.opening flag to
 *              TRUE. Lastly initialize the list's statistics fields
 *
 * Return:      Success: Pointer to the new H5P_mt_list_t struct.
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_list_t *
H5P__mt_create_list__internal(H5P_mt_class_t *parent, uint64_t src_version)
{
    H5P_mt_list_t               *new_list     = NULL; /* New list to be created */
    bool                         inc_ref_flag = FALSE;
    H5P_mt_active_thread_count_t list_thrd; /* thrd struct for new list */
    H5P_mt_list_sptr_t           fl_next;   /* new list's free list struct */

    H5P_mt_list_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__mt_create_list__internal__num_calls), 1);

    /* Update parent's ref count. Currently can't fail */
    H5P__inc_ref_count(parent, FALSE);
    inc_ref_flag = TRUE;

    /* Allocates a new property list */
    new_list = H5P__mt_alloc_list();
    if (NULL == new_list) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create new property list.");
    }

    /* Initialize list fields */
    atomic_store(&(new_list->tag), H5P_MT_LIST_TAG);

    new_list->pclass_id      = atomic_load(&(parent->id));
    new_list->pclass_ptr     = parent;
    new_list->pclass_version = src_version;

    atomic_store(&(new_list->plist_id), H5I_INVALID_HID);
    atomic_store(&(new_list->curr_version), 1);
    atomic_store(&(new_list->next_version), 2);

    new_list->def_ver_ptr = NULL;
    new_list->lkup_tbl    = NULL;

    new_list->nprops_inherited = 0;
    atomic_store(&(new_list->nprops_added), 0);
    atomic_store(&(new_list->nprops), 0);

    /* Creates the sentinel nodes and sets the negative sentinel as pl_head */
    new_list->pl_head = H5P__create_sentinels(FALSE);
    if (NULL == new_list->pl_head) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create sentinels nodes.");
    }

    atomic_store(&(new_list->log_pl_len), 0);
    atomic_store(&(new_list->phys_pl_len), 2);

    /* Set class initialization flag to false for now */
    atomic_store(&(new_list->class_init), FALSE);

    /* Set opening flag to TRUE */
    list_thrd.count   = 0;
    list_thrd.opening = TRUE;
    list_thrd.closing = FALSE;
    atomic_store(&(new_list->thrd), list_thrd);

    fl_next.ptr = NULL;
    fl_next.sn  = 0;
    atomic_store(&(new_list->fl_next), fl_next);

    /* Initialize all the stats for the list */
    H5P__init_stats_list(new_list);

    ret_value = new_list;

done:

    /* If an error occured, properly handle the allocated memory */
    if (!ret_value) {
        if (new_list) {
            H5P_close(new_list);
        }
        if (inc_ref_flag) {
            H5P__dec_ref_count(parent, FALSE);
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_create_list__internal*/

/****************************************************************************************
 * Function:    H5P__mt_alloc_list
 *
 * Purpose:     Gets a pointer to a valid H5P_mt_list_t struct by either reallocating
 *              one from the free list if one is reallocable, or by allocating a new one
 *              from memory.
 *
 *              NOTE: In this iteration the free list has not been tested and a new one
 *              is allocated from memory every time.
 *
 * Return:      Success: Returns a pointer to a H5P_mt_list_t
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_list_t *
H5P__mt_alloc_list(void)
{
    H5P_mt_list_sptr_t fl_head; /* First node of the list free list */
    H5P_mt_list_sptr_t test_fl_head;
    H5P_mt_list_sptr_t fl_tail;
    H5P_mt_list_sptr_t fl_next;          /* Next node of the list free list */
    H5P_mt_list_t     *head_list;        /* List the fl_head points to */
    H5P_mt_list_t     *new_list = NULL;  /* New list to be allocated */
    bool               done     = FALSE; /* Flag to exit a loop */

    H5P_mt_list_t *ret_value = NULL;

    FUNC_ENTER_NOAPI(FAIL)

    /* Get head of the free list */
    fl_head = atomic_load(&(H5P_mt_g.list_fl_head));
    fl_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

    head_list = fl_head.ptr;

    /* If no list structs are reallocable, alloc from memory */
    if (atomic_load(&(head_list->tag)) != H5P_MT_LIST_FL_REALLOC_TAG || (fl_head.ptr == fl_tail.ptr)) {
        new_list = (H5P_mt_list_t *)malloc(sizeof(H5P_mt_list_t));
        if (NULL == new_list) {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "property list allocation failed");
        }

        /* update stats */
        atomic_fetch_add(&(H5P_mt_g.num_list_structs_allocated_from_heap), 1);
    }
    /* If a struct on the free list is reallocable, clear it and return it */
    else {
        /* Remove the struct from the free list */
        do {
            fl_head = atomic_load(&(H5P_mt_g.list_fl_head));
            fl_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

            fl_next = atomic_load(&(fl_head.ptr->fl_next));

            test_fl_head = atomic_load(&(H5P_mt_g.list_fl_head));

            if (test_fl_head.ptr == fl_head.ptr && test_fl_head.sn == fl_head.sn) {
                head_list = fl_head.ptr;

                if (!atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_head), &fl_head, fl_next)) {
                    /* failed, update stats and try again */
                    atomic_fetch_add(&(H5P_mt_g.list_fl_head_update_cols), 1);

                    /* assert is to not get stuck in an infinite loop while testing */
                    assert(H5P_MT_ASSERT_FAIL);
                }
                else {
                    /* success, update stats and continue */
                    atomic_fetch_add(&(H5P_mt_g.list_fl_head_update), 1);
                    atomic_fetch_sub(&(H5P_mt_g.list_fl_len), 1);

                    done = TRUE;
                }
            }

        } while (!done);

        /* Ensure the struct is cleared of any previous data */
        new_list = H5P__clear_mt_list(fl_head.ptr);

        /* update stats */
        atomic_fetch_add(&(H5P_mt_g.num_list_structs_allocated_from_fl), 1);

    } /* end else */

done:

    ret_value = new_list;

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_alloc_list() */

/****************************************************************************************
 * Function:    H5P__init_lkup_tbl
 *
 *              Multithread version only function.
 *
 * Purpose:     Allocates and initializes an array of H5P_mt_list_table_entry_t structs
 *              for a property list field called lkup_tbl.
 *
 *
 * Details:     The function first iterates through the LFSLL of the list's parent class
 *              to counts the number of valid properties.
 *              Two things make a property invalid:
 *                  1) any property that has a create_version greater than the parent
 *                     class's version from which the new list is derived.
 *                  2) any property that has a delete_version less than or equal to the
 *                     parent class's version from which the new list is derived.
 *
 *              With the number of valid properties for the new list, the lkup_tbl is
 *              allocated. The parent class's LFSLL is iterated again, and the lkup_tbl's
 *              entries are setup to the valid properties. The chksum and *name fields in
 *              the lkup_tbl are copied from the parents's property. If the property
 *              doesn't have a create callback, the base.ptr is set to point to
 *              the property in the parent class's LFSLL, and base.ver is set to the
 *              intial version of the list (which is 1). base_delete_version is
 *              initialized to 0, and curr.ptr and curr.ver are initialized to NULL and 0
 *              respectively. Lastly, the parent's property increments it's ref_count, to
 *              count the new list having a pointer to that property.
 *
 *              However, if the parent's property has a create callback, a new
 *              property structure must be created, as a copy of the parent's. The new
 *              prop will be inserted into the list's LFSLL, and curr.ptr will point to
 *              the new_prop, curr.ver will be set to 1 (as will
 *              new_prop->create_version). base.ptr will be set to NULL, because we must
 *              ensure that we don't do anything to change or edit the parent's prop.
 *
 *              NOTE: We do not need to sort the lkup_tbl because the parent's LFSLL is
 *              already sorted, and we are initializing the lkup_tbl in the correct order
 *              from the start.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__init_lkup_tbl(H5P_mt_class_t *parent, uint64_t version, H5P_mt_list_t *new_list)
{
    H5P_mt_list_table_entry_t *entry;             /* Entry in the lkup_tbl */
    H5P_mt_list_prop_ref_t     base;              /* base prop ref in entry */
    H5P_mt_list_prop_ref_t     curr;              /* curr prop ref in entry */
    H5P_mt_prop_t             *valid_prop;        /* Valid prop in parent to reference */
    H5P_mt_prop_t             *parent_prop;       /* prev prop in parent LFSLL */
    H5P_mt_prop_t             *new_prop;          /* new prop if needed to be created */
    H5P_mt_prop_value_t        valid_prop_value;  /* Value of the valid prop to copy */
    uint32_t                   nprops;            /* number of properties in the list */
    uint32_t                   deletes       = 0; /* Tracks number of deletes */
    uint32_t                   nodes_visited = 0; /* Tracks number of nodes visited */
    uint32_t                   thrd_cols     = 0; /* Tracks number of thread cols */
    bool                       chksum_cols   = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    atomic_fetch_add(&(H5P_mt_g.H5P__init_lkup_tbl__num_calls), 1);

    parent_prop = parent->pl_head;
    assert(parent_prop);
    assert(atomic_load(&(parent_prop->tag)) == H5P_MT_PROP_TAG);
    assert(parent_prop->sentinel);

    /* Iterate the parent's LFSLL and count the valid properties */
    do {
        valid_prop = H5P__get_next_valid_prop(parent_prop, version, NULL);

        if (valid_prop) {
            /* Increment the number of props inherited from parent */
            new_list->nprops_inherited++;

            /* Iterate to next valid prop */
            parent_prop = valid_prop;
        }

    } while (valid_prop);

    /* Allocate the lkup_tbl array, as long as there are inherited props */
    if (new_list->nprops_inherited > 0) {
        /* Allocates the number of entries needed in the lkup_tbl */
        new_list->lkup_tbl = (H5P_mt_list_table_entry_t *)malloc(new_list->nprops_inherited *
                                                                 sizeof(H5P_mt_list_table_entry_t));
        if (NULL == new_list->lkup_tbl) {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "lkup_tbl allocation failed");
        }
    }
    else {
        new_list->lkup_tbl = NULL;
    }

    nprops = 0;

    parent_prop = parent->pl_head;

    /* Set up each lkup_tbl entry to the valid properties in the parent's LFSLL */
    if (new_list->lkup_tbl) {
        do {
            valid_prop = H5P__get_next_valid_prop(parent_prop, version, NULL);

            if (valid_prop) {
                /* Initialize the entry's fields */

                entry = &new_list->lkup_tbl[nprops];

                entry->chksum = valid_prop->chksum;
                entry->name   = strdup(valid_prop->name);
                if (entry->name == NULL) {
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Failed to copy name buffer.");
                }

                /**
                 * If the valid_prop has a create callback, set the base.ptr to NULL and
                 * create a new prop struct. Call the create callback and insert the
                 * new_prop into the new list's LFSLL.
                 */
                if (valid_prop->create) {
                    /* update stats */
                    atomic_fetch_add(&(new_list->num_inherited_with_create_cb), 1);

                    base.ptr = NULL;
                    base.ver = 1;
                    atomic_store(&(entry->base), base);

                    atomic_store(&(entry->base_delete_version), 0);

                    valid_prop_value = atomic_load(&(valid_prop->value));

                    new_prop =
                        H5P__create_prop(valid_prop->name, valid_prop_value.ptr, valid_prop_value.size, FALSE,
                                         1, valid_prop->create, valid_prop->set, valid_prop->get,
                                         valid_prop->encode, valid_prop->decode, valid_prop->del,
                                         valid_prop->copy, valid_prop->cmp, valid_prop->close);
                    if (NULL == new_prop)
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL,
                                    "Failed creating property for property list.");

                        /* Call the create callback */
#if 1
                    if (H5P__global_lock_prop_cb__create(new_prop, new_prop->name, valid_prop_value.size,
                                                         valid_prop_value.ptr) < 0) {
                        assert(H5P_MT_ASSERT_FAIL);
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Create property callback failed");
                    }
#else
                    if ((new_prop->create)(new_prop->name, valid_prop_value.size, valid_prop_value.ptr) < 0) {
                        assert(H5P_MT_ASSERT_FAIL);
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, "Can't create property");
                    }
#endif

                    /* Set the property's in_lkup_tbl flag */
                    new_prop->in_lkup_tbl = TRUE;

                    /* Inserts the new_prop into the new_list's LFSLL */
                    H5P__mt_ins_or_mod_prop__lfsll_ins(new_list->pl_head, new_prop, &deletes, &nodes_visited,
                                                       &thrd_cols, &chksum_cols);

                    atomic_fetch_add(&(new_list->log_pl_len), 1);
                    atomic_fetch_add(&(new_list->phys_pl_len), 1);

                    /* Set curr.ptr to point to the new prop in the list's LFSLL */
                    curr.ptr = new_prop;
                    curr.ver = 1;
                    atomic_store(&(entry->curr), curr);
                    atomic_store(&(entry->first_ver_of_curr), 1);

                } /* end if ( valid_prop->create ) */

                /**
                 * If new prop doesn't have the create callback set base.ptr to point to the
                 * parent's prop and curr.ptr to NULL
                 */
                else {
                    base.ptr = valid_prop;
                    base.ver = 1;
                    atomic_store(&(entry->base), base);

                    /* Increment parent's prop's ref_count */
                    atomic_fetch_add(&(valid_prop->ref_count), 1);

                    atomic_store(&(entry->base_delete_version), 0);

                    curr.ptr = NULL;
                    curr.ver = 0;
                    atomic_store(&(entry->curr), curr);
                    atomic_store(&(entry->first_ver_of_curr), 0);

                    /* update stats */
                    atomic_fetch_add(&(parent->num_prop_ref_count_update), 1);
                }

                /* Increment number of properties */
                nprops++;
                assert(nprops <= new_list->nprops_inherited);

                /* Iterate in the parent's LFSLL */
                parent_prop = valid_prop;

            } /* end if ( valid_prop ) */

        } while (valid_prop);
    }

    assert(nprops == new_list->nprops_inherited);

    atomic_store(&(new_list->nprops), nprops);

done:

    /* Clean up if error occurred */
    if ((ret_value == FAIL) && new_list->lkup_tbl) {
        free(new_list->lkup_tbl);
        new_list->lkup_tbl = NULL;
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__init_lkup_tbl() */

/****************************************************************************************
 * Function:    H5P__init_lkup_tbl_copy
 *
 *              Multithread version only function.
 *
 * Purpose:     Allocates and initializes an array of H5P_mt_list_table_entry_t structs
 *              for a property list field called lkup_tbl, that is a copy of another
 *              list's lkup_tbl.
 *
 *
 * Details:     First we allocate the new lkup_tbl with the number of entries that equal
 *              the number of properties the original list inherited from its parent.
 *
 *              Then we iterate through the list's lkup_tbl and set chksum and name
 *              fields. If the old_list's entry's (old_entry) most current version of the
 *              property is in it's curr.ptr, that property is checked to ensure it's
 *              valid. If it is valid a copy of that property is created and if it has
 *              a copy callback it is called, then it's inserted into the new list's
 *              LFSLL.
 *
 *              If the old list's curr.ptr property is not valid (i.e. it's been deleted)
 *              the new list's curr.ptr is set to NULL. There isn't a need to create a
 *              property structure that isn't valid for the new list.
 *
 *              NOTE: If curr.ptr is not NULL, regardless of if it's valid, base.ptr is
 *              set to NULL. No need to point to the parent's prop when it's not valid
 *              in this list. Additionally, the new list's curr.ver is set 1 and it's
 *              base.ver is set to 0.
 *
 *              If old_curr.ptr is NULL and old_curr.ver == 0, then the old list's
 *              entry's base_delete_version is checked and if valid, the parent class's
 *              property's ref_count is incremented, and the new list's base.ptr is set
 *              to point to it, with base.ver set to 1.
 *
 *              NOTE: old_curr.ver == 0 means that there was never a property curr.ptr
 *              pointed to. If this new list is a copy of a copy then there is a
 *              possibility that old_curr.ptr is NULL but old_curr.ver > 0. That means
 *              that if old_curr.ver is greater than 0, then both curr and base pointers
 *              are set to NULL, but curr.ver and first_ver_of_curr are set to 1.
 *
 *              If the old_base.ptr isn't NULL but is deleted we set base.ptr and
 *              curr.ptr to NULL, but set base.ver to 1 which shows that the last valid
 *              was the base.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__init_lkup_tbl_copy(H5P_mt_list_t *old_list, uint64_t version, H5P_mt_list_t *new_list)
{
    H5P_mt_list_table_entry_t *new_entry;         /* Entry in the new list's lkup_tbl */
    H5P_mt_list_table_entry_t *old_entry;         /* Old list's entry in the lkup_tbl */
    H5P_mt_list_prop_ref_t     new_base;          /* Base for the new list's entry */
    H5P_mt_list_prop_ref_t     old_base;          /* Base for the old list's entry */
    H5P_mt_list_prop_ref_t     new_curr;          /* Curr for the new list's entry */
    H5P_mt_list_prop_ref_t     old_curr;          /* Curr for the old list's entry */
    H5P_mt_prop_t             *old_prop;          /* Property from the old list */
    H5P_mt_prop_t             *new_prop;          /* Property for the new list */
    H5P_mt_prop_value_t        old_prop_value;    /* Value from an old list's prop */
    H5P_mt_class_t            *parent;            /* Parent class of the old list */
    uint64_t                   old_base_delete;   /* Old list's entry's base_delete_version */
    uint32_t                   nprops;            /* Number of props in the new list */
    uint32_t                   deletes       = 0; /* Tracks number of deletes */
    uint32_t                   nodes_visited = 0; /* Tracks number of nodes visited */
    uint32_t                   thrd_cols     = 0; /* Tracks number of thread cols */
    bool                       chksum_cols   = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__init_lkup_tbl_copy__num_calls), 1);

    assert(old_list);
    assert(old_list->tag == H5P_MT_LIST_TAG);

    /* Copy values from the old_list to the new copy */
    new_list->nprops_inherited = old_list->nprops_inherited;
    atomic_store(&(new_list->nprops_added), atomic_load(&(old_list->nprops_added)));
    atomic_store(&(new_list->nprops), atomic_load(&(old_list->nprops)));

    /* Allocates the number of entries needed in the lkup_tbl */
    if (old_list->nprops_inherited > 0) {
        new_list->lkup_tbl = (H5P_mt_list_table_entry_t *)malloc(new_list->nprops_inherited *
                                                                 sizeof(H5P_mt_list_table_entry_t));
        if (NULL == new_list->lkup_tbl) {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "lkup_tbl allocation failed");
        }
    }
    else {
        new_list->lkup_tbl = NULL;
    }

    /* Set up each lkup_tbl entry to copy the old_list's lkup_tbl */
    for (nprops = 0; nprops < new_list->nprops_inherited; nprops++) {
        new_entry = &new_list->lkup_tbl[nprops];
        old_entry = &old_list->lkup_tbl[nprops];

        new_entry->chksum = old_entry->chksum;
        new_entry->name   = strdup(old_entry->name);
        if (new_entry->name == NULL) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Failed to copy name buffer.");
        }

        old_curr = atomic_load(&(old_entry->curr));

        /* If not NULL, old_curr is the most recent version */
        if (old_curr.ptr) {
            old_prop       = old_curr.ptr;
            old_prop_value = atomic_load(&(old_prop->value));

            if ((0 == (atomic_load(&(old_prop->delete_version)))) ||
                (version < (atomic_load(&(old_prop->delete_version))))) {

                /* Create a new property that's a copy of the old_prop */
                new_prop = H5P__create_prop(old_prop->name, old_prop_value.ptr, old_prop_value.size, FALSE, 1,
                                            old_prop->create, old_prop->set, old_prop->get, old_prop->encode,
                                            old_prop->decode, old_prop->del, old_prop->copy, old_prop->cmp,
                                            old_prop->close);
                if (NULL == new_prop)
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "Failed creating property for property list.");

                /* If the new_prop has the copy callback, call it */
                if (new_prop->copy) {
#if 1
                    if (H5P__global_lock_prop_cb__copy(new_prop, new_prop->name, old_prop_value.size,
                                                       old_prop_value.ptr) < 0) {
                        assert(H5P_MT_ASSERT_FAIL);
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");
                    }
#else
                    if ((new_prop->copy)(new_prop->name, old_prop_value.size, old_prop_value.ptr) < 0) {
                        assert(H5P_MT_ASSERT_FAIL);
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");
                    }
#endif
                }

                /* Set the property's in_lkup_tbl flag */
                new_prop->in_lkup_tbl = TRUE;

                /* Inserts the new_prop into the new_list's LFSLL */
                H5P__mt_ins_or_mod_prop__lfsll_ins(new_list->pl_head, new_prop, &deletes, &nodes_visited,
                                                   &thrd_cols, &chksum_cols);

                atomic_fetch_add(&(new_list->log_pl_len), 1);
                atomic_fetch_add(&(new_list->phys_pl_len), 1);

                /* Finish setting up new_entry */
                new_curr.ptr = new_prop;
                new_curr.ver = 1;
                atomic_store(&(new_entry->curr), new_curr);
                atomic_store(&(new_entry->first_ver_of_curr), 1);
                if (atomic_load(&(old_entry->base_delete_version)) > 0) {
                    atomic_store(&(new_entry->base_delete_version), 1);
                }
                else {
                    atomic_store(&(new_entry->base_delete_version), 0);
                }
            }
            else {
                /* Create a new property that's a copy of the old_prop */
                new_prop = H5P__create_prop(old_prop->name, old_prop_value.ptr, old_prop_value.size, FALSE, 1,
                                            old_prop->create, old_prop->set, old_prop->get, old_prop->encode,
                                            old_prop->decode, old_prop->del, old_prop->copy, old_prop->cmp,
                                            old_prop->close);
                if (NULL == new_prop)
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "Failed creating property for property list.");

                /* Set the property's in_lkup_tbl flag */
                new_prop->in_lkup_tbl = TRUE;

                /* Inserts the new_prop into the new_list's LFSLL */
                H5P__mt_ins_or_mod_prop__lfsll_ins(new_list->pl_head, new_prop, &deletes, &nodes_visited,
                                                   &thrd_cols, &chksum_cols);

                /* Set the deleted version since the prop is deleted */
                atomic_store(&(new_prop->delete_version), 1);

                /* Increment new_list's physical length */
                atomic_fetch_add(&(new_list->phys_pl_len), 1);

                new_curr.ptr = new_prop;
                new_curr.ver = 1;
                atomic_store(&(new_entry->curr), new_curr);
                atomic_store(&(new_entry->first_ver_of_curr), 1);
                atomic_store(&(new_entry->base_delete_version), 1);
            }

            /**
             * NOTE: This verion of H5Pint_mt.c only copies the most current version
             * of the list, so if a curr.ptr exists we do not need to copy the base.
             * If this proves incorrect in testing, it will be changed.
             */
            new_base.ptr = NULL;
            new_base.ver = 0;
            atomic_store(&(new_entry->base), new_base);

        } /* end if ( old_curr.ptr ) */
        /**
         * If old_curr.ptr is NULL, then set up the base to point
         * to the parent class property
         */
        else if ((!old_curr.ptr) && (old_curr.ver == 0)) {
            old_base = atomic_load(&(old_entry->base));

            old_base_delete = atomic_load(&(old_entry->base_delete_version));

            /**
             * If old_base.ptr is not NULL and isn't deleted, atomically increment the
             * parent class's property's ref_count, and atomically set the new base to
             * point to the parent's property.
             *
             * NOTE: if the base is deleted we set the pointer to NULL, because we can't
             * access it anyway so it's pointless to go through the process. If this
             * proves incorrect in testing, it will be changed.
             */
            if (old_base.ptr && (0 == old_base_delete || version < old_base_delete)) {
                /* Atomically increment parent class's property ref_count */
                old_prop = old_base.ptr;

                atomic_fetch_add(&(old_prop->ref_count), 1);

                parent = old_list->pclass_ptr;

                /* success, update stats */
                atomic_fetch_add(&(parent->num_prop_ref_count_update), 1);

                /* Finish setting up new_base */
                new_base.ptr = old_base.ptr;
                new_base.ver = 1;
                atomic_store(&(new_entry->base), new_base);
                atomic_store(&(new_entry->base_delete_version), 0);
            }
            /* If old_base.ptr is NULL or base is deleted, finish setting up new_entry */
            else {
                new_base.ptr = NULL;
                new_base.ver = 1;
                atomic_store(&(new_entry->base), new_base);

                if (old_base_delete > 0 && old_base_delete <= version) {
                    atomic_store(&(new_entry->base_delete_version), 1);
                }
                else {
                    atomic_store(&(new_entry->base_delete_version), 0);
                }

                /* update stats */
                atomic_fetch_add(&(new_list->num_lkup_tbl_copy_entries_blank), 1);
            }

            /* Finish setting up new_entry */
            new_curr.ptr = NULL;
            new_curr.ver = 0;
            atomic_store(&(new_entry->curr), new_curr);

            atomic_store(&(new_entry->first_ver_of_curr), 0);

        }    /* end if () */
        else /* ( ! old_curr.ptr ) && ( old_curr.ver > 0 ) */
        {
            /**
             * NOTE: If this is the case then neither the base nor curr are valid,
             * and most likely this is a copy of copy, so set base.ptr and curr.ptr
             * to NULL, but set curr.ptr to 1 to ensure the base is left alone in
             * future operations.
             */
            new_base.ptr = NULL;
            new_base.ver = 0;
            atomic_store(&(new_entry->base), new_base);

            atomic_store(&(new_entry->base_delete_version), 0);

            new_curr.ptr = NULL;
            new_curr.ver = 1;
            atomic_store(&(new_entry->curr), new_curr);

            atomic_store(&(new_entry->first_ver_of_curr), 1);

        } /* end else */

    } /* end for ( nprops = 0; nprops < new_list->nprops_inherited; nprops++ ) */

done:

    /* Clean up if error occurred */
    if ((ret_value == FAIL) && new_list->lkup_tbl) {
        free(new_list->lkup_tbl);
        new_list->lkup_tbl = NULL;
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__init_lkup_tbl_copy() */

/****************************************************************************************
 * Function:    H5P__create_sentinels
 *
 *              Multithread version only function.
 *
 * Purpose:     Creates the two sentinel nodes for a class's or list's LFSLL.
 *
 *              Negative sentinel will always be the first node in a LFSLL and the
 *              positive sentinel will always be the last node in a LFSLL, with every
 *              other node inserted between them.
 *
 * Return:      Success: Returns a pointer to 'neg_sentinel' (H5P_mt_prop_t)
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__create_sentinels(bool in_prop_class)
{
    H5P_mt_prop_t      *pos_sentinel = NULL; /* Positive sentinel for LFSLL */
    H5P_mt_prop_aptr_t  pos_next;            /* next struct for positive sentinel */
    H5P_mt_prop_value_t pos_value;           /* value struct for positive sentinel */
    H5P_mt_prop_t      *neg_sentinel = NULL; /* Negitive sentinel for LFSLL */
    H5P_mt_prop_aptr_t  neg_next;            /* next struct for negative sentinel */
    H5P_mt_prop_value_t neg_value;           /* value struct for negative sentinel */

    H5P_mt_prop_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    /* Allocating and initializing the positive sentinel node of the LFSLL */
    pos_sentinel = H5P__mt_alloc_prop();
    if (pos_sentinel == NULL) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "Positive sentinel allocation failed.");
    }

    /* Initalize property fields */
    atomic_store(&(pos_sentinel->tag), H5P_MT_PROP_TAG);

    pos_next.ptr          = NULL;
    pos_next.deleted      = FALSE;
    pos_next.dummy_bool_1 = FALSE;
    pos_next.dummy_bool_2 = FALSE;
    pos_next.dummy_bool_3 = FALSE;
    atomic_store(&(pos_sentinel->next), pos_next);

    pos_sentinel->sentinel      = TRUE;
    pos_sentinel->in_prop_class = in_prop_class;

    /* ref_count is only used if the property is in a property class. */
    atomic_store(&(pos_sentinel->ref_count), 0);

    pos_sentinel->in_lkup_tbl = FALSE;

    pos_sentinel->chksum = LLONG_MAX;

    pos_sentinel->name = strdup("pos_sentinel");
    if (pos_sentinel->name == NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, NULL, "Failed to copy name buffer.");
    }

    pos_value.ptr  = NULL;
    pos_value.size = 0;

    atomic_store(&(pos_sentinel->value), pos_value);

    atomic_store(&(pos_sentinel->create_version), 1);
    atomic_store(&(pos_sentinel->delete_version), 0);

    pos_sentinel->callbacks_mt_safe = FALSE;
    pos_sentinel->create            = NULL;
    pos_sentinel->set               = NULL;
    pos_sentinel->get               = NULL;
    pos_sentinel->encode            = NULL;
    pos_sentinel->decode            = NULL;
    pos_sentinel->del               = NULL;
    pos_sentinel->copy              = NULL;
    pos_sentinel->cmp               = NULL;
    pos_sentinel->close             = NULL;

    /* Allocating and initializing the negative sentinel node of the LFSLL */
    neg_sentinel = H5P__mt_alloc_prop();
    if (neg_sentinel == NULL) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "Negative sentinel allocation failed.");
    }

    /* Initalize property fields */
    atomic_store(&(neg_sentinel->tag), H5P_MT_PROP_TAG);

    neg_next.ptr          = pos_sentinel;
    neg_next.deleted      = FALSE;
    neg_next.dummy_bool_1 = FALSE;
    neg_next.dummy_bool_2 = FALSE;
    neg_next.dummy_bool_3 = FALSE;
    atomic_store(&(neg_sentinel->next), neg_next);

    neg_sentinel->sentinel      = TRUE;
    neg_sentinel->in_prop_class = in_prop_class;

    /* ref_count is only used if the property is in a property class. */
    atomic_store(&(neg_sentinel->ref_count), 0);

    neg_sentinel->chksum = LLONG_MIN;

    neg_sentinel->name = strdup("neg_sentinel");
    if (neg_sentinel->name == NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, NULL, "Failed to copy name buffer.");
    }

    neg_value.ptr  = NULL;
    neg_value.size = 0;

    atomic_store(&(neg_sentinel->value), neg_value);

    atomic_store(&(neg_sentinel->create_version), 1);
    atomic_store(&(neg_sentinel->delete_version), 0); /* Set to 0 because it's not deleted */

    neg_sentinel->callbacks_mt_safe = FALSE;
    neg_sentinel->create            = NULL;
    neg_sentinel->set               = NULL;
    neg_sentinel->get               = NULL;
    neg_sentinel->encode            = NULL;
    neg_sentinel->decode            = NULL;
    neg_sentinel->del               = NULL;
    neg_sentinel->copy              = NULL;
    neg_sentinel->cmp               = NULL;
    neg_sentinel->close             = NULL;

    ret_value = neg_sentinel;

done:

    /* Clean up if an error occurred */
    if ((ret_value == NULL) && neg_sentinel) {
        if (neg_sentinel) {
            free(neg_sentinel);
        }
        if (pos_sentinel) {
            free(pos_sentinel);
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__create_setinels() */

/****************************************************************************************
 * Function:    H5P__create_prop
 *
 *              Multithread version of H5P__create_prop().
 *
 * Purpose:     Creates a new property (H5P_mt_prop_t)
 *
 *              This function allocates and initializes a new property structure
 *              (H5P_mt_prop_t) to be used when a new property is added to a class's or
 *              list's LFSLL.
 *
 *              It is also used when a modification is done to an existing property.
 *              The way the versioning system works, a modification to a property
 *              requires a new property struct for the modification and has its
 *              create_version set to the new version of the class or list to represent
 *              when this modification occured and when it's is valid.
 *
 *              NOTE: For more information on the parameters of this function see the
 *              description comment for the H5P_mt_prop_t stucture in H5Ppkg_mt_.h
 *
 *
 * Return:      Success: Returns a pointer to the new H5P_mt_prop_t (property struct)
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__create_prop(const char *name, const void *value_ptr, size_t value_size, bool in_prop_class,
                 uint64_t version, H5P_prp_create_func_t prp_create, H5P_prp_set_func_t prp_set,
                 H5P_prp_get_func_t prp_get, H5P_prp_encode_func_t prp_encode,
                 H5P_prp_decode_func_t prp_decode, H5P_prp_delete_func_t prp_del,
                 H5P_prp_copy_func_t prp_copy, H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close)
{
    H5P_mt_prop_t      *new_prop; /* The new property to be created */
    H5P_mt_prop_aptr_t  next;     /* The new property's next field */
    H5P_mt_prop_value_t value;    /* The new property's value field */

    H5P_mt_prop_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__create_prop__num_calls), 1);

    assert(name);
    assert(value_size > 0 || value_ptr == NULL);

    /* Allocate memory for the new property */
    new_prop = H5P__mt_alloc_prop();

    if (NULL == new_prop) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "property allocation failed");
    }

    /* Initalize property fields */
    atomic_store(&(new_prop->tag), H5P_MT_PROP_TAG);

    next.ptr          = NULL;
    next.deleted      = FALSE;
    next.dummy_bool_1 = FALSE;
    next.dummy_bool_2 = FALSE;
    next.dummy_bool_3 = FALSE;
    atomic_store(&(new_prop->next), next);

    new_prop->sentinel      = FALSE;
    new_prop->in_prop_class = in_prop_class;

    atomic_store(&(new_prop->ref_count), 0);

    new_prop->in_lkup_tbl = FALSE;

    new_prop->chksum = H5_checksum_metadata(name, strlen(name), 0);

    assert(new_prop->chksum > LLONG_MIN && new_prop->chksum < LLONG_MAX);

    new_prop->name = strdup(name);
    if (new_prop->name == NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, NULL, "Failed to copy name buffer.");
    }

    value.ptr = malloc(value_size);
    if (NULL == value.ptr) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "Value buffer allocation failed.");
    }

    memcpy(value.ptr, value_ptr, value_size);

    value.size = value_size;
    atomic_store(&(new_prop->value), value);

    atomic_store(&(new_prop->create_version), version);
    atomic_store(&(new_prop->delete_version), 0);

    /* Initialize the Callbacks */

    /**
     * NOTE: at the current iteration callbacks_mt_safe is always set to FALSE.
     * Further testing is needed to find out which callbacks, if any, are
     * multithread safe.
     */
    new_prop->callbacks_mt_safe = FALSE;

    new_prop->create = prp_create;
    new_prop->set    = prp_set;
    new_prop->get    = prp_get;
    new_prop->encode = prp_encode;
    new_prop->decode = prp_decode;
    new_prop->del    = prp_del;
    new_prop->copy   = prp_copy;

    /* Use custom comparison routine if available, otherwise default to memcmp() */
    if (prp_cmp) {
        new_prop->cmp = prp_cmp;
    }
    else {
        new_prop->cmp = &memcmp;
    }

    new_prop->close = prp_close;

    if (prp_create == NULL && prp_set == NULL && prp_get == NULL && prp_encode == NULL &&
        prp_decode == NULL && prp_del == NULL && prp_copy == NULL) {
        /* update stats */
        atomic_fetch_add(&(H5P_mt_g.num_props_created_wo_cbs), 1);
    }

    ret_value = new_prop;

done:

    /* Clean up if an error occurred */
    if ((ret_value == NULL) && (new_prop)) {
        free(new_prop);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__create_prop() */

/****************************************************************************************
 * Function:    H5P__mt_alloc_prop
 *
 *              Multithread version only function.
 *
 * Purpose:     Gets a pointer to a valid H5P_mt_prop_t struct by either reallocating
 *              one from the free list if one is reallocable, or by allocating a new one
 *              from memory.
 *
 *              NOTE: In this iteration no struct on the free list can be reallocated.
 *
 * Return:      Success: Returns a pointer to a H5P_mt_prop_t
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__mt_alloc_prop(void)
{
    H5P_mt_prop_aptr_t fl_head; /* First node of the prop free list */
    H5P_mt_prop_aptr_t fl_tail;
    H5P_mt_prop_aptr_t fl_next;          /* Next node of the prop free list */
    H5P_mt_prop_t     *head_prop;        /* Property fl_head points to */
    H5P_mt_prop_t     *new_prop = NULL;  /* New property to be allocated */
    bool               done     = FALSE; /* Flag to exit a loop when setting atomics */

    H5P_mt_prop_t *ret_value = NULL;

    FUNC_ENTER_NOAPI(FAIL)

    /* Get head of the free list */
    fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));
    fl_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

    head_prop = fl_head.ptr;

    /* If no prop structs are reallocable, alloc from memory */
    if (atomic_load(&(head_prop->tag)) != H5P_MT_PROP_FL_REALLOC_TAG || (fl_head.ptr == fl_tail.ptr)) {
        new_prop = (H5P_mt_prop_t *)malloc(sizeof(H5P_mt_prop_t));

        if (NULL == new_prop) {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "property allocation failed");
        }

        /* update stats */
        atomic_fetch_add(&(H5P_mt_g.num_prop_structs_allocated_from_heap), 1);
    }
    /* If a struct on the free list is reallocable, clear it and return it */
    else {
        /* Remove the struct from the free list */
        do {
            fl_next = atomic_load(&(head_prop->next));

            fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));

            if (!atomic_compare_exchange_strong(&(H5P_mt_g.prop_fl_head), &fl_head, fl_next)) {
                /* failed, update stats and try again */
                atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update_cols), 1);

                /* assert is to not get stuck in an infinite loop while testing */
                assert(H5P_MT_ASSERT_FAIL);
            }
            else {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update), 1);
                atomic_fetch_sub(&(H5P_mt_g.prop_fl_len), 1);

                done = TRUE;
            }

        } while (!done);

        /* Ensure the struct is cleared of any previous data */
        new_prop = H5P__clear_mt_prop(fl_head.ptr);

        /* update stats */
        atomic_fetch_add(&(H5P_mt_g.num_prop_structs_allocated_from_fl), 1);

    } /* end else */

done:

    ret_value = new_prop;

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_alloc_prop() */

/****************************************************************************************
 * Function:    H5P__mt_copy_lfsll
 *
 * Purpose:     Given a list or class, the head property of an existing list's or
 *              class's LFSLL, and a version, this function copies the valid properties
 *              from one list to another list, or from one class to another class.
 *
 *              Two things make a property invalid:
 *                  1) any property with a create_version greater than the parent class's
 *                     version from which the new class or list is derived.
 *                  2) any property with a delete_version less than or equal to the
 *                     parent class's version from which the new class or list is derived
 *
 *              When iterating the original LFSLL and the next valid property is found,
 *              the bool in_lkup_tbl is checked and if TRUE we are done and can get the
 *              next valid property, because this property would have been copied into
 *              the list's lfsll when the lkup_tbl was copied. Class's don't have to
 *              worry about this, for them in_lkup_tbl will always be FALSE. If
 *              in_lkup_tbl is FALSE, a new property is created using the valid
 *              property's name, value (H5P_mt_prop_value_t), in_prop_class, and
 *              callback function fields. That new property is then inserted into the new
 *              class's LFSLL, with it's create_version at 1 to show that this property
 *              was added during the class's or list's creation.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_copy_lfsll(void *param, H5P_mt_prop_t *old_prop, uint64_t version)
{
    uint32_t            tag;               /* Determines if param is a list or class */
    H5P_mt_class_t     *new_class = NULL;  /* Used if param is a class */
    H5P_mt_list_t      *new_list  = NULL;  /* Used if param is a list */
    H5P_mt_prop_t      *new_plhead;        /* head of the new list's or class's LFSLL */
    H5P_mt_prop_t      *valid_prop;        /* The next valid prop to copy over */
    H5P_mt_prop_t      *new_prop;          /* New prop to store in the new LFSLL */
    H5P_mt_prop_value_t value;             /* Value of a property */
    uint32_t            phys_pl_len   = 0; /* Physicaly length of new LFSLL */
    uint32_t            log_pl_len    = 0; /* Logicaly length of new LFSLL */
    uint32_t            deletes       = 0; /* Tracks number of deletes */
    uint32_t            nodes_visited = 0; /* Tracks number of nodes visited */
    uint32_t            thrd_cols     = 0; /* Tracks number of thread cols */
    bool                chksum_cols   = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    /* Determine if this is a list or class */

    tag = *(uint32_t *)param;

    /* If param is a class */
    if (tag == H5P_MT_CLASS_TAG) {
        new_class = (H5P_mt_class_t *)param;

        assert(new_class);
        assert(atomic_load(&(new_class->tag)) == H5P_MT_CLASS_TAG);

        /* Get head of the LFSLL */
        new_plhead = new_class->pl_head;
    }
    else if (tag == H5P_MT_LIST_TAG)
    /* If param is a list */
    {
        new_list = (H5P_mt_list_t *)param;

        assert(new_list);
        assert(new_list->tag == H5P_MT_LIST_TAG);

        /* Get head of the LFSLL */
        new_plhead = new_list->pl_head;
    }
    else {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Type passed in wasn't a class or list.");
    }

    assert(old_prop);
    assert(old_prop->tag == H5P_MT_PROP_TAG);
    assert(old_prop->sentinel);

    assert(new_plhead);
    assert(new_plhead->tag == H5P_MT_PROP_TAG);
    assert(new_plhead->sentinel);

    do {
        /* Gets the next valid prop or NULL if there isn't another valid */
        valid_prop = H5P__get_next_valid_prop(old_prop, version, NULL);

        if (valid_prop) {
            /* If in_lkup_tbl is TRUE the prop was already added */
            if (!valid_prop->in_lkup_tbl) {
                value = atomic_load(&(valid_prop->value));

                /* Creates a new property from the parent's valid_prop */
                new_prop = H5P__create_prop(
                    valid_prop->name, value.ptr, value.size, valid_prop->in_prop_class, 1, valid_prop->create,
                    valid_prop->set, valid_prop->get, valid_prop->encode, valid_prop->decode, valid_prop->del,
                    valid_prop->copy, valid_prop->cmp, valid_prop->close);
                if (NULL == new_prop) {
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL,
                                "Failed creating property for property list class.");
                }

                /* If working on a list's LFSLL */
                if (new_list) {
                    /* If the prop has a copy callback, call it */
                    if (new_prop->copy) {
#if 1
                        if (H5P__global_lock_prop_cb__copy(new_prop, new_prop->name, value.size, value.ptr) <
                            0) {
                            assert(H5P_MT_ASSERT_FAIL);
                            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");
                        }
#else
                        if ((new_prop->copy)(new_prop->name, value.size, value.ptr) < 0) {
                            assert(H5P_MT_ASSERT_FAIL);
                            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");
                        }
#endif
                    }
                }

                /* Inserts the new_prop into the new_class's LFSLL */
                H5P__mt_ins_or_mod_prop__lfsll_ins(new_plhead, new_prop, &deletes, &nodes_visited, &thrd_cols,
                                                   &chksum_cols);

                /* Increment physical and logical lengths */
                phys_pl_len++;
                log_pl_len++;

            } /* end if ( ! done ) */

            /* Iterate the old LFSLL */
            old_prop = valid_prop;

        } /* end if ( valid_prop ) */

    } while (valid_prop);

    if (new_class) {
        /* update physical and logical lengths */
        atomic_fetch_add(&(new_class->phys_pl_len), phys_pl_len);
        atomic_fetch_add(&(new_class->log_pl_len), log_pl_len);
    }
    else {
        /* update physical and logical lengths */
        atomic_fetch_add(&(new_list->phys_pl_len), phys_pl_len);
        atomic_fetch_add(&(new_list->log_pl_len), log_pl_len);
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_copy_lfsll() */

/****************************************************************************************
 * Function:    H5P__register_real
 *
 *              Multithread safe version of H5P__register_real().
 *
 * Purpose:     Internal routine to register a new property in a property list class
 *              during H5P initialization.
 *
 *              This function has had a pretty significant change from the original to
 *              the multithread safe version.
 *
 *              The original version was called by H5P__register() to actually insert the
 *              new property into the class after potentially creating a copy of the
 *              class (see making_H5P_multi-thread_safe_sketch_design for details on that
 *              process). The multithread safe H5P structures removed the need to do
 *              that, instead going with a structure versioning system (more details in
 *              the document mentioned, or in H5Ppkg_mt.h).
 *
 *              Because of that H5P__register_real() was repurposed to instead insert new
 *              properties into a LFSLL (specifically only used on a class's LFSLL),
 *              during H5P initialization. This allows a class's default properties to be
 *              inserted without incrementing the version of the class or incrementing
 *              other unnecessary class or global statistics for testing and debugging
 *              that inserting default properties during initialization aren't needed to
 *              be counted towards.
 *
 *              NOTE: In the future the name of H5P__register_real() may be changed to
 *              more accurately depict its purpose, but to fit existing tests without
 *              altering too much, the name is kept the same.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__register_real(H5P_mt_class_t *pclass, const char *name, size_t size, const void *def_value,
                   H5P_prp_create_func_t prp_create, H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get,
                   H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode,
                   H5P_prp_delete_func_t prp_delete, H5P_prp_copy_func_t prp_copy,
                   H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close)
{
    H5P_mt_prop_t *pl_head;
    H5P_mt_prop_t *new_prop;
    uint32_t       deletes     = 0;
    uint32_t       visited     = 0;
    uint32_t       thrd_cols   = 0;
    bool           chksum_cols = FALSE;

    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    atomic_fetch_add(&(pclass->H5P__register_real__num_calls), 1);

    /**
     * Create the new MT property structure
     *
     * NOTE: the version of the property is always 1, because this
     * function should only be used during initialization of H5P.
     */
    if (NULL ==
        (new_prop = H5P__create_prop(name, def_value, size, TRUE, 1, prp_create, prp_set, prp_get, prp_encode,
                                     prp_decode, prp_delete, prp_copy, prp_cmp, prp_close))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, "Can't create property");
    }

    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    pl_head = pclass->pl_head;

    assert(pl_head);
    assert(atomic_load(&(pl_head->tag)) == H5P_MT_PROP_TAG);

    /* Insert the new property into the class without incrementing version number */
    if ((ret_value = H5P__mt_ins_or_mod_prop__lfsll_ins(pl_head, new_prop, &deletes, &visited, &thrd_cols,
                                                        &chksum_cols)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL, "Can't insert property into class");
    }

    atomic_fetch_add(&(pclass->phys_pl_len), 1);
    atomic_fetch_add(&(pclass->log_pl_len), 1);

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__register_real() MT safe version*/

/****************************************************************************************
 * Function:    H5P__register
 *
 *              Multithread safe version of H5P__register().
 *
 * Purpose:     Internal routine to register a new property in a property list class at
 *              the next_version of the class and increments the current version of the
 *              class to that next version upon completion.
 *
 *              This function has had a significant change from the original to the
 *              multithread safe version.
 *
 *              The original version of this function potentially created a copy of the
 *              class originally intended to have the new property inserted into, in the
 *              case where the original class had existing derived classes or lists. Then
 *              call H5P__register_real() to actually insert the new property into the
 *              new copy of the class. The updated multithread H5P structures removed the
 *              need to do this, instead using an in-structure versioning system that
 *              only allows derived list to access properties valid at the version they
 *              were derived from, and derived clases create their own copies of the
 *              valid properties at the version they were derived from (see
 *              making_H5P_multi-thread_safe_sketch_design or H5Ppkg_mt.h for more
 *              details).
 *
 *              Because of these changes H5P__register() and H5P__register_real() were
 *              altered to fit these new methods. H5P__register() now handles the process
 *              of creating the new property and inserting it into the target class
 *              during normal library operations (of course still calling other functions
 *              for more their more specific tasks). While H5P__register_real() is used
 *              during initialization of H5P to remove some of the process that isn't
 *              necessary until the library (or at least H5P) is fully initialized.
 *
 * Details:     First check the class's thrd.opening and thrd.closing flags and
 *              increments the thrd.count of the class by calling H5P__inc_thrd_count().
 *              Then calls H5P__mt_enforce_serialization() to check if this thread is
 *              allowed to continue, or if there are other threads actively modifying the
 *              class structure (see the above mentioned document for more details). If
 *              there are already threads modifying the structure, this thread waits
 *              until its turn before searching the class's LFSLL to ensure the property
 *              doesn't already exist in the class.
 *
 *              NOTE: The current operation only searches the class for the property
 *              after H5P__mt_enforce_serialization(), because if done prior, the result
 *              can change. Two examples are if the property doesn't exist in the class
 *              prior, but another inserts it while this class is waiting for that thread
 *              to finish. Or if the property does exist in the class, but another thread
 *              is in the process of deleting it so it wouldn't by the time this thread
 *              can actually modify the class. Thus, this thread still has to search the
 *              the class after H5P__mt_enforce_serialization() completes, so searching
 *              the class is only done after.
 *
 *              H5P__mt_ins_or_mod_prop__class() is called to create the property
 *              structure, insert it into the class's LFSLL, and correctly adjust other
 *              of the class's fields and stats as necessary for this specific instance.
 *
 *              NOTE: Zero-sized properties are allowed and do not store any data in the
 *              property list.  These may be used as flags to indicate the presence or
 *              absence of a particular piece of information.  The 'default' pointer for
 *              a zero-sized property may be set to NULL. The property 'create' & 'close'
 *              callbacks are called for zero-sized properties, but the 'set' and 'get'
 *              callbacks are never called.
 *
 *              Lastly, update the class's current version to the next_version this
 *              thread got upon entering the class, and decrement the class's thrd.count.
 *
 * Parameters:  H5P_mt_class_t **ppclass:   Pointer to the property class to modify.
 *              const cont *name:           Name of the property to register.
 *              size_t size:                Size of the property's value in bytes.
 *              void *def_value:            Pointer to a buffer containing the default
 *                                          property value.
 *              H5P_prp_create_func_t:      Function pointer to property create callback.
 *              H5P_prp_set_func_t:         Function pointer to property set callback.
 *              H5P_prp_get_func_t:         Function pointer to property get callback.
 *              H5P_prp_encode_func_t       Function pointer to property encode callback.
 *              H5P_prp_decode_func_t       Function pointer to property decode callback.
 *              H5P_prp_delete_func_t       Function pointer to property delete callback.
 *              H5P_prp_copy_func_t         Function pointer to property copy callback.
 *              H5P_prp_compare_func_t      Function pointer to property compare callback
 *              H5P_prp_close_func_t        Function pointer to property close callback.
 *
 *              NOTE: for more details on the property callbacks see the detailed
 *              structure description for H5P_mt_prop_t in H5Ppkg_mt.h.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__register(H5P_mt_class_t **ppclass, const char *name, size_t size, void *def_value,
              H5P_prp_create_func_t prp_create, H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get,
              H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode,
              H5P_prp_delete_func_t prp_delete, H5P_prp_copy_func_t prp_copy, H5P_prp_compare_func_t prp_cmp,
              H5P_prp_close_func_t prp_close)
{
    H5P_mt_class_t *class;
    H5P_mt_prop_t *new_prop      = NULL;  /* New prop to be created and inserted */
    uint64_t       curr_version  = 0;     /* Current version of the class */
    uint64_t       next_version  = 0;     /* Next version of the class */
    bool           inc_thrd_flag = FALSE; /* Flag to dec class's thrd count */
    bool           ver_updated   = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    class = *ppclass;

    assert(class);
    assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);
    assert(name);
    assert((size > 0 && def_value != NULL) || (size == 0));

    /* update stats */
    atomic_fetch_add(&(class->H5P__register__num_calls), 1);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(class)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment class's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    curr_version = atomic_load(&(class->curr_version));
    next_version = atomic_fetch_add(&(class->next_version), 1);
    ver_updated  = TRUE;

    /* Ensure another thread isn't modifying the class */
    if ((curr_version + 1) < next_version) {
        if (0 == (curr_version = H5P__mt_enforce_serialization(class, curr_version, next_version))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }

        curr_version = atomic_load(&(class->curr_version));
    }

    assert(curr_version + 1 == next_version);

    /**
     * Callback function for testing, to pass the exact version
     * of the class the new property is being inserted at without
     * having to search the LFSLL try to find the property and check.
     */
    if (H5P_mt_cb.ver_cb) {
        (H5P_mt_cb.ver_cb)(next_version);
    }
    /**
     * end testing function
     */

    /* Ensure the property doesn't already exist in the class */
    new_prop = H5P__mt_search__class(class, name, curr_version);
    if (new_prop) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Property already exists in the class.");
    }

    /* Create and insert the property into the class */
    if ((ret_value = H5P__mt_ins_or_mod_prop__class(class, name, def_value, size, TRUE, next_version,
                                                    prp_create, prp_set, prp_get, prp_encode, prp_decode,
                                                    prp_delete, prp_copy, prp_cmp, prp_close)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to register property in class");
    }

done:

    /* Update class's curr_version */
    if (ver_updated) {
        /* Update the class's current version */
        curr_version = atomic_fetch_add(&(class->curr_version), 1);
        assert(curr_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(class->curr_version)) > atomic_load(&(H5P_mt_g.max_class_version_number))) {
            atomic_store(&(H5P_mt_g.max_class_version_number), atomic_load(&(class->curr_version)));
        }
    }

    /* If the class's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(class)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__register() MT safe version */

/****************************************************************************************
 * Function:    H5P__mt_ins_or_mod_prop__class
 *
 *              Multithread safe version only function
 *
 * Purpose:     Creates a new property structure (H5P_mt_prop_t), either for a new
 *              property or for a new version of an existing property, and inserts it
 *              into the LFSLL of the provided property class (H5P_mt_class_t).
 *
 *              NOTE: When 'modifying' a property is this multithread safe version of
 *              H5P, a new property struct must be created with a new create_version.
 *
 *              H5P__create_prop() is called to allocate and initialize the new
 *              property struct.
 *
 *              H5P__mt_ins_or_mod_prop__lfsll_ins() is called to insert the new property
 *              into the class's LFSLL.
 *
 *              If this is a new property, and not a new version of a property that
 *              already exists in the class, increment increment logical length of the
 *              lfsll and nprops_added.
 *
 *              Increment phys_pl_len.
 *
 *              Any necessary stats fields are then updated.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_ins_or_mod_prop__class(H5P_mt_class_t *class, const char *name, void *value, size_t size, bool is_new,
                               uint64_t prop_version, H5P_prp_create_func_t prp_create,
                               H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get,
                               H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode,
                               H5P_prp_delete_func_t prp_del, H5P_prp_copy_func_t prp_copy,
                               H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close)
{
    H5P_mt_prop_t *new_prop = NULL; /* New prop to be created and inserted */
    H5P_mt_prop_t *pl_head;         /* Head of the LFSLL of the class */
    // H5P_mt_prop_t     *next_prop;       /* Next prop in LFSLL after the new prop */
    // H5P_mt_prop_aptr_t next;            /* New prop's next struct field */
    // uint64_t           delete_version = 0;
    uint32_t deletes     = 0; /* Tracks number of deletes */
    uint32_t visited     = 0; /* Tracks number of nodes visited */
    uint32_t thrd_cols   = 0; /* Tracks number of thread collisions */
    uint64_t avg_visited = 0; /* Stats variable */
    uint64_t num_calls   = 0; /* Stats variable */
    bool     chksum_cols = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(class->H5P__mt_ins_or_mod_prop__class__num_calls), 1);

    assert(class);
    assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);
    assert(name);
    assert((size > 0 && value != NULL) || (size == 0));

    /* This thread can now proceed and create the new property */
    new_prop = H5P__create_prop(name, value, size, TRUE, prop_version, prp_create, prp_set, prp_get,
                                prp_encode, prp_decode, prp_del, prp_copy, prp_cmp, prp_close);
    if (new_prop == NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, "Failed to create new property.");
    }

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    pl_head = class->pl_head;

    assert(pl_head);
    assert(atomic_load(&(pl_head->tag)) == H5P_MT_PROP_TAG);
    assert(pl_head->sentinel);

    /* Insert the property into the LFSLL */
    H5P__mt_ins_or_mod_prop__lfsll_ins(pl_head, new_prop, &deletes, &visited, &thrd_cols, &chksum_cols);

    if (is_new) {
        atomic_fetch_add(&(class->log_pl_len), 1);
        atomic_fetch_add(&(class->nprops_added), 1);
    }

    /* Increment physical length of the lfsll */
    atomic_fetch_add(&(class->phys_pl_len), 1);

    /* update stats */
    atomic_store(&(class->num_insert_nodes_visited), visited);

    if (chksum_cols) {
        atomic_fetch_add(&(class->num_insert_prop__chksum_cols), 1);
    }

    if (visited > atomic_load(&(class->insert_max_nodes_visited))) {
        atomic_store(&(class->insert_max_nodes_visited), visited);
    }

    avg_visited = atomic_load(&(class->insert_avg_nodes_visited));
    num_calls   = atomic_load(&(class->H5P__mt_ins_or_mod_prop__class__num_calls));

    avg_visited = H5P__calc_avg_visited(avg_visited, num_calls, visited);

    atomic_store(&(class->insert_avg_nodes_visited), avg_visited);

    atomic_fetch_add(&(class->num_insert_prop__cols), thrd_cols);
    atomic_fetch_add(&(class->num_insert_prop__success), 1);
    atomic_fetch_add(&(H5P_mt_g.num_props_inserted_classes), 1);

    if (atomic_load(&(class->phys_pl_len)) > atomic_load(&(H5P_mt_g.max_class_num_phys_props))) {
        atomic_store(&(H5P_mt_g.max_class_num_phys_props), atomic_load(&(class->phys_pl_len)));
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_ins_or_mod_prop__class() */

/****************************************************************************************
 * Function:    H5P_insert
 *
 *              Multithread safe version of H5P_insert().
 *
 * Purpose:     Internal routine to insert a new property in a property list at the
 *              list's next_version and increments the current version of the list to
 *              that next_version upon completion.
 *
 * Details:     First check the list's thrd.opening and thrd.closing flags and
 *              increments the thrd.count of the list by calling H5P__inc_thrd_count().
 *              Then calls H5P__mt_enforce_serialization() to check if this thread is
 *              allowed to continue, or if there are other threads actively modifying the
 *              list structure. If there are already threads modifying the structure,
 *              this thread waits until its turn before searching the list to ensure the
 *              property doesn't already exist in the list.
 *
 *              H5P__mt_ins_or_mod_prop__list() is called to create the property
 *              structure, insert it into the list's LFSLL, call the appropriate callback
 *              if necessary, and correctly adjust other of the list's fields and stats
 *              as necessary for this specific instance.
 *
 *              NOTE: Zero-sized properties are allowed and do not store any data in the
 *              property list.  These may be used as flags to indicate the presence or
 *              absence of a particular piece of information.  The 'default' pointer for
 *              a zero-sized property may be set to NULL. The property 'create' & 'close'
 *              callbacks are called for zero-sized properties, but the 'set' and 'get'
 *              callbacks are never called.
 *
 *              Lastly, update the list's current version to the next_version this
 *              thread got upon entering the list, and decrement the list's thrd.count.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P_insert(H5P_mt_list_t *list, const char *name, size_t size, void *value, H5P_prp_set_func_t prp_set,
           H5P_prp_get_func_t prp_get, H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode,
           H5P_prp_delete_func_t prp_delete, H5P_prp_copy_func_t prp_copy, H5P_prp_compare_func_t prp_cmp,
           H5P_prp_close_func_t prp_close)
{
    H5P_mt_prop_t *new_prop      = NULL;  /* New prop to be created and inserted */
    uint64_t       curr_version  = 0;     /* Current version of list or class */
    uint64_t       next_version  = 0;     /* Next version of list or class */
    bool           inc_thrd_flag = FALSE; /* Flag to dec parent's thrd count */
    bool           ver_updated   = FALSE;

    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI_NOINIT

    assert(list);
    assert(name);
    assert((size > 0 && value != NULL) || (size == 0));

    /* update stats */
    atomic_fetch_add(&(list->H5P_insert__num_calls), 1);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(list)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment list's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    curr_version = atomic_load(&(list->curr_version));
    next_version = atomic_fetch_add(&(list->next_version), 1);
    ver_updated  = TRUE;

    /* Ensure another thread isn't modifying the class */
    if ((curr_version + 1) < next_version) {
        if (0 == (curr_version = H5P__mt_enforce_serialization(list, curr_version, next_version))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }

        curr_version = atomic_load(&(list->curr_version));
    }

    assert(curr_version + 1 == next_version);

    /**
     * Callback function for testing, to pass the exact version
     * of the list the new property is being inserted at without
     * having to search the LFSLL try to find the property and check.
     */
    if (H5P_mt_cb.ver_cb) {
        (H5P_mt_cb.ver_cb)(next_version);
    }
    /**
     * end testing function
     */

    /* Ensure the property doesn't already exist in the list */
    new_prop = H5P__mt_search__list(list, name, curr_version);
    if (new_prop) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Property already exists in the list.");
    }

    /* Create the new property and insert it into the property list */
    if ((ret_value = H5P__mt_ins_or_mod_prop__list(list, name, value, size, FALSE, FALSE, TRUE, next_version,
                                                   NULL, prp_set, prp_get, prp_encode, prp_decode, prp_delete,
                                                   prp_copy, prp_cmp, prp_close)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to register MT property in MT plist");
    }

done:

    /* Update list's curr_version */
    if (ver_updated) {
        /* Update the list's current version */
        curr_version = atomic_fetch_add(&(list->curr_version), 1);
        assert(curr_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(list->curr_version)) > atomic_load(&(H5P_mt_g.max_list_version_number))) {
            atomic_store(&(H5P_mt_g.max_list_version_number), atomic_load(&(list->curr_version)));
        }

        /* Update global atomic plist version if list is a default plist */
        if (list->def_ver_ptr) {
            atomic_fetch_add((list->def_ver_ptr), 1);

            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_default_list_mods), 1);
        }
    }

    /* If the parent's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(list)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_insert() MT safe version */

/****************************************************************************************
 * Function:    H5P__mt_ins_or_mod_prop__list
 *
 *              Multithread safe only function
 *
 * Purpose:     Creates a new property structure (H5P_mt_prop_t), either for a new
 *              property or for a new version of an existing property, and inserts it
 *              into the LFSLL of the provided property list (H5P_mt_list_t).
 *
 *              NOTE: When 'modifying' a property is this multithread safe version of
 *              H5P, a new property struct must be created with a new create_version.
 *
 *              H5P__create_prop() is called to allocate and initialize the new
 *              property struct.
 *
 *              If the copy flag parameter or the create flag parameter is TRUE the
 *              property is checked for a copy or create callback respectively, and if
 *              it exists it is called.
 *
 *              H5P__mt_ins_or_mod_prop__lfsll_ins() is called to insert the new property
 *              into the lists's LFSLL.
 *
 *              The list's lkup_tbl is searched and if there is an entry for the new
 *              property, that entry is updated so curr.ptr points to the new property
 *              and curr.ver is set to the new property's create_version. Additionally,
 *              the fields log_pl_len and nprops are updated if necessary.
 *
 *              If this is a new property and doesn't have an entry in the lkup_tbl
 *              increment logical length of the lfsll, nprops, and nprops_added.
 *
 *              Increment phys_pl_len.
 *
 *              Any necessary stats fields are then updated.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_ins_or_mod_prop__list(H5P_mt_list_t *list, const char *name, void *value, size_t size, bool create,
                              bool copy, bool is_new, uint64_t prop_version, H5P_prp_create_func_t prp_create,
                              H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get,
                              H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode,
                              H5P_prp_delete_func_t prp_del, H5P_prp_copy_func_t prp_copy,
                              H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close)
{
    H5P_mt_prop_t             *new_prop = NULL;   /* New prop to be created and inserted */
    H5P_mt_prop_t             *pl_head;           /* Head of the LFSLL of the class or list */
    H5P_mt_prop_value_t        prop_value;        /* Value of the new prop */
    bool                       done      = FALSE; /* Flag to exit a loop to setting atomics */
    bool                       base_flag = FALSE;
    H5P_mt_list_table_entry_t *entry;            /* An entry in lkup_tbl if param is a list */
    H5P_mt_list_prop_ref_t     curr;             /* Curr struct field for list's entry */
    H5P_mt_list_prop_ref_t     new_curr;         /* Updated curr for list's entry if needed */
    uint32_t                   deletes      = 0; /* Tracks number of deletes */
    uint32_t                   visited      = 0; /* Tracks number of nodes visited */
    uint32_t                   thrd_cols    = 0; /* Tracks number of thread collisions */
    uint64_t                   avg_visited  = 0; /* Stats variable */
    uint64_t                   num_calls    = 0; /* Stats variable */
    bool                       chksum_cols  = FALSE;
    bool                       prop_cleanup = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(list->H5P__mt_ins_or_mod_prop__class__num_calls), 1);

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);
    assert(name);
    assert((size > 0 && value != NULL) || (size == 0));

    /* This thread can now proceed and create the new property */
    new_prop = H5P__create_prop(name, value, size, FALSE, prop_version, prp_create, prp_set, prp_get,
                                prp_encode, prp_decode, prp_del, prp_copy, prp_cmp, prp_close);
    if (new_prop == NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, "Failed to create new property.");
    }

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    pl_head = list->pl_head;

    assert(pl_head);
    assert(atomic_load(&(pl_head->tag)) == H5P_MT_PROP_TAG);
    assert(pl_head->sentinel);

    /**** Perform appropriate callback ****/

    prop_value = atomic_load(&(new_prop->value));

    /* If copy is TRUE and the copy callback exists call it */
    if (copy) {
        if (new_prop->copy) {
#if 1
            if (H5P__global_lock_prop_cb__copy(new_prop, new_prop->name, prop_value.size, prop_value.ptr) <
                0) {
                prop_cleanup = TRUE;
                assert(H5P_MT_ASSERT_FAIL);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Copy property callback failed");
            }
#else
            if ((new_prop->copy)(new_prop->name, prop_value.size, prop_value.ptr)) {
                prop_cleanup = TRUE;
                assert(H5P_MT_ASSERT_FAIL);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Copy property callback failed");
            }
#endif
        }
    }
    /* If create is TRUE and the create callback exists call it */
    else if (create) {
        if (new_prop->create) {
#if 1
            if (H5P__global_lock_prop_cb__create(new_prop, new_prop->name, prop_value.size, prop_value.ptr) <
                0) {
                prop_cleanup = TRUE;
                assert(H5P_MT_ASSERT_FAIL);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Create property callback failed");
            }
#else
            if ((new_prop->create)(new_prop->name, prop_value.size, prop_value.ptr)) {
                prop_cleanup = TRUE;
                assert(H5P_MT_ASSERT_FAIL);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Create property callback failed");
            }
#endif
        }
    }

    /* Insert the property into the LFSLL */
    H5P__mt_ins_or_mod_prop__lfsll_ins(pl_head, new_prop, &deletes, &visited, &thrd_cols, &chksum_cols);

    /**
     * Check if this is a 'modification' to a prop in the lkup_tbl.
     * If copying is TRUE, then lkup_tbl has already been searched
     *
     * TODO: ensure this is still correct, or if this needs done even
     * if copy is TRUE.
     */
    if (!copy) {
        entry = H5P__mt_search_lkup_tbl(list->lkup_tbl, 0, (list->nprops_inherited - 1), new_prop->chksum,
                                        new_prop->name);

        /* If entry isn't NULL, this is a 'modification' to a prop in the lkup_tbl */
        if (entry) {
            /* Atomically update entry->curr to the new version of the property */
            done = FALSE;
            do {
                curr = atomic_load(&(entry->curr));

                assert(curr.ver < prop_version);

                /* If there isn't a valid previous version of this property */
                if (NULL == H5P__mt_entry_find_version(entry, prop_version, &base_flag)) {
                    /* Increment counts */
                    atomic_fetch_add(&(list->nprops), 1);
                    atomic_fetch_add(&(list->log_pl_len), 1);
                }
                /* If the base is the previous valid verson */
                else if (!curr.ptr) {
                    /**
                     * Increment logical length of LFSLL, but nprops, because this
                     * is just a new version of a property already being counted.
                     */
                    atomic_fetch_add(&(list->log_pl_len), 1);
                }

                /* If this isn't the first version curr.ptr is pointing to */
                if (curr.ptr) {
                    /* If the version curr.ptr points to is NOT valid */
                    if (1 == H5P__is_valid(curr.ptr, prop_version)) {
                        /* Increment logical length of the LFSLL */
                        atomic_fetch_add(&(list->log_pl_len), 1);
                    }
                }

                /* Set the flag to show this prop is from the lkup_tbl */
                new_prop->in_lkup_tbl = TRUE;

                new_curr.ptr = new_prop;
                new_curr.ver = prop_version;

                /** TODO: May need to change this to atomic_store() */
                if (!atomic_compare_exchange_strong(&(entry->curr), &curr, new_curr)) {
                    /* attempt failed, update stats and try again */
                    atomic_fetch_add(&(list->num_insert_update_entry_cols), 1);

                    /* To not get stuck in an infinite loop while testing */
                    assert(H5P_MT_ASSERT_FAIL);
                }
                else {
                    /* attempt succeeded, update stats */
                    atomic_fetch_add(&(list->num_insert_update_entry), 1);

                    done = TRUE;
                }

                /* If this is the first curr version, set first_ver_of_curr */
                if (0 == atomic_load(&(entry->first_ver_of_curr))) {
                    atomic_store(&(entry->first_ver_of_curr), prop_version);
                }

            } while (!done);

        } /* end if ( entry ) */

    } /* end if ( ! copy ) */

    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    /* If this is a new property update counts */
    if (!new_prop->in_lkup_tbl && is_new) {
        /* Update counts */
        atomic_fetch_add(&(list->nprops), 1);
        atomic_fetch_add(&(list->nprops_added), 1);
        atomic_fetch_add(&(list->log_pl_len), 1);
    }

    /* Increment physical length of the lfsll */
    atomic_fetch_add(&(list->phys_pl_len), 1);

    /* update stats */
    atomic_store(&(list->num_insert_nodes_visited), visited);

    if (visited > atomic_load(&(list->insert_max_nodes_visited))) {
        atomic_store(&(list->insert_max_nodes_visited), visited);
    }

    avg_visited = atomic_load(&(list->insert_avg_nodes_visited));
    num_calls   = atomic_load(&(list->H5P__mt_ins_or_mod_prop__class__num_calls));

    avg_visited = H5P__calc_avg_visited(avg_visited, num_calls, visited);

    atomic_store(&(list->insert_avg_nodes_visited), avg_visited);

    atomic_fetch_add(&(list->num_insert_prop_cols), thrd_cols);

    atomic_fetch_add(&(list->num_insert_prop_success), 1);
    atomic_fetch_add(&(H5P_mt_g.num_props_inserted_lists), 1);

    if (atomic_load(&(list->phys_pl_len)) > atomic_load(&(H5P_mt_g.max_list_num_phys_props))) {
        atomic_store(&(H5P_mt_g.max_list_num_phys_props), atomic_load(&(list->phys_pl_len)));
    }

done:

    /* Cleanup if error occurred */
    if ((ret_value == FAIL) && (new_prop != NULL) && (prop_cleanup)) {
        H5P__mt_close_prop(new_prop);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_ins_or_mod_prop__list() */

/****************************************************************************************
 * Function:    H5P__mt_ins_or_mod_prop__lfsll_ins
 *
 *              Multithread safe only function
 *
 * Purpose:     Inserts a new property (H5P_mt_prop_t struct) into the LFSLL of a
 *              property list (H5P_mt_list_t) or property list class (H5P_mt_class_t).
 *
 *              Helper function that passes pointers to H5P__find_mod_point() to find the
 *              location to insert the new property. Then inserts that new property
 *              between the properties returned by H5P__find_mod_point().
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_ins_or_mod_prop__lfsll_ins(H5P_mt_prop_t *pl_head, H5P_mt_prop_t *new_prop, uint32_t *deletes_ptr,
                                   uint32_t *nodes_visited_ptr, uint32_t *thrd_cols_ptr,
                                   bool *chksum_cols_ptr)
{
    H5P_mt_prop_t     *first_prop;            /* Prop directly before new prop in LFSLL */
    H5P_mt_prop_t     *second_prop;           /* Prop directly after new prop in LFSLL */
    H5P_mt_prop_aptr_t next;                  /* Next struct field pointing to second_prop */
    H5P_mt_prop_aptr_t updated_next;          /* Next struct field to point to new_prop */
    bool               done          = FALSE; /* Flag to exit a loop when setting atomics */
    uint32_t           deletes       = 0;     /* Tracks number of deletes */
    uint32_t           nodes_visited = 0;     /* Tracks number of nodes visited */
    uint32_t           thrd_cols     = 0;     /* Tracks number of thread cols */
    bool               chksum_cols   = FALSE;

    uint64_t new_prop_ver;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    atomic_fetch_add(&(H5P_mt_g.H5P__mt_ins_or_mod_prop__lfsll_ins__num_calls), 1);

    assert(pl_head);
    assert(atomic_load(&(pl_head->tag)) == H5P_MT_PROP_TAG);
    assert(pl_head->sentinel);

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    /**
     * Primary purpose of H5P__mt_ins_or_mod_prop__lfsll_ins is wrapped in this do-while
     * loop to ensure true atomicity. When updating the first_prop to have its next
     * pointer point to the new prop instead of the second_prop, if it fails that means
     * something has changed since grabbing those pointers, and the entire process must
     * be attempted again.
     */
    do {
        first_prop  = NULL;
        second_prop = NULL;

        new_prop_ver = atomic_load(&(new_prop->create_version));

        /**
         * The current implementation of H5P__find_mod_point() should either succeed or
         * trigger an assertion -- thus no need to check return value at present.
         */

        H5P__find_mod_point(pl_head, &first_prop, &second_prop, &deletes, &nodes_visited, &thrd_cols,
                            new_prop->chksum, new_prop->name, new_prop_ver);

        assert(first_prop);
        assert(second_prop);

        assert(atomic_load(&(first_prop->tag)) == H5P_MT_PROP_TAG);
        assert(atomic_load(&(second_prop->tag)) == H5P_MT_PROP_TAG);

        /* Check to make sure there isn't a collision of chksums with different names */
        if (first_prop->chksum == new_prop->chksum) {
            if (0 != strcmp(first_prop->name, new_prop->name)) {
                chksum_cols = TRUE;
                atomic_fetch_add(&(H5P_mt_g.num_chksum_cols), 1);
            }
        }
        if (first_prop->chksum != second_prop->chksum) {
            if (second_prop->chksum == new_prop->chksum) {
                if (0 != strcmp(second_prop->name, new_prop->name)) {
                    HGOTO_ERROR(H5E_PLIST, H5E_EXISTS, FAIL, "chksum collision");
                }
            }
        }

        /* Prep new_prop to be inserted between first_prop and second_prop */
        next = atomic_load(&(first_prop->next));
        assert(next.ptr == second_prop);

        atomic_store(&(new_prop->next), next);

        /** Attempt to atomically insert new_prop
         *
         * NOTE: If this fails, another thread modified the LFSLL and we must
         * update stats and restart to ensure new_prop is correctly inserted.
         */

        updated_next.ptr          = new_prop;
        updated_next.deleted      = FALSE;
        updated_next.dummy_bool_1 = FALSE;
        updated_next.dummy_bool_2 = FALSE;
        updated_next.dummy_bool_3 = FALSE;

        if (!atomic_compare_exchange_strong(&(first_prop->next), &next, updated_next)) {
            thrd_cols++;

            /* assert is to not get stuck in an infinite loop while testing */
            assert(H5P_MT_ASSERT_FAIL);
        }
        else /* The attempt was successful update stats mark done */
        {
            done = TRUE;
        }

    } while (!done);

    /* Update pointers for stats collecting passed into the function */
    *deletes_ptr       = deletes;
    *nodes_visited_ptr = nodes_visited;
    *thrd_cols_ptr     = thrd_cols;
    *chksum_cols_ptr   = chksum_cols;

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_ins_or_mod_prop__lfsll_ins() */

/****************************************************************************************
 * Function:    H5P__unregister
 *
 *              Multithread safe version of H5P__unregister().
 *
 * Purpose:     Internal routine to set the delete_version of a property (H5P_mt_prop_t)
 *              in a property list class (H5P_mt_class_t).
 *
 *              Gets the property's chksum by calling H5_checksum_metadata() on the
 *              provided property name.
 *
 *              Calls H5P__inc_thrd_count() to check the class's thrd.opening and
 *              thrd.closing flags and to increment thrd.count.
 *
 *              H5P__mt_enforce_serialization() is called to check if there is another
 *              thread modifying the LFSLL. If not then proceed, if there is this thread
 *              must wait it's turn.
 *
 *              Then iterate the LFSLL to find the target property by calling the
 *              function H5P__find_mod_point(). The target property is pointed to by prop
 *              and its delete_version is set to the class's next_version. *
 *
 *              Update the log_pl_len and nprops_added fields if necessary, and update
 *              stats.
 *
 *              Update the class's curr_version to the next_version (same version number
 *              that was set as the property's delete_version).
 *
 *              Decrement the class's thrd.count.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__unregister(H5P_mt_class_t *class, const char *name)
{
    H5P_mt_prop_t *prev_prop;             /* Previous prop in the LFSLL */
    H5P_mt_prop_t *prop;                  /* Target prop found in the class */
    H5P_mt_prop_t *pl_head;               /* Head of the LFSLL */
    uint64_t       curr_version;          /* Current version of the class */
    uint64_t       next_version;          /* Next version of the class */
    bool           inc_thrd_flag = FALSE; /* Flag to dec thrd count */
    int64_t        chksum;                /* chksum of the target prop */
    uint32_t       deletes     = 0;       /* Tracks number of deletes */
    uint32_t       visited     = 0;       /* Tracks number of nodes visited */
    uint32_t       thrd_cols   = 0;       /* Tracks number of thread cols */
    uint64_t       avg_visited = 0;       /* Stats variable */
    uint64_t       num_calls   = 0;       /* Stats variable */
    bool           ver_updated = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(name);
    assert(class);
    assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);

    /* Get the chksum for the target prop to delete */
    chksum = H5_checksum_metadata(name, strlen(name), 0);

    /* update stats */
    atomic_fetch_add(&(class->H5P__unregister__num_calls), 1);

    /* Increment thread count */
    if (H5P__inc_thrd_count(class) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment class's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    curr_version = atomic_load(&(class->curr_version));
    next_version = atomic_fetch_add(&(class->next_version), 1);
    ver_updated  = TRUE;

    /* Ensure another thread isn't modifying the LFSLL */
    if ((curr_version + 1) < next_version) {
        if (0 == (curr_version = H5P__mt_enforce_serialization(class, curr_version, next_version))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }

        curr_version = atomic_load(&(class->curr_version));
    }

    assert(curr_version + 1 == next_version);

    /**
     * Callback function for testing, to pass the exact version
     * of the class the new property is being deleted at without
     * having to search the LFSLL try to find the property and check.
     */
    if (H5P_mt_cb.ver_cb) {
        (H5P_mt_cb.ver_cb)(next_version);
    }
    /**
     * end testing function
     */

    pl_head   = class->pl_head;
    prev_prop = NULL;
    prop      = NULL;

    /**
     * The current implementation of H5P__find_mod_point() should either succeed
     * or trigger an assertion -- thus no need to check return value at present.
     */
    H5P__find_mod_point(pl_head, &prev_prop, &prop, &deletes, &visited, &thrd_cols, chksum, name,
                        curr_version);

    assert(prev_prop);
    assert(atomic_load(&(prev_prop->tag)) == H5P_MT_PROP_TAG);
    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

    if (prop->chksum != chksum || 0 != strcmp(prop->name, name)) {
        atomic_fetch_add(&(H5P_mt_g.num_props_deleted_classes_prop_not_found), 1);
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "Property doesn't exist.");
    }

    /* Ensure property isn't already marked as deleted */
    if (0 < atomic_load(&(prop->delete_version))) {
        atomic_fetch_add(&(H5P_mt_g.num_props_deleted_classes_already_deleted), 1);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Property is already marked as deleted.");
    }

    /* Set the prop's delete version */
    atomic_store(&(prop->delete_version), next_version);

    /* Update lengths and counts accordingly */

    /**
     * If prev_prop has same chksum as prop, then prev_prop is
     * a newer version and prop isn't counted in the log_pl_len.
     */
    if (prev_prop->chksum != prop->chksum) {
        atomic_fetch_sub(&(class->log_pl_len), 1);

        H5P_mt_prop_aptr_t next;
        H5P_mt_prop_t     *next_prop;
        H5P_mt_prop_t     *check_prop;

        check_prop = prop;
        next       = atomic_load(&(check_prop->next));
        next_prop  = next.ptr;

        while (next_prop->chksum == prop->chksum) {
            check_prop = next_prop;
            next       = atomic_load(&(next_prop->next));
            next_prop  = next.ptr;
        }

        /* If create_version is > 1, then prop is counted with nprops_added */
        if (1 < atomic_load(&(check_prop->create_version))) {
            atomic_fetch_sub(&(class->nprops_added), 1);
        }
    }

    /* update stats */
    atomic_fetch_add(&(class->num_delete_prop__success), 1);
    atomic_store(&(class->num_delete_prop__nodes_visited), visited);
    atomic_fetch_add(&(class->num_delete_prop__cols), thrd_cols);
    atomic_fetch_add(&(H5P_mt_g.num_props_deleted_classes), 1);

    if (visited > atomic_load(&(class->delete_prop__max_nodes_visited))) {
        atomic_store(&(class->delete_prop__max_nodes_visited), visited);
    }

    avg_visited = atomic_load(&(class->delete_prop__avg_nodes_visited));
    num_calls   = atomic_load(&(class->H5P__unregister__num_calls));

    avg_visited = H5P__calc_avg_visited(avg_visited, num_calls, visited);

    atomic_store(&(class->delete_prop__avg_nodes_visited), avg_visited);

done:

    /* Cleanup if error occurred */
    if (ver_updated) {
        /* Update the class's current version */
        curr_version = atomic_fetch_add(&(class->curr_version), 1);
        assert(curr_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(class->curr_version)) > atomic_load(&(H5P_mt_g.max_class_version_number))) {
            atomic_store(&(H5P_mt_g.max_class_version_number), atomic_load(&(class->curr_version)));
        }
    }

    /* If the thrd_count was incremented, decremented it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(class)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__unregister() */

/****************************************************************************************
 * Function:    H5P_remove
 *
 *              Multithread safe version of H5P__remove().
 *
 * Purpose:     Internal routine to set the delete_version of a property (H5P_mt_prop_t)
 *              in a property list (H5P_mt_list_t).
 *
 *              Gets the property's chksum by calling H5_checksum_metadata() on the
 *              provided property name.
 *
 *              Calls H5P__inc_thrd_count() to check the list's thrd.opening and
 *              thrd.closing flags and to increment thrd.count.
 *
 *              H5P__mt_enforce_serialization() is called to check if there is another
 *              thread modifying the LFSLL. If not then proceed, if there is this thread
 *              must wait it's turn.
 *
 *              Calls H5P__mt_delete_prop__list() to find the target property, set its
 *              delete_version, call the delete callback if the property has one, update
 *              other necessary fields (i.e log_pl_len, etc.), and update stats.
 *
 *              Update the list's curr_version to the next_version (same version number
 *              that was set as the base_delete_version or property's delete_version).
 *
 *              Decrement the list's thrd.count.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P_remove(H5P_mt_list_t *list, const char *name)
{
    uint64_t curr_version;          /* Current version of the list */
    uint64_t next_version;          /* Next version of the list */
    bool     inc_thrd_flag = FALSE; /* Flag to dec thrd count */
    int64_t  chksum;                /* chksum of the target prop */
    bool     ver_updated = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    assert(name);
    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

    chksum = H5_checksum_metadata(name, strlen(name), 0);

    /* update stats */
    atomic_fetch_add(&(list->H5P_remove__num_calls), 1);

    /* Increment thread count */
    if (H5P__inc_thrd_count(list) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    curr_version = atomic_load(&(list->curr_version));
    next_version = atomic_fetch_add(&(list->next_version), 1);
    ver_updated  = TRUE;

    /* Ensure another thread isn't modifing the LFSLL */
    if ((curr_version + 1) < next_version) {
        if (0 == (curr_version = H5P__mt_enforce_serialization(list, curr_version, next_version))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }

        curr_version = atomic_load(&(list->curr_version));
    }

    assert(curr_version + 1 == next_version);

    /**
     * Callback function for testing, to pass the exact version of the
     * list the new property is being deleted at without having to search
     * the lkup_tbl and LFSLL to find the property and check.
     */
    if (H5P_mt_cb.ver_cb) {
        (H5P_mt_cb.ver_cb)(next_version);
    }
    /**
     * end testing function
     */

#if 1
    if ((H5P__mt_delete_prop__list(list, chksum, name, curr_version, next_version)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTDELETE, FAIL, "Can't delete property");
    }
#else
    pl_head = list->pl_head;
    prev_prop = NULL;
    prop = NULL;

    entry = H5P__mt_search_lkup_tbl(list->lkup_tbl, 0, (list->nprops_inherited - 1), chksum, name);

    /* If not NULL, entry contains prop to delete, but must find correct version */
    if (entry) {
        prop = H5P__mt_entry_find_version(entry, curr_version, &base_flag);

        /* If NULL, the current version of the prop was already deleted */
        if (NULL == prop) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Property already deleted.");
        }

        if (base_flag) {
            assert(0 == atomic_load(&(entry->base_delete_version)));

            /* Set base_delete_version */
            atomic_store(&(entry->base_delete_version), next_version);

            /* Decrement nprops */
            atomic_fetch_sub(&(list->nprops), 1);

            /* update stats */
            atomic_fetch_add(&(list->num_set_delete__base_delete_version), 1);

            done = TRUE;
        }
        else {
            /* Ensure the property isn't already deleted */
            assert(0 == atomic_load(&(prop->delete_version)));

            /* Set prop's delete_version */
            atomic_store(&(prop->delete_version), next_version);

            curr = atomic_load(&(entry->curr));

            /* If TRUE, the property that curr.ptr points to is the target_prop */
            if ((atomic_load(&(prop->create_version))) == (atomic_load(&(curr.ptr->create_version)))) {
                /* Decrement logical length and nprops */
                atomic_fetch_sub(&(list->log_pl_len), 1);
                atomic_fetch_sub(&(list->nprops), 1);

                /* update stats */
                atomic_fetch_add(&(list->num_set_delete__curr_entry), 1);
            }
            else /* target prop is not the most recent version */
            {
                /* update stats */
                atomic_fetch_add(&(list->num_set_delete__older_curr), 1);
            }

            done = TRUE;
        }

    } /* end if ( entry ) */

    if (!done) {
        /**
         * The current implementation of H5P__find_mod_point() should either succeed
         * or trigger an assertion -- thus no need to check return value at present.
         */
        H5P__find_mod_point(pl_head, &prev_prop, &prop, &deletes, &visited, &thrd_cols, chksum, name,
                            curr_version);

        assert(prev_prop);
        assert(atomic_load(&(prev_prop->tag)) == H5P_MT_PROP_TAG);

        assert(prop);
        assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

        if (prop->chksum != chksum || 0 != strcmp(prop->name, name)) {
            atomic_fetch_add(&(H5P_mt_g.num_props_deleted_lists_prop_not_found), 1);
            HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "Property doesn't exist.");
        }

        /* Ensure property isn't already marked as deleted */
        if (0 < atomic_load(&(prop->delete_version))) {
            atomic_fetch_add(&(H5P_mt_g.num_props_deleted_lists_already_deleted), 1);
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Property is already marked as deleted.");
        }

        /* Set the prop's delete_verison */
        atomic_store(&(prop->delete_version), next_version);

        /**
         * If prev_prop has same chksum as prop, then prev_prop is
         * a newer version and prop isn't counted in the log_pl_len.
         */
        if (prev_prop->chksum != prop->chksum) {
            atomic_fetch_sub(&(list->log_pl_len), 1);
            atomic_fetch_sub(&(list->nprops), 1);
            atomic_fetch_sub(&(list->nprops_added), 1);
        }

        /* update stats */
        atomic_fetch_add(&(list->num_deletes_from_lfsll), 1);

        done = TRUE;

    } /* end while ( ! done ) */

    /* If the prop has a del callback, call it */
    if (prop->del) {
        value = atomic_load(&(prop->value));

        if ((*(prop->del))(list->plist_id, prop->name, value.size, value.ptr) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTFREE, FAIL, "can't release property value");
        }
    }

    /* update stats */
    atomic_fetch_add(&(list->num_set_delete__success), 1);
    atomic_store(&(list->num_set_delete__nodes_visited), visited);
    atomic_fetch_add(&(list->num_set_delete__cols), thrd_cols);
    atomic_fetch_add(&(H5P_mt_g.num_props_deleted_lists), 1);

    if (visited > 0) {
        if (visited > atomic_load(&(list->set_delete__max_nodes_visited))) {
            atomic_store(&(list->set_delete__max_nodes_visited), visited);
        }

        avg_visited = atomic_load(&(list->set_delete__avg_nodes_visited));
        num_calls = atomic_load(&(list->num_deletes_from_lfsll));

        avg_visited = H5P__calc_avg_visited(avg_visited, num_calls, visited);

        atomic_store(&(list->set_delete__avg_nodes_visited), avg_visited);
    }
#endif

done:

    /* Cleanup if error occurred */
    if (ver_updated) {
        /* Update the list's current version */
        curr_version = atomic_fetch_add(&(list->curr_version), 1);
        assert(curr_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(list->curr_version)) > atomic_load(&(H5P_mt_g.max_list_version_number))) {
            atomic_store(&(H5P_mt_g.max_list_version_number), atomic_load(&(list->curr_version)));
        }

        /* Update global atomic plist version if list is a default plist */
        if (list->def_ver_ptr) {
            atomic_fetch_add((list->def_ver_ptr), 1);

            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_default_list_mods), 1);
        }
    }

    /* If the thrd_count was incremented, decremented it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(list)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_remove() MT safe version */

/****************************************************************************************
 * Function:    H5P__mt_delete_prop__list
 *
 *              Multithread safe only function
 *
 * Purpose:     Internal function that sets the delete_version on a property in a
 *              property list to the list's next_version.
 *
 *              Iterate the lkup_tbl by calling H5P__mt_search_lkup_tbl(). If entry
 *              is not NULL, we must find the correct version of the property using
 *              H5P__mt_entry_find_version(), in case the most current version is not the
 *              version being deleted. If base_flag is TRUE, we set the entry's
 *              base_delete_version, or if FALSE, set the delete_version of the returned
 *              property, to the list's next version.
 *
 *              If the list's lkup_tbl doesn't contain the target_prop iterate the LFSLL
 *              using H5P__find_mod_point(), and set the delete_version of the returned
 *              property to the list's next version.
 *
 *              The property is checked for a delete callback, and if it exists it is
 *              called.
 *
 *              Update the log_pl_len, nprops, and nprops_added fields if necessary, and
 *              stats are updated.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_delete_prop__list(H5P_mt_list_t *list, int64_t chksum, const char *name, uint64_t curr_version,
                          uint64_t next_version)
{
    H5P_mt_prop_t             *prev_prop = NULL;    /* Previous prop in the LFSLL */
    H5P_mt_prop_t             *prop      = NULL;    /* Target prop found in the list */
    H5P_mt_prop_t             *pl_head;             /* Head of the LFSLL */
    H5P_mt_prop_value_t        value;               /* Value of the target prop */
    H5P_mt_list_table_entry_t *entry;               /* Entry in a list's lkup_tbl */
    H5P_mt_list_prop_ref_t     curr;                /* Curr field of the entry */
    bool                       done        = FALSE; /* Flag to exit loops to set atomics */
    bool                       base_flag   = FALSE; /* Flag if entry's base is target prop */
    uint32_t                   deletes     = 0;     /* Tracks number of deletes */
    uint32_t                   visited     = 0;     /* Tracks number of nodes visited */
    uint32_t                   thrd_cols   = 0;     /* Tracks number of thread cols */
    uint64_t                   avg_visited = 0;     /* Stats variable */
    uint64_t                   num_calls   = 0;     /* Stats variable */

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(name);
    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

    /* update stats */
    atomic_fetch_add(&(list->H5P__mt_delete_prop__list__num_calls), 1);

    pl_head   = list->pl_head;
    prev_prop = NULL;
    prop      = NULL;

    entry = H5P__mt_search_lkup_tbl(list->lkup_tbl, 0, (list->nprops_inherited - 1), chksum, name);

    /* If not NULL, entry contains prop to delete, but must find correct version */
    if (entry) {
        prop = H5P__mt_entry_find_version(entry, curr_version, &base_flag);

        /* If NULL, the current version of the prop was already deleted */
        if (NULL == prop) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Property already deleted.");
        }

        if (base_flag) {
            assert(0 == atomic_load(&(entry->base_delete_version)));

            /* Set base_delete_version */
            atomic_store(&(entry->base_delete_version), next_version);

            /* Decrement nprops */
            atomic_fetch_sub(&(list->nprops), 1);

            /* update stats */
            atomic_fetch_add(&(list->num_delete_prop__base_delete_version), 1);

            done = TRUE;
        }
        else {
            /* Ensure the property isn't already deleted */
            assert(0 == atomic_load(&(prop->delete_version)));

            /* Set prop's delete_version */
            atomic_store(&(prop->delete_version), next_version);

            curr = atomic_load(&(entry->curr));

            /* If TRUE, the property that curr.ptr points to is the target_prop */
            if ((atomic_load(&(prop->create_version))) == (atomic_load(&(curr.ptr->create_version)))) {
                /* Decrement logical length and nprops */
                atomic_fetch_sub(&(list->log_pl_len), 1);
                atomic_fetch_sub(&(list->nprops), 1);

                /* update stats */
                atomic_fetch_add(&(list->num_delete_prop__curr_entry), 1);
            }
            else /* target prop is not the most recent version */
            {
                /* update stats */
                atomic_fetch_add(&(list->num_delete_prop__older_curr), 1);
            }

            done = TRUE;
        }

    } /* end if ( entry ) */

    if (!done) {
        /**
         * The current implementation of H5P__find_mod_point() should either succeed
         * or trigger an assertion -- thus no need to check return value at present.
         */
        H5P__find_mod_point(pl_head, &prev_prop, &prop, &deletes, &visited, &thrd_cols, chksum, name,
                            curr_version);

        assert(prev_prop);
        assert(atomic_load(&(prev_prop->tag)) == H5P_MT_PROP_TAG);

        assert(prop);
        assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

        if (prop->chksum != chksum || 0 != strcmp(prop->name, name)) {
            atomic_fetch_add(&(H5P_mt_g.num_props_deleted_lists_prop_not_found), 1);
            HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "Property doesn't exist.");
        }

        /* Ensure property isn't already marked as deleted */
        if (0 < atomic_load(&(prop->delete_version))) {
            atomic_fetch_add(&(H5P_mt_g.num_props_deleted_lists_already_deleted), 1);
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Property is already marked as deleted.");
        }

        /* Set the prop's delete_verison */
        atomic_store(&(prop->delete_version), next_version);

        /**
         * If prev_prop has same chksum as prop, then prev_prop is
         * a newer version and prop isn't counted in the log_pl_len.
         */
        if (prev_prop->chksum != prop->chksum) {
            atomic_fetch_sub(&(list->log_pl_len), 1);
            atomic_fetch_sub(&(list->nprops), 1);
            atomic_fetch_sub(&(list->nprops_added), 1);
        }

        /* update stats */
        atomic_fetch_add(&(list->num_delete_props_from_lfsll), 1);

        done = TRUE;

    } /* end while ( ! done ) */

    /* If the prop has a del callback, call it */
    if (prop->del) {
        value = atomic_load(&(prop->value));
#if 1
        if (H5P__global_lock_prop_cb__del(prop, list->plist_id, prop->name, value.size, value.ptr) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTFREE, FAIL, "can't release property value");
        }
#else
        if ((*(prop->del))(list->plist_id, prop->name, value.size, value.ptr) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTFREE, FAIL, "can't release property value");
        }
#endif
    }

    /* update stats */
    atomic_fetch_add(&(list->num_delete_prop__success), 1);
    atomic_store(&(list->num_delete_prop__nodes_visited), visited);
    atomic_fetch_add(&(list->num_delete_prop__cols), thrd_cols);
    atomic_fetch_add(&(H5P_mt_g.num_props_deleted_lists), 1);

    if (visited > 0) {
        if (visited > atomic_load(&(list->delete_prop__max_nodes_visited))) {
            atomic_store(&(list->delete_prop__max_nodes_visited), visited);
        }

        avg_visited = atomic_load(&(list->delete_prop__avg_nodes_visited));
        num_calls   = atomic_load(&(list->num_delete_props_from_lfsll));

        avg_visited = H5P__calc_avg_visited(avg_visited, num_calls, visited);

        atomic_store(&(list->delete_prop__avg_nodes_visited), avg_visited);
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_delete_prop__list() */

/****************************************************************************************
 * Function:    H5P__mt_search__class
 *
 *              Multithread only function that replaces the original function
 *              H5P__find_prop_pclass().
 *
 *              NOTE: H5P__find_prop_pclass() was replaced due to H5P__mt_search__class()
 *              being made to search a specific provided version of a class, and upon
 *              further modifications to H5P to make it multithread safe,
 *              H5P__find_prop_pclass() wasn't used by anything anymore.
 *
 * Purpose:     Searches a property class (H5P_mt_class_t) for a property (H5P_mt_prop_t)
 *              at a specified version.
 *
 *              Given the name of a property from the parameter name, the chksum of the
 *              property is calculated and then the thread count of the class structure
 *              is incremented.
 *
 *              The class's LFSLL is then searched for the target property by calling
 *              H5P__mt_search_lfsll(). If the property is found the most current version
 *              is returned after decrementing the class's thread count.
 *
 * Return:      Success: Returns a pointer to the target property valid at the specified
 *                       version of the class
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__mt_search__class(H5P_mt_class_t *class, const char *name, uint64_t version)
{
    H5P_mt_prop_t *prop = NULL; /* Prop being searched for */
    H5P_mt_prop_t *pl_head;     /* Head of the LFSLL */
    int64_t        chksum;      /* Chksum for the prop from name */
    uint64_t       avg_visited   = 0;
    uint64_t       num_calls     = 0;
    uint64_t       visited       = 0;
    bool           inc_thrd_flag = FALSE; /* Flag to dec thrd count of struct */
    bool           chksum_cols   = FALSE;

    H5P_mt_prop_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    assert(name);
    assert(class);
    assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG ||
           atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);

    if (atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, "Class is invalid");
    }

    chksum = H5_checksum_metadata(name, strlen(name), 0);

    /* update stats */
    atomic_fetch_add(&(class->H5P__mt_search_prop__class__num_calls), 1);

    /* Increment thread count */
    if (H5P__inc_thrd_count(class) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, "Couldn't increment class's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /**
     * stat for tracking number of searches that occur
     * while an insert or delete are taking place
     */
    uint64_t class_version = atomic_load(&(class->curr_version));
    uint64_t next_version  = atomic_load(&(class->next_version));

    if ((class_version + 1) != next_version) {
        atomic_fetch_add(&(H5P_mt_g.num_searches_while_an_op_occurs_class), 1);
    }

    pl_head = class->pl_head;

    /* Search the LFSLL for the target prop */
    prop = H5P__mt_search_lfsll(pl_head, chksum, name, version, &visited, &chksum_cols);

    /* If target prop exists in the class */
    if (prop) {
        if (0 < atomic_load(&(prop->delete_version)) && atomic_load(&(prop->delete_version)) <= version) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, "Property is marked deleted.");
        }
    }

    /* If chksum_cols is TRUE update stats */
    if (chksum_cols) {
        atomic_fetch_add(&(class->num_search_chksum_cols), 1);
    }

    /* update stats */
    atomic_store(&(class->num_search_class__nodes_visited), visited);

    if (visited > atomic_load(&(class->search_class__max_nodes_visited))) {
        atomic_store(&(class->search_class__max_nodes_visited), visited);
    }

    avg_visited = atomic_load(&(class->search_class__avg_nodes_visited));
    num_calls   = atomic_load(&(class->H5P__mt_search_prop__class__num_calls));

    avg_visited = H5P__calc_avg_visited(avg_visited, num_calls, visited);

    atomic_store(&(class->search_class__avg_nodes_visited), avg_visited);

    if (prop) {
        atomic_fetch_add(&(class->num_search_class__success), 1);
    }
    else {
        atomic_fetch_add(&(H5P_mt_g.num_searches_classes_prop_not_found), 1);
    }

    atomic_fetch_add(&(H5P_mt_g.num_searches_classes), 1);

    ret_value = prop;

done:

    /* If the parent's thrd_count was incremented, it must be decremented */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(class)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, NULL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_search__class() */

/****************************************************************************************
 * Function:    H5P__find_prop_plist
 *
 * Purpose:     Multithread safe version of H5P__find_prop_plist() and
 *              H5P__find_prop_pclass() which are internal routines to check for a
 *              property in a property list and property list class respectively.
 *
 *              This is a passthrough function that calls H5P__mt_search__list()
 *              which handles the actual searching for the property in the list.
 *
 * Return:      Success: Pointer to the property
 *
 *              Failure: NULL (not in the plist)
 *
 ****************************************************************************************
 */
H5P_genprop_t *
H5P__find_prop_plist(H5P_mt_list_t *plist, const char *name)
{
    H5P_genprop_t *ret_value = NULL; /* Return value */
    uint64_t       version;

    FUNC_ENTER_PACKAGE

    assert(plist);
    assert(name);

    /* update stats */
    atomic_fetch_add(&(plist->H5P__find_prop_plist__num_calls), 1);

    /* If the context is initialized grab version from it */
    if (H5P_H5CX_INIT_g) {
        if (0 == (version = H5CX_get_plist_version(atomic_load(&(plist->plist_id))))) {
            version = atomic_load(&(plist->curr_version));
        }
        else {
            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_list_version_from_ctx), 1);
        }
    }
    else {
        version = atomic_load(&(plist->curr_version));
    }

    /* Search the list at the version from the context or curr_version if not in context */
    if (NULL == (ret_value = H5P__mt_search__list(plist, name, version))) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, NULL, "can't find property in property list");
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__find_prop_plist() MT safe version */

/****************************************************************************************
 * Function:    H5P__mt_search__list
 *
 *              Multithread only function that mostly replaces the original function
 *              H5P__find_prop_plist().
 *
 *              NOTE: H5P__find_prop_plist() was mostly replaced due to H5P__mt_search__list()
 *              being made to search a specific provided version of a list, and upon
 *              further modifications to H5P to make it multithread safe,
 *              H5P__find_prop_plist() was only used by H5P__decode()
 *
 * Purpose:     Searches a property list (H5P_mt_list_t) for a property (H5P_mt_prop_t)
 *              at a specified version.
 *
 *              Given the name of a propert from the parameter name, the chksum of the
 *              property is calculated and then the thread count of the list structure
 *              is incremented.
 *
 *              The list's lkup_tbl is searched first, and if the target property isn't
 *              found the LFSLL is then searched.
 *
 *              If the property is found at the specified version is returned after
 *              decrementing the list's thread count.
 *
 * Return:      Success: Returns a pointer to the target property valid at the specified
 *                       version of the list
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__mt_search__list(H5P_mt_list_t *list, const char *name, uint64_t version)
{
    H5P_mt_prop_t             *prop = NULL; /* Prop being searched for */
    H5P_mt_prop_t             *pl_head;     /* Head of the LFSLL */
    H5P_mt_list_table_entry_t *entry;       /* Entry in a list's lkup_tbl */
    uint64_t                   curr_version;
    uint64_t                   next_version;
    uint64_t                   visited = 0;
    int64_t                    chksum;                /* Chksum for the prop from name */
    bool                       done          = FALSE; /* Flag to exit a loop to set atomics */
    bool                       inc_thrd_flag = FALSE; /* Flag to dec thrd count of struct */
    bool                       chksum_cols   = FALSE;
    bool                       base_flag     = FALSE;

    H5P_mt_prop_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    assert(name);
    assert(list);
#ifndef NDEBUG
    if (atomic_load(&(list->tag)) != H5P_MT_LIST_TAG) {
        fprintf(stderr, "\nList tag is NOT valid\n");
        return NULL;
    }
#endif
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

    chksum = H5_checksum_metadata(name, strlen(name), 0);

    /* update stats */
    atomic_fetch_add(&(list->H5P__mt_search_prop__list__num_calls), 1);

    /* Increment thread count */
    if (H5P__inc_thrd_count(list) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, "Couldn't increment class's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /**
     * stat for tracking number of searches that occur
     * while an insert or delete are taking place
     */
    curr_version = atomic_load(&(list->curr_version));
    next_version = atomic_load(&(list->next_version));

    if ((curr_version + 1) != next_version) {
        atomic_fetch_add(&(H5P_mt_g.num_searches_while_an_op_occurs_list), 1);
    }

    pl_head = list->pl_head;

    entry = H5P__mt_search_lkup_tbl(list->lkup_tbl, 0, (list->nprops_inherited - 1), chksum, name);

    if (entry) {
        assert(entry->chksum == chksum);
        assert(0 == (strcmp(entry->name, name)));

        prop = H5P__mt_entry_find_version(entry, version, &base_flag);

        /**
         * If prop isn't NULL we know it's not deleted due to
         * that being checked in H5P__mt_entry_find_version().
         */
        if (prop) {
            if (base_flag) {
                atomic_fetch_add(&(list->num_search_list__found_base), 1);
            }
            else {
                atomic_fetch_add(&(list->num_search_list__found_curr), 1);
            }
            done = TRUE;
        }
        else {
            atomic_fetch_add(&(list->num_target_prop_found_but_deleted), 1);

            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, "Property is marked deleted.");
        }

    } /* end if ( entry ) */

    /* If not in the lkup_tbl search the LFSLL */
    if (!done) {
        /* Search the LFSLL for the target prop */
        prop = H5P__mt_search_lfsll(pl_head, chksum, name, version, &visited, &chksum_cols);

        /* If the target prop exists in the LFSLL */
        if (prop) {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

            if (0 < atomic_load(&(prop->delete_version)) && atomic_load(&(prop->delete_version)) <= version) {
                HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, "Property is marked deleted.");
            }
        }

        /* If chksum_cols is TRUE update stats */
        if (chksum_cols) {
            atomic_fetch_add(&(list->num_search_chksum_cols), 1);
        }

        done = TRUE;
    }

    ret_value = prop;

done:

    /* update stats */
    if (prop) {
        atomic_fetch_add(&(list->num_search_list__success), 1);
    }
    else {
        atomic_fetch_add(&(H5P_mt_g.num_searches_lists_prop_not_found), 1);
    }

    atomic_fetch_add(&(H5P_mt_g.num_searches_lists), 1);

    /* If the parent's thrd_count was incremented, it must be decremented */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(list)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, NULL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_search__list() */

/****************************************************************************************
 * Function:    H5P__mt_search_lkup_tbl
 *
 *              Multithread safe only function
 *
 * Purpose:     Searches a property list's (H5P_mt_list_t) lkup_tbl for an entry that has
 *              the same chksum as the provided chksum.
 *
 *              The lkup_tbl is an array, so for efficiency an array binary search is
 *              used to search it.
 *
 * Return:      Success: Returns a pointer to the entry in the lkup_tbl
 *                       NULL if the entry doesn't exist in the lkup_tbl.
 *
 *              Failure: Can't fail
 *
 ****************************************************************************************
 */
H5P_mt_list_table_entry_t *
H5P__mt_search_lkup_tbl(H5P_mt_list_table_entry_t *lkup_tbl, size_t left_entry, size_t right_entry,
                        int64_t chksum, const char *name)
{
    H5P_mt_list_table_entry_t *entry;  /* Entry in the lkup_tbl */
    int32_t                    left;   /* left side of the search */
    int32_t                    middle; /* index of lkup_tbl to check */
    int32_t                    right;  /* right side of the search */
    int32_t                    cmp_result;

    H5P_mt_list_table_entry_t *ret_value = NULL;

    FUNC_ENTER_NOAPI_NOERR

    left  = (int32_t)left_entry;
    right = (int32_t)right_entry;

    /* Binary search the lkup_tbl for the chksum */
    while (left <= right) {
        middle = left + (right - left) / 2;

        entry = &lkup_tbl[middle];

        if (entry->chksum == chksum) {
            cmp_result = strcmp(entry->name, name);

            if (0 == cmp_result) {
                ret_value = entry;
                break;
            }
            else if (0 < cmp_result) {
                left = middle + 1;

                /* update stats */
                atomic_fetch_add(&(H5P_mt_g.num_chksum_cols), 1);
            }
            else {
                right = middle - 1;

                /* update stats */
                atomic_fetch_add(&(H5P_mt_g.num_chksum_cols), 1);
            }
        }

        else if (entry->chksum < chksum) {
            left = middle + 1;
        }
        else {
            right = middle - 1;
        }

    } /* end while ( left <= right ) */

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_search_lkup_tbl() */

/****************************************************************************************
 * Function:    H5P__mt_entry_find_version
 *
 *              Multithread safe only version
 *
 * Purpose:     Finds the correct version of a property from a list's lkup_tbl.
 *
 *              First we check if the entry's curr.ptr field is NULL, and if it isn't
 *              check if the version we're searching for is greater than or equal to
 *              first_ver_of_curr. If curr.ptr is NULL, or if curr.ptr isn't NULL but
 *              version is less than first_ver_of_curr, we know the base is the version
 *              we're searching for and don't need to check curr.ptr and previous
 *              versions of it. Then set is_base to TRUE so the calling function knows
 *              that the returned property is the base and is in the parent's LFSLL,
 *              and not actually in the list.
 *
 *              If curr.ptr is not NULL, then we compare curr.ver to the version we're
 *              searching for. If curr.ver is equal or less than version and hasn't
 *              been deleted, it's the correct version of the property. If curr.ver is
 *              greater than version, we must iterate the LFSLL to the next property.
 *              First check the chksum of the next property and if not the same, then
 *              there isn't a valid version of the property for the version we're
 *              searching for. If it does have the same chksum, check it's
 *              version and if it's deleted.
 *
 *              If at any point we find the correct version of the property and it isn't
 *              deleted, we return that property. If the correct version was found but
 *              it has been deleted, we return NULL. If while iterating the LFSLL and
 *              next prop has a different chksum, break and check if the base has been
 *              deleted and if it hasn't return base.ptr. But if base has been deleted,
 *              return NULL.
 *
 *              NOTE: In the lkup_tbl an entry's curr.ptr field points to the most
 *              recent version of the property in the LFSLL of the list. Thus, if the
 *              version we're searching for is older than the most recent one, we
 *              iterate to the next property in the LFSLL, since the LFSLL is sorted
 *              first by chksum then by version number.
 *
 *
 * Return:      Success: Returns a pointer to the correct version of the property, and if
 *                       base.ptr is the correct version also sets the parameter
 *                       bool *base_flag to TRUE, so the calling function knows base.ptr
 *                       is the correct version.
 *
 *              Failure: NULL  NOTE: this means there isn't a valid version of the prop
 *                                   for the version we are searching for.
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__mt_entry_find_version(H5P_mt_list_table_entry_t *entry, uint64_t version, bool *base_flag)
{
    H5P_mt_list_prop_ref_t curr;            /* Curr struct field for list's entry */
    H5P_mt_list_prop_ref_t base;            /* Base struct field for list's entry */
    H5P_mt_prop_t         *prop;            /* property being checked if correct version */
    H5P_mt_prop_aptr_t     next;            /* prop next struct field for iterating */
    uint64_t               curr_create_ver; /* Create version of current prop */
    uint64_t               curr_delete_ver; /* delete version of current prop */
    uint64_t               base_delete_ver; /* Entry's base_delete_ver field */
    bool                   done    = FALSE; /* Flag to exit a loop to set atomics */
    bool                   is_base = FALSE; /* Flag to dec thrd count of struct */

    H5P_mt_prop_t *ret_value = NULL;

    FUNC_ENTER_NOAPI_NOERR

    curr = atomic_load(&(entry->curr));
    base = atomic_load(&(entry->base));

    /**
     * If curr.ptr isn't NULL and the version being searched for isn't less
     * than the version at which curr first was created, search for the valid
     * version in the LFSLL.
     */
    if (curr.ptr && (version >= (atomic_load(&(entry->first_ver_of_curr))))) {
        prop = curr.ptr;

        assert(prop->chksum == entry->chksum);

        /**
         * Ensure we have the correct version and it's not marked deleted.
         * If not correct version iterate to the next version in the LFSLL.
         */
        do {
            curr_delete_ver = atomic_load(&(prop->delete_version));
            curr_create_ver = atomic_load(&(prop->create_version));

            /* If the property is the correct version */
            if (curr_create_ver <= version) {
                /* Ensure the property hasn't been deleted */
                if (curr_delete_ver == 0 || curr_delete_ver > version) {
                    ret_value = prop;
                }
                else {
                    HGOTO_DONE(NULL);
                }

                done = TRUE;
            }

            /* If not the correct version iterate to the next version in the lfsll */
            if (!done) {
                next = atomic_load(&(prop->next));
                prop = next.ptr;

                /* If the chksums don't match then we're now on a different prop */
                if (prop->chksum != entry->chksum) {
                    break;
                }
            }

        } while (!done);

    } /* end if ( curr.ptr ) */

    /* If done is FALSE when we get here, check the base isn't deleted for our version */
    if (!done) {
        base_delete_ver = atomic_load(&(entry->base_delete_version));

        /* If the base isn't deleted for the version, return it and set is_base */
        if (base_delete_ver == 0 || base_delete_ver > version) {
            ret_value = base.ptr;

            is_base = TRUE;
        }
        else {
            HGOTO_DONE(NULL);
        }

    } /* end if ( ! done ) */

done:

    *base_flag = is_base;

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_entry_find_version() */

/****************************************************************************************
 * Function:    H5P__find_mod_point
 *
 *              Multithread version only function
 *
 * Purpose:     Iterates the lock free singly linked list (LFSLL) of a property list
 *              class (H5P_mt_class_t) or property list (H5P_mt_list_t).
 *
 *              This function is called when the LFSLL of a class or list is needed to
 *              be iterated to find the point where a new property needs to be inserted,
 *              or is to have its delete_version set.
 *
 *              As parameters this function takes the head of the LFSLL from either the
 *              class or list we are iterating, and two property double pointers,
 *              first_ptr_ptr and second_ptr_ptr which are used to pass the two
 *              properties, back to the calling function, for either a property to be
 *              added between them, or to set the delete_version on a prop. The parameter
 *              chksum is the chksum of the property we are trying to find, and version
 *              is the version we need to find the valid property at. deletes_ptr,
 *              nodes_visited_ptr, and thrd_cols_ptr are for stats collecting.
 *
 *              NOTE: When simply searching for prop, not for an insertion or deletion,
 *              H5P__mt_search__class() or H5P__mt_search__list() is used instead of this
 *              function. This is because the two search functions don't compare versions
 *              and always return the most current version. If in the future specific
 *              versions will need to be searched for then this function will most likely
 *              replace the search functions where appropriate.
 *
 *              Starting at the head of the LFSLL, the first_prop is the sentinel node,
 *              so we use second_prop, and compare it to the chksum and version provided.
 *              If a prop with a matching chksum and a create_version less than or equal
 *              to the provided version, that is set as second_ptr_ptr, and the property
 *              directly before it in the LFSLL is set as first_ptr_ptr.
 *
 *              If the second_prop has a larger chksum, then that prop is set as
 *              second_ptr_ptr and it's previous prop is first_ptr_ptr.
 *
 *              If the second_prop has a smaller chksum, then iterate the LFSLL.
 *
 *
 *              NOTE: for more information on how the LFSLL is sorted see the description
 *              comment for structure H5P_mt_class_t in H5Ppkg_mt.h.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__find_mod_point(H5P_mt_prop_t *pl_head, H5P_mt_prop_t **first_ptr_ptr, H5P_mt_prop_t **second_ptr_ptr,
                    uint32_t *deletes_ptr, uint32_t *nodes_visited_ptr, uint32_t *thrd_cols_ptr,
                    int64_t chksum, const char *name, uint64_t prop_version)
{
    bool               done          = FALSE; /* Flag to exit a loop to set atomics */
    uint32_t           thrd_cols     = 0;     /* Tracks number of deletes */
    uint32_t           deletes       = 0;     /* Tracks number of nodes visited */
    uint32_t           nodes_visited = 0;     /* Tracks number of thread cols */
    H5P_mt_prop_t     *first_prop;            /* Previous prop in LFSLL */
    H5P_mt_prop_t     *second_prop;           /* Current prop in LFSLL */
    H5P_mt_prop_aptr_t next_prop;             /* Prop next struct field for iteration */
    uint64_t           sec_prop_ver;          /* Version of second_prop */
    int32_t            name_cmp_result;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    assert(pl_head->sentinel);

    assert(first_ptr_ptr);
    assert(NULL == *first_ptr_ptr);
    assert(second_ptr_ptr);
    assert(NULL == *second_ptr_ptr);
    assert(deletes_ptr);
    assert(nodes_visited_ptr);
    assert(thrd_cols_ptr);
    assert(chksum > LLONG_MIN && chksum < LLONG_MAX);
    assert(name);
    assert(prop_version > 0);

    first_prop = pl_head;

    assert(first_prop);
    assert(atomic_load(&(first_prop->tag)) == H5P_MT_PROP_TAG);
    assert(first_prop->sentinel);

    next_prop   = atomic_load(&(first_prop->next));
    second_prop = next_prop.ptr;

    assert(second_prop);
    assert(atomic_load(&(second_prop->tag)) == H5P_MT_PROP_TAG);

    /* Iterate the LFSLL to find the two properties needed for the modification */
    do {
        nodes_visited++;

        if (second_prop->chksum == chksum) {
            /* In the case there is a chksum collision */
            if (0 != (name_cmp_result = strcmp(second_prop->name, name))) {
                atomic_fetch_add(&(H5P_mt_g.num_chksum_cols), 1);

                if (0 > name_cmp_result) {
                    next_prop = atomic_load(&(second_prop->next));

                    first_prop  = second_prop;
                    second_prop = next_prop.ptr;
                }
                else if (0 < name_cmp_result) {
                    assert(first_prop->chksum != chksum);

                    done = TRUE;
                }
            }

            sec_prop_ver = atomic_load(&(second_prop->create_version));

            if (sec_prop_ver <= prop_version) {
                done = TRUE;
            }
            else if (sec_prop_ver > prop_version) {
                next_prop = atomic_load(&(second_prop->next));

                first_prop  = second_prop;
                second_prop = next_prop.ptr;
            }
        }
        else if (second_prop->chksum > chksum) {
            done = TRUE;
        }
        else {
            next_prop = atomic_load(&(second_prop->next));

            first_prop  = second_prop;
            second_prop = next_prop.ptr;
        }

    } while (!done);

    assert(done);

    assert(first_prop->chksum <= chksum);
    assert(second_prop->chksum >= chksum);

    /**
     * Update the pointers passed into the function with the pointers for the
     * correct positions in the LFSLL, and the stats gathered while iterating.
     */
    *first_ptr_ptr  = first_prop;
    *second_ptr_ptr = second_prop;
    *thrd_cols_ptr += thrd_cols;
    *deletes_ptr += deletes;
    *nodes_visited_ptr += nodes_visited;

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__find_mod_point() */

/****************************************************************************************
 * Function:    H5P__mt_search_lfsll
 *
 *              Multithread version only function
 *
 * Purpose:     Searches a LFSLL to find the most recent version of property.
 *
 *              This function does this by calling H5P__get_next_valid_prop() and
 *              checking if the returned prop's chksum matches provided chksum. If it
 *              matches that property is returned. If it doesn't match
 *              H5P__get_next_valid_prop() is called again to get the next valid prop.
 *
 *              If H5P__get_next_valid_prop() returns NULL, then a property with the
 *              provided chksum either isn't in the lfsll or has been deleted.
 *
 *              NOTE: Even though this function searches for the most recent version of
 *              a property, we still need to compare to the version that was the
 *              curr_version when this search began. This is in case a new version of the
 *              property was inserted while this search was occuring.
 *
 *              NOTE: for more information on how the LFSLL is sorted see the description
 *              comment for structure H5P_mt_class_t in H5Ppkg_mt.h.
 *
 *
 * Return:      Success: Returns a pointer to the most recent version of the property,
 *                       that matches the provided chksum.
 *
 *              Failure: NULL  NOTE: this means for the provided version the prop is
 *                                   deleted, or the property isn't in the lfsll.
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__mt_search_lfsll(H5P_mt_prop_t *pl_head, int64_t chksum, const char *name, uint64_t version,
                     uint64_t *visited, bool *chksum_cols)
{
    H5P_mt_prop_t *prev_prop;    /* Prev prop in LFSLL */
    H5P_mt_prop_t *valid_prop;   /* next valid prop in LFSLL to compare chksums */
    bool           done = FALSE; /* Flag to break out of loop */

    H5P_mt_prop_t *ret_value = NULL;

    FUNC_ENTER_NOAPI_NOERR

    assert(pl_head);
    assert((atomic_load(&(pl_head->tag))) == H5P_MT_PROP_TAG);
    assert(pl_head->sentinel);

    prev_prop = pl_head;

    /* Iterate the lfsll searching for chksum */
    do {
        valid_prop = H5P__get_next_valid_prop(prev_prop, version, visited);

        /* If valid prop is NULL we searched the lfsll and there isn't a valid version */
        if (valid_prop == NULL) {
            done = TRUE;
        }
        /* If the valid prop chksum matches the target chksum */
        else if (valid_prop->chksum == chksum) {
            /* Double check the property by comparing the name */
            if (0 == strcmp(name, valid_prop->name)) {
                ret_value = valid_prop;

                done = TRUE;
            }
            else /* If the names don't match iterate again */
            {
                /* update global stats and mark TRUE for structure specific stats */
                atomic_fetch_add(&(H5P_mt_g.num_chksum_cols), 1);
                *chksum_cols = TRUE;

                prev_prop = valid_prop;
            }
        }
        /* Iterate LFSLL */
        else {
            prev_prop = valid_prop;
        }

    } while (!done);

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_search_lfsll() */

/****************************************************************************************
 * Function:    H5P__get_next_valid_prop
 *
 *              New function for multithread safe H5P
 *
 * Purpose:     Gets the next valid prop in the LFSLL.
 *
 *              This function is passed a known property in a LFSLL and it iterates the
 *              LFSLL till it finds a property with a different chksum. Then it calls
 *              H5P__find_valid_version() to ensure only the most current version of the
 *              property, in correllation with the version passed to the function, is the
 *              version that is selected.
 *
 *              If H5P__find_valid_version() returns NULL, then there is not a next valid
 *              property in the LFSLL.
 *
 *              If H5P__find_valid_version() returns a property, we must check the chksum
 *              of that property. If the chksums match then we return that property,
 *              however, if the chksums do not match, then no version of that property
 *              was valid, and we must now check for a valid version of the returned
 *              property.
 *
 *              NOTE: we return the property with a different chksum to prevent double
 *              iteration over the LFSLL. This way we simply have to compare chksums,
 *              and we know there wasn't a valid version and can start looking into the
 *              property for a valid verison. Otherwise, we'd have to iterate again over
 *              the LFSLL to get to that property.
 *
 *              NOTE: the uint64_t *visited sometimes will be NULL. This is due to this
 *              parameter is used to track the number of properties iterated in the LFSLL
 *              while searching for a specific property. However, some functions call
 *              this one with the intention of iterating every valid property in the
 *              list, which will throw off the statistics tracking lengths of searches.
 *
 *
 * Return:      Success: Returns a pointer to the next valid property in a LFSLL,
 *
 *              Failure: NULL if there is not a next valid.
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__get_next_valid_prop(H5P_mt_prop_t *prop, uint64_t version, uint64_t *visited)
{
    H5P_mt_prop_t     *check_prop;    /* Property we start the search from */
    H5P_mt_prop_t     *returned_prop; /* Prop returned to check if is valid */
    H5P_mt_prop_aptr_t next;          /* Prop next struct field for iterating */
    int64_t            curr_chksum;   /* Chksum of check prop */
    int64_t            next_chksum;   /* Chksum of the prop after check_prop */
    uint64_t           nodes_visited = 0;
    bool               done          = FALSE; /* Flag for exiting a loop */
    bool               iterate       = FALSE; /* Flag if iterating is necessary */

    H5P_mt_prop_t *ret_value = NULL;

    FUNC_ENTER_NOAPI_NOERR

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

    check_prop = prop;

    /* Iterate to find the next new chksum */
    do {
        curr_chksum = check_prop->chksum;

        next       = atomic_load(&(check_prop->next));
        check_prop = next.ptr;

        assert(check_prop);
        assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG ||
               atomic_load(&(prop->tag)) == H5P_MT_PROP_INVALID_TAG);

        next_chksum = check_prop->chksum;

        /**
         * Some of the calls for this function iterate the entire LFSLL, like comparing
         * two classes or lists, thus counting the number of visited nodes is unnecessary
         */
        if (visited) {
            nodes_visited++;
        }

        iterate = FALSE;

        /* If TRUE then not a new property and need to iterate again */
        if (curr_chksum == next_chksum) {
            iterate = TRUE;
        }

        /* If iterate is false check for a valid version of the property */
        if (!iterate) {
            do {
                /* If TRUE then we've reached the pos sentinel and can exit */
                if (next_chksum == LLONG_MAX) {
                    ret_value = NULL;
                    done      = TRUE;
                    break;
                }

                returned_prop = H5P__find_valid_version(check_prop, version, &nodes_visited);

                /* If NULL no version is valid, iterate for next new chksum */
                if (!returned_prop) {
                    iterate = TRUE;
                }
                /* If chksums equal then next_valid_prop is the valid version */
                else if (returned_prop->chksum == check_prop->chksum) {
                    ret_value = returned_prop;
                    done      = TRUE;
                }
                /**
                 * If chksums differ then all versions were iterated and none were valid,
                 * and returned_prop is the most recent version of the next prop to
                 * check.
                 */
                else {
                    check_prop  = returned_prop;
                    next_chksum = check_prop->chksum;
                }

            } while ((!iterate) && (!done));
        }

    } while (!done);

    if (visited) {
        *visited += nodes_visited;
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__get_next_valid_prop() */

/****************************************************************************************
 * Function:    H5P__find_valid_version
 *
 *              New function for multithread safe H5P
 *
 * Purpose:     Iterates the LFSLL to find the valid version of a given property in
 *              correlation to the version passed to this function.
 *
 *              This function calls H5P__is_valid() to check if a property is valid,
 *              and if H5P__is_valid() returns 0 this function returns that property.
 *
 *              If 1 is returned we iterate to the next version of that property.
 *              If the next property in the LFSLL has a different chksum, then we return
 *              that property to show there is not a valid version of the given prop.
 *
 *              NOTE: we return the property with a different chksum to prevent double
 *              iteration over the LFSLL. This way the calling function knows there is
 *              not a valid version of that property and if it needs it can now start
 *              from the next property instead of iterating again over the LFSLL to get
 *              it anyway.
 *
 *              If -1 is returned we know there are no valid versions of this property
 *              because it has a delete_version set for the version we are searching for,
 *              and we return NULL.
 *
 *
 * Return:      Success: Returns a pointer to the correct version, NULL if a
 *                       delete_version is set so we don't need to check the other
 *                       versions, or a pointer to the next property if no valid versions
 *
 *              Failure: Can not fail
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__find_valid_version(H5P_mt_prop_t *prop, uint64_t version, uint64_t *visited)
{
    H5P_mt_prop_t     *check_prop;            /* Property to find a valid version of */
    H5P_mt_prop_aptr_t next;                  /* Prop next struct field for iteratin */
    bool               done          = FALSE; /* Flag for exiting a loop */
    int32_t            valid         = 0;     /* Used to show if a prop is valid */
    uint64_t           nodes_visited = 0;

    H5P_mt_prop_t *ret_value;

    FUNC_ENTER_NOAPI_NOERR

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG ||
           atomic_load(&(prop->tag)) == H5P_MT_PROP_INVALID_TAG);

    check_prop = prop;

    /* Find a valid version of the property, if one exists */
    do {
        valid = H5P__is_valid(check_prop, version);

        /* check_prop is valid */
        if (valid == 0) {
            ret_value = check_prop;
            done      = TRUE;
        }
        /* No version of check_prop is valid */
        else if (valid == -1) {
            ret_value = NULL;
            done      = TRUE;
        }
        /* check_prop is not valid, iterate to the next version */
        else {
            next       = atomic_load(&(check_prop->next));
            check_prop = next.ptr;

            if (visited) {
                nodes_visited++;
            }

            assert(check_prop);
            assert(atomic_load(&(check_prop->tag)) == H5P_MT_PROP_TAG);

            /* If TRUE then we're on a different prop and no versions were valid */
            if (check_prop->chksum != prop->chksum) {
                ret_value = check_prop;
                done      = TRUE;
            }
        }

    } while (!done);

    if (visited) {
        *visited += nodes_visited;
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__find_valid_version() */

/****************************************************************************************
 * Function:    H5P__is_valid
 *
 *              New function for multithread safe H5P
 *
 * Purpose:     Checks if a given property is valid based on the provided version.
 *
 *              NOTE: we return an int32_t instead of herr_t or a bool because if the
 *              delete_version is greater than 0 and less than or equal to the version we
 *              are checking if valid for, we know that no other version is valid.
 *              Passing this information back could prevent further iteration in the
 *              LFSLL to check for a valid version when we already know there isn't one.
 *
 * Return:      Success: Returns an int32_t
 *
 *               0 = property is valid,
 *               1 = this version of the property is not valid
 *              -1 = The delete_version is set, so no version is valid.
 *
 *              Failure: Can not fail
 *
 ****************************************************************************************
 */
int32_t
H5P__is_valid(H5P_mt_prop_t *prop, uint64_t version)
{
    uint64_t create_ver; /* Create version of the property */
    uint64_t delete_ver; /* delete version of the property */

    int32_t ret_value = 0;

    FUNC_ENTER_NOAPI_NOERR

    create_ver = atomic_load(&(prop->create_version));
    delete_ver = atomic_load(&(prop->delete_version));

    /* If TRUE then prop is a valid version */
    if ((create_ver <= version) && ((delete_ver == 0) || delete_ver > version)) {
        ret_value = 0;
    }
    /* If TRUE then no version of this property is valid */
    else if ((create_ver <= version) && (delete_ver > 0) && (delete_ver <= version)) {
        ret_value = -1;
    }
    /* If TRUE this property is not valid, but another version might be */
    else {
        ret_value = 1;
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__is_valid() */

/****************************************************************************************
 * Function:    H5P_poke
 *
 *              Multithread safe version of H5P_poke().
 *
 * Purpose:     Internal routine to create a new version of a property with an updated
 *              value in a property list, without calling its set or get callback
 *              routines.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P_poke(H5P_mt_list_t *list, const char *name, void *value)
{
    H5P_mt_prop_t      *prop;
    H5P_mt_prop_value_t prop_value;
    uint64_t            curr_version  = 0;     /* Current version of list or class */
    uint64_t            next_version  = 0;     /* Next version of list or class */
    bool                inc_thrd_flag = FALSE; /* Flag to dec parent's thrd count */
    bool                ver_updated   = FALSE;

    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Increment thread count */
    if ((H5P__inc_thrd_count(list)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment list's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* update stats */
    atomic_fetch_add(&(list->H5P_poke__num_calls), 1);

    curr_version = atomic_load(&(list->curr_version));
    next_version = atomic_fetch_add(&(list->next_version), 1);
    ver_updated  = TRUE;

    /* Ensure another thread isn't modifying the class */
    if ((curr_version + 1) < next_version) {
        if (0 == (curr_version = H5P__mt_enforce_serialization(list, curr_version, next_version))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }

        curr_version = atomic_load(&(list->curr_version));
    }

    assert(curr_version + 1 == next_version);

    /* Searches for the property to poke */
    prop = H5P__mt_search__list(list, name, curr_version);

    /* If the property doesn't exist in the list, fail */
    if (!prop) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property doesn't exist");
    }

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

    prop_value = atomic_load(&(prop->value));

    /* Create a new version of the property that is a copy of it with the new value */
    if (0 > H5P__mt_ins_or_mod_prop__list(list, name, value, prop_value.size, FALSE, FALSE, FALSE,
                                          next_version, prop->create, prop->set, prop->get, prop->encode,
                                          prop->decode, prop->del, prop->copy, prop->cmp, prop->close)) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTOPERATE, FAIL,
                    "can't create new property version for the updated value");
    }

done:

    /* Update list's curr_version */
    if (ver_updated) {
        /* Update the list's current version */
        curr_version = atomic_fetch_add(&(list->curr_version), 1);
        assert(curr_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(list->curr_version)) > atomic_load(&(H5P_mt_g.max_list_version_number))) {
            atomic_store(&(H5P_mt_g.max_list_version_number), atomic_load(&(list->curr_version)));
        }

        /* Update global atomic plist version if list is a default plist */
        if (list->def_ver_ptr) {
            atomic_fetch_add((list->def_ver_ptr), 1);

            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_default_list_mods), 1);
        }
    }

    /* If the parent's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(list)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_poke() MT safe version */

/****************************************************************************************
 * Function:    H5P_set
 *
 *              Multithread safe version of H5P_set().
 *
 * Purpose:     Internal routine to create a new version of a property in a property list
 *              with an updated value, and calls the property's 'set' callback on the new
 *              property copy, if it has one.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P_set(H5P_mt_list_t *list, const char *name, const void *value)
{
    H5P_mt_prop_t      *prop;
    H5P_mt_prop_value_t prop_value;
    H5P_mt_prop_value_t new_value;
    H5P_mt_prop_value_t tmp_value     = {NULL, 0};
    const void         *prp_value     = NULL;
    uint64_t            curr_version  = 0;     /* Current version of list or class */
    uint64_t            next_version  = 0;     /* Next version of list or class */
    bool                inc_thrd_flag = FALSE; /* Flag to dec list's thrd count */
    bool                ver_updated   = FALSE;
    bool                done          = TRUE;

    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Increment thread count */
    if ((H5P__inc_thrd_count(list)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment list's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* update stats */
    atomic_fetch_add(&(list->H5P_set__num_calls), 1);

    curr_version = atomic_load(&(list->curr_version));
    next_version = atomic_fetch_add(&(list->next_version), 1);
    ver_updated  = TRUE;

    /* Ensure another thread isn't modifying the class */
    if ((curr_version + 1) < next_version) {
        if (0 == (curr_version = H5P__mt_enforce_serialization(list, curr_version, next_version))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }

        curr_version = atomic_load(&(list->curr_version));
    }

    assert(curr_version + 1 == next_version);

    /**
     * Callback function for testing, to pass the exact version of the
     * list the new verion of the property is being inserted at without
     * having to search the lkup_tbl and LFSLL to find the property and check.
     */
    if (H5P_mt_cb.ver_cb) {
        (H5P_mt_cb.ver_cb)(next_version);
    }

    /* Search for the property to modify */
    prop = H5P__mt_search__list(list, name, curr_version);

    /* If the property doesn't exist in the list, fail */
    if (!prop) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property doesn't exist");
    }
    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

    prop_value = atomic_load(&(prop->value));

    /* Create a new version of the property that is a copy of it */
    if (0 > H5P__mt_ins_or_mod_prop__list(list, name, (void *)prop_value.ptr, prop_value.size, FALSE, FALSE,
                                          FALSE, next_version, prop->create, prop->set, prop->get,
                                          prop->encode, prop->decode, prop->del, prop->copy, prop->cmp,
                                          prop->close)) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTOPERATE, FAIL, "can't operate on MT plist to set value");
    }

    /**
     * NOTE: Currently must search the list for new version of the property structure,
     * because H5P__mt_ins_or_mod_prop__main() returns SUCCESS/FAIL but may update that
     * so it returns the property and we don't have to search again.
     */
    curr_version = atomic_load(&(list->curr_version));

    prop = H5P__mt_search__list(list, name, next_version);

    if (!prop) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property doesn't exist");
    }

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

    prop_value = atomic_load(&(prop->value));

    /* If the property has a set callback call it */
    if (prop->set) {
        /* Create a tmp copy of the value, in case the callback fails */
        if (NULL == (tmp_value.ptr = H5MM_malloc(prop_value.size))) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "memory allocation failed temporary property value");
        }

        H5MM_memcpy(tmp_value.ptr, value, prop_value.size);

#if 1
        if (H5P__global_lock_prop_cb__set(prop, atomic_load(&(list->plist_id)), name, prop_value.size,
                                          tmp_value.ptr) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "can't set property value");
        }
#else
        /* Call the user's callback */
        if ((*(prop->set))(atomic_load(&(list->plist_id)), name, prop_value.size, tmp_value.ptr) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "can't set property value");
        }
#endif

        prp_value = tmp_value.ptr;
    }
    else
        prp_value = value;

    /* Free any previous value for the property */
    if (prop->del) {
        /* Call the user's 'delete' callback */
#if 1
        if (H5P__global_lock_prop_cb__del(prop, list->plist_id, name, prop_value.size, prop_value.ptr) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTFREE, FAIL, "can't release property value");
        }
#else
        if ((*(prop->del))(list->plist_id, name, prop_value.size, prop_value.ptr) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTFREE, FAIL, "can't release property value");
        }
#endif
    }

    /* memcpy into new buffer to atomically set */
    new_value.ptr  = prop_value.ptr;
    new_value.size = prop_value.size;

    H5MM_memcpy(new_value.ptr, prp_value, prop_value.size);

    /* Atomically set new value */
    do {
        /* Atomically update the value structure in the property */
        if (!atomic_compare_exchange_strong(&(prop->value), &prop_value, new_value)) {
            /* failed, update stats and try again */
            atomic_fetch_add(&(list->num_set_new_value_cols), 1);
        }
        else {
            /* success, update stats and continue */
            atomic_fetch_add(&(list->num_set_new_value), 1);

            done = TRUE;
        }

    } while (!done);

    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

done:

    if (tmp_value.ptr) {
        H5MM_xfree(tmp_value.ptr);
    }

    /* Update list's curr_version if ver_updated flag is TRUE */
    if (ver_updated) {
        /* Update the list's current version */
        curr_version = atomic_fetch_add(&(list->curr_version), 1);
        assert(curr_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(list->curr_version)) > atomic_load(&(H5P_mt_g.max_list_version_number))) {
            atomic_store(&(H5P_mt_g.max_list_version_number), atomic_load(&(list->curr_version)));
        }

        /* Update global atomic plist version if list is a default plist */
        if (list->def_ver_ptr) {
            atomic_fetch_add((list->def_ver_ptr), 1);

            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_default_list_mods), 1);
        }
    }

    /* If the list's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(list)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_set() MT safe version */

/****************************************************************************************
 * Function:    H5P__class_get
 *
 *              Multithread safe version of H5P__class_get().
 *
 * Purpose:     Internal routine to get a property's value from a property class
 *
 *              NOTE: currently this will always grab the most current version of the
 *              property in the class, but H5P__mt_search__class() can be used for
 *              searching a specific version of a class.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__class_get(H5P_genclass_t *pclass, const char *name, void *value)
{
    H5P_mt_prop_t      *prop; /* Temporary property pointer */
    H5P_mt_prop_value_t prop_value;
    uint64_t            version;
    bool                inc_thrd_flag = FALSE; /* Flag to dec class's thrd count */

    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(pclass);
    assert(name);
    assert(value);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(pclass)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment list's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    version = atomic_load(&(pclass->curr_version));

    /**
     * Callback function for testing, to pass the exact version
     * of the class the property is being searched for.
     */
    if (H5P_mt_cb.ver_cb) {
        (H5P_mt_cb.ver_cb)(version);
    }

    /* Search the pclass to find the most current version of the of the property */
    if (NULL == (prop = H5P__mt_search__class(pclass, name, version))) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property doesn't exist in pclass");
    }

    prop_value = atomic_load(&(prop->value));

    /* Ensure the size isn't 0 */
    if (0 == prop_value.size) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "property has zero size");
    }

    /* Copy the retrieved property value into the provided buffer */
    H5MM_memcpy(value, prop_value.ptr, prop_value.size);

done:

    /* If the class's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(pclass)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__class_get() */

/****************************************************************************************
 * Function:    H5P__class_set
 *
 *              Multithread safe version of H5P__class_set().
 *
 * Purpose:     Internal routine to create a new version of a property in a class with an
 *              updated value.
 *
 *              NOTE: The set callback routine registered for this property will _NOT_ be
 *              called. This routine is designed for internal library use only, and the
 *              previous value is overwritten, not released in any way.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__class_set(H5P_genclass_t *pclass, const char *name, void *value)
{
    H5P_mt_prop_t      *prop; /* Temporary property pointer */
    H5P_mt_prop_value_t prop_value;
    uint64_t            curr_version  = 0;     /* Current version of list or class */
    uint64_t            next_version  = 0;     /* Next version of list or class */
    bool                inc_thrd_flag = FALSE; /* Flag to dec list's thrd count */
    bool                ver_updated   = FALSE;

    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(pclass);
    assert(name);
    assert(value);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(pclass)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment pclass's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* update stats */
    atomic_fetch_add(&(pclass->H5P__class_set__num_calls), 1);

    curr_version = atomic_load(&(pclass->curr_version));
    next_version = atomic_fetch_add(&(pclass->next_version), 1);
    ver_updated  = TRUE;

    /* Ensure another thread isn't modifying the class */
    if ((curr_version + 1) < next_version) {
        if (0 == (curr_version = H5P__mt_enforce_serialization(pclass, curr_version, next_version))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }

        curr_version = atomic_load(&(pclass->curr_version));
    }

    assert(curr_version + 1 == next_version);

    /**
     * Callback function for testing, to pass the exact version of the
     * class the new verion of the property is being inserted at without
     * having to search the LFSLL to find the property and check.
     */
    if (H5P_mt_cb.ver_cb) {
        (H5P_mt_cb.ver_cb)(next_version);
    }

    /* Search the pclass to find the most current version of the of the property */
    if (NULL == (prop = H5P__mt_search__class(pclass, name, curr_version))) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property doesn't exist in pclass");
    }

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

    prop_value = atomic_load(&(prop->value));

    /* Ensure the size isn't 0 */
    if (0 == prop_value.size)
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "property has zero size");

    /* Create and insert the new property struct for the updated value */
    if ((H5P__mt_ins_or_mod_prop__class(pclass, name, value, prop_value.size, FALSE, next_version,
                                        prop->create, prop->set, prop->get, prop->encode, prop->decode,
                                        prop->del, prop->copy, prop->cmp, prop->close)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTOPERATE, FAIL,
                    "can't create new property version for the updated value");
    }

done:

    /* Update class's curr_version */
    if (ver_updated) {
        /* Update the class's current version */
        curr_version = atomic_fetch_add(&(pclass->curr_version), 1);
        assert(curr_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(pclass->curr_version)) > atomic_load(&(H5P_mt_g.max_class_version_number))) {
            atomic_store(&(H5P_mt_g.max_class_version_number), atomic_load(&(pclass->curr_version)));
        }
    }

    /* If the class's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(pclass)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__class_set() MT safe version */

/****************************************************************************************
 * Function:    H5P_exist_plist
 *
 *              Multithread safe version of H5P_exist_plist().
 *
 * Purpose:     Internal routine to query the existence of a property in a property list.
 *
 * Return:      Success: 1 if property exists in the plist, 0 if it doesn't
 *
 *              Failure: -1
 *
 ****************************************************************************************
 */
htri_t
H5P_exist_plist(H5P_mt_list_t *list, const char *name)
{
    H5P_mt_prop_t *prop;
    uint64_t       version;
    bool           inc_thrd_flag = FALSE; /* Flag to dec list's thrd count */

    htri_t ret_value = FAIL; /* return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(list);
    assert(name);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(list)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment list's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* If the context is initialized grab version from it */
    if (H5P_H5CX_INIT_g) {
        if (0 == (version = H5CX_get_plist_version(atomic_load(&(list->plist_id))))) {
            version = atomic_load(&(list->curr_version));
        }
        else {
            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_list_version_from_ctx), 1);
        }
    }
    else {
        version = atomic_load(&(list->curr_version));
    }

    /* Search the list for the property */
    if (NULL == (prop = H5P__mt_search__list(list, name, version))) {
        ret_value = FALSE;
    }
    else {
        ret_value = TRUE;
    }

done:

    /* If the list's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(list)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_exist_plist() MT safe version */

/****************************************************************************************
 * Function:    H5P__exist_pclass
 *
 *              Multithread safe version of H5P__exist_pclass().
 *
 * Purpose:     Internal routine to query the existence of a property in a property class
 *
 * Return:      Success: 1 if property exists in the plist, 0 if it doesn't
 *
 *              Failure: -1
 *
 ****************************************************************************************
 */
htri_t
H5P__exist_pclass(H5P_genclass_t *pclass, const char *name)
{
    H5P_mt_prop_t *prop;
    uint64_t       version;
    bool           inc_thrd_flag = FALSE; /* Flag to dec class's thrd count */

    htri_t ret_value = FAIL; /* return value */

    FUNC_ENTER_NOAPI(FAIL);

    assert(pclass);
    assert(name);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(pclass)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    version = atomic_load(&(pclass->curr_version));

    /* Search the list for the property */
    if (NULL == (prop = H5P__mt_search__class(pclass, name, version))) {
        ret_value = FALSE;
    }
    else {
        ret_value = TRUE;
    }

done:

    /* If the class's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(pclass)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__exist_pclass() MT safe version */

/****************************************************************************************
 * Function:    H5P__get_size_plist
 *
 *              Multithread safe version of H5P__get_size_plist().
 *
 * Purpose:     Internal routine to query the size of a property in a property list.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__get_size_plist(H5P_genplist_t *plist, const char *name, size_t *size)
{
    H5P_genprop_t      *prop; /* Temporary property pointer */
    H5P_mt_prop_value_t value;
    uint64_t            version;
    bool                inc_thrd_flag = FALSE; /* Flag to dec list's thrd count */

    herr_t ret_value = SUCCEED; /* return value */

    FUNC_ENTER_PACKAGE

    assert(plist);
    assert(name);
    assert(size);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(plist)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment list's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* If the context is initialized grab version from it */
    if (H5P_H5CX_INIT_g) {
        if (0 == (version = H5CX_get_plist_version(atomic_load(&(plist->plist_id))))) {
            version = atomic_load(&(plist->curr_version));
        }
        else {
            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_list_version_from_ctx), 1);
        }
    }
    else {
        version = atomic_load(&(plist->curr_version));
    }

    /* Get the property from the MT property list */
    if (NULL == (prop = H5P__mt_search__list(plist, name, version))) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "MT property isn't in MT property list");
    }

    /* Atomically grab the value of the MT property */
    value = atomic_load(&(prop->value));

    /* Return the size of the MT property*/
    *size = value.size;

done:

    /* If the list's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(plist)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__get_size_plist() */

/****************************************************************************************
 * Function:    H5P__get_size_pclass
 *
 *              Multithread safe version of H5P__get_size_pclass().
 *
 * Purpose:     Internal routine to query the size of a property in a property class.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__get_size_pclass(H5P_genclass_t *pclass, const char *name, size_t *size)
{
    H5P_genprop_t      *prop; /* Temporary property pointer */
    H5P_mt_prop_value_t value;
    uint64_t            version;
    bool                inc_thrd_flag = FALSE; /* Flag to dec list's thrd count */

    herr_t ret_value = SUCCEED; /* return value */

    FUNC_ENTER_PACKAGE

    assert(pclass);
    assert(name);
    assert(size);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(pclass)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment pclass's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    version = atomic_load(&(pclass->curr_version));

    /* Find property */
    if ((prop = H5P__mt_search__class(pclass, name, version)) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property doesn't exist");

    /* Atomically grab the value of the MT property */
    value = atomic_load(&(prop->value));

    /* Return the size of the MT property*/
    *size = value.size;

done:

    /* If the class's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(pclass)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__get_size_pclass() */

/****************************************************************************************
 * Function:    H5P__get_nprops_plist
 *
 *              Multithread safe version of H5P__get_nprops_plist().
 *
 * Purpose:     Internal routine to query the number of properties in a property list at
 *              the most recent version of the list.
 *
 *              NOTE: In the multithread safe H5P structure H5P_mt_list_t, the nprops
 *              field counts the number of valid properties in the lkup_tbl and LFSLL at
 *              the most recent. Due to the nature of multithreaded programming this
 *              value may be briefly incorrect during property additions or deletions.
 *              For a more accurate count, or for a count at an older version of the
 *              list, the lkup_tbl and LFSLL will need to be scanned at the version,
 *              which can be done via the multithread function H5P__count_nprops_plist().
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__get_nprops_plist(H5P_mt_list_t *plist, size_t *nprops)
{
    bool   inc_thrd_flag = FALSE;   /* Flag to dec list's thrd count */
    herr_t ret_value     = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(plist);
    assert(nprops);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(plist)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment list's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    *nprops = atomic_load(&(plist->nprops));

done:

    /* If the list's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(plist)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__get_nprops_plist() */

/****************************************************************************************
 * Function:    H5P__count_nprops_plist
 *
 *              New function for multithread safe H5P.
 *
 * Purpose:     Internal routine that iterates the list's lkup_tbl and LFSLL to get an
 *              accurate count of the number of valid properties in a property list at a
 *              specified version.
 *
 *              NOTE: If the parameter version is 0, the most recent version of the list
 *              is used.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__count_nprops_plist(H5P_mt_list_t *list, size_t *_nprops, uint64_t version)
{
    H5P_mt_prop_t             *prop = NULL;
    H5P_mt_list_table_entry_t *entry;
    bool                       inc_thrd_flag = FALSE; /* Flag to dec thrd count */
    bool                       base_flag     = FALSE;
    size_t                     nprops        = 0;

    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(list);
    assert(_nprops);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(list)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* If parameter version is 0 get curr_version */
    if (version == 0) {
        version = atomic_load(&(list->curr_version));
    }

    /* Iterate the lkup_tbl and count valid props */
    for (size_t i = 0; i < list->nprops_inherited; i++) {
        entry = &list->lkup_tbl[i];

        if ((prop = H5P__mt_entry_find_version(entry, version, &base_flag)) != NULL) {
            nprops++;
        }
    }

    /* If the LFSLL has more than sentinel nodes, iterate it and count valid props */
    if (atomic_load(&(list->phys_pl_len)) > 2) {
        prop = list->pl_head;
        assert(prop);
        assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
        assert(prop->sentinel);

        do {
            if ((prop = H5P__get_next_valid_prop(prop, version, NULL)) != NULL) {
                if (!prop->in_lkup_tbl) {
                    nprops++;
                }
            }

        } while (prop);
    }

    *_nprops = nprops;

done:

    /* If the thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(list)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__count_nprops_plist() */

/****************************************************************************************
 * Function:    H5P_get_nprops_pclass
 *
 *              Multithread safe version of H5P_get_nprops_pclass().
 *
 * Purpose:     Internal routine to query the number of properties in a property class at
 *              the most recent version of the class.
 *
 *              If parameter recurse is TRUE log_pl_len is used, which tracks the total
 *              number of valid properties at the most recent version, which the original
 *              H5P_genclass_t would have to iterate up the inheritance tree to count,
 *              but H5P_mt_class_t inherits its parent's valid properties at the version
 *              it is derived from.
 *
 *              If recurse is FALSE nprops_added is used, which tracks the number of
 *              valid properties that have been added or modified in this class
 *              specifically, at the most recent version.
 *
 *              NOTE: public API H5Pget_nprops always calls this with recurse as FALSE.
 *
 *              NOTE: Due to the nature of multithreaded programming these values may be
 *              briefly incorrect during property additions or deletions. For a more
 *              accurate count, or for a count at an older version of the class, the
 *              LFSLL will need to be scanned at the version, which can be done via the
 *              multithread function H5P__count_nprops_pclass().
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P_get_nprops_pclass(H5P_mt_class_t *pclass, size_t *nprops, hbool_t recurse)
{
    bool   inc_thrd_flag = FALSE;   /* Flag to dec thrd count */
    herr_t ret_value     = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(pclass);
    assert(nprops);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(pclass)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    if (recurse) {
        *nprops = atomic_load(&(pclass->log_pl_len));
    }
    else {
        *nprops = atomic_load(&(pclass->nprops_added));
    }

done:

    /* If the thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(pclass)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_get_nprops_pclass() */

/****************************************************************************************
 * Function:    H5P__count_nprops_pclass
 *
 *              New function for the multithread safe H5P
 *
 * Purpose:     Internal routine that iterates the class's LFSLL to get an accurate count
 *              of the number of valid properties in a class at a specified version.
 *
 *              NOTE: If the parameter version is 0, the most recent version of the class
 *              is used.
 *
 *              If parameter recurse is TRUE log_pl_len is used, which tracks the total
 *              number of valid properties at the most recent version, which the original
 *              H5P_genclass_t would have to iterate up the inheritance tree to count,
 *              but H5P_mt_class_t inherits its parent's valid properties at the version
 *              it is derived from.
 *
 *              If recurse is FALSE nprops_added is used, which tracks the number of
 *              valid properties that have been added or modified in this class
 *              specifically, at the most recent version.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__count_nprops_pclass(H5P_mt_class_t *class, size_t *_nprops, uint64_t version, hbool_t recurse)
{
    H5P_mt_prop_t *prop          = NULL;
    bool           inc_thrd_flag = FALSE; /* Flag to dec class's thrd count */
    size_t         nprops        = 0;

    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(class);
    assert(nprops);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(class)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* If parameter version is 0 get curr_version */
    if (version == 0) {
        version = atomic_load(&(class->curr_version));
    }

    if (recurse) {
        /* If there are more properties than just the sentinel nodes */
        if (atomic_load(&(class->phys_pl_len)) > 2) {
            prop = class->pl_head;
            assert(prop);
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert(prop->sentinel);

            /* Iterate the LFSLL */
            do {
                if ((prop = H5P__get_next_valid_prop(prop, version, NULL)) != NULL) {
                    assert(prop);
                    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

                    /* Increment count for valid props */
                    nprops++;
                }

            } while (prop);

        } /* end if ( atomic_load(&(class->phys_pl_len)) > 2 ) */
    }
    else {
        /* If there are more properties than just the sentinel nodes */
        if (atomic_load(&(class->phys_pl_len)) > 2) {
            prop = class->pl_head;
            assert(prop);
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert(prop->sentinel);

            /* Iterate the LFSLL */
            do {
                if ((prop = H5P__get_next_valid_prop(prop, version, NULL)) != NULL) {
                    /**
                     * If the create_version of this property is 1, then it is an
                     * inherited property and can't be counted since recurse is FALSE.
                     */
                    if (1 < atomic_load(&(prop->create_version))) {
                        nprops++;
                    }
                }

            } while (prop);

        } /* end if ( atomic_load(&(class->phys_pl_len)) > 2 ) */
    }

    *_nprops = nprops;

done:

    /* If the class's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(class)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__count_nprops_pclass() */

/****************************************************************************************
 * Function:    H5P__cmp_class
 *
 *              Multithread safe version of H5P__cmp_class().
 *
 * Purpose:     Internal routine to compare two property classes at their most current
 *              versions
 *
 *              This function just gets the current versions of the two classes and
 *              calls H5P__mt_cmp_class() to perform the actual comparison.
 *
 * Return:      Success: 0 if equal, 1 if not equal
 *
 *              Failure: -1
 *
 ****************************************************************************************
 */
int
H5P__cmp_class(H5P_genclass_t *pclass1, H5P_genclass_t *pclass2)
{
    uint64_t pclass1_ver;
    uint64_t pclass2_ver;
    int      ret_value = 1;

    FUNC_ENTER_PACKAGE

    pclass1_ver = atomic_load(&(pclass1->curr_version));
    pclass2_ver = atomic_load(&(pclass2->curr_version));

    if (0 > (ret_value = H5P__mt_cmp_class(pclass1, pclass1_ver, pclass2, pclass2_ver))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOMPARE, FAIL, "can't compare MT property classes");
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__cmp_class() MT safe version */

/****************************************************************************************
 * Function:    H5P__mt_cmp_class()
 *
 *              New function for multithread safe H5P.
 *
 * Purpose:     Compares two classes (H5P_mt_class_t) and determines if they are equal at
 *              the specified versions.
 *
 *              NOTE: This function is kept as a separate function from H5P__cmp_class()
 *              because it allows two classes to be compared at versions specified by the
 *              user, which is useful for testing and debugging, and may be useful to
 *              other functions in future iterations of multithread H5P.
 *
 * Details:     First each class has it's thread count incremented, and the current
 *              version grabbed from both.
 *
 *              NOTE: The versions cannot be compared because a copy of a class could
 *              have a different version from the original.
 *
 *              The classes more simple fields are compared to ensure they are equal,
 *              the parent info and pointers, class name and type, creation and close
 *              callback functions and data.
 *
 *              NOTE: The nprops_added, log_pl_len, and phys_pl_len fields cannot be
 *              compared. If one of the classes is a copy, any properties added to
 *              original before being copied, will not be counted as nprops_added for the
 *              copy. A copy will also have a different phys_pl_len, due to any non-valid
 *              properties at the version being copied will not be added into the copy's
 *              LFSLL. log_pl_len could also differ due to log_pl_len tracking the number
 *              of valid properties in the most current version. Meaning that if one of
 *              the classes gets updated in a way that changes log_pl_len by another
 *              thread they will no longer match.
 *
 *              The LFSLLs are then iterated and each valid property is compared for the
 *              version of the class being compared. Due to how the LFSLLs are sorted
 *              any difference between the properties means there is one property in
 *              one class while not being in the other, or at least a property was updated
 *              in one and not the other.
 *              This is done by calling H5P__get_next_valid_prop() on both classes and
 *              H5P__mt_prop_cmp() on returned properties.
 *
 *              Lastly the thrd counts are decrecmented on the classes.
 *
 *
 * Return:      Success: 0 means the two parameters are equal
 *                       1 means the two parameters are not equal
 *
 *              Failure: -1
 ****************************************************************************************
 */
int32_t
H5P__mt_cmp_class(H5P_mt_class_t *class1, uint64_t version1, H5P_mt_class_t *class2, uint64_t version2)
{
    H5P_mt_prop_t *prev_prop1 = NULL;
    H5P_mt_prop_t *prev_prop2 = NULL;
    H5P_mt_prop_t *valid_prop1;
    H5P_mt_prop_t *valid_prop2;
    int32_t        cmp_result;
    bool           inc_thrd_flag_1 = FALSE;
    bool           inc_thrd_flag_2 = FALSE;

    int32_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(class1);
    assert(class2);
    assert(atomic_load(&(class1->tag)) == H5P_MT_CLASS_TAG);
    assert(atomic_load(&(class2->tag)) == H5P_MT_CLASS_TAG);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(class1)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, (-1), "Couldn't increment class's thread count.");
    }

    inc_thrd_flag_1 = TRUE;

    if ((H5P__inc_thrd_count(class2)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, (-1), "Couldn't increment class's thread count.");
    }

    inc_thrd_flag_2 = TRUE;

    /* Check whether they have the same fields */
    if (class1->parent_id != class2->parent_id)
        HGOTO_DONE(1);
    if (class1->parent_ptr != class2->parent_ptr)
        HGOTO_DONE(1);
    if (class1->parent_version != class2->parent_version)
        HGOTO_DONE(1);
    if ((cmp_result = strcmp(class1->name, class2->name)) != 0)
        HGOTO_DONE(1);
    if (class1->type != class2->type)
        HGOTO_DONE(1);

    /* Check whether they have creation callback functions & data */
    if (class1->create_func == NULL && class2->create_func != NULL)
        HGOTO_DONE(1);
    if (class1->create_func != NULL && class2->create_func == NULL)
        HGOTO_DONE(1);
    if (class1->create_func != class2->create_func)
        HGOTO_DONE(1);
    if (class1->create_data < class2->create_data)
        HGOTO_DONE(1);
    if (class1->create_data > class2->create_data)
        HGOTO_DONE(1);

    /* Check whether they have close callback functions & data */
    if (class1->close_func == NULL && class2->close_func != NULL)
        HGOTO_DONE(1);
    if (class1->close_func != NULL && class2->close_func == NULL)
        HGOTO_DONE(1);
    if (class1->close_func != class2->close_func)
        HGOTO_DONE(1);
    if (class1->close_data < class2->close_data)
        HGOTO_DONE(1);
    if (class1->close_data > class2->close_data)
        HGOTO_DONE(1);

    /* Compare the properties in the LFSLLs */
    prev_prop1 = class1->pl_head;
    prev_prop2 = class2->pl_head;

    assert(prev_prop1);
    assert(prev_prop2);
    assert(atomic_load(&(prev_prop1->tag)) == H5P_MT_PROP_TAG);
    assert(atomic_load(&(prev_prop2->tag)) == H5P_MT_PROP_TAG);

    if ((!prev_prop1->sentinel) || (!prev_prop2->sentinel))
        HGOTO_DONE(1);

    do {
        /* Get the next valid props in the LFSLLs to compare */
        valid_prop1 = H5P__get_next_valid_prop(prev_prop1, version1, NULL);
        valid_prop2 = H5P__get_next_valid_prop(prev_prop2, version2, NULL);

        /* If only one of the props is NULL return 1, else compare the props */
        if (valid_prop1 == NULL && valid_prop2 != NULL)
            HGOTO_DONE(1);
        else if (valid_prop1 != NULL && valid_prop2 == NULL)
            HGOTO_DONE(1);
        else if (valid_prop1 && valid_prop2) {
            /* Compare the two properties */
            if (0 != H5P__mt_prop_cmp(valid_prop1, valid_prop2))
                HGOTO_DONE(1);
        }

        /* Update prev_props */
        prev_prop1 = valid_prop1;
        prev_prop2 = valid_prop2;

    } while (valid_prop1);

done:

    /* If inc_thrd_flags are TRUE decrement the thrd.count for that class */
    if (inc_thrd_flag_1) {
        if (0 > H5P__dec_thrd_count(class1)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, (-1), "Failure to decrement thrd_count.");
        }
    }
    if (inc_thrd_flag_2) {
        if (0 > H5P__dec_thrd_count(class2)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, (-1), "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_cmp_class() */

/****************************************************************************************
 * Function:    H5P__cmp_plist
 *
 *              Multithread safe version of H5P__cmp_plist().
 *
 * Purpose:     Internal routine to compare two property lists at their most current
 *              versions
 *
 *              This function just gets the current versions of the two lists and
 *              calls H5P__mt_cmp_list() to perform the actual comparison.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__cmp_plist(H5P_genplist_t *plist1, H5P_genplist_t *plist2, int *cmp_ret)
{
    uint64_t plist1_ver;
    uint64_t plist2_ver;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    plist1_ver = atomic_load(&(plist1->curr_version));
    plist2_ver = atomic_load(&(plist2->curr_version));

    /* Multithread safe function to compare the lists */
    if (0 > (*cmp_ret = H5P__mt_cmp_list(plist1, plist1_ver, plist2, plist2_ver))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOMPARE, FAIL, "can't compare MT property lists");
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__cmp_plist() MT safe version */

/****************************************************************************************
 * Function:    H5P__mt_cmp_list()
 *
 *              New function for multithread safe H5P.
 *
 * Purpose:     Compares two lists (H5P_mt_list_t) and determines if they are equal at
 *              the specified versions.
 *
 *              NOTE: This function is kept as a separate function from H5P__cmp_plist()
 *              because it allows two lists to be compared at versions specified by the
 *              user, which is useful for testing and debugging, and may be useful to
 *              other functions in future iterations of multithread H5P.
 *
 * Details:     First each list has it's thread count incremented, and the current
 *              version grabbed from both.
 *
 *              NOTE: The versions cannot be compared because a copy of a list could
 *              have a different version number from the original.
 *
 *              The lists more simple fields are compared to ensure they are equal
 *              (the parent info and pointers, nprops_inherited, and class_init).
 *
 *              NOTE: The nprops_added, nprops, log_pl_len, and phys_pl_len fields cannot
 *              be compared. A copy will have a different phys_pl_len, due to any
 *              non-valid properties at the version being copied will not be added into
 *              the copy's LFSLL, causing different phys_pl_lens. Nprops, nprops_added,
 *              and log_pl_len fields could differ due to them tracking their values at
 *              the most recent version of the list, so if another thread makes a change
 *              to one of the lists the fields will not match.
 *
 *              The lkup_tbls are compared by iterating the entries and comparing the
 *              chksums and names of each. If they match the valid properties for the
 *              version being compared is retrieved from each (either the base being
 *              grabbed from the parent or curr being grabbed from the LFSLL) and are
 *              compared.
 *
 *              If the lkup_tbls match, the LFSLLs are then iterated and each valid
 *              property is compared for the version of the lists being compared. Due to
 *              how the LFSLLs are sorted any difference between the properties means
 *              there is one property in one LFSLL while not being in the other, or at
 *              least a property was updated in one and not the other.
 *              This is done by calling H5P__mt_next_prop_to_cmp() on both lists, which
 *              just calls H5P__get_next_valid_prop() but then checks if the in_lkup_tbl
 *              flag is TRUE, if so loops to call H5P__get_next_valid_prop() again. This
 *              is because all properties in the lkup_tbl have alread been compared.
 *              H5P__mt_prop_cmp() is called on the returned properties.
 *
 *              Lastly the thrd counts are decrement on the lists.
 *
 *
 * Return:      Success: 0 means the two parameters are equal
 *                       1 means the two parameters are not equal
 *
 *              Failure: -1
 ****************************************************************************************
 */
int32_t
H5P__mt_cmp_list(H5P_mt_list_t *list1, uint64_t version1, H5P_mt_list_t *list2, uint64_t version2)
{
    H5P_mt_list_table_entry_t *entry1;
    H5P_mt_list_table_entry_t *entry2;
    H5P_mt_prop_t             *prev_prop1;
    H5P_mt_prop_t             *prev_prop2;
    H5P_mt_prop_t             *valid_prop1;
    H5P_mt_prop_t             *valid_prop2;
    int32_t                    cmp_result      = 0;
    bool                       inc_thrd_flag_1 = FALSE;
    bool                       inc_thrd_flag_2 = FALSE;
    bool                       base_flag_1     = FALSE;
    bool                       base_flag_2     = FALSE;

    int32_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(list1);
    assert(list2);
    assert(atomic_load(&(list1->tag)) == H5P_MT_LIST_TAG);
    assert(atomic_load(&(list2->tag)) == H5P_MT_LIST_TAG);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(list1)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, (-1), "Couldn't increment class's thread count.");
    }

    inc_thrd_flag_1 = TRUE;

    if ((H5P__inc_thrd_count(list2)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, (-1), "Couldn't increment class's thread count.");
    }

    inc_thrd_flag_2 = TRUE;

    /* Check whether they have the same fields */
    if (list1->pclass_id != list2->pclass_id)
        HGOTO_DONE(1);
    if (list1->pclass_ptr != list2->pclass_ptr)
        HGOTO_DONE(1);
    if (list1->nprops_inherited != list2->nprops_inherited)
        HGOTO_DONE(1);
    if (list1->class_init != list2->class_init)
        HGOTO_DONE(1);

    /* Check if the properties in the lkup_tbls are the same */

    for (uint32_t idx = 0; idx < list1->nprops_inherited; idx++) {
        entry1 = &list1->lkup_tbl[idx];
        entry2 = &list2->lkup_tbl[idx];

        /* Compare chksum, name, and base_delete_version fields */
        if (entry1->chksum != entry2->chksum)
            HGOTO_DONE(1);
        if ((cmp_result = HDstrcmp(entry1->name, entry2->name)) != 0)
            HGOTO_DONE(1);

        valid_prop1 = H5P__mt_entry_find_version(entry1, version1, &base_flag_1);
        valid_prop2 = H5P__mt_entry_find_version(entry2, version2, &base_flag_2);

        /* If only one of the props is NULL return 1, else compare the props */
        if (valid_prop1 == NULL && valid_prop2 != NULL)
            HGOTO_DONE(1);
        else if (valid_prop1 != NULL && valid_prop2 == NULL)
            HGOTO_DONE(1);
        else if (valid_prop1 && valid_prop2) {
            /* Compare the two properties */
            if (0 != H5P__mt_prop_cmp(valid_prop1, valid_prop2))
                HGOTO_DONE(1);
        }

    } /* end for ( uint32_t idx = 0; idx < list1->nprops_inherited; idx++ ) */

    /* Compare the properties in the LFSLLs */
    prev_prop1 = list1->pl_head;
    prev_prop2 = list2->pl_head;

    assert(prev_prop1);
    assert(prev_prop2);
    assert(atomic_load(&(prev_prop1->tag)) == H5P_MT_PROP_TAG);
    assert(atomic_load(&(prev_prop2->tag)) == H5P_MT_PROP_TAG);

    if ((!prev_prop1->sentinel) || (!prev_prop2->sentinel))
        HGOTO_DONE(1);

    do {
        /**
         * Get the next valid props in the LFSLLs to compare.
         * Skips props with the in_lkup_tbl flag set to TRUE,
         * since they were compared when comparing lkup_tbls.
         */
        valid_prop1 = H5P__mt_next_prop_to_cmp(prev_prop1, version1);
        valid_prop2 = H5P__mt_next_prop_to_cmp(prev_prop2, version2);

        /* If only one of the props is NULL return 1, else compare the props */
        if (valid_prop1 == NULL && valid_prop2 != NULL)
            HGOTO_DONE(1);
        else if (valid_prop1 != NULL && valid_prop2 == NULL)
            HGOTO_DONE(1);
        else if (valid_prop1 && valid_prop2) {
            /* Compare the two properties */
            if (0 != H5P__mt_prop_cmp(valid_prop1, valid_prop2))
                HGOTO_DONE(1);
        }

        /* Update prev_props */
        prev_prop1 = valid_prop1;
        prev_prop2 = valid_prop2;

    } while (valid_prop1);

done:

    /* If inc_thrd_flags are TRUE decrement the thrd.count for that list */
    if (inc_thrd_flag_1) {
        if (0 > H5P__dec_thrd_count(list1)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, (-1), "Failure to decrement thrd_count.");
        }
    }
    if (inc_thrd_flag_2) {
        if (0 > H5P__dec_thrd_count(list2)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, (-1), "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_cmp_list() */

/****************************************************************************************
 * Function:    H5P__mt_prop_cmp()
 *
 *              New function for the multithread H5P
 *
 * Purpose:     Checks if the two properties are the same or are different properties
 *
 *
 * Return:      Success:  0 the props are the same,
 *                        1 the props are different
 *
 *              Failure: -1
 ****************************************************************************************
 */
int32_t
H5P__mt_prop_cmp(H5P_mt_prop_t *prop1, H5P_mt_prop_t *prop2)
{
    H5P_mt_prop_value_t value1; /* prop1's value */
    H5P_mt_prop_value_t value2; /* prop2's value */
    int                 cmp_value;

    int32_t ret_value = 0;

    FUNC_ENTER_PACKAGE

    assert(prop1);
    assert((atomic_load(&(prop1->tag)) == H5P_MT_PROP_TAG) ||
           (atomic_load(&(prop1->tag)) == H5P_MT_PROP_VALID_ONFL_TAG));

    assert(prop2);
    assert((atomic_load(&(prop2->tag)) == H5P_MT_PROP_TAG) ||
           (atomic_load(&(prop2->tag)) == H5P_MT_PROP_VALID_ONFL_TAG));

    if (prop1->chksum != prop2->chksum) {
        HGOTO_DONE(1);
    }

    if (0 != strcmp(prop1->name, prop2->name)) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, (int32_t)(-1),
                    "Two different properties have the same checksum.");
    }

    /* Comparing the two props' callbacks */
    if (prop1->create == NULL && prop2->create != NULL)
        HGOTO_DONE(1);
    if (prop1->create != NULL && prop2->create == NULL)
        HGOTO_DONE(1);
    if (prop1->create != prop2->create)
        HGOTO_DONE(1);

    if (prop1->set == NULL && prop2->set != NULL)
        HGOTO_DONE(1);
    if (prop1->set != NULL && prop2->set == NULL)
        HGOTO_DONE(1);
    if (prop1->set != prop2->set)
        HGOTO_DONE(1);

    if (prop1->get == NULL && prop2->get != NULL)
        HGOTO_DONE(1);
    if (prop1->get != NULL && prop2->get == NULL)
        HGOTO_DONE(1);
    if (prop1->get != prop2->get)
        HGOTO_DONE(1);

    if (prop1->encode == NULL && prop2->encode != NULL)
        HGOTO_DONE(1);
    if (prop1->encode != NULL && prop2->encode == NULL)
        HGOTO_DONE(1);
    if (prop1->encode != prop2->encode)
        HGOTO_DONE(1);

    if (prop1->decode == NULL && prop2->decode != NULL)
        HGOTO_DONE(1);
    if (prop1->decode != NULL && prop2->decode == NULL)
        HGOTO_DONE(1);
    if (prop1->decode != prop2->decode)
        HGOTO_DONE(1);

    if (prop1->del == NULL && prop2->del != NULL)
        HGOTO_DONE(1);
    if (prop1->del != NULL && prop2->del == NULL)
        HGOTO_DONE(1);
    if (prop1->del != prop2->del)
        HGOTO_DONE(1);

    if (prop1->copy == NULL && prop2->copy != NULL)
        HGOTO_DONE(1);
    if (prop1->copy != NULL && prop2->copy == NULL)
        HGOTO_DONE(1);
    if (prop1->copy != prop2->copy)
        HGOTO_DONE(1);

    if (prop1->cmp == NULL && prop2->cmp != NULL)
        HGOTO_DONE(1);
    if (prop1->cmp != NULL && prop2->cmp == NULL)
        HGOTO_DONE(1);
    if (prop1->cmp != prop2->cmp)
        HGOTO_DONE(1);

    if (prop1->close == NULL && prop2->close != NULL)
        HGOTO_DONE(1);
    if (prop1->close != NULL && prop2->close == NULL)
        HGOTO_DONE(1);
    if (prop1->close != prop2->close)
        HGOTO_DONE(1);

    value1 = atomic_load(&(prop1->value));
    value2 = atomic_load(&(prop2->value));

    /* Compare value size and pointer */
    if (value1.size != value2.size)
        HGOTO_DONE(1);
    if (value1.ptr == NULL && value2.ptr != NULL)
        HGOTO_DONE(1);
    if (value1.ptr != NULL && value2.ptr == NULL)
        HGOTO_DONE(1);
    if (value1.ptr) {
        /* Call the compare callback */
#if 1
        if ((cmp_value = H5P__global_lock_prop_cb__cmp(prop1, value1.ptr, value2.ptr, value1.size)) != 0)
            HGOTO_DONE(cmp_value);
#else
        if ((cmp_value = prop1->cmp(value1.ptr, value2.ptr, value1.size)) != 0)
            HGOTO_DONE(cmp_value);
#endif
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_prop_cmp() */

/****************************************************************************************
 * Function:    H5P__prop_cmp_test()
 *
 *              New function for the multithread H5P
 *
 * Purpose:     Checks if the two properties are the same or are different properties
 *
 *              NOTE: This function is the exact same as the above function
 *              H5P__mt_prop_cmp(), with the exception that of not grabbing the global
 *              mutex prior to calling the property cmp callback.
 *
 *              This function exists for testing purposes, to quickly compare many
 *              properties without the overhead of grabbing the global mutex when it
 *              isn't needed for the specific test.
 *
 *
 * Return:      Success:  0 the props are the same,
 *                        1 the props are different
 *
 *              Failure: -1
 ****************************************************************************************
 */
int32_t
H5P__prop_cmp_test(H5P_mt_prop_t *prop1, H5P_mt_prop_t *prop2)
{
    H5P_mt_prop_value_t value1; /* prop1's value */
    H5P_mt_prop_value_t value2; /* prop2's value */
    int                 cmp_value;

    int32_t ret_value = 0;

    FUNC_ENTER_PACKAGE

    assert(prop1);
    assert((atomic_load(&(prop1->tag)) == H5P_MT_PROP_TAG) ||
           (atomic_load(&(prop1->tag)) == H5P_MT_PROP_VALID_ONFL_TAG));

    assert(prop2);
    assert((atomic_load(&(prop2->tag)) == H5P_MT_PROP_TAG) ||
           (atomic_load(&(prop2->tag)) == H5P_MT_PROP_VALID_ONFL_TAG));

    if (prop1->chksum != prop2->chksum) {
        HGOTO_DONE(1);
    }

    if (0 != strcmp(prop1->name, prop2->name)) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, (int32_t)(-1),
                    "Two different properties have the same checksum.");
    }

    /* Comparing the two props' callbacks */
    if (prop1->create == NULL && prop2->create != NULL)
        HGOTO_DONE(1);
    if (prop1->create != NULL && prop2->create == NULL)
        HGOTO_DONE(1);
    if (prop1->create != prop2->create)
        HGOTO_DONE(1);

    if (prop1->set == NULL && prop2->set != NULL)
        HGOTO_DONE(1);
    if (prop1->set != NULL && prop2->set == NULL)
        HGOTO_DONE(1);
    if (prop1->set != prop2->set)
        HGOTO_DONE(1);

    if (prop1->get == NULL && prop2->get != NULL)
        HGOTO_DONE(1);
    if (prop1->get != NULL && prop2->get == NULL)
        HGOTO_DONE(1);
    if (prop1->get != prop2->get)
        HGOTO_DONE(1);

    if (prop1->encode == NULL && prop2->encode != NULL)
        HGOTO_DONE(1);
    if (prop1->encode != NULL && prop2->encode == NULL)
        HGOTO_DONE(1);
    if (prop1->encode != prop2->encode)
        HGOTO_DONE(1);

    if (prop1->decode == NULL && prop2->decode != NULL)
        HGOTO_DONE(1);
    if (prop1->decode != NULL && prop2->decode == NULL)
        HGOTO_DONE(1);
    if (prop1->decode != prop2->decode)
        HGOTO_DONE(1);

    if (prop1->del == NULL && prop2->del != NULL)
        HGOTO_DONE(1);
    if (prop1->del != NULL && prop2->del == NULL)
        HGOTO_DONE(1);
    if (prop1->del != prop2->del)
        HGOTO_DONE(1);

    if (prop1->copy == NULL && prop2->copy != NULL)
        HGOTO_DONE(1);
    if (prop1->copy != NULL && prop2->copy == NULL)
        HGOTO_DONE(1);
    if (prop1->copy != prop2->copy)
        HGOTO_DONE(1);

    if (prop1->cmp == NULL && prop2->cmp != NULL)
        HGOTO_DONE(1);
    if (prop1->cmp != NULL && prop2->cmp == NULL)
        HGOTO_DONE(1);
    if (prop1->cmp != prop2->cmp)
        HGOTO_DONE(1);

    if (prop1->close == NULL && prop2->close != NULL)
        HGOTO_DONE(1);
    if (prop1->close != NULL && prop2->close == NULL)
        HGOTO_DONE(1);
    if (prop1->close != prop2->close)
        HGOTO_DONE(1);

    value1 = atomic_load(&(prop1->value));
    value2 = atomic_load(&(prop2->value));

    /* Compare value size and pointer */
    if (value1.size != value2.size)
        HGOTO_DONE(1);
    if (value1.ptr == NULL && value2.ptr != NULL)
        HGOTO_DONE(1);
    if (value1.ptr != NULL && value2.ptr == NULL)
        HGOTO_DONE(1);
    if (value1.ptr) {
        /* Call the compare callback */
        if ((cmp_value = prop1->cmp(value1.ptr, value2.ptr, value1.size)) != 0)
            HGOTO_DONE(cmp_value);
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__prop_cmp_test() */

/****************************************************************************************
 * Function:    H5P_class_isa
 *
 *              Multithread safe version of H5P_class_isa().
 *
 * Purpose:     Internal routine to query whether a property class is the same another
 *              class, or that class's parent, at their most current versions.
 *
 *              Gets the current versions of both classes and calls H5P__mt_cmp_class()
 *              which actually does the comparison.
 *
 *              If they are not equal the parent class of pclass1 is retrieved, and
 *              pclass2 is compared to that class. This will iterate up the inheritance
 *              tree until either two classes are equal or a class is reached that
 *              doesn't have a parent.
 *
 *              NOTE: the return values of this function and H5P__mt_cmp_class() were
 *              kept the same as the original H5P_class_isa() and H5P__cmp_class(),
 *              however that means they are opposite of each other. Where H5P_class_isa()
 *              returns 1 if the classes are equal (or if a parent of pclass1 is equal to
 *              pclass2), while H5P__cmp_class() and the multithread H5P__mt_cmp_class()
 *              return 0 if the two classes are equal. This is something that may be
 *              updated in the future to avoid confusion, but are left the same as the
 *              originals for now.
 *
 * Return:      Success: 1 if TRUE, 0 if FALSE
 *
 *              Failure: negative
 *
 ****************************************************************************************
 */
htri_t
H5P_class_isa(H5P_mt_class_t *pclass1, H5P_mt_class_t *pclass2)
{
    uint64_t pclass1_ver;
    uint64_t pclass2_ver;
    int32_t  equal;
    htri_t   ret_value = FAIL; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(pclass1);
    assert(pclass2);

    pclass1_ver = atomic_load(&(pclass1->curr_version));
    pclass2_ver = atomic_load(&(pclass2->curr_version));

    /* Compare property classes */
    if ((equal = H5P__mt_cmp_class(pclass1, pclass1_ver, pclass2, pclass2_ver)) == 0) {
        HGOTO_DONE(TRUE);
    }
    else {
        /* If classes are not equal, try comparing with pclass1's parent class */
        if (equal == 1) {
            if (pclass1->parent_ptr) {
                ret_value = H5P_class_isa(pclass1->parent_ptr, pclass2);
            }
            else {
                HGOTO_DONE(FALSE);
            }
        }
        else if (equal == -1) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to compare MT property list classes");
        }
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_class_isa() MT safe version */

/****************************************************************************************
 * Function:    H5P_isa_class
 *
 *              Multithread safe version of H5P_isa_class().
 *
 * Purpose:     Internal routine to query whether a property list is a certain class
 *
 *              This function grabs the parent class of the target property list and
 *              calls H5P_class_isa() to compare the list's parent with the provided
 *              pclass_id, to see if the provided list's parent, or a parent up the
 *              inheritance tree is equal to the provided class.
 *
 *              NOTE: With the updated H5P multithread structures it would probably be
 *              better to simply iterate the inheritance tree comparing the parent
 *              pointers to the target class's pointer to see if they are equal, and then
 *              determining which version of the class the list is derived from. However,
 *              this wouldn't take into account if the target class is a copy of a class
 *              in the inheritance tree, and would fail where it was supposed to succeed.
 *              Just something to consider for future iterations of multithread H5P.
 *
 * Return:      Success: 1 if TRUE, 0 if FALSE
 *
 *              Failure: negative
 *
 ****************************************************************************************
 */
htri_t
H5P_isa_class(hid_t plist_id, hid_t pclass_id)
{
    H5P_genplist_t *plist;  /* Property list to query */
    H5P_genclass_t *pclass; /* Property list class */
    bool            inc_thrd_flag = FALSE;

    htri_t ret_value = FAIL; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check arguments. */
    if (NULL == (plist = (H5P_genplist_t *)H5I_object_verify(plist_id, H5I_GENPROP_LST))) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");
    }

    /* Increment thread count */
    if ((H5P__inc_thrd_count(plist)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, (-1), "Couldn't increment class's thread count.");
    }

    inc_thrd_flag = TRUE;

    if (NULL == (pclass = (H5P_genclass_t *)H5I_object_verify(pclass_id, H5I_GENPROP_CLS))) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property class");
    }

    /**
     * NOTE: the thread count of pclass is not incremented here, due to it being incremented
     * in H5P__mt_cmp_class() that is called by H5P_class_isa(). It may be changed to be
     * incremented here as well, just as a safety check.
     */

    /* Compare the property list's parent class against the other class */
    if ((ret_value = H5P_class_isa(plist->pclass_ptr, pclass)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to compare property list classes");
    }

done:

    /* If TRUE, decrement thrd count. */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(plist)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, (-1), "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_isa_class() */

/****************************************************************************************
 * Function:    H5P_object_verify
 *
 *              No changes were made for multithread H5P, other than updating to the
 *              multithread H5P structures.
 *
 * Purpose:     Internal routine to query whether a property list is a certain class and
 *              returns a pointer to the target property list.
 *
 * Return:      Success: Pointer to the target property list
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_list_t *
H5P_object_verify(hid_t plist_id, hid_t pclass_id, bool allow_default)
{
    H5P_mt_list_t *ret_value = NULL; /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Compare the property list's class against the other class */
    if (H5P_isa_class(plist_id, pclass_id) != TRUE) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, NULL, "property list is not a member of the class");
    }

    (void)allow_default;

    /* Get the plist structure */
    if (NULL == (ret_value = (H5P_mt_list_t *)H5I_object(plist_id))) {
        HGOTO_ERROR(H5E_ID, H5E_BADID, NULL, "can't find object for ID");
    }

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_object_verify() */

/****************************************************************************************
 * Function:    H5P__iterate_plist
 *
 *              Multithread safe version of H5P__iterate_plist().
 *
 * Purpose:     Internal routine to iterate over the properties in a property list.
 *
 *              NOTE: This function was only updated for multithread H5P to ensure
 *              existing tests that use it still pass. Other than those tests its
 *              functionality has been taken over by other functions that search the list
 *              as a whole or specifically search the list's lkup_tbl or LFSLL at the
 *              list's current version or an older specified version. Thus, this function
 *              will most likely no longer be used in future iterations of multithread
 *              H5P.
 *
 *              NOTE: The multithread safe version of H5P doesn't use skip lists and thus
 *              doesn't iterate the same way. Do to the versioning system the function
 *              H5P__get_next_valid_prop() is called to get the next valid property in
 *              the list's LFSLL, skipping over any non-valid properties, for the most
 *              current version of the plist. Which is what would be stored in the
 *              original H5P_genlist_t structure. Then if iter_all_prop is TRUE the
 *              lkup_tbl of the plist is then iterated skipping any entry where the
 *              entry's curr.ptr is valid version, because it was already iterated over
 *              when iterating the LFSLL. The properties in the lkup_tbl are the
 *              properties inherited from the parent, which the original non multithread
 *              safe H5P list would have iterated over the parent class's properties.
 *
 *              NOTE: the index field curr_idx tracks the index of the LFSLL when
 *              iterating it (sentinal properties are skipped), and when moving on the
 *              lkup_tbl curr_idx continues to increment. Meaning that index 0 is the
 *              first valid property in the LFSLL (or if there are no valid properties
 *              in the LFSLL, then index 0 is the first valid property in the lkup_tbl).
 *
 *              NOTE: for more details on the multithread safe H5P structures and how
 *              they are handled, see the detailed descriptions of them in the file
 *              H5Ppkg_mt.h
 *
 * Return:      Success: 0
 *
 *              Failure: -1 (which should only happen if the callback fails)
 *
 ****************************************************************************************
 */
int
H5P__iterate_plist(H5P_mt_list_t *plist, hbool_t iter_all_prop, int *idx, H5P_iterate_int_t cb_func,
                   void *udata)
{
    H5P_mt_prop_t             *valid_prop;            /* Next valid property to iterate over */
    H5P_mt_prop_t             *prev_prop;             /* Prev prop that was iterated over */
    uint64_t                   version;               /* Current version of the plist */
    H5P_mt_list_table_entry_t *entry = NULL;          /* Entry in the lkup_tbl to iterate over */
    uint32_t                   tbl_idx;               /* Index of the lkup_tbl */
    bool                       base_flag = FALSE;     /* Flag for lkup_tbl entries */
    int                        dest_idx;              /* Index to start callback ops on */
    int                        curr_idx      = 0;     /* Current index in LFSLL and lkup_tbl */
    bool                       inc_thrd_flag = FALSE; /* Flag to dec thrd count */

    int ret_value = 0; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Increment thread count */
    if ((H5P__inc_thrd_count(plist)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* If the context is initialized grab version from it */
    if (H5P_H5CX_INIT_g) {
        if (0 == (version = H5CX_get_plist_version(atomic_load(&(plist->plist_id))))) {
            version = atomic_load(&(plist->curr_version));
        }
        else {
            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_list_version_from_ctx), 1);
        }
    }
    else {
        version = atomic_load(&(plist->curr_version));
    }

    prev_prop = plist->pl_head;
    assert(prev_prop);
    assert(atomic_load(&(prev_prop->tag)) == H5P_MT_PROP_TAG);
    assert(prev_prop->sentinel);

    dest_idx = *idx;

    /**
     * NOTE: The current index (curr_idx) starts at the beginning of the LFSLL
     * but the callback will not get called until curr_idx is equal to or
     * greater than the provided idx (dest_idx).
     */
    do {
        /* Gets the next valid property in the LFSLL */
        valid_prop = H5P__get_next_valid_prop(prev_prop, version, NULL);

        if (valid_prop) {
            if (curr_idx >= dest_idx) {
                /* Call the provided callback */
                ret_value = (cb_func)(valid_prop, udata);

                if (ret_value != 0) {
                    HGOTO_DONE(ret_value);
                }
            }

            /* Increment curr_idx and iterate */
            curr_idx++;

            prev_prop = valid_prop;

        } /* end if ( valid_prop ) */

    } while (valid_prop);

    /* If iter_all_prop is TRUE then iterate the lkup_tbl as well */
    if (iter_all_prop) {
        for (tbl_idx = 0; tbl_idx < plist->nprops_inherited; tbl_idx++) {
            entry = &plist->lkup_tbl[tbl_idx];

            /* Ensure the lkup_tbl entry contains a valid version */
            valid_prop = H5P__mt_entry_find_version(entry, version, &base_flag);

            if (valid_prop) {
                /**
                 * If base_flag is TRUE, then entry->base is the valid version for the
                 * version of the plist we are iterating over, and we must increment
                 * the idx counters.
                 *
                 * If base_flag is FALSE, then entry->curr valid version which was
                 * already iterated over in the LFSLL.
                 */
                if (base_flag) {
                    if (curr_idx >= dest_idx) {
                        /* Call the provided callback */
                        ret_value = (cb_func)(valid_prop, udata);

                        if (ret_value != 0) {
                            HGOTO_DONE(ret_value);
                        }
                    }

                    /* Increment curr_idx and iterate */
                    curr_idx++;

                } /* end if ( base_flag ) */
            }
        } /* end for() */

    } /* end if ( iter_all_prop ) */

    *idx = curr_idx;

done:

    /* If the thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(plist)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__iterate_plist() MT safe version*/

/****************************************************************************************
 * Function:    H5P__iterate_pclass
 *
 *              Multithread safe version of H5P__iterate_pclass().
 *
 * Purpose:     Internal routine to iterate over the properties in a property list.
 *
 *              NOTE: This function was only updated for multithread H5P to ensure
 *              existing tests that specifically use it still pass. Other than those tests its
 *              functionality has been taken over by other functions that search the
 *              class's LFSLL at the current version or an older specified version. Thus,
 *              this function will most likely no longer be used in future iterations of
 *              multithread H5P.
 *
 * Return:      0
 *
 ****************************************************************************************
 */
int
H5P__iterate_pclass(H5P_mt_class_t *pclass, int *idx, H5P_iterate_int_t cb_func, void *udata)
{
    H5P_mt_prop_t *valid_prop;
    H5P_mt_prop_t *prev_prop;
    uint64_t       version;
    int            dest_idx;
    int            curr_idx      = 0;     /* Current iteration index */
    bool           inc_thrd_flag = FALSE; /* Flag to dec thrd count */

    int ret_value = 0; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Increment thread count */
    if ((H5P__inc_thrd_count(pclass)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* Get the current version of the pclass */
    version = atomic_load(&(pclass->curr_version));

    prev_prop = pclass->pl_head;
    assert(prev_prop);
    assert(atomic_load(&(prev_prop->tag)) == H5P_MT_PROP_TAG);
    assert(prev_prop->sentinel);

    dest_idx = *idx;

    /**
     * NOTE: The current index (curr_idx) starts at the beginning of the LFSLL
     * but the callback will not get called until curr_idx is equal to or
     * greater than the provided idx (dest_idx).
     */
    do {
        /* Gets the next valid property in the LFSLL */
        valid_prop = H5P__get_next_valid_prop(prev_prop, version, NULL);

        if (valid_prop) {
            if (curr_idx >= dest_idx) {
                /* Call the provided callback */
                ret_value = (cb_func)(valid_prop, udata);

                /* If not 0, then we are done */
                if (ret_value != 0) {
                    HGOTO_DONE(ret_value);
                }
            }

            /* Increment curr_idx and iterate */
            curr_idx++;

            prev_prop = valid_prop;

        } /* end if ( valid_prop ) */

    } while (valid_prop);

    *idx = curr_idx;

done:

    /* If the thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(pclass)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__iterate_pclass() MT safe version*/

/****************************************************************************************
 * Function:    H5P_peek
 *
 *              Multithread safe version of H5P_peek().
 *
 * Purpose:     Internal routine that retrieves a "shallow" copy of the value for the
 *              valid property, with the provided name, in a property list. If there is a
 *              'get' callback routine registered for this property, it is _NOT_ called.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P_peek(H5P_genplist_t *plist, const char *name, void *value)
{
    H5P_mt_prop_t      *prop;
    H5P_mt_prop_value_t prop_value;
    uint64_t            version;
    bool                inc_thrd_flag = FALSE; /* Flag to dec thrd count */

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    /* Increment thread count */
    if ((H5P__inc_thrd_count(plist)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* If the context is initialized grab version from it */
    if (H5P_H5CX_INIT_g) {
        if (0 == (version = H5CX_get_plist_version(atomic_load(&(plist->plist_id))))) {

#if H5P_PLIST_CX_LOG /* Used for testing and debugging */
            H5P_mt_class_t *parent = plist->pclass_ptr;

            /* Creates a file to log all plists not stored in the context */
            FILE *H5P_plist_cx_log = fopen("H5P_plist_not_in_cx.log", "a");

            fprintf(H5P_plist_cx_log, "plist not in H5CX: plist_id: %ld    type: %d\n",
                    atomic_load(&(plist->plist_id)), parent->type);
#endif

            version = atomic_load(&(plist->curr_version));
        }
        else {
            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_list_version_from_ctx), 1);
        }
    }
    else {
        version = atomic_load(&(plist->curr_version));
    }

    /* Search for the property */
    if (NULL == (prop = H5P__mt_search__list(plist, name, version))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "MT property isn't in MT list");
    }

    prop_value = atomic_load(&(prop->value));

    /* Check for property size > 0 */
    if (0 == prop_value.size) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "property has zero size");
    }

    /* Make a (shallow) copy of the value */
    H5MM_memcpy(value, prop_value.ptr, prop_value.size);

done:

    /* If the thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(plist)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_peek() MT safe version */

/****************************************************************************************
 * Function:    H5P_get
 *
 *              Multithread safe version of H5P_get().
 *
 * Purpose:     Internal routine to retrieve a copy of the value for a property in a
 *              property list.
 *
 *              The property name must exist or this routine will fail. If the property
 *              has a registered 'get' callback routine, the copy of the value of the
 *              property will first be passed to that callback routine and any changes
 *              to the copy of the value will be used when returning the property value
 *              from this routine.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P_get(H5P_mt_list_t *plist, const char *name, void *value)
{
    H5P_mt_prop_t      *prop;
    H5P_mt_prop_value_t prop_value;
    void               *tmp_value = NULL;
    uint64_t            version;
    bool                inc_thrd_flag = FALSE; /* Flag to dec thrd count */

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(value);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(plist)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* If the context is initialized grab version from it */
    if (H5P_H5CX_INIT_g) {
        if (0 == (version = H5CX_get_plist_version(atomic_load(&(plist->plist_id))))) {

#if H5P_PLIST_CX_LOG /* Used for testing and debugging */
            H5P_mt_class_t *parent = plist->pclass_ptr;

            /* Creates a file to log all plists not stored in the context */
            FILE *H5P_plist_cx_log = fopen("H5P_plist_not_in_cx.log", "a");

            fprintf(H5P_plist_cx_log, "plist not in H5CX: plist_id: %ld    type: %d\n",
                    atomic_load(&(plist->plist_id)), parent->type);
#endif
            version = atomic_load(&(plist->curr_version));
        }
        else {
            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_list_version_from_ctx), 1);
        }
    }
    else {
        version = atomic_load(&(plist->curr_version));
    }

    /* Find the property and get the value */
    if (NULL == (prop = H5P__mt_search__list(plist, name, version))) {
        if (H5P_mt_cb.ver_cb) {
            (H5P_mt_cb.ver_cb)(version);
        }

        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "MT property isn't in MT list");
    }
    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

    if (H5P_mt_cb.ver_cb) {
        (H5P_mt_cb.ver_cb)(version);
    }

    prop_value = atomic_load(&(prop->value));

    /* Call the 'get' callback, if it exists */
    if (prop->get) {
        /* Make a copy of the current value, in case the callback fails */
        if (NULL == (tmp_value = H5MM_malloc(prop_value.size))) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "memory allocation failed temporary property value");
        }

        H5MM_memcpy(tmp_value, prop_value.ptr, prop_value.size);
#if 1
        if (H5P__global_lock_prop_cb__get(prop, atomic_load(&(plist->plist_id)), name, prop_value.size,
                                          tmp_value) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "Property's get callback failed");
        }
#else
        /* Call the callback */
        if ((*(prop->get))(plist->plist_id, name, prop_value.size, tmp_value) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "can't set property value");
        }
#endif

        H5MM_memcpy(value, tmp_value, prop_value.size);
    }
    else
        H5MM_memcpy(value, prop_value.ptr, prop_value.size);

done:

    if (tmp_value)
        H5MM_xfree(tmp_value);

    /* If the thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(plist)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_get MT safe version */

/****************************************************************************************
 * Function:    H5P__copy_prop_plist
 *
 *              Multithread safe version of H5P__copy_prop_plist().
 *
 * Purpose:     Internal routine to copy a property from one list to another.
 *
 *              If the target property already exists in the destination list, it must be
 *              deleted and have its delete callback be called, if it has one. Then a
 *              copy of the target property from the source list is created and inserted
 *              in the destination list and if the property has a copy callback, call it.
 *
 *              NOTE: Because two modifications to the list are done in this case, the
 *              version of the list must be incremented twice. After incrementing
 *              thrd.count for both lists and searching the source list for the target
 *              property, the current version of the destination list is atomically
 *              grabbed, and its next version is grabbed and incremented atomically by
 *              atomic_fetch_add. This is followed by the usual call to
 *              H5P__mt_enforce_serialization(). Then the destination list is searched
 *              to check if the target property already exists in it. If it does, the
 *              destination list has its next_version grabbed and incremented atomically
 *              again, storing the new grabbed next_version into the variable
 *              next_version2. The property is deleted from the destination list, and
 *              its current version is updated. H5P__mt_enforce_serialization() must be
 *              called a second time to ensure it's safe to continue, or if another
 *              thread entered the structure waiting for its turn to make a modification,
 *              this thread must wait for its turn. Once this thread can continue, a
 *              copy of the source list's target property is created and inserted into
 *              the destination list and the copy callback is called if it exists. Then
 *              the current version of destination list must be incremented again after
 *              the second modification is completed.
 *
 *              NOTE: The property wouldn't need to be deleted from the destination list
 *              and could normally just be inserted due to the versioning system of the
 *              multithread H5P structures. However, because the property delete callback
 *              is expected, it must be deleted first.
 *
 *
 *              If the target property does not already exist in the destination list,
 *              a copy of the target property from the source list is created and
 *              inserted in the destination list and if the property has a create
 *              callback, call it.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__copy_prop_plist(hid_t dst_id, hid_t src_id, const char *name)
{
    H5P_mt_list_t      *dst_plist;
    H5P_mt_list_t      *src_plist;
    H5P_mt_prop_t      *dst_prop;
    H5P_mt_prop_t      *src_prop;
    H5P_mt_prop_value_t value;
    int64_t             chksum            = 0;
    uint64_t            src_version       = 0;
    uint64_t            dst_version       = 0;
    uint64_t            next_version      = 0;
    uint64_t            next_version2     = 0;
    bool                inc_thrd_flag_src = FALSE; /* Flag to dec thrd count */
    bool                inc_thrd_flag_dst = FALSE; /* Flag to dec thrd count */
    bool                ver_updated       = FALSE;
    bool                ver2_updated      = FALSE;

    herr_t ret_value = SUCCEED; /* return value */

    FUNC_ENTER_PACKAGE

    assert(name);

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__copy_prop_plist__num_calls), 1);

    /* Get the objects to operate on */
    if (NULL == (src_plist = (H5P_mt_list_t *)H5I_object(src_id)) ||
        NULL == (dst_plist = (H5P_mt_list_t *)H5I_object(dst_id))) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property object doesn't exist");
    }

    /* Increment thread count */
    if (H5P__inc_thrd_count(src_plist) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag_src = TRUE;
    }

    /* Increment thread count */
    if (H5P__inc_thrd_count(dst_plist) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag_dst = TRUE;
    }

    /* Grab the current version of the src_plist */
    src_version = atomic_load(&(src_plist->curr_version));

    /* Get the pointer to the source property */
    if (NULL == (src_prop = H5P__mt_search__list(src_plist, name, src_version))) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "MT property doesn't exist");
    }

    assert(src_prop);
    assert(atomic_load(&(src_prop->tag)) == H5P_MT_PROP_TAG);

    value = atomic_load(&(src_prop->value));

    /* Get the version of the dst_plist */
    dst_version  = atomic_load(&(dst_plist->curr_version));
    next_version = atomic_fetch_add(&(dst_plist->next_version), 1);
    ver_updated  = TRUE;

    /* Ensure another thread isn't modifing the LFSLL */
    if ((dst_version + 1) < next_version) {
        if (0 == (dst_version = H5P__mt_enforce_serialization(dst_plist, dst_version, next_version))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }

        dst_version = atomic_load(&(dst_plist->curr_version));
    }

    assert(dst_version + 1 == next_version);

    /* If the property exists in the destination already */
    if ((dst_prop = H5P__mt_search__list(dst_plist, name, dst_version)) != NULL) {
        assert(dst_prop);
        assert(atomic_load(&(dst_prop->tag)) == H5P_MT_PROP_TAG);

        /**
         * NOTE: Because versions of lists are incremented after every modification
         * and because the target property already exists in the dst_plist and must
         * be deleted, another next_version must be atomically grabbed and incremented
         * for the insert of the copied property.
         */
        next_version2 = atomic_fetch_add(&(dst_plist->next_version), 1);
        ver2_updated  = TRUE;

        chksum = H5_checksum_metadata(name, strlen(name), 0);

        /**
         * Delete the property from the destination list,
         * and call the close callback if it has one.
         */
        if ((H5P__mt_delete_prop__list(dst_plist, chksum, name, dst_version, next_version)) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTDELETE, FAIL, "unable to remove property");
        }

        /* Update the curr_version of the dst_plist */
        if (ver_updated) {
            /* Update the list's current version */
            dst_version = atomic_fetch_add(&(dst_plist->curr_version), 1);
            assert(dst_version + 1 == next_version);

            ver_updated = FALSE;

            /* update stats */
            if (atomic_load(&(dst_plist->curr_version)) > atomic_load(&(H5P_mt_g.max_list_version_number))) {
                atomic_store(&(H5P_mt_g.max_list_version_number), atomic_load(&(dst_plist->curr_version)));
            }

            /* Update global atomic plist version if list is a default plist */
            if (dst_plist->def_ver_ptr) {
                atomic_fetch_add((dst_plist->def_ver_ptr), 1);

                /* update stats */
                atomic_fetch_add(&(H5P_mt_g.num_default_list_mods), 1);
            }
        }

        dst_version = atomic_load(&(dst_plist->curr_version));

        /* Ensure another thread wasn't waiting and is now modifing the structure before continuing */
        if ((dst_version + 1) < next_version2) {
            if (0 == (dst_version = H5P__mt_enforce_serialization(dst_plist, dst_version, next_version2))) {
                HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
            }

            dst_version = atomic_load(&(dst_plist->curr_version));
        }

        /**
         * Create a copy of the source prop,
         * call the copy callback,
         * and insert it into the dst_plist
         */
        if (0 < H5P__mt_ins_or_mod_prop__list(
                    dst_plist, src_prop->name, value.ptr, value.size, FALSE, TRUE, TRUE, next_version2,
                    src_prop->create, src_prop->set, src_prop->get, src_prop->encode, src_prop->decode,
                    src_prop->del, src_prop->copy, src_prop->cmp, src_prop->close)) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Failed copying and inserting MT property");
        }

    } /* end if() */

    /* If the property doesn't exist in the destination */
    else {
        /**
         * Create a copy of the source prop,
         * call the create callback,
         * and insert it into the dst_plist
         */
        if (0 < H5P__mt_ins_or_mod_prop__list(
                    dst_plist, src_prop->name, value.ptr, value.size, TRUE, FALSE, TRUE, next_version,
                    src_prop->create, src_prop->set, src_prop->get, src_prop->encode, src_prop->decode,
                    src_prop->del, src_prop->copy, src_prop->cmp, src_prop->close)) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Failed copying and inserting MT property");
        }
    }

done:

    /* Update dst_plist's curr_version if ver_updated is TRUE */
    if (ver_updated) {
        /* Update the list's current version */
        dst_version = atomic_fetch_add(&(dst_plist->curr_version), 1);
        assert(dst_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(dst_plist->curr_version)) > atomic_load(&(H5P_mt_g.max_list_version_number))) {
            atomic_store(&(H5P_mt_g.max_list_version_number), atomic_load(&(dst_plist->curr_version)));
        }

        /* Update global atomic plist version if list is a default plist */
        if (dst_plist->def_ver_ptr) {
            atomic_fetch_add((dst_plist->def_ver_ptr), 1);

            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_default_list_mods), 1);
        }
    }

    /* Update dst_plist's curr_version if ver2_updated is TRUE */
    if (ver2_updated) {
        /* Update the list's current version */
        dst_version = atomic_fetch_add(&(dst_plist->curr_version), 1);
        assert(dst_version + 1 == next_version2);

        /* update stats */
        if (atomic_load(&(dst_plist->curr_version)) > atomic_load(&(H5P_mt_g.max_list_version_number))) {
            atomic_store(&(H5P_mt_g.max_list_version_number), atomic_load(&(dst_plist->curr_version)));
        }

        /* Update global atomic plist version if list is a default plist */
        if (dst_plist->def_ver_ptr) {
            atomic_fetch_add((dst_plist->def_ver_ptr), 1);

            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_default_list_mods), 1);
        }
    }

    /* If the thrd_count was increment, decrement it */
    if (inc_thrd_flag_src) {
        if (0 > H5P__dec_thrd_count(src_plist)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    /* If the thrd_count was increment, decrement it */
    if (inc_thrd_flag_dst) {
        if (0 > H5P__dec_thrd_count(dst_plist)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__copy_prop_plist() MT safe version */

/****************************************************************************************
 * Function:    H5P__copy_prop_pclass
 *
 *              Multithread safe version of H5P__copy_prop_pclass().
 *
 * Purpose:     Internal routine to copy a property from one class to another
 *
 *              In the non-multithread version of H5P__copy_prop_pclass() if the property
 *              already exists in the destination class it is deleted first, just like in
 *              H5P__copy_prop_plist(). However in this multithread version that is not
 *              necessary, due to the versioning system of the multithread H5P structures
 *              and for H5P__copy_prop_pclass() the property delete callback is not
 *              called.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__copy_prop_pclass(hid_t dst_id, hid_t src_id, const char *name)
{
    H5P_mt_class_t     *dst_pclass;
    H5P_mt_class_t     *src_pclass;
    H5P_mt_prop_t      *prop;
    H5P_mt_prop_value_t value;
    uint64_t            src_version       = 0;
    uint64_t            dst_version       = 0;
    uint64_t            next_version      = 0;
    bool                inc_thrd_flag_src = FALSE; /* Flag to dec thrd count */
    bool                inc_thrd_flag_dst = FALSE; /* Flag to dec thrd count */
    bool                ver_updated       = FALSE;

    herr_t ret_value = SUCCEED; /* return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(name);

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__copy_prop_pclass__num_calls), 1);

    /* Get the objects to operate on */
    if (NULL == (src_pclass = (H5P_mt_class_t *)H5I_object(src_id)) ||
        NULL == (dst_pclass = (H5P_mt_class_t *)H5I_object(dst_id))) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property object doesn't exist");
    }

    /* Increment thread count */
    if (H5P__inc_thrd_count(src_pclass) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag_src = TRUE;
    }

    /* Increment thread count */
    if (H5P__inc_thrd_count(dst_pclass) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag_dst = TRUE;
    }

    /* Get current version of the source class */
    src_version = atomic_load(&(src_pclass->curr_version));

    /* Get the property from the source */
    if (NULL == (prop = H5P__mt_search__class(src_pclass, name, src_version))) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "unable to locate MT property");
    }

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

    value = atomic_load(&prop->value);

    /* Get the version of the dst_pclass */
    dst_version  = atomic_load(&(dst_pclass->curr_version));
    next_version = atomic_fetch_add(&(dst_pclass->next_version), 1);
    ver_updated  = TRUE;

    /* Ensure another thread isn't modifing the LFSLL */
    if ((dst_version + 1) < next_version) {
        if (0 == (dst_version = H5P__mt_enforce_serialization(dst_pclass, dst_version, next_version))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }

        dst_version = atomic_load(&(dst_pclass->curr_version));
    }

    /* Create a copy of the source prop, and insert it into the dst_pclass */
    if (0 < H5P__mt_ins_or_mod_prop__class(dst_pclass, prop->name, value.ptr, value.size, TRUE, next_version,
                                           prop->create, prop->set, prop->get, prop->encode, prop->decode,
                                           prop->del, prop->copy, prop->cmp, prop->close)) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Failed copying and inserting MT property");
    }

done:

    /* Update dst_pclass's curr_version if ver_updated is TRUE */
    if (ver_updated) {
        /* Update the list's current version */
        dst_version = atomic_fetch_add(&(dst_pclass->curr_version), 1);
        assert(dst_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(dst_pclass->curr_version)) > atomic_load(&(H5P_mt_g.max_list_version_number))) {
            atomic_store(&(H5P_mt_g.max_list_version_number), atomic_load(&(dst_pclass->curr_version)));
        }
    }

    /* If the thrd_count was increment, decrement it */
    if (inc_thrd_flag_src) {
        if (0 > H5P__dec_thrd_count(src_pclass)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    /* If the thrd_count was increment, decrement it */
    if (inc_thrd_flag_dst) {
        if (0 > H5P__dec_thrd_count(dst_pclass)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__copy_prop_pclass() MT safe version */

/****************************************************************************************
 * Function:    H5P_close
 *
 *              Multithread safe version of H5P_close()
 *
 * Purpose:     Internal routine to close a property list.
 *
 *              Check if the list is already marked closing and if it is throw an error,
 *              because it shouldn't be marked closing until it is actually closing.
 *
 *              Then set closing to TRUE, and check if opening is TRUE, or if there
 *              are other threads in this struct, and if so we loop and check again.
 *
 *              When opening is FALSE and there are no other threads in the struct, we
 *              check if the property list initialization function completed. If so call
 *              the close callback (cb) on the parent class.
 *
 *              Next iterate the lkup_tbl and if base.ptr is not NULL and the property
 *              has the close cb, call it. Then iterate the LFSLL and do the same
 *              thing (if the property has the close cb call it).
 *
 *              Decrement the ref_count.pl of this lists's parent, then add this list to
 *              the tail of the list free list instead of freeing the struct.
 *
 *              Decrement the parent's thrd.count.
 *
 *              Call H5I_dec_ref() to decrement the parent's ID ref count now that this
 *              list is closed.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P_close(H5P_genplist_t *list)
{
    H5P_mt_class_t              *parent;
    H5P_mt_list_t               *fl_list;
    H5P_mt_list_sptr_t           fl_head;
    H5P_mt_list_sptr_t           fl_tail;
    H5P_mt_list_sptr_t           fl_next;
    H5P_mt_list_sptr_t           fl_update;
    H5P_mt_active_thread_count_t local_thrd;
    H5P_mt_active_thread_count_t closing_thrd;
    H5P_mt_list_table_entry_t   *entry;
    H5P_mt_list_prop_ref_t       base_ref;
    H5P_mt_prop_t               *base_prop;
    H5P_mt_prop_t               *prop;
    H5P_mt_prop_t               *valid_prop;
    H5P_mt_prop_value_t          prop_value;
    H5P_mt_class_ref_counts_t    ref_count;
    uint64_t                     list_version;
    uint64_t                     delete_version;
    bool                         done              = FALSE;
    bool                         inc_thrd_flag     = FALSE;
    bool                         try_to_free_entry = FALSE;

    herr_t ret_value = SUCCEED; /* return value */

    FUNC_ENTER_NOAPI_NOINIT

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P_close__num_calls), 1);

    local_thrd = atomic_load(&(list->thrd));

    /* If closing is already TRUE, throw an error */
    if (local_thrd.closing) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Closing flag is set when it shouldn't be.");
    }

    /* Atomically set the closing to be TRUE */
    do {
        local_thrd = atomic_load(&(list->thrd));

        closing_thrd         = local_thrd;
        closing_thrd.closing = TRUE;

        /* Atomically update closing flag to TRUE */
        if (!atomic_compare_exchange_strong(&(list->thrd), &local_thrd, closing_thrd)) {
            /* failed, update stats and try again */
            atomic_fetch_add(&(list->num_thrd_update_cols), 1);

            /* assert is to not get stuck in an infinite loop while testing */
            assert(H5P_MT_ASSERT_FAIL);
        }
        else {
            /* success, update stats and mark done */
            atomic_fetch_add(&(list->num_thrd_closing_flag_set), 1);

            done = TRUE;
        }

    } while (!done);

    assert(done);
    done = FALSE;

    local_thrd = atomic_load(&(list->thrd));
    assert(local_thrd.closing);

    /* Ensure struct isn't opening, and that it's empty of other threads */
    do {
        /* If opening is TRUE, sleep and try again */
        if (local_thrd.opening) {
            atomic_fetch_add(&(list->num_thrd_opening_flag_set), 1);

            /** NOTE: This assert is to prevent an infinite loop while testing */
            assert(H5P_MT_ASSERT_FAIL);

            sleep(1);
        }
        /* If there are any other threads in the struct, wait for them to drain out */
        else if (local_thrd.count > 0) {
            /** NOTE: This assert is to prevent an infinite loop while testing */
            assert(H5P_MT_ASSERT_FAIL);

            sleep(1);
        }
        else {
            done = TRUE;
        }

        local_thrd = atomic_load(&(list->thrd));

    } while (!done);

    assert(done);
    done = FALSE;

    local_thrd = atomic_load(&(list->thrd));
    assert(local_thrd.opening == FALSE);
    assert(local_thrd.closing == TRUE);
    assert(local_thrd.count == 0);

    /* Get the current version of the list */
    list_version = atomic_load(&(list->curr_version));

    /* Check the property list initialization function completed */
    if (atomic_load(&(list->class_init))) {
        parent = list->pclass_ptr;
        assert(parent);

        /**
         * NOTE: May need to add a tracker to ensure we don't call the close cb on a
         * parent's prop that has the same name as a prop in the list we already called
         * the close cb on.
         */

        /* Call the class close callback up the parent inheritance tree, if needed */
        while (parent) {
            assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_TAG);

            /* update parent's thrd count */
            if (H5P__inc_thrd_count(parent) < 0) {
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment parent's thread count.");
            }

            inc_thrd_flag = TRUE;

            /* class close callback */
            if (parent->close_func) {
                (parent->close_func)(list->plist_id, parent->close_data);
            }

            /* Decrement the thrd count */
            if (0 > H5P__dec_thrd_count(parent)) {
                HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement parent's thrd_count.");
            }

            inc_thrd_flag = FALSE;

            parent = parent->parent_ptr;
        } /* while ( parent ) */

    } /* end if ( atomic_load(&(list->class_init)) ) */

    /* Set parent variable back to the list's direct parent class */
    parent = list->pclass_ptr;
    assert(parent);
    assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_TAG);

    /* update parent's thrd count */
    if (H5P__inc_thrd_count(parent) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment parent's thread count.");
    }

    inc_thrd_flag = TRUE;

    /**
     * Using H5P__get_next_valid_prop() iterate the LFSLL and grab only the most
     * current version of a valid property. If it has a close callback call it.
     * This ensures only one version of a property will call the close callback to
     * prevent a double free error.
     */

    prop = list->pl_head;
    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(prop->sentinel);

    do {
        /* Get the next valid prop in the LFSLL */
        valid_prop = H5P__get_next_valid_prop(prop, list_version, NULL);

        if (valid_prop) {
            /* If the property has a close callback call it */
            if (valid_prop->close) {
                prop_value = atomic_load(&(valid_prop->value));
#if 1
                H5P__global_lock_prop_cb__close(valid_prop, valid_prop->name, prop_value.size,
                                                prop_value.ptr);
#else
                (valid_prop->close)(valid_prop->name, prop_value.size, prop_value.ptr);
#endif
            }

            prop = valid_prop;
        }

    } while (valid_prop);

    /* Scan the lkup_tbl and if base.ptr != NULL, and the close cb exists call it */

    /**
     * Iterate the lkup_tbl and if the first_ver_of_curr == 0, the base must be checked
     * and if not deleted and not NULL, call the property close callback if it exists.
     * Otherwise, if first_ver_of_curr is greater than 0 then the close callback has
     * already been called on another version of the property, if the callback exists.
     */
    for (uint32_t i = 0; i < list->nprops_inherited; i++) {
        entry = &list->lkup_tbl[i];

        base_ref = atomic_load(&(entry->base));

        if (0 == atomic_load(&(entry->first_ver_of_curr))) {
            delete_version = atomic_load(&(entry->base_delete_version));

            /* If the base is not marked for deletion */
            if (delete_version == 0 || delete_version > list_version) {
                /* Ensure the base pointer isn't NULL */
                if (base_ref.ptr) {
                    base_prop = base_ref.ptr;

                    /* If the prop has a close callback, call it */
                    if (base_prop->close) {
                        prop_value = atomic_load(&(base_prop->value));

                        /* property close callback */
#if 1
                        H5P__global_lock_prop_cb__close(base_prop, base_prop->name, prop_value.size,
                                                        prop_value.ptr);
#else
                        (base_prop->close)(base_prop->name, prop_value.size, prop_value.ptr);
#endif
                    }
                }
            }

        } /* end if ( 0 == atomic_load(&(entry->first_ver_of_curr)) ) */

        /* If the base.ptr isn't NULL, decrement it's ref_count */
        if (base_ref.ptr) {
            base_prop = base_ref.ptr;

            assert(0 != (atomic_load(&(base_prop->ref_count))));
            atomic_fetch_sub(&(base_prop->ref_count), 1);
        }

    } /* end for ( uint32_t i = 0; i < list->nprops_inherited; i++ ) */

    /* Decrement parent's ref count of derived lists */
    if (H5P__dec_ref_count(parent, FALSE) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Couldn't decrement parent's pl ref count.");
    }

    done = FALSE;

    /* Atomically updates the current tail to point to the list being added */

    fl_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

    if (fl_tail.ptr) {
        do {
            fl_list = fl_tail.ptr;

            fl_next = atomic_load(&(fl_list->fl_next));

            assert(!fl_next.ptr);

            fl_update.ptr = list;
            fl_update.sn  = fl_next.sn + 1;

            if (!atomic_compare_exchange_strong(&(fl_list->fl_next), &fl_next, fl_update)) {
                /* failed, updated stats and try again */
                atomic_fetch_add(&(H5P_mt_g.list_fl_next_update_cols), 1);

                /* assert is to not get stuck in an infinite loop while testing */
                assert(H5P_MT_ASSERT_FAIL);
            }
            else {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.list_fl_next_update), 1);

                done = TRUE;
            }

        } while (!done);

    } /* end if ( fl_tail.ptr ) */

    done = FALSE;

    /* Atomically updates the list_fl_tail to point to the new tail */

    do {
        fl_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

        fl_update.ptr = list;
        fl_update.sn  = fl_tail.sn + 1;

        if (!atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_tail), &fl_tail, fl_update)) {
            /* failed, updated stats and try again */
            atomic_fetch_add(&(H5P_mt_g.list_fl_tail_update_cols), 1);

            /* assert is to not get stuck in an infinite loop while testing */
            assert(H5P_MT_ASSERT_FAIL);
        }
        else {
            /* success, update stats and continue */
            atomic_fetch_add(&(H5P_mt_g.list_fl_tail_update), 1);
            atomic_fetch_add(&(H5P_mt_g.num_list_added_to_fl), 1);

            atomic_fetch_add(&(H5P_mt_g.list_fl_len), 1);

            if (atomic_load(&(H5P_mt_g.list_fl_len)) > atomic_load(&(H5P_mt_g.list_max_desired_fl_len))) {
                try_to_free_entry = TRUE;
            }

            done = TRUE;
        }

    } while (!done);

    /**
     * If this is the first list added to the list free list, have the
     * head pointer point to it as well.
     */
    fl_head = atomic_load(&(H5P_mt_g.list_fl_head));

    if (!fl_head.ptr) {
        done = FALSE;

        do {
            fl_head = atomic_load(&(H5P_mt_g.list_fl_head));

            fl_update.ptr = list;
            fl_update.sn  = fl_head.sn + 1;

            if (!atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_head), &fl_head, fl_update)) {
                /* failed, updated stats and try again */
                atomic_fetch_add(&(H5P_mt_g.list_fl_head_update_cols), 1);

                /* assert is to not get stuck in an infinite loop while testing */
                assert(H5P_MT_ASSERT_FAIL);
            }
            else {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.list_fl_head_update), 1);

                done = TRUE;
            }

        } while (!done);

    } /* end if ( ! fl_head.ptr ) */

    atomic_store(&(list->tag), H5P_MT_LIST_INVALID_TAG);

    if (try_to_free_entry) {
        done = FALSE;

        do {
            fl_head = atomic_load(&(H5P_mt_g.list_fl_head));
            fl_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

            assert(fl_head.ptr);
            assert(fl_tail.ptr);

            if (fl_head.ptr != fl_tail.ptr) {
                fl_list = fl_head.ptr;

                if (atomic_load(&(fl_list->tag)) == H5P_MT_LIST_FL_REALLOC_TAG) {
                    fl_next = atomic_load(&(fl_list->fl_next));

                    if (!atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_head), &fl_head, fl_next)) {
                        atomic_fetch_add(&(H5P_mt_g.list_fl_head_update_cols), 1);
                    }
                    else {
                        atomic_fetch_add(&(H5P_mt_g.list_fl_head_update), 1);
                        atomic_fetch_add(&(H5P_mt_g.list_fl_head_freed_due_to_max_len), 1);

                        H5P__clear_mt_list(fl_list);

                        free(fl_list);

                        done = TRUE;
                    }
                }
                else {
                    atomic_fetch_add(&(H5P_mt_g.list_fl_head_free_skipped_no_reallocable), 1);

                    done = TRUE;
                }
            }
            else {
                atomic_fetch_add(&(H5P_mt_g.list_fl_head_free_skipped_due_to_empty), 1);

                done = TRUE;
            }

        } while (!done);

    } /* end if ( try_to_free_entry ) */

done:

    /* update parent's thrd count */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(parent)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }

        ref_count = atomic_load(&(parent->ref_count));

        if (ref_count.deleted == FALSE) {
            if (0 > H5I_dec_ref(atomic_load(&(parent->id)))) {
                assert(FALSE);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL,
                            "unable to decrement parent's ID ref_count in index");
            }
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_close() MT safe version */

/****************************************************************************************
 * Function:    H5P__clear_mt_list
 *
 * Purpose:     Frees all allocated memory in the list and clears all fields
 *
 *              This function first makes sure that no other thread is accessing this
 *              list struct, and then itsets the tag to H5P_MT_LIST_INVALID_TAG to mark
 *              that this list is no longer valid for threads to access.
 *
 *              Then it frees all entries in the lkup_tbl, frees the lkup_tbl, adds all
 *              properties from the LFSLL to the property free list, and resets all of
 *              its stats fields.
 *
 *
 * Return:      Success: Returns a pointer to the list after all data being cleared.
 *
 *              Failure: NULL *
 ****************************************************************************************
 */
H5P_mt_list_t *
H5P__clear_mt_list(H5P_mt_list_t *list)
{
    H5P_mt_active_thread_count_t thrd;
    H5P_mt_list_prop_ref_t       prop_ref;
    H5P_mt_list_table_entry_t   *entry;
    H5P_mt_prop_t               *first_prop;
    H5P_mt_prop_aptr_t           next_ptr;
    hid_t                        null_list_id = 0;
    size_t                       phys_pl_len;
    size_t                       i;

    H5P_mt_list_t *ret_value;

    FUNC_ENTER_PACKAGE

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_FL_REALLOC_TAG);

    thrd = atomic_load(&(list->thrd));

    assert(thrd.count == 0);
    assert(thrd.opening == FALSE);
    assert(thrd.closing == TRUE);

    /* Clears the list's fields */

    atomic_store(&(list->tag), H5P_MT_LIST_INVALID_TAG);

    list->pclass_id  = H5I_INVALID_HID;
    list->pclass_ptr = NULL;

    atomic_store(&(list->plist_id), null_list_id);

    /* Clear the lkup_tbl, including free any allocated memory */
    for (i = 0; i < list->nprops_inherited; i++) {
        entry = &list->lkup_tbl[i];

        entry->chksum = 0;

        free(entry->name);

        entry->name = NULL;

        prop_ref.ptr = NULL;
        prop_ref.ver = 0;

        atomic_store(&(entry->base), prop_ref);
        atomic_store(&(entry->base_delete_version), 0);
        atomic_store(&(entry->curr), prop_ref);

    } /* end for ( i = 0; i < list->nprops_inherited; i++ ) */

    free(list->lkup_tbl);

    list->lkup_tbl = NULL;

    /* Iterate the LFSLL and add all properties to the free list */
    phys_pl_len = atomic_load(&(list->phys_pl_len));

    for (i = 0; i < (phys_pl_len); i++) {
        first_prop = list->pl_head;
        assert(first_prop);
        assert(atomic_load(&(first_prop->tag)) == H5P_MT_PROP_TAG);

        next_ptr = atomic_load(&(first_prop->next));

        list->pl_head = next_ptr.ptr;

        if (H5P__mt_close_prop(first_prop) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Failed to add property to free list.");
        }

        atomic_fetch_sub(&(list->phys_pl_len), 1);

    } /* end for() */

    atomic_store(&(list->log_pl_len), 0);

    /* Ensure the list is empty, then free the list */
    first_prop = list->pl_head;
    assert(!first_prop);

    assert(0 == (atomic_load(&(list->log_pl_len))));
    assert(0 == (atomic_load(&(list->phys_pl_len))));

    if (0 > (H5P__reset_stats_list(list))) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Failed resetting stats fields.");
    }

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.num_lists_freed), 1);

done:

    ret_value = list;

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__clear_mt_list() */

/****************************************************************************************
 * Function:    H5P_get_class_name
 *
 *              Multithread safe version of H5P_get_class_name().
 *
 * Purpose:     Internal routine to query the name of a generic property list class
 *
 *              NOTE: The pointer to the name must be free'd by the user for successful
 *              calls.
 *
 *              NOTE: Only changes were incrementing and decrementing the class's
 *              thrd.count.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
char *
H5P_get_class_name(H5P_mt_class_t *pclass)
{
    bool inc_thrd_flag = FALSE; /* Flag to dec thrd count */

    char *ret_value = NULL; /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Increment thread count */
    if ((H5P__inc_thrd_count(pclass)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    assert(pclass);

    /* Get class name */
    ret_value = H5MM_xstrdup(pclass->name);

done:

    /* If the thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(pclass)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, NULL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_get_class_name() */

/****************************************************************************************
 * Function:    H5P__get_class_path
 *
 *              Multithread safe version of H5P__get_class_path().
 *
 * Purpose:     Internal routine to query the full path of a property list class
 *
 *              The pointer to the malloc'ed string must be free'd by the user for
 *              successful calls.
 *
 *              NOTE: Only changes were incrementing and decrementing the class's
 *              thrd.count.
 *
 * Return:      Success: Pointer to a malloc'ed string containing the full path of class
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
char *
H5P__get_class_path(H5P_mt_class_t *pclass)
{
    bool inc_thrd_flag = FALSE; /* Flag to dec thrd count */

    char *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(pclass);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(pclass)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* Recursively build the full path */
    if (pclass->parent_ptr != NULL) {
        char *par_path; /* Parent class's full path */

        /* Get the parent class's path */
        par_path = H5P__get_class_path(pclass->parent_ptr);

        if (par_path != NULL) {
            size_t ret_str_len;

            /**
             * Allocate enough space for the parent class's path, plus the '/'
             * separator, this class's name and the string terminator
             *
             * Extra "+3" to quiet GCC warning - 2019/07/05, QAK
             */
            ret_str_len = HDstrlen(par_path) + HDstrlen(pclass->name) + 1 + 3;
            if (NULL == (ret_value = (char *)H5MM_malloc(ret_str_len))) {
                HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed for class name");
            }

            /* Build the full path for this class */
            HDsnprintf(ret_value, ret_str_len, "%s/%s", par_path, pclass->name);

            /* Free the parent class's path */
            H5MM_xfree(par_path);
        } /* end if */
        else
            ret_value = H5MM_xstrdup(pclass->name);
    } /* end if */
    else
        ret_value = H5MM_xstrdup(pclass->name);

done:

    /* If the thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(pclass)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, NULL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__get_class_path() MT safe version */

/****************************************************************************************
 * Function:    H5P__open_class_path_cb
 *
 *              Multithread safe version of H5P__open_class_path_cb().
 *
 * Purpose:     Multithread safe version of H5P__open_class_path_cb() which is an
 *              internal callback routine to check for duplicated names in parent class.
 *
 *              NOTE: only change between this version and the original is the name
 *              change for the new multithread safe structures, obj->parent changed to
 *              obj->parent_ptr.
 *
 * Return:      Success: Pointer to a property class object
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
static int
H5P__open_class_path_cb(void *_obj, hid_t H5_ATTR_UNUSED id, void *_key)
{
    H5P_genclass_t    *obj       = (H5P_genclass_t *)_obj;    /* Pointer to the class for this ID */
    H5P_check_class_t *key       = (H5P_check_class_t *)_key; /* Pointer to key information for comparison */
    int                ret_value = 0;                         /* Return value */

    FUNC_ENTER_PACKAGE_NOERR

    assert(obj);
    assert(H5I_GENPROP_CLS == H5I_get_type(id));
    assert(key);

    /* Check if the class object has the same parent as the new class */
    if (obj->parent_ptr == key->parent) {
        /* Check if they have the same name */
        if (HDstrcmp(obj->name, key->name) == 0) {
            key->new_class = obj;
            ret_value      = 1; /* Indicate a match */
        }                       /* end if */
    }                           /* end if */

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5P__open_class_path_cb() MT safe version */

/****************************************************************************************
 * Function:    H5P__open_class_path
 *
 *              No changes were made to this function for multithread safe H5P
 *
 * Purpose:     Internal routine to open [a copy of] a class with its full path name
 *
 *
 * Return:      Success: Pointer to the new copy property class object
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_genclass_t *
H5P__open_class_path(const char *path)
{
    char             *tmp_path = NULL;  /* Temporary copy of the path */
    char             *curr_name;        /* Pointer to current component of path name */
    char             *delimit;          /* Pointer to path delimiter during traversal */
    H5P_genclass_t   *curr_class;       /* Pointer to class during path traversal */
    H5P_check_class_t check_info;       /* Structure to hold the information for checking duplicate names */
    H5P_genclass_t   *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(path);

    /* Duplicate the path to use */
    tmp_path = H5MM_xstrdup(path);
    assert(tmp_path);

    /* Find the generic property class with this full path */
    curr_name  = tmp_path;
    curr_class = NULL;
    while (NULL != (delimit = HDstrchr(curr_name, '/'))) {
        /* Change the delimiter to terminate the string */
        *delimit = '\0';

        /* Set up the search structure */
        check_info.parent    = curr_class;
        check_info.name      = curr_name;
        check_info.new_class = NULL;

        /* Find the class with this name & parent by iterating over the open classes */
        if (H5I_iterate(H5I_GENPROP_CLS, H5P__open_class_path_cb, &check_info, FALSE) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADITER, NULL, "can't iterate over classes");
        }
        else if (NULL == check_info.new_class) {
            HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, NULL, "can't locate class");
        }

        /* Advance the pointer in the path to the start of the next component */
        curr_class = check_info.new_class;
        curr_name  = delimit + 1;
    } /* end while */

    /* Should be pointing to the last component in the path name now... */

    /* Set up the search structure */
    check_info.parent    = curr_class;
    check_info.name      = curr_name;
    check_info.new_class = NULL;

    /* Find the class with this name & parent by iterating over the open classes */
    if (H5I_iterate(H5I_GENPROP_CLS, H5P__open_class_path_cb, &check_info, FALSE) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADITER, NULL, "can't iterate over classes");
    }
    else if (NULL == check_info.new_class) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, NULL, "can't locate class");
    }

    /* Copy it */
    if (NULL == (ret_value = H5P__copy_pclass(check_info.new_class))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, NULL, "can't copy property class");
    }

done:
    /* Free the duplicated path */
    H5MM_xfree(tmp_path);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__open_class_path() */

/****************************************************************************************
 * Function:    H5P__get_class_parent
 *
 *              No changes were made to this function for multithread H5P
 *
 * Purpose:     Internal routine to query the parent class of a property class
 *
 *
 * Return:      Success: Pointer to the parent class of a property class
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_class_t *
H5P__get_class_parent(H5P_mt_class_t *pclass)
{
    bool inc_thrd_flag = FALSE; /* Flag to dec thrd count */

    H5P_genclass_t *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(pclass);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(pclass)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, "Couldn't increment thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /* Get property size */
    if (NULL == (ret_value = pclass->parent_ptr)) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, NULL, "can't locate class");
    }

done:

    /* If the thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(pclass)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, NULL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__get_class_parent() */

/****************************************************************************************
 * Function:    H5P__close_class
 *
 *              Multithread safe version of H5P__close_class()
 *
 * Purpose:     Internal routine to close a property list class.
 *
 *              Check if the class is already marked deleted, and if it isn't mark
 *              it deleted.
 *
 *              Check if it's marked as closing, if it is throw an error, because it
 *              shouldn't be marked as closing until it is actually being closed.
 *
 *              Check if ref_count.pl == 0, ref_count.plc == 0, and ref_count.delete is
 *              TRUE. If one of those is not correct, return and when one of the
 *              ref_counts is decremented it will be checked again.
 *
 *              NOTE: H5P__close_class() should only be called by H5I when and ID ref
 *              count hits zero, or during the clean up of specific functions when an
 *              error occurs. Because of this ref_count.pl and ref_count.plc should
 *              always be 0 when H5P__close_class() is called, since derived objects
 *              also increment the ID ref count of their parent.
 *
 *              If both ref counts are zero, mark it as closing. This will prevent more
 *              threads from accessing the struct so it can close when any other current
 *              threads in the struct exit. We then check if it's opening or contains
 *              other threads. If either are true we sleep and check again.
 *
 *              When opening is FALSE and there are no other threads in the struct,
 *              increment the parent's thrd.count then decrement the parent's
 *              ref_count.plc.
 *
 *              Add this struct to the tail of the class free list. If the length of the
 *              class free list is greater than the max desired length attempt to free
 *              a struct from the free list, if one is available to be freed.
 *
 *              Decrement the parent's thrd.count.
 *
 *              Call H5I_dec_ref() to decrement the parent's ID index.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__close_class(H5P_mt_class_t *class)
{
    H5P_mt_class_t              *parent = NULL;
    H5P_mt_class_t              *fl_class;
    H5P_mt_class_sptr_t          fl_head;
    H5P_mt_class_sptr_t          fl_tail;
    H5P_mt_class_sptr_t          fl_next;
    H5P_mt_class_sptr_t          fl_update;
    H5P_mt_active_thread_count_t local_thrd;
    H5P_mt_active_thread_count_t closing_thrd;
    H5P_mt_class_ref_counts_t    ref_count;
    H5P_mt_class_ref_counts_t    update_rc; /* rc = ref_count */
    bool                         done              = FALSE;
    bool                         inc_thrd_flag     = FALSE;
    bool                         try_to_free_entry = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__close_class__num_calls), 1);

    assert(class);
    assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);
    fl_next = atomic_load(&(class->fl_next));

    ref_count = atomic_load(&(class->ref_count));

    /* If deleted is not already TRUE, atomically set it to TRUE */
    if (!ref_count.deleted) {
        do {
            ref_count = atomic_load(&(class->ref_count));

            update_rc         = ref_count;
            update_rc.deleted = TRUE;

            if (!atomic_compare_exchange_strong(&(class->ref_count), &ref_count, update_rc)) {
                /* failed, update stats */
                atomic_fetch_add(&(class->num_ref_count_cols), 1);

                /* assert is to not get stuck in an infinite loop while testing */
                assert(H5P_MT_ASSERT_FAIL);
            }
            else {
                /* success, update stats */
                atomic_fetch_add(&(class->num_ref_count_update), 1);
                atomic_fetch_add(&(class->num_ref_count_marked_deleted), 1);

                done = TRUE;
            }

        } while (!done);

        assert(done);

        done = FALSE;

    } /* end if ( ! ref_count.deleted ) */

    /* If closing is already TRUE, throw an error */

    local_thrd = atomic_load(&(class->thrd));

    if (local_thrd.closing) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Closing flag is already set.");
    }

    ref_count = atomic_load(&(class->ref_count));

    /**
     * If there are no active derived lists or classes, we can close this class.
     * Else, we must check again when a derived list or class count gets decremented.
     */
    if (ref_count.pl == 0 && ref_count.plc == 0 && ref_count.deleted) {
        /* Set the closing flag to TRUE to prevent new threads from entering the struct */
        do {
            local_thrd = atomic_load(&(class->thrd));

            closing_thrd         = local_thrd;
            closing_thrd.closing = TRUE;

            /* Atomically update closing flag to TRUE */
            if (!atomic_compare_exchange_strong(&(class->thrd), &local_thrd, closing_thrd)) {
                /* failed, update stats and try again */
                atomic_fetch_add(&(class->num_thrd_update_cols), 1);

                /* assert is to not get stuck in an infinite loop while testing */
                assert(H5P_MT_ASSERT_FAIL);
            }
            else {
                /* success, update stats and mark done */
                atomic_fetch_add(&(class->num_thrd_closing_flag_set), 1);

                done = TRUE;
            }

        } while (!done);

        assert(done);
        done = FALSE;

        local_thrd = atomic_load(&(class->thrd));
        assert(local_thrd.closing);

        /* If the struct is opening or has other threads wait and try again */
        do {
            /* If opening is TRUE, sleep and try again */
            if (local_thrd.opening) {
                atomic_fetch_add(&(class->num_thrd_opening_flag_set), 1);

                /** NOTE: This assert is to prevent an infinite loop while testing */
                assert(H5P_MT_ASSERT_FAIL);

                sleep(1);
            }
            /* If there are any other threads in the struct, wait for them to drain out */
            else if (local_thrd.count > 0) {
                /** NOTE: This assert is to prevent an infinite loop while testing */
                assert(H5P_MT_ASSERT_FAIL);

                sleep(1);
            }
            else {
                done = TRUE;
            }

            local_thrd = atomic_load(&(class->thrd));

        } while (!done);

        assert(done);
        done = FALSE;

        assert(local_thrd.opening == FALSE);
        assert(local_thrd.closing == TRUE);
        assert(local_thrd.count == 0);

        /* Decrement the parents ref count for derived classes */
        if (class->parent_ptr) {
            parent = class->parent_ptr;
            assert(parent);
            assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_TAG);

            /* update parent's thrd count */
            if (H5P__inc_thrd_count(parent) < 0) {
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment parent's thread count.");
            }

            inc_thrd_flag = TRUE;

            /* update parent's ref count of derived classes */
            if (H5P__dec_ref_count(parent, TRUE) < 0) {
                HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Couldn't decrement parent's plc ref count.");
            }

        } /* end if ( class->parent_ptr ) */

        /**
         * Atomically update the current tail of the class's
         * free list to point to this class being closed.
         */

        fl_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

        if (fl_tail.ptr) {
            do {
                fl_class = fl_tail.ptr;

                fl_next = atomic_load(&(fl_class->fl_next));

                assert(!fl_next.ptr);

                fl_update.ptr = class;
                fl_update.sn  = fl_next.sn + 1;

                if (!atomic_compare_exchange_strong(&(fl_class->fl_next), &fl_next, fl_update)) {
                    /* failed, update stats and try again */
                    atomic_fetch_add(&(H5P_mt_g.class_fl_next_update_cols), 1);

                    /* assert is to not get stuck in an infinite loop while testing */
                    assert(H5P_MT_ASSERT_FAIL);
                }
                else {
                    /* success, update stats and continue */
                    atomic_fetch_add(&(H5P_mt_g.class_fl_next_update), 1);

                    done = TRUE;
                }

            } while (!done);

        } /* end if ( fl_tail.ptr ) */

        done = FALSE;

        /* Atomically update the class_fl_tail to point to the new tail */

        do {
            fl_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

            fl_update.ptr = class;
            fl_update.sn  = fl_tail.sn + 1;

            if (!atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_tail), &fl_tail, fl_update)) {
                /* failed, updated stats and try again */
                atomic_fetch_add(&(H5P_mt_g.class_fl_tail_update_cols), 1);

                /* assert is to not get stuck in an infinite loop while testing */
                assert(H5P_MT_ASSERT_FAIL);
            }
            else {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.class_fl_tail_update), 1);
                atomic_fetch_add(&(H5P_mt_g.num_class_added_to_fl), 1);

                atomic_fetch_add(&(H5P_mt_g.class_fl_len), 1);

                if (atomic_load(&(H5P_mt_g.class_fl_len)) >
                    atomic_load(&(H5P_mt_g.class_max_desired_fl_len))) {
                    try_to_free_entry = TRUE;
                }

                done = TRUE;
            }

        } while (!done);

        /**
         * If this is the first class added to the class free list, have the
         * head pointer point to it as well.
         */
        fl_head = atomic_load(&(H5P_mt_g.class_fl_head));

        if (!fl_head.ptr) {
            done = FALSE;

            do {
                fl_head = atomic_load(&(H5P_mt_g.class_fl_head));

                fl_update.ptr = class;
                fl_update.sn  = fl_head.sn + 1;

                if (!atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_head), &fl_head, fl_update)) {
                    /* failed, updated stats and try again */
                    atomic_fetch_add(&(H5P_mt_g.class_fl_head_update_cols), 1);

                    /* assert is to not get stuck in an infinite loop while testing */
                    assert(H5P_MT_ASSERT_FAIL);
                }
                else {
                    /* success, update stats and continue */
                    atomic_fetch_add(&(H5P_mt_g.class_fl_head_update), 1);

                    done = TRUE;
                }

            } while (!done);

        } /* end if ( ! fl_head.ptr ) */

        atomic_store(&(class->tag), H5P_MT_CLASS_INVALID_TAG);

        /**
         * If TRUE then the free list has more entries then the desired max.
         * If an entry is reallocable, remove it from the free list and free
         * it to lower the number of entries.
         */
        if (try_to_free_entry) {
            done = FALSE;

            do {
                fl_head = atomic_load(&(H5P_mt_g.class_fl_head));
                fl_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

                assert(fl_head.ptr);
                assert(fl_tail.ptr);

                if (fl_head.ptr != fl_tail.ptr) {
                    fl_class = fl_head.ptr;

                    if (atomic_load(&(fl_class->tag)) == H5P_MT_CLASS_FL_REALLOC_TAG) {
                        fl_next = atomic_load(&(fl_class->fl_next));

                        if (!atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_head), &fl_head, fl_next)) {
                            atomic_fetch_add(&(H5P_mt_g.class_fl_head_update_cols), 1);
                        }
                        else {
                            atomic_fetch_add(&(H5P_mt_g.class_fl_head_update), 1);
                            atomic_fetch_add(&(H5P_mt_g.class_fl_head_freed_due_to_max_len), 1);

                            H5P__clear_mt_class(fl_class);

                            free(fl_class);

                            done = TRUE;
                        }
                    }
                    else {
                        atomic_fetch_add(&(H5P_mt_g.class_fl_head_free_skipped_no_reallocable), 1);

                        done = TRUE;
                    }
                }
                else {
                    atomic_fetch_add(&(H5P_mt_g.class_fl_head_free_skipped_due_to_empty), 1);

                    done = TRUE;
                }

            } while (!done);

        } /* end if ( try_to_free_entry ) */

    } /* end if ( ref_count.pl == 0 && ref_count.plc == 0 && ref_count.deleted ) */
    else {
        if (ref_count.pl > 0) {
            atomic_fetch_add(&(H5P_mt_g.close_class_but_pl_not_zero), 1);
        }
        if (ref_count.plc > 0) {
            atomic_fetch_add(&(H5P_mt_g.close_class_but_plc_not_zero), 1);
        }
    }

done:

    /* update parent's thrd count */
    if (parent != NULL && inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(parent)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }

        ref_count = atomic_load(&(parent->ref_count));

        if (ref_count.deleted == FALSE) {
            if (0 > H5I_dec_ref(atomic_load(&(parent->id)))) {
                assert(FALSE);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL,
                            "unable to decrement parent's ID ref_count in index");
            }
        }

    } /* end if ( parent != NULL && inc_thrd_flag ) */

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__close_class() MT safe version*/

/****************************************************************************************
 * Function:    H5P__clear_mt_class
 *
 * Purpose:     Frees all allocated memory in the class and clears all fields
 *
 *              This function first makes sure that no other thread is accessing this
 *              class struct, and then sets the tag to H5P_MT_CLASS_INVALID_TAG to mark
 *              that this class is no longer valid for threads to access.
 *
 *              Then it frees its name, adds all properties to the property free list,
 *              and resets all of its stats fields.
 *
 *
 * Return:      Success: Returns a pointer to the class after all data being cleared.
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_class_t *
H5P__clear_mt_class(H5P_mt_class_t *class)
{
    H5P_mt_active_thread_count_t thrd;
    H5P_mt_prop_t               *first_prop;
    H5P_mt_prop_aptr_t           next_ptr;
    H5P_mt_class_ref_counts_t    ref_count;
    size_t                       phys_pl_len;
    size_t                       i;

    H5P_mt_class_t *ret_value;

    FUNC_ENTER_PACKAGE

    assert(class);
    assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_FL_REALLOC_TAG);

    thrd = atomic_load(&(class->thrd));

    assert(thrd.count == 0);
    assert(thrd.opening == FALSE);
    assert(thrd.closing == TRUE);

    ref_count = atomic_load(&(class->ref_count));
    assert(ref_count.pl == 0);
    assert(ref_count.plc == 0);

    /* Clears the class's fields */

    atomic_store(&(class->tag), H5P_MT_CLASS_INVALID_TAG);

    class->parent_id  = H5I_INVALID_HID;
    class->parent_ptr = NULL;

    free(class->name);
    class->name = NULL;

    /** TODO: should a search of the index for this id be done first? */
    atomic_store(&(class->id), H5I_INVALID_HID);

    /* Iterate the LFSLL and add all properties to the free list */
    phys_pl_len = atomic_load(&(class->phys_pl_len));

    for (i = 0; i < phys_pl_len; i++) {
        first_prop = class->pl_head;
        assert(first_prop);
        assert(atomic_load(&(first_prop->tag)) == H5P_MT_PROP_TAG);

        next_ptr = atomic_load(&(first_prop->next));

        class->pl_head = next_ptr.ptr;

        if (H5P__mt_close_prop(first_prop) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Failed to add property to free list.");
        }

        atomic_fetch_sub(&(class->phys_pl_len), 1);

    } /* end for() */

    atomic_store(&(class->log_pl_len), 0);

    /* Ensure the LFSLL is empty, then free the class */
    first_prop = class->pl_head;
    assert(!first_prop);

    class->pl_head = NULL;

    assert(0 == (atomic_load(&(class->log_pl_len))));
    assert(0 == (atomic_load(&(class->phys_pl_len))));

    if (0 > (H5P__reset_stats_class(class))) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Failed resetting stats fields.");
    }

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.num_classes_freed), 1);

done:

    ret_value = class;

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__clear_mt_class() */

/****************************************************************************************
 * Function:    H5P__mt_next_prop_to_cmp
 *
 * Purpose:     Retrieves the next valid property that is not in a lkup_tbl.
 *
 *              Called by H5P__mt_cmp_list() this function ensures that when
 *              comparing two properties from the LFSLLs a property that has the flag
 *              in_lkup_tbl set to TRUE is not grabbed for comparison. The lkup_tbls are
 *              iterated and the current version of those properties is grabbed for the
 *              comparisons prior to comparing the LFSLLs. Thus, any valid property with
 *              in_lkup_tbl set to TRUE should have already been compared and must be
 *              skipped here. If there are no valid propertis with in_lkup_tbl set to
 *              FALSE then NULL is returned.
 *
 *
 * Return:      Success: Returns the next valid property that has in_lkup_tbl set to FALSE,
 *                       or NULL
 *
 *              Failure: Can not fail
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__mt_next_prop_to_cmp(H5P_mt_prop_t *prop, uint64_t version)
{
    H5P_mt_prop_t *valid_prop;
    H5P_mt_prop_t *prev_prop;

    H5P_mt_prop_t *ret_value;

    FUNC_ENTER_NOAPI_NOERR

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

    prev_prop = prop;

    if (NULL == (valid_prop = H5P__get_next_valid_prop(prev_prop, version, NULL))) {
        HGOTO_DONE(NULL);
    }
    if (valid_prop->in_lkup_tbl) {
        ret_value = H5P__mt_next_prop_to_cmp(valid_prop, version);
    }
    else {
        ret_value = valid_prop;
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_cmp_next_prop() */

/**
 * H5P__mt_encode() and H5P__mt_encod_prop() were multithread functions that
 * were planned to replace the existing H5P__encode() and H5P__encode_cb(),
 * however, due to lack of time and a low priority these were not finished.
 */
#if 0
/****************************************************************************************
 * Function:    H5P__mt_encode()
 *
 * Purpose:     Routine to convert the property values in a property list into a binary
 *              buffer. The encoding of property values will be done according to the
 *              file format setting in fapl_id.
 *
 *              NOTE: This routine is very limited in which lists can be encoded. Due to
 *              the way decoding the encoded binary works, only predefined property lists
 *              may be encoded, and only predefined ones that have not had any completely
 *              new properties added that don't exist in the parent class.
 *
 *              First increment the thread count of the list and set inc_thrd_flag to
 *              TRUE. Then call H5P__mt_enforce_serialization() to ensure there
 *              isn't another operation that can change the list occuring, before
 *              continuing.
 *
 *              The first two bytes are encoded, where the first is the encoding version
 *              and the second is the type of property list.
 *
 *              The lkup_tbl is iterated and the valid version of the property is found
 *              and H5P__mt_encode_prop() is called to encode the property's name and
 *              value.
 *
 *              The lfsll is then iterated via H5P__get_next_valid_prop() to get the
 *              valid properties and encode them via H5P__mt_encode_prop.
 *
 *              A terminator is encoded for the list.
 *
 *              Before returning, decrement the thread count of the list if
 *              inc_thrd_flag is TRUE.
 *
 * Return:      SUCCEED/FAIL

 ****************************************************************************************
 */
herr_t
H5P__mt_encode(H5P_mt_list_t *list, uint64_t version, void *buf, size_t *nalloc)
{
    H5P_mt_list_table_entry_t *entry;                          /* entry in the lkup_tbl */
    H5P_mt_prop_t             *valid_prop;                     /* prop to try encoding next */
    H5P_mt_prop_t             *prev_prop;                      /* previous valid_prop in the lfsll */
    uint8_t                   *p             = (uint8_t *)buf; /* tmp pointer to encode buffer */
    size_t                     encode_size   = 0;              /* size of buf needed to encode properties */
    bool                       encode        = TRUE;           /* bool for if the list should be encoded */
    bool                       base_flag     = FALSE;
    bool                       inc_thrd_flag = FALSE;
    bool                       ver_updated   = FALSE;
    uint64_t                   curr_version;
    uint64_t                   next_version;
    size_t                     nprops_inherited; /* Number of entries in the lkup_tbl */
    uint32_t                   idx;              /* Index of the lkup_tbl */

/**
 * Fields used in asserts during debug mode to ensure
 * the correct number of properties are being encoded.
 */
#ifndef NDEBUG
    size_t   nprops     = 0; /* total # of props in lkup_tbl + lfsll */
    size_t   log_pl_len = 0; /* Number of props in the lfsll */
    uint64_t prop_count = 0; /* Used to check the correct number of props were encoded */
#endif

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);
    assert(atomic_load(&(list->curr_version)) >= version);

    /* Increment thread count */
    if (H5P__inc_thrd_count(list) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "Couldn't increment list's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    curr_version = atomic_load(&(list->curr_version));
    next_version = atomic_fetch_add(&(list->next_version), 1);
    ver_updated  = TRUE;

    if ((curr_version + 1) < next_version) {
        if ((curr_version = H5P__mt_enforce_serialization(list, curr_version, next_version)) == 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }
    }

    nprops_inherited = list->nprops_inherited;

    /* Sanity Check */
    if (NULL == nalloc) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "bad allocation size pointer");
    }

    /* If buf is NULL, nothing to encode */
    if (NULL == p) {
        encode = FALSE;
    }

    /* If not false encode first two bytes of buf */
    if (encode) {
        /* Sets first byte to encoding version # */
        *p++ = (uint8_t)H5P_ENCODE_VERS;

        /* Sets second byte to type of property list */
        *p++ = (uint8_t)list->pclass_ptr->type;
    }

    encode_size += 2;

    /* Iterate the lkup_tbl entries */
    for (idx = 0; idx < nprops_inherited; idx++) {
        entry = &list->lkup_tbl[idx];

        valid_prop = H5P__mt_entry_find_version(entry, version, &base_flag);

        /* If prop isn't NULL, then it's a valid prop */
        if (valid_prop) {
            if (H5P__mt_encode_prop(valid_prop, encode, &encode_size, &p) < 0) {
                HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL,
                            "Error while encoding properties in the lkup_tbl");
            }
        }

    } /* end for ( idx = 0; idx < nprops_inherited; idx++ ) */

    assert(idx == nprops_inherited);

    /* Iterate the lfsll */
    prev_prop = list->pl_head;
    assert(prev_prop);
    assert(prev_prop->sentinel);

    do {
        /* Gets the next valid prop or NULL if there isn't another valid prop */
        valid_prop = H5P__get_next_valid_prop(prev_prop, version, NULL);

        if (valid_prop) {
#ifndef NDEBUG
            prop_count++;
#endif
            /* If in_lkup_tbl is TRUE, the property was already encoded */
            if (!valid_prop->in_lkup_tbl) {
                if (H5P__mt_encode_prop(valid_prop, encode, &encode_size, &p) < 0) {
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL,
                                "Error while encoding properties in the lfsll");
                }
            }
        }

    } while (valid_prop);

#ifndef NDEBUG
    log_pl_len = atomic_load(&(list->log_pl_len));
    nprops     = atomic_load(&(list->nprops));
    assert(prop_count == log_pl_len);
    assert(nprops == (idx + prop_count));
#endif

    /* Encode a terminator for the list of properties */
    if (encode) {
        *p++ = 0;
    }

    encode_size++;

    /* Set the size of the buffer needed to encode the property list */
    *nalloc = encode_size;

done:

    if (ver_updated) {
        /* Update the list's current version */
        curr_version = atomic_fetch_add(&(list->curr_version), 1);
        assert(curr_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(list->curr_version)) > atomic_load(&(H5P_mt_g.max_list_version_number))) {
            atomic_store(&(H5P_mt_g.max_list_version_number), atomic_load(&(list->curr_version)));
        }
    }

    /* If the thrd_count was incremented, decremented it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(list)) {
            HDONE_ERROR(H5E_PLIST, H5E_CANTDEC, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_encode() */

/****************************************************************************************
 * Function:    H5P__mt_encode_prop()
 *
 * Purpose:     Routine to encode the name and value of a property into the provided
 *              buffer.
 *
 * Return:      SUCCEED/FAIL

 ****************************************************************************************
 */
herr_t
H5P__mt_encode_prop(H5P_mt_prop_t *prop, bool encode, size_t *encode_size, uint8_t **p)
{
    H5P_mt_prop_value_t value;      /* Value structure for the property */
    size_t              value_size; /* Encoded size of the property's value */
    size_t              name_len;   /* len of the prop's name */

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(p);

    /* Check if the property can be encoded */
    if (prop->encode) {
        name_len = HDstrlen(prop->name) + 1;
        value    = atomic_load(&(prop->value));

        /* If encode is TRUE, encode the property's name */
        if (encode) {
            HDstrcpy((char *)*(p), prop->name);
            *(uint8_t **)(p) += name_len;
        }
        *encode_size += name_len;

        value_size = 0;

        /* If not NULL, encode the property value */
        if ((prop->encode)(value.ptr, (void **)&p, &value_size) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL, "property encoding routine failed");
        }

        *encode_size += value_size;

    } /* end if ( prop-> encode ) */

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_encode_prop() */
#endif

/****************************************************************************************
 * Function:    H5P__mt_close_prop
 *
 * Purpose:     Multithread version of a function to close a property.
 *
 *              If the property's ref_count is 0, then the property struct is removed
 *              from the lfsll of the list or class, and is then added to the tail of the
 *              property free list.
 *
 *              NOTE: currently properties stay on the free list until shutdown
 *              procedure. But eventually properties on the free list when there is a
 *              guarantee they will not be accessed again, their tag will be set to
 *              H5P_MT_PROP_FL_REALLOC_TAG and the structure can be reused the next time
 *              a new property structure is needed instead of allocated a new one from
 *              memory.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_close_prop(H5P_mt_prop_t *prop)
{
    H5P_mt_prop_t     *fl_prop;
    H5P_mt_prop_aptr_t fl_head;
    H5P_mt_prop_aptr_t fl_tail;
    H5P_mt_prop_aptr_t fl_next;
    H5P_mt_prop_aptr_t fl_update;
    bool               done              = FALSE;
    bool               try_to_free_entry = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(0 == (atomic_load(&(prop->ref_count))));

    atomic_store(&(prop->tag), H5P_MT_PROP_VALID_ONFL_TAG);

    /* intiate fl_update's fields */
    fl_update.ptr          = NULL;
    fl_update.deleted      = FALSE;
    fl_update.dummy_bool_1 = FALSE;
    fl_update.dummy_bool_2 = FALSE;
    fl_update.dummy_bool_3 = FALSE;

    fl_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

    /* Atomically update the current tail to point to the new tail */
    if (fl_tail.ptr) {
        do {
            fl_prop = fl_tail.ptr;

            fl_next = atomic_load(&(fl_prop->next));

            if (fl_next.ptr) {
                fl_next.ptr = NULL;

                atomic_store(&(fl_prop->next), fl_next);
            }

            fl_update.ptr = prop;

            if (!atomic_compare_exchange_strong(&(fl_prop->next), &fl_next, fl_update)) {
                /* failed, update stats and try again */
                atomic_fetch_add(&(H5P_mt_g.prop_fl_next_update_cols), 1);

                /* assert is to not get stuck in an infinite loop while testing */
                assert(H5P_MT_ASSERT_FAIL);
            }
            else {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.prop_fl_next_update), 1);

                done = TRUE;
            }

        } while (!done);

    } /* end if ( fl_tail.ptr ) */

    done = FALSE;

    /* Atomically updates the prop_fl_tail to point to the new tail */

    do {
        fl_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

        fl_update.ptr = prop;

        if (!atomic_compare_exchange_strong(&(H5P_mt_g.prop_fl_tail), &fl_tail, fl_update)) {
            /* failed, updated stats and try again */
            atomic_fetch_add(&(H5P_mt_g.prop_fl_tail_update_cols), 1);

            /* assert is to not get stuck in an infinite loop while testing */
            assert(H5P_MT_ASSERT_FAIL);
        }
        else {
            /* success, update stats and continue */
            atomic_fetch_add(&(H5P_mt_g.prop_fl_tail_update), 1);
            atomic_fetch_add(&(H5P_mt_g.num_props_added_to_fl), 1);

            atomic_fetch_add(&(H5P_mt_g.prop_fl_len), 1);

            if (atomic_load(&(H5P_mt_g.prop_fl_len)) > atomic_load(&(H5P_mt_g.prop_max_desired_fl_len))) {
                try_to_free_entry = TRUE;
            }

            done = TRUE;
        }

    } while (!done);

    if (try_to_free_entry) {
        done = FALSE;

        do {
            fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));
            fl_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

            assert(fl_head.ptr);
            assert(fl_tail.ptr);

            if (fl_head.ptr != fl_tail.ptr) {
                fl_prop = fl_head.ptr;

                if (atomic_load(&(fl_prop->tag)) == H5P_MT_PROP_FL_REALLOC_TAG) {
                    fl_next = atomic_load(&(fl_prop->next));

                    if (!atomic_compare_exchange_strong(&(H5P_mt_g.prop_fl_head), &fl_head, fl_next)) {
                        atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update_cols), 1);
                    }
                    else {
                        atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update), 1);
                        atomic_fetch_add(&(H5P_mt_g.prop_fl_head_freed_due_to_max_len), 1);

                        H5P__clear_mt_prop(fl_prop);

                        free(fl_prop);

                        done = TRUE;
                    }
                }
                else {
                    atomic_fetch_add(&(H5P_mt_g.prop_fl_head_free_skipped_no_reallocable), 1);

                    done = TRUE;
                }
            }
            else {
                atomic_fetch_add(&(H5P_mt_g.prop_fl_head_free_skipped_due_to_empty), 1);

                done = TRUE;
            }

        } while (!done);

    } /* end if ( try_to_free_entry ) */

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_close_prop() */

/****************************************************************************************
 * Function:    H5P__clear_mt_prop
 *
 * Purpose:     Clears all fields to default values, and frees a property's name and
 *              value buffers, if they exist.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__clear_mt_prop(H5P_mt_prop_t *prop)
{
    H5P_mt_prop_aptr_t  nulls_prop_ptr;
    H5P_mt_prop_value_t nulls_value = {NULL, 0ULL};
    H5P_mt_prop_value_t prop_value;

    H5P_mt_prop_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_FL_REALLOC_TAG);

    nulls_prop_ptr.ptr          = NULL;
    nulls_prop_ptr.deleted      = FALSE;
    nulls_prop_ptr.dummy_bool_1 = FALSE;
    nulls_prop_ptr.dummy_bool_2 = FALSE;
    nulls_prop_ptr.dummy_bool_3 = FALSE;

    assert(prop);

    if (0 != (atomic_load(&(prop->ref_count)))) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "The property being cleared has a non-zero ref_count.");
    }

    assert(H5P_MT_PROP_INVALID_TAG);

    /* Clear all fields */
    atomic_store(&(prop->next), nulls_prop_ptr);

    prop->chksum = 0;

    free(prop->name);
    prop->name = NULL;

    prop_value = atomic_load(&(prop->value));

    if (prop_value.ptr) {
        free(prop_value.ptr);
    }

    prop_value = nulls_value;
    atomic_store(&(prop->value), prop_value);

    atomic_store(&(prop->create_version), 0);
    atomic_store(&(prop->delete_version), 0);

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.num_props_freed), 1);

done:

    ret_value = prop;

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__clear_mt_prop() */

/****************************************************************************************
 * Function:    H5P__mt_enforce_serialization
 *
 * Purpose:     Compares the curr_version to the next_version.
 *
 *              To ensure true atomicity, any modification to either a class's or list's
 *              LFSLL must be done one thread at a time. So, the curr_version must be one
 *              less than next_version, and if curr_version is 2+ less than next_version
 *              it sleeps and checks again, to allow the thread currently modifying the
 *              struct time to finish.
 *
 *              NOTE: sleeping is a temporary solution and future iterations of this
 *              code will improve how this is handled.
 *
 *              NOTE: for more details on this process, see the description comment for
 *              H5P_mt_class_t in H5Ppkg_mt.h.
 *
 * Return:      Success: Returns curr_version that is one less than next_version
 *
 *              Failure: 0
 *
 ****************************************************************************************
 */
uint64_t
H5P__mt_enforce_serialization(void *param, uint64_t curr_version, uint64_t next_version)
{
    uint32_t tag;
    H5P_mt_class_t *class = NULL;
    H5P_mt_list_t *list   = NULL;

    uint64_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__mt_enforce_serialization__num_calls), 1);

    tag = *(uint32_t *)param;

    /* If param is a class */
    if (tag == H5P_MT_CLASS_TAG) {
        class = (H5P_mt_class_t *)param;
    }
    /* If param is a list */
    else if (tag == H5P_MT_LIST_TAG) {
        list = (H5P_mt_list_t *)param;
    }
    else {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, 0, "Tag is not a list or a class.");
    }

    while ((curr_version + 1) < next_version) {
        sleep(1);

        if (class) {
            atomic_fetch_add(&(class->num_wait_for_curr_version_to_inc), 1);
            curr_version = atomic_load(&(class->curr_version));

            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_classes_loop_enforce_serial), 1);
        }
        else {
            atomic_fetch_add(&(list->num_wait_for_curr_version_to_inc), 1);
            curr_version = atomic_load(&(list->curr_version));

            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.num_lists_loop_enforce_serial), 1);
        }

    } /* end while ( curr_version + 1 < next_version ) */

    if (curr_version >= next_version) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, 0, "Threads were not operated in correct version order.");
    }

    assert((curr_version + 1) == next_version);

    ret_value = curr_version;

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_enforce_serialization() */

/****************************************************************************************
 * Function:    H5P__inc_thrd_count
 *
 * Purpose:     Increments the count field in the thrd struct in either H5P_mt_class_t
 *              or H5P_mt_list_t, to track the number of threads currently accessing the
 *              class or list structure.
 *
 *              First determine if we're dealing with a class or a list, and then make
 *              sure it's not opening or closing. As long as neither are TRUE increment
 *              thrd.count and return TRUE, allowing the function that called this one to
 *              continue.
 *
 *              If opening is TRUE, then we sleep and check again. A class or list while
 *              opening is only to visible briefly to other threads before completing the
 *              opening process and setting opening to FALSE.
 *
 *              If closing is TRUE, throw an error, a thread should not be accessing a
 *              closing structure.
 *
 *              NOTE: sleeping is a temporary solution and future iterations of this
 *              code will improve how this is handled.
 *
 *              NOTE: for more details on this process, see the description comment for
 *              H5P_mt_class_t in H5Ppkg_mt.h.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__inc_thrd_count(void *param)
{
    uint32_t tag;
    H5P_mt_class_t *class             = NULL;
    H5P_mt_list_t               *list = NULL;
    H5P_mt_active_thread_count_t thrd;
    H5P_mt_active_thread_count_t update_thrd;
    bool                         done = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    tag = *(uint32_t *)param;

    /* If param is a class */
    if (tag == H5P_MT_CLASS_TAG) {
        class = (H5P_mt_class_t *)param;
    }
    /* If param is a list */
    else if (tag == H5P_MT_LIST_TAG) {
        list = (H5P_mt_list_t *)param;
    }
    else {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Tag is not a list or a class.");
    }

    /* Ensure the class isn't opening or closing and increment thread count */
    do {
        if (class) {
            thrd = atomic_load(&(class->thrd));
        }
        else {
            thrd = atomic_load(&(list->thrd));
        }

        /* If closing, throw an error */
        if (thrd.closing) {
            /* This assert is normally NULL, but may be set to fail here for testing */
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Closing flag is set.");
        }
        /* If opening, sleep and try again */
        else if (thrd.opening) {
            /* update stats */
            if (class) {
                atomic_fetch_add(&(class->num_thrd_opening_flag_set), 1);
            }
            else {
                atomic_fetch_add(&(list->num_thrd_opening_flag_set), 1);
            }

#ifndef H5_HAVE_MULTITHREAD

            /**
             * This is for testing that this MT safety net gets trigged while running
             * tests in single thread, to prevent an infinite loop.
             */
            return FAIL;
#else

            sleep(1);

#endif
        }
        else {
            update_thrd = thrd;
            update_thrd.count++;

            if (class) {
                if (!atomic_compare_exchange_strong(&(class->thrd), &thrd, update_thrd)) {
                    /* attempt failed, update stats and try again */
                    atomic_fetch_add(&(class->num_thrd_update_cols), 1);

                    /* assert is to not get stuck in an infinite loop while testing */
                    assert(H5P_MT_ASSERT_FAIL);
                }
                else {
                    /* attempt succeded update stats and set done */
                    atomic_fetch_add(&(class->num_thrd_count_update), 1);

                    done = TRUE;
                }
            }
            else {
                if (!atomic_compare_exchange_strong(&(list->thrd), &thrd, update_thrd)) {
                    /* attempt failed, update stats and try again */
                    atomic_fetch_add(&(list->num_thrd_update_cols), 1);

                    /* assert is to not get stuck in an infinite loop while testing */
                    assert(H5P_MT_ASSERT_FAIL);
                }
                else {
                    /* attempt succeded update stats and set done */
                    atomic_fetch_add(&(list->num_thrd_count_update), 1);

                    done = TRUE;
                }
            }

        } /* end else */

    } while (!done);

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__inc_thrd_count() */

/****************************************************************************************
 * Function:    H5P__dec_thrd_count
 *
 * Purpose:     Decrements the count field in the thrd struct in either H5P_mt_class_t
 *              or H5P_mt_list_t, to track the number of threads currently accessing the
 *              class or list structure.
 *
 *              First determine if we're dealing with a class or a list, and then
 *              decrement thrd.count and return TRUE, allowing the function that called
 *              this one to continue.
 *
 *              We do not check if the struct is opening or closing, because
 *              H5P__inc_thrd_count() is called before this function so, opening must be
 *              FALSE. Closing cannot be set to TRUE as long as a thrd.count is > 1, so
 *              it also cannot be TRUE until after this function is completed.
 *
 *              NOTE: for more details on this process, see the description comment for
 *              H5P_mt_class_t in H5Ppkg_mt.h.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__dec_thrd_count(void *param)
{
    uint32_t tag;
    H5P_mt_class_t *class             = NULL;
    H5P_mt_list_t               *list = NULL;
    H5P_mt_active_thread_count_t thrd;
    H5P_mt_active_thread_count_t update_thrd;
    bool                         done = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    tag = *(uint32_t *)param;

    /* If param is a class */
    if (tag == H5P_MT_CLASS_TAG) {
        class = (H5P_mt_class_t *)param;
    }
    /* If param is a list */
    else if (tag == H5P_MT_LIST_TAG) {
        list = (H5P_mt_list_t *)param;
    }
    else {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Tag is not a list or a class.");
    }

    /* Ensure the class isn't opening and decrement the thread count */
    do {
        if (class) {
            thrd = atomic_load(&(class->thrd));
        }
        else {
            thrd = atomic_load(&(list->thrd));
        }

        if (thrd.opening) {
            assert(FALSE);
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Opening flag is set, but shouldn't be");
        }

        update_thrd = thrd;
        update_thrd.count--;

        if (class) {
            if (!atomic_compare_exchange_strong(&(class->thrd), &thrd, update_thrd)) {
                /* attempt failed, update stats and try again */
                atomic_fetch_add(&(class->num_thrd_update_cols), 1);

                /* assert is to not get stuck in an infinite loop while testing */
                assert(H5P_MT_ASSERT_FAIL);
            }
            else {
                /* attempt succeded update stats and set done */
                atomic_fetch_add(&(class->num_thrd_count_update), 1);

                done = TRUE;
            }
        }
        else {
            if (!atomic_compare_exchange_strong(&(list->thrd), &thrd, update_thrd)) {
                /* attempt failed, update stats and try again */
                atomic_fetch_add(&(list->num_thrd_update_cols), 1);

                /* assert is to not get stuck in an infinite loop while testing */
                assert(H5P_MT_ASSERT_FAIL);
            }
            else {
                /* attempt succeded update stats and set done */
                atomic_fetch_add(&(list->num_thrd_count_update), 1);

                done = TRUE;
            }
        }

    } while (!done);

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__dec_thrd_count() */

/****************************************************************************************
 * Function:    H5P__inc_ref_count
 *
 * Purpose:     Increments the reference count of a class for either plc or pl, depending
 *              on if the the new struct derived from the parent is a H5P_mt_class_t or a
 *              H5P_mt_list_t.
 *
 *              NOTE: for more details on this process, see the description comment for
 *              H5P_mt_class_t in H5Ppkg_mt.h.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__inc_ref_count(H5P_mt_class_t *parent, bool plc)
{
    H5P_mt_class_ref_counts_t ref_count;
    H5P_mt_class_ref_counts_t update_ref;
    bool                      done = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    do {
        ref_count  = atomic_load(&(parent->ref_count));
        update_ref = ref_count;

        if (plc) {
            update_ref.plc++;
        }
        else {
            update_ref.pl++;
        }

        /* Attempt to atomically update the parent class's count of derived classes */
        if (!atomic_compare_exchange_strong(&(parent->ref_count), &ref_count, update_ref)) {
            /* attempt failed, update stats and try again */
            atomic_fetch_add(&(parent->num_ref_count_cols), 1);

            /* assert is to not get stuck in an infinite loop while testing */
            assert(H5P_MT_ASSERT_FAIL);
        }
        else /* Attempt was successful */
        {
            /* attempt succeeded, update stats and continue */
            atomic_fetch_add(&(parent->num_ref_count_update), 1);

            done = TRUE;
        }

    } while (!done);

    /* update stats */
    if (update_ref.deleted) {
        atomic_fetch_add(&(parent->num_ref_count_inc_while_deleted), 1);
    }

    if (update_ref.plc > ref_count.plc) {
        if (update_ref.plc > atomic_load(&(H5P_mt_g.max_derived_classes))) {
            atomic_store(&(H5P_mt_g.max_derived_classes), update_ref.plc);
        }
    }
    else if (update_ref.pl > ref_count.pl) {
        if (update_ref.pl > atomic_load(&(H5P_mt_g.max_derived_lists))) {
            atomic_store(&(H5P_mt_g.max_derived_lists), update_ref.pl);
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__inc_ref_count () */

/****************************************************************************************
 * Function:    H5P__dec_ref_count
 *
 * Purpose:     Decrements the reference count of a class for either plc or pl, depending
 *              on if the derived struct being deleted is a H5P_mt_class_t or a
 *              H5P_mt_list_t respectively.
 *
 *              NOTE: for more details on this process, see the description comment for
 *              H5P_mt_class_t in H5Ppkg_mt.h.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__dec_ref_count(H5P_mt_class_t *parent, bool plc)
{
    H5P_mt_class_ref_counts_t ref_count;
    H5P_mt_class_ref_counts_t update_ref;
    bool                      done = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    do {
        ref_count  = atomic_load(&(parent->ref_count));
        update_ref = ref_count;

        if (plc) {
            update_ref.plc--;
        }
        else {
            update_ref.pl--;
        }

        /* Attempt to atomically update the parent class's count of derived classes */
        if (!atomic_compare_exchange_strong(&(parent->ref_count), &ref_count, update_ref)) {
            /* attempt failed, update stats and try again */
            atomic_fetch_add(&(parent->num_ref_count_cols), 1);

            /* assert is to not get stuck in an infinite loop while testing */
            assert(H5P_MT_ASSERT_FAIL);
        }
        else /* Attempt was successful */
        {
            /* attempt succeeded, update stats and continue */
            atomic_fetch_add(&(parent->num_ref_count_update), 1);

            done = TRUE;
        }

    } while (!done);

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__dec_ref_count() */

/****************************************************************************************
 * Function:    H5P__calc_avg_visited
 *
 * Purpose:     Calculates an updated average for the stats that track average.
 *
 * Return:      Success: New average
 *
 *              Failure: can't fail
 *
 ****************************************************************************************
 */
uint64_t
H5P__calc_avg_visited(uint64_t avg_visited, uint64_t num_calls, uint64_t visited)
{
    int64_t avg;
    int64_t calls;
    int64_t v;

    uint64_t ret_value = 0;

    avg   = (int64_t)avg_visited;
    calls = (int64_t)num_calls;
    v     = (int64_t)visited;

    if (avg == 0) {
        avg++;
    }

    avg = avg + (((v - avg) + (calls / 2)) / calls);

    ret_value = (uint64_t)avg;

    return (ret_value);

} /* H5P__calc_avg_visited() */

/****************************************************************************************
 * Function:    H5P__new_plist_of_type
 *
 * Purpose:     Multithread version of H5P__new_plist_of_type() which is the same as the
 *              original version, but uses the multithread safe structures.
 *
 * Return:      Success: Returns a pointer to the new H5P_mt_class_t structure
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
hid_t
H5P__new_plist_of_type(H5P_plist_type_t type)
{
    H5P_mt_class_t *pclass;
    hid_t           class_id; /* ID of class to create */

    hid_t ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity checks */
    HDcompile_assert(H5P_TYPE_REFERENCE_ACCESS == (H5P_TYPE_MAX_TYPE - 1));
    assert(type >= H5P_TYPE_USER && type <= H5P_TYPE_REFERENCE_ACCESS);

    /* Check arguments */
    if (type == H5P_TYPE_USER) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, H5I_INVALID_HID, "can't create user property list");
    }
    if (type == H5P_TYPE_ROOT) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, H5I_INVALID_HID,
                    "shouldn't be creating root class property list");
    }

    /* Instantiate a property list of the proper type */
    switch (type) {
        case H5P_TYPE_OBJECT_CREATE:
            class_id = H5P_CLS_OBJECT_CREATE_ID_g;
            break;

        case H5P_TYPE_FILE_CREATE:
            class_id = H5P_CLS_FILE_CREATE_ID_g;
            break;

        case H5P_TYPE_FILE_ACCESS:
            class_id = H5P_CLS_FILE_ACCESS_ID_g;
            break;

        case H5P_TYPE_DATASET_CREATE:
            class_id = H5P_CLS_DATASET_CREATE_ID_g;
            break;

        case H5P_TYPE_DATASET_ACCESS:
            class_id = H5P_CLS_DATASET_ACCESS_ID_g;
            break;

        case H5P_TYPE_DATASET_XFER:
            class_id = H5P_CLS_DATASET_XFER_ID_g;
            break;

        case H5P_TYPE_FILE_MOUNT:
            class_id = H5P_CLS_FILE_MOUNT_ID_g;
            break;

        case H5P_TYPE_GROUP_CREATE:
            class_id = H5P_CLS_GROUP_CREATE_ID_g;
            break;

        case H5P_TYPE_GROUP_ACCESS:
            class_id = H5P_CLS_GROUP_ACCESS_ID_g;
            break;

        case H5P_TYPE_DATATYPE_CREATE:
            class_id = H5P_CLS_DATATYPE_CREATE_ID_g;
            break;

        case H5P_TYPE_DATATYPE_ACCESS:
            class_id = H5P_CLS_DATATYPE_ACCESS_ID_g;
            break;

        case H5P_TYPE_MAP_CREATE:
            class_id = H5P_CLS_MAP_CREATE_ID_g;
            break;

        case H5P_TYPE_MAP_ACCESS:
            class_id = H5P_CLS_MAP_ACCESS_ID_g;
            break;

        case H5P_TYPE_STRING_CREATE:
            class_id = H5P_CLS_STRING_CREATE_ID_g;
            break;

        case H5P_TYPE_ATTRIBUTE_CREATE:
            class_id = H5P_CLS_ATTRIBUTE_CREATE_ID_g;
            break;

        case H5P_TYPE_ATTRIBUTE_ACCESS:
            class_id = H5P_CLS_ATTRIBUTE_ACCESS_ID_g;
            break;

        case H5P_TYPE_OBJECT_COPY:
            class_id = H5P_CLS_OBJECT_COPY_ID_g;
            break;

        case H5P_TYPE_LINK_CREATE:
            class_id = H5P_CLS_LINK_CREATE_ID_g;
            break;

        case H5P_TYPE_LINK_ACCESS:
            class_id = H5P_CLS_LINK_ACCESS_ID_g;
            break;

        case H5P_TYPE_VOL_INITIALIZE:
            class_id = H5P_CLS_VOL_INITIALIZE_ID_g;
            break;

        case H5P_TYPE_REFERENCE_ACCESS:
            class_id = H5P_CLS_REFERENCE_ACCESS_ID_g;
            break;

        case H5P_TYPE_USER: /* shut compiler warnings up */
        case H5P_TYPE_ROOT:
        case H5P_TYPE_MAX_TYPE:
        default:
            HGOTO_ERROR(H5E_PLIST, H5E_BADRANGE, H5I_INVALID_HID, "invalid property list type: %u\n",
                        (unsigned)type);
    } /* end switch */

    /* Get the class object */
    if (NULL == (pclass = (H5P_mt_class_t *)H5I_object(class_id))) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, H5I_INVALID_HID, "not a property class");
    }

    /* Create the new property list */
    if ((ret_value = H5P_create_id(pclass, TRUE)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, H5I_INVALID_HID, "unable to create property list");
    }

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P__new_plist_of_type() MT safe version */

/****************************************************************************************
 * Function:    H5P_get_class
 *
 *              Multithread safe version of H5P_get_class().
 *
 * Purpose:     Internal routine which queries the parent class of a property list.
 *
 *              Additionally checks if the deleted flag has been set, and if so
 *              atomically resets it to FALSE.
 *
 *              NOTE: in the current iteration of multithread H5P a class's delete flag
 *              should not be set to TRUE while the class has existing derived objects,
 *              but this is left as a safety net.
 *
 * Return:      Success:    Non-NULL class of property list.
 *              Failure:    NULL
 *
 ****************************************************************************************
 */
H5P_genclass_t *
H5P_get_class(H5P_genplist_t *plist)
{
    H5P_mt_class_t           *pclass;
    H5P_mt_class_ref_counts_t ref_count;
    H5P_mt_class_ref_counts_t local_rc;
    bool                      done = FALSE;

    H5P_mt_class_t *ret_value;

    FUNC_ENTER_NOAPI_NOERR

    assert(plist);
    assert(atomic_load(&(plist->tag)) == H5P_MT_LIST_TAG);

    pclass = plist->pclass_ptr;

    assert(pclass);
    assert(atomic_load(&(pclass->tag)) == H5P_MT_CLASS_TAG);

    /* Ensure the class isn't marked deleted */
    do {
        ref_count = atomic_load(&(pclass->ref_count));

        if (ref_count.deleted) {
            local_rc         = ref_count;
            local_rc.deleted = FALSE;

            if (!atomic_compare_exchange_strong(&(pclass->ref_count), &ref_count, local_rc)) {
                atomic_fetch_add(&(pclass->num_ref_count_cols), 1);
            }
            else {
                atomic_fetch_add(&(pclass->num_ref_count_unmarked_deleted), 1);
                atomic_fetch_add(&(H5P_mt_g.class_un_marked_as_deleted), 1);

                done = TRUE;
            }
        }
        else {
            done = TRUE;
        }

    } while (!done);

    ret_value = pclass;

    FUNC_LEAVE_NOAPI(ret_value);

} /* end H5P_get_class() MT safe version */

/****************************************************************************************
 * Function:    H5P__grab_global_mutex
 *
 * Purpose:
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__grab_global_mutex(bool *have_global_mutex, bool *mutex_acquired)
{
    bool done = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    atomic_fetch_add(&(H5P_mt_g.H5P__grab_global_mutex__num_calls), 1);

    do {
        /* Check if we already have the global mutex */
        if (H5TS_have_mutex(&H5_g.init_lock, have_global_mutex) < 0) {
            HGOTO_ERROR(H5E_LIB, H5E_CANTGET, FAIL, "Can't determine whether we have the global mutex");
        }

        /* If we already have the global mutex, update stats and set done */
        if (*have_global_mutex) {
            atomic_fetch_add(&(H5P_mt_g.num_already_have_global_mutex), 1);
            done = TRUE;
        }
        /* Else, attempt to acquire the global mutex */
        else {

            if (H5TS_mutex_acquire(&H5_g.init_lock, 1, mutex_acquired) < 0) {
                HGOTO_ERROR(H5E_INTERNAL, H5E_SYSERRSTR, FAIL, "H5TS_mutex_acquire reported failure");
            }
            else {
                /**
                 * Failed to acquire the global mutex, probably because another thread has it.
                 * Sleep and try again.
                 *
                 * TODO: may need to add code to handle the case if this a deadlock.
                 */
                if ((*mutex_acquired) == FALSE) {
                    atomic_fetch_add(&(H5P_mt_g.global_mutex_acquire_failures), 1);

                    sleep(1);

                    continue;
                }
                /* Acquired global mutex, update stats and set done */
                else {
                    atomic_fetch_add(&(H5P_mt_g.global_mutex_acquire_success), 1);
                    done = TRUE;
                }
            }
        }

    } while (!done);

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5P__grab_global_mutex() */

/****************************************************************************************
 * Function:    H5P__global_lock_prop_cb__create
 *
 * Purpose:
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__global_lock_prop_cb__create(H5P_mt_prop_t *prop, const char *name, size_t size, void *value)
{
    bool have_global_mutex = FALSE;
    bool mutex_acquired    = FALSE;
    // bool   done              = FALSE;
    // bool   cb_error          = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(name);
    assert(size > 0);
    assert(value);

    atomic_fetch_add(&(H5P_mt_g.H5P__global_lock_prop_cb__create__num_calls), 1);

    /**
     * Check if we already have the global mutex,
     * and if we don't grab it.
     */
    if (H5P__grab_global_mutex(&have_global_mutex, &mutex_acquired) < 0) {
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "Failed checking or grabbing mutex");
    }

    /* update stats */
    if (have_global_mutex) {
        atomic_fetch_add(&(H5P_mt_g.num_already_have_mutex__create_cb), 1);
    }

    /* Call the user's callback */
    if ((prop->create)(prop->name, size, value) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "property create callback failed");
    }

done:

    /**
     * If the global mutex was grabbed in this function,
     * it must be released.
     */
    if (mutex_acquired) {
        H5_API_UNLOCK
        atomic_fetch_add(&(H5P_mt_g.global_mutex_unlocks), 1);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5P__global_lock_prop_cb__create() */

/****************************************************************************************
 * Function:    H5P__global_lock_prop_cb__set
 *
 * Purpose:
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__global_lock_prop_cb__set(H5P_mt_prop_t *prop, hid_t plist_id, const char *name, size_t size, void *value)
{
    bool have_global_mutex = TRUE;  /* trivially so in single thread builds */
    bool mutex_acquired    = FALSE; /* flag for if we have acquired the global mutex */
    // bool   done              = FALSE;
    // bool   cb_error          = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(plist_id != H5I_INVALID_HID);
    assert(name);
    assert(size > 0);
    assert(value);

    atomic_fetch_add(&(H5P_mt_g.H5P__global_lock_prop_cb__set__num_calls), 1);

    /**
     * Check if we already have the global mutex,
     * and if we don't grab it.
     */
    if (H5P__grab_global_mutex(&have_global_mutex, &mutex_acquired) < 0) {
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "Failed checking or grabbing mutex");
    }

    /* update stats */
    if (have_global_mutex) {
        atomic_fetch_add(&(H5P_mt_g.num_already_have_mutex__set_cb), 1);
    }

    /* Call the user's callback */
    if ((*(prop->set))(plist_id, name, size, value) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "property set callback failed");
    }

done:

    /**
     * If the global mutex was grabbed in this function,
     * it must be released.
     */
    if (mutex_acquired) {
        H5_API_UNLOCK
        atomic_fetch_add(&(H5P_mt_g.global_mutex_unlocks), 1);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5P__global_lock_prop_cb__set() */

/****************************************************************************************
 * Function:    H5P__global_lock_prop_cb__get
 *
 * Purpose:
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__global_lock_prop_cb__get(H5P_mt_prop_t *prop, hid_t plist_id, const char *name, size_t size, void *value)
{
    bool have_global_mutex = TRUE;  /* trivially so in single thread builds */
    bool mutex_acquired    = FALSE; /* flag for if we have acquired the global mutex */
    // bool   done              = FALSE;
    // bool   cb_error          = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(plist_id != H5I_INVALID_HID);
    assert(name);
    assert(size > 0);
    assert(value);

    atomic_fetch_add(&(H5P_mt_g.H5P__global_lock_prop_cb__get__num_calls), 1);

    /**
     * Check if we already have the global mutex,
     * and if we don't grab it.
     */
    if (H5P__grab_global_mutex(&have_global_mutex, &mutex_acquired) < 0) {
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "Failed checking or grabbing mutex");
    }

    /* update stats */
    if (have_global_mutex) {
        atomic_fetch_add(&(H5P_mt_g.num_already_have_mutex__get_cb), 1);
    }

    /* Call the user's callback */
    if ((*(prop->get))(plist_id, name, size, value) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "property get callback failed");
    }

done:

    /**
     * If the global mutex was grabbed in this function,
     * it must be released.
     */
    if (mutex_acquired) {
        H5_API_UNLOCK
        atomic_fetch_add(&(H5P_mt_g.global_mutex_unlocks), 1);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5P__global_lock_prop_cb__get() */

/****************************************************************************************
 * Function:    H5P__global_lock_prop_cb__encode
 *
 * Purpose:
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__global_lock_prop_cb__encode(H5P_mt_prop_t *prop, void *value, void **pp, size_t *value_len)
{
    bool have_global_mutex = TRUE;  /* trivially so in single thread builds */
    bool mutex_acquired    = FALSE; /* flag for if we have acquired the global mutex */
    // bool   done              = FALSE;
    // bool   cb_error          = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(value);
    assert(pp);
    // assert(*pp);
    assert(value_len);

    atomic_fetch_add(&(H5P_mt_g.H5P__global_lock_prop_cb__encode__num_calls), 1);

    /**
     * Check if we already have the global mutex,
     * and if we don't grab it.
     */
    if (H5P__grab_global_mutex(&have_global_mutex, &mutex_acquired) < 0) {
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "Failed checking or grabbing mutex");
    }

    /* update stats */
    if (have_global_mutex) {
        atomic_fetch_add(&(H5P_mt_g.num_already_have_mutex__encode_cb), 1);
    }

    /* Call the user's callback */
    if ((*(prop->encode))(value, pp, value_len) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL, "property encode callback failed");
    }

done:

    /**
     * If the global mutex was grabbed in this function,
     * it must be released.
     */
    if (mutex_acquired) {
        H5_API_UNLOCK
        atomic_fetch_add(&(H5P_mt_g.global_mutex_unlocks), 1);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5P__global_lock_prop_cb__encode() */

/****************************************************************************************
 * Function:    H5P__global_lock_prop_cb__decode
 *
 * Purpose:
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__global_lock_prop_cb__decode(H5P_mt_prop_t *prop, const void **pp, void *value_buf)
{
    bool have_global_mutex = TRUE;  /* trivially so in single thread builds */
    bool mutex_acquired    = FALSE; /* flag for if we have acquired the global mutex */
    // bool   done              = FALSE;
    // bool   cb_error          = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(pp);
    assert(*pp);
    assert(value_buf);

    atomic_fetch_add(&(H5P_mt_g.H5P__global_lock_prop_cb__decode__num_calls), 1);

    /**
     * Check if we already have the global mutex,
     * and if we don't grab it.
     */
    if (H5P__grab_global_mutex(&have_global_mutex, &mutex_acquired) < 0) {
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "Failed checking or grabbing mutex");
    }

    /* update stats */
    if (have_global_mutex) {
        atomic_fetch_add(&(H5P_mt_g.num_already_have_mutex__decode_cb), 1);
    }

    /* Call the user's callback */
    if ((*(prop->decode))(pp, value_buf) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTDECODE, FAIL, "property decode callback failed");
    }

done:

    /**
     * If the global mutex was grabbed in this function,
     * it must be released.
     */
    if (mutex_acquired) {
        H5_API_UNLOCK
        atomic_fetch_add(&(H5P_mt_g.global_mutex_unlocks), 1);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5P__global_lock_prop_cb__decode() */

/****************************************************************************************
 * Function:    H5P__global_lock_prop_cb__del
 *
 * Purpose:
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__global_lock_prop_cb__del(H5P_mt_prop_t *prop, hid_t plist_id, const char *name, size_t size, void *value)
{
    bool have_global_mutex = TRUE;  /* trivially so in single thread builds */
    bool mutex_acquired    = FALSE; /* flag for if we have acquired the global mutex */
    // bool   done              = FALSE;
    // bool   cb_error          = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(plist_id != H5I_INVALID_HID);
    assert(name);
    assert(size > 0);
    assert(value);

    atomic_fetch_add(&(H5P_mt_g.H5P__global_lock_prop_cb__del__num_calls), 1);

    /**
     * Check if we already have the global mutex,
     * and if we don't grab it.
     */
    if (H5P__grab_global_mutex(&have_global_mutex, &mutex_acquired) < 0) {
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "Failed checking or grabbing mutex");
    }

    /* update stats */
    if (have_global_mutex) {
        atomic_fetch_add(&(H5P_mt_g.num_already_have_mutex__del_cb), 1);
    }

    /* Call the user's callback */
    if ((*(prop->del))(plist_id, name, size, value) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTFREE, FAIL, "property del callback failed");
    }

done:

    /**
     * If the global mutex was grabbed in this function,
     * it must be released.
     */
    if (mutex_acquired) {
        H5_API_UNLOCK
        atomic_fetch_add(&(H5P_mt_g.global_mutex_unlocks), 1);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5P__global_lock_prop_cb__del() */

/****************************************************************************************
 * Function:    H5P__global_lock_prop_cb__copy
 *
 * Purpose:
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__global_lock_prop_cb__copy(H5P_mt_prop_t *prop, const char *name, size_t size, void *value)
{
    bool have_global_mutex = TRUE;  /* trivially so in single thread builds */
    bool mutex_acquired    = FALSE; /* flag for if we have acquired the global mutex */
    // bool   done              = FALSE;
    // bool   cb_error          = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(name);
    assert(size > 0);
    assert(value);

    atomic_fetch_add(&(H5P_mt_g.H5P__global_lock_prop_cb__copy__num_calls), 1);

    /**
     * Check if we already have the global mutex,
     * and if we don't grab it.
     */
    if (H5P__grab_global_mutex(&have_global_mutex, &mutex_acquired) < 0) {
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "Failed checking or grabbing mutex");
    }

    /* update stats */
    if (have_global_mutex) {
        atomic_fetch_add(&(H5P_mt_g.num_already_have_mutex__copy_cb), 1);
    }

    /* Call the user's callback */
    if ((*(prop->copy))(name, size, value) < 0) {
        HDONE_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "property copy callback failed");
        ;
    }

done:

    /**
     * If the global mutex was grabbed in this function,
     * it must be released.
     */
    if (mutex_acquired) {
        H5_API_UNLOCK
        atomic_fetch_add(&(H5P_mt_g.global_mutex_unlocks), 1);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5P__global_lock_prop_cb__copy() */

/****************************************************************************************
 * Function:    H5P__global_lock_prop_cb__cmp
 *
 * Purpose:
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__global_lock_prop_cb__cmp(H5P_mt_prop_t *prop, void *value1, void *value2, size_t size)
{
    bool have_global_mutex = TRUE;  /* trivially so in single thread builds */
    bool mutex_acquired    = FALSE; /* flag for if we have acquired the global mutex */
    // bool   done              = FALSE;
    int cmp_value = 0;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG ||
           atomic_load(&(prop->tag)) == H5P_MT_PROP_VALID_ONFL_TAG);
    assert(value1);
    assert(value2);
    assert(size > 0);

    atomic_fetch_add(&(H5P_mt_g.H5P__global_lock_prop_cb__cmp__num_calls), 1);

    /**
     * Check if we already have the global mutex,
     * and if we don't grab it.
     */
    if (H5P__grab_global_mutex(&have_global_mutex, &mutex_acquired) < 0) {
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "Failed checking or grabbing mutex");
    }

    /* update stats */
    if (have_global_mutex) {
        atomic_fetch_add(&(H5P_mt_g.num_already_have_mutex__cmp_cb), 1);
    }

    /* Call the user's callback */
    if ((cmp_value = prop->cmp(value1, value2, size)) != 0) {
        ret_value = FAIL;
    }

done:

    /**
     * If the global mutex was grabbed in this function,
     * it must be released.
     */
    if (mutex_acquired) {
        H5_API_UNLOCK
        atomic_fetch_add(&(H5P_mt_g.global_mutex_unlocks), 1);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5P__global_lock_prop_cb__cmp() */

/****************************************************************************************
 * Function:    H5P__global_lock_prop_cb__close
 *
 * Purpose:
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__global_lock_prop_cb__close(H5P_mt_prop_t *prop, const char *name, size_t size, void *value)
{
    bool have_global_mutex = TRUE;  /* trivially so in single thread builds */
    bool mutex_acquired    = FALSE; /* flag for if we have acquired the global mutex */
    // bool   done              = FALSE;
    // bool   cb_error          = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(name);
    assert(size > 0);
    assert(value);

    atomic_fetch_add(&(H5P_mt_g.H5P__global_lock_prop_cb__close__num_calls), 1);

    /**
     * Check if we already have the global mutex,
     * and if we don't grab it.
     */
    if (H5P__grab_global_mutex(&have_global_mutex, &mutex_acquired) < 0) {
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "Failed checking or grabbing mutex");
    }

    /* update stats */
    if (have_global_mutex) {
        atomic_fetch_add(&(H5P_mt_g.num_already_have_mutex__close_cb), 1);
    }

    /**
     * Call the close callback and ignore the return value,
     * there's nothing we can do about it
     */
    (*(prop->close))(name, size, value);

done:

    /**
     * If the global mutex was grabbed in this function,
     * it must be released.
     */
    if (mutex_acquired) {
        H5_API_UNLOCK
        atomic_fetch_add(&(H5P_mt_g.global_mutex_unlocks), 1);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5P__global_lock_prop_cb__close() */

/****************************************************************************************
 * Function:    H5P__init_stats_global
 *
 * Purpose:     Initializes the stats fields for the H5P_mt_g global struct
 *
 *              NOTE: These statistics are only  maintained in the multi-thread
 *              implementation of H5P.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__init_stats_global(void)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    /* Property free list stats */
    atomic_init(&(H5P_mt_g.prop_fl_head_update), 0ULL);
    atomic_init(&(H5P_mt_g.prop_fl_head_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.prop_fl_tail_update), 0ULL);
    atomic_init(&(H5P_mt_g.prop_fl_tail_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.prop_fl_next_update), 0ULL);
    atomic_init(&(H5P_mt_g.prop_fl_next_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.num_props_added_to_fl), 0ULL);
    atomic_init(&(H5P_mt_g.prop_fl_head_freed_due_to_max_len), 0ULL);
    atomic_init(&(H5P_mt_g.prop_fl_head_free_skipped_due_to_empty), 0ULL);
    atomic_init(&(H5P_mt_g.prop_fl_head_free_skipped_no_reallocable), 0ULL);

    /* Class free list stats */
    atomic_init(&(H5P_mt_g.class_fl_head_update), 0ULL);
    atomic_init(&(H5P_mt_g.class_fl_head_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.class_fl_tail_update), 0ULL);
    atomic_init(&(H5P_mt_g.class_fl_tail_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.class_fl_next_update), 0ULL);
    atomic_init(&(H5P_mt_g.class_fl_next_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.num_class_added_to_fl), 0ULL);
    atomic_init(&(H5P_mt_g.class_fl_head_freed_due_to_max_len), 0ULL);
    atomic_init(&(H5P_mt_g.class_fl_head_free_skipped_due_to_empty), 0ULL);
    atomic_init(&(H5P_mt_g.class_fl_head_free_skipped_no_reallocable), 0ULL);

    /* List free list stats */
    atomic_init(&(H5P_mt_g.list_fl_head_update), 0ULL);
    atomic_init(&(H5P_mt_g.list_fl_head_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.list_fl_tail_update), 0ULL);
    atomic_init(&(H5P_mt_g.list_fl_tail_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.list_fl_next_update), 0ULL);
    atomic_init(&(H5P_mt_g.list_fl_next_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.num_list_added_to_fl), 0ULL);
    atomic_init(&(H5P_mt_g.list_fl_head_freed_due_to_max_len), 0ULL);
    atomic_init(&(H5P_mt_g.list_fl_head_free_skipped_due_to_empty), 0ULL);
    atomic_init(&(H5P_mt_g.list_fl_head_free_skipped_no_reallocable), 0ULL);

    /* stats for creating or copying classes */
    atomic_init(&(H5P_mt_g.H5P__create_class__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__copy_pclass__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__mt_create_class__internal__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_classes_created_wo_parent), 0ULL);
    atomic_init(&(H5P_mt_g.num_class_structs_allocated_from_heap), 0ULL);
    atomic_init(&(H5P_mt_g.num_class_structs_allocated_from_fl), 0ULL);

    /* stats for creating or copying lists */
    atomic_init(&(H5P_mt_g.H5P_create_id__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__create_list__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.H5P_copy_plist__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__mt_create_list__internal__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_list_structs_allocated_from_heap), 0ULL);
    atomic_init(&(H5P_mt_g.num_list_structs_allocated_from_fl), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__init_lkup_tbl__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__init_lkup_tbl_copy__num_calls), 0ULL);

    /* stats for creating or copying properties */
    atomic_init(&(H5P_mt_g.H5P__create_prop__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__copy_prop_plist__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__copy_prop_pclass__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_prop_structs_allocated_from_heap), 0ULL);
    atomic_init(&(H5P_mt_g.num_prop_structs_allocated_from_fl), 0ULL);
    atomic_init(&(H5P_mt_g.num_props_created_wo_cbs), 0ULL);

    /* stats for property inserts */
    atomic_init(&(H5P_mt_g.num_props_inserted_classes), 0ULL);
    atomic_init(&(H5P_mt_g.num_props_inserted_lists), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__mt_ins_or_mod_prop__lfsll_ins__num_calls), 0ULL);

    /* stats for number of deletes */
    atomic_init(&(H5P_mt_g.num_props_deleted_classes), 0ULL);
    atomic_init(&(H5P_mt_g.num_props_deleted_classes_prop_not_found), 0ULL);
    atomic_init(&(H5P_mt_g.num_props_deleted_classes_already_deleted), 0ULL);
    atomic_init(&(H5P_mt_g.num_props_deleted_lists), 0ULL);
    atomic_init(&(H5P_mt_g.num_props_deleted_lists_prop_not_found), 0ULL);
    atomic_init(&(H5P_mt_g.num_props_deleted_lists_already_deleted), 0ULL);

    /* stats for searches */
    atomic_init(&(H5P_mt_g.num_searches_classes), 0ULL);
    atomic_init(&(H5P_mt_g.num_searches_classes_prop_not_found), 0ULL);
    atomic_init(&(H5P_mt_g.num_searches_while_an_op_occurs_class), 0ULL);
    atomic_init(&(H5P_mt_g.num_searches_lists), 0ULL);
    atomic_init(&(H5P_mt_g.num_searches_lists_prop_not_found), 0ULL);
    atomic_init(&(H5P_mt_g.num_searches_while_an_op_occurs_list), 0ULL);

    /* Default list stats */
    atomic_init(&(H5P_mt_g.num_default_list_mods), 0ULL);

    /* stats for getting list version from context */
    atomic_init(&(H5P_mt_g.num_list_version_from_ctx), 0ULL);

    /* Property chksum cols stats */
    atomic_init(&(H5P_mt_g.num_chksum_cols), 0ULL);

    /* H5P__mt_enforce_serialization stats */
    atomic_init(&(H5P_mt_g.H5P__mt_enforce_serialization__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_classes_loop_enforce_serial), 0ULL);
    atomic_init(&(H5P_mt_g.num_lists_loop_enforce_serial), 0ULL);

    /* stats for marking classes deleted or unmarking classes as deleted */
    atomic_init(&(H5P_mt_g.H5P__close_class_cb__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__close_class__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.close_class_but_pl_not_zero), 0ULL);
    atomic_init(&(H5P_mt_g.close_class_but_plc_not_zero), 0ULL);
    atomic_init(&(H5P_mt_g.class_un_marked_as_deleted), 0ULL);

    /* stats for closing lists */
    atomic_init(&(H5P_mt_g.H5P__close_list_cb__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.H5P_close__num_calls), 0ULL);

    /* Clear function stats */
    atomic_init(&(H5P_mt_g.num_classes_freed), 0ULL);
    atomic_init(&(H5P_mt_g.num_lists_freed), 0ULL);
    atomic_init(&(H5P_mt_g.num_props_freed), 0ULL);

    /* H5P_mt_class_t and H5P_mt_list_t comparison stats */
    atomic_init(&(H5P_mt_g.max_derived_classes), 0ULL);
    atomic_init(&(H5P_mt_g.max_derived_lists), 0ULL);
    atomic_init(&(H5P_mt_g.max_class_num_phys_props), 2ULL);
    atomic_init(&(H5P_mt_g.max_list_num_phys_props), 2ULL);
    atomic_init(&(H5P_mt_g.max_class_version_number), 0ULL);
    atomic_init(&(H5P_mt_g.max_list_version_number), 0ULL);

    /* Global Mutex stats */
    atomic_init(&(H5P_mt_g.H5P__grab_global_mutex__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.global_mutex_acquire_failures), 0ULL);
    atomic_init(&(H5P_mt_g.global_mutex_acquire_success), 0ULL);
    atomic_init(&(H5P_mt_g.global_mutex_unlocks), 0ULL);

    /* Property callback stats */
    atomic_init(&(H5P_mt_g.H5P__global_lock_prop_cb__create__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_already_have_mutex__create_cb), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__global_lock_prop_cb__set__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_already_have_mutex__set_cb), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__global_lock_prop_cb__get__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_already_have_mutex__get_cb), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__global_lock_prop_cb__encode__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_already_have_mutex__encode_cb), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__global_lock_prop_cb__decode__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_already_have_mutex__decode_cb), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__global_lock_prop_cb__del__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_already_have_mutex__del_cb), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__global_lock_prop_cb__copy__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_already_have_mutex__copy_cb), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__global_lock_prop_cb__cmp__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_already_have_mutex__cmp_cb), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__global_lock_prop_cb__close__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_already_have_mutex__close_cb), 0ULL);

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__init_stats_global */

/****************************************************************************************
 * Function:    H5P__reset_stats_global
 *
 * Purpose:     Resets the stats fields for the H5P_mt_g global struct
 *
 *              NOTE: These statistics are only  maintained in the multi-thread
 *              implementation of H5P.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__reset_stats_global(void)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    /* Property free list stats */
    atomic_store(&(H5P_mt_g.prop_fl_head_update), 0ULL);
    atomic_store(&(H5P_mt_g.prop_fl_head_update_cols), 0ULL);
    atomic_store(&(H5P_mt_g.prop_fl_tail_update), 0ULL);
    atomic_store(&(H5P_mt_g.prop_fl_tail_update_cols), 0ULL);
    atomic_store(&(H5P_mt_g.prop_fl_next_update), 0ULL);
    atomic_store(&(H5P_mt_g.prop_fl_next_update_cols), 0ULL);
    atomic_store(&(H5P_mt_g.num_props_added_to_fl), 0ULL);
    atomic_store(&(H5P_mt_g.prop_fl_head_freed_due_to_max_len), 0ULL);
    atomic_store(&(H5P_mt_g.prop_fl_head_free_skipped_due_to_empty), 0ULL);
    atomic_store(&(H5P_mt_g.prop_fl_head_free_skipped_no_reallocable), 0ULL);

    /* Class free list stats */
    atomic_store(&(H5P_mt_g.class_fl_head_update), 0ULL);
    atomic_store(&(H5P_mt_g.class_fl_head_update_cols), 0ULL);
    atomic_store(&(H5P_mt_g.class_fl_tail_update), 0ULL);
    atomic_store(&(H5P_mt_g.class_fl_tail_update_cols), 0ULL);
    atomic_store(&(H5P_mt_g.class_fl_next_update), 0ULL);
    atomic_store(&(H5P_mt_g.class_fl_next_update_cols), 0ULL);
    atomic_store(&(H5P_mt_g.num_class_added_to_fl), 0ULL);
    atomic_store(&(H5P_mt_g.class_fl_head_freed_due_to_max_len), 0ULL);
    atomic_store(&(H5P_mt_g.class_fl_head_free_skipped_due_to_empty), 0ULL);
    atomic_store(&(H5P_mt_g.class_fl_head_free_skipped_no_reallocable), 0ULL);

    /* List free list stats */
    atomic_store(&(H5P_mt_g.list_fl_head_update), 0ULL);
    atomic_store(&(H5P_mt_g.list_fl_head_update_cols), 0ULL);
    atomic_store(&(H5P_mt_g.list_fl_tail_update), 0ULL);
    atomic_store(&(H5P_mt_g.list_fl_tail_update_cols), 0ULL);
    atomic_store(&(H5P_mt_g.list_fl_next_update), 0ULL);
    atomic_store(&(H5P_mt_g.list_fl_next_update_cols), 0ULL);
    atomic_store(&(H5P_mt_g.num_list_added_to_fl), 0ULL);
    atomic_store(&(H5P_mt_g.list_fl_head_freed_due_to_max_len), 0ULL);
    atomic_store(&(H5P_mt_g.list_fl_head_free_skipped_due_to_empty), 0ULL);
    atomic_store(&(H5P_mt_g.list_fl_head_free_skipped_no_reallocable), 0ULL);

    /* stats for creating or copying classes */
    atomic_store(&(H5P_mt_g.H5P__create_class__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__copy_pclass__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__mt_create_class__internal__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_classes_created_wo_parent), 0ULL);
    atomic_store(&(H5P_mt_g.num_class_structs_allocated_from_heap), 0ULL);
    atomic_store(&(H5P_mt_g.num_class_structs_allocated_from_fl), 0ULL);

    /* stats for creating or copying lists */
    atomic_store(&(H5P_mt_g.H5P_create_id__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__create_list__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.H5P_copy_plist__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__mt_create_list__internal__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_list_structs_allocated_from_heap), 0ULL);
    atomic_store(&(H5P_mt_g.num_list_structs_allocated_from_fl), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__init_lkup_tbl__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__init_lkup_tbl_copy__num_calls), 0ULL);

    /* stats for creating or copying props */
    atomic_store(&(H5P_mt_g.H5P__create_prop__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__copy_prop_plist__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__copy_prop_pclass__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_prop_structs_allocated_from_heap), 0ULL);
    atomic_store(&(H5P_mt_g.num_prop_structs_allocated_from_fl), 0ULL);
    atomic_store(&(H5P_mt_g.num_props_created_wo_cbs), 0ULL);

    /* stats for property inserts */
    atomic_store(&(H5P_mt_g.num_props_inserted_classes), 0ULL);
    atomic_store(&(H5P_mt_g.num_props_inserted_lists), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__mt_ins_or_mod_prop__lfsll_ins__num_calls), 0ULL);

    /* stats for number of deletes */
    atomic_store(&(H5P_mt_g.num_props_deleted_classes), 0ULL);
    atomic_store(&(H5P_mt_g.num_props_deleted_classes_prop_not_found), 0ULL);
    atomic_store(&(H5P_mt_g.num_props_deleted_classes_already_deleted), 0ULL);
    atomic_store(&(H5P_mt_g.num_props_deleted_lists), 0ULL);
    atomic_store(&(H5P_mt_g.num_props_deleted_lists_prop_not_found), 0ULL);
    atomic_store(&(H5P_mt_g.num_props_deleted_lists_already_deleted), 0ULL);

    /* stats for searches */
    atomic_store(&(H5P_mt_g.num_searches_classes), 0ULL);
    atomic_store(&(H5P_mt_g.num_searches_classes_prop_not_found), 0ULL);
    atomic_store(&(H5P_mt_g.num_searches_while_an_op_occurs_class), 0ULL);
    atomic_store(&(H5P_mt_g.num_searches_lists), 0ULL);
    atomic_store(&(H5P_mt_g.num_searches_lists_prop_not_found), 0ULL);
    atomic_store(&(H5P_mt_g.num_searches_while_an_op_occurs_list), 0ULL);

    /* Default list stats */
    atomic_store(&(H5P_mt_g.num_default_list_mods), 0ULL);

    /* stats for getting list version from context */
    atomic_store(&(H5P_mt_g.num_list_version_from_ctx), 0ULL);

    /* Property chksum cols stats */
    atomic_store(&(H5P_mt_g.num_chksum_cols), 0ULL);

    /* H5P__mt_enforce_serialization stats */
    atomic_store(&(H5P_mt_g.H5P__mt_enforce_serialization__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_classes_loop_enforce_serial), 0ULL);
    atomic_store(&(H5P_mt_g.num_lists_loop_enforce_serial), 0ULL);

    /* stats for marking classes deleted or unmarking classes as deleted */
    atomic_store(&(H5P_mt_g.H5P__close_class_cb__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__close_class__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.close_class_but_pl_not_zero), 0ULL);
    atomic_store(&(H5P_mt_g.close_class_but_plc_not_zero), 0ULL);
    atomic_store(&(H5P_mt_g.class_un_marked_as_deleted), 0ULL);

    /* stats for closing lists */
    atomic_store(&(H5P_mt_g.H5P__close_list_cb__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.H5P_close__num_calls), 0ULL);

    /* Clear function stats */
    atomic_store(&(H5P_mt_g.num_classes_freed), 0ULL);
    atomic_store(&(H5P_mt_g.num_lists_freed), 0ULL);
    atomic_store(&(H5P_mt_g.num_props_freed), 0ULL);

    /* H5P_mt_class_t and H5P_mt_list_t comparison stats */
    atomic_store(&(H5P_mt_g.max_derived_classes), 0ULL);
    atomic_store(&(H5P_mt_g.max_derived_lists), 0ULL);
    atomic_store(&(H5P_mt_g.max_class_num_phys_props), 2ULL);
    atomic_store(&(H5P_mt_g.max_list_num_phys_props), 2ULL);
    atomic_store(&(H5P_mt_g.max_class_version_number), 0ULL);
    atomic_store(&(H5P_mt_g.max_list_version_number), 0ULL);

    /* Global Mutex stats */
    atomic_store(&(H5P_mt_g.H5P__grab_global_mutex__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.global_mutex_acquire_failures), 0ULL);
    atomic_store(&(H5P_mt_g.global_mutex_acquire_success), 0ULL);
    atomic_store(&(H5P_mt_g.global_mutex_unlocks), 0ULL);

    /* Property callback stats */
    atomic_store(&(H5P_mt_g.H5P__global_lock_prop_cb__create__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_already_have_mutex__create_cb), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__global_lock_prop_cb__set__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_already_have_mutex__set_cb), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__global_lock_prop_cb__get__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_already_have_mutex__get_cb), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__global_lock_prop_cb__encode__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_already_have_mutex__encode_cb), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__global_lock_prop_cb__decode__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_already_have_mutex__decode_cb), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__global_lock_prop_cb__del__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_already_have_mutex__del_cb), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__global_lock_prop_cb__copy__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_already_have_mutex__copy_cb), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__global_lock_prop_cb__cmp__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_already_have_mutex__cmp_cb), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__global_lock_prop_cb__close__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_already_have_mutex__close_cb), 0ULL);

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__reset_stats_global()*/

/****************************************************************************************
 * Function:    H5P__init_stats_class
 *
 * Purpose:     Initializes the stats fields for a H5P_mt_class_t
 *
 *              NOTE: These statistics are only  maintained in teh multi-thread
 *              implementation of H5P.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__init_stats_class(H5P_mt_class_t *class)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    /* Initialize H5P_mt_class_t stats fields */

    /* H5P_mt_class_t insert stats */
    atomic_init(&(class->H5P__register_real__num_calls), 0ULL);
    atomic_init(&(class->H5P__register__num_calls), 0ULL);
    atomic_init(&(class->H5P__mt_ins_or_mod_prop__class__num_calls), 0ULL);
    atomic_init(&(class->H5P__class_set__num_calls), 0ULL);
    atomic_init(&(class->insert_max_nodes_visited), 0ULL);
    atomic_init(&(class->insert_avg_nodes_visited), 0ULL);
    atomic_init(&(class->num_insert_nodes_visited), 0ULL);
    atomic_init(&(class->num_insert_prop__cols), 0ULL);
    atomic_init(&(class->num_insert_prop__success), 0ULL);
    atomic_init(&(class->num_insert_prop__chksum_cols), 0ULL);

    /* H5P_mt_class_t set delete version stats */
    atomic_init(&(class->H5P__unregister__num_calls), 0ULL);
    atomic_init(&(class->delete_prop__max_nodes_visited), 0ULL);
    atomic_init(&(class->delete_prop__avg_nodes_visited), 0ULL);
    atomic_init(&(class->num_delete_prop__nodes_visited), 0ULL);
    atomic_init(&(class->num_delete_prop__cols), 0ULL);
    atomic_init(&(class->num_delete_prop__success), 0ULL);
    atomic_init(&(class->num_delete_prop__chksum_cols), 0ULL);

    /* H5P_mt_class_t search stats */
    atomic_init(&(class->H5P__mt_search_prop__class__num_calls), 0ULL);
    atomic_init(&(class->search_class__max_nodes_visited), 0ULL);
    atomic_init(&(class->search_class__avg_nodes_visited), 0ULL);
    atomic_init(&(class->num_search_class__nodes_visited), 0ULL);
    atomic_init(&(class->num_search_class__success), 0ULL);
    atomic_init(&(class->num_search_chksum_cols), 0ULL);

    /* Version check stats */
    atomic_init(&(class->num_wait_for_curr_version_to_inc), 0ULL);

    /* H5P_mt_active_thread_count_t stats */
    atomic_init(&(class->num_thrd_update_cols), 0ULL);
    atomic_init(&(class->num_thrd_count_update), 0ULL);
    atomic_init(&(class->num_thrd_closing_flag_set), 0ULL);
    atomic_init(&(class->num_thrd_opening_flag_set), 0ULL);

    /* H5P_mt_class_ref_counts_t stats */
    atomic_init(&(class->num_ref_count_cols), 0ULL);
    atomic_init(&(class->num_ref_count_update), 0ULL);
    atomic_init(&(class->num_ref_count_inc_while_deleted), 0ULL);
    atomic_init(&(class->num_ref_count_marked_deleted), 0ULL);
    atomic_init(&(class->num_ref_count_unmarked_deleted), 0ULL);

    /* Property ref_count stats */
    atomic_init(&(class->num_prop_ref_count_update), 0ULL);

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__init_stats_class() */

/****************************************************************************************
 * Function:    H5P__reset_stats_class
 *
 * Purpose:     Resets the stats fields for a H5P_mt_class_t
 *
 *              Currently this function is not being used, but is for future iterations
 *              when/if the class free list is used to reset the stats of a class being
 *              reallocated from the free list.
 *
 *              NOTE: These statistics are only  maintained in teh multi-thread
 *              implementation of H5P.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__reset_stats_class(H5P_mt_class_t *class)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    /* Initialize H5P_mt_class_t stats fields */

    /* H5P_mt_class_t insert stats */
    atomic_store(&(class->H5P__register_real__num_calls), 0ULL);
    atomic_store(&(class->H5P__register__num_calls), 0ULL);
    atomic_store(&(class->H5P__mt_ins_or_mod_prop__class__num_calls), 0ULL);
    atomic_store(&(class->H5P__class_set__num_calls), 0ULL);
    atomic_store(&(class->insert_max_nodes_visited), 0ULL);
    atomic_store(&(class->insert_avg_nodes_visited), 0ULL);
    atomic_store(&(class->num_insert_nodes_visited), 0ULL);
    atomic_store(&(class->num_insert_prop__cols), 0ULL);
    atomic_store(&(class->num_insert_prop__success), 0ULL);
    atomic_store(&(class->num_insert_prop__chksum_cols), 0ULL);

    /* H5P_mt_class_t set delete version stats */
    atomic_store(&(class->H5P__unregister__num_calls), 0ULL);
    atomic_store(&(class->delete_prop__max_nodes_visited), 0ULL);
    atomic_store(&(class->delete_prop__avg_nodes_visited), 0ULL);
    atomic_store(&(class->num_delete_prop__nodes_visited), 0ULL);
    atomic_store(&(class->num_delete_prop__cols), 0ULL);
    atomic_store(&(class->num_delete_prop__success), 0ULL);
    atomic_store(&(class->num_delete_prop__chksum_cols), 0ULL);

    /* H5P_mt_class_t search stats */
    atomic_store(&(class->H5P__mt_search_prop__class__num_calls), 0ULL);
    atomic_store(&(class->search_class__max_nodes_visited), 0ULL);
    atomic_store(&(class->search_class__avg_nodes_visited), 0ULL);
    atomic_store(&(class->num_search_class__nodes_visited), 0ULL);
    atomic_store(&(class->num_search_class__success), 0ULL);
    atomic_store(&(class->num_search_chksum_cols), 0ULL);

    /* Version check stats */
    atomic_store(&(class->num_wait_for_curr_version_to_inc), 0ULL);

    /* H5P_mt_active_thread_count_t stats */
    atomic_store(&(class->num_thrd_update_cols), 0ULL);
    atomic_store(&(class->num_thrd_count_update), 0ULL);
    atomic_store(&(class->num_thrd_closing_flag_set), 0ULL);
    atomic_store(&(class->num_thrd_opening_flag_set), 0ULL);

    /* H5P_mt_class_ref_counts_t stats */
    atomic_store(&(class->num_ref_count_cols), 0ULL);
    atomic_store(&(class->num_ref_count_update), 0ULL);
    atomic_store(&(class->num_ref_count_inc_while_deleted), 0ULL);
    atomic_store(&(class->num_ref_count_marked_deleted), 0ULL);
    atomic_store(&(class->num_ref_count_unmarked_deleted), 0ULL);

    /* Property ref_count stats */
    atomic_store(&(class->num_prop_ref_count_update), 0ULL);

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__reset_stats_class() */

/****************************************************************************************
 * Function:    H5P__init_stats_list
 *
 * Purpose:     Initializes the stats fields for a H5P_mt_list_t
 *
 *              NOTE: These statistics are only  maintained in teh multi-thread
 *              implementation of H5P.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__init_stats_list(H5P_mt_list_t *list)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    /* Initialize H5P_mt_list_t stats fields */

    /* H5P_mt_list_t insert stats */
    atomic_init(&(list->H5P_insert__num_calls), 0ULL);
    atomic_init(&(list->H5P__mt_ins_or_mod_prop__class__num_calls), 0ULL);
    atomic_init(&(list->H5P_poke__num_calls), 0ULL);
    atomic_init(&(list->H5P_set__num_calls), 0ULL);
    atomic_init(&(list->insert_max_nodes_visited), 0ULL);
    atomic_init(&(list->insert_avg_nodes_visited), 0ULL);
    atomic_init(&(list->num_insert_nodes_visited), 0ULL);
    atomic_init(&(list->num_insert_prop_cols), 0ULL);
    atomic_init(&(list->num_insert_prop_success), 0ULL);
    atomic_init(&(list->num_insert_update_entry), 0ULL);
    atomic_init(&(list->num_insert_update_entry_cols), 0ULL);
    atomic_init(&(list->num_insert_prop__chksum_cols), 0ULL);
    atomic_init(&(list->num_set_new_value_cols), 0ULL);
    atomic_init(&(list->num_set_new_value), 0ULL);

    /* H5P_mt_list_t set delete version stats */
    atomic_init(&(list->H5P_remove__num_calls), 0ULL);
    atomic_init(&(list->H5P__mt_delete_prop__list__num_calls), 0ULL);
    atomic_init(&(list->num_delete_props_from_lfsll), 0ULL);
    atomic_init(&(list->delete_prop__max_nodes_visited), 0ULL);
    atomic_init(&(list->delete_prop__avg_nodes_visited), 0ULL);
    atomic_init(&(list->num_delete_prop__nodes_visited), 0ULL);
    atomic_init(&(list->num_delete_prop__cols), 0ULL);
    atomic_init(&(list->num_delete_prop__success), 0ULL);
    atomic_init(&(list->num_delete_prop_chksum_cols), 0ULL);
    atomic_init(&(list->num_delete_prop__base_delete_version), 0ULL);
    atomic_init(&(list->num_delete_prop__curr_entry), 0ULL);
    atomic_init(&(list->num_delete_prop__older_curr), 0ULL);

    /* H5P_mt_list_t search stats */
    atomic_init(&(list->H5P__find_prop_plist__num_calls), 0ULL);
    atomic_init(&(list->H5P__mt_search_prop__list__num_calls), 0ULL);
    atomic_init(&(list->search_list__max_nodes_visited), 0ULL);
    atomic_init(&(list->search_list__avg_nodes_visited), 0ULL);
    atomic_init(&(list->num_search_list__nodes_visited), 0ULL);
    atomic_init(&(list->num_search_list__success), 0ULL);
    atomic_init(&(list->num_search_chksum_cols), 0ULL);
    atomic_init(&(list->num_search_list__found_base), 0ULL);
    atomic_init(&(list->num_search_list__found_curr), 0ULL);
    atomic_init(&(list->num_target_prop_found_but_deleted), 0ULL);

    /* Version check stats */
    atomic_init(&(list->num_wait_for_curr_version_to_inc), 0ULL);

    /* H5P_mt_active_thread_count_t stats */
    atomic_init(&(list->num_thrd_update_cols), 0ULL);
    atomic_init(&(list->num_thrd_count_update), 0ULL);
    atomic_init(&(list->num_thrd_closing_flag_set), 0ULL);
    atomic_init(&(list->num_thrd_opening_flag_set), 0ULL);

    /* initializing lkup_tbl stats */
    atomic_init(&(list->num_inherited_with_create_cb), 0ULL);
    atomic_init(&(list->num_lkup_tbl_copy_entries_blank), 0ULL);

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__init_stats_list() */

/****************************************************************************************
 * Function:    H5P__reset_stats_list
 *
 * Purpose:     Resets the stats fields for a H5P_mt_list_t
 *
 *              Currently this function is not being used, but is for future iterations
 *              when/if the list free list is used to reset the stats of a list being
 *              reallocated from the free list.
 *
 *              NOTE: These statistics are only  maintained in teh multi-thread
 *              implementation of H5P.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__reset_stats_list(H5P_mt_list_t *list)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    /* Initialize H5P_mt_list_t stats fields */

    /* H5P_mt_list_t insert stats */
    atomic_store(&(list->H5P_insert__num_calls), 0ULL);
    atomic_store(&(list->H5P__mt_ins_or_mod_prop__class__num_calls), 0ULL);
    atomic_store(&(list->H5P_poke__num_calls), 0ULL);
    atomic_store(&(list->H5P_set__num_calls), 0ULL);
    atomic_store(&(list->insert_max_nodes_visited), 0ULL);
    atomic_store(&(list->insert_avg_nodes_visited), 0ULL);
    atomic_store(&(list->num_insert_nodes_visited), 0ULL);
    atomic_store(&(list->num_insert_prop_cols), 0ULL);
    atomic_store(&(list->num_insert_prop_success), 0ULL);
    atomic_store(&(list->num_insert_update_entry), 0ULL);
    atomic_store(&(list->num_insert_update_entry_cols), 0ULL);
    atomic_store(&(list->num_insert_prop__chksum_cols), 0ULL);
    atomic_store(&(list->num_set_new_value_cols), 0ULL);
    atomic_store(&(list->num_set_new_value), 0ULL);

    /* H5P_mt_list_t set delete version stats */
    atomic_store(&(list->H5P_remove__num_calls), 0ULL);
    atomic_store(&(list->H5P__mt_delete_prop__list__num_calls), 0ULL);
    atomic_store(&(list->num_delete_props_from_lfsll), 0ULL);
    atomic_store(&(list->delete_prop__max_nodes_visited), 0ULL);
    atomic_store(&(list->delete_prop__avg_nodes_visited), 0ULL);
    atomic_store(&(list->num_delete_prop__nodes_visited), 0ULL);
    atomic_store(&(list->num_delete_prop__cols), 0ULL);
    atomic_store(&(list->num_delete_prop__success), 0ULL);
    atomic_store(&(list->num_delete_prop_chksum_cols), 0ULL);
    atomic_store(&(list->num_delete_prop__base_delete_version), 0ULL);
    atomic_store(&(list->num_delete_prop__curr_entry), 0ULL);
    atomic_store(&(list->num_delete_prop__older_curr), 0ULL);

    /* H5P_mt_list_t search stats */
    atomic_store(&(list->H5P__find_prop_plist__num_calls), 0ULL);
    atomic_store(&(list->H5P__mt_search_prop__list__num_calls), 0ULL);
    atomic_store(&(list->search_list__max_nodes_visited), 0ULL);
    atomic_store(&(list->search_list__avg_nodes_visited), 0ULL);
    atomic_store(&(list->num_search_list__nodes_visited), 0ULL);
    atomic_store(&(list->num_search_list__success), 0ULL);
    atomic_store(&(list->num_search_chksum_cols), 0ULL);
    atomic_store(&(list->num_search_list__found_base), 0ULL);
    atomic_store(&(list->num_search_list__found_curr), 0ULL);
    atomic_store(&(list->num_target_prop_found_but_deleted), 0ULL);

    /* Version check stats */
    atomic_store(&(list->num_wait_for_curr_version_to_inc), 0ULL);

    /* H5P_mt_active_thread_count_t stats */
    atomic_store(&(list->num_thrd_update_cols), 0ULL);
    atomic_store(&(list->num_thrd_count_update), 0ULL);
    atomic_store(&(list->num_thrd_closing_flag_set), 0ULL);
    atomic_store(&(list->num_thrd_opening_flag_set), 0ULL);

    /* initializing lkup_tbl stats */
    atomic_store(&(list->num_inherited_with_create_cb), 0ULL);
    atomic_store(&(list->num_lkup_tbl_copy_entries_blank), 0ULL);

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__init_stats_list() */

/****************************************************************************************
 * Function:    H5P__dump_stats_global
 *
 * Purpose:     Dump the stats maintained in a H5P_mt_g global structure to the specified
 *              file.
 *
 *              NOTE: These statistics are only  maintained in teh multi-thread
 *              implementation of H5P.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__dump_stats_global(FILE *file_ptr)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    fprintf(file_ptr, "\n\nH5P Multi-Thread STATS for the global structure\n\n");

    /* Property free list stats */
    fprintf(file_ptr, "H5P_mt_g.prop_fl_head_update                         = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_head_update))));
    fprintf(file_ptr, "H5P_mt_g.prop_fl_head_update_cols                    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_head_update_cols))));
    fprintf(file_ptr, "H5P_mt_g.prop_fl_tail_update                         = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_tail_update))));
    fprintf(file_ptr, "H5P_mt_g.prop_fl_tail_update_cols                    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_tail_update_cols))));
    fprintf(file_ptr, "H5P_mt_g.prop_fl_next_update                         = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_next_update))));
    fprintf(file_ptr, "H5P_mt_g.prop_fl_next_update_cols                    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_next_update_cols))));
    fprintf(file_ptr, "H5P_mt_g.num_props_added_to_fl                       = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_added_to_fl))));
    fprintf(file_ptr, "H5P_mt_g.prop_fl_head_freed_due_to_max_len           = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_head_freed_due_to_max_len))));
    fprintf(file_ptr, "H5P_mt_g.prop_fl_head_free_skipped_due_to_empty      = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_head_free_skipped_due_to_empty))));
    fprintf(file_ptr, "H5P_mt_g.prop_fl_head_free_skipped_no_reallocable    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_head_free_skipped_no_reallocable))));

    /* Class free list stats */
    fprintf(file_ptr, "H5P_mt_g.class_fl_head_update                        = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.class_fl_head_update))));
    fprintf(file_ptr, "H5P_mt_g.class_fl_head_update_cols                   = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.class_fl_head_update_cols))));
    fprintf(file_ptr, "H5P_mt_g.class_fl_tail_update                        = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.class_fl_tail_update))));
    fprintf(file_ptr, "H5P_mt_g.class_fl_tail_update_cols                   = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.class_fl_tail_update_cols))));
    fprintf(file_ptr, "H5P_mt_g.class_fl_next_update                        = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.class_fl_next_update))));
    fprintf(file_ptr, "H5P_mt_g.class_fl_next_update_cols                   = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.class_fl_next_update_cols))));
    fprintf(file_ptr, "H5P_mt_g.num_class_added_to_fl                       = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_class_added_to_fl))));
    fprintf(file_ptr, "H5P_mt_g.class_fl_head_freed_due_to_max_len          = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.class_fl_head_freed_due_to_max_len))));
    fprintf(file_ptr, "H5P_mt_g.class_fl_head_free_skipped_due_to_empty     = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.class_fl_head_free_skipped_due_to_empty))));
    fprintf(file_ptr, "H5P_mt_g.class_fl_head_free_skipped_no_reallocable   = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.class_fl_head_free_skipped_no_reallocable))));

    /* List free list stats */
    fprintf(file_ptr, "H5P_mt_g.list_fl_head_update                         = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.list_fl_head_update))));
    fprintf(file_ptr, "H5P_mt_g.list_fl_head_update_cols                    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.list_fl_head_update_cols))));
    fprintf(file_ptr, "H5P_mt_g.list_fl_tail_update                         = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.list_fl_tail_update))));
    fprintf(file_ptr, "H5P_mt_g.list_fl_tail_update_cols                    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.list_fl_tail_update_cols))));
    fprintf(file_ptr, "H5P_mt_g.list_fl_next_update                         = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.list_fl_next_update))));
    fprintf(file_ptr, "H5P_mt_g.list_fl_next_update_cols                    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.list_fl_next_update_cols))));
    fprintf(file_ptr, "H5P_mt_g.num_list_added_to_fl                        = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_list_added_to_fl))));
    fprintf(file_ptr, "H5P_mt_g.list_fl_head_freed_due_to_max_len           = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.list_fl_head_freed_due_to_max_len))));
    fprintf(file_ptr, "H5P_mt_g.list_fl_head_free_skipped_due_to_empty      = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.list_fl_head_free_skipped_due_to_empty))));
    fprintf(file_ptr, "H5P_mt_g.list_fl_head_free_skipped_no_reallocable    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.list_fl_head_free_skipped_no_reallocable))));

    /* stats for creating or copying classes */
    fprintf(file_ptr, "H5P_mt_g.H5P__create_class__num_calls                = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__create_class__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.H5P__copy_pclass__num_calls                 = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__copy_pclass__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.H5P__mt_create_class__internal__num_calls   = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__mt_create_class__internal__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_classes_created_wo_parent               = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_classes_created_wo_parent))));
    fprintf(file_ptr, "H5P_mt_g.num_class_structs_allocated_from_heap       = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_class_structs_allocated_from_heap))));
    fprintf(file_ptr, "H5P_mt_g.num_class_structs_allocated_from_fl         = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_class_structs_allocated_from_fl))));

    /* stats for creating or copying lists */
    fprintf(file_ptr, "H5P_mt_g.H5P_create_id__num_calls                    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P_create_id__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.H5P__create_list__num_calls                 = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__create_list__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.H5P_copy_plist__num_calls                   = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P_copy_plist__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.H5P__mt_create_list__internal__num_calls    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__mt_create_list__internal__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_list_structs_allocated_from_heap        = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_list_structs_allocated_from_heap))));
    fprintf(file_ptr, "H5P_mt_g.num_list_structs_allocated_from_fl          = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_list_structs_allocated_from_fl))));
    fprintf(file_ptr, "H5P_mt_g.H5P__init_lkup_tbl__num_calls               = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__init_lkup_tbl__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.H5P__init_lkup_tbl_copy__num_calls          = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__init_lkup_tbl_copy__num_calls))));

    /* stats for creating or copying props */
    fprintf(file_ptr, "H5P_mt_g.H5P__create_prop__num_calls                 = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__create_prop__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.H5P__copy_prop_plist__num_calls             = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__copy_prop_plist__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.H5P__copy_prop_pclass__num_calls            = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__copy_prop_pclass__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_prop_structs_allocated_from_heap        = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_prop_structs_allocated_from_heap))));
    fprintf(file_ptr, "H5P_mt_g.num_prop_structs_allocated_from_fl          = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_prop_structs_allocated_from_fl))));
    fprintf(file_ptr, "H5P_mt_g.num_props_created_wo_cbs                    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_created_wo_cbs))));

    /* stats for property inserts */
    fprintf(file_ptr, "H5P_mt_g.num_props_inserted_classes                  = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_inserted_classes))));
    fprintf(file_ptr, "H5P_mt_g.num_props_inserted_lists                    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_inserted_lists))));
    fprintf(file_ptr, "H5P_mt_g.H5P__init_lkup_tbl__num_calls               = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__mt_ins_or_mod_prop__lfsll_ins__num_calls))));

    /* stats for number of deletes */
    fprintf(file_ptr, "H5P_mt_g.num_props_deleted_classes                   = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_deleted_classes))));
    fprintf(file_ptr, "H5P_mt_g.num_props_deleted_classes_prop_not_found    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_deleted_classes_prop_not_found))));
    fprintf(file_ptr, "H5P_mt_g.num_props_deleted_classes_already_deleted   = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_deleted_classes_already_deleted))));
    fprintf(file_ptr, "H5P_mt_g.num_props_deleted_lists                     = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_deleted_lists))));
    fprintf(file_ptr, "H5P_mt_g.num_props_deleted_lists_prop_not_found      = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_deleted_lists_prop_not_found))));
    fprintf(file_ptr, "H5P_mt_g.num_props_deleted_lists_already_deleted     = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_deleted_lists_already_deleted))));

    /* stats for searches */
    fprintf(file_ptr, "H5P_mt_g.num_searches_classes                        = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_searches_classes))));
    fprintf(file_ptr, "H5P_mt_g.num_searches_classes_prop_not_found         = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_searches_classes_prop_not_found))));
    fprintf(file_ptr, "H5P_mt_g.num_searches_while_an_op_occurs_class       = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_searches_while_an_op_occurs_class))));
    fprintf(file_ptr, "H5P_mt_g.num_searches_lists                          = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_searches_lists))));
    fprintf(file_ptr, "H5P_mt_g.num_searches_lists_prop_not_found           = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_searches_lists_prop_not_found))));
    fprintf(file_ptr, "H5P_mt_g.num_searches_while_an_op_occurs_list        = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_searches_while_an_op_occurs_list))));

    /* Default list stats */
    fprintf(file_ptr, "H5P_mt_g.num_default_list_mods                       = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_default_list_mods))));

    /* stats for getting list version from context */
    fprintf(file_ptr, "H5P_mt_g.num_list_version_from_ctx                   = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_list_version_from_ctx))));

    /* Property chksum cols stats */
    fprintf(file_ptr, "H5P_mt_g.num_chksum_cols                             = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_chksum_cols))));

    /* H5P__mt_enforce_serialization stats */
    fprintf(file_ptr, "H5P_mt_g.H5P__mt_enforce_serialization__num_calls    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__mt_enforce_serialization__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_classes_loop_enforce_serial             = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_classes_loop_enforce_serial))));
    fprintf(file_ptr, "H5P_mt_g.num_lists_loop_enforce_serial               = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_lists_loop_enforce_serial))));

    /* stats for marking classes deleted or unmarking classes as deleted */
    fprintf(file_ptr, "H5P_mt_g.H5P__close_class_cb__num_calls              = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__close_class_cb__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.H5P__close_class__num_calls                 = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__close_class__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.close_class_but_pl_not_zero                 = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.close_class_but_pl_not_zero))));
    fprintf(file_ptr, "H5P_mt_g.close_class_but_plc_not_zero                = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.close_class_but_plc_not_zero))));
    fprintf(file_ptr, "H5P_mt_g.class_un_marked_as_deleted                  = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.class_un_marked_as_deleted))));

    /* stats for closing lists */
    fprintf(file_ptr, "H5P_mt_g.H5P__close_list_cb__num_calls               = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__close_list_cb__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.H5P_close__num_calls                        = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P_close__num_calls))));

    /* Clear function stats */
    fprintf(file_ptr, "H5P_mt_g.num_classes_freed                           = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_classes_freed))));
    fprintf(file_ptr, "H5P_mt_g.num_lists_freed                             = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_lists_freed))));
    fprintf(file_ptr, "H5P_mt_g.num_props_freed                             = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_freed))));

    /* H5P_mt_class_t and H5P_mt_list_t comparison stats */
    fprintf(file_ptr, "H5P_mt_g.max_derived_classes                         = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.max_derived_classes))));
    fprintf(file_ptr, "H5P_mt_g.max_derived_lists                           = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.max_derived_lists))));
    fprintf(file_ptr, "H5P_mt_g.max_class_num_phys_props                    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.max_class_num_phys_props))));
    fprintf(file_ptr, "H5P_mt_g.max_list_num_phys_props                     = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.max_list_num_phys_props))));
    fprintf(file_ptr, "H5P_mt_g.max_class_version_number                    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.max_class_version_number))));
    fprintf(file_ptr, "H5P_mt_g.max_list_version_number                     = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.max_list_version_number))));

    /* Global Mutex stats */
    fprintf(file_ptr, "H5P_mt_g.H5P__grab_global_mutex__num_calls           = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__grab_global_mutex__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_already_have_global_mutex               = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_already_have_global_mutex))));
    fprintf(file_ptr, "H5P_mt_g.global_mutex_acquire_failures               = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.global_mutex_acquire_failures))));
    fprintf(file_ptr, "H5P_mt_g.global_mutex_acquire_success                = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.global_mutex_acquire_success))));
    fprintf(file_ptr, "H5P_mt_g.global_mutex_unlocks                        = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.global_mutex_unlocks))));

    /* Global Mutex stats */
    fprintf(file_ptr, "H5P_mt_g.H5P__global_lock_prop_cb__create__num_calls = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__global_lock_prop_cb__create__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_already_have_mutex__create_cb           = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_already_have_mutex__create_cb))));
    fprintf(file_ptr, "H5P_mt_g.H5P__global_lock_prop_cb__set__num_calls    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__global_lock_prop_cb__set__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_already_have_mutex__set_cb              = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_already_have_mutex__set_cb))));
    fprintf(file_ptr, "H5P_mt_g.H5P__global_lock_prop_cb__get__num_calls    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__global_lock_prop_cb__get__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_already_have_mutex__get_cb              = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_already_have_mutex__get_cb))));
    fprintf(file_ptr, "H5P_mt_g.H5P__global_lock_prop_cb__encode__num_calls = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__global_lock_prop_cb__encode__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_already_have_mutex__encode_cb           = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_already_have_mutex__encode_cb))));
    fprintf(file_ptr, "H5P_mt_g.H5P__global_lock_prop_cb__decode__num_calls = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__global_lock_prop_cb__decode__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_already_have_mutex__decode_cb           = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_already_have_mutex__decode_cb))));
    fprintf(file_ptr, "H5P_mt_g.H5P__global_lock_prop_cb__del__num_calls    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__global_lock_prop_cb__del__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_already_have_mutex__del_cb              = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_already_have_mutex__del_cb))));
    fprintf(file_ptr, "H5P_mt_g.H5P__global_lock_prop_cb__copy__num_calls   = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__global_lock_prop_cb__copy__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_already_have_mutex__copy_cb             = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_already_have_mutex__copy_cb))));
    fprintf(file_ptr, "H5P_mt_g.H5P__global_lock_prop_cb__cmp__num_calls    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__global_lock_prop_cb__cmp__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_already_have_mutex__cmp_cb              = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_already_have_mutex__cmp_cb))));
    fprintf(file_ptr, "H5P_mt_g.H5P__global_lock_prop_cb__close__num_calls  = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__global_lock_prop_cb__close__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_already_have_mutex__close_cb            = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_already_have_mutex__close_cb))));

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__dump_stats_global() */

/****************************************************************************************
 * Function:    H5P__dump_stats_class
 *
 * Purpose:     Dump the stats maintained in a H5P_mt_class_t structure to the specified
 *              file.
 *
 *              NOTE: These statistics are only  maintained in teh multi-thread
 *              implementation of H5P.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__dump_stats_class(FILE *file_ptr, H5P_mt_class_t *class)
{
    char  *name;
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    name = strdup(class->name);
    if (name == NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Failed to copy name buffer.");
    }

    fprintf(file_ptr, "\n\nH5P Multi-Thread STATS for class %s:\n\n", name);

    fprintf(file_ptr, "class->H5P__register_real__num_calls             = %lld\n",
            (unsigned long long)(atomic_load(&(class->H5P__register_real__num_calls))));
    fprintf(file_ptr, "class->H5P__register__num_calls                  = %lld\n",
            (unsigned long long)(atomic_load(&(class->H5P__register__num_calls))));
    fprintf(file_ptr, "class->H5P__mt_ins_or_mod_prop__class__num_calls = %lld\n",
            (unsigned long long)(atomic_load(&(class->H5P__mt_ins_or_mod_prop__class__num_calls))));
    fprintf(file_ptr, "class->insert_max_nodes_visited                  = %lld\n",
            (unsigned long long)(atomic_load(&(class->insert_max_nodes_visited))));
    fprintf(file_ptr, "class->insert_avg_nodes_visited                  = %lld\n",
            (unsigned long long)(atomic_load(&(class->insert_avg_nodes_visited))));
    fprintf(file_ptr, "class->num_insert_nodes_visited                  = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_insert_nodes_visited))));
    fprintf(file_ptr, "class->num_insert_prop__cols                     = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_insert_prop__cols))));
    fprintf(file_ptr, "class->num_insert_prop__success                  = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_insert_prop__success))));
    fprintf(file_ptr, "class->num_insert_prop__chksum_cols              = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_insert_prop__chksum_cols))));

    fprintf(file_ptr, "class->H5P__unregister__num_calls                = %lld\n",
            (unsigned long long)(atomic_load(&(class->H5P__unregister__num_calls))));
    fprintf(file_ptr, "class->delete_prop__max_nodes_visited            = %lld\n",
            (unsigned long long)(atomic_load(&(class->delete_prop__max_nodes_visited))));
    fprintf(file_ptr, "class->delete_prop__avg_nodes_visited            = %lld\n",
            (unsigned long long)(atomic_load(&(class->delete_prop__avg_nodes_visited))));
    fprintf(file_ptr, "class->num_delete_prop__nodes_visited            = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_delete_prop__nodes_visited))));
    fprintf(file_ptr, "class->num_delete_prop__cols                     = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_delete_prop__cols))));
    fprintf(file_ptr, "class->num_delete_prop__success                  = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_delete_prop__success))));
    fprintf(file_ptr, "class->num_delete_prop__chksum_cols              = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_delete_prop__chksum_cols))));

    fprintf(file_ptr, "class->H5P__mt_search_prop__class__num_calls     = %lld\n",
            (unsigned long long)(atomic_load(&(class->H5P__mt_search_prop__class__num_calls))));
    fprintf(file_ptr, "class->search_class__max_nodes_visited           = %lld\n",
            (unsigned long long)(atomic_load(&(class->search_class__max_nodes_visited))));
    fprintf(file_ptr, "class->search_class__avg_nodes_visited           = %lld\n",
            (unsigned long long)(atomic_load(&(class->search_class__avg_nodes_visited))));
    fprintf(file_ptr, "class->num_search_class__nodes_visited           = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_search_class__nodes_visited))));
    fprintf(file_ptr, "class->num_search_class__success                 = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_search_class__success))));
    fprintf(file_ptr, "class->num_search_chksum_cols                    = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_search_chksum_cols))));

    fprintf(file_ptr, "class->num_wait_for_curr_version_to_inc          = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_wait_for_curr_version_to_inc))));

    fprintf(file_ptr, "class->num_thrd_update_cols                      = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_thrd_update_cols))));
    fprintf(file_ptr, "class->num_thrd_count_update                     = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_thrd_count_update))));
    fprintf(file_ptr, "class->num_thrd_closing_flag_set                 = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_thrd_closing_flag_set))));
    fprintf(file_ptr, "class->num_thrd_opening_flag_set                 = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_thrd_opening_flag_set))));

    fprintf(file_ptr, "class->num_ref_count_cols                        = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_ref_count_cols))));
    fprintf(file_ptr, "class->num_ref_count_update                      = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_ref_count_update))));
    fprintf(file_ptr, "class->num_ref_count_inc_while_deleted           = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_ref_count_inc_while_deleted))));
    fprintf(file_ptr, "class->num_ref_count_marked_deleted              = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_ref_count_marked_deleted))));
    fprintf(file_ptr, "class->num_ref_count_unmarked_deleted            = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_ref_count_unmarked_deleted))));

    fprintf(file_ptr, "class->num_prop_ref_count_update                 = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_prop_ref_count_update))));

done:

    if (name) {
        free(name);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__dump_stats_class() */

/****************************************************************************************
 * Function:    H5P__dump_stats_list
 *
 * Purpose:     Dump the stats maintained in a H5P_mt_list_t structure to the specified
 *              file.
 *
 *              NOTE: These statistics are only  maintained in teh multi-thread
 *              implementation of H5P.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__dump_stats_list(FILE *file_ptr, H5P_mt_list_t *list)
{
    H5P_mt_class_t *parent;
    char           *name;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    parent = list->pclass_ptr;

    name = strdup(parent->name);
    if (name == NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Failed to copy name buffer.");
    }

    fprintf(file_ptr, "\n\nH5P Multi-Thread STATS for a list from derived class %s:\n\n", name);

    fprintf(file_ptr, "list->H5P_insert__num_calls                      = %lld\n",
            (unsigned long long)(atomic_load(&(list->H5P_insert__num_calls))));
    fprintf(file_ptr, "list->H5P__mt_ins_or_mod_prop__class__num_calls  = %lld\n",
            (unsigned long long)(atomic_load(&(list->H5P__mt_ins_or_mod_prop__class__num_calls))));
    fprintf(file_ptr, "list->H5P_poke__num_calls                        = %lld\n",
            (unsigned long long)(atomic_load(&(list->H5P_poke__num_calls))));
    fprintf(file_ptr, "list->H5P_set__num_calls                         = %lld\n",
            (unsigned long long)(atomic_load(&(list->H5P_set__num_calls))));
    fprintf(file_ptr, "list->insert_max_nodes_visited                   = %lld\n",
            (unsigned long long)(atomic_load(&(list->insert_max_nodes_visited))));
    fprintf(file_ptr, "list->insert_avg_nodes_visited                   = %lld\n",
            (unsigned long long)(atomic_load(&(list->insert_avg_nodes_visited))));
    fprintf(file_ptr, "list->num_insert_nodes_visited                   = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_insert_nodes_visited))));
    fprintf(file_ptr, "list->num_insert_prop_cols                       = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_insert_prop_cols))));
    fprintf(file_ptr, "list->num_insert_prop_success                    = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_insert_prop_success))));
    fprintf(file_ptr, "list->num_insert_update_entry_cols               = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_insert_update_entry_cols))));
    fprintf(file_ptr, "list->num_insert_update_entry                    = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_insert_update_entry))));
    fprintf(file_ptr, "list->num_insert_prop__chksum_cols               = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_insert_prop__chksum_cols))));

    fprintf(file_ptr, "list->H5P_remove__num_calls                      = %lld\n",
            (unsigned long long)(atomic_load(&(list->H5P_remove__num_calls))));
    fprintf(file_ptr, "list->H5P__mt_delete_prop__list__num_calls       = %lld\n",
            (unsigned long long)(atomic_load(&(list->H5P__mt_delete_prop__list__num_calls))));
    fprintf(file_ptr, "list->num_delete_props_from_lfsll                = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_delete_props_from_lfsll))));
    fprintf(file_ptr, "list->delete_prop__max_nodes_visited             = %lld\n",
            (unsigned long long)(atomic_load(&(list->delete_prop__max_nodes_visited))));
    fprintf(file_ptr, "list->delete_prop__avg_nodes_visited             = %lld\n",
            (unsigned long long)(atomic_load(&(list->delete_prop__avg_nodes_visited))));
    fprintf(file_ptr, "list->num_delete_prop__nodes_visited             = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_delete_prop__nodes_visited))));
    fprintf(file_ptr, "list->num_delete_prop__cols                      = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_delete_prop__cols))));
    fprintf(file_ptr, "list->num_delete_prop__success                   = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_delete_prop__success))));
    fprintf(file_ptr, "list->num_delete_prop_chksum_cols                = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_delete_prop_chksum_cols))));
    fprintf(file_ptr, "list->num_delete_prop__base_delete_version       = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_delete_prop__base_delete_version))));
    fprintf(file_ptr, "list->num_delete_prop__curr_entry                = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_delete_prop__curr_entry))));
    fprintf(file_ptr, "list->num_delete_prop__older_curr                = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_delete_prop__older_curr))));

    fprintf(file_ptr, "list->H5P__find_prop_plist__num_calls            = %lld\n",
            (unsigned long long)(atomic_load(&(list->H5P__find_prop_plist__num_calls))));
    fprintf(file_ptr, "list->H5P__mt_search_prop__list__num_calls       = %lld\n",
            (unsigned long long)(atomic_load(&(list->H5P__mt_search_prop__list__num_calls))));
    fprintf(file_ptr, "list->search_list__max_nodes_visited             = %lld\n",
            (unsigned long long)(atomic_load(&(list->search_list__max_nodes_visited))));
    fprintf(file_ptr, "list->search_list__avg_nodes_visited             = %lld\n",
            (unsigned long long)(atomic_load(&(list->search_list__avg_nodes_visited))));
    fprintf(file_ptr, "list->num_search_list__nodes_visited             = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_search_list__nodes_visited))));
    fprintf(file_ptr, "list->num_search_list__success                   = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_search_list__success))));
    fprintf(file_ptr, "list->num_search_list__found_base                = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_search_list__found_base))));
    fprintf(file_ptr, "list->num_search_list__found_curr                = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_search_list__found_curr))));
    fprintf(file_ptr, "list->num_target_prop_found_but_deleted          = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_target_prop_found_but_deleted))));

    fprintf(file_ptr, "list->num_wait_for_curr_version_to_inc           = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_wait_for_curr_version_to_inc))));

    fprintf(file_ptr, "list->num_thrd_update_cols                       = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_thrd_update_cols))));
    fprintf(file_ptr, "list->num_thrd_count_update                      = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_thrd_count_update))));
    fprintf(file_ptr, "list->num_thrd_closing_flag_set                  = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_thrd_closing_flag_set))));
    fprintf(file_ptr, "list->num_thrd_opening_flag_set                  = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_thrd_opening_flag_set))));

    fprintf(file_ptr, "list->num_inherited_with_create_cb               = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_inherited_with_create_cb))));
    fprintf(file_ptr, "list->num_lkup_tbl_copy_entries_blank            = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_lkup_tbl_copy_entries_blank))));

done:

    if (name) {
        free(name);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__dump_stats_class() */

#else /* H5_HAVE_MULTITHREAD */

/*-------------------------------------------------------------------------
 * Function:    H5P_init_phase1
 *
 * Purpose:     Initialize the interface from some other layer. This should
 *              be followed with a call to H5P_init_phase2 after the H5P
 *              interface is completely setup.
 *
 * Return:      Success:    non-negative
 *              Failure:    negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5P_init_phase1(void)
{
    size_t tot_init = 0; /* Total # of classes initialized */
    size_t pass_init;    /* # of classes initialized in each pass */
    size_t u;
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDcompile_assert(H5P_TYPE_REFERENCE_ACCESS == (H5P_TYPE_MAX_TYPE - 1));

    /*
     * Initialize the Generic Property class & object groups.
     */
    if (H5I_register_type(H5I_GENPROPCLS_CLS) < 0)
        HGOTO_ERROR(H5E_ID, H5E_CANTINIT, FAIL, "unable to initialize ID group");
    if (H5I_register_type(H5I_GENPROPLST_CLS) < 0)
        HGOTO_ERROR(H5E_ID, H5E_CANTINIT, FAIL, "unable to initialize ID group");

    /* Repeatedly pass over the list of property list classes for the library,
     * initializing each class if its parent class is initialized, until no
     * more progress is made.
     */
    tot_init = 0;
    do {
        /* Reset pass initialization counter */
        pass_init = 0;

        /* Make a pass over all the library's property list classes */
        for (u = 0; u < NELMTS(init_class); u++) {
            H5P_libclass_t const *lib_class = init_class[u]; /* Current class to operate on */

            /* Check if the current class hasn't been initialized and can be now */
            assert(lib_class->class_id);
            if (*lib_class->class_id == (-1) &&
                (lib_class->par_pclass == NULL || *lib_class->par_pclass != NULL)) {
                /* Sanity check - only the root class is not allowed to have a parent class */
                assert(lib_class->par_pclass || lib_class == H5P_CLS_ROOT);

                /* Allocate the new class */
                if (NULL == (*lib_class->pclass = H5P__create_class(
                                 lib_class->par_pclass ? *lib_class->par_pclass : NULL, lib_class->name,
                                 lib_class->type, lib_class->create_func, lib_class->create_data,
                                 lib_class->copy_func, lib_class->copy_data, lib_class->close_func,
                                 lib_class->close_data)))
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "class initialization failed");

                /* Call routine to register properties for class */
                if (lib_class->reg_prop_func && (*lib_class->reg_prop_func)(*lib_class->pclass) < 0)
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "can't register properties");

                /* Register the new class */
                if ((*lib_class->class_id = H5I_register(H5I_GENPROP_CLS, *lib_class->pclass, FALSE)) < 0)
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "can't register property list class");

                /* Only register the default property list if it hasn't been created yet */
                if (lib_class->def_plist_id && *lib_class->def_plist_id == (-1)) {
                    /* Register the default property list for the new class*/
                    if ((*lib_class->def_plist_id = H5P_create_id(*lib_class->pclass, FALSE)) < 0)
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL,
                                    "can't register default property list for class");
                } /* end if */

                /* Increment class initialization counters */
                pass_init++;
                tot_init++;
            } /* end if */
        }     /* end for */
    } while (pass_init > 0);

    /* Verify that all classes were initialized */
    assert(tot_init == NELMTS(init_class));

done:
    if (ret_value < 0 && tot_init > 0) {
        /* First uninitialize all default property lists */
        H5I_clear_type(H5I_GENPROP_LST, FALSE, FALSE);

        /* Then uninitialize any initialized libclass */
        for (u = 0; u < NELMTS(init_class); u++) {
            H5P_libclass_t const *lib_class = init_class[u]; /* Current class to operate on */

            assert(lib_class->class_id);
            if (*lib_class->class_id >= 0) {
                /* Close the class ID */
                if (H5I_dec_ref(*lib_class->class_id) < 0)
                    HDONE_ERROR(H5E_PLIST, H5E_CLOSEERROR, FAIL, "unable to close property list class ID");
            }
            else if (lib_class->pclass && *lib_class->pclass) {
                /* Close a half-initialized pclass */
                if (H5P__close_class(*lib_class->pclass) < 0)
                    HDONE_ERROR(H5E_PLIST, H5E_CLOSEERROR, FAIL, "unable to close property list class");
            }
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)
}

/*-------------------------------------------------------------------------
 * Function:    H5P_init_phase2
 *
 * Purpose:     Finish initializing the interface from some other package.
 *
 * Note:        This is broken out as a separate routine so that the
 *              library's default VFL driver can be chosen and initialized
 *              after the entire H5P interface has been initialized.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5P_init_phase2(void)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    /* Set up the default VFL driver */
    if (H5P__facc_set_def_driver() < 0) {
        HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, "unable to set default VFL driver");
    }

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P_init_phase2() */

/*--------------------------------------------------------------------------
 NAME
    H5P_term_package
 PURPOSE
    Terminate various H5P objects
 USAGE
    void H5P_term_package()
 RETURNS
    Non-negative on success/Negative on failure
 DESCRIPTION
    Release the ID group and any other resources allocated.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
     Can't report errors...
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
int
H5P_term_package(void)
{
    int n = 0;

    FUNC_ENTER_NOAPI_NOINIT_NOERR

    int64_t nlist, nclass;

    /* Destroy HDF5 library property classes & lists */

    /* Check if there are any open property list classes or lists */
    nclass = H5I_nmembers(H5I_GENPROP_CLS);
    nlist  = H5I_nmembers(H5I_GENPROP_LST);

    /* If there are any open classes or groups, attempt to get rid of them. */
    if ((nclass + nlist) > 0) {
        /* Clear the lists */
        if (nlist > 0) {
            (void)H5I_clear_type(H5I_GENPROP_LST, FALSE, FALSE);

            /* Reset the default property lists, if they've been closed */
            if (H5I_nmembers(H5I_GENPROP_LST) == 0) {
                H5P_LST_ATTRIBUTE_ACCESS_ID_g = H5I_INVALID_HID;
                H5P_LST_ATTRIBUTE_CREATE_ID_g = H5I_INVALID_HID;
                H5P_LST_DATASET_ACCESS_ID_g   = H5I_INVALID_HID;
                H5P_LST_DATASET_CREATE_ID_g   = H5I_INVALID_HID;
                H5P_LST_DATASET_XFER_ID_g     = H5I_INVALID_HID;
                H5P_LST_DATATYPE_ACCESS_ID_g  = H5I_INVALID_HID;
                H5P_LST_DATATYPE_CREATE_ID_g  = H5I_INVALID_HID;
                H5P_LST_FILE_ACCESS_ID_g      = H5I_INVALID_HID;
                H5P_LST_FILE_CREATE_ID_g      = H5I_INVALID_HID;
                H5P_LST_FILE_MOUNT_ID_g       = H5I_INVALID_HID;
                H5P_LST_GROUP_ACCESS_ID_g     = H5I_INVALID_HID;
                H5P_LST_GROUP_CREATE_ID_g     = H5I_INVALID_HID;
                H5P_LST_LINK_ACCESS_ID_g      = H5I_INVALID_HID;
                H5P_LST_LINK_CREATE_ID_g      = H5I_INVALID_HID;
                H5P_LST_MAP_ACCESS_ID_g       = H5I_INVALID_HID;
                H5P_LST_MAP_CREATE_ID_g       = H5I_INVALID_HID;
                H5P_LST_OBJECT_COPY_ID_g      = H5I_INVALID_HID;
                H5P_LST_REFERENCE_ACCESS_ID_g = H5I_INVALID_HID;
                H5P_LST_VOL_INITIALIZE_ID_g   = H5I_INVALID_HID;
            }
        }

        /* Only attempt to close the classes after all the lists are closed */
        if (nlist == 0 && nclass > 0) {
            (void)H5I_clear_type(H5I_GENPROP_CLS, FALSE, FALSE);

            /* Reset the default property classes and IDs if they've been closed */
            if (H5I_nmembers(H5I_GENPROP_CLS) == 0) {
                H5P_CLS_ROOT_g = NULL;

                H5P_CLS_ATTRIBUTE_ACCESS_g = NULL;
                H5P_CLS_ATTRIBUTE_CREATE_g = NULL;
                H5P_CLS_DATASET_ACCESS_g   = NULL;
                H5P_CLS_DATASET_CREATE_g   = NULL;
                H5P_CLS_DATASET_XFER_g     = NULL;
                H5P_CLS_DATATYPE_ACCESS_g  = NULL;
                H5P_CLS_DATATYPE_CREATE_g  = NULL;
                H5P_CLS_FILE_ACCESS_g      = NULL;
                H5P_CLS_FILE_CREATE_g      = NULL;
                H5P_CLS_FILE_MOUNT_g       = NULL;
                H5P_CLS_GROUP_ACCESS_g     = NULL;
                H5P_CLS_GROUP_CREATE_g     = NULL;
                H5P_CLS_LINK_ACCESS_g      = NULL;
                H5P_CLS_LINK_CREATE_g      = NULL;
                H5P_CLS_MAP_ACCESS_g       = NULL;
                H5P_CLS_MAP_CREATE_g       = NULL;
                H5P_CLS_OBJECT_COPY_g      = NULL;
                H5P_CLS_OBJECT_CREATE_g    = NULL;
                H5P_CLS_REFERENCE_ACCESS_g = NULL;
                H5P_CLS_STRING_CREATE_g    = NULL;
                H5P_CLS_VOL_INITIALIZE_g   = NULL;

                H5P_CLS_ROOT_ID_g = H5I_INVALID_HID;

                H5P_CLS_ATTRIBUTE_ACCESS_ID_g = H5I_INVALID_HID;
                H5P_CLS_ATTRIBUTE_CREATE_ID_g = H5I_INVALID_HID;
                H5P_CLS_DATASET_ACCESS_ID_g   = H5I_INVALID_HID;
                H5P_CLS_DATASET_CREATE_ID_g   = H5I_INVALID_HID;
                H5P_CLS_DATASET_XFER_ID_g     = H5I_INVALID_HID;
                H5P_CLS_DATATYPE_ACCESS_ID_g  = H5I_INVALID_HID;
                H5P_CLS_DATATYPE_CREATE_ID_g  = H5I_INVALID_HID;
                H5P_CLS_FILE_ACCESS_ID_g      = H5I_INVALID_HID;
                H5P_CLS_FILE_CREATE_ID_g      = H5I_INVALID_HID;
                H5P_CLS_FILE_MOUNT_ID_g       = H5I_INVALID_HID;
                H5P_CLS_GROUP_ACCESS_ID_g     = H5I_INVALID_HID;
                H5P_CLS_GROUP_CREATE_ID_g     = H5I_INVALID_HID;
                H5P_CLS_LINK_ACCESS_ID_g      = H5I_INVALID_HID;
                H5P_CLS_LINK_CREATE_ID_g      = H5I_INVALID_HID;
                H5P_CLS_MAP_ACCESS_ID_g       = H5I_INVALID_HID;
                H5P_CLS_MAP_CREATE_ID_g       = H5I_INVALID_HID;
                H5P_CLS_OBJECT_COPY_ID_g      = H5I_INVALID_HID;
                H5P_CLS_OBJECT_CREATE_ID_g    = H5I_INVALID_HID;
                H5P_CLS_REFERENCE_ACCESS_ID_g = H5I_INVALID_HID;
                H5P_CLS_STRING_CREATE_ID_g    = H5I_INVALID_HID;
                H5P_CLS_VOL_INITIALIZE_ID_g   = H5I_INVALID_HID;
            }
        }

        n++; /*H5I*/
    }
    else {
        /* Destroy the property list and class id groups */
        n += (H5I_dec_type_ref(H5I_GENPROP_LST) > 0);
        n += (H5I_dec_type_ref(H5I_GENPROP_CLS) > 0);
    } /* end else */

    FUNC_LEAVE_NOAPI(n)
} /* end H5P_term_package() */

/*-------------------------------------------------------------------------
 * Function:    H5P__close_class_cb
 *
 * Purpose:     Called when the ref count reaches zero on a property class's ID
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5P__close_class_cb(void *_pclass, void H5_ATTR_UNUSED **request)
{
    H5P_genclass_t *pclass = (H5P_genclass_t *)_pclass; /* Property list class to close */

    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(pclass);

    /* Close the property list class object */
    if (H5P__close_class(pclass) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CLOSEERROR, FAIL, "unable to close property list class");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P__close_class_cb() */

/*-------------------------------------------------------------------------
 * Function:    H5P__close_list_cb
 *
 * Purpose:     Called when the ref count reaches zero on a property list's ID
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5P__close_list_cb(void *_plist, void H5_ATTR_UNUSED **request)
{
    H5P_genplist_t *plist = (H5P_genplist_t *)_plist; /* Property list to close */

    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(plist);

    /* Close the property list object */
    if (H5P_close(plist) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CLOSEERROR, FAIL, "unable to close property list");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P__close_list_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P__do_prop_cb1
 PURPOSE
    Internal routine to call a property list callback routine and update
    the property list accordingly.
 USAGE
    herr_t H5P__do_prop_cb1(slist,prop,cb)
        H5SL_t *slist;          IN/OUT: Skip list to hold changed properties
        H5P_genprop_t *prop;    IN: Property to call callback for
        H5P_prp_cb1_t *cb;      IN: Callback routine to call
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Calls the callback routine passed in.  If the callback routine changes
    the property value, then the property is duplicated and added to skip list.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static herr_t
H5P__do_prop_cb1(H5SL_t *slist, H5P_genprop_t *prop, H5P_prp_cb1_t cb)
{
    void          *tmp_value = NULL;    /* Temporary value buffer */
    H5P_genprop_t *pcopy     = NULL;    /* Copy of property to insert into skip list */
    herr_t         ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(slist);
    assert(prop);
    assert(prop->cmp);
    assert(cb);

    /* Allocate space for a temporary copy of the property value */
    if (NULL == (tmp_value = H5MM_malloc(prop->size)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "memory allocation failed for temporary property value");
    H5MM_memcpy(tmp_value, prop->value, prop->size);

    /* Call "type 1" callback ('create', 'copy' or 'close') */
    if (cb(prop->name, prop->size, tmp_value) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "Property callback failed");

    /* Make a copy of the class's property */
    if (NULL == (pcopy = H5P__dup_prop(prop, H5P_PROP_WITHIN_LIST)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");

    /* Copy the changed value into the new property */
    H5MM_memcpy(pcopy->value, tmp_value, prop->size);

    /* Insert the changed property into the property list */
    if (H5P__add_prop(slist, pcopy) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL, "Can't insert property into skip list");

done:
    /* Release the temporary value buffer */
    if (tmp_value)
        H5MM_xfree(tmp_value);

    /* Cleanup on failure */
    if (ret_value < 0)
        if (pcopy)
            H5P__free_prop(pcopy);

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P__do_prop_cb1() */

/*--------------------------------------------------------------------------
 NAME
    H5P__copy_pclass
 PURPOSE
    Internal routine to copy a generic property class
 USAGE
    hid_t H5P__copy_pclass(pclass)
        H5P_genclass_t *pclass;      IN: Property class to copy
 RETURNS
    Success: valid property class ID on success (non-negative)
    Failure: negative
 DESCRIPTION
    Copy a property class and return the ID.  This routine does not make
    any callbacks.  (They are only make when operating on property lists).

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
H5P_genclass_t *
H5P__copy_pclass(H5P_genclass_t *pclass)
{
    H5P_genclass_t *new_pclass = NULL; /* Property list class copied */
    H5P_genprop_t  *pcopy;             /* Copy of property to insert into class */
    H5P_genclass_t *ret_value = NULL;  /* return value */

    FUNC_ENTER_PACKAGE

    assert(pclass);

    /*
     * Create new property class object
     */

    /* Create the new property list class */
    if (NULL == (new_pclass = H5P__create_class(pclass->parent, pclass->name, pclass->type,
                                                pclass->create_func, pclass->create_data, pclass->copy_func,
                                                pclass->copy_data, pclass->close_func, pclass->close_data)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "unable to create property list class");

    /* Copy the properties registered for this class */
    if (pclass->nprops > 0) {
        H5SL_node_t *curr_node; /* Current node in skip list */

        /* Walk through the properties in the old class */
        curr_node = H5SL_first(pclass->props);
        while (curr_node != NULL) {
            /* Make a copy of the class's property */
            if (NULL == (pcopy = H5P__dup_prop((H5P_genprop_t *)H5SL_item(curr_node), H5P_PROP_WITHIN_CLASS)))
                HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, NULL, "Can't copy property");

            /* Insert the initialized property into the property list */
            if (H5P__add_prop(new_pclass->props, pcopy) < 0)
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, NULL, "Can't insert property into class");

            /* Increment property count for class */
            new_pclass->nprops++;

            /* Get the next property node in the list */
            curr_node = H5SL_next(curr_node);
        } /* end while */
    }     /* end if */

    /* Set the return value */
    ret_value = new_pclass;

done:
    if (NULL == ret_value && new_pclass)
        H5P__close_class(new_pclass);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__copy_pclass() */

/*--------------------------------------------------------------------------
 NAME
    H5P_copy_plist
 PURPOSE
    Internal routine to copy a generic property list
 USAGE
        hid_t H5P_copy_plist(old_plist_id)
            hid_t old_plist_id;             IN: Property list ID to copy
 RETURNS
    Success: valid property list ID on success (non-negative)
    Failure: H5I_INVALID_HID
 DESCRIPTION
    Copy a property list and return the ID.  This routine calls the
    class 'copy' callback after any property 'copy' callbacks are called
    (assuming all property 'copy' callbacks return successfully).

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
hid_t
H5P_copy_plist(const H5P_genplist_t *old_plist, hbool_t app_ref)
{
    H5P_genclass_t *tclass;           /* Temporary class pointer */
    H5P_genplist_t *new_plist = NULL; /* New property list generated from copy */
    H5P_genprop_t  *tmp;              /* Temporary pointer to properties */
    H5P_genprop_t  *new_prop;         /* New property created for copy */
    hid_t           new_plist_id;     /* Property list ID of new list created */
    H5SL_node_t    *curr_node;        /* Current node in skip list */
    H5SL_t         *seen = NULL;      /* Skip list containing properties already seen */
    size_t          nseen;            /* Number of items 'seen' */
    hbool_t         has_parent_class; /* Flag to indicate that this property list's class has a parent */
    hid_t           ret_value = H5I_INVALID_HID; /* return value */

    FUNC_ENTER_NOAPI(H5I_INVALID_HID)

    assert(old_plist);

    /*
     * Create new property list object
     */

    /* Allocate room for the property list */
    if (NULL == (new_plist = H5FL_CALLOC(H5P_genplist_t)))
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, H5I_INVALID_HID, "memory allocation failed");

    /* Set class state */
    new_plist->pclass     = old_plist->pclass;
    new_plist->nprops     = 0;     /* Initially the plist has the same number of properties as the class */
    new_plist->class_init = FALSE; /* Initially, wait until the class callback finishes to set */

    /* Initialize the skip list to hold the changed properties */
    if ((new_plist->props = H5SL_create(H5SL_TYPE_STR, NULL)) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, H5I_INVALID_HID,
                    "can't create skip list for changed properties");

    /* Create the skip list for deleted properties */
    if ((new_plist->del = H5SL_create(H5SL_TYPE_STR, NULL)) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, H5I_INVALID_HID,
                    "can't create skip list for deleted properties");

    /* Create the skip list to hold names of properties already seen
     * (This prevents a property in the class hierarchy from having it's
     * 'create' callback called, if a property in the class hierarchy has
     * already been seen)
     */
    if ((seen = H5SL_create(H5SL_TYPE_STR, NULL)) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, H5I_INVALID_HID, "can't create skip list for seen properties");
    nseen = 0;

    /* Cycle through the deleted properties & copy them into the new list's deleted section */
    if (H5SL_count(old_plist->del) > 0) {
        curr_node = H5SL_first(old_plist->del);
        while (curr_node) {
            char *new_name; /* Pointer to new name */

            /* Duplicate string for insertion into new deleted property skip list */
            if ((new_name = H5MM_xstrdup((char *)H5SL_item(curr_node))) == NULL)
                HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, H5I_INVALID_HID, "memory allocation failed");

            /* Insert property name into deleted list */
            if (H5SL_insert(new_plist->del, new_name, new_name) < 0)
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, H5I_INVALID_HID,
                            "can't insert property into deleted skip list");

            /* Add property name to "seen" list */
            if (H5SL_insert(seen, new_name, new_name) < 0)
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, H5I_INVALID_HID,
                            "can't insert property into seen skip list");
            nseen++;

            /* Get the next property node in the skip list */
            curr_node = H5SL_next(curr_node);
        } /* end while */
    }     /* end if */

    /* Cycle through the properties and copy them also */
    if (H5SL_count(old_plist->props) > 0) {
        curr_node = H5SL_first(old_plist->props);
        while (curr_node) {
            /* Get a pointer to the node's property */
            tmp = (H5P_genprop_t *)H5SL_item(curr_node);

            /* Make a copy of the list's property */
            if (NULL == (new_prop = H5P__dup_prop(tmp, H5P_PROP_WITHIN_LIST)))
                HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, H5I_INVALID_HID, "Can't copy property");

            /* Call property copy callback, if it exists */
            if (new_prop->copy) {
                if ((new_prop->copy)(new_prop->name, new_prop->size, new_prop->value) < 0) {
                    H5P__free_prop(new_prop);
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, H5I_INVALID_HID, "Can't copy property");
                } /* end if */
            }     /* end if */

            /* Insert the initialized property into the property list */
            if (H5P__add_prop(new_plist->props, new_prop) < 0) {
                H5P__free_prop(new_prop);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, H5I_INVALID_HID, "Can't insert property into list");
            } /* end if */

            /* Add property name to "seen" list */
            if (H5SL_insert(seen, new_prop->name, new_prop->name) < 0)
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, H5I_INVALID_HID,
                            "can't insert property into seen skip list");
            nseen++;

            /* Increment the number of properties in list */
            new_plist->nprops++;

            /* Get the next property node in the skip list */
            curr_node = H5SL_next(curr_node);
        } /* end while */
    }     /* end if */

    /*
     * Check for copying class properties (up through list of parent classes also),
     * initialize each with default value & make property 'copy' callback.
     */
    tclass           = old_plist->pclass;
    has_parent_class = (hbool_t)(tclass != NULL && tclass->parent != NULL && tclass->parent->nprops > 0);
    while (tclass != NULL) {
        if (tclass->nprops > 0) {
            /* Walk through the properties in the old class */
            curr_node = H5SL_first(tclass->props);
            while (curr_node != NULL) {
                /* Get pointer to property from node */
                tmp = (H5P_genprop_t *)H5SL_item(curr_node);

                /* Only "copy" properties we haven't seen before */
                if (nseen == 0 || H5SL_search(seen, tmp->name) == NULL) {
                    /* Call property copy callback, if it exists */
                    if (tmp->copy) {
                        /* Call the callback & insert changed value into skip list (if necessary) */
                        if (H5P__do_prop_cb1(new_plist->props, tmp, tmp->copy) < 0)
                            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, H5I_INVALID_HID, "Can't create property");
                    } /* end if */

                    /* Add property name to "seen" list, if we have other classes to work on */
                    if (has_parent_class) {
                        if (H5SL_insert(seen, tmp->name, tmp->name) < 0)
                            HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, H5I_INVALID_HID,
                                        "can't insert property into seen skip list");
                        nseen++;
                    } /* end if */

                    /* Increment the number of properties in list */
                    new_plist->nprops++;
                } /* end if */

                /* Get the next property node in the skip list */
                curr_node = H5SL_next(curr_node);
            } /* end while */
        }     /* end if */

        /* Go up to parent class */
        tclass = tclass->parent;
    } /* end while */

    /* Increment the number of property lists derived from class */
    if (H5P__access_class(new_plist->pclass, H5P_MOD_INC_LST) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, H5I_INVALID_HID, "Can't increment class ref count");

    /* Get an ID for the property list */
    if ((new_plist_id = H5I_register(H5I_GENPROP_LST, new_plist, app_ref)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to register property list");

    /* Save the property list ID in the property list struct, for use in the property class's 'close' callback
     */
    new_plist->plist_id = new_plist_id;

    /* Call the class callback (if it exists) now that we have the property list ID
     * (up through chain of parent classes also)
     */
    tclass = new_plist->pclass;
    while (NULL != tclass) {
        if (NULL != tclass->copy_func) {
            if ((tclass->copy_func)(new_plist_id, old_plist->plist_id, old_plist->pclass->copy_data) < 0) {
                /* Delete ID, ignore return value */
                H5I_remove(new_plist_id);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, H5I_INVALID_HID, "Can't initialize property");
            } /* end if */
        }     /* end if */

        /* Go up to parent class */
        tclass = tclass->parent;
    } /* end while */

    /* Set the class initialization flag */
    new_plist->class_init = TRUE;

    /* Set the return value */
    ret_value = new_plist_id;

done:
    /* Release the list of 'seen' properties */
    if (seen != NULL)
        H5SL_close(seen);

    if (H5I_INVALID_HID == ret_value && new_plist)
        H5P_close(new_plist);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_copy_plist() */

/*--------------------------------------------------------------------------
 NAME
    H5P__dup_prop
 PURPOSE
    Internal routine to duplicate a property
 USAGE
    H5P_genprop_t *H5P__dup_prop(oprop)
        H5P_genprop_t *oprop;   IN: Pointer to property to copy
        H5P_prop_within_t type; IN: Type of object the property will be inserted into
 RETURNS
    Returns a pointer to the newly created duplicate of a property on success,
        NULL on failure.
 DESCRIPTION
    Allocates memory and copies property information into a new property object.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static H5P_genprop_t *
H5P__dup_prop(H5P_genprop_t *oprop, H5P_prop_within_t type)
{
    H5P_genprop_t *prop      = NULL; /* Pointer to new property copied */
    H5P_genprop_t *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(oprop);
    assert(type != H5P_PROP_WITHIN_UNKNOWN);

    /* Allocate the new property */
    if (NULL == (prop = H5FL_MALLOC(H5P_genprop_t)))
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed");

    /* Copy basic property information */
    H5MM_memcpy(prop, oprop, sizeof(H5P_genprop_t));

    /* Check if we should duplicate the name or share it */

    /* Duplicating property for a class */
    if (type == H5P_PROP_WITHIN_CLASS) {
        assert(oprop->type == H5P_PROP_WITHIN_CLASS);
        assert(oprop->shared_name == FALSE);

        /* Duplicate name */
        prop->name = H5MM_xstrdup(oprop->name);
    } /* end if */
    /* Duplicating property for a list */
    else {
        /* Check if we are duplicating a property from a list or a class */

        /* Duplicating a property from a list */
        if (oprop->type == H5P_PROP_WITHIN_LIST) {
            /* If the old property's name wasn't shared, we have to copy it here also */
            if (!oprop->shared_name)
                prop->name = H5MM_xstrdup(oprop->name);
        } /* end if */
        /* Duplicating a property from a class */
        else {
            assert(oprop->type == H5P_PROP_WITHIN_CLASS);
            assert(oprop->shared_name == FALSE);

            /* Share the name */
            prop->shared_name = TRUE;

            /* Set the type */
            prop->type = type;
        } /* end else */
    }     /* end else */

    /* Duplicate current value, if it exists */
    if (oprop->value != NULL) {
        assert(prop->size > 0);
        if (NULL == (prop->value = H5MM_malloc(prop->size)))
            HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed");
        H5MM_memcpy(prop->value, oprop->value, prop->size);
    } /* end if */

    /* Set return value */
    ret_value = prop;

done:
    /* Free any resources allocated */
    if (ret_value == NULL) {
        if (prop != NULL) {
            if (prop->name != NULL)
                H5MM_xfree(prop->name);
            if (prop->value != NULL)
                H5MM_xfree(prop->value);
            prop = H5FL_FREE(H5P_genprop_t, prop);
        } /* end if */
    }     /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__dup_prop() */

/*--------------------------------------------------------------------------
 NAME
    H5P__create_prop
 PURPOSE
    Internal routine to create a new property
 USAGE
    H5P_genprop_t *H5P__create_prop(name,size,type,value,prp_create,prp_set,
                                   prp_get,prp_delete,prp_close, prp_encode, prp_decode)
        const char *name;       IN: Name of property to register
        size_t size;            IN: Size of property in bytes
        H5P_prop_within_t type; IN: Type of object the property will be inserted into
        void *value;            IN: Pointer to buffer containing value for property
        H5P_prp_create_func_t prp_create;   IN: Function pointer to property
                                    creation callback
        H5P_prp_set_func_t prp_set; IN: Function pointer to property set callback
        H5P_prp_get_func_t prp_get; IN: Function pointer to property get callback
        H5P_prp_encode_func_t prp_encode; IN: Function pointer to property encode
        H5P_prp_decode_func_t prp_decode; IN: Function pointer to property decode
        H5P_prp_delete_func_t prp_delete; IN: Function pointer to property delete callback
        H5P_prp_copy_func_t prp_copy; IN: Function pointer to property copy callback
        H5P_prp_compare_func_t prp_cmp; IN: Function pointer to property compare callback
        H5P_prp_close_func_t prp_close; IN: Function pointer to property close
                                    callback
 RETURNS
    Returns a pointer to the newly created property on success,
        NULL on failure.
 DESCRIPTION
    Allocates memory and copies property information into a new property object.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static H5P_genprop_t *
H5P__create_prop(const char *name, size_t size, H5P_prop_within_t type, const void *value,
                 H5P_prp_create_func_t prp_create, H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get,
                 H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode,
                 H5P_prp_delete_func_t prp_delete, H5P_prp_copy_func_t prp_copy,
                 H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close)
{
    H5P_genprop_t *prop      = NULL; /* Pointer to new property copied */
    H5P_genprop_t *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(name);
    assert((size > 0 && value != NULL) || (size == 0));
    assert(type != H5P_PROP_WITHIN_UNKNOWN);

    /* Allocate the new property */
    if (NULL == (prop = H5FL_MALLOC(H5P_genprop_t)))
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed");

    /* Set the property initial values */
    prop->name        = H5MM_xstrdup(name); /* Duplicate name */
    prop->shared_name = FALSE;
    prop->size        = size;
    prop->type        = type;

    /* Duplicate value, if it exists */
    if (value != NULL) {
        if (NULL == (prop->value = H5MM_malloc(prop->size)))
            HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed");
        H5MM_memcpy(prop->value, value, prop->size);
    } /* end if */
    else
        prop->value = NULL;

    /* Set the function pointers */
    prop->create = prp_create;
    prop->set    = prp_set;
    prop->get    = prp_get;
    prop->encode = prp_encode;
    prop->decode = prp_decode;
    prop->del    = prp_delete;
    prop->copy   = prp_copy;
    /* Use custom comparison routine if available, otherwise default to memcmp() */
    if (prp_cmp != NULL)
        prop->cmp = prp_cmp;
    else
        prop->cmp = &memcmp;
    prop->close = prp_close;

    /* Set return value */
    ret_value = prop;

done:
    /* Free any resources allocated */
    if (ret_value == NULL) {
        if (prop != NULL) {
            if (prop->name != NULL)
                H5MM_xfree(prop->name);
            if (prop->value != NULL)
                H5MM_xfree(prop->value);
            prop = H5FL_FREE(H5P_genprop_t, prop);
        } /* end if */
    }     /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__create_prop() */

/*--------------------------------------------------------------------------
 NAME
    H5P__add_prop
 PURPOSE
    Internal routine to insert a property into a property skip list
 USAGE
    herr_t H5P__add_prop(slist, prop)
        H5SL_t *slist;          IN/OUT: Pointer to skip list of properties
        H5P_genprop_t *prop;    IN: Pointer to property to insert
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
    Inserts a property into a skip list of properties.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P__add_prop(H5SL_t *slist, H5P_genprop_t *prop)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(slist);
    assert(prop);
    assert(prop->type != H5P_PROP_WITHIN_UNKNOWN);

    /* Insert property into skip list */
    if (H5SL_insert(slist, prop, prop->name) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL, "can't insert property into skip list");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__add_prop() */

/*--------------------------------------------------------------------------
 NAME
    H5P__find_prop_plist
 PURPOSE
    Internal routine to check for a property in a property list's skip list
 USAGE
    H5P_genprop_t *H5P_find_prop(plist, name)
        const H5P_genplist_t *plist;  IN: Pointer to property list to check
        const char *name;       IN: Name of property to check for
 RETURNS
    Returns pointer to property on success, NULL on failure.
 DESCRIPTION
    Checks for a property in a property list's skip list of properties.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
H5P_genprop_t *
H5P__find_prop_plist(const H5P_genplist_t *plist, const char *name)
{
    H5P_genprop_t *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(plist);
    assert(name);

    /* Check if the property has been deleted from list */
    if (H5SL_search(plist->del, name) != NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, NULL, "property deleted from skip list");
    } /* end if */
    else {
        /* Get the property data from the skip list */
        if (NULL == (ret_value = (H5P_genprop_t *)H5SL_search(plist->props, name))) {
            H5P_genclass_t *tclass; /* Temporary class pointer */

            /* Couldn't find property in list itself, start searching through class info */
            tclass = plist->pclass;
            while (tclass != NULL) {
                /* Find the property in the class */
                if (NULL != (ret_value = (H5P_genprop_t *)H5SL_search(tclass->props, name)))
                    /* Got pointer to property - leave now */
                    break;

                /* Go up to parent class */
                tclass = tclass->parent;
            } /* end while */

            /* Check if we haven't found the property */
            if (ret_value == NULL)
                HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, NULL, "can't find property in skip list");
        } /* end else */
    }     /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__find_prop_plist() */

/*--------------------------------------------------------------------------
 NAME
    H5P__find_prop_pclass
 PURPOSE
    Internal routine to check for a property in a class skip list
 USAGE
    H5P_genprop_t *H5P__find_prop_class(pclass, name)
        H5P_genclass *pclass;   IN: Pointer generic property class to check
        const char *name;       IN: Name of property to check for
 RETURNS
    Returns pointer to property on success, NULL on failure.
 DESCRIPTION
    Checks for a property in a class's skip list of properties.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static H5P_genprop_t *
H5P__find_prop_pclass(H5P_genclass_t *pclass, const char *name)
{
    H5P_genprop_t *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(pclass);
    assert(name);

    /* Get the property from the skip list */
    if (NULL == (ret_value = (H5P_genprop_t *)H5SL_search(pclass->props, name)))
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, NULL, "can't find property in skip list");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__find_prop_pclass() */

/*--------------------------------------------------------------------------
 NAME
    H5P__free_prop
 PURPOSE
    Internal routine to destroy a property node
 USAGE
    herr_t H5P__free_prop(prop)
        H5P_genprop_t *prop;    IN: Pointer to property to destroy
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
    Releases all the memory for a property list.  Does _not_ call the
    properties 'close' callback, that should already have been done.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static herr_t
H5P__free_prop(H5P_genprop_t *prop)
{
    FUNC_ENTER_PACKAGE_NOERR

    assert(prop);

    /* Release the property value if it exists */
    if (prop->value)
        H5MM_xfree(prop->value);

    /* Only free the name if we own it */
    if (!prop->shared_name)
        H5MM_xfree(prop->name);

    prop = H5FL_FREE(H5P_genprop_t, prop);

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* H5P__free_prop() */

/*--------------------------------------------------------------------------
 NAME
    H5P__free_prop_cb
 PURPOSE
    Internal routine to properties from a property skip list
 USAGE
    herr_t H5P__free_prop_cb(item, key, op_data)
        void *item;             IN/OUT: Pointer to property
        void *key;              IN/OUT: Pointer to property key
        void *_make_cb;         IN: Whether to make property callbacks or not
 RETURNS
    Returns zero on success, negative on failure.
 DESCRIPTION
        Calls the property 'close' callback for a property & frees property
    info.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static herr_t
H5P__free_prop_cb(void *item, void H5_ATTR_UNUSED *key, void *op_data)
{
    H5P_genprop_t *tprop   = (H5P_genprop_t *)item; /* Temporary pointer to property */
    hbool_t        make_cb = *(hbool_t *)op_data;   /* Whether to make property 'close' callback */

    FUNC_ENTER_PACKAGE_NOERR

    assert(tprop);

    /* Call the close callback and ignore the return value, there's nothing we can do about it */
    if (make_cb && tprop->close != NULL)
        (tprop->close)(tprop->name, tprop->size, tprop->value);

    /* Free the property, ignoring return value, nothing we can do */
    H5P__free_prop(tprop);

    FUNC_LEAVE_NOAPI(0)
} /* H5P__free_prop_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P__free_del_name_cb
 PURPOSE
    Internal routine to free 'deleted' property name
 USAGE
    herr_t H5P__free_del_name_cb(item, key, op_data)
        void *item;             IN/OUT: Pointer to deleted name
        void *key;              IN/OUT: Pointer to key
        void *op_data;          IN: Operator callback data (unused)
 RETURNS
    Returns zero on success, negative on failure.
 DESCRIPTION
    Frees the deleted property name
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static herr_t
H5P__free_del_name_cb(void *item, void H5_ATTR_UNUSED *key, void H5_ATTR_UNUSED *op_data)
{
    char *del_name = (char *)item; /* Temporary pointer to deleted name */

    FUNC_ENTER_PACKAGE_NOERR

    assert(del_name);

    /* Free the name */
    H5MM_xfree(del_name);

    FUNC_LEAVE_NOAPI(0)
} /* H5P__free_del_name_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P__access_class
 PURPOSE
    Internal routine to increment or decrement list & class dependencies on a
        property list class
 USAGE
    herr_t H5P__access_class(pclass,mod)
        H5P_genclass_t *pclass;     IN: Pointer to class to modify
        H5P_class_mod_t mod;        IN: Type of modification to class
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Increment/Decrement the class or list dependencies for a given class.
    This routine is the final arbiter on decisions about actually releasing a
    class in memory, such action is only taken when the reference counts for
    both dependent classes & lists reach zero.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P__access_class(H5P_genclass_t *pclass, H5P_class_mod_t mod)
{
    FUNC_ENTER_PACKAGE_NOERR

    assert(pclass);
    assert(mod > H5P_MOD_ERR && mod < H5P_MOD_MAX);

    switch (mod) {
        case H5P_MOD_INC_CLS: /* Increment the dependent class count*/
            pclass->classes++;
            break;

        case H5P_MOD_DEC_CLS: /* Decrement the dependent class count*/
            pclass->classes--;
            break;

        case H5P_MOD_INC_LST: /* Increment the dependent list count*/
            pclass->plists++;
            break;

        case H5P_MOD_DEC_LST: /* Decrement the dependent list count*/
            pclass->plists--;
            break;

        case H5P_MOD_INC_REF: /* Increment the ID reference count*/
            /* Reset the deleted flag if incrementing the reference count */
            if (pclass->deleted)
                pclass->deleted = FALSE;
            pclass->ref_count++;
            break;

        case H5P_MOD_DEC_REF: /* Decrement the ID reference count*/
            pclass->ref_count--;

            /* Mark the class object as deleted if reference count drops to zero */
            if (pclass->ref_count == 0)
                pclass->deleted = TRUE;
            break;

        case H5P_MOD_ERR:
        case H5P_MOD_MAX:
        default:
            assert(0 && "Invalid H5P class modification");
    } /* end switch */

    /* Check if we can release the class information now */
    if (pclass->deleted && pclass->plists == 0 && pclass->classes == 0) {
        H5P_genclass_t *par_class = pclass->parent; /* Pointer to class's parent */

        assert(pclass->name);
        H5MM_xfree(pclass->name);

        /* Free the class properties without making callbacks */
        if (pclass->props) {
            hbool_t make_cb = FALSE;

            H5SL_destroy(pclass->props, H5P__free_prop_cb, &make_cb);
        } /* end if */

        pclass = H5FL_FREE(H5P_genclass_t, pclass);

        /* Reduce the number of dependent classes on parent class also */
        if (par_class != NULL)
            H5P__access_class(par_class, H5P_MOD_DEC_CLS);
    } /* end if */

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* H5P__access_class() */

/*--------------------------------------------------------------------------
 NAME
    H5P__open_class_path_cb
 PURPOSE
    Internal callback routine to check for duplicated names in parent class.
 USAGE
    int H5P__open_class_path_cb(obj, id, key)
        H5P_genclass_t *obj;    IN: Pointer to class
        hid_t id;               IN: ID of object being looked at
        const void *key;        IN: Pointer to information used to compare
                                    classes.
 RETURNS
    Returns >0 on match, 0 on no match and <0 on failure.
 DESCRIPTION
    Checks whether a property list class has the same parent and name as a
    new class being created.  This is a callback routine for H5I_search()
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static int
H5P__open_class_path_cb(void *_obj, hid_t H5_ATTR_UNUSED id, void *_key)
{
    H5P_genclass_t    *obj       = (H5P_genclass_t *)_obj;    /* Pointer to the class for this ID */
    H5P_check_class_t *key       = (H5P_check_class_t *)_key; /* Pointer to key information for comparison */
    int                ret_value = 0;                         /* Return value */

    FUNC_ENTER_PACKAGE_NOERR

    assert(obj);
    assert(H5I_GENPROP_CLS == H5I_get_type(id));
    assert(key);

    /* Check if the class object has the same parent as the new class */
    if (obj->parent == key->parent) {
        /* Check if they have the same name */
        if (HDstrcmp(obj->name, key->name) == 0) {
            key->new_class = obj;
            ret_value      = 1; /* Indicate a match */
        }                       /* end if */
    }                           /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P__open_class_path_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P__create_class
 PURPOSE
    Internal routine to create a new property list class.
 USAGE
    H5P_genclass_t H5P__create_class(par_class, name, type,
                cls_create, create_data, cls_close, close_data)
        H5P_genclass_t *par_class;  IN: Pointer to parent class
        const char *name;       IN: Name of class we are creating
        H5P_plist_type_t type;  IN: Type of class we are creating
        H5P_cls_create_func_t;  IN: The callback function to call when each
                                    property list in this class is created.
        void *create_data;      IN: Pointer to user data to pass along to class
                                    creation callback.
        H5P_cls_copy_func_t;    IN: The callback function to call when each
                                    property list in this class is copied.
        void *copy_data;        IN: Pointer to user data to pass along to class
                                    copy callback.
        H5P_cls_close_func_t;   IN: The callback function to call when each
                                    property list in this class is closed.
        void *close_data;       IN: Pointer to user data to pass along to class
                                    close callback.
 RETURNS
    Returns a pointer to the newly created property list class on success,
        NULL on failure.
 DESCRIPTION
    Allocates memory and attaches a class to the property list class hierarchy.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
H5P_genclass_t *
H5P__create_class(H5P_genclass_t *par_class, const char *name, H5P_plist_type_t type,
                  H5P_cls_create_func_t cls_create, void *create_data, H5P_cls_copy_func_t cls_copy,
                  void *copy_data, H5P_cls_close_func_t cls_close, void *close_data)
{
    H5P_genclass_t *pclass    = NULL; /* Property list class created */
    H5P_genclass_t *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(name);
    /* Allow internal classes to break some rules */
    /* (This allows the root of the tree to be created with this routine -QAK) */
    if (type == H5P_TYPE_USER)
        assert(par_class);

    /* Allocate room for the class */
    if (NULL == (pclass = H5FL_CALLOC(H5P_genclass_t)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "property list class allocation failed");

    /* Set class state */
    pclass->parent = par_class;
    if (NULL == (pclass->name = H5MM_xstrdup(name)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "property list class name allocation failed");
    pclass->type      = type;
    pclass->nprops    = 0;                /* Classes are created without properties initially */
    pclass->plists    = 0;                /* No properties lists of this class yet */
    pclass->classes   = 0;                /* No classes derived from this class yet */
    pclass->ref_count = 1;                /* This is the first reference to the new class */
    pclass->deleted   = FALSE;            /* Not deleted yet... :-) */
    pclass->revision  = H5P_GET_NEXT_REV; /* Get a revision number for the class */

    /* Create the skip list for properties */
    if (NULL == (pclass->props = H5SL_create(H5SL_TYPE_STR, NULL)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "can't create skip list for properties");

    /* Set callback functions and pass-along data */
    pclass->create_func = cls_create;
    pclass->create_data = create_data;
    pclass->copy_func   = cls_copy;
    pclass->copy_data   = copy_data;
    pclass->close_func  = cls_close;
    pclass->close_data  = close_data;

    /* Increment parent class's derived class value */
    if (par_class != NULL) {
        if (H5P__access_class(par_class, H5P_MOD_INC_CLS) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, NULL, "Can't increment parent class ref count");
    } /* end if */

    /* Set return value */
    ret_value = pclass;

done:
    /* Free any resources allocated */
    if (ret_value == NULL)
        if (pclass) {
            if (pclass->name)
                H5MM_xfree(pclass->name);
            if (pclass->props) {
                hbool_t make_cb = FALSE;

                H5SL_destroy(pclass->props, H5P__free_prop_cb, &make_cb);
            } /* end if */
            pclass = H5FL_FREE(H5P_genclass_t, pclass);
        } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__create_class() */

/*--------------------------------------------------------------------------
 NAME
    H5P__create
 PURPOSE
    Internal routine to create a new property list of a property list class.
 USAGE
    H5P_genplist_t *H5P__create(class)
        H5P_genclass_t *class;  IN: Property list class create list from
 RETURNS
    Returns a pointer to the newly created property list on success,
        NULL on failure.
 DESCRIPTION
        Creates a property list of a given class.  If a 'create' callback
    exists for the property list class, it is called before the
    property list is passed back to the user.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
        If this routine is called from a library routine other than
    H5P_c, the calling routine is responsible for getting an ID for
    the property list and calling the class 'create' callback (if one exists)
    and also setting the "class_init" flag.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static H5P_genplist_t *
H5P__create(H5P_genclass_t *pclass)
{
    H5P_genclass_t *tclass;           /* Temporary class pointer */
    H5P_genplist_t *plist = NULL;     /* New property list created */
    H5P_genprop_t  *tmp;              /* Temporary pointer to parent class properties */
    H5SL_t         *seen      = NULL; /* Skip list to hold names of properties already seen */
    H5P_genplist_t *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(pclass);

    /*
     * Create new property list object
     */

    /* Allocate room for the property list */
    if (NULL == (plist = H5FL_CALLOC(H5P_genplist_t)))
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed");

    /* Set class state */
    plist->pclass     = pclass;
    plist->nprops     = 0;     /* Initially the plist has the same number of properties as the class */
    plist->class_init = FALSE; /* Initially, wait until the class callback finishes to set */

    /* Create the skip list for changed properties */
    if ((plist->props = H5SL_create(H5SL_TYPE_STR, NULL)) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "can't create skip list for changed properties");

    /* Create the skip list for deleted properties */
    if ((plist->del = H5SL_create(H5SL_TYPE_STR, NULL)) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "can't create skip list for deleted properties");

    /* Create the skip list to hold names of properties already seen
     * (This prevents a property in the class hierarchy from having it's
     * 'create' callback called, if a property in the class hierarchy has
     * already been seen)
     */
    if ((seen = H5SL_create(H5SL_TYPE_STR, NULL)) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "can't create skip list for seen properties");

    /*
     * Check if we should copy class properties (up through list of parent classes also),
     * initialize each with default value & make property 'create' callback.
     */
    tclass = pclass;
    while (tclass != NULL) {
        if (tclass->nprops > 0) {
            H5SL_node_t *curr_node; /* Current node in skip list */

            /* Walk through the properties in the old class */
            curr_node = H5SL_first(tclass->props);
            while (curr_node != NULL) {
                /* Get pointer to property from node */
                tmp = (H5P_genprop_t *)H5SL_item(curr_node);

                /* Only "create" properties we haven't seen before */
                if (H5SL_search(seen, tmp->name) == NULL) {
                    /* Call property creation callback, if it exists */
                    if (tmp->create) {
                        /* Call the callback & insert changed value into skip list (if necessary) */
                        if (H5P__do_prop_cb1(plist->props, tmp, tmp->create) < 0)
                            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, NULL, "Can't create property");
                    } /* end if */

                    /* Add property name to "seen" list */
                    if (H5SL_insert(seen, tmp->name, tmp->name) < 0)
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, NULL,
                                    "can't insert property into seen skip list");

                    /* Increment the number of properties in list */
                    plist->nprops++;
                } /* end if */

                /* Get the next property node in the skip list */
                curr_node = H5SL_next(curr_node);
            } /* end while */
        }     /* end if */

        /* Go up to parent class */
        tclass = tclass->parent;
    } /* end while */

    /* Increment the number of property lists derived from class */
    if (H5P__access_class(plist->pclass, H5P_MOD_INC_LST) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, NULL, "Can't increment class ref count");

    /* Set return value */
    ret_value = plist;

done:
    /* Release the skip list of 'seen' properties */
    if (seen != NULL)
        H5SL_close(seen);

    /* Release resources allocated on failure */
    if (ret_value == NULL) {
        if (plist != NULL) {
            /* Close & free any changed properties */
            if (plist->props) {
                unsigned make_cb = 1;

                H5SL_destroy(plist->props, H5P__free_prop_cb, &make_cb);
            } /* end if */

            /* Close the deleted property skip list */
            if (plist->del)
                H5SL_close(plist->del);

            /* Release the property list itself */
            plist = H5FL_FREE(H5P_genplist_t, plist);
        } /* end if */
    }     /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__create() */

/*--------------------------------------------------------------------------
 NAME
    H5P_create_id
 PURPOSE
    Internal routine to create a new property list of a property list class.
 USAGE
    hid_t H5P_create_id(pclass)
        H5P_genclass_t *pclass;       IN: Property list class create list from
 RETURNS
    Returns a valid property list ID on success, H5I_INVALID_HID on failure.
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
H5P_create_id(H5P_genclass_t *pclass, hbool_t app_ref)
{
    H5P_genclass_t *tclass;                      /* Temporary class pointer */
    H5P_genplist_t *plist     = NULL;            /* Property list created */
    hid_t           plist_id  = FAIL;            /* Property list ID */
    hid_t           ret_value = H5I_INVALID_HID; /* return value */

    FUNC_ENTER_NOAPI(H5I_INVALID_HID)

    assert(pclass);

    /* Create the new property list */
    if ((plist = H5P__create(pclass)) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, H5I_INVALID_HID, "unable to create property list");

    /* Get an ID for the property list */
    if ((plist_id = H5I_register(H5I_GENPROP_LST, plist, app_ref)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to register property list");

    /* Save the property list ID in the property list struct, for use in the property class's 'close' callback
     */
    plist->plist_id = plist_id;

    /* Call the class callback (if it exists) now that we have the property list ID
     * (up through chain of parent classes also)
     */
    tclass = plist->pclass;
    while (NULL != tclass) {
        if (NULL != tclass->create_func) {
            if ((tclass->create_func)(plist_id, tclass->create_data) < 0) {
                /* Delete ID, ignore return value */
                H5I_remove(plist_id);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, H5I_INVALID_HID, "Can't initialize property");
            } /* end if */
        }     /* end if */

        /* Go up to parent class */
        tclass = tclass->parent;
    } /* end while */

    /* Set the class initialization flag */
    plist->class_init = TRUE;

    /* Set the return value */
    ret_value = plist_id;

done:
    if (H5I_INVALID_HID == ret_value && plist)
        H5P_close(plist);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_create_id() */

/*--------------------------------------------------------------------------
 NAME
    H5P__register_real
 PURPOSE
    Internal routine to register a new property in a property list class.
 USAGE
    herr_t H5P__register_real(class, name, size, default, prp_create, prp_set,
                             prp_get, prp_close, prp_encode, prp_decode)
        H5P_genclass_t *class;  IN: Property list class to modify
        const char *name;       IN: Name of property to register
        size_t size;            IN: Size of property in bytes
        void *def_value;        IN: Pointer to buffer containing default value
                                    for property in newly created property lists
        H5P_prp_create_func_t prp_create;   IN: Function pointer to property
                                    creation callback
        H5P_prp_set_func_t prp_set; IN: Function pointer to property set callback
        H5P_prp_get_func_t prp_get; IN: Function pointer to property get callback
        H5P_prp_encode_func_t prp_encode; IN: Function pointer to property encode
        H5P_prp_decode_func_t prp_decode; IN: Function pointer to property decode
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

        The 'encode' callback is called when a property list with this
    property is being encoded.  H5P_prp_encode_func_t is defined as:
        typedef herr_t (*H5P_prp_encode_func_t)(void *f, size_t *size,
        void *value, void *plist, uint8_t **buf);
    where the parameters to the callback function are:
        void *f;            IN: A fake file structure used to encode.
        size_t *size;       IN/OUT: The size of the buffer to encode the property.
        void *value;        IN: The value of the property being encoded.
        void *plist;        IN: The property list structure.
        uint8_t **buf;      OUT: The buffer that holds the encoded property;
    The 'encode' routine returns the size needed to encode the property value
    if the buffer passed in is NULL or the size is zero. Otherwise it encodes
    the property value into binary in buf.

        The 'decode' callback is called when a property list with this
    property is being decoded.  H5P_prp_encode_func_t is defined as:
        typedef herr_t (*H5P_prp_encode_func_t)(void *f, size_t *size,
        void *value, void *plist, uint8_t **buf);
    where the parameters to the callback function are:
        void *f;            IN: A fake file structure used to decode.
        size_t *size;       IN: H5_ATTR_UNUSED
        void *value;        IN: H5_ATTR_UNUSED
        void *plist;        IN: The property list structure.
        uint8_t **buf;      IN: The buffer that holds the binary encoded property;
    The 'decode' routine decodes the binary buffer passed in and transforms it into
    corresponding property values that are set in the property list passed in.

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
H5P__register_real(H5P_genclass_t *pclass, const char *name, size_t size, const void *def_value,
                   H5P_prp_create_func_t prp_create, H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get,
                   H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode,
                   H5P_prp_delete_func_t prp_delete, H5P_prp_copy_func_t prp_copy,
                   H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close)
{
    H5P_genprop_t *new_prop  = NULL;    /* Temporary property pointer */
    herr_t         ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(pclass);
    assert(0 == pclass->plists);
    assert(0 == pclass->classes);
    assert(name);
    assert((size > 0 && def_value != NULL) || (size == 0));

    /* Check for duplicate named properties */
    if (NULL != H5SL_search(pclass->props, name))
        HGOTO_ERROR(H5E_PLIST, H5E_EXISTS, FAIL, "property already exists");

    /* Create property object from parameters */
    if (NULL == (new_prop = H5P__create_prop(name, size, H5P_PROP_WITHIN_CLASS, def_value, prp_create,
                                             prp_set, prp_get, prp_encode, prp_decode, prp_delete, prp_copy,
                                             prp_cmp, prp_close)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, "Can't create property");

    /* Insert property into property list class */
    if (H5P__add_prop(pclass->props, new_prop) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL, "Can't insert property into class");

    /* Increment property count for class */
    pclass->nprops++;

    /* Update the revision for the class */
    pclass->revision = H5P_GET_NEXT_REV;

done:
    if (ret_value < 0)
        if (new_prop && H5P__free_prop(new_prop) < 0)
            HDONE_ERROR(H5E_PLIST, H5E_CANTRELEASE, FAIL, "unable to close property");

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__register_real() */

/*--------------------------------------------------------------------------
 NAME
    H5P__register
 PURPOSE
    Internal routine to register a new property in a property list class.
 USAGE
    herr_t H5P__register(class, name, size, default, prp_create, prp_set, prp_get, prp_close)
        H5P_genclass_t **class; IN: Property list class to modify
        const char *name;       IN: Name of property to register
        size_t size;            IN: Size of property in bytes
        void *def_value;        IN: Pointer to buffer containing default value
                                    for property in newly created property lists
        H5P_prp_create_func_t prp_create;   IN: Function pointer to property
                                    creation callback
        H5P_prp_set_func_t prp_set; IN: Function pointer to property set callback
        H5P_prp_get_func_t prp_get; IN: Function pointer to property get callback
        H5P_prp_encode_func_t prp_encode; IN: Function pointer to property encode
        H5P_prp_decode_func_t prp_decode; IN: Function pointer to property decode
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

        The 'encode' callback is called when a property list with this
    property is being encoded.  H5P_prp_encode_func_t is defined as:
        typedef herr_t (*H5P_prp_encode_func_t)(void *f, size_t *size,
        void *value, void *plist, uint8_t **buf);
    where the parameters to the callback function are:
        void *f;            IN: A fake file structure used to encode.
        size_t *size;       IN/OUT: The size of the buffer to encode the property.
        void *value;        IN: The value of the property being encoded.
        void *plist;        IN: The property list structure.
        uint8_t **buf;      OUT: The buffer that holds the encoded property;
    The 'encode' routine returns the size needed to encode the property value
    if the buffer passed in is NULL or the size is zero. Otherwise it encodes
    the property value into binary in buf.

        The 'decode' callback is called when a property list with this
    property is being decoded.  H5P_prp_encode_func_t is defined as:
        typedef herr_t (*H5P_prp_encode_func_t)(void *f, size_t *size,
        void *value, void *plist, uint8_t **buf);
    where the parameters to the callback function are:
        void *f;            IN: A fake file structure used to decode.
        size_t *size;       IN: H5_ATTR_UNUSED
        void *value;        IN: H5_ATTR_UNUSED
        void *plist;        IN: The property list structure.
        uint8_t **buf;      IN: The buffer that holds the binary encoded property;
    The 'decode' routine decodes the binary buffer passed in and transforms it into
    corresponding property values that are set in the property list passed in.

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
H5P__register(H5P_genclass_t **ppclass, const char *name, size_t size, const void *def_value,
              H5P_prp_create_func_t prp_create, H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get,
              H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode,
              H5P_prp_delete_func_t prp_delete, H5P_prp_copy_func_t prp_copy, H5P_prp_compare_func_t prp_cmp,
              H5P_prp_close_func_t prp_close)
{
    H5P_genclass_t *pclass    = *ppclass; /* Pointer to class to modify */
    H5P_genclass_t *new_class = NULL;     /* New class pointer */
    herr_t          ret_value = SUCCEED;  /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(ppclass);
    assert(pclass);

    /* Check if class needs to be split because property lists or classes have
     *  been created since the last modification was made to the class.
     */
    if (pclass->plists > 0 || pclass->classes > 0) {
        if (NULL == (new_class = H5P__create_class(
                         pclass->parent, pclass->name, pclass->type, pclass->create_func, pclass->create_data,
                         pclass->copy_func, pclass->copy_data, pclass->close_func, pclass->close_data)))
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "can't copy class");

        /* Walk through the skip list of the old class and copy properties */
        if (pclass->nprops > 0) {
            H5SL_node_t *curr_node; /* Current node in skip list */

            /* Walk through the properties in the old class */
            curr_node = H5SL_first(pclass->props);
            while (curr_node != NULL) {
                H5P_genprop_t *pcopy; /* Property copy */

                /* Make a copy of the class's property */
                if (NULL ==
                    (pcopy = H5P__dup_prop((H5P_genprop_t *)H5SL_item(curr_node), H5P_PROP_WITHIN_CLASS)))
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");

                /* Insert the initialized property into the property class */
                if (H5P__add_prop(new_class->props, pcopy) < 0)
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL, "Can't insert property into class");

                /* Increment property count for class */
                new_class->nprops++;

                /* Get the next property node in the skip list */
                curr_node = H5SL_next(curr_node);
            } /* end while */
        }     /* end if */

        /* Use the new class instead of the old one */
        pclass = new_class;
    } /* end if */

    /* Really register the property in the class */
    if (H5P__register_real(pclass, name, size, def_value, prp_create, prp_set, prp_get, prp_encode,
                           prp_decode, prp_delete, prp_copy, prp_cmp, prp_close) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, "can't register property");

    /* Update pointer to pointer to class, if a new one was generated */
    if (new_class)
        *ppclass = pclass;

done:
    if (ret_value < 0)
        if (new_class && H5P__close_class(new_class) < 0)
            HDONE_ERROR(H5E_PLIST, H5E_CANTRELEASE, FAIL, "unable to close new property class");

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__register() */

/*--------------------------------------------------------------------------
 NAME
    H5P_insert
 PURPOSE
    Internal routine to insert a new property in a property list.
 USAGE
    herr_t H5P_insert(plist, name, size, value, prp_set, prp_get, prp_close,
                      prp_encode, prp_decode)
        H5P_genplist_t *plist;  IN: Property list to add property to
        const char *name;       IN: Name of property to add
        size_t size;            IN: Size of property in bytes
        void *value;            IN: Pointer to the value for the property
        H5P_prp_set_func_t prp_set; IN: Function pointer to property set callback
        H5P_prp_get_func_t prp_get; IN: Function pointer to property get callback
        H5P_prp_encode_func_t prp_encode; IN: Function pointer to property encode
        H5P_prp_decode_func_t prp_decode; IN: Function pointer to property decode
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

        The 'encode' callback is called when a property list with this
    property is being encoded.  H5P_prp_encode_func_t is defined as:
        typedef herr_t (*H5P_prp_encode_func_t)(void *f, size_t *size,
        void *value, void *plist, uint8_t **buf);
    where the parameters to the callback function are:
        void *f;            IN: A fake file structure used to encode.
        size_t *size;       IN/OUT: The size of the buffer to encode the property.
        void *value;        IN: The value of the property being encoded.
        void *plist;        IN: The property list structure.
        uint8_t **buf;      OUT: The buffer that holds the encoded property;
    The 'encode' routine returns the size needed to encode the property value
    if the buffer passed in is NULL or the size is zero. Otherwise it encodes
    the property value into binary in buf.

        The 'decode' callback is called when a property list with this
    property is being decoded.  H5P_prp_encode_func_t is defined as:
        typedef herr_t (*H5P_prp_encode_func_t)(void *f, size_t *size,
        void *value, void *plist, uint8_t **buf);
    where the parameters to the callback function are:
        void *f;            IN: A fake file structure used to decode.
        size_t *size;       IN: H5_ATTR_UNUSED
        void *value;        IN: H5_ATTR_UNUSED
        void *plist;        IN: The property list structure.
        uint8_t **buf;      IN: The buffer that holds the binary encoded property;
    The 'decode' routine decodes the binary buffer passed in and transforms it into
    corresponding property values that are set in the property list passed in.

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
H5P_insert(H5P_genplist_t *plist, const char *name, size_t size, void *value, H5P_prp_set_func_t prp_set,
           H5P_prp_get_func_t prp_get, H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode,
           H5P_prp_delete_func_t prp_delete, H5P_prp_copy_func_t prp_copy, H5P_prp_compare_func_t prp_cmp,
           H5P_prp_close_func_t prp_close)
{
    H5P_genprop_t *new_prop  = NULL;    /* Temporary property pointer */
    herr_t         ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI_NOINIT

    assert(plist);
    assert(name);
    assert((size > 0 && value != NULL) || (size == 0));

    /* Check for duplicate named properties */
    if (NULL != H5SL_search(plist->props, name))
        HGOTO_ERROR(H5E_PLIST, H5E_EXISTS, FAIL, "property already exists");

    /* Check if the property has been deleted */
    if (NULL != H5SL_search(plist->del, name)) {
        char *temp_name = NULL;

        /* Remove the property name from the deleted property skip list */
        if (NULL == (temp_name = (char *)H5SL_remove(plist->del, name)))
            HGOTO_ERROR(H5E_PLIST, H5E_CANTDELETE, FAIL, "can't remove property from deleted skip list");

        /* free the name of the removed property */
        H5MM_xfree(temp_name);
    } /* end if */
    else {
        H5P_genclass_t *tclass; /* Temporary class pointer */

        /* Check if the property is already in the class hierarchy */
        tclass = plist->pclass;
        while (tclass) {
            if (tclass->nprops > 0) {
                /* Find the property in the class */
                if (NULL != H5SL_search(tclass->props, name))
                    HGOTO_ERROR(H5E_PLIST, H5E_EXISTS, FAIL, "property already exists");
            } /* end if */

            /* Go up to parent class */
            tclass = tclass->parent;
        } /* end while */
    }     /* end else */

    /* Ok to add to property list */

    /* Create property object from parameters */
    if (NULL ==
        (new_prop = H5P__create_prop(name, size, H5P_PROP_WITHIN_LIST, value, NULL, prp_set, prp_get,
                                     prp_encode, prp_decode, prp_delete, prp_copy, prp_cmp, prp_close)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, "Can't create property");

    /* Insert property into property list class */
    if (H5P__add_prop(plist->props, new_prop) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL, "Can't insert property into class");

    /* Increment property count for class */
    plist->nprops++;

done:
    if (ret_value < 0)
        if (new_prop && H5P__free_prop(new_prop) < 0)
            HDONE_ERROR(H5E_PLIST, H5E_CANTRELEASE, FAIL, "unable to close property");

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_insert() */

/*--------------------------------------------------------------------------
 NAME
    H5P__do_prop
 PURPOSE
    Internal routine to perform an operation on a property in a property list
 USAGE
    herr_t H5P__do_prop(plist, name, cb, udata)
        H5P_genplist_t *plist;  IN: Property list to find property in
        const char *name;       IN: Name of property to set
        H5P_do_plist_op_t plist_op;  IN: Pointer to the callback to invoke when the
                                    property is found in the property list
        H5P_do_pclass_op_t pclass_op; IN: Pointer to the callback to invoke when the
                                    property is found in the property class
        void *udata;            IN: Pointer to the user data for the callback
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Finds a property in a property list and calls the callback with it.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static herr_t
H5P__do_prop(H5P_genplist_t *plist, const char *name, H5P_do_plist_op_t plist_op,
             H5P_do_pclass_op_t pclass_op, void *udata)
{
    H5P_genclass_t *tclass;              /* Temporary class pointer */
    H5P_genprop_t  *prop;                /* Temporary property pointer */
    herr_t          ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(plist_op);
    assert(pclass_op);

    /* Check if the property has been deleted */
    if (NULL != H5SL_search(plist->del, name))
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property doesn't exist");

    /* Find property in changed list */
    if (NULL != (prop = (H5P_genprop_t *)H5SL_search(plist->props, name))) {
        /* Call the 'found in property list' callback */
        if ((*plist_op)(plist, name, prop, udata) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTOPERATE, FAIL, "can't operate on property");
    } /* end if */
    else {
        /*
         * Check if we should set class properties (up through list of parent classes also),
         * & make property 'set' callback.
         */
        tclass = plist->pclass;
        while (NULL != tclass) {
            if (tclass->nprops > 0) {
                /* Find the property in the class */
                if (NULL != (prop = (H5P_genprop_t *)H5SL_search(tclass->props, name))) {
                    /* Call the 'found in class' callback */
                    if ((*pclass_op)(plist, name, prop, udata) < 0)
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTOPERATE, FAIL, "can't operate on property");

                    /* Leave */
                    break;
                } /* end if */
            }     /* end if */

            /* Go up to parent class */
            tclass = tclass->parent;
        } /* end while */

        /* If we get this far, then it wasn't in the list of changed properties,
         * nor in the properties in the class hierarchy, indicate an error
         */
        if (NULL == tclass)
            HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "can't find property in skip list");
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__do_prop() */

/*--------------------------------------------------------------------------
 NAME
    H5P__poke_plist_cb
 PURPOSE
    Internal callback for H5P__do_prop, to overwrite a property's value in a property list.
 USAGE
    herr_t H5P__poke_plist_cb(plist, name, value)
        H5P_genplist_t *plist;  IN: Property list to overwrite property in
        const char *name;       IN: Name of property to overwrite
        H5P_genprop_t *prop;    IN: Property to overwrite
        void *udata;            IN: User data for operation
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
    Overwrite a value for a property in a property list.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
    Called when the property is found in the property list.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static herr_t
H5P__poke_plist_cb(H5P_genplist_t H5_ATTR_NDEBUG_UNUSED *plist, const char H5_ATTR_NDEBUG_UNUSED *name,
                   H5P_genprop_t *prop, void *_udata)
{
    H5P_prop_set_ud_t *udata     = (H5P_prop_set_ud_t *)_udata; /* User data for callback */
    herr_t             ret_value = SUCCEED;                     /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(prop);

    /* Check for property size >0 */
    if (0 == prop->size)
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "property has zero size");

    /* Overwrite value in property */
    H5MM_memcpy(prop->value, udata->value, prop->size);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__poke_plist_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P__poke_pclass_cb
 PURPOSE
    Internal callback for H5P__do_prop, to overwrite a property's value in a property list.
 USAGE
    herr_t H5P__poke_pclass_cb(plist, name, value)
        H5P_genplist_t *plist;  IN: Property list to overwrite property in
        const char *name;       IN: Name of property to overwrite
        H5P_genprop_t *prop;    IN: Property to overwrite
        void *udata;            IN: User data for operation
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
    Overwrite a value for a property in a property list.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
    Called when the property is found in the property class.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static herr_t
H5P__poke_pclass_cb(H5P_genplist_t *plist, const char H5_ATTR_NDEBUG_UNUSED *name, H5P_genprop_t *prop,
                    void *_udata)
{
    H5P_prop_set_ud_t *udata     = (H5P_prop_set_ud_t *)_udata; /* User data for callback */
    H5P_genprop_t     *pcopy     = NULL;    /* Copy of property to insert into skip list */
    herr_t             ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(prop);
    assert(prop->cmp);

    /* Check for property size >0 */
    if (0 == prop->size)
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "property has zero size");

    /* Make a copy of the class's property */
    if (NULL == (pcopy = H5P__dup_prop(prop, H5P_PROP_WITHIN_LIST)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");

    H5MM_memcpy(pcopy->value, udata->value, pcopy->size);

    /* Insert the changed property into the property list */
    if (H5P__add_prop(plist->props, pcopy) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL, "Can't insert changed property into skip list");

done:
    /* Cleanup on failure */
    if (ret_value < 0)
        if (pcopy)
            H5P__free_prop(pcopy);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__poke_pclass_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P_poke
 PURPOSE
    Internal routine to overwrite a property's value in a property list.
 USAGE
    herr_t H5P_poke(plist, name, value)
        H5P_genplist_t *plist;  IN: Property list to find property in
        const char *name;       IN: Name of property to overwrite
        void *value;            IN: Pointer to the value for the property
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Overwrites a property in a property list (i.e. a "shallow" copy over
    the property value).  The property name must exist or this routine will
    fail.  If there is a setget' callback routine registered for this property,
    it is _NOT_ called.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
        This routine may not be called for zero-sized properties and will
    return an error in that case.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P_poke(H5P_genplist_t *plist, const char *name, const void *value)
{
    H5P_prop_set_ud_t udata;               /* User data for callback */
    herr_t            ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(value);

    /* Find the property and set the value */
    udata.value = value;
    if (H5P__do_prop(plist, name, H5P__poke_plist_cb, H5P__poke_pclass_cb, &udata) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTOPERATE, FAIL, "can't operate on plist to overwrite value");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_poke() */

/*--------------------------------------------------------------------------
 NAME
    H5P__set_plist_cb
 PURPOSE
    Internal callback for H5P__do_prop, to set a property's value in a property list.
 USAGE
    herr_t H5P__set_plist_cb(plist, name, value)
        H5P_genplist_t *plist;  IN: Property list to set property in
        const char *name;       IN: Name of property to set
        H5P_genprop_t *prop;    IN: Property to set
        void *udata;            IN: User data for operation
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
    Sets a new value for a property in a property list.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
    Called when the property is found in the property list.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static herr_t
H5P__set_plist_cb(H5P_genplist_t *plist, const char *name, H5P_genprop_t *prop, void *_udata)
{
    H5P_prop_set_ud_t *udata     = (H5P_prop_set_ud_t *)_udata; /* User data for callback */
    void              *tmp_value = NULL;                        /* Temporary value for property */
    const void        *prp_value = NULL;                        /* Property value */
    herr_t             ret_value = SUCCEED;                     /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(prop);

    /* Check for property size >0 */
    if (0 == prop->size)
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "property has zero size");

    /* Make a copy of the value and pass to 'set' callback */
    if (NULL != prop->set) {
        /* Make a copy of the current value, in case the callback fails */
        if (NULL == (tmp_value = H5MM_malloc(prop->size)))
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "memory allocation failed temporary property value");
        H5MM_memcpy(tmp_value, udata->value, prop->size);

        /* Call user's callback */
        if ((*(prop->set))(plist->plist_id, name, prop->size, tmp_value) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "can't set property value");

        /* Set the pointer for copying */
        prp_value = tmp_value;
    } /* end if */
    /* No 'set' callback, just copy value */
    else
        prp_value = udata->value;

    /* Free any previous value for the property */
    if (NULL != prop->del) {
        /* Call user's 'delete' callback */
        if ((*(prop->del))(plist->plist_id, name, prop->size, prop->value) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTFREE, FAIL, "can't release property value");
    } /* end if */

    /* Copy new [possibly unchanged] value into property value */
    H5MM_memcpy(prop->value, prp_value, prop->size);

done:
    /* Free the temporary value buffer */
    if (tmp_value != NULL)
        H5MM_xfree(tmp_value);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__set_plist_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P__set_pclass_cb
 PURPOSE
    Internal callback for H5P__do_prop, to set a property's value in a property list.
 USAGE
    herr_t H5P__set_pclass_cb(plist, name, value)
        H5P_genplist_t *plist;  IN: Property list to set property in
        const char *name;       IN: Name of property to set
        H5P_genprop_t *prop;    IN: Property to set
        void *udata;            IN: User data for operation
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
    Sets a new value for a property in a property list.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
    Called when the property is found in the property class.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static herr_t
H5P__set_pclass_cb(H5P_genplist_t *plist, const char *name, H5P_genprop_t *prop, void *_udata)
{
    H5P_prop_set_ud_t *udata     = (H5P_prop_set_ud_t *)_udata; /* User data for callback */
    H5P_genprop_t     *pcopy     = NULL;    /* Copy of property to insert into skip list */
    void              *tmp_value = NULL;    /* Temporary value for property */
    const void        *prp_value = NULL;    /* Property value */
    herr_t             ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(prop);
    assert(prop->cmp);

    /* Check for property size >0 */
    if (0 == prop->size)
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "property has zero size");

    /* Make a copy of the value and pass to 'set' callback */
    if (NULL != prop->set) {
        /* Make a copy of the current value, in case the callback fails */
        if (NULL == (tmp_value = H5MM_malloc(prop->size)))
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "memory allocation failed temporary property value");
        H5MM_memcpy(tmp_value, udata->value, prop->size);

        /* Call user's callback */
        if ((*(prop->set))(plist->plist_id, name, prop->size, tmp_value) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "can't set property value");

        /* Set the pointer for copying */
        prp_value = tmp_value;
    } /* end if */
    /* No 'set' callback, just copy value */
    else
        prp_value = udata->value;

    /* Make a copy of the class's property */
    if (NULL == (pcopy = H5P__dup_prop(prop, H5P_PROP_WITHIN_LIST)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");

    H5MM_memcpy(pcopy->value, prp_value, pcopy->size);

    /* Insert the changed property into the property list */
    if (H5P__add_prop(plist->props, pcopy) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL, "Can't insert changed property into skip list");

done:
    /* Free the temporary value buffer */
    if (tmp_value != NULL)
        H5MM_xfree(tmp_value);

    /* Cleanup on failure */
    if (ret_value < 0)
        if (pcopy)
            H5P__free_prop(pcopy);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__set_pclass_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P_set
 PURPOSE
    Internal routine to set a property's value in a property list.
 USAGE
    herr_t H5P_set(plist, name, value)
        H5P_genplist_t *plist;  IN: Property list to find property in
        const char *name;       IN: Name of property to set
        const void *value;      IN: Pointer to the value for the property
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
H5P_set(H5P_genplist_t *plist, const char *name, const void *value)
{
    H5P_prop_set_ud_t udata;               /* User data for callback */
    herr_t            ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(value);

    /* Find the property and set the value */
    udata.value = value;
    if (H5P__do_prop(plist, name, H5P__set_plist_cb, H5P__set_pclass_cb, &udata) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTOPERATE, FAIL, "can't operate on plist to set value");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_set() */

/*--------------------------------------------------------------------------
 NAME
    H5P__class_get
 PURPOSE
    Internal routine to get a property's value from a property class.
 USAGE
    herr_t H5P__class_get(pclass, name, value)
        const H5P_genclass_t *pclass; IN: Property class to find property in
        const char *name;       IN: Name of property to get
        void *value;            IN: Pointer to the value for the property
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Gets the current value for a property in a property class.  The property
    name must exist or this routine will fail.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
        The 'get' callback routine registered for this property will _NOT_ be
    called, this routine is designed for internal library use only!

        This routine may not be called for zero-sized properties and will
    return an error in that case.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P__class_get(const H5P_genclass_t *pclass, const char *name, void *value)
{
    H5P_genprop_t *prop;                /* Temporary property pointer */
    herr_t         ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(pclass);
    assert(name);
    assert(value);

    /* Find property in list */
    if (NULL == (prop = (H5P_genprop_t *)H5SL_search(pclass->props, name)))
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property doesn't exist");

    /* Check for property size >0 */
    if (0 == prop->size)
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "property has zero size");

    /* Copy the property value */
    H5MM_memcpy(value, prop->value, prop->size);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__class_get() */

/*--------------------------------------------------------------------------
 NAME
    H5P__class_set
 PURPOSE
    Internal routine to set a property's value in a property class.
 USAGE
    herr_t H5P__class_set(pclass, name, value)
        const H5P_genclass_t *pclass; IN: Property class to find property in
        const char *name;       IN: Name of property to set
        const void *value;      IN: Pointer to the value for the property
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Sets a new value for a property in a property class.  The property name
    must exist or this routine will fail.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
        The 'set' callback routine registered for this property will _NOT_ be
    called, this routine is designed for internal library use only!

        This routine may not be called for zero-sized properties and will
    return an error in that case.

        The previous value is overwritten, not released in any way.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P__class_set(const H5P_genclass_t *pclass, const char *name, const void *value)
{
    H5P_genprop_t *prop;                /* Temporary property pointer */
    herr_t         ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(pclass);
    assert(name);
    assert(value);

    /* Find property in list */
    if (NULL == (prop = (H5P_genprop_t *)H5SL_search(pclass->props, name)))
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property doesn't exist");

    /* Check for property size >0 */
    if (0 == prop->size)
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "property has zero size");

    /* Copy the property value */
    H5MM_memcpy(prop->value, value, prop->size);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__class_set() */

/*--------------------------------------------------------------------------
 NAME
    H5P_exist_plist
 PURPOSE
    Internal routine to query the existence of a property in a property list.
 USAGE
    htri_t H5P_exist_plist(plist, name)
        const H5P_genplist_t *plist;  IN: Property list to check
        const char *name;       IN: Name of property to check for
 RETURNS
    Success: Positive if the property exists in the property list, zero
            if the property does not exist.
    Failure: negative value
 DESCRIPTION
        This routine checks if a property exists within a property list.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
htri_t
H5P_exist_plist(const H5P_genplist_t *plist, const char *name)
{
    htri_t ret_value = FAIL; /* return value */

    FUNC_ENTER_NOAPI_NOINIT_NOERR

    assert(plist);
    assert(name);

    /* Check for property in deleted property list */
    if (H5SL_search(plist->del, name) != NULL)
        ret_value = FALSE;
    else {
        /* Check for property in changed property list */
        if (H5SL_search(plist->props, name) != NULL)
            ret_value = TRUE;
        else {
            H5P_genclass_t *tclass; /* Temporary class pointer */

            tclass = plist->pclass;
            while (tclass != NULL) {
                if (H5SL_search(tclass->props, name) != NULL)
                    HGOTO_DONE(TRUE);

                /* Go up to parent class */
                tclass = tclass->parent;
            } /* end while */

            /* If we've reached here, we couldn't find the property */
            ret_value = FALSE;
        } /* end else */
    }     /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_exist_plist() */

/*--------------------------------------------------------------------------
 NAME
    H5P__exist_pclass
 PURPOSE
    Internal routine to query the existence of a property in a property class.
 USAGE
    herr_t H5P__exist_pclass(pclass, name)
        H5P_genclass_t *pclass;  IN: Property class to check
        const char *name;       IN: Name of property to check for
 RETURNS
    Success: Positive if the property exists in the property class, zero
            if the property does not exist.
    Failure: negative value
 DESCRIPTION
        This routine checks if a property exists within a property class.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
htri_t
H5P__exist_pclass(H5P_genclass_t *pclass, const char *name)
{
    htri_t ret_value = FAIL; /* return value */

    FUNC_ENTER_PACKAGE_NOERR

    assert(pclass);
    assert(name);

    /* Check for property in property list */
    if (H5SL_search(pclass->props, name) != NULL)
        ret_value = TRUE;
    else {
        H5P_genclass_t *tclass; /* Temporary class pointer */

        tclass = pclass->parent;
        while (tclass != NULL) {
            if (H5SL_search(tclass->props, name) != NULL)
                HGOTO_DONE(TRUE);

            /* Go up to parent class */
            tclass = tclass->parent;
        } /* end while */

        /* If we've reached here, we couldn't find the property */
        ret_value = FALSE;
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__exist_pclass() */

/*--------------------------------------------------------------------------
 NAME
    H5P__get_size_plist
 PURPOSE
    Internal routine to query the size of a property in a property list.
 USAGE
    herr_t H5P__get_size_plist(plist, name)
        const H5P_genplist_t *plist;  IN: Property list to check
        const char *name;       IN: Name of property to query
        size_t *size;           OUT: Size of property
 RETURNS
    Success: non-negative value
    Failure: negative value
 DESCRIPTION
        This routine retrieves the size of a property's value in bytes.  Zero-
    sized properties are allowed and return a value of 0.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P__get_size_plist(const H5P_genplist_t *plist, const char *name, size_t *size)
{
    H5P_genprop_t *prop;                /* Temporary property pointer */
    herr_t         ret_value = SUCCEED; /* return value */

    FUNC_ENTER_PACKAGE

    assert(plist);
    assert(name);
    assert(size);

    /* Find property */
    if (NULL == (prop = H5P__find_prop_plist(plist, name)))
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property doesn't exist");

    /* Get property size */
    *size = prop->size;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__get_size_plist() */

/*--------------------------------------------------------------------------
 NAME
    H5P__get_size_pclass
 PURPOSE
    Internal routine to query the size of a property in a property class.
 USAGE
    herr_t H5P__get_size_pclass(pclass, name)
        H5P_genclass_t *pclass; IN: Property class to check
        const char *name;       IN: Name of property to query
        size_t *size;           OUT: Size of property
 RETURNS
    Success: non-negative value
    Failure: negative value
 DESCRIPTION
        This routine retrieves the size of a property's value in bytes.  Zero-
    sized properties are allowed and return a value of 0.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P__get_size_pclass(H5P_genclass_t *pclass, const char *name, size_t *size)
{
    H5P_genprop_t *prop;                /* Temporary property pointer */
    herr_t         ret_value = SUCCEED; /* return value */

    FUNC_ENTER_PACKAGE

    assert(pclass);
    assert(name);
    assert(size);

    /* Find property */
    if ((prop = H5P__find_prop_pclass(pclass, name)) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property doesn't exist");

    /* Get property size */
    *size = prop->size;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__get_size_pclass() */

/*--------------------------------------------------------------------------
 NAME
    H5P__get_nprops_plist
 PURPOSE
    Internal routine to query the number of properties in a property list
 USAGE
    herr_t H5P__get_nprops_plist(plist, nprops)
        H5P_genplist_t *plist;  IN: Property list to check
        size_t *nprops;         OUT: Number of properties in the property list
 RETURNS
    Success: non-negative value
    Failure: negative value
 DESCRIPTION
        This routine retrieves the number of a properties in a property list.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P__get_nprops_plist(const H5P_genplist_t *plist, size_t *nprops)
{
    FUNC_ENTER_PACKAGE_NOERR

    assert(plist);
    assert(nprops);

    /* Get property size */
    *nprops = plist->nprops;

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* H5P__get_nprops_plist() */

/*--------------------------------------------------------------------------
 NAME
    H5P_get_nprops_pclass
 PURPOSE
    Internal routine to query the number of properties in a property class
 USAGE
    herr_t H5P_get_nprops_pclass(pclass, nprops)
        H5P_genclass_t *pclass;  IN: Property class to check
        size_t *nprops;         OUT: Number of properties in the property list
        hbool_t recurse;        IN: Include properties in parent class(es) also
 RETURNS
    Success: non-negative value (can't fail)
    Failure: negative value
 DESCRIPTION
    This routine retrieves the number of a properties in a property class.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P_get_nprops_pclass(const H5P_genclass_t *pclass, size_t *nprops, hbool_t recurse)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI_NOERR

    assert(pclass);
    assert(nprops);

    /* Get number of properties */
    *nprops = pclass->nprops;

    /* Check if the class is derived, and walk up the chain, if so */
    if (recurse)
        while (pclass->parent != NULL) {
            pclass = pclass->parent;
            *nprops += pclass->nprops;
        } /* end while */

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_get_nprops_pclass() */

/*--------------------------------------------------------------------------
 NAME
    H5P__cmp_prop
 PURPOSE
    Internal routine to compare two generic properties
 USAGE
    int H5P__cmp_prop(prop1, prop2)
        H5P_genprop_t *prop1;    IN: 1st property to compare
        H5P_genprop_t *prop1;    IN: 2nd property to compare
 RETURNS
    Success: negative if prop1 "less" than prop2, positive if prop1 "greater"
        than prop2, zero if prop1 is "equal" to prop2
    Failure: can't fail
 DESCRIPTION
        This function compares two generic properties together to see if
    they are the same property.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static int
H5P__cmp_prop(const H5P_genprop_t *prop1, const H5P_genprop_t *prop2)
{
    int cmp_value;     /* Value from comparison */
    int ret_value = 0; /* return value */

    FUNC_ENTER_PACKAGE_NOERR

    assert(prop1);
    assert(prop2);

    /* Check the name */
    if ((cmp_value = HDstrcmp(prop1->name, prop2->name)) != 0)
        HGOTO_DONE(cmp_value);

    /* Check the size of properties */
    if (prop1->size < prop2->size)
        HGOTO_DONE(-1);
    if (prop1->size > prop2->size)
        HGOTO_DONE(1);

    /* Check if they both have the same 'create' callback */
    if (prop1->create == NULL && prop2->create != NULL)
        HGOTO_DONE(-1);
    if (prop1->create != NULL && prop2->create == NULL)
        HGOTO_DONE(1);
    if (prop1->create != prop2->create)
        HGOTO_DONE(-1);

    /* Check if they both have the same 'set' callback */
    if (prop1->set == NULL && prop2->set != NULL)
        HGOTO_DONE(-1);
    if (prop1->set != NULL && prop2->set == NULL)
        HGOTO_DONE(1);
    if (prop1->set != prop2->set)
        HGOTO_DONE(-1);

    /* Check if they both have the same 'get' callback */
    if (prop1->get == NULL && prop2->get != NULL)
        HGOTO_DONE(-1);
    if (prop1->get != NULL && prop2->get == NULL)
        HGOTO_DONE(1);
    if (prop1->get != prop2->get)
        HGOTO_DONE(-1);

    /* Check if they both have the same 'encode' callback */
    if (prop1->encode == NULL && prop2->encode != NULL)
        HGOTO_DONE(-1);
    if (prop1->encode != NULL && prop2->encode == NULL)
        HGOTO_DONE(1);
    if (prop1->encode != prop2->encode)
        HGOTO_DONE(-1);

    /* Check if they both have the same 'decode' callback */
    if (prop1->decode == NULL && prop2->decode != NULL)
        HGOTO_DONE(-1);
    if (prop1->decode != NULL && prop2->decode == NULL)
        HGOTO_DONE(1);
    if (prop1->decode != prop2->decode)
        HGOTO_DONE(-1);

    /* Check if they both have the same 'delete' callback */
    if (prop1->del == NULL && prop2->del != NULL)
        HGOTO_DONE(-1);
    if (prop1->del != NULL && prop2->del == NULL)
        HGOTO_DONE(1);
    if (prop1->del != prop2->del)
        HGOTO_DONE(-1);

    /* Check if they both have the same 'copy' callback */
    if (prop1->copy == NULL && prop2->copy != NULL)
        HGOTO_DONE(-1);
    if (prop1->copy != NULL && prop2->copy == NULL)
        HGOTO_DONE(1);
    if (prop1->copy != prop2->copy)
        HGOTO_DONE(-1);

    /* Check if they both have the same 'compare' callback */
    if (prop1->cmp == NULL && prop2->cmp != NULL)
        HGOTO_DONE(-1);
    if (prop1->cmp != NULL && prop2->cmp == NULL)
        HGOTO_DONE(1);
    if (prop1->cmp != prop2->cmp)
        HGOTO_DONE(-1);

    /* Check if they both have the same 'close' callback */
    if (prop1->close == NULL && prop2->close != NULL)
        HGOTO_DONE(-1);
    if (prop1->close != NULL && prop2->close == NULL)
        HGOTO_DONE(1);
    if (prop1->close != prop2->close)
        HGOTO_DONE(-1);

    /* Check if they both have values allocated (or not allocated) */
    if (prop1->value == NULL && prop2->value != NULL)
        HGOTO_DONE(-1);
    if (prop1->value != NULL && prop2->value == NULL)
        HGOTO_DONE(1);
    if (prop1->value != NULL) {
        /* Call comparison routine */
        if ((cmp_value = prop1->cmp(prop1->value, prop2->value, prop1->size)) != 0)
            HGOTO_DONE(cmp_value);
    } /* end if */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__cmp_prop() */

/*--------------------------------------------------------------------------
 NAME
    H5P__cmp_class
 PURPOSE
    Internal routine to compare two generic property classes
 USAGE
    int H5P__cmp_class(pclass1, pclass2)
        H5P_genclass_t *pclass1;    IN: 1st property class to compare
        H5P_genclass_t *pclass2;    IN: 2nd property class to compare
 RETURNS
    Success: negative if class1 "less" than class2, positive if class1 "greater"
        than class2, zero if class1 is "equal" to class2
    Failure: can't fail
 DESCRIPTION
        This function compares two generic property classes together to see if
    they are the same class.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
int
H5P__cmp_class(const H5P_genclass_t *pclass1, const H5P_genclass_t *pclass2)
{
    H5SL_node_t *tnode1, *tnode2; /* Temporary pointer to property nodes */
    int          cmp_value;       /* Value from comparison */
    int          ret_value = 0;   /* Return value */

    FUNC_ENTER_PACKAGE_NOERR

    assert(pclass1);
    assert(pclass2);

    /* Use the revision number to quickly check for identical classes */
    if (pclass1->revision == pclass2->revision)
        HGOTO_DONE(0);

    /* Check the name */
    if ((cmp_value = HDstrcmp(pclass1->name, pclass2->name)) != 0)
        HGOTO_DONE(cmp_value);

    /* Check the number of properties */
    if (pclass1->nprops < pclass2->nprops)
        HGOTO_DONE(-1);
    if (pclass1->nprops > pclass2->nprops)
        HGOTO_DONE(1);

    /* Check the number of property lists created from the class */
    if (pclass1->plists < pclass2->plists)
        HGOTO_DONE(-1);
    if (pclass1->plists > pclass2->plists)
        HGOTO_DONE(1);

    /* Check the number of classes derived from the class */
    if (pclass1->classes < pclass2->classes)
        HGOTO_DONE(-1);
    if (pclass1->classes > pclass2->classes)
        HGOTO_DONE(1);

    /* Check the number of ID references open on the class */
    if (pclass1->ref_count < pclass2->ref_count)
        HGOTO_DONE(-1);
    if (pclass1->ref_count > pclass2->ref_count)
        HGOTO_DONE(1);

    /* Check the property list types */
    if (pclass1->type < pclass2->type)
        HGOTO_DONE(-1);
    if (pclass1->type > pclass2->type)
        HGOTO_DONE(1);

    /* Check whether they are deleted or not */
    if (pclass1->deleted < pclass2->deleted)
        HGOTO_DONE(-1);
    if (pclass1->deleted > pclass2->deleted)
        HGOTO_DONE(1);

    /* Check whether they have creation callback functions & data */
    if (pclass1->create_func == NULL && pclass2->create_func != NULL)
        HGOTO_DONE(-1);
    if (pclass1->create_func != NULL && pclass2->create_func == NULL)
        HGOTO_DONE(1);
    if (pclass1->create_func != pclass2->create_func)
        HGOTO_DONE(-1);
    if (pclass1->create_data < pclass2->create_data)
        HGOTO_DONE(-1);
    if (pclass1->create_data > pclass2->create_data)
        HGOTO_DONE(1);

    /* Check whether they have close callback functions & data */
    if (pclass1->close_func == NULL && pclass2->close_func != NULL)
        HGOTO_DONE(-1);
    if (pclass1->close_func != NULL && pclass2->close_func == NULL)
        HGOTO_DONE(1);
    if (pclass1->close_func != pclass2->close_func)
        HGOTO_DONE(-1);
    if (pclass1->close_data < pclass2->close_data)
        HGOTO_DONE(-1);
    if (pclass1->close_data > pclass2->close_data)
        HGOTO_DONE(1);

    /* Cycle through the properties and compare them also */
    tnode1 = H5SL_first(pclass1->props);
    tnode2 = H5SL_first(pclass2->props);
    while (tnode1 || tnode2) {
        H5P_genprop_t *prop1, *prop2; /* Property for node */

        /* Check if they both have properties in this skip list node */
        if (tnode1 == NULL && tnode2 != NULL)
            HGOTO_DONE(-1);
        if (tnode1 != NULL && tnode2 == NULL)
            HGOTO_DONE(1);

        /* Compare the two properties */
        prop1 = (H5P_genprop_t *)H5SL_item(tnode1);
        prop2 = (H5P_genprop_t *)H5SL_item(tnode2);
        if ((cmp_value = H5P__cmp_prop(prop1, prop2)) != 0)
            HGOTO_DONE(cmp_value);

        /* Advance the pointers */
        tnode1 = H5SL_next(tnode1);
        tnode2 = H5SL_next(tnode2);
    } /* end while */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__cmp_class() */

/*--------------------------------------------------------------------------
 NAME
    H5P__cmp_plist_cb
 PURPOSE
    Internal callback routine when iterating over properties in property list
    to compare them for equality
 USAGE
    int H5P__cmp_plist_cb(prop, udata)
        H5P_genprop_t *prop;        IN: Pointer to the property
        void *udata;                IN/OUT: Pointer to iteration data from user
 RETURNS
    Success: Returns whether to continue (H5_ITER_CONT) or stop (H5_ITER_STOP)
            iterating over the property lists.
    Failure: Negative value (H5_ITER_ERROR)
 DESCRIPTION
    This routine compares a property from one property list (the one being
    iterated over, to a property from the second property list (which is
    looked up).  Iteration is stopped if the comparison is non-equal.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static int
H5P__cmp_plist_cb(H5P_genprop_t *prop, void *_udata)
{
    H5P_plist_cmp_ud_t *udata = (H5P_plist_cmp_ud_t *)_udata; /* Pointer to user data */
    htri_t              prop2_exist; /* Whether the property exists in the second property list */
    int                 ret_value = H5_ITER_CONT; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(prop);
    assert(udata);

    /* Check if the property exists in the second property list */
    if ((prop2_exist = H5P_exist_plist(udata->plist2, prop->name)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, H5_ITER_ERROR, "can't lookup existence of property?");
    if (prop2_exist) {
        const H5P_genprop_t *prop2; /* Pointer to property in second plist */

        /* Look up same property in second property list */
        if (NULL == (prop2 = H5P__find_prop_plist(udata->plist2, prop->name)))
            HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, H5_ITER_ERROR, "property doesn't exist");

        /* Compare the two properties */
        if ((udata->cmp_value = H5P__cmp_prop(prop, prop2)) != 0)
            HGOTO_DONE(H5_ITER_STOP);
    } /* end if */
    else {
        /* Property exists in first list, but not second */
        udata->cmp_value = 1;
        HGOTO_DONE(H5_ITER_STOP);
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P__cmp_plist_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P__cmp_plist
 PURPOSE
    Internal routine to compare two generic property lists
 USAGE
    herr_t H5P__cmp_plist(plist1, plist2, cmp_ret)
        H5P_genplist_t *plist1;    IN: 1st property list to compare
        H5P_genplist_t *plist2;    IN: 2nd property list to compare
        int *cmp_ret;              OUT: Comparison value for two property lists
                                        Negative if list1 "less" than list2,
                                        positive if list1 "greater" than list2,
                                        zero if list1 is "equal" to list2
 RETURNS
    Success: non-negative value
    Failure: negative value
 DESCRIPTION
        This function compares two generic property lists together to see if
    they are equal.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P__cmp_plist(const H5P_genplist_t *plist1, const H5P_genplist_t *plist2, int *cmp_ret)
{
    H5P_plist_cmp_ud_t udata;               /* User data for callback */
    int                idx       = 0;       /* Index of property to begin with */
    herr_t             ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(plist1);
    assert(plist2);
    assert(cmp_ret);

    /* Check the number of properties */
    if (plist1->nprops < plist2->nprops) {
        *cmp_ret = -1;
        HGOTO_DONE(SUCCEED);
    } /* end if */
    if (plist1->nprops > plist2->nprops) {
        *cmp_ret = 1;
        HGOTO_DONE(SUCCEED);
    } /* end if */

    /* Check whether they've been initialized */
    if (plist1->class_init < plist2->class_init) {
        *cmp_ret = -1;
        HGOTO_DONE(SUCCEED);
    } /* end if */
    if (plist1->class_init > plist2->class_init) {
        *cmp_ret = 1;
        HGOTO_DONE(SUCCEED);
    } /* end if */

    /* Set up iterator callback info */
    udata.cmp_value = 0;
    udata.plist2    = plist2;

    /* Iterate over properties in first property list */
    if ((ret_value = H5P__iterate_plist(plist1, TRUE, &idx, H5P__cmp_plist_cb, &udata)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to iterate over list");
    if (ret_value != 0) {
        *cmp_ret = udata.cmp_value;
        HGOTO_DONE(SUCCEED);
    } /* end if */

    /* Check the parent classes */
    if ((*cmp_ret = H5P__cmp_class(plist1->pclass, plist2->pclass)) != 0)
        HGOTO_DONE(SUCCEED);

    /* Property lists must be equal, set comparison value to 0 */
    *cmp_ret = 0;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__cmp_plist() */

/*--------------------------------------------------------------------------
 NAME
    H5P_class_isa
 PURPOSE
    Internal routine to query whether a property class is the same as another
    class.
 USAGE
    htri_t H5P_class_isa(pclass1, pclass2)
        H5P_genclass_t *pclass1;   IN: Property class to check
        H5P_genclass_t *pclass2;   IN: Property class to compare with
 RETURNS
    Success: TRUE (1) or FALSE (0)
    Failure: negative value
 DESCRIPTION
    This routine queries whether a property class is the same as another class,
    and walks up the hierarchy of derived classes, checking if the first class
    is derived from the second class also.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
htri_t
H5P_class_isa(const H5P_genclass_t *pclass1, const H5P_genclass_t *pclass2)
{
    htri_t ret_value = FAIL; /* Return value */

    FUNC_ENTER_NOAPI_NOERR

    assert(pclass1);
    assert(pclass2);

    /* Compare property classes */
    if (H5P__cmp_class(pclass1, pclass2) == 0) {
        HGOTO_DONE(TRUE);
    }
    else {
        /* Check if the class is derived, and walk up the chain, if so */
        if (pclass1->parent != NULL)
            ret_value = H5P_class_isa(pclass1->parent, pclass2);
        else
            HGOTO_DONE(FALSE);
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_class_isa() */

/*--------------------------------------------------------------------------
 NAME
    H5P_isa_class
 PURPOSE
    Internal routine to query whether a property list is a certain class
 USAGE
    hid_t H5P_isa_class(plist_id, pclass_id)
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
    This function is special in that it is an internal library function, but
    accepts hid_t's as parameters.  Since it is used in basically the same way
    as the H5I functions, this should be OK.  Don't make more library functions
    which accept hid_t's without thorough discussion. -QAK
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
htri_t
H5P_isa_class(hid_t plist_id, hid_t pclass_id)
{
    H5P_genplist_t *plist;  /* Property list to query */
    H5P_genclass_t *pclass; /* Property list class */

    htri_t ret_value = FAIL; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check arguments. */
    if (NULL == (plist = (H5P_genplist_t *)H5I_object_verify(plist_id, H5I_GENPROP_LST)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");
    if (NULL == (pclass = (H5P_genclass_t *)H5I_object_verify(pclass_id, H5I_GENPROP_CLS)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property class");

    /* Compare the property list's class against the other class */
    if ((ret_value = H5P_class_isa(plist->pclass, pclass)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, FAIL, "unable to compare property list classes");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_isa_class() */

/*-------------------------------------------------------------------------
 * Function:    H5P_is_default_plist
 *
 * Purpose:     Determine if the provided ID refers to a default property list.
 *
 * Return:      True if the ID refers to a default property list, false otherwise.
 *
 *-------------------------------------------------------------------------
 */
bool
H5P_is_default_plist(hid_t plist_id)
{
    hid_t H5I_def_plists[] = {
        H5P_LST_FILE_CREATE_ID_g,      H5P_LST_FILE_ACCESS_ID_g,      H5P_LST_DATASET_CREATE_ID_g,
        H5P_LST_DATASET_ACCESS_ID_g,   H5P_LST_DATASET_XFER_ID_g,     H5P_LST_FILE_MOUNT_ID_g,
        H5P_LST_GROUP_CREATE_ID_g,     H5P_LST_GROUP_ACCESS_ID_g,     H5P_LST_DATATYPE_CREATE_ID_g,
        H5P_LST_DATATYPE_ACCESS_ID_g,  H5P_LST_MAP_CREATE_ID_g,       H5P_LST_MAP_ACCESS_ID_g,
        H5P_LST_ATTRIBUTE_CREATE_ID_g, H5P_LST_ATTRIBUTE_ACCESS_ID_g, H5P_LST_OBJECT_COPY_ID_g,
        H5P_LST_LINK_CREATE_ID_g,      H5P_LST_LINK_ACCESS_ID_g,      H5P_LST_VOL_INITIALIZE_ID_g,
        H5P_LST_REFERENCE_ACCESS_ID_g};

    size_t num_default_plists = (size_t)(sizeof(H5I_def_plists) / sizeof(H5I_def_plists[0]));

    if (plist_id == H5P_DEFAULT)
        return true;

    for (size_t i = 0; i < num_default_plists; i++) {
        if (plist_id == H5I_def_plists[i])
            return true;
    }

    return false;
}

/*--------------------------------------------------------------------------
 NAME
    H5P_object_verify
 PURPOSE
    Internal routine to query whether a property list is a certain class and
        retrieve the property list object associated with it.
 USAGE
    void *H5P_object_verify(plist_id, pclass_id, allow_default)
        hid_t plist_id;         IN: Property list to query
        hid_t pclass_id;        IN: Property class to query
        bool  allow_default;    IN: Whether to consider the default property lists valid
 RETURNS
    Success: valid pointer to a property list object
    Failure: NULL
 DESCRIPTION
    This routine queries whether a property list is member of a certain class
    and retrieves the property list object associated with it.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
    This function is special in that it is an internal library function, but
    accepts hid_t's as parameters.  Since it is used in basically the same way
    as the H5I functions, this should be OK.  Don't make more library functions
    which accept hid_t's without thorough discussion. -QAK

    This function is similar (in spirit) to H5I_object_verify()
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
H5P_genplist_t *
H5P_object_verify(hid_t plist_id, hid_t pclass_id, bool allow_default)
{
    H5P_genplist_t *ret_value = NULL; /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Compare the property list's class against the other class */
    if (H5P_isa_class(plist_id, pclass_id) != TRUE) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, NULL, "property list is not a member of the class");
    }

    if (!allow_default && H5P_is_default_plist(plist_id)) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOMPARE, NULL, "property list is a default list");
    }

    /* Get the plist structure */
    if (NULL == (ret_value = (H5P_genplist_t *)H5I_object(plist_id))) {
        HGOTO_ERROR(H5E_ID, H5E_BADID, NULL, "can't find object for ID");
    }

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_object_verify() */

/*--------------------------------------------------------------------------
 NAME
    H5P__iterate_plist_cb
 PURPOSE
    Internal callback routine when iterating over properties in property list
 USAGE
    int H5P__iterate_plist_cb(item, key, udata)
        void *item;                 IN: Pointer to the property
        void *key;                  IN: Pointer to the property's name
        void *udata;            IN/OUT: Pointer to iteration data from user
 RETURNS
    Success: Returns the return value of the last call to ITER_FUNC
 DESCRIPTION
    This routine calls the actual callback routine for the property in the
property list.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static int
H5P__iterate_plist_cb(void *_item, void *_key, void *_udata)
{
    H5P_genprop_t       *item      = (H5P_genprop_t *)_item;        /* Pointer to the property */
    char                *key       = (char *)_key;                  /* Pointer to the property's name */
    H5P_iter_plist_ud_t *udata     = (H5P_iter_plist_ud_t *)_udata; /* Pointer to user data */
    int                  ret_value = H5_ITER_CONT;                  /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(item);
    assert(key);

    /* Check if we've found the correctly indexed property */
    if (*udata->curr_idx_ptr >= udata->prev_idx) {
        /* Call the callback function */
        ret_value = (*udata->cb_func)(item, udata->udata);
        if (ret_value != 0)
            HGOTO_DONE(ret_value);
    } /* end if */

    /* Increment the current index */
    (*udata->curr_idx_ptr)++;

    /* Add property name to 'seen' list */
    if (H5SL_insert(udata->seen, key, key) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, H5_ITER_ERROR, "can't insert property into 'seen' skip list");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P__iterate_plist_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P__iterate_plist_pclass_cb
 PURPOSE
    Internal callback routine when iterating over properties in property class
 USAGE
    int H5P__iterate_plist_pclass_cb(item, key, udata)
        void *item;                 IN: Pointer to the property
        void *key;                  IN: Pointer to the property's name
        void *udata;            IN/OUT: Pointer to iteration data from user
 RETURNS
    Success: Returns the return value of the last call to ITER_FUNC
 DESCRIPTION
    This routine verifies that the property hasn't already been seen or was
deleted, and then chains to the property list callback.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static int
H5P__iterate_plist_pclass_cb(void *_item, void *_key, void *_udata)
{
    H5P_genprop_t       *item      = (H5P_genprop_t *)_item;        /* Pointer to the property */
    char                *key       = (char *)_key;                  /* Pointer to the property's name */
    H5P_iter_plist_ud_t *udata     = (H5P_iter_plist_ud_t *)_udata; /* Pointer to user data */
    int                  ret_value = H5_ITER_CONT;                  /* Return value */

    FUNC_ENTER_PACKAGE_NOERR

    /* Sanity check */
    assert(item);
    assert(key);

    /* Only call iterator callback for properties we haven't seen
     * before and that haven't been deleted.
     */
    if (NULL == H5SL_search(udata->seen, key) && NULL == H5SL_search(udata->plist->del, key))
        ret_value = H5P__iterate_plist_cb(item, key, udata);

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P__iterate_plist_pclass_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P__iterate_plist
 PURPOSE
    Internal routine to iterate over the properties in a property list
 USAGE
    int H5P__iterate_plist(plist, iter_all_prop, idx, cb_func, iter_data)
        const H5P_genplist_t *plist; IN: Property list to iterate over
        hbool_t iter_all_prop;      IN: Whether to iterate over all properties
                                        (TRUE), or just non-default (i.e. changed)
                                        properties (FALSE).
        int *idx;                   IN/OUT: Index of the property to begin with
        H5P_iterate_t cb_func;    IN: Function pointer to function to be
                                        called with each property iterated over.
        void *iter_data;            IN/OUT: Pointer to iteration data from user
 RETURNS
    Success: Returns the return value of the last call to ITER_FUNC if it was
                non-zero, or zero if all properties have been processed.
    Failure: negative value
 DESCRIPTION
    This routine iterates over the properties in the property object specified
with PLIST_ID.  For each property in the object, the ITER_DATA and some
additional information, specified below, are passed to the ITER_FUNC function.
The iteration begins with the IDX property in the object and the next element
to be processed by the operator is returned in IDX.  If IDX is NULL, then the
iterator starts at the first property; since no stopping point is returned in
this case, the iterator cannot be restarted if one of the calls to its operator
returns non-zero.

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
H5P__iterate_plist(const H5P_genplist_t *plist, hbool_t iter_all_prop, int *idx, H5P_iterate_int_t cb_func,
                   void *udata)
{
    H5P_genclass_t     *tclass;           /* Temporary class pointer */
    H5P_iter_plist_ud_t udata_int;        /* User data for skip list iterator */
    H5SL_t             *seen      = NULL; /* Skip list to hold names of properties already seen */
    int                 curr_idx  = 0;    /* Current iteration index */
    int                 ret_value = 0;    /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(plist);
    assert(idx);
    assert(cb_func);

    /* Create the skip list to hold names of properties already seen */
    if (NULL == (seen = H5SL_create(H5SL_TYPE_STR, NULL)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, "can't create skip list for seen properties");

    /* Set up iterator callback info */
    udata_int.plist        = plist;
    udata_int.cb_func      = cb_func;
    udata_int.udata        = udata;
    udata_int.seen         = seen;
    udata_int.curr_idx_ptr = &curr_idx;
    udata_int.prev_idx     = *idx;

    /* Iterate over properties in property list proper */
    /* (Will be only the non-default (i.e. changed) properties) */
    ret_value = H5SL_iterate(plist->props, H5P__iterate_plist_cb, &udata_int);
    if (ret_value != 0)
        HGOTO_DONE(ret_value);

    /* Check for iterating over all properties, or just non-default ones */
    if (iter_all_prop) {
        /* Walk up the class hierarchy */
        tclass = plist->pclass;
        while (tclass != NULL) {
            /* Iterate over properties in property list class */
            ret_value = H5SL_iterate(tclass->props, H5P__iterate_plist_pclass_cb, &udata_int);
            if (ret_value != 0)
                HGOTO_DONE(ret_value);

            /* Go up to parent class */
            tclass = tclass->parent;
        } /* end while */
    }     /* end if */

done:
    /* Set the index we stopped at */
    *idx = curr_idx;

    /* Release the skip list of 'seen' properties */
    if (seen != NULL)
        H5SL_close(seen);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__iterate_plist() */

/*--------------------------------------------------------------------------
 NAME
    H5P__iterate_pclass_cb
 PURPOSE
    Internal callback routine when iterating over properties in property list
    class
 USAGE
    int H5P__iterate_pclass_cb(item, key, udata)
        void *item;                 IN: Pointer to the property
        void *key;                  IN: Pointer to the property's name
        void *udata;            IN/OUT: Pointer to iteration data from user
 RETURNS
    Success: Returns the return value of the last call to ITER_FUNC
 DESCRIPTION
    This routine calls the actual callback routine for the property in the
property list class.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static int
H5P__iterate_pclass_cb(void *_item, void H5_ATTR_NDEBUG_UNUSED *_key, void *_udata)
{
    H5P_genprop_t        *item      = (H5P_genprop_t *)_item;         /* Pointer to the property */
    H5P_iter_pclass_ud_t *udata     = (H5P_iter_pclass_ud_t *)_udata; /* Pointer to user data */
    int                   ret_value = 0;                              /* Return value */

    FUNC_ENTER_PACKAGE_NOERR

    /* Sanity check */
    assert(item);
    assert((char *)_key);

    /* Check if we've found the correctly indexed property */
    if (*udata->curr_idx_ptr >= udata->prev_idx) {
        /* Call the callback function */
        ret_value = (*udata->cb_func)(item, udata->udata);
        if (ret_value != 0)
            HGOTO_DONE(ret_value);
    } /* end if */

    /* Increment the current index */
    (*udata->curr_idx_ptr)++;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P__iterate_pclass_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P__iterate_pclass
 PURPOSE
    Internal routine to iterate over the properties in a property class
 USAGE
    herr_t H5P__iterate_pclass(pclass, idx, cb_func, iter_data)
        const H5P_genpclass_t *pclass; IN: Property list class to iterate over
        int *idx;                   IN/OUT: Index of the property to begin with
        H5P_iterate_t cb_func;    IN: Function pointer to function to be
                                        called with each property iterated over.
        void *iter_data;            IN/OUT: Pointer to iteration data from user
 RETURNS
    Success: Returns the return value of the last call to ITER_FUNC if it was
                non-zero, or zero if all properties have been processed.
    Failure: negative value
 DESCRIPTION
    This routine iterates over the properties in the property object specified
with PCLASS_ID.  For each property in the object, the ITER_DATA and some
additional information, specified below, are passed to the ITER_FUNC function.
The iteration begins with the IDX property in the object and the next element
to be processed by the operator is returned in IDX.  If IDX is NULL, then the
iterator starts at the first property; since no stopping point is returned in
this case, the iterator cannot be restarted if one of the calls to its operator
returns non-zero.

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
H5P__iterate_pclass(const H5P_genclass_t *pclass, int *idx, H5P_iterate_int_t cb_func, void *udata)
{
    H5P_iter_pclass_ud_t udata_int;     /* User data for skip list iterator */
    int                  curr_idx  = 0; /* Current iteration index */
    int                  ret_value = 0; /* Return value */

    FUNC_ENTER_PACKAGE_NOERR

    /* Sanity check */
    assert(pclass);
    assert(idx);
    assert(cb_func);

    /* Set up iterator callback info */
    udata_int.cb_func      = cb_func;
    udata_int.udata        = udata;
    udata_int.curr_idx_ptr = &curr_idx;
    udata_int.prev_idx     = *idx;

    /* Iterate over properties in property list class proper */
    ret_value = H5SL_iterate(pclass->props, H5P__iterate_pclass_cb, &udata_int);
    if (ret_value != 0)
        HGOTO_DONE(ret_value);

done:
    /* Set the index we stopped at */
    *idx = curr_idx;

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__iterate_pclass() */

/*--------------------------------------------------------------------------
 NAME
    H5P__peek_cb
 PURPOSE
    Internal callback for H5P__do_prop, to peek at a property's value in a property list.
 USAGE
    herr_t H5P__peek_plist_cb(plist, name, value)
        H5P_genplist_t *plist;  IN: Property list to peek property in
        const char *name;       IN: Name of property to peek
        H5P_genprop_t *prop;    IN: Property to peek
        void *udata;            IN: User data for operation
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Peeks at a new value for a property in a property list.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
    Called when the property is found in the property list and when it's found
        for the property class.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static herr_t
H5P__peek_cb(H5P_genplist_t H5_ATTR_NDEBUG_UNUSED *plist, const char H5_ATTR_NDEBUG_UNUSED *name,
             H5P_genprop_t *prop, void *_udata)
{
    H5P_prop_get_ud_t *udata     = (H5P_prop_get_ud_t *)_udata; /* User data for callback */
    herr_t             ret_value = SUCCEED;                     /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(prop);

    /* Check for property size >0 */
    if (0 == prop->size)
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "property has zero size");

    /* Make a (shallow) copy of the value */
    H5MM_memcpy(udata->value, prop->value, prop->size);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__peek_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P_peek
 PURPOSE
    Internal routine to look at the value of a property in a property list.
 USAGE
    herr_t H5P_peek(plist, name, value)
        H5P_genplist_t *plist;  IN: Property list to check
        const char *name;       IN: Name of property to query
        void *value;            OUT: Pointer to the buffer for the property value
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Retrieves a "shallow" copy of the value for a property in a property
    list.  The property name must exist or this routine will fail.  If there
    is a 'get' callback routine registered for this property, it is _NOT_
    called.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
        This routine may not be called for zero-sized properties and will
    return an error in that case.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P_peek(H5P_genplist_t *plist, const char *name, void *value)
{
    H5P_prop_get_ud_t udata;               /* User data for callback */
    herr_t            ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(value);

    /* Find the property and peek at the value */
    udata.value = value;
    if (H5P__do_prop(plist, name, H5P__peek_cb, H5P__peek_cb, &udata) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTOPERATE, FAIL, "can't operate on plist to peek at value");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_peek() */

/*--------------------------------------------------------------------------
 NAME
    H5P__get_cb
 PURPOSE
    Internal callback for H5P__do_prop, to get a property's value in a property list.
 USAGE
    herr_t H5P__get_plist_cb(plist, name, value)
        H5P_genplist_t *plist;  IN: Property list to get property in
        const char *name;       IN: Name of property to get
        H5P_genprop_t *prop;    IN: Property to get
        void *udata;            IN: User data for operation
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Gets a new value for a property in a property list.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
    Called when the property is found in the property list and when it's found
        for the property class.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static herr_t
H5P__get_cb(H5P_genplist_t *plist, const char *name, H5P_genprop_t *prop, void *_udata)
{
    H5P_prop_get_ud_t *udata     = (H5P_prop_get_ud_t *)_udata; /* User data for callback */
    void              *tmp_value = NULL;                        /* Temporary value for property */
    herr_t             ret_value = SUCCEED;                     /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(prop);

    /* Check for property size >0 */
    if (0 == prop->size)
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "property has zero size");

    /* Call the 'get' callback, if there is one */
    if (NULL != prop->get) {
        /* Make a copy of the current value, in case the callback fails */
        if (NULL == (tmp_value = H5MM_malloc(prop->size)))
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "memory allocation failed temporary property value");
        H5MM_memcpy(tmp_value, prop->value, prop->size);

        /* Call user's callback */
        if ((*(prop->get))(plist->plist_id, name, prop->size, tmp_value) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "can't set property value");

        /* Copy new [possibly unchanged] value into return value */
        H5MM_memcpy(udata->value, tmp_value, prop->size);
    } /* end if */
    /* No 'get' callback, just copy value */
    else
        H5MM_memcpy(udata->value, prop->value, prop->size);

done:
    /* Free the temporary value buffer */
    if (tmp_value)
        H5MM_xfree(tmp_value);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__get_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P_get
 PURPOSE
    Internal routine to query the value of a property in a property list.
 USAGE
    herr_t H5P_get(plist, name, value)
        H5P_genplist_t *plist;  IN: Property list to check
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
H5P_get(H5P_genplist_t *plist, const char *name, void *value)
{
    H5P_prop_get_ud_t udata;               /* User data for callback */
    herr_t            ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(value);

    /* Find the property and get the value */
    udata.value = value;
    if (H5P__do_prop(plist, name, H5P__get_cb, H5P__get_cb, &udata) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTOPERATE, FAIL, "can't operate on plist to get value");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_get() */

/*--------------------------------------------------------------------------
 NAME
    H5P__del_plist_cb
 PURPOSE
    Internal callback for H5P__do_prop, to remove a property's value in a property list.
 USAGE
    herr_t H5P__del_plist_cb(plist, name, value)
        H5P_genplist_t *plist;  IN: Property list to remove property from
        const char *name;       IN: Name of property to remove
        H5P_genprop_t *prop;    IN: Property to remove
        void *udata;            IN: User data for operation
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Remove a property in a property list.  Called when the
    property is found in the property list.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static herr_t
H5P__del_plist_cb(H5P_genplist_t *plist, const char *name, H5P_genprop_t *prop, void H5_ATTR_UNUSED *_udata)
{
    char  *del_name  = NULL;    /* Pointer to deleted name */
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(prop);

    /* Pass value to 'close' callback, if it exists */
    if (NULL != prop->del) {
        /* Call user's callback */
        if ((*(prop->del))(plist->plist_id, name, prop->size, prop->value) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTFREE, FAIL, "can't release property value");
    } /* end if */

    /* Duplicate string for insertion into new deleted property skip list */
    if (NULL == (del_name = H5MM_xstrdup(name)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "memory allocation failed");

    /* Insert property name into deleted list */
    if (H5SL_insert(plist->del, del_name, del_name) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL, "can't insert property into deleted skip list");

    /* Remove the property from the skip list */
    if (NULL == H5SL_remove(plist->props, prop->name))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTDELETE, FAIL, "can't remove property from skip list");

    /* Free the property, ignoring return value, nothing we can do */
    H5P__free_prop(prop);

    /* Decrement the number of properties in list */
    plist->nprops--;

done:
    /* Error cleanup */
    if (ret_value < 0)
        if (del_name)
            H5MM_xfree(del_name);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__del_plist_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P__del_pclass_cb
 PURPOSE
    Internal callback for H5P__do_prop, to remove a property's value in a property list.
 USAGE
    herr_t H5P__del_pclass_cb(plist, name, value)
        H5P_genplist_t *plist;  IN: Property list to remove property from
        const char *name;       IN: Name of property to remove
        H5P_genprop_t *prop;    IN: Property to remove
        void *udata;            IN: User data for operation
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Remove a property in a property list.  Called when the
    property is found in the property class.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static herr_t
H5P__del_pclass_cb(H5P_genplist_t *plist, const char *name, H5P_genprop_t *prop, void H5_ATTR_UNUSED *_udata)
{
    char  *del_name  = NULL;    /* Pointer to deleted name */
    void  *tmp_value = NULL;    /* Temporary value for property */
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(plist);
    assert(name);
    assert(prop);

    /* Pass value to 'del' callback, if it exists */
    if (NULL != prop->del) {
        /* Allocate space for a temporary copy of the property value */
        if (NULL == (tmp_value = H5MM_malloc(prop->size)))
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL,
                        "memory allocation failed for temporary property value");
        H5MM_memcpy(tmp_value, prop->value, prop->size);

        /* Call user's callback */
        if ((*(prop->del))(plist->plist_id, name, prop->size, tmp_value) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "can't close property value");
    } /* end if */

    /* Duplicate string for insertion into new deleted property skip list */
    if (NULL == (del_name = H5MM_xstrdup(name)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "memory allocation failed");

    /* Insert property name into deleted list */
    if (H5SL_insert(plist->del, del_name, del_name) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL, "can't insert property into deleted skip list");

    /* Decrement the number of properties in list */
    plist->nprops--;

done:
    /* Free the temporary value buffer */
    if (tmp_value)
        H5MM_xfree(tmp_value);

    /* Error cleanup */
    if (ret_value < 0)
        if (del_name)
            H5MM_xfree(del_name);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__del_pclass_cb() */

/*--------------------------------------------------------------------------
 NAME
    H5P_remove
 PURPOSE
    Internal routine to remove a property from a property list.
 USAGE
    herr_t H5P_remove(plist, name)
        H5P_genplist_t *plist;  IN: Property list to modify
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
H5P_remove(H5P_genplist_t *plist, const char *name)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    assert(plist);
    assert(name);

    /* Find the property and get the value */
    if (H5P__do_prop(plist, name, H5P__del_plist_cb, H5P__del_pclass_cb, NULL) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTOPERATE, FAIL, "can't operate on plist to remove value");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_remove() */

/*--------------------------------------------------------------------------
 NAME
    H5P__copy_prop_plist
 PURPOSE
    Internal routine to copy a property from one list to another
 USAGE
    herr_t H5P__copy_prop_plist(dst_plist, src_plist, name)
        hid_t dst_id;               IN: ID of destination property list or class
        hid_t src_id;               IN: ID of source property list or class
        const char *name;           IN: Name of property to copy
 RETURNS
    Success: non-negative value.
    Failure: negative value.
 DESCRIPTION
    Copies a property from one property list to another.

    If a property is copied from one list to another, the property will be
    first deleted from the destination list (generating a call to the 'close'
    callback for the property, if one exists) and then the property is copied
    from the source list to the destination list (generating a call to the
    'copy' callback for the property, if one exists).

    If the property does not exist in the destination list, this call is
    equivalent to calling H5Pinsert2 and the 'create' callback will be called
    (if such a callback exists for the property).

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P__copy_prop_plist(hid_t dst_id, hid_t src_id, const char *name)
{
    H5P_genplist_t *dst_plist;           /* Pointer to destination property list */
    H5P_genplist_t *src_plist;           /* Pointer to source property list */
    H5P_genprop_t  *prop;                /* Temporary property pointer */
    H5P_genprop_t  *new_prop  = NULL;    /* Pointer to new property */
    herr_t          ret_value = SUCCEED; /* return value */

    FUNC_ENTER_PACKAGE

    assert(name);

    /* Get the objects to operate on */

    if (NULL == (src_plist = (H5P_genplist_t *)H5I_object(src_id)) ||
        NULL == (dst_plist = (H5P_genplist_t *)H5I_object(dst_id)))
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property object doesn't exist");

    /* If the property exists in the destination already */
    if (NULL != H5P__find_prop_plist(dst_plist, name)) {
        /* Delete the property from the destination list, calling the 'close' callback if necessary */
        if (H5P_remove(dst_plist, name) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTDELETE, FAIL, "unable to remove property");

        /* Get the pointer to the source property */
        prop = H5P__find_prop_plist(src_plist, name);

        /* Make a copy of the source property */
        if ((new_prop = H5P__dup_prop(prop, H5P_PROP_WITHIN_LIST)) == NULL)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");

        /* Call property copy callback, if it exists */
        if (new_prop->copy) {
            if ((new_prop->copy)(new_prop->name, new_prop->size, new_prop->value) < 0)
                HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");
        } /* end if */

        /* Insert the initialized property into the property list */
        if (H5P__add_prop(dst_plist->props, new_prop) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL, "Can't insert property into list");

        /* Increment the number of properties in list */
        dst_plist->nprops++;
    } /* end if */

    /* If not, get the information required to do an H5Pinsert2 with the property into the destination list */
    /* Or using the MT safe functions */
    else {

        /* Get the pointer to the source property */
        if (NULL == (prop = H5P__find_prop_plist(src_plist, name)))
            HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "property doesn't exist");

        /* Create property object from parameters */
        if (NULL ==
            (new_prop = H5P__create_prop(prop->name, prop->size, H5P_PROP_WITHIN_LIST, prop->value,
                                         prop->create, prop->set, prop->get, prop->encode, prop->decode,
                                         prop->del, prop->copy, prop->cmp, prop->close)))
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, "Can't create property");

        /* Call property creation callback, if it exists */
        if (new_prop->create) {
            if ((new_prop->create)(new_prop->name, new_prop->size, new_prop->value) < 0)
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "Can't initialize property");
        } /* end if */

        /* Insert property into property list class */
        if (H5P__add_prop(dst_plist->props, new_prop) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL, "Can't insert property into class");

        /* Increment property count for class */
        dst_plist->nprops++;
    } /* end else */

done:
    /* Cleanup, if necessary */
    if (ret_value < 0) {
        if (new_prop != NULL)
            H5P__free_prop(new_prop);

    } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__copy_prop_plist() */

/*--------------------------------------------------------------------------
 NAME
    H5P__copy_prop_pclass
 PURPOSE
    Internal routine to copy a property from one class to another
 USAGE
    herr_t H5P__copy_prop_pclass(dst_pclass, src_pclass, name)
        H5P_genclass_t	*dst_pclass;    IN: Pointer to destination class
        H5P_genclass_t	*src_pclass;    IN: Pointer to source class
        const char *name;               IN: Name of property to copy
 RETURNS
    Success: non-negative value.
    Failure: negative value.
 DESCRIPTION
    Copies a property from one property class to another.

    If a property is copied from one class to another, all the property
    information will be first deleted from the destination class and then the
    property information will be copied from the source class into the
    destination class.

    If the property does not exist in the destination class or list, this call
    is equivalent to calling H5Pregister2.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P__copy_prop_pclass(hid_t dst_id, hid_t src_id, const char *name)
{
    H5P_genclass_t *src_pclass;      /* Source property class, containing property to copy */
    H5P_genclass_t *dst_pclass;      /* Destination property class */
    H5P_genclass_t *orig_dst_pclass; /* Original destination property class */
    H5P_genprop_t  *prop;            /* Temporary property pointer */

    herr_t ret_value = SUCCEED; /* return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    assert(name);

    /* Get property list classes */
    if (NULL == (src_pclass = (H5P_genclass_t *)H5I_object(src_id)))
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "source property class object doesn't exist");
    if (NULL == (dst_pclass = (H5P_genclass_t *)H5I_object(dst_id)))
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "destination property class object doesn't exist");

    /* Get the property from the source */
    if (NULL == (prop = H5P__find_prop_pclass(src_pclass, name)))
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "unable to locate property");

    /* If the property exists in the destination already */
    if (H5P__exist_pclass(dst_pclass, name)) {
        /* Delete the old property from the destination class */
        if (H5P__unregister(dst_pclass, name) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTDELETE, FAIL, "unable to remove property");
    } /* end if */

    /* Register the property into the destination */
    orig_dst_pclass = dst_pclass;
    if (H5P__register(&dst_pclass, name, prop->size, prop->value, prop->create, prop->set, prop->get,
                      prop->encode, prop->decode, prop->del, prop->copy, prop->cmp, prop->close) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTDELETE, FAIL, "unable to remove property");

    /* Check if the property class changed and needs to be substituted in the ID */
    if (dst_pclass != orig_dst_pclass) {
        H5P_genclass_t *old_dst_pclass; /* Old destination property class */

        /* Substitute the new destination property class in the ID */
        if (NULL == (old_dst_pclass = (H5P_genclass_t *)H5I_subst(dst_id, dst_pclass)))
            HGOTO_ERROR(H5E_PLIST, H5E_CANTSET, FAIL, "unable to substitute property class in ID");
        assert(old_dst_pclass == orig_dst_pclass);

        /* Close the previous class */
        if (H5P__close_class(old_dst_pclass) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCLOSEOBJ, FAIL,
                        "unable to close original property class after substitution");
    } /* end if */

done:
    /* Cleanup, if necessary */

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__copy_prop_pclass() */

#ifdef H5_HAVE_MULTITHREAD

/**
 * NOTE: There is not a multithread safe version of this functions because
 * the purpose of this function is handled else where in the multithread
 * safe functions.
 */

#else
/*--------------------------------------------------------------------------
 NAME
    H5P__unregister
 PURPOSE
    Internal routine to remove a property from a property list class.
 USAGE
    herr_t H5P__unregister(pclass, name)
        H5P_genclass_t *pclass; IN: Property list class to modify
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
H5P__unregister(H5P_genclass_t *pclass, const char *name)
{
    H5P_genprop_t *prop;                /* Temporary property pointer */
    herr_t         ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(pclass);
    assert(name);

    /* Get the property node from the skip list */
    if ((prop = (H5P_genprop_t *)H5SL_search(pclass->props, name)) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "can't find property in skip list");

    /* Remove the property from the skip list */
    if (H5SL_remove(pclass->props, prop->name) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTDELETE, FAIL, "can't remove property from skip list");

    /* Free the property, ignoring return value, nothing we can do */
    H5P__free_prop(prop);

    /* Decrement the number of registered properties in class */
    pclass->nprops--;

    /* Update the revision for the class */
    pclass->revision = H5P_GET_NEXT_REV;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__unregister() */
#endif

/*--------------------------------------------------------------------------
 NAME
    H5P_close
 PURPOSE
    Internal routine to close a property list.
 USAGE
    herr_t H5P_close(plist)
        H5P_genplist_t *plist;  IN: Property list to close
 RETURNS
    Returns non-negative on success, negative on failure.
 DESCRIPTION
        Closes a property list.  If a 'close' callback exists for the property
    list class, it is called before the property list is destroyed.  If 'close'
    callbacks exist for any individual properties in the property list, they are
    called after the class 'close' callback.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
        The property list class 'close' callback routine is not called from
    here, it must have been checked for and called properly prior to this routine
    being called.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5P_close(H5P_genplist_t *plist)
{
    H5P_genclass_t *tclass;              /* Temporary class pointer */
    H5SL_t         *seen = NULL;         /* Skip list to hold names of properties already seen */
    size_t          nseen;               /* Number of items 'seen' */
    hbool_t         has_parent_class;    /* Flag to indicate that this property list's class has a parent */
    size_t          ndel;                /* Number of items deleted */
    H5SL_node_t    *curr_node;           /* Current node in skip list */
    H5P_genprop_t  *tmp;                 /* Temporary pointer to properties */
    unsigned        make_cb   = 0;       /* Operator data for property free callback */
    herr_t          ret_value = SUCCEED; /* return value */

    FUNC_ENTER_NOAPI_NOINIT

    assert(plist);

    /* Make call to property list class close callback, if needed
     * (up through chain of parent classes also)
     */
    if (plist->class_init) {
        tclass = plist->pclass;
        while (NULL != tclass) {
            if (NULL != tclass->close_func) {
                /* Call user's "close" callback function, ignoring return value */
                (tclass->close_func)(plist->plist_id, tclass->close_data);
            } /* end if */

            /* Go up to parent class */
            tclass = tclass->parent;
        } /* end while */
    }     /* end if */

    /* Create the skip list to hold names of properties already seen
     * (This prevents a property in the class hierarchy from having it's
     * 'close' callback called, if a property in the class hierarchy has
     * already been seen)
     */
    if ((seen = H5SL_create(H5SL_TYPE_STR, NULL)) == NULL)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, "can't create skip list for seen properties");
    nseen = 0;

    /* Walk through the changed properties in the list */
    if (H5SL_count(plist->props) > 0) {
        curr_node = H5SL_first(plist->props);
        while (curr_node != NULL) {
            /* Get pointer to property from node */
            tmp = (H5P_genprop_t *)H5SL_item(curr_node);

            /* Call property close callback, if it exists */
            if (tmp->close) {
                /* Call the 'close' callback */
                (tmp->close)(tmp->name, tmp->size, tmp->value);
            } /* end if */

            /* Add property name to "seen" list */
            if (H5SL_insert(seen, tmp->name, tmp->name) < 0)
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL, "can't insert property into seen skip list");
            nseen++;

            /* Get the next property node in the skip list */
            curr_node = H5SL_next(curr_node);
        } /* end while */
    }     /* end if */

    /* Determine number of deleted items from property list */
    ndel = H5SL_count(plist->del);

    /*
     * Check if we should remove class properties (up through list of parent classes also),
     * initialize each with default value & make property 'remove' callback.
     */
    tclass           = plist->pclass;
    has_parent_class = (hbool_t)(tclass != NULL && tclass->parent != NULL && tclass->parent->nprops > 0);
    while (tclass != NULL) {
        if (tclass->nprops > 0) {
            /* Walk through the properties in the class */
            curr_node = H5SL_first(tclass->props);
            while (curr_node != NULL) {
                /* Get pointer to property from node */
                tmp = (H5P_genprop_t *)H5SL_item(curr_node);

                /* Only "delete" properties we haven't seen before
                 * and that haven't already been deleted
                 */
                if ((nseen == 0 || H5SL_search(seen, tmp->name) == NULL) &&
                    (ndel == 0 || H5SL_search(plist->del, tmp->name) == NULL)) {

                    /* Call property close callback, if it exists */
                    if (tmp->close) {
                        void *tmp_value; /* Temporary value buffer */

                        /* Allocate space for a temporary copy of the property value */
                        if (NULL == (tmp_value = H5MM_malloc(tmp->size)))
                            HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL,
                                        "memory allocation failed for temporary property value");
                        H5MM_memcpy(tmp_value, tmp->value, tmp->size);

                        /* Call the 'close' callback */
                        (tmp->close)(tmp->name, tmp->size, tmp_value);

                        /* Release the temporary value buffer */
                        H5MM_xfree(tmp_value);
                    } /* end if */

                    /* Add property name to "seen" list, if we have other classes to work on */
                    if (has_parent_class) {
                        if (H5SL_insert(seen, tmp->name, tmp->name) < 0)
                            HGOTO_ERROR(H5E_PLIST, H5E_CANTINSERT, FAIL,
                                        "can't insert property into seen skip list");
                        nseen++;
                    } /* end if */
                }     /* end if */

                /* Get the next property node in the skip list */
                curr_node = H5SL_next(curr_node);
            } /* end while */
        }     /* end if */

        /* Go up to parent class */
        tclass = tclass->parent;
    } /* end while */

    /* Decrement class's dependent property list value! */
    if (H5P__access_class(plist->pclass, H5P_MOD_DEC_LST) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "Can't decrement class ref count");

    /* Free the list of 'seen' properties */
    H5SL_close(seen);
    seen = NULL;

    /* Free the list of deleted property names */
    H5SL_destroy(plist->del, H5P__free_del_name_cb, NULL);

    /* Free the properties */
    H5SL_destroy(plist->props, H5P__free_prop_cb, &make_cb);

    /* Destroy property list object */
    plist = H5FL_FREE(H5P_genplist_t, plist);

done:
    /* Release the skip list of 'seen' properties */
    if (seen != NULL)
        H5SL_close(seen);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_close() */

/*--------------------------------------------------------------------------
 NAME
    H5P_get_class_name
 PURPOSE
    Internal routine to query the name of a generic property list class
 USAGE
    char *H5P_get_class_name(pclass)
        H5P_genclass_t *pclass;    IN: Property list class to check
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
H5P_get_class_name(H5P_genclass_t *pclass)
{
    char *ret_value = NULL; /* Return value */

    FUNC_ENTER_NOAPI_NOERR

    assert(pclass);

    /* Get class name */
    ret_value = H5MM_xstrdup(pclass->name);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P_get_class_name() */

/*--------------------------------------------------------------------------
 NAME
    H5P__get_class_path
 PURPOSE
    Internal routine to query the full path of a generic property list class
 USAGE
    char *H5P__get_class_name(pclass)
        H5P_genclass_t *pclass;    IN: Property list class to check
 RETURNS
    Success: Pointer to a malloc'ed string containing the full path of class
    Failure: NULL
 DESCRIPTION
        This routine retrieves the full path name of a generic property list
    class, starting with the root of the class hierarchy.
    The pointer to the name must be free'd by the user for successful calls.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
char *
H5P__get_class_path(H5P_genclass_t *pclass)
{
    char *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(pclass);

    /* Recursively build the full path */
    if (pclass->parent != NULL) {
        char *par_path; /* Parent class's full path */

        /* Get the parent class's path */
        par_path = H5P__get_class_path(pclass->parent);
        if (par_path != NULL) {
            size_t ret_str_len;

            /* Allocate enough space for the parent class's path, plus the '/'
             * separator, this class's name and the string terminator
             */
            ret_str_len = HDstrlen(par_path) + HDstrlen(pclass->name) + 1 +
                          3; /* Extra "+3" to quiet GCC warning - 2019/07/05, QAK */
            if (NULL == (ret_value = (char *)H5MM_malloc(ret_str_len)))
                HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed for class name");

            /* Build the full path for this class */
            HDsnprintf(ret_value, ret_str_len, "%s/%s", par_path, pclass->name);

            /* Free the parent class's path */
            H5MM_xfree(par_path);
        } /* end if */
        else
            ret_value = H5MM_xstrdup(pclass->name);
    } /* end if */
    else
        ret_value = H5MM_xstrdup(pclass->name);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__get_class_path() */

/*--------------------------------------------------------------------------
 NAME
    H5P__open_class_path
 PURPOSE
    Internal routine to open [a copy of] a class with its full path name
 USAGE
    H5P_genclass_t *H5P__open_class_path(path)
        const char *path;       IN: Full path name of class to open [copy of]
 RETURNS
    Success: Pointer to a generic property class object
    Failure: NULL
 DESCRIPTION
    This routine opens [a copy] of the class indicated by the full path.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
H5P_genclass_t *
H5P__open_class_path(const char *path)
{
    char             *tmp_path = NULL;  /* Temporary copy of the path */
    char             *curr_name;        /* Pointer to current component of path name */
    char             *delimit;          /* Pointer to path delimiter during traversal */
    H5P_genclass_t   *curr_class;       /* Pointer to class during path traversal */
    H5P_check_class_t check_info;       /* Structure to hold the information for checking duplicate names */
    H5P_genclass_t   *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(path);

    /* Duplicate the path to use */
    tmp_path = H5MM_xstrdup(path);
    assert(tmp_path);

    /* Find the generic property class with this full path */
    curr_name  = tmp_path;
    curr_class = NULL;
    while (NULL != (delimit = HDstrchr(curr_name, '/'))) {
        /* Change the delimiter to terminate the string */
        *delimit = '\0';

        /* Set up the search structure */
        check_info.parent    = curr_class;
        check_info.name      = curr_name;
        check_info.new_class = NULL;

        /* Find the class with this name & parent by iterating over the open classes */
        if (H5I_iterate(H5I_GENPROP_CLS, H5P__open_class_path_cb, &check_info, FALSE) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADITER, NULL, "can't iterate over classes");
        }
        else if (NULL == check_info.new_class) {
            HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, NULL, "can't locate class");
        }

        /* Advance the pointer in the path to the start of the next component */
        curr_class = check_info.new_class;
        curr_name  = delimit + 1;
    } /* end while */

    /* Should be pointing to the last component in the path name now... */

    /* Set up the search structure */
    check_info.parent    = curr_class;
    check_info.name      = curr_name;
    check_info.new_class = NULL;

    /* Find the class with this name & parent by iterating over the open classes */
    if (H5I_iterate(H5I_GENPROP_CLS, H5P__open_class_path_cb, &check_info, FALSE) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADITER, NULL, "can't iterate over classes");
    }
    else if (NULL == check_info.new_class) {
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, NULL, "can't locate class");
    }

    /* Copy it */
    if (NULL == (ret_value = H5P__copy_pclass(check_info.new_class))) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, NULL, "can't copy property class");
    }

done:
    /* Free the duplicated path */
    H5MM_xfree(tmp_path);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__open_class_path() */

/*--------------------------------------------------------------------------
 NAME
    H5P__get_class_parent
 PURPOSE
    Internal routine to query the parent class of a generic property class
 USAGE
    H5P_genclass_t *H5P__get_class_parent(pclass)
        H5P_genclass_t *pclass;    IN: Property class to check
 RETURNS
    Success: Pointer to the parent class of a property class
    Failure: NULL
 DESCRIPTION
    This routine retrieves a pointer to the parent class for a property class.

 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
H5P_genclass_t *
H5P__get_class_parent(const H5P_genclass_t *pclass)
{
    H5P_genclass_t *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE_NOERR

    assert(pclass);

    /* Get property size */
    ret_value = pclass->parent;

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__get_class_parent() */

/*--------------------------------------------------------------------------
 NAME
    H5P__close_class
 PURPOSE
    Internal routine to close a property list class.
 USAGE
    herr_t H5P__close_class(class)
        H5P_genclass_t *class;  IN: Property list class to close
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
H5P__close_class(H5P_genclass_t *pclass)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI_NOINIT

    assert(pclass);

    /* Decrement the reference count & check if the object should go away */
    if (H5P__access_class(pclass, H5P_MOD_DEC_REF) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_NOTFOUND, FAIL, "can't decrement ID ref count");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5P__close_class() */

/*-------------------------------------------------------------------------
 * Function:       H5P__new_plist_of_type
 *
 * Purpose:        Create a new property list, of a given type
 *
 * Return:	   Success:	ID of new property list
 *		   Failure:	H5I_INVALID_HID
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5P__new_plist_of_type(H5P_plist_type_t type)
{
    H5P_genclass_t *pclass;                      /* Class of property list to create */
    hid_t           class_id;                    /* ID of class to create */
    hid_t           ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity checks */
    HDcompile_assert(H5P_TYPE_REFERENCE_ACCESS == (H5P_TYPE_MAX_TYPE - 1));
    assert(type >= H5P_TYPE_USER && type <= H5P_TYPE_REFERENCE_ACCESS);

    /* Check arguments */
    if (type == H5P_TYPE_USER)
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, H5I_INVALID_HID, "can't create user property list");
    if (type == H5P_TYPE_ROOT)
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, H5I_INVALID_HID,
                    "shouldn't be creating root class property list");

    /* Instantiate a property list of the proper type */
    switch (type) {
        case H5P_TYPE_OBJECT_CREATE:
            class_id = H5P_CLS_OBJECT_CREATE_ID_g;
            break;

        case H5P_TYPE_FILE_CREATE:
            class_id = H5P_CLS_FILE_CREATE_ID_g;
            break;

        case H5P_TYPE_FILE_ACCESS:
            class_id = H5P_CLS_FILE_ACCESS_ID_g;
            break;

        case H5P_TYPE_DATASET_CREATE:
            class_id = H5P_CLS_DATASET_CREATE_ID_g;
            break;

        case H5P_TYPE_DATASET_ACCESS:
            class_id = H5P_CLS_DATASET_ACCESS_ID_g;
            break;

        case H5P_TYPE_DATASET_XFER:
            class_id = H5P_CLS_DATASET_XFER_ID_g;
            break;

        case H5P_TYPE_FILE_MOUNT:
            class_id = H5P_CLS_FILE_MOUNT_ID_g;
            break;

        case H5P_TYPE_GROUP_CREATE:
            class_id = H5P_CLS_GROUP_CREATE_ID_g;
            break;

        case H5P_TYPE_GROUP_ACCESS:
            class_id = H5P_CLS_GROUP_ACCESS_ID_g;
            break;

        case H5P_TYPE_DATATYPE_CREATE:
            class_id = H5P_CLS_DATATYPE_CREATE_ID_g;
            break;

        case H5P_TYPE_DATATYPE_ACCESS:
            class_id = H5P_CLS_DATATYPE_ACCESS_ID_g;
            break;

        case H5P_TYPE_MAP_CREATE:
            class_id = H5P_CLS_MAP_CREATE_ID_g;
            break;

        case H5P_TYPE_MAP_ACCESS:
            class_id = H5P_CLS_MAP_ACCESS_ID_g;
            break;

        case H5P_TYPE_STRING_CREATE:
            class_id = H5P_CLS_STRING_CREATE_ID_g;
            break;

        case H5P_TYPE_ATTRIBUTE_CREATE:
            class_id = H5P_CLS_ATTRIBUTE_CREATE_ID_g;
            break;

        case H5P_TYPE_ATTRIBUTE_ACCESS:
            class_id = H5P_CLS_ATTRIBUTE_ACCESS_ID_g;
            break;

        case H5P_TYPE_OBJECT_COPY:
            class_id = H5P_CLS_OBJECT_COPY_ID_g;
            break;

        case H5P_TYPE_LINK_CREATE:
            class_id = H5P_CLS_LINK_CREATE_ID_g;
            break;

        case H5P_TYPE_LINK_ACCESS:
            class_id = H5P_CLS_LINK_ACCESS_ID_g;
            break;

        case H5P_TYPE_VOL_INITIALIZE:
            class_id = H5P_CLS_VOL_INITIALIZE_ID_g;
            break;

        case H5P_TYPE_REFERENCE_ACCESS:
            class_id = H5P_CLS_REFERENCE_ACCESS_ID_g;
            break;

        case H5P_TYPE_USER: /* shut compiler warnings up */
        case H5P_TYPE_ROOT:
        case H5P_TYPE_MAX_TYPE:
        default:
            HGOTO_ERROR(H5E_PLIST, H5E_BADRANGE, FAIL, "invalid property list type: %u\n", (unsigned)type);
    } /* end switch */

    /* Get the class object */
    if (NULL == (pclass = (H5P_genclass_t *)H5I_object(class_id)))
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, H5I_INVALID_HID, "not a property class");

    /* Create the new property list */
    if ((ret_value = H5P_create_id(pclass, TRUE)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, H5I_INVALID_HID, "unable to create property list");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5P__new_plist_of_type() */

/*-------------------------------------------------------------------------
 * Function:	H5P_get_plist_id
 *
 * Purpose:	Quick and dirty routine to retrieve property list ID from
 *		property list structure.
 *          (Mainly added to stop non-file routines from poking about in the
 *          H5P_genplist_t data structure)
 *
 * Return:      Success:        Non-negative ID of property list.
 *              Failure:        H5I_INVALID_HID
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5P_get_plist_id(const H5P_genplist_t *plist)
{
    /* Use FUNC_ENTER_NOAPI_NOINIT_NOERR here to avoid performance issues */
    FUNC_ENTER_NOAPI_NOINIT_NOERR

    assert(plist);

    FUNC_LEAVE_NOAPI(plist->plist_id)
} /* end H5P_get_plist_id() */

/*-------------------------------------------------------------------------
 * Function:	H5P_get_class
 *
 * Purpose:	Quick and dirty routine to retrieve property list class from
 *		property list structure.
 *          (Mainly added to stop non-file routines from poking about in the
 *          H5P_genplist_t data structure)
 *
 * Return:      Success:        Non-NULL class of property list.
 *              Failure:        NULL
 *
 *-------------------------------------------------------------------------
 */
H5P_genclass_t *
H5P_get_class(const H5P_genplist_t *plist)
{
    /* Use FUNC_ENTER_NOAPI_NOINIT_NOERR here to avoid performance issues */
    FUNC_ENTER_NOAPI_NOINIT_NOERR

    assert(plist);

    FUNC_LEAVE_NOAPI(plist->pclass)
} /* end H5P_get_class() */

/*-------------------------------------------------------------------------
 * Function:       H5P_ignore_cmp
 *
 * Purpose:        Callback routine to ignore comparing property values.
 *
 * Return:         zero
 *
 *-------------------------------------------------------------------------
 */
int
H5P_ignore_cmp(const void H5_ATTR_UNUSED *val1, const void H5_ATTR_UNUSED *val2, size_t H5_ATTR_UNUSED size)
{
    FUNC_ENTER_NOAPI_NOINIT_NOERR

    FUNC_LEAVE_NOAPI(0)
} /* end H5P_ignore_cmp() */

#endif /* H5_HAVE_MULTITHREAD */
