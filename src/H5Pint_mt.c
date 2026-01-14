/*
 * Purpose: Multi-Thread Safe Generic Property Functions
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
#include "H5Ppkg_mt.h"

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

/********************/
/* Package Typedefs */
/********************/

/********************/
/* Local Prototypes */
/********************/

/*********************/
/* Package Variables */
/*********************/

H5P_mt_t H5P_mt_g;

H5P_mt_class_t *H5P_MT_CLS_ROOT_g = NULL;

H5P_mt_class_t *H5P_MT_CLS_ATTRIBUTE_ACCESS_g = NULL;
H5P_mt_class_t *H5P_MT_CLS_ATTRIBUTE_CREATE_g = NULL;
H5P_mt_class_t *H5P_MT_CLS_DATASET_ACCESS_g   = NULL;
H5P_mt_class_t *H5P_MT_CLS_DATASET_CREATE_g   = NULL;
H5P_mt_class_t *H5P_MT_CLS_DATASET_XFER_g     = NULL;
H5P_mt_class_t *H5P_MT_CLS_DATATYPE_ACCESS_g  = NULL;
H5P_mt_class_t *H5P_MT_CLS_DATATYPE_CREATE_g  = NULL;
H5P_mt_class_t *H5P_MT_CLS_FILE_ACCESS_g      = NULL;
H5P_mt_class_t *H5P_MT_CLS_FILE_CREATE_g      = NULL;
H5P_mt_class_t *H5P_MT_CLS_FILE_MOUNT_g       = NULL;
H5P_mt_class_t *H5P_MT_CLS_GROUP_ACCESS_g     = NULL;
H5P_mt_class_t *H5P_MT_CLS_GROUP_CREATE_g     = NULL;
H5P_mt_class_t *H5P_MT_CLS_LINK_ACCESS_g      = NULL;
H5P_mt_class_t *H5P_MT_CLS_LINK_CREATE_g      = NULL;
H5P_mt_class_t *H5P_MT_CLS_MAP_ACCESS_g       = NULL;
H5P_mt_class_t *H5P_MT_CLS_MAP_CREATE_g       = NULL;
H5P_mt_class_t *H5P_MT_CLS_OBJECT_COPY_g      = NULL;
H5P_mt_class_t *H5P_MT_CLS_OBJECT_CREATE_g    = NULL;
H5P_mt_class_t *H5P_MT_CLS_REFERENCE_ACCESS_g = NULL;
H5P_mt_class_t *H5P_MT_CLS_STRING_CREATE_g    = NULL;
H5P_mt_class_t *H5P_MT_CLS_VOL_INITIALIZE_g   = NULL;

/*****************************/
/* Library Private Variables */
/*****************************/

/*******************/
/* Local Variables */
/*******************/

/****************************************************************************************
 * Function:    H5P_mt_init_free_lists
 *
 * Purpose:     Initializes the H5P_mt_g global struct which contains the free lists for
 *              classes, lists, and properties, and global stats for H5P.
 *
 *              NOTE: Once initialized, the free lists will always contain at least one
 *              entry of H5P_mt_prop_apt_t for the prop free list, H5P_mt_class_sptr_t
 *              for the class free list, and one H5P_mt_list_sptr_t for the list free
 *              list. Until an object is closed and added to their respective free list
 *              the H5P_mt_prop_apt_t's, H5P_mt_class_sptr_t's, and H5P_mt_list_sptr_t's
 *              ptr field will be set to NULL.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P_mt_init_free_lists(void)
{
    H5P_mt_prop_aptr_t  fl_prop;
    H5P_mt_list_sptr_t  fl_list;
    H5P_mt_class_sptr_t fl_class;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    /* Initialize a property free list node */
    fl_prop.ptr          = NULL;
    fl_prop.deleted      = FALSE;
    fl_prop.dummy_bool_1 = FALSE;
    fl_prop.dummy_bool_2 = FALSE;
    fl_prop.dummy_bool_3 = FALSE;

    fl_class.ptr = NULL;
    fl_class.sn  = 0;

    fl_list.ptr = NULL;
    fl_list.sn  = 0;

    atomic_init(&(H5P_mt_g.prop_fl_head), fl_prop);
    atomic_init(&(H5P_mt_g.prop_fl_tail), fl_prop);
    atomic_init(&(H5P_mt_g.prop_fl_len), 0ULL);
    atomic_init(&(H5P_mt_g.prop_max_desired_fl_len), H5P__MAX_PROP_FL_LEN);

    atomic_init(&(H5P_mt_g.list_fl_head), fl_list);
    atomic_init(&(H5P_mt_g.list_fl_tail), fl_list);
    atomic_init(&(H5P_mt_g.list_fl_len), 0ULL);
    atomic_init(&(H5P_mt_g.class_max_desired_fl_len), H5P__MAX_CLASS_FL_LEN);

    atomic_init(&(H5P_mt_g.class_fl_head), fl_class);
    atomic_init(&(H5P_mt_g.class_fl_tail), fl_class);
    atomic_init(&(H5P_mt_g.class_fl_len), 0ULL);
    atomic_init(&(H5P_mt_g.list_max_desired_fl_len), H5P__MAX_LIST_FL_LEN);

    H5P__init_stats_global();

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_mt_init_free_lists() */

/****************************************************************************************
 * Function:    H5P__mt_create_class
 *
 * Purpose:     Function to create a new property list class (H5P_mt_class_t)
 *
 *              NOTE: property list classes are referred to as classes and property lists
 *              are referred to as lists.
 *
 *              A multi-thread safe function to create a new class, derived from an
 *              existing class. The new class copies the properties from it's parent
 *              class that are valid for the version of the parent class the new class is
 *              derived from.
 *
 *              NOTE: A specific version of the parent class to be derived from can be
 *              selected via the src_version parameter. If wanting the most current
 *              version set src_version = 0.
 *
 * Details:     NOTE: for more information on the H5P_mt_class_t structure or specific
 *              fields, check the detailed comment for the structure in H5Ppkg_mt.h
 *
 *              To ensure multi-thread safety the first step is to check the parent
 *              class's thrd field to ensure the parent class is not in the process
 *              opening or closing. If opening is TRUE the function loops checking again.
 *              If opening, the class will only be briefly visible to other threads
 *              before completing the opening process, so this thread should see that
 *              opening's been set to FALSE without waiting long (stats are collected so
 *              if this proves incorrect, it can be found quickly and fixed). If closing
 *              is true, an error is thrown. If neither are true, the count field in the
 *              parent class's thrd field is incremented to show another thread is
 *              accessing the structure.
 *
 *              The parent class increments it's ref_count->plc field to count the new
 *              derived class, this and incrementing the parent's thrd count prevent the
 *              parent class from being deleted out from under this derived class.
 *
 *              Next, memory is allocated for the new H5P_mt_class_t, by calling
 *              H5P__mt_alloc_class(), and the fields are initialized. The class's name
 *              and type are set to the provided name and type parameters.
 *
 *              The new class's pl_head is initialized to the property (H5P_mt_prop_t)
 *              that is returned by the function H5P__create_sentinels(), which returns
 *              the negative sentinel with it's next pointer pointing to the positive
 *              sentinel, and any new or modified properties will be inserted between
 *              them.
 *
 *              The parent class's LFSLL is then iterated to find and create copies of
 *              the valid properties, using the function H5P__mt_copy_lfsll().
 *
 *              The thrd.opening field of the new class is initialized TRUE, to prevent
 *              other threads from accessing the structure until it's completely set up.
 *
 *              NOTE: This class will be visible to other threads after H5I_register()
 *              is called to get an ID for this class. After that, any finishing set
 *              up is completed and thrd.opening is set to FALSE (all done in the
 *              function that calls this one).
 *
 *              The provided callback functions and data are copied into the class.
 *
 *              H5P__init_stats_class() is called to initialize the stats fields of
 *              the H5P_mt_class_t structure to collect stats for testing purposes.
 *
 *              Finally, the thrd.count fields are decremented for the parent class.
 *
 *
 * Return:      Success: Returns a pointer to the new H5P_mt_class_t struct.
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_class_t *
H5P__mt_create_class(H5P_mt_class_t *parent, const char *name, H5P_plist_type_t type, uint64_t src_version,
                     H5P_cls_create_func_t create_func, void *create_data, H5P_cls_copy_func_t copy_func,
                     void *copy_data, H5P_cls_close_func_t close_func, void *close_data)
{
    H5P_mt_class_t              *new_class = NULL;      /* New class to be created */
    hid_t                        parent_id;             /* ID of the parent class */
    H5P_mt_class_ref_counts_t    ref_count;             /* ref_count for new class */
    H5P_mt_active_thread_count_t thrd;                  /* thrd struct for new class */
    H5P_mt_class_sptr_t          fl_next;               /* new class's free list struct */
    uint64_t                     parent_version = 0;    /* Parent's version to derive */
    size_t                       phys_pl_len;           /* new class's LFSLL physical length */
    size_t                       log_pl_len;            /* new class's LFSLL logical length */
    bool                         inc_thrd_flag = FALSE; /* Flag to dec parent's thrd count */

    H5P_mt_class_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__mt_create_class__num_calls), 1);

    /**
     * If parent is NULL then this is the root class, and there isn't a parent class to
     * increment. Otherwise, increment the thrd count in the parent class.
     */
    if (parent != NULL) {
        assert((atomic_load(&(parent->tag))) == H5P_MT_CLASS_TAG);

        parent_version = atomic_load(&(parent->curr_version));

        /* Increment parent's thrd count */
        if (H5P__inc_thrd_count(parent) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Couldn't increment parent's thread count.");
        }

        inc_thrd_flag = TRUE;
    }

    /**
     * NOTE: This is for testing to choose a specific version to create the class at to
     * ensure only the valid properties for that version are copied over
     */
    if (src_version > 0) {
        /* If src_version is larger than any version of the parent, throw an error */
        if (src_version > parent_version) {
            assert(src_version <= parent_version);
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL,
                        "src_version is greater than parent's most current version.");
        }

        parent_version = src_version;
    }

    /* Update parent's ref count if parent is NULL. Currently can't fail */
    if (parent != NULL) {
        H5P__inc_ref_count(parent, TRUE);
    }

    /* Allocates a new property list class */
    new_class = H5P__mt_alloc_class();
    if (NULL == new_class) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create new property list class.");
    }

    /* Initialize class fields */
    atomic_store(&(new_class->tag), H5P_MT_CLASS_TAG);

    /* If root class then there is no parent */
    if (parent != NULL) {
        parent_id            = atomic_load(&(parent->id)); /** TODO: remove parent_id and just atomic_load into next line */
        new_class->parent_id = parent_id;
    }
    else {
        new_class->parent_id = H5I_INVALID_HID;
    }

    new_class->parent_ptr     = parent;
    new_class->parent_version = parent_version;

    new_class->name = strdup(name);
    if (new_class->name == NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, NULL, "Failed to copy name buffer.");
    }

    /** NOTE: Will to be set after creation and H5I_register() is called */
    atomic_store(&(new_class->id), H5I_INVALID_HID);
    new_class->type = type;

    atomic_store(&(new_class->curr_version), 1);
    atomic_store(&(new_class->next_version), 2);

    /* Creates the sentinel nodes and sets the negative sentinel as the head */
    new_class->pl_head = H5P__create_sentinels(TRUE);
    if (new_class->pl_head == NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create sentinel nodes.");
    }

    phys_pl_len = 2;
    log_pl_len  = 0;
    atomic_store(&(new_class->phys_pl_len), phys_pl_len);
    atomic_store(&(new_class->log_pl_len), log_pl_len);
    atomic_store(&(new_class->nprops_added), 0);

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

    /* Continue intializing the class's fields */

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


#if 1 /* testing increment parent's id ref_count */

    /** 
     * If the parent isn't NULL (should only occur for root class),
     * increment the ID for the parent in the index.
     */
    if ( parent )
    {
        if ( 0 >= H5I_inc_ref(atomic_load(&(parent->id)), FALSE) )
        {
            assert(FALSE);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, 
                        "unable to increment parent's ID ref_count in index");
        }
    }

#endif


    ret_value = new_class;

done:

    /* Clean up if an error occurred */
    if ( (ret_value == NULL) && (new_class) )
    {
        free(new_class);
    }

    /* update parent's thrd count */
    if (parent != NULL && inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(parent)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_create_class() */

/****************************************************************************************
 * Function:    H5P__mt_copy_class
 *
 * Purpose:     Function to create a copy of an existing property list class
 *              (H5P_mt_class_t)
 *
 *              NOTE: This function is very similar to H5P__mt_create_class(), with some
 *              minor changes to ensure the correct fields are copied from the original
 *              class.
 *
 *              NOTE: the parent class of the original is still treated as the parent of
 *              this copy, thus the parent's ref_count.plc is incremented for this copy.
 *
 *              NOTE: og_class is the original class that is being copied.
 *
 *
 * Return:      Success: Pointer to the new H5P_mt_class_t struct.
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_class_t *
H5P__mt_copy_class(H5P_mt_class_t *og_class)
{
    H5P_mt_class_t              *parent    = NULL;      /* Parent of original class */
    H5P_mt_class_t              *new_class = NULL;      /* Copy of the og_class */
    H5P_mt_class_ref_counts_t    ref_count;             /* ref count for new class */
    H5P_mt_active_thread_count_t thrd;                  /* thrd struct for new class */
    H5P_mt_class_sptr_t          fl_next;               /* new class's free list struct */
    uint64_t                     ver_at_copy = 0;       /* version of parent og_class is derived */
    size_t                       phys_pl_len;           /* new class's LFSLL physical length */
    size_t                       log_pl_len;            /* new class's LFSLL logical length */
    bool                         inc_thrd_flag = FALSE; /* Flag to dec og_class's thrd count*/
    bool                         par_thrd_flag = FALSE; /* Flag to dec parent's thrd count */

    H5P_mt_class_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__mt_copy_class__num_calls), 1);

    assert(og_class);
    assert(atomic_load(&(og_class->tag)) == H5P_MT_CLASS_TAG);

    /* Increment the original class's thrd count */
    if (H5P__inc_thrd_count(og_class) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Couldn't increment og_class's thread count.");
    }

    inc_thrd_flag = TRUE;

    /**
     * The new class is a copy so it must be 'derived' from
     * the same version of the parent as the original class.
     */
    ver_at_copy = atomic_load(&(og_class->curr_version));

    parent = og_class->parent_ptr;

    /**
     * If parent is NULL, there isn't a parent class to increment.
     * Otherwise, increment the thrd count in the parent class.
     */
    if (parent != NULL) {
        assert((atomic_load(&(parent->tag))) == H5P_MT_CLASS_TAG);

        /* Increment parent's thrd count */
        if (H5P__inc_thrd_count(parent) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Couldn't increment parent's thread count.");
        }

        par_thrd_flag = TRUE;

        /* Increment parent's ref_count.plc for the new class */
        H5P__inc_ref_count(parent, TRUE);

        /* update parents's thrd count, since no longer accessing parent */
        if (parent && par_thrd_flag) {
            if (0 > H5P__dec_thrd_count(parent)) {
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Failure to decrement parent's thrd_count.");
            }

            par_thrd_flag = FALSE;
        }
    }

    /* Allocates a new property list class */
    new_class = H5P__mt_alloc_class();
    if (NULL == new_class) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create new property list class.");
    }

    /* Initialize class fields */
    atomic_store(&(new_class->tag), H5P_MT_CLASS_TAG);

    new_class->parent_id      = og_class->parent_id;
    new_class->parent_ptr     = parent;
    new_class->parent_version = og_class->parent_version;

    new_class->name = strdup(og_class->name);
    if (new_class->name == NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, NULL, "Failed to copy name buffer.");
    }

    /** NOTE: ID is set after creation by calling function */
    atomic_store(&(new_class->id), H5I_INVALID_HID);
    new_class->type = og_class->type;

    atomic_store(&(new_class->curr_version), 1);
    atomic_store(&(new_class->next_version), 2);

    /* Creates the sentinel nodes and sets the negative sentinel as the head */
    new_class->pl_head = H5P__create_sentinels(TRUE);
    if (new_class->pl_head == NULL) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create sentinel nodes.");
    }

    phys_pl_len = 2;
    log_pl_len  = 0;
    atomic_store(&(new_class->phys_pl_len), phys_pl_len);
    atomic_store(&(new_class->log_pl_len), log_pl_len);
    atomic_store(&(new_class->nprops_added), 0);

    /* Copy the valid properties from the original class's LFSLL */
    if (0 > H5P__mt_copy_lfsll(new_class, og_class->pl_head, ver_at_copy)) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, NULL, "Failed to copy parent's lfsll.");
    }

    /* Continue intializing the class's fields */

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
    new_class->create_func = og_class->create_func;
    new_class->create_data = og_class->create_data;
    new_class->copy_func   = og_class->copy_func;
    new_class->copy_data   = og_class->copy_data;
    new_class->close_func  = og_class->close_func;
    new_class->close_data  = og_class->close_data;

    /* Initializes the class's stats fields */
    H5P__init_stats_class(new_class);


#if 1 /* testing increment parent's id ref_count */

    /** 
     * If the parent isn't NULL (should only occur for root class),
     * increment the ID for the parent in the index.
     */
    if ( parent )
    {
        if ( 0 >= H5I_inc_ref(atomic_load(&(parent->id)), FALSE) )
        {
            assert(FALSE);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, 
                        "unable to increment parent's ID ref_count in index");
        }
    }

#endif


    ret_value = new_class;

done:

    /* Clean up if an error occurred */
    if ( (ret_value == NULL) && (new_class) )
    {
        free(new_class);
    }

    /* update parent class's thrd count */
    if (parent && par_thrd_flag) {
        if (0 > H5P__dec_thrd_count(parent)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Failure to decrement parent's thrd_count.");
        }
    }

    /* update original class's thrd count */
    if (og_class && inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(og_class)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_copy_class() */

/****************************************************************************************
 * Function:    H5P__mt_alloc_class
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

    head_class = fl_head.ptr;

    /* If no class structs are reallocable, alloc from memory */
    if (head_class == NULL || (atomic_load(&(head_class->tag)) != H5P_MT_CLASS_FL_REALLOC_TAG)) {
        new_class = (H5P_mt_class_t *)malloc(sizeof(H5P_mt_class_t));
        if (NULL == new_class) {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "property list class allocation failed");
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

            fl_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

            /**
             * If head and tail ptr are equal than only one class is on the free list,
             * and the tail pointer must be updated.
             */
            if (fl_head.ptr == fl_tail.ptr) {
                done = FALSE;

                do {
                    fl_next = atomic_load(&(head_class->fl_next));

                    fl_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

                    if (!atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_tail), &fl_tail, fl_next)) {
                        /* failed, update stats and try again */
                        atomic_fetch_add(&(H5P_mt_g.class_fl_tail_update_cols), 1);

                        /* assert is not get stuck in an infinite loop while testing */
                        assert(H5P_MT_ASSERT_FAIL);
                    }
                    else {
                        /* success, update stats and continue */
                        atomic_fetch_add(&(H5P_mt_g.class_fl_tail_update), 1);

                        done = TRUE;
                    }

                } while (!done);

            } /* end if ( fl_head.ptr == fl_tail.ptr ) */

        } while (!done);

        /* Ensure the struct is cleared of any previous data */
        new_class = H5P__clear_mt_class(fl_head.ptr);

        /* update stats */
        atomic_fetch_add(&(H5P_mt_g.num_class_structs_allocated_from_fl), 1);

    } /* end else */

done:

    ret_value = new_class;

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_alloc_class() */

/****************************************************************************************
 * Function:    H5P__mt_create_list
 *
 * Purpose:     Multithread safe function to create a new property list (H5P_mt_list_t)
 *              derived from a property list class (H5P_mt_class_t), or to create a new
 *              property list that is a copy from an existing property list.
 *
 *              NOTE: property list classes are referred to as classes and property lists
 *              are referred to as lists.
 *
 *              Lists create an array of H5P_mt_list_table_entry_t of length
 *              nprops_inherited which point to the valid properties in the parent
 *              class's LFSLL.
 *
 *              NOTE: A specific version of the parent class to be derived from can be
 *              selected via the src_version parameter. If wanting the most current
 *              version set src_version = 0.
 *
 * Details:     NOTE: for more information of the H5P_mt_list_t structure or specific
 *              fields, check the detailed comment for the struture in H5Ppkg_mt.h
 *
 *              To ensure multi-thread safety the first step is to check the parent
 *              class's thrd field to ensure the parent class is not in the process
 *              opening or closing. If opening is TRUE the function loops checking again.
 *              If opening, the class will only be briefly visible to other threads
 *              before completing the opening process, so this thread should see that
 *              opening's been set to FALSE without waiting long (stats are collected, so
 *              if this proves incorrect, it can be found quickly and fixed). If closing
 *              is true, an error is thrown. If neither are true, the count field is
 *              incremented to show another thread is accessing the structure.
 *
 *              If copying another list, the above is done for the original list as well.
 *
 *              Next, we increment the ref_count of the parent class's derived lists,
 *              regardless of creating a new list or copying an existing one. Doing this
 *              now ensures, that the parent class cannot be deleted out from under us.
 *
 *              Memory is allocated for the new H5P_mt_list_t, by calling
 *              H5P__mt_alloc_list(), and the fields are initialized.
 *
 *              The new list's pl_head is initialized to the property that is returned
 *              by the the function H5P__create_sentinels(), which returns the negative
 *              sentine with it's next pointer pointing to the positive sentinel, and
 *              any new or modified properties will be inserted between them.
 *
 *              The thrd.opening field of the new list is initialized TRUE, to prevent
 *              other threads from accessing the structure until it's completely set up.
 *
 *              If this is a new list being created the lkup_tbl is allocated and
 *              initialized by the function H5P__init_lkup_tbl() based on the parent
 *              class.
 *
 *              But if this is a copy of an existing list, then the lkup_tbl is allocated
 *              and initialized by the function H5P__init_lkup_tbl_copy() to copy the
 *              og_list. Then H5P__mt_copy_lfsll() is called to copy the valid props
 *              from the original list to the new one.
 *
 *              H5P__init_stats_list() function is called to initialize the stats fields
 *              of the H5P_mt_list_t structure to collect stats for testing purposes.
 *
 *              H5I_register() is called on the new list to register it in the index and
 *              get an id.
 *
 *              NOTE: Now that we have an ID and are in the index, we are visible to
 *              other threads. However, thrd.opening being TRUE prevents any other thread
 *              from accessing the struct until we finish the opening process.
 *
 *              Now that we have an ID for the new list, we check if this is a newly
 *              created list, or a copy of an existing one, and perform the respective
 *              class callback on the parent class and up the inheritance tree.
 *
 *              The new list's thrd thrd.opening is set to FALSE allowing other threads
 *              to access this structure.
 *
 *              The parent class's thrd.count is decremented.
 *
 *              NOTE: May separate this in a future iteration into two functions, one
 *              for creating a new list, and one for copying an existing list. They are
 *              currently the same function because much of the code in the two functions
 *              would be the same.
 *
 *
 * Return:      Success: Returns a pointer to the new H5P_mt_list_t struct.
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_list_t *
H5P__mt_create_list(H5P_mt_class_t *parent, H5P_mt_list_t *og_list, bool copy, uint64_t src_version,
                    bool app_ref)
{
    H5P_mt_list_t               *new_list = NULL;            /* New list to be created */
    hid_t                        parent_id;                  /* ID of the parent class */
    H5P_mt_active_thread_count_t list_thrd;                  /* thrd struct for new list */
    H5P_mt_list_sptr_t           fl_next;                    /* new list's free list struct */
    uint64_t                     new_list_version;           /* parent's version to derive from */
    uint64_t                     og_version = 0;                 /* version of the original list */
    bool                         inc_thrd_flag      = FALSE; /* Flag to dec parent's thrd count */
    bool                         inc_thrd_flag_list = FALSE; /* flag to dec og_list thrd count */
    hid_t                        new_plist_id;               /* ID of the new list */

    H5P_mt_list_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__mt_create_list__num_calls), 1);

    assert(parent);
    assert((atomic_load(&(parent->tag))) == H5P_MT_CLASS_TAG);

    /* Get current version of parent to derive from */
    new_list_version = atomic_load(&(parent->curr_version));

    /* if og_list isn't NULL ensure it's valid */
    if (og_list) {
        assert(og_list->tag == H5P_MT_LIST_TAG);
        og_version = atomic_load(&(og_list->curr_version));
    }

    /* If copying an existing list */
    if (copy) {
        /* Increment the original list's thrd.count */
        if (H5P__inc_thrd_count(og_list) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, "Couldn't increment old list's thread count.");
        }
        else {
            inc_thrd_flag_list = TRUE; /* Set flag to dec thrd count */
        }

        /* If copying a list, the version derived from is the same as the original */
        new_list_version = og_list->pclass_version;
    }

    /* Increment parent's thrd count */
    if (H5P__inc_thrd_count(parent) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, "Couldn't increment parent's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /**
     * NOTE: This is for testing to choose a specific version to create the list at to
     * ensure only the valid properties for that version are stored in the lkup_tbl.
     */
    if (src_version > 0) {
        /* If src_version is larger than any version of the parent, throw an error */
        if (src_version > new_list_version) {
            assert(src_version <= new_list_version);
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL,
                        "src_version is greater than parents's most current version.");
        }

        new_list_version = src_version;
    }

    /* Update parent's ref count. Currently can't fail */
    H5P__inc_ref_count(parent, FALSE);

    /* Allocates a new property list */
    new_list = H5P__mt_alloc_list();
    if (NULL == new_list) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create new property list.");
    }

    /* Initialize list fields */
    atomic_store(&(new_list->tag), H5P_MT_LIST_TAG);

    parent_id           = atomic_load(&(parent->id));
    new_list->pclass_id = parent_id;

    new_list->pclass_ptr     = parent;
    new_list->pclass_version = new_list_version;

    atomic_store(&(new_list->plist_id), H5I_INVALID_HID);
    atomic_store(&(new_list->curr_version), 1);
    atomic_store(&(new_list->next_version), 2);

    new_list->lkup_tbl = NULL;

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

    /* Set class initialization flag to false fo now */
    atomic_store(&(new_list->class_init), FALSE);

    /* Set opening flag to TRUE */
    list_thrd.count   = 0;
    list_thrd.opening = TRUE;
    list_thrd.closing = FALSE;
    atomic_store(&(new_list->thrd), list_thrd);

    fl_next.ptr = NULL;
    fl_next.sn  = 0;
    atomic_store(&(new_list->fl_next), fl_next);

    /* Allocate and intialize lkup_tbl */

    /* If copy, then we must copy the original list's lkup_tbl and lfsll */
    if (copy) {
        /* Copy original list's lkup_tbl */
        if (0 > (H5P__init_lkup_tbl_copy(og_list, og_version, new_list))) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to copy lkup_tbl.");
        }

        /* Iterate the og_list's LFSLL and copy the valid props into the new list */
        if (0 > H5P__mt_copy_lfsll(new_list, og_list->pl_head, og_version)) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, NULL, "Failed to copy list's lfsll");
        }
    }
    /* Else we must initialize the lkup_tbl from the parent class */
    else {
        if (0 > (H5P__init_lkup_tbl(parent, new_list_version, new_list))) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create lkup_tbl.");
        }
    }

    /* Initialize all the stats for the list */
    H5P__init_stats_list(new_list);

    /* Register the new list in the index and get and ID. */
    if ((new_plist_id = H5I_register(H5I_GENPROP_LST, new_list, app_ref)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, NULL, "unable to register property list");
    }

    atomic_store(&(new_list->plist_id), new_plist_id);

    /**
     * If copy is TRUE, then call the class copy callback on the
     * parent classes up the inheritance tree, if it exists.
     */
    if (copy) {
        while (parent) {
            /* If the class has a copy callback, call it */
            if (parent->copy_func) {
                hid_t new_list_id = atomic_load(&(new_list->plist_id));
                hid_t og_list_id  = atomic_load(&(og_list->plist_id));

                /* If the copy callback fails remove the list's id from the index */
                if ((parent->copy_func)(new_list_id, og_list_id, parent->copy_data) < 0) {
                    H5I_remove(new_plist_id);

                    assert(H5P_MT_ASSERT_FAIL);
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, NULL, "Can't initialize property");
                }

            } /* end if ( parent0>copy_func ) */

            /* Increment up the parent tree */
            parent = parent->parent_ptr;

        } /* end while ( parent ) */

        /* Reset parent to point to the new list's direct parent class */
        parent = new_list->pclass_ptr;

    } /* end if ( copy ) */
    /**
     * If copy is FALSE, then call the class create callback on
     * the parent classes up the inheritance tree, if it exits.
     */
    else {
        while (parent) {
            /* If the parent has a create callback, call it */
            if (parent->create_func) {
                hid_t new_list_id = atomic_load(&(new_list->plist_id));

                /* If the create callback fails remove the list's id from the index */
                if ((parent->create_func)(new_list_id, parent->create_data) < 0) {
                    H5I_remove(new_plist_id);

                    assert(H5P_MT_ASSERT_FAIL);
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, NULL, "Can't initialize property");
                }

            } /* end if ( parent->create_func ) */

            /* Increment up the parent tree */
            parent = parent->parent_ptr;

        } /* end while ( parent ) */

        /* Reset parent to point to the new list's direct parent class */
        parent = new_list->pclass_ptr;

    } /* end else */

    /* Set the class initialization flag */
    atomic_store(&(new_list->class_init), TRUE);

    /* update thrd struct of the new_list to mark opening FALSE */
    list_thrd.count   = 0;
    list_thrd.opening = FALSE;
    list_thrd.closing = FALSE;

    atomic_store(&(new_list->thrd), list_thrd);


#if 1 /* testing increment parent's id's ref_count */

    if ( 0 >= H5I_inc_ref(atomic_load(&(parent->id)), FALSE) )
    {
        assert(FALSE);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, NULL, 
                    "unable to increment parent's ID ref_count in index");
    }

#endif


done:

    /* update parent's thrd count */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(parent)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Failure to decrement thrd_count.");
        }
    }

    /* update original list's thrd count */
    if (inc_thrd_flag_list) {
        if (0 > H5P__dec_thrd_count(og_list)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Failure to decrement thrd_count.");
        }
    }

    /* If an error occured, properly handle the allocated memory */

    if ( new_list )
    {
        new_plist_id = atomic_load(&(new_list->plist_id));

        if (H5I_INVALID_HID == new_plist_id && new_list) {
            H5P__mt_close_list(new_list);
        }
    }

    ret_value = new_list;

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_create_list() */

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

    head_list = fl_head.ptr;

    /* If no list structs are reallocable, alloc from memory */
    if (head_list == NULL || (atomic_load(&(head_list->tag)) != H5P_MT_LIST_FL_REALLOC_TAG)) {
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

                fl_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

                /**
                 * If head and tail ptr are equal than only one list is on the free list,
                 * and the tail pointer must be updated.
                 */
                if (fl_head.ptr == fl_tail.ptr) {
                    done = FALSE;

                    do {
                        fl_next = atomic_load(&(head_list->fl_next));

                        fl_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

                        if (!atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_tail), &fl_tail, fl_next)) {
                            /* failed, update stats and try again */
                            atomic_fetch_add(&(H5P_mt_g.list_fl_tail_update_cols), 1);

                            /* assert is not get stuck in an infinite loop while testing */
                            assert(H5P_MT_ASSERT_FAIL);
                        }
                        else {
                            /* success, update stats and continue */
                            atomic_fetch_add(&(H5P_mt_g.list_fl_tail_update), 1);

                            done = TRUE;
                        }

                    } while (!done);

                } /* end if ( fl_head.ptr == fl_tail.ptr ) */
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
                        H5P__mt_create_prop(valid_prop->name, valid_prop_value.ptr, valid_prop_value.size,
                                            FALSE, 1, valid_prop->create, valid_prop->set, valid_prop->get,
                                            valid_prop->encode, valid_prop->decode, valid_prop->del,
                                            valid_prop->copy, valid_prop->cmp, valid_prop->close);
                    if (NULL == new_prop)
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL,
                                    "Failed creating property for property list.");

                    /* Call the create callback */
                    if ((new_prop->create)(new_prop->name, valid_prop_value.size, valid_prop_value.ptr) < 0) {
                        assert(H5P_MT_ASSERT_FAIL);
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, "Can't create property");
                    }

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
    if ( (ret_value == FAIL) && new_list->lkup_tbl )
    {
        free(new_list->lkup_tbl);
        new_list->lkup_tbl = NULL;
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__init_lkup_tbl() */

/****************************************************************************************
 * Function:    H5P__init_lkup_tbl_copy
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
    H5P_mt_list_table_entry_t *new_entry;      /* Entry in the new list's lkup_tbl */
    H5P_mt_list_table_entry_t *old_entry;      /* Old list's entry in the lkup_tbl */
    H5P_mt_list_prop_ref_t     new_base;       /* Base for the new list's entry */
    H5P_mt_list_prop_ref_t     old_base;       /* Base for the old list's entry */
    H5P_mt_list_prop_ref_t     new_curr;       /* Curr for the new list's entry */
    H5P_mt_list_prop_ref_t     old_curr;       /* Curr for the old list's entry */
    H5P_mt_prop_t             *old_prop;       /* Property from the old list */
    H5P_mt_prop_t             *new_prop;       /* Property for the new list */
    H5P_mt_prop_value_t        old_prop_value; /* Value from an old list's prop */
    H5P_mt_class_t            *parent;         /* Parent class of the old list */
    uint64_t old_base_delete;   /* Old list's entry's base_delete_version */
    uint32_t nprops;            /* Number of props in the new list */
    uint32_t deletes       = 0; /* Tracks number of deletes */
    uint32_t nodes_visited = 0; /* Tracks number of nodes visited */
    uint32_t thrd_cols     = 0; /* Tracks number of thread cols */
    bool     chksum_cols   = FALSE;

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
    new_list->lkup_tbl =
        (H5P_mt_list_table_entry_t *)malloc(new_list->nprops_inherited * sizeof(H5P_mt_list_table_entry_t));
    if (NULL == new_list->lkup_tbl) {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "lkup_tbl allocation failed");
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
            old_prop = old_curr.ptr;
            old_prop_value = atomic_load(&(old_prop->value));

            if ((0 == (atomic_load(&(old_prop->delete_version)))) ||
                (version <= (atomic_load(&(old_prop->delete_version))))) {

                /* Create a new property that's a copy of the old_prop */
                new_prop = H5P__mt_create_prop(old_prop->name, old_prop_value.ptr, old_prop_value.size, FALSE,
                                               1, old_prop->create, old_prop->set, old_prop->get,
                                               old_prop->encode, old_prop->decode, old_prop->del,
                                               old_prop->copy, old_prop->cmp, old_prop->close);
                if (NULL == new_prop)
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "Failed creating property for property list.");

                /* If the new_prop has the copy callback, call it */
                if (new_prop->copy) {
                    if ((new_prop->copy)(new_prop->name, old_prop_value.size, old_prop_value.ptr) < 0) {
                        assert(H5P_MT_ASSERT_FAIL);
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");
                    }
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
            else 
            {
                /* Create a new property that's a copy of the old_prop */
                new_prop = H5P__mt_create_prop(old_prop->name, old_prop_value.ptr, old_prop_value.size, FALSE,
                                               1, old_prop->create, old_prop->set, old_prop->get,
                                               old_prop->encode, old_prop->decode, old_prop->del,
                                               old_prop->copy, old_prop->cmp, old_prop->close);
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
                
                if ( old_base_delete > 0 && old_base_delete <= version )
                {
                    atomic_store(&(new_entry->base_delete_version), 1);
                }
                else
                {
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
    if ( (ret_value == FAIL) && new_list->lkup_tbl )
    {
        free(new_list->lkup_tbl);
        new_list->lkup_tbl = NULL;
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__init_lkup_tbl_copy() */

/****************************************************************************************
 * Function:    H5P__create_sentinels
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
    if ( (ret_value == NULL) && neg_sentinel )
    {
        if ( neg_sentinel )
        {
            free(neg_sentinel);
        }
        if ( pos_sentinel )
        {
            free(pos_sentinel);
        }        
    }


    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__create_setinels() */

/****************************************************************************************
 * Function:    H5P__mt_create_prop
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
H5P__mt_create_prop(const char *name, const void *value_ptr, size_t value_size, bool in_prop_class,
                    uint64_t version, H5P_prp_create_func_t prp_create, H5P_prp_set_func_t prp_set,
                    H5P_prp_get_func_t prp_get, H5P_prp_encode_func_t prp_encode,
                    H5P_prp_decode_func_t prp_decode, H5P_prp_delete_func_t prp_del,
                    H5P_prp_copy_func_t prp_copy, H5P_prp_compare_func_t prp_cmp,
                    H5P_prp_close_func_t prp_close)
{
    H5P_mt_prop_t      *new_prop; /* The new property to be created */
    H5P_mt_prop_aptr_t  next;     /* The new property's next field */
    H5P_mt_prop_value_t value;    /* The new property's value field */

    H5P_mt_prop_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.H5P__mt_create_prop__num_calls), 1);

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


    ret_value = new_prop;

done:

    /* Clean up if an error occurred */
    if ( (ret_value == NULL) && (new_prop) )
    {
        free(new_prop);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_create_prop() */

/****************************************************************************************
 * Function:    H5P__mt_alloc_prop
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

    head_prop = fl_head.ptr;

    /* If no prop structs are reallocable, alloc from memory */
    if (head_prop == NULL || (atomic_load(&(head_prop->tag)) != H5P_MT_PROP_FL_REALLOC_TAG)) {
        new_prop = (H5P_mt_prop_t *)malloc(sizeof(H5P_mt_prop_t));

        if (NULL == new_prop) {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "property allocation failed");
        }

        /* update stats */
        atomic_fetch_add(&(H5P_mt_g.num_prop_structs_allocated_from_heap), 1);
    }
    /* If a struct on the fre  list is reallocable, clear it and return it */
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

            fl_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

            /**
             * If head and tail ptr are equal than only one property is on the free list,
             * and the tail pointer must be updated.
             */
            if (fl_head.ptr == fl_tail.ptr) {
                done = FALSE;

                do {
                    fl_next = atomic_load(&(head_prop->next));

                    fl_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

                    if (!atomic_compare_exchange_strong(&(H5P_mt_g.prop_fl_tail), &fl_tail, fl_next)) {
                        /* failed, update stats and try again */
                        atomic_fetch_add(&(H5P_mt_g.prop_fl_tail_update_cols), 1);

                        /* assert is not get stuck in an infinite loop while testing */
                        assert(H5P_MT_ASSERT_FAIL);
                    }
                    else {
                        /* success, update stats and continue */
                        atomic_fetch_add(&(H5P_mt_g.prop_fl_tail_update), 1);

                        done = TRUE;
                    }

                } while (!done);

            } /* end if ( fl_head.ptr == fl_tail.ptr ) */

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
                new_prop = H5P__mt_create_prop(
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
                        if ((new_prop->copy)(new_prop->name, value.size, value.ptr) < 0) {
                            assert(H5P_MT_ASSERT_FAIL);
                            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");
                        }
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
 * Function:    H5P__mt_ins_or_mod_prop__class
 *
 * Purpose:     Inserts a new property (H5P_mt_prop_t struct) into the LFSLL of a
 *              property list class (H5P_mt_class_t).
 *
 *              NOTE: When 'modifying' a property is this multithread safe version of
 *              H5P, a new property struct must be created with a new create_version.
 *              Because of this, when an entirely new property is created, or a new
 *              property is created for the new version of a property, this function is
 *              called to handle either case.
 *
 *              This function first increments the thread reference count of the class
 *              (list->thrd.count), and then calls H5P__mt_enforce_serialization().
 *              Which checks if this thread is allowed to continue, or if there are other
 *              threads actively modifying the class structure. If there are already
 *              threads modifying the structure, this thread waits until its turn before
 *              calling H5P__mt_create_prop() to create the new H5P_mt_prop_t struct for
 *              the new property.
 *
 *              NOTE: For multiple threads simultaneiously either modifying, inserting,
 *              or deleting a property in the list, there is an ordering that must be
 *              followed. See the comment description above H5P_mt_list_t or
 *              H5P_mt_class_t in H5Ppkg_mt.h for more details.
 *
 *              Next this function calls H5P__mt_ins_or_mod_prop__lfsll_ins(), which
 *              is the function that actually inserts the property in the LFSLL.
 *
 *              NOTE: See the comment description above struct H5P_mt_prop_t in
 *              H5Ppkg_mt.h for details on how properties are sorted.
 *
 *              Then phys_pl_len, and nprops_added and log_pl_len if this is a brand new
 *              property and not a new version of an existing property. Any necessary
 *              stats fields are updated, before incrementing the curr_version of the
 *              class.
 *
 *              Lastly the thread count of the class is decremented.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_ins_or_mod_prop__class(H5P_mt_class_t *class, const char *name, void *value, size_t size,
                               bool is_new, H5P_prp_create_func_t prp_create, 
                               H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get, 
                               H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode,
                               H5P_prp_delete_func_t prp_del, H5P_prp_copy_func_t prp_copy, 
                               H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close)
{
    H5P_mt_prop_t     *new_prop = NULL;    /* New prop to be created and inserted */
    H5P_mt_prop_t     *pl_head;            /* Head of the LFSLL of the class */
    H5P_mt_prop_t     *next_prop;          /* Next prop in LFSLL after the new prop */
    H5P_mt_prop_aptr_t next;               /* New prop's next struct field */
    uint64_t           curr_version   = 0; /* Current version of the class */
    uint64_t           next_version   = 0; /* Next version of the class */
    uint64_t           delete_version = 0;
    bool               inc_thrd_flag  = FALSE; /* Flag to dec parent's thrd count */
    uint32_t           deletes        = 0;     /* Tracks number of deletes */
    uint32_t           visited        = 0;     /* Tracks number of nodes visited */
    uint32_t           thrd_cols      = 0;     /* Tracks number of thread collisions */
    uint64_t           avg_visited    = 0;     /* Stats variable */
    uint64_t           num_calls      = 0;     /* Stats variable */
    bool               chksum_cols    = FALSE;
    bool               ver_updated    = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(class->H5P__insert_prop_class__num_calls), 1);

    assert(class);
    assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);
    assert(name);
    assert((size > 0 && value != NULL) || (size == 0));

    /* Increment thread count */
    if ((H5P__inc_thrd_count(class)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Couldn't increment class's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    curr_version = atomic_load(&(class->curr_version));
    next_version = atomic_fetch_add(&(class->next_version), 1);
    ver_updated = TRUE;

    /* Ensure another thread isn't modifying the class */
    if ((curr_version + 1) < next_version) {
        if (0 == (curr_version = H5P__mt_enforce_serialization(class, curr_version, next_version))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }

        curr_version = atomic_load(&(class->curr_version));
    }

    assert(curr_version + 1 == next_version);

    /* If this is a new property being inserted, ensure it doesn't already exist */
    if ( is_new )
    {
        if ( NULL != (new_prop = H5P__mt_search__class(class, name, curr_version)) )
        {
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Property already exists in the class.");
        }
    }

    /* This thread can now proceed and create the new property */
    new_prop = H5P__mt_create_prop(name, value, size, TRUE, next_version, prp_create, prp_set, prp_get,
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

    next      = atomic_load(&(new_prop->next));
    next_prop = next.ptr;

    assert(atomic_load(&(next_prop->tag)) == H5P_MT_PROP_TAG);

    /**
     * If the next prop in the lfsll has the same chksum then don't increment logical
     * length, because this is a 'modification' to an existing prop and not an entirely
     * property. With the exception of if the next prop is deleted. The logical length
     * was decremented upon it's deletion so must increment it now.
     */
    if (new_prop->chksum == next_prop->chksum) {
        delete_version = atomic_load(&(next_prop->delete_version));

        if (delete_version > 0 && delete_version < curr_version) {
            atomic_fetch_add(&(class->log_pl_len), 1);
            atomic_fetch_add(&(class->nprops_added), 1);
        }
    }
    else {

        atomic_fetch_add(&(class->log_pl_len), 1);
        atomic_fetch_add(&(class->nprops_added), 1);
    }

    /* Increment physical length of the lfsll */
    atomic_fetch_add(&(class->phys_pl_len), 1);

    /** TODO: add #if debug for updating avg and max visited */

    /* update stats */
    atomic_store(&(class->num_insert_nodes_visited), visited);

    if (chksum_cols) {
        atomic_fetch_add(&(class->num_insert_prop__chksum_cols), 1);
    }

    if (visited > atomic_load(&(class->insert_max_nodes_visited))) {
        atomic_store(&(class->insert_max_nodes_visited), visited);
    }

    avg_visited = atomic_load(&(class->insert_avg_nodes_visited));
    num_calls   = atomic_load(&(class->H5P__insert_prop_class__num_calls));

    avg_visited = H5P__calc_avg_visited(avg_visited, num_calls, visited);

    atomic_store(&(class->insert_avg_nodes_visited), avg_visited);

    atomic_fetch_add(&(class->num_insert_prop__cols), thrd_cols);
    atomic_fetch_add(&(class->num_insert_prop__success), 1);
    atomic_fetch_add(&(H5P_mt_g.num_props_inserted_classes), 1);

    if (atomic_load(&(class->phys_pl_len)) > atomic_load(&(H5P_mt_g.max_class_num_phys_props))) {
        atomic_store(&(H5P_mt_g.max_class_num_phys_props), atomic_load(&(class->phys_pl_len)));
    }

done:

    /* Cleanup if error occurred */
    if ( ver_updated )
    {
        /* Update the class's current version */
        curr_version = atomic_fetch_add(&(class->curr_version), 1);

        //next_version = atomic_load(&(class->next_version));
        assert(curr_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(class->curr_version)) > atomic_load(&(H5P_mt_g.max_class_version_number))) {
            atomic_store(&(H5P_mt_g.max_class_version_number), atomic_load(&(class->curr_version)));
        }
    }

    /* If the parent's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(class)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_ins_or_mod_prop__class() */

/****************************************************************************************
 * Function:    H5P__mt_ins_or_mod_prop__list
 *
 * Purpose:     Inserts a new property (H5P_mt_prop_t struct) into the LFSLL of a
 *              property list (H5P_mt_list_t).
 *
 *              NOTE: When 'modifying' a property is this multithread safe version of
 *              H5P, a new property struct must be created with a new create_version.
 *              Because of this, when an entirely new property is created, or a new
 *              property is created for the new version of a property, this function is
 *              called to handle either case.
 *
 *              This function first increments the thread reference count of the class
 *              (list->thrd.count), and then calls H5P__mt_enforce_serialization().
 *              Which checks if this thread is allowed to continue, or if there are other
 *              threads actively modifying the class structure. If there are already
 *              threads modifying the structure, this thread waits until its turn before
 *              calling H5P__mt_create_prop() to create the new H5P_mt_prop_t struct for
 *              the new property.
 *
 *              NOTE: For multiple threads simultaneiously either modifying, inserting,
 *              or deleting a property in the list, there is an ordering that must be
 *              followed. See the comment description above H5P_mt_list_t or
 *              H5P_mt_class_t in H5Ppkg_mt.h for more details.
 *
 *              If the copy flag parameter is TRUE the property is checked for a copy
 *              callback, and if it exists it is called.
 *
 *              Else if the create flag parameter is TRUE the property is checked for a
 *              create callback, and if it exists it is called.
 *
 *              Next this function calls H5P__mt_ins_or_mod_prop__lfsll_ins(), which
 *              is the function that actually inserts the property in the LFSLL.
 *
 *              In case this is a 'modification' to an existing property, the list must
 *              iterate its lkup_tbl and if an entry contains an older version of the new
 *              property, it must update the curr.ptr to point to the new version, update
 *              curr.ver to the new version, and set the new_prop->in_lkup_tbl to TRUE.
 *
 *              NOTE: See the comment description above struct H5P_mt_prop_t in
 *              H5Ppkg_mt.h for details on how properties are sorted.
 *
 *              Then phys_pl_len, and nprops_added and log_pl_len if this is a brand new
 *              property and not a new version of an existing property. Any necessary
 *              stats fields are updated, before incrementing the curr_version of the
 *              class.
 *
 *              Lastly the thread count of the class is decremented.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_ins_or_mod_prop__list(H5P_mt_list_t *list, const char *name, void *value, size_t size, bool create,
                              bool copy, bool is_new, H5P_prp_create_func_t prp_create, 
                              H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get, 
                              H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode, 
                              H5P_prp_delete_func_t prp_del, H5P_prp_copy_func_t prp_copy, 
                              H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close)
{
    H5P_mt_prop_t             *new_prop = NULL;       /* New prop to be created and inserted */
    H5P_mt_prop_t             *pl_head;               /* Head of the LFSLL of the class or list */
    H5P_mt_prop_t             *next_prop;             /* Next prop in LFSLL after the new prop */
    H5P_mt_prop_aptr_t         next;                  /* New prop's next struct field */
    H5P_mt_prop_value_t        prop_value;            /* Value of the new prop */
    uint64_t                   curr_version  = 0;     /* Current version of list or class */
    uint64_t                   next_version  = 0;     /* Next version of list or class */
    bool                       done          = FALSE; /* Flag to exit a loop to setting atomics */
    bool                       inc_thrd_flag = FALSE; /* Flag to dec parent's thrd count */
    bool                       base_flag     = FALSE;
    H5P_mt_list_table_entry_t *entry;           /* An entry in lkup_tbl if param is a list */
    H5P_mt_list_prop_ref_t     curr;            /* Curr struct field for list's entry */
    H5P_mt_list_prop_ref_t     new_curr;        /* Updated curr for list's entry if needed */
    uint32_t                   deletes     = 0; /* Tracks number of deletes */
    uint32_t                   visited     = 0; /* Tracks number of nodes visited */
    uint32_t                   thrd_cols   = 0; /* Tracks number of thread collisions */
    uint64_t                   avg_visited = 0; /* Stats variable */
    uint64_t                   num_calls   = 0; /* Stats variable */
    bool                       chksum_cols = FALSE;
    bool                       ver_updated = FALSE;
    bool                       prop_cleanup = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    /* update stats */
    atomic_fetch_add(&(list->H5P__insert_prop_list__num_calls), 1);

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);
    assert(name);
    assert((size > 0 && value != NULL) || (size == 0));

    /* Increment thread count */
    if ((H5P__inc_thrd_count(list)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Couldn't increment list's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    curr_version = atomic_load(&(list->curr_version));
    next_version = atomic_fetch_add(&(list->next_version), 1);
    ver_updated = TRUE;

    /* Ensure another thread isn't modifying the class */
    if ((curr_version + 1) < next_version) {
        if (0 == (curr_version = H5P__mt_enforce_serialization(list, curr_version, next_version))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }

        curr_version = atomic_load(&(list->curr_version));
    }

    assert(curr_version + 1 == next_version);

    /* If this is a new property being inserted, ensure it doesn't already exist */
    if ( is_new )
    {
        if ( NULL != (new_prop = H5P__mt_search__list(list, name, curr_version)) )
        {
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Property already exists in the list.");
        }
    }


    /* This thread can now proceed and create the new property */
    new_prop = H5P__mt_create_prop(name, value, size, FALSE, next_version, prp_create, prp_set, prp_get,
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
            if ((new_prop->copy)(new_prop->name, prop_value.size, prop_value.ptr)) {
                prop_cleanup = TRUE;
                assert(H5P_MT_ASSERT_FAIL);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Copy property callback failed");
            }
        }
    }
    /* If create is TRUE and the create callback exists call it */
    else if (create) {
        if (new_prop->create) {
            if ((new_prop->create)(new_prop->name, prop_value.size, prop_value.ptr)) {
                prop_cleanup = TRUE;
                assert(H5P_MT_ASSERT_FAIL);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Create property callback failed");
            }
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

                assert(curr.ver < next_version);

                /* If there isn't a valid previous version of this property */
                if (NULL == H5P__mt_entry_find_version(entry, curr_version, &base_flag)) {
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
                    if (1 == H5P__is_valid(curr.ptr, curr_version)) {
                        /* Increment logical length of the LFSLL */
                        atomic_fetch_add(&(list->log_pl_len), 1);
                    }
                }

                /* Set the flag to show this prop is from the lkup_tbl */
                new_prop->in_lkup_tbl = TRUE;

                new_curr.ptr = new_prop;
                new_curr.ver = next_version;

                /** TODO: May need to change this to atomic_store() */
                if (!atomic_compare_exchange_strong(&(entry->curr), &curr, new_curr)) {
                    /* attempt failed, update stats and try again */
                    atomic_fetch_add(&(list->num_insert_update_entry_cols), 1);

                    /* To not get stuck in an infinite loop while testing */
                    assert(H5P_MT_ASSERT_FAIL);
                }
                else {
                    /* attempt failed, update stats and try again */
                    atomic_fetch_add(&(list->num_insert_update_entry), 1);

                    done = TRUE;
                }

                /* If this is the first curr version, set first_ver_of_curr */
                if (0 == atomic_load(&(entry->first_ver_of_curr))) {
                    atomic_store(&(entry->first_ver_of_curr), next_version);
                }

            } while (!done);

        } /* end if ( entry ) */

    } /* end if ( ! copy ) */

    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    /**
     * Check if the next prop in the LFSLL has the same chksum. If it does, and
     * if that version isn't deleted, don't increment logical length, because
     * this is a new version of a prop already being counted by log_pl_len.
     */

    next      = atomic_load(&(new_prop->next));
    next_prop = next.ptr;

    /* If in_lkup_tbl flag is FALSE */
    if (!new_prop->in_lkup_tbl) {
        /* And there isn't a previous valid version */
        if (new_prop->chksum != next_prop->chksum ||
            (new_prop->chksum == next_prop->chksum && 0 != H5P__is_valid(next_prop, curr_version))) {
            /* Update counts */
            atomic_fetch_add(&(list->nprops), 1);
            atomic_fetch_add(&(list->nprops_added), 1);
            atomic_fetch_add(&(list->log_pl_len), 1);
        }
    }

    /* Increment physical length of the lfsll */
    atomic_fetch_add(&(list->phys_pl_len), 1);

    /** TODO: add #if debug for updating avg and max visited */

    /* update stats */
    atomic_store(&(list->num_insert_nodes_visited), visited);

    if (visited > atomic_load(&(list->insert_max_nodes_visited))) {
        atomic_store(&(list->insert_max_nodes_visited), visited);
    }

    avg_visited = atomic_load(&(list->insert_avg_nodes_visited));
    num_calls   = atomic_load(&(list->H5P__insert_prop_list__num_calls));

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
        free(new_prop); /** TODO: maybe this should be H5P__mt_close_prop() */
    }
    if ( ver_updated )
    {
        /* Update the list's current version */
        curr_version = atomic_fetch_add(&(list->curr_version), 1);

        //next_version = atomic_load(&(list->next_version));
        assert(curr_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(list->curr_version)) > atomic_load(&(H5P_mt_g.max_list_version_number))) {
            atomic_store(&(H5P_mt_g.max_list_version_number), atomic_load(&(list->curr_version)));
        }
    }

    /* If the parent's thrd_count was increment, decrement it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(list)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_ins_or_mod_prop__list() */

/****************************************************************************************
 * Function:    H5P__mt_ins_or_mod_prop__lfsll_ins
 *
 * Purpose:     Inserts a new property (H5P_mt_prop_t struct) into the LFSLL of a
 *              property list (H5P_mt_list_t) or property list class (H5P_mt_class_t).
 *
 *              Helper function that passes pointers to H5P__find_mod_point() to find the
 *              location to insert the new property. Then inserts that new property
 *              between the properties returned by H5P__find_mod_point().
 *
 *              NOTE: This function is called by H5P__mt_ins_or_mod_prop__main() to
 *              handle the insertion of a new or modified property.
 *              Additionally, it is called by a few other functions to insert properties
 *              into a class during that classes initialization. This is done to avoid
 *              the stats being incremented before needed, but also to avoid increasing
 *              the version of a class during its initialization while creating and
 *              inserting its default properties.
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
 * Function:    H5P__mt_delete_prop__class
 *
 * Purpose:     Sets the delete_version of a property (H5P_mt_prop_t) in a property list
 *              class (H5P_mt_class_t).
 *
 *              H5P__mt_delete_prop__class() first gets the chksum of the property via
 *              the name parameter, and then increments the thread count of the class
 *              structure.
 *
 *              H5P__mt_enforce_serialization() is called to check if there is another
 *              thread modifying the LFSLL. If not then proceed, if there is this thread
 *              must wait it's turn.
 *
 *              NOTE: For multiple threads simultaneiously either modifying, inserting,
 *              or deleting a property in the class or list, there is an ordering that
 *              must be followed. See the comment description above H5P_mt_class_t in
 *              H5Ppkg_mt.h for more details.
 *
 *              Then iterate the LFSLL to find the target_prop by calling the
 *              function H5P__find_mod_point(), and sets the delete_version of the
 *              returned property to the class's next_version
 *
 *              Finally, the class's current version is updated to the same next version
 *              that was set as the delete_version for the prop. And update stats and
 *              decrement the thrd_count of the structure this thread is in.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_delete_prop__class(H5P_mt_class_t *class, const char *name)
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
    atomic_fetch_add(&(class->H5P__delete_prop__class__num_calls), 1);

    /* Increment thread count */
    if (H5P__inc_thrd_count(class) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Couldn't increment class's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    curr_version = atomic_load(&(class->curr_version));
    next_version = atomic_fetch_add(&(class->next_version), 1);
    ver_updated = TRUE;

    /* Ensure another thread isn't modifying the LFSLL */
    if ((curr_version + 1) < next_version) {
        if (0 == (curr_version = H5P__mt_enforce_serialization(class, curr_version, next_version))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }

        curr_version = atomic_load(&(class->curr_version));
    }

    assert(curr_version + 1 == next_version);

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
    atomic_fetch_add(&(class->num_set_delete__success), 1);
    atomic_store(&(class->num_set_delete__nodes_visited), visited);
    atomic_fetch_add(&(class->num_set_delete__cols), thrd_cols);
    atomic_fetch_add(&(H5P_mt_g.num_props_deleted_classes), 1);

    if (visited > atomic_load(&(class->set_delete__max_nodes_visited))) {
        atomic_store(&(class->set_delete__max_nodes_visited), visited);
    }

    avg_visited = atomic_load(&(class->set_delete__avg_nodes_visited));
    num_calls   = atomic_load(&(class->H5P__delete_prop__class__num_calls));

    avg_visited = H5P__calc_avg_visited(avg_visited, num_calls, visited);

    atomic_store(&(class->set_delete__avg_nodes_visited), avg_visited);


done:

    /* Cleanup if error occurred */
    if ( ver_updated )
    {
        /* Update the class's current version */
        curr_version = atomic_fetch_add(&(class->curr_version), 1);

        //next_version = atomic_load(&(class->next_version));
        assert(curr_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(class->curr_version)) > atomic_load(&(H5P_mt_g.max_class_version_number))) {
            atomic_store(&(H5P_mt_g.max_class_version_number), atomic_load(&(class->curr_version)));
        }
    }

    /* If the thrd_count was incremented, decremented it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(class)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_delete_prop__class() */

/****************************************************************************************
 * Function:    H5P__mt_delete_prop__list
 *
 * Purpose:     Sets the delete_version of a property (H5P_mt_prop_t) in a property list
 *              (H5P_mt_list_t).
 *
 *              H5P__mt_delete_prop__list() first gets the chksum of the property via the
 *              name parameter, and then increments the thread count of the class
 *              structure.
 *
 *              H5P__mt_enforce_serialization() is called to check if there is another
 *              thread modifying the LFSLL. If not then proceed, if there is this thread
 *              must wait it's turn.
 *
 *              NOTE: For multiple threads simultaneiously either modifying, inserting,
 *              or deleting a property in the class or list, there is an ordering that
 *              must be followed. See the comment description above H5P_mt_class_t in
 *              H5Ppkg_mt.h for more details.
 *
 *              Then iterate the lkup_tbl by calling H5P__mt_search_lkup_tbl(). If entry
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
 *              NOTE: The process for the callback may be changed if multithread testing
 *              finds that calling the callback causes problems if other threads are on
 *              a version in which that property is not deleted and errors occur due to
 *              the callback being performed.
 *
 *              Finally, list's current version is updated to the same next version
 *              that was set as the delete_version for the prop. And update stats and
 *              decrement the thrd_count of the structure this thread is in.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_delete_prop__list(H5P_mt_list_t *list, const char *name)
{
    H5P_mt_prop_t             *prev_prop = NULL;      /* Previous prop in the LFSLL */
    H5P_mt_prop_t             *prop      = NULL;      /* Target prop found in the list */
    H5P_mt_prop_t             *pl_head;               /* Head of the LFSLL */
    H5P_mt_prop_value_t        value;                 /* Value of the target prop */
    H5P_mt_list_table_entry_t *entry;                 /* Entry in a list's lkup_tbl */
    H5P_mt_list_prop_ref_t     curr;                  /* Curr field of the entry */
    uint64_t                   curr_version;          /* Current version of the list */
    uint64_t                   next_version;          /* Next version of the list */
    bool                       done          = FALSE; /* Flag to exit loops to set atomics */
    bool                       inc_thrd_flag = FALSE; /* Flag to dec thrd count */
    bool                       base_flag     = FALSE; /* Flag if entry's base is target prop */
    int64_t                    chksum;                /* chksum of the target prop */
    uint32_t                   deletes     = 0;       /* Tracks number of deletes */
    uint32_t                   visited     = 0;       /* Tracks number of nodes visited */
    uint32_t                   thrd_cols   = 0;       /* Tracks number of thread cols */
    uint64_t                   avg_visited = 0;       /* Stats variable */
    uint64_t                   num_calls   = 0;       /* Stats variable */
    bool                       ver_updated = FALSE;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(name);
    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

    chksum = H5_checksum_metadata(name, strlen(name), 0);

    /* update stats */
    atomic_fetch_add(&(list->H5P__delete_prop__list__num_calls), 1);

    /* Increment thread count */
    if (H5P__inc_thrd_count(list) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Couldn't increment class's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    curr_version = atomic_load(&(list->curr_version));
    next_version = atomic_fetch_add(&(list->next_version), 1);
    ver_updated = TRUE;

    /* Ensure another thread isn't modifing the LFSLL */
    if ((curr_version + 1) < next_version) {
        if (0 == (curr_version = H5P__mt_enforce_serialization(list, curr_version, next_version))) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }

        curr_version = atomic_load(&(list->curr_version));
    }

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

/**
 * NOTE: May have to change how del callbacks are handled,
 * if when testing multithread this causes problems.
 */
#if 1
    /* If the prop has a del callback, call it */
    if (prop->del) {
        value = atomic_load(&(prop->value));

        if ((*(prop->del))(list->plist_id, prop->name, value.size, value.ptr) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTFREE, FAIL, "can't release property value");
        }
    }
#endif

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
        num_calls   = atomic_load(&(list->num_deletes_from_lfsll));

        avg_visited = H5P__calc_avg_visited(avg_visited, num_calls, visited);

        atomic_store(&(list->set_delete__avg_nodes_visited), avg_visited);
    }


done:

    /* Cleanup if error occurred */
    if ( ver_updated )
    {
        /* Update the list's current version */
        curr_version = atomic_fetch_add(&(list->curr_version), 1);

        //next_version = atomic_load(&(list->next_version));
        assert(curr_version + 1 == next_version);

        /* update stats */
        if (atomic_load(&(list->curr_version)) > atomic_load(&(H5P_mt_g.max_list_version_number))) {
            atomic_store(&(H5P_mt_g.max_list_version_number), atomic_load(&(list->curr_version)));
        }
    }

    /* If the thrd_count was incremented, decremented it */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(list)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_delete_prop__list() */



/****************************************************************************************
 * Function:    H5P__mt_search__class
 *
 * Purpose:     Searches a property list class (H5P_mt_class_t) for a property
 *
 *              Given the name of a property from the parameter name, the chksum of the
 *              property is calculated and then the thread count of the class structure
 *              is incremented.
 *
 *              The class's LFSLL is then searched for the target property by calling
 *              H5P__mt_search_lfsll(). If the property is found the most current version
 *              is returned after decrementing the class's thread count.
 *
 *              NOTE: This function currently only returns the most current version of
 *              the property being searched for. However, if in the future we need to
 *              search for a specific version then H5P__mt_entry_find_version() will need
 *              to be called, or the function H5P__find_mod_point() instead of
 *              H5P__mt_search_lfsll(). Regardless of which function a version parameter
 *              will need to be added.
 *
 * Return:      Success: Returns a pointer to the most recent version of the property.
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__mt_search__class(H5P_mt_class_t *class, const char *name, uint64_t version)
{
    H5P_mt_prop_t *prop = NULL;      /* Prop being searched for */
    H5P_mt_prop_t *pl_head;          /* Head of the LFSLL */
    //uint64_t       curr_version = 0; /* Current version of list or class */
    int64_t        chksum;           /* Chksum for the prop from name */
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
    
    if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG )
    {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, "Class is invalid");
    }

    chksum = H5_checksum_metadata(name, strlen(name), 0);

    /* update stats */
    atomic_fetch_add(&(class->H5P__search_prop__class__num_calls), 1);

    /* Increment thread count */
    if (H5P__inc_thrd_count(class) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, "Couldn't increment class's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /** TODO: add #if for debuggin */

    /**
     * stat for tracking number of searches that occur
     * while an insert or delete are taking place
     */
    uint64_t class_version = atomic_load(&(class->curr_version));
    uint64_t next_version  = atomic_load(&(class->next_version));

    if ((class_version + 1) != next_version) {
        atomic_fetch_add(&(H5P_mt_g.num_searches_while_an_op_occurs_class), 1);
    }

    /** 
     * TODO: need to add function that tries to get version from context here.
     */

    pl_head = class->pl_head;

    /* Search the LFSLL for the target prop */
    prop = H5P__mt_search_lfsll(pl_head, chksum, name, version, &visited, &chksum_cols);
    
    /* If target prop exists in the class */
    if ( prop )
    {
        if ( 0 < atomic_load(&(prop->delete_version)) &&
             atomic_load(&(prop->delete_version)) <= version ) 
        {
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
    num_calls   = atomic_load(&(class->H5P__search_prop__class__num_calls));

    avg_visited = H5P__calc_avg_visited(avg_visited, num_calls, visited);

    atomic_store(&(class->search_class__avg_nodes_visited), avg_visited);

    if ( prop )
    {
        atomic_fetch_add(&(class->num_search_class__success), 1);
    }
    else
    {
        atomic_fetch_add(&(H5P_mt_g.num_searches_classes_prop_not_found), 1);
    }

    atomic_fetch_add(&(H5P_mt_g.num_searches_classes), 1);

    ret_value = prop;

done:

    /* If the parent's thrd_count was incremented, it must be decremented */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(class)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_search__class() */

/****************************************************************************************
 * Function:    H5P__mt_search__list
 *
 * Purpose:     Searches a property list (H5P_mt_list_t) for a property
 *
 *              Given the name of a propert from the parameter name, the chksum of the
 *              property is calculated and then the thread count of the list structure
 *              is incremented.
 *
 *              The list's lkup_tbl is searched first. If the an entry with the matching
 *              chksum is found the most current version of the property is returned
 *              after decrementing the list's thread count.
 *
 *              If the property is not in the lkup_tbl the list's LFSLL is searched by
 *              calling H5P__mt_search_lfsll(). If the property is found the most current
 *              version is returned after decrementing the list's thread count.
 *
 *              NOTE: This function currently only returns the most current version of
 *              the property being searched for. If H5P__mt_search_lkup_tbl() returns an
 *              entry, we do not call H5P__mt_entry_find_version() and instead return the
 *              most current version. Same when searching the lfsll, instead of calling
 *              H5P__find_mod_point(), this function calls H5P__mt_search_lfsll() to only
 *              return the most recent version of the property being searched for.
 *              However, if in the future we need to search for a specific version then
 *              H5P__mt_entry_find_version() will need to get called.
 *
 * Return:      Success: Returns a pointer to the most recent version of the property.
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__mt_search__list(H5P_mt_list_t *list, const char *name, uint64_t version)
{
    H5P_mt_prop_t             *prop = NULL;      /* Prop being searched for */
    H5P_mt_prop_t             *pl_head;          /* Head of the LFSLL */
    H5P_mt_list_table_entry_t *entry;            /* Entry in a list's lkup_tbl */
    //H5P_mt_list_prop_ref_t     curr;             /* Curr struct field for list's entry */
    //H5P_mt_list_prop_ref_t     base;             /* Base struct field for list's entry */
    uint64_t                   curr_version;
    uint64_t                   next_version;
    //uint64_t                   create_version;
    uint64_t                   visited      = 0;
    int64_t                    chksum;                /* Chksum for the prop from name */
    bool                       done          = FALSE; /* Flag to exit a loop to set atomics */
    bool                       inc_thrd_flag = FALSE; /* Flag to dec thrd count of struct */
    bool                       chksum_cols   = FALSE;
    bool                       base_flag     = FALSE;

    H5P_mt_prop_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    assert(name);
    assert(list);
#if 1 /* debug */

    if ( atomic_load(&(list->tag)) != H5P_MT_LIST_TAG )
    {
        fprintf(stderr, "\nList tag is NOT valid\n");
        return NULL;
    }

#endif
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

    chksum = H5_checksum_metadata(name, strlen(name), 0);

    /* update stats */
    atomic_fetch_add(&(list->H5P__search_prop__list__num_calls), 1);

    /* Increment thread count */
    if (H5P__inc_thrd_count(list) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, "Couldn't increment class's thread count.");
    }
    else {
        inc_thrd_flag = TRUE;
    }

    /** TODO: add #if for debuggin */

    /**
     * stat for tracking number of searches that occur
     * while an insert or delete are taking place
     */
    curr_version = atomic_load(&(list->curr_version));
    next_version = atomic_load(&(list->next_version));

    if ((curr_version + 1) != next_version) {
        atomic_fetch_add(&(H5P_mt_g.num_searches_while_an_op_occurs_list), 1);
    }

    /** 
     * TODO: need to add function that tries to get version from context here.
     */

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
        if ( prop ) 
        {    
            if ( base_flag )
            {
                atomic_fetch_add(&(list->num_search_list__found_base), 1);
            }
            else
            {
                atomic_fetch_add(&(list->num_search_list__found_curr), 1);
            }                
            done = TRUE;
        }
        else {
            atomic_fetch_add(&(list->num_target_prop_found_but_deleted), 1);

            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, "Property is marked deleted.");
        }

#if 0
        curr = atomic_load(&(entry->curr));

        /* If curr.ptr isn't NULL, then find the correct version */
        if (curr.ptr) {
            prop = curr.ptr;

            /* Ensure property isn't already deleted */
            if ((0 == atomic_load(&(prop->delete_version))) ||
                (atomic_load(&(prop->delete_version)) > version)) {
                atomic_fetch_add(&(list->num_search_list__found_curr), 1);

                done = TRUE;
            }
            else {
                atomic_fetch_add(&(list->num_target_prop_found_but_deleted), 1);

                HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, "Property is marked deleted.");
            }
        }
        else /* If curr.ptr is NULL, most current version is base */
        {
            base = atomic_load(&(entry->base));
            if (base.ptr) {
                /* If base_delete_version isn't set return base.ptr */
                if (entry->base_delete_version == 0 || entry->base_delete_version > curr_version) {
                    prop = base.ptr;

                    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

                    /* update stats */
                    atomic_fetch_add(&(list->num_search_list__found_base), 1);

                    done = TRUE;
                }
                else {
                    atomic_fetch_add(&(list->num_target_prop_found_but_deleted), 1);
                    HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, "Property is marked deleted.");
                }
            }
            /**
             * Base may be NULL if list was copied and the
             * base was deleted from the original
             */
            else {
                prop = NULL;

                done = TRUE;
            }

        } /* end else */
#endif

    } /* end if ( entry ) */

    /* If not in the lkup_tbl search the LFSLL */
    if (!done) {
        /* Search the LFSLL for the target prop */
        prop = H5P__mt_search_lfsll(pl_head, chksum, name, version, &visited, &chksum_cols);
        
        /* If the target prop exists in the LFSLL */
        if ( prop )
        {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

            if ( 0 < atomic_load(&(prop->delete_version)) &&
                 atomic_load(&(prop->delete_version)) <= version) 
            {
                HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, "Property is marked deleted.");
            }
        }

        /* If chksum_cols is TRUE update stats */
        if (chksum_cols) {
            atomic_fetch_add(&(list->num_search_chksum_cols), 1);
        }

        done = TRUE;
    }

#if 0 /** TODO:This needs double checked when actually implement version from context */
    
    create_version = atomic_load(&(prop->create_version));

    /* Ensure the prop version is valid for the context version */
    if ( create_version > cx_version )
    {

    }


#endif

    ret_value = prop;

done:

    /* update stats */
    if ( prop )
    {
        atomic_fetch_add(&(list->num_search_list__success), 1);
    }
    else
    {
        atomic_fetch_add(&(H5P_mt_g.num_searches_lists_prop_not_found), 1);
    }

    atomic_fetch_add(&(H5P_mt_g.num_searches_lists), 1);

    /* If the parent's thrd_count was incremented, it must be decremented */
    if (inc_thrd_flag) {
        if (0 > H5P__dec_thrd_count(list)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_search__list() */

/****************************************************************************************
 * Function:    H5P__mt_search_lkup_tbl
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
 *                       or NULL if there is not a next valid.
 *
 *              Failure: Can not fail
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
    else if ((delete_ver > 0) && (delete_ver <= version)) {
        ret_value = -1;
    }
    /* If TRUE this property is not valid, but another version might be */
    else {
        ret_value = 1;
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__is_valid() */

/**
 * NOTE: After some changes to other functions worked, only one function called this one,
 * and it wound up being less lines of code to do the work in the other function instead
 * of calling this one. Keeping it here for now in case that changes.
 */
#if 0
/****************************************************************************************
 * Function:    H5P__mt_compare_prop()
 * 
 * Purpose:     Compares two properties to determine if they are equal or which one 
 *              should come first in a sorted LFSLL.
 * 
 * 
 * Return:      Success:  0 the props are the same, 
 *                       -1 prop1 comes before prop2, 
 *                        1 prop2 comes before prop1.
 * 
 *              Failure: -2
 ****************************************************************************************
 */
int32_t
H5P__mt_compare_prop(H5P_mt_prop_t *prop1, H5P_mt_prop_t *prop2)
{
    int32_t             str_cmp;
    uint64_t            prop1_create_ver;
    uint64_t            prop2_create_ver;

    int32_t             ret_value = 0;

    FUNC_ENTER_PACKAGE


    assert(prop1);
    assert(atomic_load(&(prop1->tag)) == H5P_MT_PROP_TAG);

    assert(prop2);
    assert(atomic_load(&(prop2->tag)) == H5P_MT_PROP_TAG);

    if ( prop1->chksum == prop2->chksum )
    {
        str_cmp = strcmp(prop1->name, prop2->name);

        if ( 0 == str_cmp )
        {
            prop1_create_ver = atomic_load(&(prop1->create_version));
            prop2_create_ver = atomic_load(&(prop2->create_version));

            if ( prop1_create_ver > prop2_create_ver )
                HGOTO_DONE(-1);
            else if ( prop1_create_ver < prop2_create_ver )
                HGOTO_DONE(1);
            
        } /* end if ( 0 == str_cmp ) */

        else
        {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, (int32_t)(-1), 
                        "Two different properties have the same checksum.");
        }

    } /* end if ( prop1->chksum == prop2->chksum ) */

    else if ( prop1->chksum > prop2->chksum )
    {
        HGOTO_DONE(1);
    }
    else if ( prop1->chksum < prop2->chksum )
    {
        HGOTO_DONE(-1);
    }


done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_compare_prop() */
#endif

/****************************************************************************************
 * Function:    H5P__mt_prop_cmp()
 *
 * Purpose:     Checks if the two properties are the same or different property
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
        if ((cmp_value = prop1->cmp(value1.ptr, value2.ptr, value1.size)) != 0)
            HGOTO_DONE(cmp_value);
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_prop_cmp() */



/****************************************************************************************
 * Function:    H5P__mt_cmp_class()
 *
 * Purpose:     Compares two classes (H5P_mt_class_t) and determines if they are equal.
 *
 *              First each class has it's thread count incremented, and the current
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
H5P__mt_cmp_class(H5P_mt_class_t *class1, uint64_t version1,
                  H5P_mt_class_t *class2, uint64_t version2)
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
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, (-1), "Couldn't increment class's thread count.");
    }

    inc_thrd_flag_1 = TRUE;

    if ((H5P__inc_thrd_count(class2)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, (-1), "Couldn't increment class's thread count.");
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
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, (-1), "Failure to decrement thrd_count.");
        }
    }
    if (inc_thrd_flag_2) {
        if (0 > H5P__dec_thrd_count(class2)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, (-1), "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_cmp_class() */

/****************************************************************************************
 * Function:    H5P__mt_cmp_list()
 *
 * Purpose:     Compares two lists (H5P_mt_list_t) and determines if they are equal.
 *
 *              First each list has it's thread count incremented, and the current
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
H5P__mt_cmp_list(H5P_mt_list_t *list1, uint64_t version1, 
                 H5P_mt_list_t *list2, uint64_t version2)
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
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, (-1), "Couldn't increment class's thread count.");
    }

    inc_thrd_flag_1 = TRUE;

    if ((H5P__inc_thrd_count(list2)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, (-1), "Couldn't increment class's thread count.");
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
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, (-1), "Failure to decrement thrd_count.");
        }
    }
    if (inc_thrd_flag_2) {
        if (0 > H5P__dec_thrd_count(list2)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, (-1), "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_cmp_list() */

/****************************************************************************************
 * Function:    H5P__mt_is_derived__class()
 *
 * Purpose:     Compares two classes (H5P_mt_class_t) and determines if a derived class
 *              correctly inherited all valid properties. 
 * 
 *              NOTE: This is function designed for use in testing.
 *
 *
 * Return:      Success: 0 means the derived class was derived from the provided parent
 *                       1 means the derived class was not derived from that parent.
 *
 *              Failure: -1
 ****************************************************************************************
 */
int32_t
H5P__mt_is_derived__class(H5P_mt_class_t *parent, uint64_t version1, 
                           H5P_mt_class_t *derived, uint64_t version2)
{
    H5P_mt_prop_t *prev_prop1 = NULL;
    H5P_mt_prop_t *prev_prop2 = NULL;
    H5P_mt_prop_t *valid_prop1;
    H5P_mt_prop_t *valid_prop2;
    bool           inc_thrd_flag_1 = FALSE;
    bool           inc_thrd_flag_2 = FALSE;

    int32_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(parent);
    assert(derived);
    assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_TAG ||
           atomic_load(&(parent->tag)) == H5P_MT_CLASS_INVALID_TAG);
    assert(atomic_load(&(derived->tag)) == H5P_MT_CLASS_TAG ||
           atomic_load(&(derived->tag)) == H5P_MT_CLASS_INVALID_TAG);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(parent)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, (-1), "Couldn't increment class's thread count.");
    }

    inc_thrd_flag_1 = TRUE;

    if ((H5P__inc_thrd_count(derived)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, (-1), "Couldn't increment class's thread count.");
    }

    inc_thrd_flag_2 = TRUE;

    /* Check fields */
    if (parent->id != derived->parent_id)
        HGOTO_DONE(1);
    if (parent != derived->parent_ptr)
        HGOTO_DONE(1);
    if ((atomic_load(&(parent->curr_version)) >= derived->parent_version))
        HGOTO_DONE(1);

    /* Compare the properties in the LFSLLs at the appropriate versions */
    prev_prop1 = parent->pl_head;
    prev_prop2 = derived->pl_head;

    assert(prev_prop1);
    assert(prev_prop2);
    assert(atomic_load(&(prev_prop1->tag)) == H5P_MT_PROP_TAG ||
           atomic_load(&(prev_prop1->tag)) == H5P_MT_PROP_VALID_ONFL_TAG);
    assert(atomic_load(&(prev_prop2->tag)) == H5P_MT_PROP_TAG ||
           atomic_load(&(prev_prop2->tag)) == H5P_MT_PROP_VALID_ONFL_TAG);

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
        if (0 > H5P__dec_thrd_count(parent)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, (-1), "Failure to decrement thrd_count.");
        }
    }
    if (inc_thrd_flag_2) {
        if (0 > H5P__dec_thrd_count(derived)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, (-1), "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_is_derived__class() */

/****************************************************************************************
 * Function:    H5P__mt_is_derived__list()
 *
 * Purpose:     Compares a derived list and a parent class and determines if the list was
 *              derived from that parent and inherited all valid properties correctly.
 * 
 *              NOTE: This is function designed for use in testing.
 *
 *
 * Return:      Success: 0 means the derived list was derived from the provided parent
 *                       1 means the derived list was not derived from that parent.
 *
 *              Failure: -1
 ****************************************************************************************
 */
int32_t
H5P__mt_is_derived__list(H5P_mt_class_t *parent, uint64_t version1, 
                          H5P_mt_list_t *derived, uint64_t version2)
{
    H5P_mt_list_table_entry_t *entry;
    H5P_mt_list_prop_ref_t     base;
    H5P_mt_list_prop_ref_t     curr;
    H5P_mt_prop_t             *prev_prop1;
    H5P_mt_prop_t             *valid_prop1;
    H5P_mt_prop_t             *valid_prop2;
    H5P_mt_prop_aptr_t         next;
    uint64_t                   prop_ver;
    int32_t                    cmp_result      = 0;
    bool                       inc_thrd_flag_1 = FALSE;
    bool                       inc_thrd_flag_2 = FALSE;
    bool                       done            = FALSE;

    int32_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(parent);
    assert(derived);
    assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_TAG ||
           atomic_load(&(parent->tag)) == H5P_MT_CLASS_INVALID_TAG);
    assert(atomic_load(&(derived->tag)) == H5P_MT_LIST_TAG ||
           atomic_load(&(derived->tag)) == H5P_MT_LIST_INVALID_TAG);

    /* Increment thread count */
    if ((H5P__inc_thrd_count(parent)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, (-1), "Couldn't increment class's thread count.");
    }

    inc_thrd_flag_1 = TRUE;

    if ((H5P__inc_thrd_count(derived)) < 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, (-1), "Couldn't increment class's thread count.");
    }

    inc_thrd_flag_2 = TRUE;

    /* Check fields */
    if (parent->id != derived->pclass_id)
        HGOTO_DONE(1);
    if (parent != derived->pclass_ptr)
        HGOTO_DONE(1);

    /* Check if the properties inherited are correct */

    prev_prop1 = parent->pl_head;
    assert(prev_prop1);
    assert(atomic_load(&(prev_prop1->tag)) == H5P_MT_PROP_TAG ||
           atomic_load(&(prev_prop1->tag)) == H5P_MT_PROP_VALID_ONFL_TAG);

    for (uint32_t idx = 0; idx < derived->nprops_inherited; idx++) {
        
        /** 
         * Get the next entry in the list's lkup_tbl which should correspond 
         * to the order of the parent's valid prop for the version.
         */
        valid_prop1 = H5P__get_next_valid_prop(prev_prop1, version1, NULL);
        entry = &derived->lkup_tbl[idx];

        /* Compare chksum, and name */
        if (valid_prop1->chksum != entry->chksum)
            HGOTO_DONE(1);
        if ((cmp_result = strcmp(valid_prop1->name, entry->name)) != 0)
            HGOTO_DONE(1);

        base = atomic_load(&(entry->base));
        curr = atomic_load(&(entry->curr));

        /** 
         * NOTE: if base.ver != 1, then this list was created as a copy from
         * another list and at the version of the original list this copy was
         * created from, the curr was the most recent ver.
         */
        done = FALSE;
        if ( base.ver == 1 )
        {
            /**
             * NOTE: if base.ptr is NULL then either this list is a copy of another list
             * and the base.ptr was not needed, or this property has a create callback.
             */
            if ( base.ptr )
            {
                if ( base.ptr != valid_prop1 )
                {
                    HGOTO_DONE(1);
                }
                else
                {
                    done = TRUE;
                }
            }
        }
        if ( ! done )
        {
            if ( curr.ptr )
            {
                valid_prop2 = curr.ptr;
                prop_ver = atomic_load(&(valid_prop2->create_version));

                while ( prop_ver > version2 )
                {
                    next = atomic_load(&(valid_prop2->next));
                    valid_prop2 = next.ptr;

                    if ( valid_prop2->chksum != valid_prop1->chksum )
                    {
                        HGOTO_DONE(1);
                    }
                }

                /* Compare the two properties */
                if (0 != H5P__mt_prop_cmp(valid_prop1, valid_prop2))
                    HGOTO_DONE(1);

            }
        }

    } /* end for ( uint32_t idx = 0; idx < list1->nprops_inherited; idx++ ) */


done:

    /* If inc_thrd_flags are TRUE decrement the thrd.count for that list */
    if (inc_thrd_flag_1) {
        if (0 > H5P__dec_thrd_count(parent)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, (-1), "Failure to decrement thrd_count.");
        }
    }
    if (inc_thrd_flag_2) {
        if (0 > H5P__dec_thrd_count(derived)) {
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, (-1), "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_is_derived__list() */

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


#if 0 /** NOTE: Didn't actually need this, just put what this does in H5P_get() */
/****************************************************************************************
 * Function:    H5P__mt_get_value()
 *
 * Purpose:     Gets the value of a specified property and copies the value into a
 *              provided buffer.
 *
 *              H5P__mt_search__list() is called to return the most recent version of the
 *              property with the matching chksum as the one provided, from the list
 *              passed in.
 *
 *              The property returned by H5P__mt_search__list() is checked if it has a
 *              get callback. If it does then a tmp buffer is allocated to store the
 *              value in case the callback fails, then the callback is called to get the
 *              value. If the property does not have a get callback, the value is simply
 *              copied into the supplied buffer.
 *
 * Return:      SUCCEED/FAIL

 ****************************************************************************************
 */
herr_t
H5P__mt_get_value(H5P_mt_list_t *list, const char *name, void *value_ptr)
{
    H5P_mt_prop_t      *prop;
    H5P_mt_prop_value_t prop_value;
    void               *tmp_value_buf = NULL;
    uint64_t            version;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_PROP_TAG);
    assert(value_ptr);



    prop = H5P__mt_search__list(list, name);

    if (NULL == prop) {
        HGOTO_ERROR(H5E_PLIST, H5E_CANTFIND, FAIL, "Target prop isn't in list.");
    }

    prop_value = atomic_load(&(prop->value));

    if (prop_value.size == 0) {
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "property has zero size");
    }

    /* If the prop has the get callback, call it */
    if (prop->get) {
        /* Make a copy of the current value, in case the callback fails */
        if (NULL == (tmp_value_buf = H5MM_malloc(prop_value.size))) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "memory allocation for tmp prop value failed");
        }

        H5MM_memcpy(tmp_value_buf, prop_value.ptr, prop_value.size);

        /* Call user's callback */
        if ((*(prop->get))(list->plist_id, name, prop_value.size, tmp_value_buf) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "can't set property value");
        }

        /* Copy new [possibly unchanged] value into return value */
        H5MM_memcpy(value_ptr, tmp_value_buf, prop_value.size);

    } /* end if ( prop->get ) */

    /* No callback, just copy value */
    else {
        H5MM_memcpy(value_ptr, prop_value.ptr, prop_value.size);
    }

done:

    /* Free the tmp value buf */
    if (tmp_value_buf)
        H5MM_xfree(tmp_value_buf);

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_get_value() */
#endif

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
 *              First the functions calls H5P__mt_enforce_serialization() to ensure there
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
 * Return:      SUCCEED/FAIL

 ****************************************************************************************
 */
herr_t
H5P__mt_encode(H5P_mt_list_t *list, uint64_t version, void *buf, size_t *nalloc)
{
    H5P_mt_list_table_entry_t *entry;                        /* entry in the lkup_tbl */
    H5P_mt_prop_t             *valid_prop;                   /* prop to try encoding next */
    H5P_mt_prop_t             *prev_prop;                    /* previous valid_prop in the lfsll */
    uint8_t                   *p           = (uint8_t *)buf; /* tmp pointer to encode buffer */
    size_t                     encode_size = 0;              /* size of buf needed to encode properties */
    bool                       encode      = TRUE;           /* bool for if the list should be encoded */
    bool                       base_flag   = FALSE;
    uint64_t                   curr_version;
    uint64_t                   next_version;
    size_t                     nprops;           /* total # of props in lkup_tbl + lfsll */
    size_t                     nprops_inherited; /* Number of entries in the lkup_tbl */
    size_t                     log_pl_len;       /* Number of props in the lfsll */
    uint64_t                   prop_count = 0;
    uint32_t                   idx; /* Index of the lkup_tbl */

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);
    assert(atomic_load(&(list->curr_version)) >= version);

    curr_version = atomic_load(&(list->curr_version));
    next_version = atomic_fetch_add(&(list->next_version), 1);

    if ((curr_version + 1) < next_version) {
        if ((curr_version = H5P__mt_enforce_serialization(list, curr_version, next_version)) == 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Error with returned current version.");
        }
    }

    nprops           = atomic_load(&(list->nprops));
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

    log_pl_len = atomic_load(&(list->log_pl_len));

    do {
        /* Gets the next valid prop or NULL if there isn't another valid prop */
        valid_prop = H5P__get_next_valid_prop(prev_prop, version, NULL);

        if (valid_prop) {
            prop_count++;

            /* If in_lkup_tbl is TRUE, the property was already encoded */
            if (!valid_prop->in_lkup_tbl) {
                if (H5P__mt_encode_prop(valid_prop, encode, &encode_size, &p) < 0) {
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL,
                                "Error while encoding properties in the lfsll");
                }
            }
        }

    } while (valid_prop);

    assert(prop_count == log_pl_len);

    nprops = atomic_load(&(list->nprops));

    assert(nprops == (idx + prop_count));

    /* Encode a terminator for the list of properties */
    if (encode) {
        *p++ = 0;
    }

    encode_size++;

    /* Set the size of the buffer needed to encode the property list */
    *nalloc = encode_size;

done:

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
        encode_size += name_len;

        value_size = 0;

        /* If not NULL, encode the property value */
        if ((prop->encode)(value.ptr, (void **)&p, &value_size) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL, "property encoding routine failed");
        }

        encode_size += value_size;

    } /* end if ( prop-> encode ) */

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_encode_prop() */

/****************************************************************************************
 * Function:    H5P__mt_get_version()
 *
 * Purpose:     Will eventually get the version of a list or class that is stored in the
 *              CX.
 *
 * Return:      Success: the version of a list or class that is stored in the CX for that
 *                       list or class.
 *
 *              Failure: 0

 ****************************************************************************************
 */
uint64_t
H5P__mt_get_version(void *param)
{
    uint32_t tag;
    H5P_mt_class_t *class        = NULL;
    H5P_mt_list_t *list          = NULL;
    bool           inc_thrd_flag = FALSE;

    uint64_t ret_value = 0;

    FUNC_ENTER_PACKAGE

    tag = *(uint32_t *)param;

    /* If param is a class */
    if (tag == H5P_MT_CLASS_TAG) {
        class = (H5P_mt_class_t *)param;

        assert(class);
        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);

        /** TODO: assert(class->ref_count.deleted == FALSE) */

        /* update stats */
        /** TODO: add stats for number of times version is gotten */

        /* Increment thread count */
        if ((H5P__inc_thrd_count(class)) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, 0, "Couldn't increment parent's thread count.");
        }
        else {
            inc_thrd_flag = TRUE;
        }

        /** TODO: replace this with function that gets version from context */
        ret_value = atomic_load(&(class->curr_version));
    }
    /* If param is a list */
    else if (tag == H5P_MT_LIST_TAG) {
        list = (H5P_mt_list_t *)param;

        assert(list);
        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

        /* update stats */
        /** TODO: add stats for number of times version is gotten */

        /* Increment thread count */
        if ((H5P__inc_thrd_count(list)) < 0) {
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, 0, "Couldn't increment parent's thread count.");
        }
        else {
            inc_thrd_flag = TRUE;
        }

        /** TODO: replace this with function that gets version from context */
        ret_value = atomic_load(&(list->curr_version));
    }
    else {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, 0, "Type passed in wasn't a class or list.");
    }

done:

    /* If the parent's thrd_count was incremented, it must be decremented */
    if (inc_thrd_flag) {
        if (class) {
            if (0 > H5P__dec_thrd_count(class)) {
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, 0, "Failure to decrement thrd_count.");
            }
        }
        else {
            if (0 > H5P__dec_thrd_count(list)) {
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, 0, "Failure to decrement thrd_count.");
            }
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_get_version() */

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

    /** TODO: turn this into a atomic_compare_strong() for list's */
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

    /**
     * If this is the first prop added to the prop free list, have the
     * head pointer point to new tail as well.
     */
    fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));

    if (!fl_head.ptr) {
        done = FALSE;

        do {
            fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));

            fl_update.ptr = prop;

            if (!atomic_compare_exchange_strong(&(H5P_mt_g.prop_fl_head), &fl_head, fl_update)) {
                /* failed, updated stats and try again */
                atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update_cols), 1);

                /* assert is to not get stuck in an infinite loop while testing */
                assert(H5P_MT_ASSERT_FAIL);
            }
            else {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update), 1);

                done = TRUE;
            }

        } while (!done);

    } /* end if ( ! fl_head.ptr ) */

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
 * Purpose:     Frees a property's name and the property itself.
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
 * Function:    H5P__mt_close_class
 *
 * Purpose:     Multithread safe function to close a property list class.
 *
 *              First check if the class is already marked deleted, and if it isn't mark
 *              it deleted.
 *
 *              Then check if it's marked as closing and if it already is, throw an
 *              error, because it shouldn't be marked as closing until it is actually
 *              being closed.
 *
 *              Check to see if the class is ready to be closed, by checking the ref
 *              counts for derived classes and lists. If one or both ref counts are
 *              not at zero, then we must exit and when a ref count gets decremented
 *              it will check again.
 *
 *              If both ref counts are zero, we will now mark it as closing. This will
 *              prevent more threads from accessing the struct so it can close when any
 *              other current threads in the struct exit. We then check if it's opening
 *              or contains other threads. If either are true we sleep and check again.
 *
 *              When opening is FALSE and there are no other threads in the struct, we
 *              decrement the parent's ref count for derived classes and then add this
 *              struct to the tail of the class free list.
 *
 *              We do this so if in the future we need to allocate another class struct,
 *              we can instead reuse this existing one. The struct will be cleared of
 *              any data before being reallocated, but we leave the data as is for now as
 *              a safety precaution, in case some thread still needs to get access it.
 *
 *              Lastly we set the closing flag back to FALSE, now that the class is
 *              finished closing and is on the free list.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_close_class(H5P_mt_class_t *class)
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

    FUNC_ENTER_PACKAGE

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
                //assert(H5P_MT_ASSERT_FAIL);

                sleep(1);
            }
            /* If there are any other threads in the struct, wait for them to drain out */
            else if (local_thrd.count > 0) {
                /** NOTE: This assert is to prevent an infinite loop while testing */
                //assert(H5P_MT_ASSERT_FAIL);

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
                HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Couldn't increment parent's thread count.");
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
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Failure to decrement thrd_count.");
        }

#if 0 /* testing increment parent's id's ref_count */

        ref_count = atomic_load(&(parent->ref_count));

        /**
         * If the parent's ref counts for derived classes and lists are zero,
         * and it's marked deleted, then call the close function on it.
         */
        if (ref_count.pl == 0 && ref_count.plc == 0 && ref_count.deleted) {
            if (0 > H5P__mt_close_class(parent)) {
                HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Failed to close the property list class.");
            }
        }
#else 

        ref_count = atomic_load(&(parent->ref_count));

        if ( ref_count.deleted == FALSE )
        {
            if ( 0 > H5I_dec_ref(atomic_load(&(parent->id))) )
            {
                assert(FALSE);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, 
                            "unable to decrement parent's ID ref_count in index");
            }
        }

#endif

    } /* end if ( parent != NULL && inc_thrd_flag ) */

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_close_class() */

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

    /** TODO: turn this into an atomic_compare_strong() */
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

        H5P__mt_close_prop(first_prop);

        atomic_fetch_sub(&(class->phys_pl_len), 1);

    } /* end for() */

    atomic_store(&(class->log_pl_len), 0);

    /* Ensure the LFSLL is empty, then free the class */
    first_prop = class->pl_head;
    assert(!first_prop);

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
 * Function:    H5P__mt_close_list
 *
 * Purpose:     Multithread safe function to close a property list
 *
 *              First we check if the list is already marked closing and if it is throw
 *              an error, because it shouldn't be marked closing until it is actually
 *              closing.
 *
 *              Then we set closing to TRUE, and check if opening is TRUE, or if there
 *              are other threads in this struct, and if so we loop and check again.
 *
 *              When opening is FALSE and there are no other threads in the struct, we
 *              check if the property list initialization function completed. If so call
 *              the close callback (cb) on the parent class.
 *
 *              Next iterate the lkup_tbl and if base.ptr is not NULL and the property
 *              has the close cb, we call it. Then iterate the LFSLL and do the same
 *              thing (if the property has the close cb call it).
 *
 *              Decrement the pl ref count of this lists's parent, then add this list to
 *              the tail of the list free list instead of freeing the struct.
 *
 *              We do this so if in the future we need to allocate another list struct,
 *              we can instead reuse this existing one. The struct will be cleared of
 *              any data before being reallocated, but we leave the data as is for now as
 *              a safety precaution.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_close_list(H5P_mt_list_t *list)
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

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

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
            //assert(H5P_MT_ASSERT_FAIL);

            sleep(1);
        }
        /* If there are any other threads in the struct, wait for them to drain out */
        else if (local_thrd.count > 0) {
            /** NOTE: This assert is to prevent an infinite loop while testing */
            //assert(H5P_MT_ASSERT_FAIL);

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
                HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Couldn't increment parent's thread count.");
            }

            inc_thrd_flag = TRUE;

            /* class close callback */
            if (parent->close_func) {
                (parent->close_func)(list->plist_id, parent->close_data);
            }

            /* Decrement the thrd count */
            if (0 > H5P__dec_thrd_count(parent)) {
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Failure to decrement parent's thrd_count.");
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
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Couldn't increment parent's thread count.");
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

                (valid_prop->close)(valid_prop->name, prop_value.size, prop_value.ptr);
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
                        (base_prop->close)(base_prop->name, prop_value.size, prop_value.ptr);
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
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Failure to decrement thrd_count.");
        }

#if 0 /* testing increment parent's id's ref_count */

        ref_count = atomic_load(&(parent->ref_count));

        /**
         * If the parent's ref counts for derived classes and lists are zero,
         * and it's marked deleted, then call the close function on it.
         */
        if (ref_count.pl == 0 && ref_count.plc == 0 && ref_count.deleted) {
            if (0 > H5P__mt_close_class(parent)) {
                HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Failed to close the property list class.");
            }
        }
#else 


    ref_count = atomic_load(&(parent->ref_count));

    if ( ref_count.deleted == FALSE )
    {
        if ( 0 > H5I_dec_ref(atomic_load(&(parent->id))) )
        {
            assert(FALSE);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, 
                        "unable to decrement parent's ID ref_count in index");
        }
    }

#endif
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_close_list() */

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
        }
        else {
            atomic_fetch_add(&(list->num_wait_for_curr_version_to_inc), 1);
            curr_version = atomic_load(&(list->curr_version));
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

#if 1

            /**
             * This is for testing that this MT safty net gets trigged while running
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

    /* Ensure the class isn't opening or closing and increment thread count */
    do {
        if (class) {
            thrd = atomic_load(&(class->thrd));
        }
        else {
            thrd = atomic_load(&(list->thrd));
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
    atomic_init(&(H5P_mt_g.H5P__mt_create_class__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__mt_copy_class__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_class_structs_allocated_from_heap), 0ULL);
    atomic_init(&(H5P_mt_g.num_class_structs_allocated_from_fl), 0ULL);

    /* stats for creating or copying lists */
    atomic_init(&(H5P_mt_g.H5P__mt_create_list__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_list_structs_allocated_from_heap), 0ULL);
    atomic_init(&(H5P_mt_g.num_list_structs_allocated_from_fl), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__init_lkup_tbl__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.H5P__init_lkup_tbl_copy__num_calls), 0ULL);

    /* stats for creating props */
    atomic_init(&(H5P_mt_g.H5P__mt_create_prop__num_calls), 0ULL);
    atomic_init(&(H5P_mt_g.num_prop_structs_allocated_from_heap), 0ULL);
    atomic_init(&(H5P_mt_g.num_prop_structs_allocated_from_fl), 0ULL);

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

    /* Property chksum cols stats */
    atomic_init(&(H5P_mt_g.num_chksum_cols), 0ULL);

    /* H5P__mt_enforce_serialization stats */
    atomic_init(&(H5P_mt_g.H5P__mt_enforce_serialization__num_calls), 0ULL);

    /* stats for marking classes deleted or unmarking classes as deleted */
    atomic_init(&(H5P_mt_g.close_class_but_pl_not_zero), 0ULL);
    atomic_init(&(H5P_mt_g.close_class_but_plc_not_zero), 0ULL);
    atomic_init(&(H5P_mt_g.class_un_marked_as_deleted), 0ULL);

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
    atomic_store(&(H5P_mt_g.H5P__mt_create_class__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__mt_copy_class__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_class_structs_allocated_from_heap), 0ULL);
    atomic_store(&(H5P_mt_g.num_class_structs_allocated_from_fl), 0ULL);

    /* stats for creating or copying lists */
    atomic_store(&(H5P_mt_g.H5P__mt_create_list__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_list_structs_allocated_from_heap), 0ULL);
    atomic_store(&(H5P_mt_g.num_list_structs_allocated_from_fl), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__init_lkup_tbl__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.H5P__init_lkup_tbl_copy__num_calls), 0ULL);

    /* stats for creating props */
    atomic_store(&(H5P_mt_g.H5P__mt_create_prop__num_calls), 0ULL);
    atomic_store(&(H5P_mt_g.num_prop_structs_allocated_from_heap), 0ULL);
    atomic_store(&(H5P_mt_g.num_prop_structs_allocated_from_fl), 0ULL);

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

    /* Property chksum cols stats */
    atomic_store(&(H5P_mt_g.num_chksum_cols), 0ULL);

    /* H5P__mt_enforce_serialization stats */
    atomic_store(&(H5P_mt_g.H5P__mt_enforce_serialization__num_calls), 0ULL);

    /* stats for marking classes deleted or unmarking classes as deleted */
    atomic_store(&(H5P_mt_g.close_class_but_pl_not_zero), 0ULL);
    atomic_store(&(H5P_mt_g.close_class_but_plc_not_zero), 0ULL);
    atomic_store(&(H5P_mt_g.class_un_marked_as_deleted), 0ULL);

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
    atomic_init(&(class->H5P__insert_prop_class__num_calls), 0ULL);
    atomic_init(&(class->insert_max_nodes_visited), 0ULL);
    atomic_init(&(class->insert_avg_nodes_visited), 0ULL);
    atomic_init(&(class->num_insert_nodes_visited), 0ULL);
    atomic_init(&(class->num_insert_prop__cols), 0ULL);
    atomic_init(&(class->num_insert_prop__success), 0ULL);
    atomic_init(&(class->num_insert_prop__chksum_cols), 0ULL);

    /* H5P_mt_class_t set delete version stats */
    atomic_init(&(class->H5P__delete_prop__class__num_calls), 0ULL);
    atomic_init(&(class->set_delete__max_nodes_visited), 0ULL);
    atomic_init(&(class->set_delete__avg_nodes_visited), 0ULL);
    atomic_init(&(class->num_set_delete__nodes_visited), 0ULL);
    atomic_init(&(class->num_set_delete__cols), 0ULL);
    atomic_init(&(class->num_set_delete__success), 0ULL);
    atomic_init(&(class->num_set_delete_chksum_cols), 0ULL);

    /* H5P_mt_class_t search stats */
    atomic_init(&(class->H5P__search_prop__class__num_calls), 0ULL);
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
    atomic_store(&(class->H5P__insert_prop_class__num_calls), 0ULL);
    atomic_store(&(class->insert_max_nodes_visited), 0ULL);
    atomic_store(&(class->insert_avg_nodes_visited), 0ULL);
    atomic_store(&(class->num_insert_nodes_visited), 0ULL);
    atomic_store(&(class->num_insert_prop__cols), 0ULL);
    atomic_store(&(class->num_insert_prop__success), 0ULL);
    atomic_store(&(class->num_insert_prop__chksum_cols), 0ULL);

    /* H5P_mt_class_t set delete version stats */
    atomic_store(&(class->H5P__delete_prop__class__num_calls), 0ULL);
    atomic_store(&(class->set_delete__max_nodes_visited), 0ULL);
    atomic_store(&(class->set_delete__avg_nodes_visited), 0ULL);
    atomic_store(&(class->num_set_delete__nodes_visited), 0ULL);
    atomic_store(&(class->num_set_delete__cols), 0ULL);
    atomic_store(&(class->num_set_delete__success), 0ULL);
    atomic_store(&(class->num_set_delete_chksum_cols), 0ULL);

    /* H5P_mt_class_t search stats */
    atomic_store(&(class->H5P__search_prop__class__num_calls), 0ULL);
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
    atomic_init(&(list->H5P__insert_prop_list__num_calls), 0ULL);
    atomic_init(&(list->insert_max_nodes_visited), 0ULL);
    atomic_init(&(list->insert_avg_nodes_visited), 0ULL);
    atomic_init(&(list->num_insert_nodes_visited), 0ULL);
    atomic_init(&(list->num_insert_prop_cols), 0ULL);
    atomic_init(&(list->num_insert_prop_success), 0ULL);
    atomic_init(&(list->num_insert_update_entry_cols), 0ULL);
    atomic_init(&(list->num_insert_update_entry), 0ULL);
    atomic_init(&(list->num_insert_prop__chksum_cols), 0ULL);

    /* H5P_mt_list_t set delete version stats */
    atomic_init(&(list->H5P__delete_prop__list__num_calls), 0ULL);
    atomic_init(&(list->num_deletes_from_lfsll), 0ULL);
    atomic_init(&(list->set_delete__max_nodes_visited), 0ULL);
    atomic_init(&(list->set_delete__avg_nodes_visited), 0ULL);
    atomic_init(&(list->num_set_delete__nodes_visited), 0ULL);
    atomic_init(&(list->num_set_delete__cols), 0ULL);
    atomic_init(&(list->num_set_delete__success), 0ULL);
    atomic_init(&(list->num_set_delete_chksum_cols), 0ULL);
    atomic_init(&(list->num_set_delete__base_delete_version), 0ULL);
    atomic_init(&(list->num_set_delete__curr_entry), 0ULL);
    atomic_init(&(list->num_set_delete__older_curr), 0ULL);

    /* H5P_mt_list_t search stats */
    atomic_init(&(list->H5P__search_prop__list__num_calls), 0ULL);
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
    atomic_store(&(list->H5P__insert_prop_list__num_calls), 0ULL);
    atomic_store(&(list->insert_max_nodes_visited), 0ULL);
    atomic_store(&(list->insert_avg_nodes_visited), 0ULL);
    atomic_store(&(list->num_insert_nodes_visited), 0ULL);
    atomic_store(&(list->num_insert_prop_cols), 0ULL);
    atomic_store(&(list->num_insert_prop_success), 0ULL);
    atomic_store(&(list->num_insert_update_entry_cols), 0ULL);
    atomic_store(&(list->num_insert_update_entry), 0ULL);
    atomic_store(&(list->num_insert_prop__chksum_cols), 0ULL);

    /* H5P_mt_list_t set delete version stats */
    atomic_store(&(list->H5P__delete_prop__list__num_calls), 0ULL);
    atomic_store(&(list->num_deletes_from_lfsll), 0ULL);
    atomic_store(&(list->set_delete__max_nodes_visited), 0ULL);
    atomic_store(&(list->set_delete__avg_nodes_visited), 0ULL);
    atomic_store(&(list->num_set_delete__nodes_visited), 0ULL);
    atomic_store(&(list->num_set_delete__cols), 0ULL);
    atomic_store(&(list->num_set_delete__success), 0ULL);
    atomic_store(&(list->num_set_delete_chksum_cols), 0ULL);
    atomic_store(&(list->num_set_delete__base_delete_version), 0ULL);
    atomic_store(&(list->num_set_delete__curr_entry), 0ULL);
    atomic_store(&(list->num_set_delete__older_curr), 0ULL);

    /* H5P_mt_list_t search stats */
    atomic_store(&(list->H5P__search_prop__list__num_calls), 0ULL);
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
    atomic_store(&(list->num_inherited_with_create_cb), 0ULL);

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
    fprintf(file_ptr, "H5P_mt_g.H5P__mt_create_class__num_calls             = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__mt_create_class__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.H5P__mt_copy_class__num_calls               = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__mt_copy_class__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_class_structs_allocated_from_heap       = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_class_structs_allocated_from_heap))));
    fprintf(file_ptr, "H5P_mt_g.num_class_structs_allocated_from_fl         = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_class_structs_allocated_from_fl))));

    /* stats for creating or copying lists */
    fprintf(file_ptr, "H5P_mt_g.H5P__mt_create_list__num_calls              = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__mt_create_list__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_list_structs_allocated_from_heap        = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_list_structs_allocated_from_heap))));
    fprintf(file_ptr, "H5P_mt_g.num_list_structs_allocated_from_fl          = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_list_structs_allocated_from_fl))));
    fprintf(file_ptr, "H5P_mt_g.H5P__init_lkup_tbl__num_calls               = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__init_lkup_tbl__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.H5P__init_lkup_tbl_copy__num_calls          = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__init_lkup_tbl_copy__num_calls))));

    /* stats for creating props */
    fprintf(file_ptr, "H5P_mt_g.H5P__mt_create_prop__num_calls              = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__mt_create_prop__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_prop_structs_allocated_from_heap        = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_prop_structs_allocated_from_heap))));
    fprintf(file_ptr, "H5P_mt_g.num_prop_structs_allocated_from_fl          = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_prop_structs_allocated_from_fl))));

    /* stats for property inserts */
    fprintf(file_ptr, "H5P_mt_g.num_props_inserted_classes                  = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_inserted_classes))));
    fprintf(file_ptr, "H5P_mt_g.num_props_inserted_lists                    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_inserted_lists))));
    fprintf(file_ptr, "H5P_mt_g.H5P__init_lkup_tbl__num_calls               = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__mt_ins_or_mod_prop__lfsll_ins__num_calls))));
    fprintf(file_ptr, "H5P_mt_g.num_chksum_cols                             = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_chksum_cols))));

    /* stats for number of deletes */
    fprintf(file_ptr, "H5P_mt_g.num_props_deleted_classes                   = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_deleted_classes))));
    fprintf(file_ptr, "H5P_mt_g.num_props_deleted_classes_prop_not_found    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_deleted_classes_prop_not_found))));
    fprintf(file_ptr, "H5P_mt_g.num_props_deleted_classes_already_deleted   = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_deleted_classes_already_deleted))));
    fprintf(file_ptr, "H5P_mt_g.num_props_deleted_lists                     = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_deleted_lists))));
    fprintf(file_ptr, "H5P_mt_g.num_props_deleted_lists_prop_not_found    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_deleted_lists_prop_not_found))));
    fprintf(file_ptr, "H5P_mt_g.num_props_deleted_lists_already_deleted   = %lld\n",
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
    fprintf(file_ptr, "H5P_mt_g.num_searches_lists_prop_not_found         = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_searches_lists_prop_not_found))));
    fprintf(file_ptr, "H5P_mt_g.num_searches_while_an_op_occurs_list        = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.num_searches_while_an_op_occurs_list))));

    /* H5P__mt_enforce_serialization stats */
    fprintf(file_ptr, "H5P_mt_g.H5P__mt_enforce_serialization__num_calls    = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.H5P__mt_enforce_serialization__num_calls))));

    /* stats for marking classes deleted or unmarking classes as deleted */
    fprintf(file_ptr, "H5P_mt_g.close_class_but_pl_not_zero                 = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.close_class_but_pl_not_zero))));
    fprintf(file_ptr, "H5P_mt_g.close_class_but_plc_not_zero                = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.close_class_but_plc_not_zero))));
    fprintf(file_ptr, "H5P_mt_g.class_un_marked_as_deleted                  = %lld\n",
            (unsigned long long)(atomic_load(&(H5P_mt_g.class_un_marked_as_deleted))));

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

    fprintf(file_ptr, "class->H5P__insert_prop_setup__num_calls  = %lld\n",
            (unsigned long long)(atomic_load(&(class->H5P__insert_prop_class__num_calls))));
    fprintf(file_ptr, "class->insert_max_nodes_visited           = %lld\n",
            (unsigned long long)(atomic_load(&(class->insert_max_nodes_visited))));
    fprintf(file_ptr, "class->insert_avg_nodes_visited           = %lld\n",
            (unsigned long long)(atomic_load(&(class->insert_avg_nodes_visited))));
    fprintf(file_ptr, "class->num_insert_nodes_visited           = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_insert_nodes_visited))));
    fprintf(file_ptr, "class->num_insert_prop__cols              = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_insert_prop__cols))));
    fprintf(file_ptr, "class->num_insert_prop__success           = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_insert_prop__success))));
    fprintf(file_ptr, "class->num_insert_prop__chksum_cols       = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_insert_prop__chksum_cols))));

    fprintf(file_ptr, "class->H5P__delete_prop__class__num_calls = %lld\n",
            (unsigned long long)(atomic_load(&(class->H5P__delete_prop__class__num_calls))));
    fprintf(file_ptr, "class->set_delete_max_nodes_visited       = %lld\n",
            (unsigned long long)(atomic_load(&(class->set_delete__max_nodes_visited))));
    fprintf(file_ptr, "class->set_delete_avg_nodes_visited       = %lld\n",
            (unsigned long long)(atomic_load(&(class->set_delete__avg_nodes_visited))));
    fprintf(file_ptr, "class->num_set_delete_nodes_visited       = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_set_delete__nodes_visited))));
    fprintf(file_ptr, "class->num_set_delete_prop_cols           = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_set_delete__cols))));
    fprintf(file_ptr, "class->num_set_delete_prop_success        = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_set_delete__success))));
    fprintf(file_ptr, "class->num_set_delete_chksum_cols         = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_set_delete_chksum_cols))));

    fprintf(file_ptr, "class->H5P__search_prop__class__num_calls        = %lld\n",
            (unsigned long long)(atomic_load(&(class->H5P__search_prop__class__num_calls))));
    fprintf(file_ptr, "class->search_class__max_nodes_visited           = %lld\n",
            (unsigned long long)(atomic_load(&(class->search_class__max_nodes_visited))));
    fprintf(file_ptr, "class->search_class__avg_nodes_visited           = %lld\n",
            (unsigned long long)(atomic_load(&(class->search_class__avg_nodes_visited))));
    fprintf(file_ptr, "class->num_search_class__nodes_visited           = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_search_class__nodes_visited))));
    fprintf(file_ptr, "class->num_search_class__success                 = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_search_class__success))));
    fprintf(file_ptr, "class->num_search_chksum_cols                 = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_search_chksum_cols))));

    fprintf(file_ptr, "class->num_wait_for_curr_version_to_inc   = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_wait_for_curr_version_to_inc))));

    fprintf(file_ptr, "class->num_thrd_update_cols               = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_thrd_update_cols))));
    fprintf(file_ptr, "class->num_thrd_count_update              = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_thrd_count_update))));
    fprintf(file_ptr, "class->num_thrd_closing_flag_set          = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_thrd_closing_flag_set))));
    fprintf(file_ptr, "class->num_thrd_opening_flag_set          = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_thrd_opening_flag_set))));

    fprintf(file_ptr, "class->num_ref_count_cols                 = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_ref_count_cols))));
    fprintf(file_ptr, "class->num_ref_count_update               = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_ref_count_update))));
    fprintf(file_ptr, "class->num_ref_count_inc_while_deleted    = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_ref_count_inc_while_deleted))));
    fprintf(file_ptr, "class->num_ref_count_marked_deleted       = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_ref_count_marked_deleted))));
    fprintf(file_ptr, "class->num_ref_count_unmarked_deleted     = %lld\n",
            (unsigned long long)(atomic_load(&(class->num_ref_count_unmarked_deleted))));

    fprintf(file_ptr, "class->num_prop_ref_count_update          = %lld\n",
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

    fprintf(file_ptr, "list->H5P__insert_prop_setup__num_calls    = %lld\n",
            (unsigned long long)(atomic_load(&(list->H5P__insert_prop_list__num_calls))));
    fprintf(file_ptr, "list->insert_max_nodes_visited             = %lld\n",
            (unsigned long long)(atomic_load(&(list->insert_max_nodes_visited))));
    fprintf(file_ptr, "cllistass->insert_avg_nodes_visited        = %lld\n",
            (unsigned long long)(atomic_load(&(list->insert_avg_nodes_visited))));
    fprintf(file_ptr, "list->num_insert_nodes_visited             = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_insert_nodes_visited))));
    fprintf(file_ptr, "list->num_insert_prop_cols                 = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_insert_prop_cols))));
    fprintf(file_ptr, "list->num_insert_prop_success              = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_insert_prop_success))));
    fprintf(file_ptr, "list->num_insert_update_entry_cols         = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_insert_update_entry_cols))));
    fprintf(file_ptr, "list->num_insert_update_entry              = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_insert_update_entry))));
    fprintf(file_ptr, "list->num_insert_prop__chksum_cols         = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_insert_prop__chksum_cols))));

    fprintf(file_ptr, "list->H5P__delete_prop__list__num_calls   = %lld\n",
            (unsigned long long)(atomic_load(&(list->H5P__delete_prop__list__num_calls))));
    fprintf(file_ptr, "list->num_deletes_from_lfsll              = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_deletes_from_lfsll))));
    fprintf(file_ptr, "list->set_delete__max_nodes_visited       = %lld\n",
            (unsigned long long)(atomic_load(&(list->set_delete__max_nodes_visited))));
    fprintf(file_ptr, "list->set_delete__avg_nodes_visited       = %lld\n",
            (unsigned long long)(atomic_load(&(list->set_delete__avg_nodes_visited))));
    fprintf(file_ptr, "list->num_set_delete__nodes_visited       = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_set_delete__nodes_visited))));
    fprintf(file_ptr, "list->num_set_delete__cols                = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_set_delete__cols))));
    fprintf(file_ptr, "list->num_set_delete__success             = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_set_delete__success))));
    fprintf(file_ptr, "list->num_set_delete_chksum_cols          = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_set_delete_chksum_cols))));
    fprintf(file_ptr, "list->num_set_delete__base_delete_version = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_set_delete__base_delete_version))));
    fprintf(file_ptr, "list->num_set_delete__curr_entry          = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_set_delete__curr_entry))));
    fprintf(file_ptr, "list->num_set_delete__older_curr          = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_set_delete__older_curr))));

    fprintf(file_ptr, "list->H5P__search_prop__list__num_calls          = %lld\n",
            (unsigned long long)(atomic_load(&(list->H5P__search_prop__list__num_calls))));
    fprintf(file_ptr, "list->search_list__max_nodes_visited             = %lld\n",
            (unsigned long long)(atomic_load(&(list->search_list__max_nodes_visited))));
    fprintf(file_ptr, "list->search_list__avg_nodes_visited             = %lld\n",
            (unsigned long long)(atomic_load(&(list->search_list__avg_nodes_visited))));
    fprintf(file_ptr, "list->num_search_list__nodes_visited             = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_search_list__nodes_visited))));
    fprintf(file_ptr, "list->num_search_list__success                   = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_search_list__success))));
    fprintf(file_ptr, "list->num_search_list__found_base            = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_search_list__found_base))));
    fprintf(file_ptr, "list->num_search_list__found_curr            = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_search_list__found_curr))));
    // fprintf(file_ptr, "list->num_search_list__found_older_curr = %lld\n",
    //(unsigned long long)(atomic_load(&(list->num_search_list__found_older_curr))));
    fprintf(file_ptr, "list->num_target_prop_found_but_deleted = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_target_prop_found_but_deleted))));

    fprintf(file_ptr, "list->num_wait_for_curr_version_to_inc     = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_wait_for_curr_version_to_inc))));

    fprintf(file_ptr, "list->num_thrd_update_cols                 = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_thrd_update_cols))));
    fprintf(file_ptr, "list->num_thrd_count_update                = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_thrd_count_update))));
    fprintf(file_ptr, "clalists->num_thrd_closing_flag_set        = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_thrd_closing_flag_set))));
    fprintf(file_ptr, "list->num_thrd_opening_flag_set            = %lld\n",
            (unsigned long long)(atomic_load(&(list->num_thrd_opening_flag_set))));

done:

    if (name) {
        free(name);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__dump_stats_class() */

/****************************************************************************************
 * Function:    H5P__mt_term_free_lists
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
                /** TODO: just assert fail for now */
                assert(FAIL);
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
                    /** TODO: just assert fail for now */
                    assert(FAIL);
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

        atomic_fetch_sub(&(H5P_mt_g.list_fl_len), 1);

    } /* end while ( fl_list_head.ptr ) */

    /* Ensure the head and tail of the list's free list are NULL */
    fl_list_head = atomic_load(&(H5P_mt_g.list_fl_head));
    fl_list_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

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
                /** TODO: just assert fail for now */
                assert(FAIL);
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
                    /** TODO: just assert fail for now */
                    assert(FAIL);
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

        atomic_fetch_sub(&(H5P_mt_g.class_fl_len), 1);

    } /* end while ( fl_class_head.ptr ) */

    /* Ensure the head and tail of the free list are NULL */
    fl_class_head = atomic_load(&(H5P_mt_g.class_fl_head));
    fl_class_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

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
                /* tmp, just assert fail for now */
                assert(FAIL);
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
                    /* tmp, just assert fail for now */
                    assert(FAIL);
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

        atomic_fetch_sub(&(H5P_mt_g.prop_fl_len), 1);

    } /* while ( fl_prop_head.ptr ) */

    /* Ensure the head and tail of the free list are NULL */
    fl_prop_head = atomic_load(&(H5P_mt_g.prop_fl_head));
    fl_prop_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

    assert(NULL == fl_prop_head.ptr);
    assert(NULL == fl_prop_tail.ptr);

    // H5P__reset_stats_global();

    FUNC_LEAVE_NOAPI(ret_value);

} /* H5P__mt_term_free_lists() */

//#endif
