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
#include "H5private.h"   /* Generic Functions			*/
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

//#ifdef H5_HAVE_MULTITHREAD

#define H5P_MT_SINGLE_THREAD_TESTING 0
#define H5P_ENCODE_VERS 0


/******************/
/* Local Typedefs */
/******************/

typedef enum {
    CLASS_TAG = H5P_MT_CLASS_TAG,
    LIST_TAG  = H5P_MT_LIST_TAG

} H5P_mt_type_t;



/********************/
/* Package Typedefs */
/********************/

/********************/
/* Local Prototypes */
/********************/

herr_t H5P_mt_init(void);

H5P_mt_class_t * 
    H5P__mt_create_class(H5P_mt_class_t *parent, const char *name, 
                         H5P_plist_type_t type, uint64_t test_version,
                         H5P_cls_create_func_t create_func, void *create_data,
                         H5P_cls_copy_func_t copy_func, void *copy_data,
                         H5P_cls_close_func_t close_func, void *close_data);

H5P_mt_class_t * 
    H5P__mt_alloc_class(void);

H5P_mt_list_t * 
    H5P__mt_create_list(H5P_mt_class_t *parent, H5P_mt_list_t* old_list, bool copy,
                        uint64_t test_version, bool app_ref);

H5P_mt_list_t * 
    H5P__mt_alloc_list(void);

herr_t 
    H5P__init_lkup_tbl(H5P_mt_class_t *parent, uint64_t version, H5P_mt_list_t *list);

herr_t
    H5P__init_lkup_tbl_copy(H5P_mt_list_t* old_list, uint64_t version, 
                            H5P_mt_list_t* new_list);

H5P_mt_prop_t * 
    H5P__create_sentinels(bool in_prop_class);

H5P_mt_prop_t * 
    H5P__mt_create_prop(const char *name, void *value_ptr, size_t value_size, 
                        bool in_prop_class, uint64_t create_version,
                        H5P_prp_create_func_t prp_create, H5P_prp_set_func_t prp_set, 
                        H5P_prp_get_func_t prp_get, H5P_prp_encode_func_t prp_encode,
                        H5P_prp_decode_func_t prp_decode, H5P_prp_delete_func_t prp_del,
                        H5P_prp_copy_func_t prp_copy, H5P_prp_compare_func_t prp_cmp,
                        H5P_prp_close_func_t prp_close);

H5P_mt_prop_t * 
    H5P__mt_alloc_prop(void);

herr_t
    H5P__mt_copy_lfsll(void* param, H5P_mt_prop_t* old_prop, uint64_t version);

herr_t 
    H5P__mt_ins_or_mod_prop__main(void *param, const char *name, void *value, 
                        size_t size, bool create, bool copy,
                        H5P_prp_create_func_t prp_create, H5P_prp_set_func_t prp_set, 
                        H5P_prp_get_func_t prp_get, H5P_prp_encode_func_t prp_encode,
                        H5P_prp_decode_func_t prp_decode, H5P_prp_delete_func_t prp_del,
                        H5P_prp_copy_func_t prp_copy, H5P_prp_compare_func_t prp_cmp,
                        H5P_prp_close_func_t prp_close);

herr_t 
    H5P__mt_ins_or_mod_prop__lfsll_ins(H5P_mt_prop_t *pl_head, H5P_mt_prop_t *new_prop, 
                                       uint32_t *deletes_ptr, uint32_t *nodes_visited_ptr, 
                                       uint32_t *thrd_cols_ptr);

herr_t 
    H5P__set_delete_version(void *param, H5P_mt_prop_t *target_prop);

H5P_mt_prop_t * 
    H5P__mt_search_prop(void *param, int64_t chksum, const char *name);

H5P_mt_list_table_entry_t * 
    H5P__mt_search_lkup_tbl(H5P_mt_list_table_entry_t * lkup_tbl, 
                            uint32_t left_entry,
                            uint32_t right_entry,
                            int64_t  chksum);

H5P_mt_prop_t *
    H5P__mt_search_lfsll(H5P_mt_prop_t *pl_head, int64_t chksum, uint64_t version);

H5P_mt_prop_t * 
    H5P__mt_entry_find_version(H5P_mt_list_table_entry_t * entry, uint64_t version, 
                               bool *base_flag);

herr_t 
    H5P__find_mod_point(H5P_mt_prop_t *pl_head, H5P_mt_prop_t **first_ptr_ptr, 
                           H5P_mt_prop_t **second_ptr_ptr, uint32_t *deletes_ptr, 
                           uint32_t *nodes_visited_ptr, uint32_t *thrd_cols_ptr, 
                           H5P_mt_prop_t *target_prop);

H5P_mt_prop_t * 
    H5P__get_next_valid_prop(H5P_mt_prop_t *prop, uint64_t version);

H5P_mt_prop_t * 
    H5P__find_valid_version(H5P_mt_prop_t *prop, uint64_t version);

int32_t 
    H5P__is_valid(H5P_mt_prop_t *prop, uint64_t version);

int32_t
    H5P__mt_compare_prop(H5P_mt_prop_t *prop1, H5P_mt_prop_t *prop2);

int32_t
    H5P__mt_is_equal(H5P_mt_prop_t *prop1, H5P_mt_prop_t *prop2);

int32_t
    H5P__mt_cmp_list_or_class(void *param1, void *param2);

uint64_t
    H5P__mt_get_version(void *param);

herr_t 
    H5P__mt_close_prop(H5P_mt_prop_t *prop);

H5P_mt_prop_t * 
    H5P__clear_mt_prop(H5P_mt_prop_t *prop);

herr_t
    H5P__mt_close_class(H5P_mt_class_t *class);

H5P_mt_class_t *
    H5P__clear_mt_class(H5P_mt_class_t *class);

herr_t
    H5P__mt_close_list(H5P_mt_list_t *list);

H5P_mt_list_t * 
    H5P__clear_mt_list(H5P_mt_list_t *list);


uint64_t H5P__mt_version_check(void *param, uint64_t curr_version, 
                               uint64_t next_version);

herr_t H5P__inc_thrd_count(void *param);

herr_t H5P__dec_thrd_count(void *param);

herr_t H5P__inc_ref_count(H5P_mt_class_t *parent, bool plc);

herr_t H5P__dec_ref_count(H5P_mt_class_t *parent, bool plc);

int64_t H5P__calc_checksum(const char *name);

herr_t H5P__mt_get_value(H5P_mt_list_t *list, int64_t chksum, const char *name,
                         void *value_ptr);

herr_t H5P__mt_encode(H5P_mt_list_t *list, uint64_t version, 
                   void *buf, size_t *nalloc);

herr_t H5P__mt_encode_prop(H5P_mt_prop_t *prop, bool encode, 
                        size_t *encode_size, uint8_t **p);

/* Stats functions */
herr_t H5P__reset_stats_global(void);

herr_t H5P__init_stats_class(H5P_mt_class_t *class);

herr_t H5P__reset_stats_class(H5P_mt_class_t *class);

herr_t H5P__init_stats_list(H5P_mt_list_t *list);

herr_t H5P__reset_stats_list(H5P_mt_list_t *list);

herr_t H5P__dump_stats_global(FILE *file_ptr);

herr_t H5P__dump_stats_class(FILE *file_ptr, H5P_mt_class_t *class);

herr_t H5P__dump_stats_list(FILE *file_ptr, H5P_mt_list_t *list);

/* shutdown function for internal testing */
void H5P__shutdown(void);




/*********************/
/* Package Variables */
/*********************/

H5P_mt_t         H5P_mt_g;

H5P_mt_class_t * H5P_MT_CLS_ROOT_g = NULL;

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
 * Function:    H5P_init
 *
 * Purpose:     Initializes the fields for the H5P_mt_g global struct, and allocates and
 *              initializes the root class. It also initializes the free lists for
 *              properties, property lists, and property classes.
 *
 * Return:      SUCCEED/FAIL    
 *
 ****************************************************************************************
 */
herr_t
H5P_mt_init(void)
{
    H5P_mt_prop_aptr_t  fl_prop;
    H5P_mt_list_sptr_t  fl_list;
    H5P_mt_class_sptr_t fl_class;
    H5P_mt_class_t    * root_class;

#if 1 //H5P_MT_SINGLE_THREAD_TESTING
#else
    H5P_mt_class_t    * att_access_cls;
    H5P_mt_class_t    * group_access_cls;
    H5P_mt_class_t    * datatype_create_cls;
    H5P_mt_class_t    * datatype_access_cls;
    H5P_mt_class_t    * vol_init_cls;
    H5P_mt_class_t    * ref_access_cls;
#endif

    herr_t                ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    /* Initialize the fields of H5P_mt_g the global struct that handles the free lists */

    atomic_init(&(H5P_mt_g.active_threads), 0);

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
    atomic_init(&(H5P_mt_g.prop_fl_len),  1ULL);

    atomic_init(&(H5P_mt_g.class_fl_head), fl_class);
    atomic_init(&(H5P_mt_g.class_fl_tail), fl_class);
    atomic_init(&(H5P_mt_g.class_fl_len),  1ULL);

    atomic_init(&(H5P_mt_g.list_fl_head), fl_list);
    atomic_init(&(H5P_mt_g.list_fl_tail), fl_list);
    atomic_init(&(H5P_mt_g.list_fl_len),  1ULL);



    /* Allocating and Initializing the root property list class */

    if ( NULL == ( root_class = H5P__mt_create_class(NULL, "root", H5P_TYPE_ROOT, 0,
                                                     NULL, NULL, NULL, NULL, 
                                                     NULL, NULL) ) )
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "root class initialization failed");

    assert(root_class);
    assert(sizeof(root_class->thrd) == 16);
    assert(sizeof(root_class->ref_count) == 16);


    H5P_MT_CLS_ROOT_g = root_class;

#if 0
    if ( NULL == ( att_access_cls = H5P__mt_create_class(NULL, "attribute_access", 
                                                         H5P_TYPE_ATTRIBUTE_ACCESS, 0,
                                                         NULL, NULL, NULL, NULL, 
                                                         NULL, NULL)))
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, 
                    "attribute access class initialization failed");
#endif
    
    /* Initialize H5P_mt_g stats fields */

    /* Free list stats */
    atomic_init(&(H5P_mt_g.prop_fl_head_update),      0ULL);
    atomic_init(&(H5P_mt_g.prop_fl_head_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.prop_fl_tail_update),      0ULL);
    atomic_init(&(H5P_mt_g.prop_fl_tail_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.prop_fl_next_update),      0ULL);
    atomic_init(&(H5P_mt_g.prop_fl_next_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.num_props_added_to_fl),    0ULL);

    atomic_init(&(H5P_mt_g.class_fl_head_update),      0ULL);
    atomic_init(&(H5P_mt_g.class_fl_head_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.class_fl_tail_update),      0ULL);
    atomic_init(&(H5P_mt_g.class_fl_tail_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.class_fl_next_update),      0ULL);
    atomic_init(&(H5P_mt_g.class_fl_next_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.num_class_added_to_fl),     0ULL);

    atomic_init(&(H5P_mt_g.list_fl_head_update),      0ULL);
    atomic_init(&(H5P_mt_g.list_fl_head_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.list_fl_tail_update),      0ULL);
    atomic_init(&(H5P_mt_g.list_fl_tail_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.list_fl_next_update),      0ULL);
    atomic_init(&(H5P_mt_g.list_fl_next_update_cols), 0ULL);
    atomic_init(&(H5P_mt_g.num_list_added_to_fl),     0ULL);

    atomic_init(&(H5P_mt_g.num_classes_freed),        0ULL);
    atomic_init(&(H5P_mt_g.num_lists_freed),          0ULL);
    atomic_init(&(H5P_mt_g.num_props_freed),          0ULL);

    /* H5P_mt_class_t and H5P_mt_list_t comparison stats */
    atomic_init(&(H5P_mt_g.max_derived_classes),      0ULL);
    atomic_init(&(H5P_mt_g.max_derived_lists),        0ULL);
    atomic_init(&(H5P_mt_g.max_class_num_phys_props), 2ULL);
    atomic_init(&(H5P_mt_g.max_list_num_phys_props),  2ULL);
    atomic_init(&(H5P_mt_g.max_class_version_number), 0ULL);
    atomic_init(&(H5P_mt_g.max_list_version_number),  0ULL);

done: 

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P_init() */



/****************************************************************************************
 * Function:    H5P__create_class
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
 *              Next, memory is allocated for the new H5P_mt_class_t and the fields are
 *              initialized, by calling H5P__mt_alloc_class(). The class's name and type 
 *              are set to the provided name and type parameters. 
 * 
 *              NOTE: at the current version, H5P__mt_alloc_class() only allocates a new
 *              class, but in the future classes that are closed and put on the class
 *              free list will be made reallocable for a new class.
 * 
 *              The new class's pl_head is initialized to the property (H5P_mt_prop_t) 
 *              that is returned by the function H5P__create_sentinels(), which returns 
 *              the negative sentinel. 
 * 
 *              The parent class's LFSLL is then iterated to find and create copies of 
 *              the valid properties, using the function H5P__mt_copy_lfsll().
 *              
 *              The thrd.opening field of the new class is initialized TRUE, to prevent
 *              other threads from accessing the structure until it's completely set up.
 * 
 *              The provided callback functions and data are copied into the class.
 * 
 *              NOTE: the H5P__init_stats_class() function is called to initialize the
 *              stats fields of the H5P_mt_class_t structure to collect stats for
 *              testing purposes.
 * 
 *              NOTE: Currently classes call H5I_register() outside of their inner 
 *              functions, but if it gets moved inside the function it should be placed
 *              here.
 * 
 *              NOTE: the thrd.opening will have to be udpated to FALSE in the function 
 *              calling this one, after H5I_register() is called.
 *              Finally, the thrd fields are updated for the new class and parent class.
 *              The new class's thrd.opening is set to FALSE allowing other threads to 
 *              now access this structure. The parent class's thrd.count is decremented.
 *              
 *
 * Return:      Success: Returns a pointer to the new H5P_mt_class_t struct.
 * 
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_class_t *
H5P__mt_create_class(H5P_mt_class_t *parent, const char *name, H5P_plist_type_t type,
                     uint64_t test_version, 
                     H5P_cls_create_func_t create_func, void *create_data,
                     H5P_cls_copy_func_t copy_func, void *copy_data,
                     H5P_cls_close_func_t close_func, void *close_data)
{
    H5P_mt_class_t             * new_class = NULL;
    hid_t                        parent_id;
    H5P_mt_class_ref_counts_t    ref_count;
    H5P_mt_active_thread_count_t thrd;
    H5P_mt_class_sptr_t          fl_next;
    uint64_t                     parent_version = 0;
    uint32_t                     phys_pl_len;
    uint32_t                     log_pl_len;
    bool                         inc_thrd_flag = FALSE;

    H5P_mt_class_t             * ret_value = NULL;


    FUNC_ENTER_PACKAGE


    /**
     * If parent is NULL then this is the root class, and there isn't a parent class to 
     * increment. Otherwise, increment the thrd count in the parent class.
     */
    if ( parent != NULL )
    {
        assert( (atomic_load(&(parent->tag))) == H5P_MT_CLASS_TAG);

        parent_version = atomic_load(&(parent->curr_version));

        /* Increment parent's thrd count */
        if ( H5P__inc_thrd_count(parent) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                        "Couldn't increment parent's thread count.");
        
        inc_thrd_flag = TRUE;
    }

    /**
     * NOTE: This is for testing to choose a specific version to create the class at to 
     * ensure only the valid properties for that version are copied over
     */
    if ( test_version > 0 )
    {
        assert(test_version <= parent_version);

        parent_version = test_version;
    }

    /* Update parent's ref count if parent is NULL. Currently can't fail */
    if ( parent != NULL )
    {
        H5P__inc_ref_count(parent, TRUE);
    }


    /* Allocates a new property list class */
    new_class = H5P__mt_alloc_class();
    if ( NULL == new_class )
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, 
                    "Failed to create new property list class.");


    /* Initialize class fields */
    atomic_store(&(new_class->tag), H5P_MT_CLASS_TAG);

    /* If root class then there is no parent */
    if ( parent != NULL )
    {
        parent_id = atomic_load(&(parent->id));
        new_class->parent_id      = parent_id;
    }
    else
    {
        new_class->parent_id = H5I_INVALID_HID;
    }

    new_class->parent_ptr     = parent;
    new_class->parent_version = parent_version;

    new_class->name = strdup(name);

    /** NOTE: Needs to be actually set after creation */
    atomic_store(&(new_class->id), H5I_INVALID_HID);
    new_class->type = type;

    atomic_store(&(new_class->curr_version), 1);
    atomic_store(&(new_class->next_version), 2);

    
    /* Creates the sentinel nodes and sets the negative sentinel as the head */
    new_class->pl_head = H5P__create_sentinels(TRUE);
    if ( new_class->pl_head == NULL )
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create sentinel nodes.");

    phys_pl_len = 2;
    log_pl_len  = 0;
    atomic_store(&(new_class->phys_pl_len), phys_pl_len);
    atomic_store(&(new_class->log_pl_len),  log_pl_len);


    /**
     * Copy the valid props from the parent into the new_class.
     * If root class then there is no parent. 
     */
    if ( parent != NULL )
    {
        /*If the phy_pl_len of the parent's LFSLL is 2, there are not props to copy */
        if ( 2 < (atomic_load(&(parent->phys_pl_len))) )
        {

            if ( 0 > H5P__mt_copy_lfsll(new_class, parent->pl_head, parent_version) )
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, NULL, 
                            "Failed to copy parent's lfsll.");
        
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

    /* Set the callbacks to the provided callbacks */
    new_class->create_func = create_func;
    new_class->create_data = create_data;
    new_class->copy_func   = copy_func;
    new_class->copy_data   = copy_data;
    new_class->close_func  = close_func;
    new_class->close_data  = close_data;


    /* Initializes the class's stats fields */
    H5P__init_stats_class(new_class);
    

    /**
     * TODO: adding a class into the index is done outside of the function, but if
     * it needs to be added into the function for some reason, it will be added here.
     */



    /* update thrd struct of the new_class to reflect it is no longer opening */
    thrd.count   = 0;
    thrd.opening = FALSE;
    thrd.closing = FALSE;

    atomic_store(&(new_class->thrd), thrd);


    ret_value = new_class;

done:

    /* update parent's thrd count */
    if ( parent != NULL && inc_thrd_flag )
    {
        if ( 0 > H5P__dec_thrd_count(parent) )
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                        "Failure to decrement thrd_count.");
    }

    

    FUNC_LEAVE_NOAPI(ret_value)
   
} /* H5P__mt_create_class() */



/****************************************************************************************
 * Function:    H5P__mt_alloc_class
 *
 * Purpose:     Gets a pointer to a valid H5P_mt_class_t struct by either reallocating
 *              one from the free list if one is reallocable, or by allocating a new one
 *              from memory.
 * 
 *              NOTE: In this iteration no struct on the free list can be reallocated.
 *
 * Return:      Success: Returns a pointer to a H5P_mt_class_t
 * 
 *              Failure: NULL    
 *
 ****************************************************************************************
 */
H5P_mt_class_t *
H5P__mt_alloc_class(void)
{
    H5P_mt_class_sptr_t fl_head;
    H5P_mt_class_sptr_t fl_next;
    H5P_mt_class_t    * head_class;
    H5P_mt_class_t    * new_class = NULL;
    bool                done = FALSE;
    
    H5P_mt_class_t    * ret_value = NULL;

    FUNC_ENTER_NOAPI(FAIL)

    fl_head = atomic_load(&(H5P_mt_g.class_fl_head));

    head_class = fl_head.ptr;

    /* If no class structs are reallocable, alloc from memory */
    if ( head_class == NULL || 
        ( atomic_load(&(head_class->tag)) != H5P_MT_CLASS_FL_REALLOC_TAG ) )
    {
        new_class = (H5P_mt_class_t *)malloc(sizeof(H5P_mt_class_t));
        if ( NULL == new_class )
        {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, 
                        "property list class allocation failed");
        }  
    }
    /* If a struct on the free list is reallocable, clear it and return it */
    else
    {
        do 
        {
            fl_next = atomic_load(&(head_class->fl_next));

            fl_head = atomic_load(&(H5P_mt_g.class_fl_head));

            if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_head),
                                                  &fl_head, fl_next))
            {
                /* failed, update stats and try again */
                atomic_fetch_add(&(H5P_mt_g.class_fl_head_update_cols), 1);
            }
            else
            {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.class_fl_head_update), 1);
                atomic_fetch_sub(&(H5P_mt_g.class_fl_len), 1);

                done = TRUE;
            }

        } while ( ! done );

        new_class = H5P__clear_mt_class(fl_head.ptr);
    }



done:

    ret_value = new_class;
    
    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_alloc_class() */



/****************************************************************************************
 * Function:    H5P__create_list
 *
 * Purpose:     Function to create a new property list (H5P_mt_list_t) derived from a 
 *              property list class (H5P_mt_class_t), or to create a new property list
 *              that is a copy from an existing property list.
 * 
 *              NOTE: property list classes are referred to as classes and property lists
 *              are referred to as lists.
 *
 *              A multi-thread safe function to create a new list, derived from an 
 *              existing class, or as a copy of an existing list. Lists create an array 
 *              of H5P_mt_list_table_entry_t of length nprops_inherited which point to 
 *              the valid properties in the parent class's LFSLL.         
 * 
 * Details:     NOTE: for more information of the H5P_mt_list_t structure or specific
 *              fields, check the detailed comment for the struture in H5Ppkg_mt.h 
 * 
 *              To ensure multi-thread safety the first step is to check the parent 
 *              class's, and if copying an existing list check that list's thrd field to 
 *              ensure the parent class is not in the process opening or closing. If 
 *              opening is TRUE the function loops checking again. If opening, the class
 *              or list will only be briefly visible to other threads before completing 
 *              the opening process, so this thread should see that opening's been set to
 *              FALSE without waiting long (stats are collected, so if this proves 
 *              incorrect, it can be found quickly and fixed). If closing is true, an 
 *              error is thrown. If neither are true, the count field is incremented to 
 *              show another thread is accessing the structure. 
 * 
 *              Next, we increment the ref_count of the parent class's derived lists, 
 *              regardless of creating a new list or copying an existing one. Doing this
 *              now ensures, that the parent class cannot be deleted out from under us.
 * 
 *              Memory is allocated for the new H5P_mt_list_t, either from memory or from
 *              the list free list if there is a reallocable list available, and the 
 *              fields are initialized. The list's pl_head field points to the negative 
 *              sentinel that is returned from the H5P__create_sentinels() function. 
 *              The thrd.opening field of the new list is initialized TRUE, to prevent
 *              other threads from accessing the structure until it's completely set up.
 * 
 *              If this is a new list being created the lkup_tbl is allocated and 
 *              initialized by the function H5P__init_lkup_tbl() based on the parent 
 *              class. 
 * 
 *              But if this is a copy of an existing list, then the lkup_tbl is allocated 
 *              and initialized by the function H5P__init_lkup_tbl_copy() to copy the 
 *              old_list. Then H5P__mt_copy_lfsll() is called to copy the valid props 
 *              from the old list to the new one.
 *              
 * 
 *              NOTE: the H5P__init_stats_list() function is called to initialize the
 *              stats fields of the H5P_mt_list_t structure to collect stats for
 *              testing purposes.
 * 
 *              H5I_register() is called on the new list to register it in the index and
 *              get an id. 
 * 
 *              NOTE: Now that we have an ID and are in the index, we are visible to 
 *              other threads, however thrd.opening being TRUE prevents any other thread
 *              from accessing the struct until we finish the opening process.
 * 
 *              Now that we have an ID for the new list, we check if this is a newly 
 *              created list, or a copy of an existing one, and perform the respective
 *              class callback on the parent of the list. 
 * 
 *              TODO: May have to iterate up the parent inheritance tree and perform
 *              the class callback on all classes up the tree.
 * 
 *              The new list's thrd thrd.opening is set to FALSE allowing other threads
 *              to now access this structure. 
 * 
 *              The parent class's thrd.count is decremented.
 * 
 *              TODO: May need to add bool app_ref as a parameter.
 *              
 *
 * Return:      Success: Returns a pointer to the new H5P_mt_list_t struct.
 * 
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_list_t *
H5P__mt_create_list(H5P_mt_class_t *parent, H5P_mt_list_t *old_list, bool copy,
                    uint64_t test_version, bool app_ref)
{
    H5P_mt_list_t              * new_list = NULL;
    hid_t                        parent_id;
    H5P_mt_active_thread_count_t list_thrd;
    H5P_mt_list_sptr_t           fl_next;
    uint64_t                     new_list_version;
    bool                         inc_thrd_flag = FALSE;
    bool                         inc_thrd_flag_list = FALSE;

#if H5P_MT_SINGLE_THREAD_TESTING
#else
    hid_t                        new_plist_id;
#endif

    H5P_mt_list_t              * ret_value = NULL;

    FUNC_ENTER_PACKAGE


    assert(parent);
    assert( (atomic_load(&(parent->tag))) == H5P_MT_CLASS_TAG);

    new_list_version = atomic_load(&(parent->curr_version));

    /* if old_list isn't NULL ensure it's valid */
    if ( old_list )
    {
        assert(old_list->tag == H5P_MT_LIST_TAG);
    }


    if ( copy )
    {
        if ( H5P__inc_thrd_count(old_list) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, 
                        "Couldn't increment old list's thread count.");
        else
            inc_thrd_flag_list = TRUE;

        /* If we are copying a list, set the version to the same one as the old list */
        new_list_version = old_list->pclass_version;
    }

    /* Increment parent's thrd count */
    if ( H5P__inc_thrd_count(parent) < 0 )
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, 
                        "Couldn't increment parent's thread count.");
    else
        inc_thrd_flag = TRUE;


    /**
     * NOTE: This is for testing to choose a specific version to create the list at to 
     * ensure only the valid properties for that version are stored in the lkup_tbl.
     */
    if ( test_version > 0 )
    {
        assert(test_version <= new_list_version);

        new_list_version = test_version;
    }



    /* Update parent's ref count. Currently can't fail */
    H5P__inc_ref_count(parent, FALSE);

    /* Allocates a new property list */
    new_list = H5P__mt_alloc_list();
    if ( NULL == new_list )
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, 
                    "Failed to create new property list.");


    /* Initialize list fields */
    atomic_store(&(new_list->tag), H5P_MT_LIST_TAG);

    parent_id = atomic_load(&(parent->id));    
    new_list->pclass_id = parent_id;

    new_list->pclass_ptr = parent;
    new_list->pclass_version = new_list_version;

    atomic_store(&(new_list->plist_id), H5I_INVALID_HID);
    atomic_store(&(new_list->curr_version), 1);
    atomic_store(&(new_list->next_version), 2);

    new_list->lkup_tbl = NULL;

    new_list->nprops_inherited = 0;
    atomic_store(&(new_list->nprops_added), 0);
    atomic_store(&(new_list->nprops), 0);

    /* Creates the sentinel nodes and sets teh negative sentinel as the head */
    new_list->pl_head = H5P__create_sentinels(FALSE);
    if ( NULL == new_list->pl_head )
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create sentinels nodes.");
    
    atomic_store(&(new_list->log_pl_len),  0);
    atomic_store(&(new_list->phys_pl_len), 2);

    /* Set class initialization flag to false fo now */
    atomic_store(&(new_list->class_init), FALSE);

    list_thrd.count   = 0;
    list_thrd.opening = TRUE;
    list_thrd.closing = FALSE;
    atomic_store(&(new_list->thrd), list_thrd);

    fl_next.ptr = NULL;
    fl_next.sn  = 0;
    atomic_store(&(new_list->fl_next), fl_next);

    /* Allocate and intialize the lkup_tbl */

    /* If copy, then we must copy how the old list's lkup_tbl and lfsll is set up */
    if ( copy )
    {
        if ( 0 > (H5P__init_lkup_tbl_copy(old_list, new_list_version, new_list)))
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to copy lkup_tbl.");

        /* Iterate the old_list's LFSLL and copy the valid props into the new list */
        if ( 0 > H5P__mt_copy_lfsll(new_list, old_list->pl_head, new_list_version) )
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, NULL, "Failed to copy list's lfsll");
    }
    /* Else we must initialize the lkup_tbl from the parent class */
    else
    {
        if ( 0 > (H5P__init_lkup_tbl(parent, new_list_version, new_list) ) )
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create lkup_tbl.");
    }


    /* Initialize all the stats for the list */
    H5P__init_stats_list(new_list);

#if H5P_MT_SINGLE_THREAD_TESTING

#else
    /**
     * NOTE: this is my first attempt at registering the list in the index
     */
    if ( ( new_plist_id = H5I_register(H5I_GENPROP_LST, new_list, app_ref) ) < 0 )
        HGOTO_ERROR(H5E_PLIST, H5E_CANTREGISTER, NULL, 
                    "unable to register property list");

    atomic_store(&(new_list->plist_id), new_plist_id);


    /** 
     * If copy is TRUE, then this is for a copy callback, and we must call the
     * class callback on the parent classes up the inheritance tree, if it exists. 
     */
    if ( copy )
    {
        if ( parent->copy_func )
        {
            hid_t new_list_id = atomic_load(&(new_list->plist_id));
            hid_t old_list_id = atomic_load(&(old_list->plist_id));

            /* If the copy callback fails remove the list's id from the index */
            if ((parent->copy_func)(new_list_id, old_list_id, parent->copy_data) < 0)
            {
                H5I_remove(new_plist_id);

                assert(H5P_MT_ASSERT_FAIL);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, NULL, 
                            "Can't initialize property");
            }
        }
    }
    /**
     * If copy is FALSE, then this is for creating a new list, and we must call the
     * create class callback on the parent class, if it exits.
     */
    else
    {
        if ( parent->create_func )
        {
            hid_t new_list_id = atomic_load(&(new_list->plist_id));

            /* If the copy callback fails remove the list's id from the index */
            if ( (parent->create_func)(new_list_id, parent->create_data) < 0 )
            {
                H5I_remove(new_plist_id);

                assert(H5P_MT_ASSERT_FAIL);
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, NULL, 
                            "Can't initialize property");
            }
        }
    }

#endif

    /* Set the class initialization flag */
    atomic_store(&(new_list->class_init), FALSE);

    /* update thrd struct of the new_list to mark opening FALSE */
    list_thrd.count   = 0;
    list_thrd.opening = FALSE;
    list_thrd.closing = FALSE;

    atomic_store(&(new_list->thrd), list_thrd);
    

done:

    /* update parent's thrd count */
    if ( inc_thrd_flag )
    {
        if ( 0 > H5P__dec_thrd_count(parent) )
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                        "Failure to decrement thrd_count.");
    }

    if ( inc_thrd_flag_list )
    {
        if ( 0 > H5P__dec_thrd_count(old_list) )
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                        "Failure to decrement thrd_count.");
    }

#if H5P_MT_SINGLE_THREAD_TESTING
#else
    new_plist_id = atomic_load(&(new_list->plist_id));

    /* If an error occured, properly handle the allocated memory */

    if ( H5I_INVALID_HID == new_plist_id && new_list )
        H5P__mt_close_list(new_list);
#endif

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
 *              NOTE: In this iteration no struct on the free list can be reallocated.
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
    H5P_mt_list_sptr_t fl_head;
    H5P_mt_list_sptr_t fl_next;
    H5P_mt_list_t    * head_list;
    H5P_mt_list_t    * new_list = NULL;
    bool               done     = FALSE;

    H5P_mt_list_t    * ret_value = NULL;

    FUNC_ENTER_NOAPI(FAIL)

    fl_head = atomic_load(&(H5P_mt_g.list_fl_head));

    head_list = fl_head.ptr;

    if ( head_list == NULL || (atomic_load(&(head_list->tag)) != H5P_MT_LIST_FL_REALLOC_TAG ) )
    {
        new_list = (H5P_mt_list_t *)malloc(sizeof(H5P_mt_list_t));
        if ( NULL == new_list )
        {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "property list allocation failed");
        } 
    }
    else
    {
        do 
        {
            fl_next = atomic_load(&(head_list->fl_next));

            fl_head = atomic_load(&(H5P_mt_g.list_fl_head));

            if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_head),
                                                  &fl_head, fl_next))
            {
                /* failed, update stats and try again */
                atomic_fetch_add(&(H5P_mt_g.list_fl_head_update_cols), 1);
            }
            else
            {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.list_fl_head_update), 1);
                atomic_fetch_sub(&(H5P_mt_g.list_fl_len), 1);

                done = TRUE;
            }

        } while ( ! done );

        new_list = H5P__clear_mt_list(fl_head.ptr);
    }



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
 * Details:     The function first iterates through the LFSLL of the list's parent class,
 *              and counts the number of valid properties. 
 *              Two things make a property invalid:
 *                  1) any property that has a create_version greater than the parent 
 *                     class's version from which the new class is derived.
 *                  2) any property that has a delete_version less than or equal to the
 *                     parent class's version from which the new class is derived.
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
    H5P_mt_list_table_entry_t * entry;
    H5P_mt_list_prop_ref_t      base;
    H5P_mt_list_prop_ref_t      curr;
    H5P_mt_prop_t             * valid_prop;
    H5P_mt_prop_t             * parent_prop;
    H5P_mt_prop_t             * new_prop;
    H5P_mt_prop_value_t         valid_prop_value;
    uint32_t                    nprops;
    uint32_t                    deletes       = 0;
    uint32_t                    nodes_visited = 0;
    uint32_t                    thrd_cols     = 0;

    herr_t                      ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE


    parent_prop = parent->pl_head;
    assert(parent_prop);
    assert(atomic_load(&(parent_prop->tag)) == H5P_MT_PROP_TAG);
    assert(parent_prop->sentinel);


    /* Iterate the parent's LFSLL and count the valid properties */
    do
    {
        valid_prop = H5P__get_next_valid_prop(parent_prop, version);

        if ( valid_prop )
        {
            new_list->nprops_inherited++;

            parent_prop = valid_prop;
        }

    } while ( valid_prop );


    /* Allocate the lkup_tbl array */

    assert(new_list->nprops_inherited > 0);

    /* Allocates the number of entries needed in the lkup_tbl */
    new_list->lkup_tbl = (H5P_mt_list_table_entry_t *)malloc(new_list->nprops_inherited * 
                                                      sizeof(H5P_mt_list_table_entry_t));
    if ( NULL == new_list->lkup_tbl )
    {    
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "lkup_tbl allocation failed");
    }

    nprops = 0;

    parent_prop = parent->pl_head;

    /* Set up each lkup_tbl entry to the valid properties in the parent's LFSLL */
    do
    {
        valid_prop = H5P__get_next_valid_prop(parent_prop, version);

        if ( valid_prop )
        {
            /* Initialize the entry's fields */

            entry = &new_list->lkup_tbl[nprops];

            entry->chksum = valid_prop->chksum;
            entry->name   = strdup(valid_prop->name);

            /** 
             * If the valid_prop has the create callback, set the base.ptr to NULL and
             * create a new prop struct. Call the create callback and insert new_prop 
             * into the new list's LFSLL.
             */
            if ( valid_prop->create )
            {
                base.ptr = NULL;
                base.ver = 1;
                atomic_store(&(entry->base), base);

                atomic_store(&(entry->base_delete_version), 0);

                valid_prop_value = atomic_load(&(valid_prop->value));

                new_prop = H5P__mt_create_prop(valid_prop->name, valid_prop_value.ptr,
                                               valid_prop_value.size, FALSE, 1,
                                               valid_prop->create, valid_prop->set, 
                                               valid_prop->get, valid_prop->encode, 
                                               valid_prop->decode, valid_prop->del, 
                                               valid_prop->copy, valid_prop->cmp, 
                                               valid_prop->close);
                if ( NULL == new_prop )
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, 
                                "Failed creating property for property list.");


                /* Call the create callback */
                if ( (new_prop->create)(new_prop->name, valid_prop_value.size,
                                                        valid_prop_value.ptr) < 0 )
                {
                    assert(H5P_MT_ASSERT_FAIL);
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, "Can't create property");
                }

                new_prop->in_lkup_tbl = TRUE;

                /* Inserts the new_prop into the new_list's LFSLL */
                H5P__mt_ins_or_mod_prop__lfsll_ins(new_list->pl_head,
                                                   new_prop,
                                                   &deletes,
                                                   &nodes_visited,
                                                   &thrd_cols);

                atomic_fetch_add(&(new_list->log_pl_len), 1);
                atomic_fetch_add(&(new_list->phys_pl_len), 1);

                curr.ptr = new_prop;
                curr.ver = 1;
                atomic_store(&(entry->curr), curr);

            } /* end if ( valid_prop->create ) */

            /** 
             * If new prop doesn't have the create callback set base.ptr to point to the
             * parent and curr.ptr to NULL
             */
            else
            {
                base.ptr = atomic_load(&(valid_prop));
                base.ver = 1;
                atomic_store(&(entry->base), base);
    
                atomic_fetch_add(&(valid_prop->ref_count), 1);
                
                atomic_store(&(entry->base_delete_version), 0);
    
                curr.ptr = NULL;
                curr.ver = 0;
                atomic_store(&(entry->curr), curr);
            }

            /* Increment number of properties */
            nprops++;
            assert(nprops <= new_list->nprops_inherited);

            /* Iterate in the parent's LFSLL to look for the next valid_prop */
            parent_prop = valid_prop;
        
        } /* end if ( valid_prop ) */

    } while ( valid_prop );
    
    assert(nprops == new_list->nprops_inherited);

    atomic_store(&(new_list->nprops), nprops);

done:

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
 *              Then we iterate through the lkup_tbl for each entry we set the chksum, 
 *              and name to copies of the old_list. If the old_list->base_delete_version
 *              is greater than 0 or less than or equal to the version parameter, we set
 *              the new_list->base_delete_version to 1 to signify that the base is 
 *              deleted.
 * 
 *              If the old_base.ptr isn't NULL and isn't deleted we atomically update the
 *              parent class's property's ref_count and atomically set new_base.ptr to 
 *              point to it. Else, we atomically set it NULL. Either way base.ver is 
 *              atomically set to 1.
 * 
 *              If the old_curr.ptr isn't NULL and also isn't deleted, then create a new 
 *              property struct, that is a copy of the prop old_curr.ptr points to. Then             
 *              make the copy callback, if it exists, add it to the new_list's LFSLL, and
 *              atomically set new_curr.ptr to point to it.
 *              Else, atomically set new_curr.ptr to NULL. Either way curr.ver is 
 *              atomically set to 1.
 *              
 *
 * Return:      SUCCEED/FAIL    
 *
 ****************************************************************************************
 */
herr_t
H5P__init_lkup_tbl_copy(H5P_mt_list_t* old_list, uint64_t version, 
                        H5P_mt_list_t* new_list)
{
    H5P_mt_list_table_entry_t * new_entry;
    H5P_mt_list_table_entry_t * old_entry;
    H5P_mt_list_prop_ref_t      new_base;
    H5P_mt_list_prop_ref_t      old_base;
    H5P_mt_list_prop_ref_t      new_curr;
    H5P_mt_list_prop_ref_t      old_curr;
    H5P_mt_prop_t             * old_prop;
    H5P_mt_prop_t             * new_prop;
    H5P_mt_prop_value_t         old_prop_value;
    H5P_mt_class_t            * parent;
    uint64_t                    old_delete_version;
    uint64_t                    old_prop_ref_count;
    uint64_t                    new_prop_ref_count;
    uint32_t                    nprops;
    uint32_t                    deletes       = 0;
    uint32_t                    nodes_visited = 0;
    uint32_t                    thrd_cols     = 0;
    bool                        done = TRUE;

    herr_t                      ret_value = SUCCEED;


    FUNC_ENTER_PACKAGE

    assert(old_list);
    assert(old_list->tag == H5P_MT_LIST_TAG);

    /* Copies some values from the old_list to the new copy */
    new_list->nprops_inherited = old_list->nprops_inherited;
    atomic_store(&(new_list->nprops_added), atomic_load(&(old_list->nprops_added)));
    atomic_store(&(new_list->nprops), atomic_load(&(old_list->nprops)));


    /* Allocates the number of entries needed in the lkup_tbl */
    new_list->lkup_tbl = (H5P_mt_list_table_entry_t *)malloc(new_list->nprops_inherited * 
                                                    sizeof(H5P_mt_list_table_entry_t));
    if ( NULL == new_list->lkup_tbl )
    {    
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, "lkup_tbl allocation failed");
    }

    /* Set up each lkup_tbl entry to copy the old_list's lkup_tbl */
    for ( nprops = 0; nprops < new_list->nprops_inherited; nprops++ )
    {
        new_entry = &new_list->lkup_tbl[nprops];
        old_entry = &old_list->lkup_tbl[nprops];

        new_entry->chksum = old_entry->chksum;
        new_entry->name   = strdup(old_entry->name);

        old_delete_version = atomic_load(&(old_entry->base_delete_version));

        /** 
         * If the entry's base has been deleted for the provided version, set the 
         * new_list's base_delete_version to 1. Since the new_list's current version
         * at creation is 1 this means it's deleted.
         */
        if ( ( 0 < old_delete_version ) && ( version >= old_delete_version ) )
        {
            atomic_init(&(new_entry->base_delete_version), 1);
        }
        /* Else, set it 0 to show it's not deleted */
        else
        {
            atomic_init(&(new_entry->base_delete_version), 0);
        }
         
                        
        old_base = atomic_load(&(old_entry->base));
        
        /**
         * If old_base.ptr is not NULL and the new base isn't deleted, atomically 
         * increment the parent class's property's ref_count, and atomically set the 
         * new base to point to the parent's property. 
         * 
         * NOTE: if the base is deleted we set the pointer to NULL, because we can't
         * access it anyway so it's pointless to go through the process. If this proves
         * incorrect in testing, it will be fixed.
         */
        if ( old_base.ptr && ( 0 == (atomic_load(&(new_entry->base_delete_version))) ) )
        {
            do 
            {
                old_prop = old_base.ptr;

                old_prop_ref_count = atomic_load(&(old_prop->ref_count));
                new_prop_ref_count = (old_prop_ref_count + 1);

                parent = old_list->pclass_ptr;

                if ( ! atomic_compare_exchange_strong(&(old_prop->ref_count), 
                                                        &old_prop_ref_count, 
                                                        new_prop_ref_count) )
                {
                    /* failed, update stats and try again */
                    atomic_fetch_add(&(parent->num_prop_ref_count_cols), 1);
                }
                else
                {
                    /* success, update stats and continue */

                    atomic_fetch_add(&(parent->num_prop_ref_count_update), 1);

                    new_base.ptr = old_base.ptr;
                    new_base.ver = 1;
                    atomic_store(&(new_entry->base), new_base);

                    done = TRUE;
                }

            } while ( ! done );
        }
        /* If old_base.ptr is NULL or is if base is deleted, set new base.ptr to NULL */
        else 
        {
            new_base.ptr = NULL;
            new_base.ver = 0;
            atomic_store(&(new_entry->base), new_base);
        }

        done = FALSE;

        old_curr = atomic_load(&(old_entry->curr));

        /* If the old entry's curr.ptr isn't NULL, create a new_prop as a copy of it */
        if ( old_curr.ptr )
        {
            old_prop = old_curr.ptr;

            if ( ( 0 == (atomic_load(&(old_prop->delete_version))) ) ||
                 ( version <= (atomic_load(&(old_prop->delete_version)))) )
            {
                old_prop_value = atomic_load(&(old_prop->value));

                new_prop = H5P__mt_create_prop(old_prop->name, old_prop_value.ptr,
                                            old_prop_value.size, FALSE, 1, 
                                            old_prop->create, old_prop->set, 
                                            old_prop->get, old_prop->encode, 
                                            old_prop->decode, old_prop->del, 
                                            old_prop->copy, old_prop->cmp, 
                                            old_prop->close);
                if ( NULL == new_prop )
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, 
                                "Failed creating property for property list.");

#if H5P_MT_SINGLE_THREAD_TESTING
#else
                /* If the new_prop has the copy callback, call it */
                if ( new_prop->copy )
                {
                    if ( (new_prop->copy)(new_prop->name, old_prop_value.size, 
                                                        old_prop_value.ptr) < 0 )
                    {
                        assert(H5P_MT_ASSERT_FAIL);
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");
                    }
                }
#endif
                new_prop->in_lkup_tbl = TRUE;

                /* Inserts the new_prop into the new_list's LFSLL */
                H5P__mt_ins_or_mod_prop__lfsll_ins(new_list->pl_head,
                                                new_prop,
                                                &deletes,
                                                &nodes_visited,
                                                &thrd_cols);

                atomic_fetch_add(&(new_list->log_pl_len), 1);
                atomic_fetch_add(&(new_list->phys_pl_len), 1);

                new_curr.ptr = new_prop;
                new_curr.ver = 1;
                atomic_store(&(new_entry->curr), new_curr);

                done = TRUE;

            } /* end if () */

        } /* if ( old_curr.ptr ) */
        
        /* if done = FALSE, then atomically set new_curr.ptr to NULL */
        if ( ! done )
        {
            new_curr.ptr = NULL;
            new_curr.ver = 0;
            atomic_store(&(new_entry->curr), new_curr);
        }
        
    } /* end for ( nprops = 0; nprops < new_list->nprops_inherited; nprops++ ) */


done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__init_lkup_tbl_copy() */



/****************************************************************************************
 * Function:    H5P__create_sentinels
 *
 * Purpose:     Creates the two sentinel nodes for a class's or list's LFSLL            
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
    H5P_mt_prop_t              * pos_sentinel = NULL;
    H5P_mt_prop_aptr_t           pos_next;
    H5P_mt_prop_value_t          pos_value;
    H5P_mt_prop_t              * neg_sentinel = NULL;
    H5P_mt_prop_aptr_t           neg_next;
    H5P_mt_prop_value_t          neg_value;

    H5P_mt_prop_t * ret_value = NULL;


    FUNC_ENTER_PACKAGE


    /* Allocating and initializing the positive sentinel node of the LFSLL */
    pos_sentinel = (H5P_mt_prop_t *)malloc(sizeof(H5P_mt_prop_t));
    if ( pos_sentinel == NULL )
    {
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

    pos_sentinel->in_lkup_tbl   = FALSE;

    pos_sentinel->chksum = LLONG_MAX;

    pos_sentinel->name = strdup("pos_sentinel");

    pos_value.ptr  = NULL;
    pos_value.size = 0;

    atomic_store(&(pos_sentinel->value), pos_value);
    
    atomic_store(&(pos_sentinel->create_version), 1);
    atomic_store(&(pos_sentinel->delete_version), 0); 


    /* Allocating and initializing the negative sentinel node of the LFSLL */
    neg_sentinel = (H5P_mt_prop_t *)malloc(sizeof(H5P_mt_prop_t));
    if ( neg_sentinel == NULL )
    {
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

    neg_value.ptr  = NULL;
    neg_value.size = 0;

    atomic_store(&(neg_sentinel->value), neg_value);
    
    atomic_store(&(neg_sentinel->create_version), 1);
    atomic_store(&(neg_sentinel->delete_version), 0); /* Set to 0 because it's not deleted */


done:

    ret_value = neg_sentinel;

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
 *              when this modification occured and is valid.
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
H5P__mt_create_prop(const char *name, void *value_ptr, size_t value_size, 
                    bool in_prop_class, uint64_t version,
                    H5P_prp_create_func_t prp_create, H5P_prp_set_func_t prp_set, 
                    H5P_prp_get_func_t prp_get, H5P_prp_encode_func_t prp_encode,
                    H5P_prp_decode_func_t prp_decode, H5P_prp_delete_func_t prp_del,
                    H5P_prp_copy_func_t prp_copy, H5P_prp_compare_func_t prp_cmp,
                    H5P_prp_close_func_t prp_close)
{
    H5P_mt_prop_t     * new_prop;
    H5P_mt_prop_aptr_t  next;
    H5P_mt_prop_value_t value;
    size_t              name_len;

    H5P_mt_prop_t * ret_value = NULL;

    FUNC_ENTER_PACKAGE

    assert(name);

    /* Allocate memory for the new property */
    new_prop = H5P__mt_alloc_prop();

    if ( NULL == new_prop )
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "property allocation failed");
    }

    /* Initalize property fields */
    atomic_store(&(new_prop->tag), H5P_MT_PROP_TAG);

    next.ptr = NULL;
    next.deleted = FALSE;
    next.dummy_bool_1 = FALSE;
    next.dummy_bool_2 = FALSE;
    next.dummy_bool_3 = FALSE;
    atomic_store(&(new_prop->next), next);

    new_prop->sentinel      = FALSE;
    new_prop->in_prop_class = in_prop_class;

    atomic_store(&(new_prop->ref_count), 0);

    new_prop->in_lkup_tbl   = FALSE;

    name_len = strlen(name);

    new_prop->chksum = H5_checksum_metadata(name, name_len, 0);

    assert( new_prop->chksum > LLONG_MIN && new_prop->chksum < LLONG_MAX );

    new_prop->name = strdup(name);

    value.ptr = malloc(value_size);
    if ( NULL == value.ptr )
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "Value buffer allocation failed.");
    }

    memcpy(value.ptr, value_ptr, value_size);

    value.size = value_size;
    atomic_store(&(new_prop->value), value);
    
    atomic_store(&(new_prop->create_version), version);
    atomic_store(&(new_prop->delete_version), 0);

    /* Initialize the Callbacks */
    new_prop->callbacks_mt_safe = FALSE;

    new_prop->create = prp_create;
    new_prop->set    = prp_set;
    new_prop->get    = prp_get;
    new_prop->encode = prp_encode;
    new_prop->decode = prp_decode;
    new_prop->del    = prp_del;
    new_prop->copy   = prp_copy;
    new_prop->cmp    = prp_cmp;
    new_prop->close  = prp_close;


done:

    ret_value = new_prop;

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
    H5P_mt_prop_aptr_t fl_head;
    H5P_mt_prop_aptr_t fl_next;
    H5P_mt_prop_t    * head_prop;
    H5P_mt_prop_t    * new_prop = NULL;
    bool               done = FALSE;
    
    H5P_mt_prop_t    * ret_value = NULL;

    FUNC_ENTER_NOAPI(FAIL)

    fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));

    head_prop = fl_head.ptr;

    if ( head_prop == NULL ||
         ( atomic_load(&(head_prop->tag)) != H5P_MT_PROP_FL_REALLOC_TAG ) )
    {
        new_prop = (H5P_mt_prop_t *)malloc(sizeof(H5P_mt_prop_t));
        if ( NULL == new_prop )
        {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "property allocation failed");
        }  
    }
    else
    {
        do
        {
            fl_next = atomic_load(&(head_prop->next));

            fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));

            if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.prop_fl_head),
                                                  &fl_head, fl_next))
            {
                /* failed, update stats and try again */
                atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update_cols), 1);
            }
            else
            {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update), 1);
                atomic_fetch_sub(&(H5P_mt_g.prop_fl_len), 1);

                done = TRUE;
            }

        } while ( ! done );
        
        new_prop = H5P__clear_mt_prop(fl_head.ptr);
    }


done:

    ret_value = new_prop;
    
    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_alloc_prop() */



/****************************************************************************************
 * Function:    H5P__mt_copy_lfsll
 *
 * Purpose:     Given a list or class, and the head property of an existing list's or 
 *              class's LFSLL, and a version, this function copies the valid properties
 *              to the given list or class.
 * 
 *              Two things make a property invalid:
 *                  1) any property with a create_version greater than the parent class's
 *                     version from which the new class or list is derived.
 *                  2) any property with a delete_version less than or equal to the 
 *                     parent class's version from which the new class or list is derived
 * 
 *              When iterating the parent's LFSLL and the next valid property is found,
 *              the bool in_lkup_tbl is check and if TRUE we are done and can get the 
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
H5P__mt_copy_lfsll(void* param, H5P_mt_prop_t* old_prop, uint64_t version)
{
    H5P_mt_type_t       tag;
    H5P_mt_class_t    * new_class = NULL;
    H5P_mt_list_t     * new_list  = NULL;
    H5P_mt_prop_t     * new_plhead;
    H5P_mt_prop_t     * valid_prop;
    H5P_mt_prop_t     * new_prop;
    H5P_mt_prop_value_t value;
    uint32_t            phys_pl_len   = 0;
    uint32_t            log_pl_len    = 0;
    uint32_t            deletes       = 0;
    uint32_t            nodes_visited = 0;
    uint32_t            thrd_cols     = 0;

    herr_t              ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    /* Determines if this is a list or class */

    tag = *((H5P_mt_type_t *) param);

    if ( tag == CLASS_TAG )
    {
        new_class = (H5P_mt_class_t *)param;

        assert(new_class);
        assert(atomic_load(&(new_class->tag)) == H5P_MT_CLASS_TAG);

        new_plhead = new_class->pl_head;
    }
    else if ( tag == LIST_TAG )
    {
        new_list = (H5P_mt_list_t *)param;

        assert(new_list);
        assert(new_list->tag == H5P_MT_LIST_TAG);

        new_plhead = new_list->pl_head;
    }
    else
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                    "Type passed in wasn't a class or list.");   
    }


    assert(old_prop);
    assert(old_prop->tag == H5P_MT_PROP_TAG);
    assert(old_prop->sentinel);

    assert(new_plhead);
    assert(new_plhead->tag == H5P_MT_PROP_TAG);
    assert(new_plhead->sentinel);

    do
    {
        /* Gets the next valid prop or NULL if there isn't another valid */
        valid_prop = H5P__get_next_valid_prop(old_prop, version);

        
        if ( valid_prop )
        {
            
            if ( ! valid_prop->in_lkup_tbl )
            {
                value = atomic_load(&(valid_prop->value));

                /* Creates a new property from the parent's valid_prop */
                new_prop = H5P__mt_create_prop(valid_prop->name, value.ptr, value.size,
                                               valid_prop->in_prop_class, 1, 
                                               valid_prop->create, valid_prop->set,
                                               valid_prop->get, valid_prop->encode, 
                                               valid_prop->decode, valid_prop->del, 
                                               valid_prop->copy, valid_prop->cmp, 
                                               valid_prop->close);
                if ( NULL == new_prop )
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, 
                                "Failed creating property for property list class.");

#if H5P_MT_SINGLE_THREAD_TESTING
#else

                if ( new_list )
                {
                    if ( new_prop->copy )
                    {
                        if ( (new_prop->copy)(new_prop->name, value.size, 
                                                            value.ptr) < 0 )
                        {
                            assert(H5P_MT_ASSERT_FAIL);
                            HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "Can't copy property");
                        }
                    }
                }
#endif
                /* Inserts the new_prop into the new_class's LFSLL */
                H5P__mt_ins_or_mod_prop__lfsll_ins(new_plhead,
                                                new_prop,
                                                &deletes,
                                                &nodes_visited,
                                                &thrd_cols);


                phys_pl_len++;
                log_pl_len++;
           
            } /* end if ( ! done ) */

            old_prop = valid_prop;

        } /* end if ( valid_prop ) */

    } while ( valid_prop );

    if ( new_class )
    {
        /* update physical and logical lengths */
        atomic_fetch_add(&(new_class->phys_pl_len), phys_pl_len);
        atomic_fetch_add(&(new_class->log_pl_len), log_pl_len);
    }
    else
    {
        /* update physical and logical lengths */
        atomic_fetch_add(&(new_list->phys_pl_len), phys_pl_len);
        atomic_fetch_add(&(new_list->log_pl_len), log_pl_len);
    }


done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_copy_lfsll() */



/****************************************************************************************
 * Function:    H5P__mt_ins_or_mod_prop__main
 *
 * Purpose:     Inserts a new property (H5P_mt_prop_t struct) into the LFSLL of a 
 *              property list (H5P_mt_list_t) or property list class (H5P_mt_class_t).
 *
 *              NOTE: When 'modifying' a property is this multithread safe version of 
 *              H5P, a new property must be created with a new create_version. Because
 *              of this, when an entirely new property is created, or a new property is
 *              created for the new version of a property, this function is called to 
 *              handle either case.
 * 
 *              This function first checks if it is dealing with a class or a list, and
 *              calls H5P__mt_create_prop() to create the new H5P_mt_prop_t struct, for
 *              either the list or class. If dealing with a list, and either the 
 *              parameter bool create or bool copy is TRUE, the associated callback is
 *              called.
 * 
 *              The primary purpose of this function is creating the new property to be
 *              inserted, handling incrementing and decrementing thread count, updating 
 *              the version of the host structure of the LFSLL after the property was 
 *              inserted, and updating stats. 
 * 
 *              A helper function, H5P__mt_ins_or_mod_prop__lfsll_ins(), is called that
 *              performs the actual insertion of the property in to the LFSLL.
 * 
 *              Lists have an extra step here. In case this is a 'modification' to an 
 *              existing property, the list must iterate its lkup_tbl and if it contains 
 *              another version of the new property, it must update the curr.ptr to point 
 *              to the new property, update curr.ver to the version of the new 
 *              property, and set the new_prop field in_lkup_tbl to TRUE.
 * 
 *              NOTE: See the comment description above struct H5P_mt_prop_t in 
 *              H5Ppkg_mt.h for details on how properties are sorted.
 * 
 *              NOTE: For multiple threads simultaneiously either modifying, inserting,
 *              or deleting a property in the list, there is an ordering that must be 
 *              followed. See the comment description above H5P_mt_list_t or 
 *              H5P_mt_class_t in H5Ppkg_mt.h for more details.
 *
 * Return:      SUCCEED/FAIL    
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_ins_or_mod_prop__main(void *param, const char *name, void *value, size_t size,
                              bool create, bool copy,
                              H5P_prp_create_func_t prp_create,
                              H5P_prp_set_func_t prp_set, 
                              H5P_prp_get_func_t prp_get, 
                              H5P_prp_encode_func_t prp_encode, 
                              H5P_prp_decode_func_t prp_decode, 
                              H5P_prp_delete_func_t prp_del,
                              H5P_prp_copy_func_t prp_copy, 
                              H5P_prp_compare_func_t prp_cmp,
                              H5P_prp_close_func_t prp_close)
{
    H5P_mt_prop_t             * new_prop = NULL;
    H5P_mt_prop_t             * pl_head;
    H5P_mt_prop_t             * next_prop;
    H5P_mt_prop_aptr_t          next;
    H5P_mt_prop_value_t         prop_value;
    uint64_t                    curr_version  = 0;
    uint64_t                    next_version  = 0;
    uint32_t                    deletes       = 0;
    uint32_t                    nodes_visited = 0;
    uint32_t                    thrd_cols     = 0;
    bool                        done          = FALSE;
    bool                        inc_thrd_flag = FALSE;
    H5P_mt_type_t               tag;
    H5P_mt_class_t            * class = NULL;
    H5P_mt_list_t             * list  = NULL;
    H5P_mt_list_table_entry_t * entry;
    H5P_mt_list_prop_ref_t      curr;
    H5P_mt_list_prop_ref_t      new_curr;

    herr_t                      ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(name);
    assert((size > 0 && value != NULL) || (size == 0));


    tag = *((H5P_mt_type_t *) param);

    if ( tag == CLASS_TAG )
    {
        class = (H5P_mt_class_t *)param;

        assert(class);
        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);

        /* update stats */
        atomic_fetch_add(&(class->H5P__insert_prop_setup__num_calls), 1);

        /* Increment thread count */
        if ( ( H5P__inc_thrd_count(class) ) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL,
                        "Couldn't increment class's thread count.");
        else
            inc_thrd_flag = TRUE;



        curr_version = atomic_load(&(class->curr_version));
        next_version = atomic_fetch_add(&(class->next_version), 1);

        /* Ensure another thread isn't modifying the LFSLL*/
        if ( (curr_version + 1 ) < next_version )
        {
            if ((curr_version = H5P__mt_version_check(class, curr_version, next_version)) == 0 )
                HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                            "Error with returned current version.");


            curr_version = atomic_load(&(class->curr_version));
        }


        /* This thread can now proceed and create the new property */
        new_prop = H5P__mt_create_prop(name, value, size, TRUE, next_version,
                                       prp_create, prp_set, prp_get, prp_encode, 
                                       prp_decode, prp_del, prp_copy, prp_cmp, 
                                       prp_close);
        
        if ( NULL == new_prop )
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, 
                        "Failed to create new property.");

        pl_head = class->pl_head;
    } /* end if ( tag == CLASS_TAG )*/

    else if ( tag == LIST_TAG )
    {
        list = (H5P_mt_list_t *)param;

        assert(list);
        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

        /* update stats */
        atomic_fetch_add(&(list->H5P__insert_prop_setup__num_calls), 1);

        /* Increment thread count */
        if ( ( H5P__inc_thrd_count(list) ) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                        "Couldn't increment list's thread count.");
        else
            inc_thrd_flag = TRUE;


        curr_version = atomic_load(&(list->curr_version));
        next_version = atomic_fetch_add(&(list->next_version), 1);

        /* Ensure another thread isn't modifying the LFSLL*/
        if ( (curr_version + 1 ) < next_version )
        {
            if ((curr_version = H5P__mt_version_check(list, curr_version, next_version)) == 0 )
                HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                            "Error with returned current version.");

        }


        /* This thread can now proceed and create the new property */
        new_prop = H5P__mt_create_prop(name, value, size, FALSE, next_version,
                                       prp_create, prp_set, prp_get, prp_encode, 
                                       prp_decode, prp_del, prp_copy, prp_cmp, 
                                       prp_close);
        
        if ( NULL == new_prop )
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, 
                        "Failed to create new property.");

        pl_head = list->pl_head;


        /**** Perform appropriate callback ****/

        prop_value = atomic_load(&(new_prop->value));

        /* If copy is TRUE and the copy callback exists call it */
        if ( copy )
        {
            if ( new_prop->copy )
            {
                if ( (new_prop->copy)(new_prop->name, prop_value.size, prop_value.ptr) )
                {
                    assert(H5P_MT_ASSERT_FAIL);
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, 
                                "Copy property callback failed");
                }
            }
        }
        /* If create is TRUE and the create callback exists call it */
        else if ( create )
        {
            if ( new_prop->create )
            {
                if ( (new_prop->create)(new_prop->name, prop_value.size, prop_value.ptr) )
                {
                    assert(H5P_MT_ASSERT_FAIL);
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, 
                                "Create property callback failed");
                }
            }
        }
    #if 0
        /* If set is TRUE and a set callback exists call it */
        else if ( set )
        {

            if ( new_prop->set )
            {
                /* Make a copy of the current value, in case the callback fails */
                if (NULL == (tmp_value.ptr = H5MM_malloc(prop_value.size)))
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, 
                                "memory allocation failed temporary property value");
                
                H5MM_memcpy(tmp_value.ptr, value, prop_value.size);

                if ( (*(new_prop->set))(list->plist_id, name, prop_value.size, 
                                        tmp_value.ptr) < 0 )
                {
                    assert(H5P_MT_ASSERT_FAIL);
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, 
                                "can't set property value");
                }
                
            }
            else
                tmp_value.ptr = 

            set_value.ptr = NULL;
            set_value.size = tmp_value.size;

            H5MM_memcpy(set_value.ptr, tmp_value.ptr, prop_value.size)
        }
#endif
    } /* end if ( tag == LIST_TAG ) */

    else
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                    "Type passed in wasn't a class or list.");
    }

    assert(curr_version + 1 ==  next_version); 

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    assert(pl_head);
    assert(atomic_load(&(pl_head->tag)) == H5P_MT_PROP_TAG);
    assert(pl_head->sentinel);



    H5P__mt_ins_or_mod_prop__lfsll_ins(pl_head,
                                       new_prop,
                                       &deletes,
                                       &nodes_visited,
                                       &thrd_cols);

    
    /* If this is a list, check if this is a 'modification' to a prop in the lkup_tbl */
    if ( ! class )
    {
        /* If copying is TRUE, then lkup_tbl has already been searched */
        if ( ! copy )
        {
            entry = H5P__mt_search_lkup_tbl(list->lkup_tbl, 0, 
                                        (list->nprops_inherited - 1), new_prop->chksum);

            /* If entry isn't NULL a version exists in the lkup_tbl */
            if ( entry )
            {
                /* Atomically update entry->curr to point to the new version of the property*/
                done = FALSE;
                do 
                {
                    curr = atomic_load(&(entry->curr));

                    assert(curr.ver < next_version);

                    new_curr.ptr = new_prop;
                    new_curr.ver = next_version;

                    if ( ! atomic_compare_exchange_strong(&(entry->curr), &curr, new_curr) )
                    {
                        /* attempt failed, update stats and try again */
                        atomic_fetch_add(&(list->num_insert_update_entry_cols), 1);
                    }
                    else
                    {
                        /* attempt succeeded, update stats and continue */
                        atomic_fetch_add(&(list->num_insert_update_entry_success), 1);

                        /* An entry's curr.ptr points to this prop, mark in_lkup_tbl */
                        new_prop->in_lkup_tbl = TRUE;

                        done = TRUE;

                    }

                } while ( ! done );
    
            } /* end if ( entry ) */
        
        } /* end if ( ! copy ) */
    
    } /* end if ( ! class ) */ 

    if ( class )
    {
        /* update stats */
        atomic_store(&(class->num_insert_nodes_visited), nodes_visited);
        atomic_fetch_add(&(class->num_insert_prop_cols), thrd_cols);
        atomic_fetch_add(&(class->num_insert_prop_success), 1);

        next = atomic_load(&(new_prop->next));
        next_prop = next.ptr;
        
        /** 
         * If the next prop in the lfsll has the same chksum then don't 
         * increment logical length, because this new prop is just a modification
         * to an existing prop.
         */
        if ( new_prop->chksum != next_prop->chksum )
        {
            atomic_fetch_add(&(class->log_pl_len),   1);
        }

        /* Increment physical length of the lfsll */
        atomic_fetch_add(&(class->phys_pl_len),  1);

        /* Update curr_version */
        atomic_store(&(class->curr_version), next_version);

        /* Update the class's current version */
        atomic_store(&(class->curr_version), next_version);


    }
    else
    {
        /* update stats */
        atomic_store(&(list->num_insert_nodes_visited), nodes_visited);
        atomic_fetch_add(&(list->num_insert_prop_cols), thrd_cols);
        atomic_fetch_add(&(list->num_insert_prop_success), 1);

        next = atomic_load(&(new_prop->next));
        next_prop = next.ptr;

        /** 
         * If the next prop in the lfsll has the same chksum then don't 
         * increment logical length, nprops_added, or nprops, because this
         * new prop is just a modification to an existing prop.
         */
        if ( new_prop->chksum != next_prop->chksum )
        {
            atomic_fetch_add(&(list->log_pl_len),   1);
            atomic_fetch_add(&(list->nprops_added), 1);
            atomic_fetch_add(&(list->nprops),       1);
        }

        /* Increment physical length of the lfsll */
        atomic_fetch_add(&(list->phys_pl_len),  1);


        /* Update curr_version */
        atomic_store(&(list->curr_version), next_version);

        /* Update the list's current version */
        atomic_store(&(list->curr_version), next_version);

    }


done:

    if ( ( ret_value == FAIL ) && ( new_prop != NULL ) )
    {
        free(new_prop);
    }

    /* If the parent's thrd_count was incremented, it must be decremented */
    if ( inc_thrd_flag )
    {
        if ( class )
        {
            if ( 0 > H5P__dec_thrd_count(class) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                            "Failure to decrement thrd_count.");
        }
        else
        {
            if ( 0 > H5P__dec_thrd_count(list) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                            "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_ins_or_mod_prop__main() */



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
 * Return:      SUCCEED/FAIL    
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_ins_or_mod_prop__lfsll_ins(H5P_mt_prop_t *pl_head, 
                                   H5P_mt_prop_t *new_prop, 
                                   uint32_t *deletes_ptr,
                                   uint32_t *nodes_visited_ptr,
                                   uint32_t *thrd_cols_ptr)
{
    H5P_mt_prop_t    * first_prop;
    H5P_mt_prop_t    * second_prop;
    H5P_mt_prop_aptr_t next;
    H5P_mt_prop_aptr_t updated_next;
    uint32_t           deletes       = 0;
    uint32_t           nodes_visited = 0;
    uint32_t           thrd_cols     = 0;
    bool               done          = FALSE;

    herr_t             ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR


    assert(pl_head);
    assert(atomic_load(&(pl_head->tag)) == H5P_MT_PROP_TAG);
    assert(pl_head->sentinel);
    
    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    do
    {
        first_prop  = NULL;
        second_prop = NULL;

        /** 
         * The current implementation of H5P__find_mod_point() should either succeed or 
         * trigger an assertion -- thus no need to check return value at present.
         */

        H5P__find_mod_point(pl_head,       
                            &first_prop,     
                            &second_prop,
                            &deletes,
                            &nodes_visited,
                            &thrd_cols,
                            new_prop);

        assert(first_prop);
        assert(second_prop);

        assert(atomic_load(&(first_prop->tag)) == H5P_MT_PROP_TAG);
        assert(atomic_load(&(second_prop->tag)) == H5P_MT_PROP_TAG);


        /* prep new_prop to be inserted between first_prop and second_prop */
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

        if ( ! atomic_compare_exchange_strong(&(first_prop->next),
                                                &next, updated_next) )
        {
            thrd_cols++;                    
        }
        else /* The attempt was successful update stats mark done */
        {
            done = TRUE;
        }

    } while ( ! done );

    /* Update pointers for stats collecting passed into the function */
    *deletes_ptr       = deletes;   
    *nodes_visited_ptr = nodes_visited;
    *thrd_cols_ptr     = thrd_cols;


    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_ins_or_mod_prop__lfsll_ins() */



/****************************************************************************************
 * Function:    H5P__set_delete_version
 *
 * Purpose:     Sets the delete_version of a property (H5P_mt_prop_t) in a property list
 *              (H5P_mt_list_t) or in a property list class (H5P_mt_class_t);
 *
 *              NOTE: Current implementation does not physically or logically delete 
 *              H5P_mt_prop_t structs from the LFSLL.
 *              
 *              H5P__set_delete_version() first determines if it's dealing with a list or
 *              a class. If it's a list we iterate the lkup_tbl with the function 
 *              H5P__mt_search_lkup_tbl() and if the property is found that entry is 
 *              returned, else NULL is returned. If entry is not NULL, we must find the
 *              correct version of the property using H5P__mt_entry_find_version(). If 
 *              base_flag is TRUE, we set the entry's base_delete_version and are done.  
 *              If base_flag is FALSE, set the delete_version of the returned property.
 * 
 *              If the list's lkup_tbl doesn't contain the target_prop or if this is a 
 *              class we must iterate the LFSLL to find the target_prop using 
 *              H5P__find_mod_point(), and set the delete_version of the returned 
 *              property that the pointer second_prop points to.
 * 
 *              Finally, we update stats and decrement the thrd_count of the structure 
 *              this thread is in.
 * 
 * 
 *              NOTE: For multiple threads simultaneiously either modifying, inserting,
 *              or deleting a property in the class, there is an ordering that must be 
 *              followed. See the comment description above H5P_mt_class_t in H5Ppkg_mt.h
 *              for more details.
 * 
 *
 * Return:      SUCCEED/FAIL    
 *
 ****************************************************************************************
 */
herr_t
H5P__set_delete_version(void *param, H5P_mt_prop_t *target_prop)
{
    H5P_mt_prop_t             * first_prop;
    H5P_mt_prop_t             * second_prop;
    H5P_mt_prop_t             * pl_head;
    H5P_mt_list_table_entry_t * entry;
    H5P_mt_list_prop_ref_t      curr;
    uint64_t                    curr_version    = 0;
    uint64_t                    next_version    = 0;
    uint64_t                    delete_version  = 0;
    uint32_t                    deletes         = 0;
    uint32_t                    nodes_visited   = 0;
    uint32_t                    thrd_cols       = 0;
    bool                        done            = FALSE;
    bool                        base_flag       = FALSE;
    bool                        inc_thrd_flag   = FALSE;
    H5P_mt_type_t               tag;
    H5P_mt_class_t            * class = NULL;
    H5P_mt_list_t             * list  = NULL;

    herr_t                      ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE


    assert(target_prop);
    assert(target_prop->tag == H5P_MT_PROP_TAG);


    tag = *((H5P_mt_type_t *) param);

    if ( tag == CLASS_TAG )
    {
        class = (H5P_mt_class_t *)param;

        assert(class);
        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);

        /* update stats */
        atomic_fetch_add(&(class->H5P__set_delete_version__num_calls), 1);

        /* Increment thread count */
        if ( ( H5P__inc_thrd_count(class) ) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                        "Couldn't increment parent's thread count.");
        else
            inc_thrd_flag = TRUE;

        curr_version = atomic_load(&(class->curr_version));
        next_version = atomic_fetch_add(&(class->next_version), 1);

        /* Ensure another thread isn't modifying the LFSLL*/
        if ( (curr_version + 1 ) < next_version )
        {
            if ((curr_version = H5P__mt_version_check(class, curr_version, next_version)) == 0)
                HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                            "Error with returned current version.");

            curr_version = atomic_load(&(class->curr_version));
        }

        pl_head = class->pl_head;
    }
    else if ( tag == LIST_TAG )
    {
        list = (H5P_mt_list_t *)param;

        assert(list);
        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

        /* update stats */
        atomic_fetch_add(&(list->H5P__set_delete_version__num_calls), 1);

        /* Increment thread count */
        if ( ( H5P__inc_thrd_count(list) ) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                        "Couldn't increment parent's thread count.");
        else
            inc_thrd_flag = TRUE;

        curr_version = atomic_load(&(list->curr_version));
        next_version = atomic_fetch_add(&(list->next_version), 1);

        /* Ensure another thread isn't modifying the LFSLL*/
        if ( (curr_version + 1 ) < next_version )
        {
            if ((curr_version = H5P__mt_version_check(list, curr_version, next_version)) == 0)
                HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                            "Error with returned current version.");

            curr_version = atomic_load(&(class->curr_version));
        }

        pl_head = list->pl_head;
    }
    else
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                    "Type passed in wasn't a class or list.");
    }



    /* If this is a list, check the lkup_tbl for the target_prop */
    do
    {
        if ( ! class )
        {
            entry = H5P__mt_search_lkup_tbl(list->lkup_tbl, 0,
                                    (list->nprops_inherited -1), target_prop->chksum);

            if ( entry )
            {
                first_prop = H5P__mt_entry_find_version(entry, curr_version, &base_flag);

                /* If NULL, at the curr_version the prop was already deleted */
                if ( NULL == first_prop )
                {
                    assert(H5P_MT_ASSERT_FAIL);
                    HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                                "Property already deleted.");
                }

                /* If TRUE, the base of the entry is the most current version */
                if ( base_flag )
                {
                    assert( 0 == atomic_load(&(entry->base_delete_version)));

                    atomic_store(&(entry->base_delete_version), next_version);

                    /* update stats */
                    atomic_fetch_add(&(list->num_set_entry_base_delete_version), 1);

                    done = TRUE;
                }
                /* The target_prop was either curr or an older version in the LFSLL */
                else
                {
                    atomic_store(&(first_prop->delete_version), next_version);

                    /* update stats */
                    curr = atomic_load(&(entry->curr));

                    /** 
                     * NOTE: If this is true then the property that curr.ptr points 
                     * to is the target_prop, if not then curr.ptr points to a newer
                     * version of target_prop and we had to iterate the LFSLL to get
                     * the correct version. Must update stats accordingly.
                     */
                    if ( ( atomic_load(&(first_prop->create_version)) ) == 
                        ( atomic_load(&(curr.ptr->create_version)) ) )
                    {
                        atomic_fetch_add(&(list->num_set_delete_on_curr_entry), 1);
                    }
                    else
                        atomic_fetch_add(&(list->num_set_delete_older_ver_than_curr), 1);


                    done = TRUE;
                }
                
                /* Since a prop in the lkup_tbl was deleted, decrement nprops */
                atomic_fetch_sub(&(list->nprops), 1);

                /* update stats */
                atomic_fetch_add(&(list->num_set_delete_prop_success), 1);
            
            } /* end if ( entry ) */

        } /* end if ( ! class ) */



        /* If not a list or not in the list's lkup_tbl search the LFSLL */

        while ( ! done )
        {
            first_prop  = NULL;
            second_prop = NULL;

            /** 
             * The current implementation of H5P__find_mod_point() should either succeed or 
             * trigger an assertion -- thus no need to check return value at present.
             */            
            H5P__find_mod_point(pl_head,         
                                &first_prop,     
                                &second_prop,
                                &deletes,
                                &nodes_visited,
                                &thrd_cols,
                                target_prop);


            assert(first_prop);
            assert(second_prop);

            assert(atomic_load(&(first_prop->tag)) == H5P_MT_PROP_TAG);
            assert(atomic_load(&(second_prop->tag)) == H5P_MT_PROP_TAG);


            delete_version = atomic_load(&(second_prop->delete_version));

            assert(delete_version == 0);

            /* Set the prop's delete_version */
            atomic_store(&(second_prop->delete_version), next_version);

            done = TRUE;

            /* update stats and logical length to reflect a deleted prop in the lfsll */
            if ( class )
            {
                atomic_fetch_add(&(class->num_set_delete_prop_success), 1);

                atomic_fetch_sub(&(class->log_pl_len), 1);
            }
            else
            {
                atomic_fetch_add(&(list->num_set_delete_prop_success), 1);

                atomic_fetch_sub(&(list->log_pl_len), 1);
            }
                

        } /* end while ( ! done ) */

    } while ( ! done );

    assert(done);


    if ( class )
    {
        /* update stats */
        atomic_store(&(class->num_set_delete_nodes_visited), nodes_visited);
        atomic_fetch_add(&(class->num_set_delete_prop_cols), thrd_cols);

        /* Update the class's current version */
        atomic_store(&(class->curr_version), next_version);
    }
    else
    {
        /* update stats */
        atomic_store(&(list->num_set_delete_nodes_visited), nodes_visited);
        atomic_fetch_add(&(list->num_set_delete_prop_cols), thrd_cols);

        /* Update the list's current version */
        atomic_store(&(list->curr_version), next_version);
    }

done:

    /* If the parent's thrd_count was incremented, it must be decremented */
    if ( inc_thrd_flag )
    {
        if ( class )
        {
            if ( 0 > H5P__dec_thrd_count(class) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                            "Failure to decrement thrd_count.");
        }
        else
        {
            if ( 0 > H5P__dec_thrd_count(list) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                            "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__set_delete_version() */



/****************************************************************************************
 * Function:    H5P__mt_search_prop
 *
 * Purpose:     Searches a property list(H5P_mt_list_t) or in a property list class 
 *              (H5P_mt_class_t) for a target chksum. 
 * 
 *              This function follows the same procedure of the function above, 
 *              H5P__set_delete_version(), only instead of setting the delete_version of
 *              the target_property and returning SUCCEED or FAIL, this function returns
 *              the pointer to the property that matches the provided chksum. 
 *              
 *              NOTE: This function currently only returns the most current version of 
 *              the property being searched for. If H5P__mt_search_lkup_tbl() returns an
 *              entry, we do not call H5P__mt_entry_find_version() and instead return the
 *              most current version. Same when searching the lfsll, instead of calling 
 *              H5P__find_mod_point(), this function calls H5P__mt_search_lfsll() to only
 *              return the most recent version of the property being searched for.
 *
 * Return:      Success: Returns a pointer to the most recent version of the property.
 * 
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__mt_search_prop(void *param, int64_t chksum, const char *name)
{
    H5P_mt_prop_t             * prop = NULL;
    H5P_mt_prop_t             * pl_head;
    H5P_mt_list_table_entry_t * entry;
    H5P_mt_list_prop_ref_t      curr;
    H5P_mt_list_prop_ref_t      base;
    uint64_t                    curr_version  = 0;
    uint64_t                    delete_ver    = 0;
    uint32_t                    nodes_visited = 0;
    bool                        done          = FALSE;
    bool                        inc_thrd_flag = FALSE;
    H5P_mt_type_t               tag;
    H5P_mt_class_t            * class = NULL;
    H5P_mt_list_t             * list  = NULL;

    H5P_mt_prop_t             * ret_value = NULL;

    FUNC_ENTER_PACKAGE



    tag = *((H5P_mt_type_t *) param);

    if ( tag == CLASS_TAG )
    {
        class = (H5P_mt_class_t *)param;

        assert(class);
        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);

        /* update stats */
        atomic_fetch_add(&(class->H5P__search_prop__num_calls), 1);

        /* Increment thread count */
        if ( ( H5P__inc_thrd_count(class) ) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, 
                        "Couldn't increment parent's thread count.");
        else
            inc_thrd_flag = TRUE;

        /** TODO: replace this with function that gets version from context */
        curr_version = atomic_load(&(class->curr_version));
            
        pl_head = class->pl_head;
    }
    else if ( tag == LIST_TAG )
    {
        list = (H5P_mt_list_t *)param;

        assert(list);
        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

        /* update stats */
        atomic_fetch_add(&(list->H5P__search_prop__num_calls), 1);

        /* Increment thread count */
        if ( ( H5P__inc_thrd_count(list) ) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, 
                        "Couldn't increment parent's thread count.");
        else
            inc_thrd_flag = TRUE;
            

        /** TODO: replace this with function that gets version from context */
        curr_version = atomic_load(&(list->curr_version));

        pl_head = list->pl_head;
    }
    else
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, 
                    "Type passed in wasn't a class or list.");
    }



    /* If this is a list, check the lkup_tbl for the provided chksum */
    do 
    {
        if ( ! class )
        {
            entry = H5P__mt_search_lkup_tbl(list->lkup_tbl, 0,
                                            (list->nprops_inherited - 1), chksum);

            if ( entry )
            {
                assert(entry->chksum == chksum);
                assert( 0 == (strcmp(entry->name, name)) );

                curr = atomic_load(&(entry->curr));

                /* If curr.ptr isn't NULL, then set ret_value to most current version */
                if ( curr.ptr )
                {
                    prop = curr.ptr;
                    
                    /* Ensures the property isn't deleted */
                    delete_ver = atomic_load(&(prop->delete_version));

                    if ( delete_ver == 0 || delete_ver > curr_version )
                    {
                        ret_value = prop;

                        atomic_fetch_add(&(list->num_search_success), 1);

                        done = TRUE;
                    }
                    else
                    {
                        atomic_fetch_add(&(list->num_target_prop_found_but_deleted), 1);
                    }

                }
                /* If curr.ptr is NULL then most current version is base */
                else
                {
                    base = atomic_load(&(entry->base));
                    assert(base.ptr);

                    /* If the base_delete_version isn't set then ret_value = base.ptr */
                    if ( entry->base_delete_version == 0 )
                    {
                        ret_value = base.ptr;

                        atomic_fetch_add(&(list->num_search_success), 1);

                        done = TRUE;
                    }
                    else
                    {
                        atomic_fetch_add(&(list->num_target_prop_found_but_deleted), 1);
                    }
                }

            } /* end if ( entry ) */

        } /* end if ( ! class ) */
        


        /* If not a list or not in the list's lkup_tbl search the LFSLL */

        while ( ! done )
        {
            prop = H5P__mt_search_lfsll(pl_head, chksum, curr_version);

            if ( prop )
            {
                ret_value = prop;
            }

            done = TRUE;
                
        } /* end while ( ! done ) */


    } while ( ! done );

    if ( class )
    {
        /* update stats */
        atomic_store(&(class->num_search_nodes_visited), nodes_visited);
        if ( prop )
            atomic_fetch_add(&(class->num_search_success), 1);
    }
    else
    {
        /* update stats */
        atomic_store(&(list->num_search_nodes_visited), nodes_visited);
        if ( prop )
            atomic_fetch_add(&(list->num_search_success), 1);
    }

done:

    /* If the parent's thrd_count was incremented, it must be decremented */
    if ( inc_thrd_flag )
    {
        if ( class )
        {
            if ( 0 > H5P__dec_thrd_count(class) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                            "Failure to decrement thrd_count.");
        }
        else
        {
            if ( 0 > H5P__dec_thrd_count(list) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                            "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_search_prop() */



/****************************************************************************************
 * Function:    H5P__mt_search_lkup_tbl
 *
 * Purpose:     Searches a property list's (H5P_mt_list_t) lkup_tbl for an entry that has
 *              the same chksum as the provided chksum.
 * 
 *              The lkup_tbl is an array, so for efficiency an array binary search is 
 *              used to search it.
 *
 * Return:      Success: Returns a pointer to the entry in the lkup_tbl, or NULL if the
 *                       entry doesn't exist in the lkup_tbl.
 * 
 *              Failure: Can't fail
 *
 ****************************************************************************************
 */
H5P_mt_list_table_entry_t *
H5P__mt_search_lkup_tbl(H5P_mt_list_table_entry_t * lkup_tbl, 
                        uint32_t                    left_entry,
                        uint32_t                    right_entry,
                        int64_t                     chksum)
{
    H5P_mt_list_table_entry_t * entry;
    int32_t                     left;
    int32_t                     middle;
    int32_t                     right;

    H5P_mt_list_table_entry_t * ret_value = NULL;

    FUNC_ENTER_NOAPI_NOERR

    left  = (int32_t)left_entry;
    right = (int32_t)right_entry;


    /* Binary search the lkup_tbl for the chksum */
    while ( left <= right )
    {
        middle = left + (right - left) / 2;

        entry = &lkup_tbl[middle];

        if ( entry->chksum == chksum )
        {
            ret_value = entry;
            break;
        }            

        else if ( entry->chksum < chksum )
            left = middle + 1;

        else
            right = middle - 1;

    }


    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_search_lkup_tbl() */



/****************************************************************************************
 * Function:    H5P__mt_entry_find_version
 *
 * Purpose:     Finds the correct version of a property that was found in a list's 
 *              lkup_tbl.
 * 
 *              First we check if the entry's curr.ptr field is NULL. If it is then the 
 *              base version that points to the parent's property in the parent's LFSLL.
 *              We then set is_base to TRUE because if H5P__set_delete_version() is 
 *              calling this function it cannot set the delete version of the parent's
 *              property, so if is_base is TRUE it can instead set base_delete_version. 
 *              However, we still return the base.ptr, because if search is the calling
 *              function it needs that pointer to the property struct.
 *              
 *              If curr.ptr is not NULL, then we compare curr.ver to the target_prop's
 *              create_version. If they equal we return curr.ptr, else we follow 
 *              curr.ptr into the LFLL and iterate till we find the correct version, and
 *              we return the pointer to the correct version of that property.
 *              
 *
 * Return:      Success: Returns a pointer to the correct version of the property, and if
 *                       base.ptr is the correct version also sets the parameter   
 *                       bool *base_flag to TRUE, so the calling function knows base.ptr 
 *                       is the correct version.
 * 
 *              Failure: NULL  NOTE: this means for the provided version the prop is
 *                                   deleted.
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__mt_entry_find_version(H5P_mt_list_table_entry_t * entry, uint64_t version, 
                           bool *base_flag)
{
    H5P_mt_list_prop_ref_t curr;
    H5P_mt_list_prop_ref_t base;
    H5P_mt_prop_t        * prop;
    H5P_mt_prop_aptr_t     next;
    uint64_t               curr_create_ver;
    uint64_t               curr_delete_ver;
    uint64_t               base_delete_ver;
    bool                   done = FALSE;
    bool                   is_base = FALSE;

    H5P_mt_prop_t        * ret_value;

    FUNC_ENTER_NOAPI_NOERR


    curr = atomic_load(&(entry->curr));
    base = atomic_load(&(entry->base));

    if ( curr.ptr )
    {
        /**
         * Ensure the prop curr is the correct version, and not marked deleted
         * If not correct version iterate to the next version in the LFSLL.
         */
        do 
        {
            prop = curr.ptr;

            /* If the chksums don't match then we're now on a different prop */
            if ( prop->chksum != entry->chksum )
            {
                break;
            }

            curr_delete_ver = atomic_load(&(prop->delete_version));
            curr_create_ver = atomic_load(&(prop->create_version));

            /* If the property is the correct version for the given version */
            if ( curr_create_ver <= version )
            {
                /* Ensure the property hasn't been deleted */
                if ( curr_delete_ver == 0 || curr_delete_ver > version )
                {
                    ret_value = prop;
                }
                else
                {
                    HGOTO_DONE(NULL);
                }

                done = TRUE;
            }

            /* If not the correct version iterate to the next version in the lfsll */
            if ( ! done )
            {
                next = atomic_load(&(prop->next));
                prop = next.ptr;
            }

        
        } while ( ! done );

    } /* end if ( curr.ptr ) */

    /* If done is FALSE when we get here, check the base */
    if ( ! done )
    {
        base_delete_ver = atomic_load(&(entry->base_delete_version));

        /* If the base isn't deleted for the version, return it and set is_base */
        if ( base_delete_ver == 0 || base_delete_ver > version )
        {
            ret_value = base.ptr;

            is_base = TRUE;
        }
        else
        {
            HGOTO_DONE(NULL);
        }

    }

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
 *              or find the property that is being searched for or is to have its 
 *              delete_version set.
 * 
 *              As parameters this function takes the head of the LFSLL from either the
 *              class or list we are iterating, and two property double pointers, 
 *              first_ptr_ptr and second_ptr_ptr. The parameter target_prop is the 
 *              property we are trying to find, and the other parameters are for stats
 *              collecting. 
 *
 *              The helper function H5P__mt_compare_prop() is called to compare the 
 *              target_prop with second_prop from the LFSLL. 
 * 
 *              If the two properties are the same then first_prop, the property directly 
 *              before second_prop in the the LFSLL, and second_prop are passed back to 
 *              the calling function. This is the case when searching for a target_prop,
 *              when trying to set the delete_version of a property for 
 *              H5P__set_delete_version().
 * 
 *              If while iterating the LFSLL and comparing the properties, second_prop
 *              has a larger chksum value than target_prop, then first_prop, still the 
 *              property directly before second_prop in the LFSLL, and second_prop are
 *              passed back to the calling function. This is the case when inserting a 
 *              new property into the LFSLL, because the target_prop's chksum falls 
 *              between the first_prop and second_prop which is where it should be 
 *              inserted. 
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
H5P__find_mod_point(H5P_mt_prop_t  *pl_head,
                    H5P_mt_prop_t **first_ptr_ptr, 
                    H5P_mt_prop_t **second_ptr_ptr, 
                    uint32_t       *deletes_ptr, 
                    uint32_t       *nodes_visited_ptr, 
                    uint32_t       *thrd_cols_ptr, 
                    H5P_mt_prop_t  *target_prop)
{
    bool               done          = FALSE;
    uint32_t           thrd_cols     = 0;
    uint32_t           deletes       = 0;
    uint32_t           nodes_visited = 0;
    int32_t            cmp_result;
    H5P_mt_prop_t    * first_prop;
    H5P_mt_prop_t    * second_prop;
    H5P_mt_prop_aptr_t next_prop;

    herr_t             ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    assert(pl_head->sentinel);
    
    assert(first_ptr_ptr);
    assert(NULL == *first_ptr_ptr);
    assert(second_ptr_ptr);
    assert(NULL == *second_ptr_ptr);
    assert(deletes_ptr);
    assert(nodes_visited_ptr);
    assert(thrd_cols_ptr);
    assert(target_prop);
    assert(atomic_load(&(target_prop->tag)) == H5P_MT_PROP_TAG);
    assert(target_prop->chksum > LLONG_MIN && target_prop->chksum < LLONG_MAX);


    first_prop = atomic_load(&(pl_head));
        
    assert(first_prop);
    assert(atomic_load(&(first_prop->tag)) == H5P_MT_PROP_TAG);
    assert(first_prop->sentinel);


    next_prop = atomic_load(&(first_prop->next));
    second_prop = next_prop.ptr;

    assert(second_prop);
    assert(atomic_load(&(second_prop->tag)) == H5P_MT_PROP_TAG);

    /* Iterate through the LFSLL of properties */
    do 
    {
        cmp_result = H5P__mt_compare_prop(second_prop, target_prop);
        /**
         * 0 = the two properties are the same.
         * 1 = second_prop comes after target_prop in the LFSLL.
         */ 
        if ( ( cmp_result == 0 ) || ( cmp_result == 1 ) )
        {

            done = TRUE;
        }
        /* -1 = second_prop comes before target_prop, must iterate lfsll*/
        else if ( cmp_result == -1 )
        {
            next_prop = atomic_load(&(second_prop->next));

            first_prop = second_prop;
            second_prop = next_prop.ptr;

            assert(second_prop);
            assert(atomic_load(&(second_prop->tag)) == H5P_MT_PROP_TAG);

            nodes_visited++;        
        } 
    
    } while ( ! done ); 

    assert(done);

    assert(first_prop->chksum <= target_prop->chksum);
    assert(second_prop->chksum >= target_prop->chksum);

    /**
     * Update the pointers passed into the function with the pointers for the 
     * correct positions in the LFSLL, and the stats gathered while iterating.
     */
    *first_ptr_ptr      = first_prop;
    *second_ptr_ptr     = second_prop;
    *thrd_cols_ptr     += thrd_cols;
    *deletes_ptr       += deletes;
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
H5P__mt_search_lfsll(H5P_mt_prop_t *pl_head, int64_t chksum, uint64_t version)
{
    H5P_mt_prop_t    * lfsll_prop;
    H5P_mt_prop_t    * valid_prop;
    bool               done = FALSE;

    H5P_mt_prop_t    * ret_value = NULL;

    FUNC_ENTER_NOAPI_NOERR

    assert(pl_head);
    assert((atomic_load(&(pl_head->tag))) == H5P_MT_PROP_TAG);
    assert(pl_head->sentinel);

    lfsll_prop = pl_head;

    /* Iterate the lfsll until chksums match or lfsll_prop->chksum is larger */
    do
    {
        valid_prop = H5P__get_next_valid_prop(lfsll_prop, version);

        /* If the valid prop chksum matches the target chksum return valid prop */
        if (valid_prop->chksum == chksum)
        {
            ret_value = valid_prop;
            
            done = TRUE;
        }
        /* If valid prop is NULL we searched the lfsll and there isn't a valid version */
        else if ( valid_prop == NULL )
        {
            done = TRUE;
        }
        else
        {
            lfsll_prop = valid_prop;
        }

    } while ( ! done );


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
 *              property in correllation with the version passed to the function, is the
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
 *
 * Return:      Success: Returns a pointer to the next valid property in a LFSLL, 
 *                       or NULL if there is not a next valid.
 * 
 *              Failure: Can not fail
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__get_next_valid_prop(H5P_mt_prop_t *prop, uint64_t version)
{
    H5P_mt_prop_t    * check_prop;
    H5P_mt_prop_t    * returned_prop;
    H5P_mt_prop_aptr_t next;
    int64_t            curr_chksum;
    int64_t            next_chksum;
    bool               done = FALSE;
    bool               iterate = FALSE;

    H5P_mt_prop_t    * ret_value = NULL;

    FUNC_ENTER_NOAPI_NOERR


    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);


    check_prop = prop;

    /* Iterate to find the next new chksum */
    do 
    {
        curr_chksum = check_prop->chksum;

        next = atomic_load(&(check_prop->next));
        check_prop = next.ptr;
    
        next_chksum = check_prop->chksum;

        iterate = FALSE;

        /* If TRUE then not a new property and need to iterate again */
        if ( curr_chksum == next_chksum )
        {
            iterate = TRUE;
        }

        /* If iterate is false check for a valid version of the property */
        if ( ! iterate )
        {

            do 
            {
                /* If TRUE then we've reached the pos sentinel and can exit */
                if ( next_chksum == LLONG_MAX )
                {
                    ret_value = NULL;
                    done = TRUE;
                    break;
                }

                returned_prop = H5P__find_valid_version(check_prop, version);

                /* If NULL then no versions are valid and we must iterate the LFSLL */
                if ( ! returned_prop )
                {
                    iterate = TRUE;
                }
                /* If chksums equal then next_valid_prop is the valid version */
                else if ( returned_prop->chksum == check_prop->chksum )
                {
                    ret_value = returned_prop;
                    done = TRUE;
                }
                /**
                 * If chksums differ then all versions were iterated and none were valid,
                 * and returned_prop is the most current version of the next prop to 
                 * check.
                 */
                else
                {
                    check_prop = returned_prop;
                    next_chksum = check_prop->chksum;
                }
                
            } while ( ( ! iterate ) && ( ! done ) );
        }

    } while ( ! done );
    
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
H5P__find_valid_version(H5P_mt_prop_t *prop, uint64_t version)
{
    H5P_mt_prop_t    * check_prop;
    H5P_mt_prop_aptr_t next;
    bool               done  = FALSE;
    int32_t            valid = 0;

    H5P_mt_prop_t    * ret_value;

    FUNC_ENTER_NOAPI_NOERR

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

    check_prop = prop;

    /* Find a valid version of the property, if one exists */
    do 
    {
        valid = H5P__is_valid(check_prop, version);

        /* check_prop is valid */
        if ( valid == 0 )
        {
            ret_value = check_prop;
            done = TRUE;
        }
        /* No version of check_prop is valid */
        else if ( valid == -1 )
        {
            ret_value = NULL;
            done = TRUE;
        }
        /* check_prop is not valid, iterate to the next version */
        else
        {
            next = atomic_load(&(check_prop->next));
            check_prop = next.ptr;

            /* If TRUE then we're on a different prop and no versions were valid */
            if ( check_prop->chksum != prop->chksum )
            {
                ret_value = check_prop;
                done = TRUE;
            }
        }

    } while ( ! done );

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__find_valid_version() */



/****************************************************************************************
 * Function:    H5P__is_valid
 *
 * Purpose:     Checks if a given property is valid based on the provided version.
 *              
 *               0 = property is valid,
 *               1 = this version of the property is not valid
 *              -1 = The delete_version is set, so no version is valid.
 * 
 *              NOTE: we return an int32_t instead of herr_t or a bool because if the 
 *              delete_version is greater than 0 and less than or equal to the version we 
 *              are checking if valid for, we know that no other version is valid. 
 *              Passing this information back could prevent further iteration in the 
 *              LFSLL to check for a valid version when we already know there isn't one.
 *
 * Return:      Success: Returns an int32_t
 * 
 *              Failure: Can not fail
 *
 ****************************************************************************************
 */
int32_t
H5P__is_valid(H5P_mt_prop_t *prop, uint64_t version)
{
    uint64_t create_ver;
    uint64_t delete_ver;

    int32_t ret_value = 0;

    FUNC_ENTER_NOAPI_NOERR

    create_ver = atomic_load(&(prop->create_version));
    delete_ver = atomic_load(&(prop->delete_version));

    /* If TRUE then prop is a valid version */
    if ( ( create_ver <= version ) && 
        ( ( delete_ver == 0 ) || delete_ver > version ) )
    {
        ret_value = 0;
    }
    /* If TRUE then no version of this property is valid */
    else if ( ( delete_ver > 0 ) && ( delete_ver <= version ) )
    {
        ret_value = -1;
    }
    /* If TRUE this property is not valid, but another version might be */
    else
    {
        ret_value = 1;
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__is_valid() */



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



/****************************************************************************************
 * Function:    H5P__mt_is_equal()
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
H5P__mt_is_equal(H5P_mt_prop_t *prop1, H5P_mt_prop_t *prop2)
{
    H5P_mt_prop_value_t value1;
    H5P_mt_prop_value_t value2;

    int32_t             ret_value = 0;

    FUNC_ENTER_PACKAGE

    assert(prop1);
    assert(atomic_load(&(prop1->tag)) == H5P_MT_PROP_TAG);

    assert(prop2);
    assert(atomic_load(&(prop2->tag)) == H5P_MT_PROP_TAG);



    if ( prop1->chksum != prop2->chksum )
        HGOTO_DONE(1);

    if ( 0 != strcmp(prop1->name, prop2->name))
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, (int32_t)(-1), 
                    "Two different properties have the same checksum.");
    }

    if ( prop1->in_prop_class != prop2->in_prop_class )
        HGOTO_DONE(1);
    if ( prop1->in_lkup_tbl != prop2->in_lkup_tbl )
        HGOTO_DONE(1);

    if ( atomic_load(&(prop1->ref_count)) != atomic_load(&(prop2->ref_count)) )
        HGOTO_DONE(1);

    
    value1 = atomic_load(&(prop1->value));
    value2 = atomic_load(&(prop2->value));

    /* Compare value size and pointer */
    if ( value1.size != value2.size )
        HGOTO_DONE(1);
    if ( value1.ptr == NULL && value2.ptr != NULL )
        HGOTO_DONE(1);
    if ( value1.ptr != NULL && value2.ptr == NULL )
        HGOTO_DONE(1);
    if (0 != memcmp(value1.ptr, value2.ptr, value1.size))
        HGOTO_DONE(1);

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


done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_is_equal() */



/****************************************************************************************
 * Function:    H5P__mt_cmp_list_or_class()
 * 
 * Purpose:     Compares two lists (H5P_mt_list_t) or two classes (H5P_mt_class_t) and 
 *              determines if they are equal or not.
 * 
 *              First the parameters are checked to ensure they are either both lists or
 *              both classes, then each one has it's thread count incremented, and the
 *              current version is checked of them both. The two parameters more basic 
 *              fields are compared and if any are not equal, 1 is returned. 
 * 
 *              Lists additionally must compare their lkup_tbls, and do this by iterating
 *              each of them and comparing each field.
 * 
 *              Finally the lfsll are iterated and each property is compared. This is 
 *              done by calling H5P__get_next_valid_prop() on both parameters and 
 *              H5P__mt_compare_prop on what they returned. Since we have the same 
 *              current version number for both parameters if any returned properties
 *              don't match the two are not equal.
 * 
 * 
 * Return:      Success: 0 means the two parameters are equal
 *                       1 means the two parameters are not equal
 * 
 *              Failure: -1
 ****************************************************************************************
 */
int32_t
H5P__mt_cmp_list_or_class(void *param1, void *param2)
{
    H5P_mt_type_t    tag1;
    H5P_mt_type_t    tag2;
    H5P_mt_class_t * class1          = NULL;
    H5P_mt_class_t * class2          = NULL;
    H5P_mt_list_t  * list1           = NULL;
    H5P_mt_list_t  * list2           = NULL;
    H5P_mt_list_table_entry_t * entry1;  
    H5P_mt_list_table_entry_t * entry2;
    H5P_mt_list_prop_ref_t      base1;
    H5P_mt_list_prop_ref_t      base2;
    H5P_mt_list_prop_ref_t      curr1;
    H5P_mt_list_prop_ref_t      curr2;
    H5P_mt_prop_t             * prev_prop1 = NULL;
    H5P_mt_prop_t             * prev_prop2 = NULL;
    H5P_mt_prop_t             * valid_prop1;
    H5P_mt_prop_t             * valid_prop2;
    uint64_t         version_1       = 0;
    uint64_t         version_2       = 0;
    int32_t          cmp_value;
    bool             inc_thrd_flag_1 = FALSE;
    bool             inc_thrd_flag_2 = FALSE;
    bool             base_flag_1     = FALSE;
    bool             base_flag_2     = FALSE;

    int32_t          ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    tag1 = *((H5P_mt_type_t *)param1);
    tag2 = *((H5P_mt_type_t *)param2);

    if ( tag1 == CLASS_TAG && tag2 == CLASS_TAG )
    {
        class1 = (H5P_mt_class_t *)param1;
        class2 = (H5P_mt_class_t *)param2;

        assert(class1);
        assert(class2);
        assert(atomic_load(&(class1->tag)) == H5P_MT_CLASS_TAG);
        assert(atomic_load(&(class2->tag)) == H5P_MT_CLASS_TAG);

        /* Increment thread count */
        if ( ( H5P__inc_thrd_count(class1) ) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, (-1),
                        "Couldn't increment class's thread count.");
        inc_thrd_flag_1 = TRUE;

        if ( ( H5P__inc_thrd_count(class2) ) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, (-1),
                        "Couldn't increment class's thread count.");
        inc_thrd_flag_2 = TRUE;

        version_1 = atomic_load(&(class1->curr_version));
        version_2 = atomic_load(&(class2->curr_version));


        /* Check whether they have the same fields */
        if ( class1->parent_id != class2->parent_id )
            HGOTO_DONE(1);
        if ( class1->parent_ptr != class2->parent_ptr )
            HGOTO_DONE(1);
        if ( class1->parent_version != class2->parent_version )
            HGOTO_DONE(1);
        if ( (cmp_value = HDstrcmp(class1->name, class2->name)) != 0 )
            HGOTO_DONE(1);
        if ( atomic_load(&(class1->id)) != atomic_load(&(class2->id)) )
            HGOTO_DONE(1);
        if ( class1->type != class2->type )
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
            

    } /* end if ( tag1 == CLASS_TAG && tag2 == CLASS_TAG ) */

    else if ( tag1 == LIST_TAG && tag2 == LIST_TAG )
    {
        list1 = (H5P_mt_list_t *)param1;
        list2 = (H5P_mt_list_t *)param2;

        assert(list1);
        assert(list2);
        assert(atomic_load(&(list1->tag)) == H5P_MT_LIST_TAG);
        assert(atomic_load(&(list2->tag)) == H5P_MT_LIST_TAG);

        /* Increment thread count */
        if ( ( H5P__inc_thrd_count(list1) ) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, (-1),
                        "Couldn't increment class's thread count.");
        inc_thrd_flag_1 = TRUE;

        if ( ( H5P__inc_thrd_count(list2) ) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, (-1),
                        "Couldn't increment class's thread count.");
        inc_thrd_flag_2 = TRUE;

        version_1 = atomic_load(&(list1->curr_version));
        version_2 = atomic_load(&(list2->curr_version));

        /* Check whether they have the same fields */
        if ( list1->pclass_id != list2->pclass_id )
            HGOTO_DONE(1);
        if ( list1->pclass_ptr != list2->pclass_ptr )
            HGOTO_DONE(1);
        if ( list1->pclass_version != list2->pclass_version )
            HGOTO_DONE(1);
        if ( atomic_load(&(list1->plist_id)) != atomic_load(&(list2->plist_id)) )
            HGOTO_DONE(1);
        if ( list1->nprops_inherited != list2->nprops_inherited )
            HGOTO_DONE(1);
        if ( list1->class_init != list2->class_init )
            HGOTO_DONE(1);


        /* Check if the lkup_tbls are the same */

        for ( uint32_t idx = 0; idx < list1->nprops_inherited; idx++ )
        {
            entry1 = &list1->lkup_tbl[idx];
            entry2 = &list2->lkup_tbl[idx];

            /* Compare chksum, name, and base_delete_version fields */
            if ( entry1->chksum != entry2->chksum )
                HGOTO_DONE(1);
            if ( (cmp_value = HDstrcmp(entry1->name, entry2->name)) != 0 )
                HGOTO_DONE(1);



            valid_prop1 = H5P__mt_entry_find_version(entry1, version_1, &base_flag_1);
            valid_prop2 = H5P__mt_entry_find_version(entry2, version_2, &base_flag_2);

            /* Ensure that both valid_props are either not NULL or are NULL */
            if ( valid_prop1 == NULL && valid_prop2 != NULL )
                HGOTO_DONE(1);
            else if ( valid_prop1 != NULL && valid_prop2 == NULL )
                HGOTO_DONE(1);




            /* Compare bases */
            base1 = atomic_load(&(entry1->base));
            base2 = atomic_load(&(entry2->base));

            if ( base_flag_1 != base_flag_2 )
                HGOTO_DONE(1);

            /* If the base is current version for both enty's then compare them */
            if ( base_flag_1 == TRUE && base_flag_2 == TRUE )
            {
                if ( base1.ptr == NULL && base2.ptr != NULL )
                    HGOTO_DONE(1);
                if ( base1.ptr != NULL && base2.ptr == NULL )
                    HGOTO_DONE(1);
                if ( base1.ptr != base2.ptr )
                    HGOTO_DONE(1);
            }

            /** 
             * Comparing only if one curr.ptr is NULL. All lfsll props are 
             * compared next. don't need to potentially compare them twice.
             */
            curr1 = atomic_load(&(entry1->curr));
            curr2 = atomic_load(&(entry2->curr));

            if ( curr1.ptr == NULL && curr2.ptr != NULL )
                HGOTO_DONE(1);
            if ( curr1.ptr != NULL && curr2.ptr == NULL )
                HGOTO_DONE(1);

        } /* end for() */
        
    } /* end if ( tag1 == LIST_TAG && tag2 == LIST_TAG ) */

    else
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, (-1), 
                "Either the types weren't the same or one type wasn't a class or list.");
    }


    /* Compare the properties in the lfsll */

    if ( class1 )
    {
        prev_prop1 = atomic_load(&(class1->pl_head));
        prev_prop2 = atomic_load(&(class2->pl_head));
    }
    else if ( list1 )
    {
        prev_prop1 = atomic_load(&(list1->pl_head));
        prev_prop2 = atomic_load(&(list2->pl_head));
    }

    if ( atomic_load(&(prev_prop1->tag)) != atomic_load(&(prev_prop2->tag)) )
        HGOTO_DONE(1);

    if ( prev_prop1->sentinel != prev_prop2->sentinel )
        HGOTO_DONE(1);

    do
    {
        /* Get the next valid props in the lfslls to compare */
        valid_prop1 = H5P__get_next_valid_prop(prev_prop1, version_1);
        valid_prop2 = H5P__get_next_valid_prop(prev_prop2, version_2);

        /* If one of the props is NULL return 1, else compare the two props */
        if (valid_prop1 == NULL && valid_prop2 != NULL )
            HGOTO_DONE(1);
        else if (valid_prop1 != NULL && valid_prop2 == NULL )
            HGOTO_DONE(1);
        else if ( valid_prop1 && valid_prop2 )
        {
            /* Full compare all fields of the valid_props */
            if ( 0 != H5P__mt_is_equal(valid_prop1, valid_prop2) )
                HGOTO_DONE(1);
        }

        /* update valid_props to prev_props to get the next valid props */
        prev_prop1 = valid_prop1;
        prev_prop2 = valid_prop2;

    } while ( valid_prop1 );



done:

    /* If working with classes ensure the thrd counts are decrement if needed */
    if ( class1 )
    {
        if ( inc_thrd_flag_1 )
        {
            if ( 0 > H5P__dec_thrd_count(class1) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, (-1), 
                            "Failure to decrement thrd_count.");
        }
        if ( inc_thrd_flag_2 )
        {
            if ( 0 > H5P__dec_thrd_count(class2) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, (-1), 
                            "Failure to decrement thrd_count.");
        }
    }
    /* If working with lists ensure the thrd counts are decrement if needed */
    else
    {
        if ( inc_thrd_flag_1 )
        {
            if ( 0 > H5P__dec_thrd_count(list1) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, (-1), 
                            "Failure to decrement thrd_count.");
        }
        if ( inc_thrd_flag_2 )
        {
            if ( 0 > H5P__dec_thrd_count(list2) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, (-1), 
                            "Failure to decrement thrd_count.");
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_cmp_list_or_class() */



#if 0
/****************************************************************************************
 * Function:    H5P__calc_checksum
 *
 * Purpose:     Creates a checksum (chksum) value for a property based on it's name.
 *
 *              This function uses the Fletcher-32 Checksum Function to create a 32-bit
 *              checksum in a 64-bit integer. A 64-bit size integer is used so the 
 *              sentinel entries as the head and tail in the LFSLL (lock free singly 
 *              linked list) as LLONG MAX and LLONG MIN respectively.
 *              
 *
 * Return:      Success: Returns a 32-bit checksum for the property
 * 
 *              Failure: 0   
 *
 ****************************************************************************************
 */
int64_t 
H5P__calc_checksum(const char* name)
{
    uint32_t        sum1; /* First Fletcher chksum accumulator */
    uint32_t        sum2; /* Second Fletcher chksum accumulator */
    const uint8_t * data; /* value to hold input name in bytes */
    size_t          len;  /* Length of the input name */
    size_t          tlen; /* temp length if name is long enough to be broken up */

    assert(name);

    /* Initializes the accumulators */
    sum1 = 0xFFFF; 
    sum2 = 0xFFFF; 

    /* Casts the input name to bytes */
    data = (const uint8_t *)name; 
    len = strlen(name);  

    /**
     * Loop to process the name in chunks
     * NOTE: Most names should be processed in only one chunk, but this is
     * still set up as a safety net to prevent integer overflow.
     */
    while (len) 
    {
        /* Process chucks of 360 bytes */
        tlen = (len > 360) ? 360 : len; 
        len -= tlen;

        do 
        {
            sum1 += *data++; /* Add the current byte to sum1 */
            sum2 += sum1;    /* Add the updated sum1 to sum2 */
            tlen--;

        } while (tlen);

        /* Reduce sums to 16 bits */
        sum1 = (sum1 & 0xFFFF) + (sum1 >> 16);
        sum2 = (sum2 & 0xFFFF) + (sum2 >> 16);

    } /* end while (len) */

    /* Final reduction to 16 bits */
    sum1 = (sum1 & 0xFFFF) + (sum1 >> 16);
    sum2 = (sum2 & 0xFFFF) + (sum2 >> 16);

    return (sum2 << 16) | sum1; /* Combine the two 16-bit sums into a 32-bit checksum */

} /* H5P__calc_checksum() */
#endif



/****************************************************************************************
 * Function:    H5P__mt_get_value()
 * 
 * Purpose:     Gets the value of a specified property and copies the value into a 
 *              provided buffer.
 * 
 *              H5P__mt_search_prop() is called to return the most recent version of the
 *              property with the matching chksum as the one provided, from the list 
 *              passed in. 
 * 
 *              The property returned by H5P__mt_search_prop() is checked if it has a 
 *              get callback. If it does then a tmp buffer is allocated to store the 
 *              value in case the callback fails, then the callback is called to get the
 *              value. If the property does not have a get callback, the value is simply
 *              copied into the supplied buffer. 
 * 
 * Return:      SUCCEED/FAIL

 ****************************************************************************************
 */
herr_t
H5P__mt_get_value(H5P_mt_list_t *list, int64_t chksum, const char *name, void *value_ptr)
{
    H5P_mt_prop_t     * prop;
    H5P_mt_prop_value_t prop_value;
    void              * tmp_value_buf = NULL;

    herr_t          ret_value = SUCCEED;
    
    FUNC_ENTER_PACKAGE

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_PROP_TAG );
    assert(value_ptr);

    prop = H5P__mt_search_prop(list, chksum, name);

    if ( NULL == prop )
        HGOTO_ERROR(H5E_PLIST, H5E_CANTFIND, FAIL, "Target prop isn't in list.");

    prop_value = atomic_load(&(prop->value));

    if ( prop_value.size == 0 )
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "property has zero size");

    /* If the prop has the get callback, call it */
    if ( prop->get )
    {
        /* Make a copy of the current value, in case the callback fails */
        if (NULL == (tmp_value_buf = H5MM_malloc(prop_value.size)))
                HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, FAIL, 
                    "memory allocation for tmp prop value failed");
        
        H5MM_memcpy(tmp_value_buf, prop_value.ptr, prop_value.size);

        /* Call user's callback */
        if ( (*(prop->get))(list->plist_id, name, prop_value.size, tmp_value_buf) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "can't set property value");

        /* Copy new [possibly unchanged] value into return value */
        H5MM_memcpy(value_ptr, tmp_value_buf, prop_value.size);
    
    } /* end if ( prop->get ) */

    /* No callback, just copy value */
    else
    {
        H5MM_memcpy(value_ptr, prop_value.ptr, prop_value.size);
    }


done:

    /* Free the tmp value buf */
    if ( tmp_value_buf )
        H5MM_xfree(tmp_value_buf);

        
    FUNC_LEAVE_NOAPI(ret_value) 

} /* H5P__mt_get_value() */



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
 *              First the functions calls H5P__mt_version_check() to ensure there isn't
 *              another operation that can change the list occuring, before continuing.
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
    H5P_mt_list_table_entry_t * entry;  /* entry in the lkup_tbl */
    H5P_mt_prop_t             * valid_prop; /* prop to try encoding next */
    H5P_mt_prop_t             * prev_prop;  /* previous valid_prop in the lfsll */  
    uint8_t  * p = (uint8_t *)buf;      /* tmp pointer to encode buffer */
    size_t     encode_size = 0;         /* size of buf needed to encode properties */
    bool       encode      = TRUE;      /* bool for if the list should be encoded */
    bool       base_flag   = FALSE;
    uint64_t   curr_version;
    uint64_t   next_version;
    uint32_t   nprops;                  /* total # of props in lkup_tbl + lfsll */
    uint32_t   nprops_inherited;        /* Number of entries in the lkup_tbl */
    uint32_t   log_pl_len;              /* Number of props in the lfsll */
    uint64_t   prop_count = 0;
    uint32_t   idx;                     /* Index of the lkup_tbl */


    herr_t       ret_value   = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);
    assert(atomic_load(&(list->curr_version)) >= version);

    curr_version = atomic_load(&(list->curr_version));
    next_version = atomic_fetch_add(&(list->next_version), 1);

    if ( ( curr_version + 1 ) < next_version )
    {
        if ((curr_version = H5P__mt_version_check(list, curr_version, next_version)) == 0)
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                        "Error with returned current version.");
    }
    

    nprops = atomic_load(&(list->nprops));
    nprops_inherited = atomic_load(&(list->nprops_inherited));

    /* Sanity Check */
    if ( NULL == nalloc )
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "bad allocation size pointer");

    /* If buf is NULL, nothing to encode */
    if ( NULL == p )
        encode = FALSE;

    /* If not false encode first two bytes of buf */
    if ( encode )
    {
        /* Sets first byte to encoding version # */
        *p++ = (uint8_t)H5P_ENCODE_VERS;

        /* Sets second byte to type of property list */
        *p++ = (uint8_t)list->pclass_ptr->type;
    }

    encode_size += 2;


    /* Iterate the lkup_tbl entries */
    for ( idx = 0; idx < nprops_inherited; idx++ )
    {
        entry = &list->lkup_tbl[idx];

        valid_prop = H5P__mt_entry_find_version(entry, version, &base_flag);

        /* If prop isn't NULL, then it's a valid prop */
        if ( valid_prop )
        {
            if ( H5P__mt_encode_prop(valid_prop, encode, &encode_size, &p) < 0 )
                HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL, 
                            "Error while encoding properties in the lkup_tbl");
        }
        
    }

    assert(idx == nprops_inherited);

    
    /* Iterate the lfsll */
    prev_prop = atomic_load(&(list->pl_head));
    assert(prev_prop);
    assert(prev_prop->sentinel);

    log_pl_len = atomic_load(&(list->log_pl_len));

    do
    {
        /* Gets the next valid prop or NULL if there isn't another valid prop */
        valid_prop = H5P__get_next_valid_prop(prev_prop, version);

        if ( valid_prop )
        {
            prop_count++;

            /* If in_lkup_tbl is TRUE, the property was already encoded */
            if ( ! valid_prop->in_lkup_tbl )
            {
                if ( H5P__mt_encode_prop(valid_prop, encode, &encode_size, &p) < 0 )
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL, 
                                "Error while encoding properties in the lfsll");
            }
        }

    } while ( valid_prop );

    assert(prop_count == log_pl_len);

    nprops = atomic_load(&(list->nprops));

    assert(nprops == (idx + prop_count));


    /* Encode a terminator for the list of properties */
    if ( encode )
        *p++ = 0;

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

    herr_t              ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(p);

    /* Check if the property can be encoded */
    if ( prop->encode )
    {
        name_len  = HDstrlen(prop->name) + 1;
        value = atomic_load(&(prop->value));


        /* If encode is TRUE, encode the property's name */
        if ( encode )
        {
            HDstrcpy((char *)*(p), prop->name);
            *(uint8_t **)(p) += name_len;
        }
        encode_size += name_len;

        value_size = 0;

        /* If not NULL, encode the property value */
        if ((prop->encode)(value.ptr, (void **)&p, &value_size) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL, 
                        "property encoding routine failed");

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
    H5P_mt_type_t    tag;
    H5P_mt_class_t * class = NULL;
    H5P_mt_list_t  * list  = NULL;
    bool             inc_thrd_flag = FALSE;

    uint64_t         ret_value = 0;

    FUNC_ENTER_PACKAGE

    tag = *((H5P_mt_type_t *) param);

    if ( tag == CLASS_TAG )
    {
        class = (H5P_mt_class_t *)param;

        assert(class);
        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);

        /** TODO: assert(class->ref_count.deleted == FALSE) */

        /* update stats */
        /** TODO: add stats for number of times version is gotten */

        /* Increment thread count */
        if ( ( H5P__inc_thrd_count(class) ) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, 0, 
                        "Couldn't increment parent's thread count.");
        else
            inc_thrd_flag = TRUE;

        /** TODO: replace this with function that gets version from context */
        ret_value = atomic_load(&(class->curr_version));

            
    }
    else if ( tag == LIST_TAG )
    {
        list = (H5P_mt_list_t *)param;

        assert(list);
        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

        /* update stats */
        /** TODO: add stats for number of times version is gotten */

        /* Increment thread count */
        if ( ( H5P__inc_thrd_count(list) ) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, 0, 
                        "Couldn't increment parent's thread count.");
        else
            inc_thrd_flag = TRUE;
            

        /** TODO: replace this with function that gets version from context */
        ret_value = atomic_load(&(list->curr_version));

    }
    else
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, 0, 
                    "Type passed in wasn't a class or list.");
    }

done:

    /* If the parent's thrd_count was incremented, it must be decremented */
    if ( inc_thrd_flag )
    {
        if ( class )
        {
            if ( 0 > H5P__dec_thrd_count(class) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, 0, 
                            "Failure to decrement thrd_count.");
        }
        else
        {
            if ( 0 > H5P__dec_thrd_count(list) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, 0, 
                            "Failure to decrement thrd_count.");
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
    H5P_mt_prop_t    * fl_prop;
    H5P_mt_prop_aptr_t fl_head;
    H5P_mt_prop_aptr_t fl_tail;
    H5P_mt_prop_aptr_t fl_next;
    H5P_mt_prop_aptr_t fl_update;
    bool               done = FALSE;

    herr_t            ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert( 0 == (atomic_load(&(prop->ref_count))));

    /** TODO: turn this into a atomic_compare_strong() */
    atomic_store(&(prop->tag), H5P_MT_PROP_VALID_ONFL_TAG);

    /* intiate fl_update's fields */
    fl_update.ptr = NULL;
    fl_update.deleted = FALSE;
    fl_update.dummy_bool_1 = FALSE;
    fl_update.dummy_bool_2 = FALSE;
    fl_update.dummy_bool_3 = FALSE;

    fl_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

    if ( fl_tail.ptr )
    {
        do 
        {
            fl_prop = fl_tail.ptr;

            fl_next = atomic_load(&(fl_prop->next));

            fl_update.ptr = prop;

            if ( ! atomic_compare_exchange_strong(&(fl_prop->next),
                                                    &fl_next, fl_update))
            {
                /* failed, update stats and try again */
                atomic_fetch_add(&(H5P_mt_g.prop_fl_next_update_cols), 1);
            }
            else
            {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.prop_fl_next_update), 1);

                done = TRUE;
            }

        } while ( ! done );
    
    } /* end if ( fl_tail.ptr ) */

    done = FALSE;

    /* Atomically updates the prop_fl_tail to point to the new tail */

    do
    {
        fl_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

        fl_update.ptr = prop;

        if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.prop_fl_tail),
                                                &fl_tail, fl_update))
        {
            /* failed, updated stats and try again */
            atomic_fetch_add(&(H5P_mt_g.prop_fl_tail_update_cols), 1);
        }
        else
        {
            /* success, update stats and continue */
            atomic_fetch_add(&(H5P_mt_g.prop_fl_tail_update), 1);
            atomic_fetch_add(&(H5P_mt_g.num_props_added_to_fl), 1);

            atomic_fetch_add(&(H5P_mt_g.prop_fl_len), 1);

            done = TRUE;
        }
    
    } while ( ! done );


    /**
     * If this is the first class added to the class free list, have the
     * head pointer point to it as well.
     */
    fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));

    if ( ! fl_head.ptr )
    {
        done = FALSE;

        do
        {
            fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));

            fl_update.ptr = prop;

            if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.prop_fl_head),
                                                    &fl_head, fl_update))
            {
                /* failed, updated stats and try again */
                atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update_cols), 1);
            }
            else
            {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update), 1);

                done = TRUE;
            }

        } while ( ! done );

    } /* end if ( ! fl_head.ptr ) */



    /* update length of the free list */
    atomic_fetch_add(&(H5P_mt_g.prop_fl_len), 1);

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.num_props_added_to_fl), 1);


    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_close_prop() */



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
H5P__mt_close_class(H5P_mt_class_t * class)
{
    H5P_mt_class_t             * parent = NULL;
    H5P_mt_class_t             * fl_class;
    H5P_mt_class_sptr_t          fl_head;
    H5P_mt_class_sptr_t          fl_tail;
    H5P_mt_class_sptr_t          fl_next;
    H5P_mt_class_sptr_t          fl_update;
    H5P_mt_active_thread_count_t local_thrd;
    H5P_mt_active_thread_count_t closing_thrd;
    H5P_mt_class_ref_counts_t    ref_count;
    H5P_mt_class_ref_counts_t    update_rc;  /* rc = ref_count */
    bool                         done = FALSE;
    bool                         inc_thrd_flag = FALSE;
    
    herr_t                       ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(class);
    assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);

    ref_count = atomic_load(&(class->ref_count));

    /* If deleted is not already TRUE, set it to TRUE atomically */
    if ( ! ref_count.deleted )
    {
        do
        {
            ref_count = atomic_load(&(class->ref_count));

            update_rc = ref_count;
            update_rc.deleted = TRUE;

            if ( ! atomic_compare_exchange_strong(&(class->ref_count),
                                                &ref_count, update_rc))
            {
                /* failed, update stats */
                atomic_fetch_add(&(class->num_ref_count_cols), 1);
            }
            else
            {
                /* success, update stats */
                atomic_fetch_add(&(class->num_ref_count_update), 1);

                done = TRUE;
            }

        } while ( ! done );

        assert(done);

        done = FALSE;

    } /* end if ( ! ref_count.deleted ) */



    /* If closing is already TRUE, throw an error */

    local_thrd = atomic_load(&(class->thrd));

    if ( local_thrd.closing )
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                    "Closing flag is set when it shouldn't be.");
    }

    ref_count = atomic_load(&(class->ref_count));

    /**
     * If there are no active derived lists or classes, we can close this class.
     * Else, we must check again when a derived list or class count gets decremented.
     */
    if ( ref_count.pl == 0 && ref_count.plc == 0 && ref_count.deleted )
    {
        /* Set the closing flag to TRUE to prevent new threads from entering the struct */
        do 
        {
            assert(local_thrd.opening == FALSE);
            assert(local_thrd.count == 0);

            closing_thrd = local_thrd;
            closing_thrd.closing = TRUE;

            /* Atomically update closing flag to TRUE */
            if ( ! atomic_compare_exchange_strong(&(class->thrd), &local_thrd, closing_thrd))
            {
                /* failed, update stats and try again */
                atomic_fetch_add(&(class->num_thrd_update_cols), 1);
            }
            else
            {
                /* success, update stats and mark done */
                atomic_fetch_add(&(class->num_thrd_closing_flag_set), 1);

                done = TRUE;
            }

        } while ( ! done );

        assert(done);
        done = FALSE;

        /* If the struct is opening or has other threads wait and try again */
        do 
        {
            /* If opening is TRUE, sleep and try again */
            if ( local_thrd.opening )
            {
                atomic_fetch_add(&(class->num_thrd_opening_flag_set), 1);
                sleep(1);
            }
            /* If there are any other threads in the struct, wait for them to drain out */
            else if ( local_thrd.count > 0 )
            {
                sleep(1);
            }
            else
            {
                done = TRUE;
            }

            local_thrd = atomic_load(&(class->thrd));

        } while ( ! done );

        assert(done);
        done = FALSE;


        local_thrd = atomic_load(&(class->thrd));
        assert(local_thrd.opening == FALSE);
        assert(local_thrd.closing == TRUE);
        assert(local_thrd.count == 0);


        /* Decrement the parents ref count for derived classes */
        if ( class->parent_ptr )
        {
            parent = class->parent_ptr;
            assert(parent);
            assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_TAG);

            /* update parent's thrd count */
            if ( H5P__inc_thrd_count(parent) < 0) 
                HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                            "Couldn't increment parent's thread count.");
            
            inc_thrd_flag = TRUE;

            /* update parent's ref count of derived classes */
            if ( H5P__dec_ref_count(parent, TRUE) < 0 )
                HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                            "Couldn't decrement parent's plc ref count."); 

        } /* end if ( class->parent_ptr ) */


        
        /* Atomically updates the current tail to point to the class being added */

        fl_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

        if ( fl_tail.ptr )
        {
            do 
            {
                fl_class = fl_tail.ptr;

                fl_next = atomic_load(&(fl_class->fl_next));

                assert( ! fl_next.ptr );

                fl_update.ptr = class;
                fl_update.sn  = fl_next.sn + 1;

                if ( ! atomic_compare_exchange_strong(&(fl_class->fl_next),
                                                        &fl_next, fl_update))
                {
                    /* failed, update stats and try again */
                    atomic_fetch_add(&(H5P_mt_g.class_fl_next_update_cols), 1);
                }
                else
                {
                    /* success, update stats and continue */
                    atomic_fetch_add(&(H5P_mt_g.class_fl_next_update), 1);

                    done = TRUE;
                } 

            } while ( ! done );

        } /* end if ( fl_tail.ptr ) */

        done = FALSE;


        /* Atomically updates the class_fl_tail to point to the new tail */

        do
        {
            fl_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

            fl_update.ptr = class;
            fl_update.sn  = fl_tail.sn + 1;

            if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_tail),
                                                    &fl_tail, fl_update))
            {
                /* failed, updated stats and try again */
                atomic_fetch_add(&(H5P_mt_g.class_fl_tail_update_cols), 1);
            }
            else
            {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.class_fl_tail_update), 1);
                atomic_fetch_add(&(H5P_mt_g.num_class_added_to_fl), 1);

                atomic_fetch_add(&(H5P_mt_g.class_fl_len), 1);

                done = TRUE;
            }
        
        } while ( ! done );


        
        /**
         * If this is the first class added to the class free list, have the
         * head pointer point to it as well.
         */
        fl_head = atomic_load(&(H5P_mt_g.class_fl_head));

        if ( ! fl_head.ptr )
        {
            done = FALSE;

            do
            {
                fl_head = atomic_load(&(H5P_mt_g.class_fl_head));

                fl_update.ptr = class;
                fl_update.sn  = fl_head.sn + 1;

                if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_head),
                                                        &fl_head, fl_update))
                {
                    /* failed, updated stats and try again */
                    atomic_fetch_add(&(H5P_mt_g.class_fl_head_update_cols), 1);
                }
                else
                {
                    /* success, update stats and continue */
                    atomic_fetch_add(&(H5P_mt_g.class_fl_head_update), 1);

                    done = TRUE;
                }
    
            } while ( ! done );

        } /* end if ( ! fl_head.ptr ) */

    } /* end if () */

done:

    local_thrd = atomic_load(&(class->thrd));
    if ( local_thrd.closing )
    {
        closing_thrd.closing = FALSE;
        atomic_store(&(class->thrd), closing_thrd);
    }

    /* update parent's thrd count */
    if ( parent != NULL && inc_thrd_flag )
    {
        if ( 0 > H5P__dec_thrd_count(parent) )
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                        "Failure to decrement thrd_count.");
    }


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
    H5P_mt_prop_t              * first_prop;
    H5P_mt_prop_aptr_t           next_ptr;
    H5P_mt_class_ref_counts_t    ref_count;
    hid_t                        null_list_id = 0;
    uint32_t                     phys_pl_len;
    uint32_t                     i;

    H5P_mt_class_t             * ret_value;

    FUNC_ENTER_PACKAGE


    thrd = atomic_load(&(class->thrd));

    assert(thrd.count == 0);
    assert(thrd.opening == FALSE);
    assert(thrd.closing == FALSE);

    ref_count = atomic_load(&(class->ref_count));
    assert(ref_count.pl  == 0);
    assert(ref_count.plc == 0);


    /* Clears the class's fields */

    /** TODO: turn this into an atomic_compare_strong() */
    atomic_store(&(class->tag), H5P_MT_CLASS_INVALID_TAG);

    class->parent_id  = H5I_INVALID_HID;
    class->parent_ptr = NULL;

    free(class->name);
    class->name = NULL;

    atomic_store(&(class->id), null_list_id);


    /* Iterate the LFSLL and add all properties to the free list */
    phys_pl_len = atomic_load(&(class->phys_pl_len));

    for ( i = 0; i < ( phys_pl_len ); i++ )
    {
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
    assert( ! first_prop);

    assert( 0 == (atomic_load(&(class->log_pl_len))));
    assert( 0 == (atomic_load(&(class->phys_pl_len))));

    if ( 0 > (H5P__reset_stats_class(class)) )
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                    "Failed resetting stats fields.");


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
 *              First we check if the list is already marked closing and if it throw an
 *              error, because it shouldn't be marked closing until it is actually 
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
 *              thing, if the property has the close cb call it. 
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
H5P__mt_close_list(H5P_mt_list_t * list)
{
    H5P_mt_class_t             * parent;
    H5P_mt_list_t              * fl_list;
    H5P_mt_list_sptr_t           fl_head;
    H5P_mt_list_sptr_t           fl_tail;
    H5P_mt_list_sptr_t           fl_next;
    H5P_mt_list_sptr_t           fl_update;
    H5P_mt_active_thread_count_t local_thrd;
    H5P_mt_active_thread_count_t closing_thrd;
    H5P_mt_list_table_entry_t  * entry;
    H5P_mt_list_prop_ref_t       base_ref;
    H5P_mt_prop_t              * base_prop;
    H5P_mt_prop_t              * prop;
    H5P_mt_prop_aptr_t           next_prop;
    H5P_mt_prop_value_t          prop_value;
    uint64_t                     list_version;
    uint64_t                     delete_version;
    bool                         done = FALSE;
    bool                         inc_thrd_flag = FALSE;
    
    herr_t                       ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);


    local_thrd = atomic_load(&(list->thrd));

    /* If closing is already TRUE, throw an error */
    if ( local_thrd.closing )
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                    "Closing flag is set when it shouldn't be.");
    }

    /* Atomically set the closing to be TRUE */
    do
    {
        assert(local_thrd.opening == FALSE);
        assert(local_thrd.count == 0);

        closing_thrd = local_thrd;
        closing_thrd.closing = TRUE;

        /* Atomically update closing flag to TRUE */
        if ( ! atomic_compare_exchange_strong(&(list->thrd), &local_thrd, closing_thrd))
        {
            /* failed, update stats and try again */
            atomic_fetch_add(&(list->num_thrd_update_cols), 1);
        }
        else
        {
            /* success, update stats and mark done */
            atomic_fetch_add(&(list->num_thrd_closing_flag_set), 1);

            done = TRUE;
        }
        
    } while ( ! done );


    assert(done);
    done = FALSE;


    /* Ensure struct isn't opening, and that it's empty of other threads */
    do
    {
        /* If opening is TRUE, sleep and try again */
        if ( local_thrd.opening )
        {
            atomic_fetch_add(&(list->num_thrd_opening_flag_set), 1);
            sleep(1);
        }
        /* If there are any other threads in the struct, wait for them to drain out */
        else if ( local_thrd.count > 0 )
        {
            sleep(1);
        }
        else
        {
            done = TRUE;
        }

        local_thrd = atomic_load(&(list->thrd));

    } while ( ! done );

    assert(done);
    done = FALSE;


    local_thrd = atomic_load(&(list->thrd));
    assert(local_thrd.opening == FALSE);
    assert(local_thrd.closing == TRUE);
    assert(local_thrd.count == 0);

    /* Get the current version of the list */
    list_version = atomic_load(&(list->curr_version));


    /* Check the property list initialization function completed */
    if ( atomic_load(&(list->class_init)) )
    {
        parent = list->pclass_ptr;
        assert(parent);
        assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_TAG);

/**
 * NOTE: This is for testing whether we need to iterate up the tree of parent classes
 * to perform the close callback, or if just performing it on this list's parent is good
 * enough. 
 * 
 * Also, may need to add a tracker to ensure we don't call the close cb on a parent's 
 * prop that has the same name as a prop in the list we already called the close cb on.
 */
#if 1
        /* Call the class close callback up the parent inheritance tree, if needed */
        while ( parent )
        {
#endif
            /* update parent's thrd count */
            if ( H5P__inc_thrd_count(parent) < 0) 
                HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                            "Couldn't increment parent's thread count.");
            
            inc_thrd_flag = TRUE;

            /* class close callback */
            if ( parent->close_func )
                (parent->close_func)(list->plist_id, parent->close_data);

            /* Decrement the thrd count */
            if ( 0 > H5P__dec_thrd_count(parent) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                            "Failure to decrement parent's thrd_count.");

            inc_thrd_flag = FALSE;
/**
 * Same as the above if 1 endif.
 */
#if 1
            parent = parent->parent_ptr;
#endif
        } /* while ( parent ) */
        
    } /* end if ( atomic_load(&(list->class_init)) ) */


    /* Increment this list's parent thrd count since we will be accessing it's props */

    parent = list->pclass_ptr;
    assert(parent);
    assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_TAG);

    /* update parent's thrd count */
    if ( H5P__inc_thrd_count(parent) < 0) 
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                "Couldn't increment parent's thread count.");
            
    inc_thrd_flag = TRUE;


    /* Scan the lkup_tbl and if base.ptr != NULL, and the close cb exists call it */
    for ( uint32_t i = 0; i < list->nprops_inherited; i++ )
    {
        entry = &list->lkup_tbl[i];

        delete_version = atomic_load(&(entry->base_delete_version));

        /* If the base has been marked for deletion */
        if ( delete_version == 0 || delete_version > list_version )
        {
            base_ref = atomic_load(&(entry->base));

            /* Ensure the base pointer isn't NULL */
            if ( base_ref.ptr )
            {
                base_prop = base_ref.ptr;

                if ( base_prop->close )
                {
                    prop_value = atomic_load(&(base_prop->value));

                    /* property close callback */
                    (base_prop->close)(base_prop->name, prop_value.size, prop_value.ptr);

                    /* Decrement the parent's prop's ref_count */
                    assert( 0 != (atomic_load(&(base_prop->ref_count))));
                    atomic_fetch_sub(&(base_prop->ref_count), 1);
                }
            }
        }
    }


    /* Iterate the LFSLL and if property has the close cb call it */

    prop = list->pl_head;
    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(prop->sentinel);

    next_prop = atomic_load(&(prop->next));
    prop = next_prop.ptr;
    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

    while ( ! prop->sentinel )
    {
        delete_version = atomic_load(&(prop->delete_version));

        if ( delete_version == 0 || ( delete_version > list_version ) )
        {
            if ( prop->close )
            {
                prop_value = atomic_load(&(prop->value));

                (prop->close)(prop->name, prop_value.size, prop_value.ptr);
            }
        }

        next_prop = atomic_load(&(prop->next));
        prop = next_prop.ptr;
    }


    /**
     * NOTE: The original code iterates all props up the parent tree and calls the
     * close cb on any property we have not already done so, if it has the callback.
     * 
     * We shouldn't need to do this, because the way our versioning system works 
     * this list's parent's class contains all properties from it's parent at the
     * time it was created. But in case we do this is the location to add that.
     */



    /* Decrement parent's ref count of derived lists */
    if ( H5P__dec_ref_count(parent, FALSE) < 0 )
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                    "Couldn't decrement parent's pl ref count."); 


    done = FALSE;

    /* Atomically updates the current tail to point to the list being added */

    fl_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

    if ( fl_tail.ptr )
    {
        do 
        {
            fl_list = fl_tail.ptr;

            fl_next = atomic_load(&(fl_list->fl_next));

            assert( ! fl_next.ptr );

            fl_update.ptr = list;
            fl_update.sn  = fl_next.sn + 1;

            if ( ! atomic_compare_exchange_strong(&(fl_list->fl_next),
                                                    &fl_next, fl_update))
            {
                /* failed, updated stats and try again */
                atomic_fetch_add(&(H5P_mt_g.list_fl_next_update_cols), 1);
            }
            else
            {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.list_fl_next_update), 1);

                done = TRUE;
            } 

        } while ( ! done );

    } /* end if ( fl_tail.ptr ) */

    done = FALSE;

    
    /* Atomically updates the list_fl_tail to point to the new tail */

    do
    {
        fl_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

        fl_update.ptr = list;
        fl_update.sn  = fl_tail.sn + 1;

        if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_tail),
                                                &fl_tail, fl_update))
        {
            /* failed, updated stats and try again */
            atomic_fetch_add(&(H5P_mt_g.list_fl_tail_update_cols), 1);
        }
        else
        {
            /* success, update stats and continue */
            atomic_fetch_add(&(H5P_mt_g.list_fl_tail_update), 1);
            atomic_fetch_add(&(H5P_mt_g.num_list_added_to_fl), 1);

            atomic_fetch_add(&(H5P_mt_g.list_fl_len), 1);

            done = TRUE;
        }
    
    } while ( ! done );


    
    /**
     * If this is the first class added to the class free list, have the
     * head pointer point to it as well.
     */
    fl_head = atomic_load(&(H5P_mt_g.list_fl_head));

    if ( ! fl_head.ptr )
    {
        done = FALSE;

        do
        {
            fl_head = atomic_load(&(H5P_mt_g.list_fl_head));

            fl_update.ptr = list;
            fl_update.sn  = fl_head.sn + 1;

            if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_head),
                                                    &fl_head, fl_update))
            {
                /* failed, updated stats and try again */
                atomic_fetch_add(&(H5P_mt_g.list_fl_head_update_cols), 1);
            }
            else
            {
                /* success, update stats and continue */
                atomic_fetch_add(&(H5P_mt_g.list_fl_head_update), 1);

                done = TRUE;
            }

        } while ( ! done );

    } /* end if ( ! fl_head.ptr ) */

done:

    local_thrd = atomic_load(&(list->thrd));
    if ( local_thrd.closing )
    {
        closing_thrd.closing = FALSE;
        atomic_store(&(list->thrd), closing_thrd);
    }

    /* update parent's thrd count */
    if ( inc_thrd_flag )
    {
        if ( 0 > H5P__dec_thrd_count(parent) )
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                        "Failure to decrement thrd_count.");
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
    H5P_mt_list_table_entry_t  * entry;
    H5P_mt_prop_t              * first_prop;
    H5P_mt_prop_aptr_t           next_ptr;
    hid_t                        null_list_id = 0;
    uint32_t                     phys_pl_len;
    uint32_t                     i;

    H5P_mt_list_t             * ret_value;

    FUNC_ENTER_PACKAGE


    thrd = atomic_load(&(list->thrd));

    assert(thrd.count == 0);
    assert(thrd.opening == FALSE);
    assert(thrd.closing == FALSE);

    /* Clears the list's fields */

    /** TODO: turn this into an atomic_compare_strong() */
    atomic_store(&(list->tag), H5P_MT_LIST_INVALID_TAG);

    list->pclass_id  = H5I_INVALID_HID;
    list->pclass_ptr = NULL;
    
    atomic_store(&(list->plist_id), null_list_id);

    /* Clear the lkup_tbl, including free any allocated memory */
    for ( i = 0; i < list->nprops_inherited; i++ )
    {
        entry = &list->lkup_tbl[i];

        entry->chksum = 0;                

        free(entry->name);

        entry->name = NULL;

        prop_ref = atomic_load(&(entry->base));

        first_prop = prop_ref.ptr;

        atomic_fetch_sub(&(first_prop->ref_count), 1);

        prop_ref.ptr = NULL;
        prop_ref.ver = 0;

        entry->base_delete_version = 0;

        prop_ref = atomic_load(&(entry->curr));

        prop_ref.ptr = NULL;
        prop_ref.ver = 0;

    } /* end for ( i = 0; i < list->nprops_inherited; i++ ) */

    free(list->lkup_tbl);

    list->lkup_tbl = NULL;

    /* Iterate the LFSLL and add all properties to the free list */
    phys_pl_len = atomic_load(&(list->phys_pl_len));

    for ( i = 0; i < ( phys_pl_len ); i++ )
    {
        first_prop = list->pl_head;
        assert(first_prop);
        assert(atomic_load(&(first_prop->tag)) == H5P_MT_PROP_TAG);

        next_ptr = atomic_load(&(first_prop->next));
        
        list->pl_head = next_ptr.ptr;

        if ( H5P__mt_close_prop(first_prop) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL,
                        "Failed to add property to free list.");


        atomic_fetch_sub(&(list->phys_pl_len), 1);

    } /* end for() */

    atomic_store(&(list->log_pl_len), 0);


    /* Ensure the list is empty, then free the list */
    first_prop = list->pl_head;
    assert( ! first_prop);

    assert( 0 == (atomic_load(&(list->log_pl_len))));
    assert( 0 == (atomic_load(&(list->phys_pl_len))));


    if ( 0 > (H5P__reset_stats_list(list)) )
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                    "Failed resetting stats fields.");
        

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.num_lists_freed), 1);


done:

    ret_value = list;

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__clear_mt_list() */



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
    H5P_mt_prop_value_t prop_value ;

    H5P_mt_prop_t     * ret_value = NULL;

    FUNC_ENTER_PACKAGE

    nulls_prop_ptr.ptr = NULL;
    nulls_prop_ptr.deleted = FALSE;
    nulls_prop_ptr.dummy_bool_1 = FALSE;
    nulls_prop_ptr.dummy_bool_2 = FALSE;
    nulls_prop_ptr.dummy_bool_3 = FALSE;

    assert(prop);

    if ( 0 != (atomic_load(&(prop->ref_count))) )
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
            "The property being cleared has a non-zero ref_count.");
    }

    assert(H5P_MT_PROP_INVALID_TAG);

    /* Clear all fields */
    atomic_store(&(prop->next), nulls_prop_ptr);
    
    prop->chksum = 0;
    
    free(prop->name);
    prop->name = NULL;

    prop_value = atomic_load(&(prop->value));
    if ( prop_value.ptr )
        free(prop_value.ptr);

    prop_value = nulls_value;
    atomic_store(&(prop->value), prop_value);

    atomic_store(&(prop->create_version), 0);
    atomic_store(&(prop->delete_version), 0);

done:

    ret_value = prop;

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__clear_mt_prop() */



/****************************************************************************************
 * Function:    H5P__mt_version_check
 *
 * Purpose:     Compares the curr_version to the next_version. 
 *
 *              To ensure true atomicity, any modification to either a class's or list's
 *              LFSLL must be done one thread at a time. So, the curr_version must be one
 *              less than next_version, and if it is less than one it sleeps and checks
 *              again. 
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
H5P__mt_version_check(void *param, uint64_t curr_version, uint64_t next_version)
{
    H5P_mt_type_t    tag;
    H5P_mt_class_t * class = NULL;
    H5P_mt_list_t  * list  = NULL;

    uint64_t           ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    tag = *((H5P_mt_type_t *) param);

    if ( tag == CLASS_TAG )
    {
        class = (H5P_mt_class_t *)param;
    }
    else if ( tag == LIST_TAG )
    {
        list = (H5P_mt_list_t *)param;
    }
    else
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, 0, "Tag is not a list or a class.");
    }


    while ( (curr_version + 1) < next_version )
    {
        sleep(1);

        if ( class )
        {
            atomic_fetch_add(&(class->num_wait_for_curr_version_to_inc), 1);
            curr_version = atomic_load(&(class->curr_version));
        }
        else
        {
            atomic_fetch_add(&(list->num_wait_for_curr_version_to_inc), 1);
            curr_version = atomic_load(&(list->curr_version));
        }

#if H5P_MT_SINGLE_THREAD_TESTING
        curr_version++;
#endif
        
    } /* end while ( curr_version + 1 < next_version ) */

    if ( curr_version >= next_version )
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, 0, 
                    "Threads were not operated in correct version order.");
    }

    assert((curr_version + 1) == next_version);

    ret_value = curr_version;

    done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_version_check() */



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
    H5P_mt_type_t                tag;
    H5P_mt_class_t             * class = NULL;
    H5P_mt_list_t              * list  = NULL;
    H5P_mt_active_thread_count_t thrd;
    H5P_mt_active_thread_count_t update_thrd;
    bool                         done    = FALSE;

    herr_t                       ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    tag = *((H5P_mt_type_t *) param);

    if ( tag == CLASS_TAG )
    {
        class = (H5P_mt_class_t *)param;
    }
    else if ( tag == LIST_TAG )
    {
        list = (H5P_mt_list_t *)param;
    }
    else
    {
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Tag is not a list or a class.");
    }


    /* Ensure the class isn't opening or closing and increment thread count */
    do 
    {
        if ( class )
            thrd = atomic_load(&(class->thrd));
        else
            thrd = atomic_load(&(list->thrd));

        /* If closing, throw an error */
        if ( thrd.closing )
        {
            /* This assert is normally NULL, but may be set to fail here for testing */
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Closing flag is set.");
        }
        /* If opening, sleep and try again */
        else if ( thrd.opening )
        {
            /* update stats */
            if ( class )
                atomic_fetch_add(&(class->num_thrd_opening_flag_set), 1);
            else
                atomic_fetch_add(&(list->num_thrd_opening_flag_set), 1);

#if H5P_MT_SINGLE_THREAD_TESTING

            /** 
             * This is for testing that this MT safty net gets trigged while running
             * tests in single thread, to prevent an infinite loop.
             */
            return FAIL;
#else

            sleep(1);

#endif
        }
        else
        {
            update_thrd = thrd;
            update_thrd.count++;

            if ( class )
            {
                if ( ! atomic_compare_exchange_strong(&(class->thrd), 
                                                        &thrd, update_thrd))
                {
                    /* attempt failed, update stats and try again */
                    atomic_fetch_add(&(class->num_thrd_update_cols), 1);
                }
                else
                {
                    /* attempt succeded update stats and set done */
                    atomic_fetch_add(&(class->num_thrd_count_update), 1);

                    done = TRUE;
                }
            }
            else
            {
                if ( ! atomic_compare_exchange_strong(&(list->thrd), 
                                                        &thrd, update_thrd))
                {
                    /* attempt failed, update stats and try again */
                    atomic_fetch_add(&(list->num_thrd_update_cols), 1);
                }
                else
                {
                    /* attempt succeded update stats and set done */
                    atomic_fetch_add(&(list->num_thrd_count_update), 1);

                    done = TRUE;
                }  
            }

        } /* end else */


    } while ( ! done );

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
    H5P_mt_type_t                tag;
    H5P_mt_class_t             * class = NULL;
    H5P_mt_list_t              * list  = NULL;
    H5P_mt_active_thread_count_t thrd;
    H5P_mt_active_thread_count_t update_thrd;
    bool                         done    = FALSE;

    herr_t                       ret_value = SUCCEED;


    FUNC_ENTER_NOAPI(FAIL)


    tag = *((H5P_mt_type_t *) param);

    if ( tag == CLASS_TAG )
    {
        class = (H5P_mt_class_t *)param;
    }
    else if ( tag == LIST_TAG )
    {
        list = (H5P_mt_list_t *)param;
    }
    else
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Tag is not a list or a class.");
    }


    /* Ensure the class isn't opening or closing and increment thread count */
    do 
    {
        if ( class )
        {
            thrd = atomic_load(&(class->thrd));
        }
        else
        {
            thrd = atomic_load(&(list->thrd));   
        }


        update_thrd = thrd;
        update_thrd.count--;

        if ( class )
        {
            if ( ! atomic_compare_exchange_strong(&(class->thrd), 
                                                    &thrd, update_thrd))
            {
                /* attempt failed, update stats and try again */
                atomic_fetch_add(&(class->num_thrd_update_cols), 1);
            }
            else
            {
                /* attempt succeded update stats and set done */
                atomic_fetch_add(&(class->num_thrd_count_update), 1);

                done = TRUE;
            }
        }
        else
        {
            if ( ! atomic_compare_exchange_strong(&(list->thrd), 
                                                    &thrd, update_thrd))
            {
                /* attempt failed, update stats and try again */
                atomic_fetch_add(&(list->num_thrd_update_cols), 1);
            }
            else
            {
                /* attempt succeded update stats and set done */
                atomic_fetch_add(&(list->num_thrd_count_update), 1);

                done = TRUE;
            }  
        }


    } while ( ! done );

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

    herr_t                    ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    do
    {    
        ref_count = atomic_load(&(parent->ref_count));
        update_ref = ref_count;

        if ( plc )
        {
            update_ref.plc++;
        }
        else
        {
            update_ref.pl++;
        }

        /* Attempt to atomically update the parent class's count of derived classes */
        if ( ! atomic_compare_exchange_strong(&(parent->ref_count), &ref_count, update_ref) )
        {
            /* attempt failed, update stats and try again */
            atomic_fetch_add(&(parent->num_ref_count_cols), 1);
        }
        else /* Attempt was successful */
        {
            /* attempt succeeded, update stats and continue */
            atomic_fetch_add(&(parent->num_ref_count_update), 1);

            done = TRUE;
        }        

    } while ( ! done );

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__inc_ref_count () */



/****************************************************************************************
 * Function:    H5P__dec_ref_count
 *
 * Purpose:     Decrements the reference count of a class for either plc or pl, depending
 *              on if the derived struct being deleted is a H5P_mt_class_t or a
 *              H5P_mt_list_t respectively.
 * 
 *              After decrementing the ref count, it checks if both ref counts, plc and 
 *              pl, are zero and if the class is marked deleted. If both are zero, and
 *              it's marked deleted, call H5P__mt_close_class().
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

    herr_t                    ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    do
    {    
        ref_count = atomic_load(&(parent->ref_count));
        update_ref = ref_count;

        if ( plc )
        {
            update_ref.plc--;
        }
        else
        {
            update_ref.pl--;
        }

        /* Attempt to atomically update the parent class's count of derived classes */
        if ( ! atomic_compare_exchange_strong(&(parent->ref_count), &ref_count, update_ref) )
        {
            /* attempt failed, update stats and try again */
            atomic_fetch_add(&(parent->num_ref_count_cols), 1);
        }
        else /* Attempt was successful */
        {
            /* attempt succeeded, update stats and continue */
            atomic_fetch_add(&(parent->num_ref_count_update), 1);

            done = TRUE;
        }        

    } while ( ! done );


    /** 
     * If the ref counts for derived classes and lists are zero, and 
     * the class is marked deleted, then call the close function on it.
     */
    ref_count = atomic_load(&(parent->ref_count));

    if ( ref_count.pl == 0 && ref_count.plc == 0 && ref_count.deleted )
    {
        if ( 0 > H5P__mt_close_class(parent) )
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                        "Failed to close the property list class.");
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__dec_ref_count() */



/****************************************************************************************
 * Function:    H5P__reset_stats_global
 *
 * Purpose:     Resets the stats fields for the H5P_mt_g global struct
 * 
 *              NOTE: These statistics are only  maintained in teh multi-thread 
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

    /* Free list stats */
    atomic_store(&(H5P_mt_g.prop_fl_head_update),      0ULL);
    atomic_store(&(H5P_mt_g.prop_fl_head_update_cols), 0ULL);
    atomic_store(&(H5P_mt_g.prop_fl_tail_update),      0ULL);
    atomic_store(&(H5P_mt_g.prop_fl_tail_update_cols), 0ULL);
    atomic_store(&(H5P_mt_g.prop_fl_next_update),      0ULL);
    atomic_store(&(H5P_mt_g.prop_fl_next_update_cols), 0ULL);
    atomic_store(&(H5P_mt_g.num_props_added_to_fl),    0ULL);

    /* stats for the clear functions */
    atomic_store(&(H5P_mt_g.num_classes_freed),        0ULL);
    atomic_store(&(H5P_mt_g.num_lists_freed),          0ULL);
    atomic_store(&(H5P_mt_g.num_props_freed),          0ULL);

    /* H5P_mt_class_t and H5P_mt_list_t comparison stats */
    atomic_store(&(H5P_mt_g.max_derived_classes),      0ULL);
    atomic_store(&(H5P_mt_g.max_derived_lists),        0ULL);
    atomic_store(&(H5P_mt_g.max_class_num_phys_props), 2ULL);
    atomic_store(&(H5P_mt_g.max_list_num_phys_props),  2ULL);
    atomic_store(&(H5P_mt_g.max_class_version_number), 0ULL);
    atomic_store(&(H5P_mt_g.max_list_version_number),  0ULL);

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
    atomic_init(&(class->H5P__insert_prop_setup__num_calls),  0ULL);
    atomic_init(&(class->insert_max_nodes_visited),           0ULL);
    atomic_init(&(class->insert_avg_nodes_visited),           0ULL);
    atomic_init(&(class->num_insert_nodes_visited),           0ULL);
    atomic_init(&(class->num_insert_prop_cols),               0ULL);
    atomic_init(&(class->num_insert_prop_success),            0ULL);
    atomic_init(&(class->num_insert_chksum_cols),             0ULL);

    /* H5P_mt_class_t set delete version stats */
    atomic_init(&(class->H5P__set_delete_version__num_calls), 0ULL);
    atomic_init(&(class->set_delete_max_nodes_visited),       0ULL);
    atomic_init(&(class->set_delete_avg_nodes_visited),       0ULL);
    atomic_init(&(class->num_set_delete_nodes_visited),       0ULL);
    atomic_init(&(class->num_set_delete_prop_cols),           0ULL);
    atomic_init(&(class->num_set_delete_prop_success),        0ULL);
    atomic_init(&(class->num_set_delete_chksum_cols),         0ULL);

    /* H5P_mt_class_t search stats */
    atomic_init(&(class->H5P__search_prop__num_calls),        0ULL);
    atomic_init(&(class->search_max_nodes_visited),           0ULL);
    atomic_init(&(class->search_avg_nodes_visited),           0ULL);
    atomic_init(&(class->num_search_nodes_visited),           0ULL);
    atomic_init(&(class->num_search_success),                 0ULL);
    atomic_init(&(class->num_target_prop_found_but_deleted),  0ULL);

    /* Version check stats */
    atomic_init(&(class->num_wait_for_curr_version_to_inc),   0ULL);

    /* H5P_mt_active_thread_count_t stats */
    atomic_init(&(class->num_thrd_update_cols),               0ULL);
    atomic_init(&(class->num_thrd_count_update),              0ULL);
    atomic_init(&(class->num_thrd_closing_flag_set),          0ULL);
    atomic_init(&(class->num_thrd_opening_flag_set),          0ULL);

    /* H5P_mt_class_ref_counts_t stats */
    atomic_init(&(class->num_ref_count_cols),                 0ULL);
    atomic_init(&(class->num_ref_count_update),               0ULL);

    /* Property ref_count stats */
    atomic_init(&(class->num_prop_ref_count_cols),            0ULL);
    atomic_init(&(class->num_prop_ref_count_update),          0ULL);

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
    atomic_store(&(class->H5P__insert_prop_setup__num_calls),  0ULL);
    atomic_store(&(class->insert_max_nodes_visited),           0ULL);
    atomic_store(&(class->insert_avg_nodes_visited),           0ULL);
    atomic_store(&(class->num_insert_nodes_visited),           0ULL);
    atomic_store(&(class->num_insert_prop_cols),               0ULL);
    atomic_store(&(class->num_insert_prop_success),            0ULL);
    atomic_store(&(class->num_insert_chksum_cols),             0ULL);

    /* H5P_mt_class_t set delete version stats */
    atomic_store(&(class->H5P__set_delete_version__num_calls), 0ULL);
    atomic_store(&(class->set_delete_max_nodes_visited),       0ULL);
    atomic_store(&(class->set_delete_avg_nodes_visited),       0ULL);
    atomic_store(&(class->num_set_delete_nodes_visited),       0ULL);
    atomic_store(&(class->num_set_delete_prop_cols),           0ULL);
    atomic_store(&(class->num_set_delete_prop_success),        0ULL);
    atomic_store(&(class->num_set_delete_chksum_cols),         0ULL);

    /* H5P_mt_class_t search stats */
    atomic_store(&(class->H5P__search_prop__num_calls),        0ULL);
    atomic_store(&(class->search_max_nodes_visited),           0ULL);
    atomic_store(&(class->search_avg_nodes_visited),           0ULL);
    atomic_store(&(class->num_search_nodes_visited),           0ULL);
    atomic_store(&(class->num_search_success),                 0ULL);
    atomic_store(&(class->num_target_prop_found_but_deleted),  0ULL);

    /* Version check stats */
    atomic_store(&(class->num_wait_for_curr_version_to_inc),   0ULL);

    /* H5P_mt_active_thread_count_t stats */
    atomic_store(&(class->num_thrd_update_cols),               0ULL);
    atomic_store(&(class->num_thrd_count_update),              0ULL);
    atomic_store(&(class->num_thrd_closing_flag_set),          0ULL);
    atomic_store(&(class->num_thrd_opening_flag_set),          0ULL);

    /* H5P_mt_class_ref_counts_t stats */
    atomic_store(&(class->num_ref_count_cols),                 0ULL);
    atomic_store(&(class->num_ref_count_update),               0ULL);

    /* Property ref_count stats */
    atomic_store(&(class->num_prop_ref_count_cols),            0ULL);
    atomic_store(&(class->num_prop_ref_count_update),          0ULL);

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
    atomic_init(&(list->H5P__insert_prop_setup__num_calls),    0ULL);
    atomic_init(&(list->insert_max_nodes_visited),             0ULL);
    atomic_init(&(list->insert_avg_nodes_visited),             0ULL);
    atomic_init(&(list->num_insert_nodes_visited),             0ULL);
    atomic_init(&(list->num_insert_prop_cols),                 0ULL);
    atomic_init(&(list->num_insert_prop_success),              0ULL);
    atomic_init(&(list->num_insert_chksum_cols),               0ULL);
    atomic_init(&(list->num_insert_update_entry_cols),         0ULL);
    atomic_init(&(list->num_insert_update_entry_success),      0ULL);

    /* H5P_mt_list_t set delete version stats */
    atomic_init(&(list->H5P__set_delete_version__num_calls),   0ULL);
    atomic_init(&(list->set_delete_max_nodes_visited),         0ULL);
    atomic_init(&(list->set_delete_avg_nodes_visited),         0ULL);
    atomic_init(&(list->num_set_delete_nodes_visited),         0ULL);
    atomic_init(&(list->num_set_delete_prop_cols),             0ULL);
    atomic_init(&(list->num_set_delete_prop_success),          0ULL);
    atomic_init(&(list->num_set_delete_chksum_cols),           0ULL);
    atomic_init(&(list->num_set_entry_base_delete_version),    0ULL);
    atomic_init(&(list->num_set_delete_on_curr_entry),         0ULL);
    atomic_init(&(list->num_set_delete_older_ver_than_curr),   0ULL);

    /* H5P_mt_list_t search stats */
    atomic_init(&(list->H5P__search_prop__num_calls),          0ULL);
    atomic_init(&(list->search_max_nodes_visited),             0ULL);
    atomic_init(&(list->search_avg_nodes_visited),             0ULL);
    atomic_init(&(list->num_search_nodes_visited),             0ULL);
    atomic_init(&(list->num_search_success),                   0ULL);
    atomic_init(&(list->num_search_tbl_found_base),            0ULL);
    atomic_init(&(list->num_search_tbl_found_curr),            0ULL);
    atomic_init(&(list->num_search_tbl_found_older_than_curr), 0ULL);
    atomic_init(&(list->num_target_prop_found_but_deleted),    0ULL);
    

    /* Version check stats */
    atomic_init(&(list->num_wait_for_curr_version_to_inc),     0ULL);

    /* H5P_mt_active_thread_count_t stats */
    atomic_init(&(list->num_thrd_update_cols),                 0ULL);
    atomic_init(&(list->num_thrd_count_update),                0ULL);
    atomic_init(&(list->num_thrd_closing_flag_set),            0ULL);
    atomic_init(&(list->num_thrd_opening_flag_set),            0ULL);

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
    atomic_store(&(list->H5P__insert_prop_setup__num_calls),    0ULL);
    atomic_store(&(list->insert_max_nodes_visited),             0ULL);
    atomic_store(&(list->insert_avg_nodes_visited),             0ULL);
    atomic_store(&(list->num_insert_nodes_visited),             0ULL);
    atomic_store(&(list->num_insert_prop_cols),                 0ULL);
    atomic_store(&(list->num_insert_prop_success),              0ULL);
    atomic_store(&(list->num_insert_chksum_cols),               0ULL);
    atomic_store(&(list->num_insert_update_entry_cols),         0ULL);
    atomic_store(&(list->num_insert_update_entry_success),      0ULL);

    /* H5P_mt_list_t set delete version stats */
    atomic_store(&(list->H5P__set_delete_version__num_calls),   0ULL);
    atomic_store(&(list->set_delete_max_nodes_visited),         0ULL);
    atomic_store(&(list->set_delete_avg_nodes_visited),         0ULL);
    atomic_store(&(list->num_set_delete_nodes_visited),         0ULL);
    atomic_store(&(list->num_set_delete_prop_cols),             0ULL);
    atomic_store(&(list->num_set_delete_prop_success),          0ULL);
    atomic_store(&(list->num_set_delete_chksum_cols),           0ULL);
    atomic_store(&(list->num_set_entry_base_delete_version),    0ULL);
    atomic_store(&(list->num_set_delete_on_curr_entry),         0ULL);
    atomic_store(&(list->num_set_delete_older_ver_than_curr),   0ULL);

    /* H5P_mt_list_t search stats */
    atomic_store(&(list->H5P__search_prop__num_calls),          0ULL);
    atomic_store(&(list->search_max_nodes_visited),             0ULL);
    atomic_store(&(list->search_avg_nodes_visited),             0ULL);
    atomic_store(&(list->num_search_nodes_visited),             0ULL);
    atomic_store(&(list->num_search_success),                   0ULL);
    atomic_store(&(list->num_search_tbl_found_base),            0ULL);
    atomic_store(&(list->num_search_tbl_found_curr),            0ULL);
    atomic_store(&(list->num_search_tbl_found_older_than_curr), 0ULL);
    atomic_store(&(list->num_target_prop_found_but_deleted),    0ULL);

    /* Version check stats */
    atomic_store(&(list->num_wait_for_curr_version_to_inc),     0ULL);

    /* H5P_mt_active_thread_count_t stats */
    atomic_store(&(list->num_thrd_update_cols),                 0ULL);
    atomic_store(&(list->num_thrd_count_update),                0ULL);
    atomic_store(&(list->num_thrd_closing_flag_set),            0ULL);
    atomic_store(&(list->num_thrd_opening_flag_set),            0ULL);

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

    fprintf(file_ptr, "H5P_mt_g.prop_fl_head_update      = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_head_update))));
    fprintf(file_ptr, "H5P_mt_g.prop_fl_head_update_cols = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_head_update_cols))));
    fprintf(file_ptr, "H5P_mt_g.prop_fl_tail_update      = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_tail_update))));
    fprintf(file_ptr, "H5P_mt_g.prop_fl_tail_update_cols = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_tail_update_cols))));
    fprintf(file_ptr, "H5P_mt_g.prop_fl_next_update      = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_next_update))));
    fprintf(file_ptr, "H5P_mt_g.prop_fl_next_update_cols = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.prop_fl_next_update_cols))));
    fprintf(file_ptr, "H5P_mt_g.num_props_added_to_fl    = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_added_to_fl))));

    fprintf(file_ptr, "H5P_mt_g.num_classes_freed        = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.num_classes_freed))));
    fprintf(file_ptr, "H5P_mt_g.num_lists_freed          = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.num_lists_freed))));
    fprintf(file_ptr, "H5P_mt_g.num_props_freed          = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.num_props_freed))));

    fprintf(file_ptr, "H5P_mt_g.max_derived_classes      = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.max_derived_classes))));
    fprintf(file_ptr, "H5P_mt_g.max_derived_lists        = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.max_derived_lists))));
    fprintf(file_ptr, "H5P_mt_g.max_class_num_phys_props = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.max_class_num_phys_props))));
    fprintf(file_ptr, "H5P_mt_g.max_list_num_phys_props  = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.max_list_num_phys_props))));
    fprintf(file_ptr, "H5P_mt_g.max_class_version_number = %lld\n", 
        (unsigned long long)(atomic_load(&(H5P_mt_g.max_class_version_number))));
    fprintf(file_ptr, "H5P_mt_g.max_list_version_number  = %lld\n", 
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
    char * name;
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR


    name = strdup(class->name);

    fprintf(file_ptr, "\n\nH5P Multi-Thread STATS for class %s:\n\n", name);

    fprintf(file_ptr, "class->H5P__insert_prop_setup__num_calls  = %lld\n", 
        (unsigned long long)(atomic_load(&(class->H5P__insert_prop_setup__num_calls))));
    fprintf(file_ptr, "class->insert_max_nodes_visited           = %lld\n", 
        (unsigned long long)(atomic_load(&(class->insert_max_nodes_visited))));
    fprintf(file_ptr, "class->insert_avg_nodes_visited           = %lld\n", 
        (unsigned long long)(atomic_load(&(class->insert_avg_nodes_visited))));
    fprintf(file_ptr, "class->num_insert_nodes_visited           = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_insert_nodes_visited))));
    fprintf(file_ptr, "class->num_insert_prop_cols               = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_insert_prop_cols))));
    fprintf(file_ptr, "class->num_insert_prop_success            = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_insert_prop_success))));
    fprintf(file_ptr, "class->num_insert_chksum_cols             = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_insert_chksum_cols))));

    fprintf(file_ptr, "class->H5P__set_delete_version__num_calls = %lld\n", 
        (unsigned long long)(atomic_load(&(class->H5P__set_delete_version__num_calls))));
    fprintf(file_ptr, "class->set_delete_max_nodes_visited       = %lld\n", 
        (unsigned long long)(atomic_load(&(class->set_delete_max_nodes_visited))));
    fprintf(file_ptr, "class->set_delete_avg_nodes_visited       = %lld\n", 
        (unsigned long long)(atomic_load(&(class->set_delete_avg_nodes_visited))));
    fprintf(file_ptr, "class->num_set_delete_nodes_visited       = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_set_delete_nodes_visited))));
    fprintf(file_ptr, "class->num_set_delete_prop_cols           = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_set_delete_prop_cols))));
    fprintf(file_ptr, "class->num_set_delete_prop_success        = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_set_delete_prop_success))));
    fprintf(file_ptr, "class->num_set_delete_chksum_cols         = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_set_delete_chksum_cols))));

    fprintf(file_ptr, "class->H5P__search_prop__num_calls        = %lld\n", 
        (unsigned long long)(atomic_load(&(class->H5P__search_prop__num_calls))));
    fprintf(file_ptr, "class->search_max_nodes_visited           = %lld\n", 
        (unsigned long long)(atomic_load(&(class->search_max_nodes_visited))));
    fprintf(file_ptr, "class->search_avg_nodes_visited           = %lld\n", 
        (unsigned long long)(atomic_load(&(class->search_avg_nodes_visited))));
    fprintf(file_ptr, "class->num_search_nodes_visited           = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_search_nodes_visited))));
    fprintf(file_ptr, "class->num_search_success                 = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_search_success))));

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
    fprintf(file_ptr, "class->num_ref_count_cols                 = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_ref_count_cols))));

    fprintf(file_ptr, "class->num_prop_ref_count_cols            = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_prop_ref_count_cols))));
    fprintf(file_ptr, "class->num_prop_ref_count_update          = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_prop_ref_count_update))));
    
    

    free(name);

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
    H5P_mt_class_t * parent;
    char           * name;

    herr_t           ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    parent = list->pclass_ptr;

    name = strdup(parent->name);


    fprintf(file_ptr, "\n\nH5P Multi-Thread STATS for a list from derived class %s:\n\n",
            name);


    fprintf(file_ptr, "list->H5P__insert_prop_setup__num_calls    = %lld\n", 
        (unsigned long long)(atomic_load(&(list->H5P__insert_prop_setup__num_calls))));
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
    fprintf(file_ptr, "list->num_insert_chksum_cols               = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_insert_chksum_cols))));
    fprintf(file_ptr, "list->num_insert_update_entry_cols         = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_insert_update_entry_cols))));
    fprintf(file_ptr, "list->num_insert_update_entry_success      = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_insert_update_entry_success))));

    fprintf(file_ptr, "list->H5P__set_delete_version__num_calls   = %lld\n", 
        (unsigned long long)(atomic_load(&(list->H5P__set_delete_version__num_calls))));
    fprintf(file_ptr, "list->set_delete_max_nodes_visited         = %lld\n", 
        (unsigned long long)(atomic_load(&(list->set_delete_max_nodes_visited))));
    fprintf(file_ptr, "list->set_delete_avg_nodes_visited         = %lld\n", 
        (unsigned long long)(atomic_load(&(list->set_delete_avg_nodes_visited))));
    fprintf(file_ptr, "list->num_set_delete_nodes_visited         = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_set_delete_nodes_visited))));
    fprintf(file_ptr, "list->num_set_delete_prop_cols             = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_set_delete_prop_cols))));
    fprintf(file_ptr, "list->num_set_delete_prop_success          = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_set_delete_prop_success))));
    fprintf(file_ptr, "list->num_set_delete_chksum_cols           = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_set_delete_chksum_cols))));
    fprintf(file_ptr, "list->num_set_entry_base_delete_version    = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_set_entry_base_delete_version))));
    fprintf(file_ptr, "list->num_set_delete_on_curr_entry         = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_set_delete_on_curr_entry))));
    fprintf(file_ptr, "list->num_set_delete_older_ver_than_curr   = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_set_delete_older_ver_than_curr))));

    fprintf(file_ptr, "list->H5P__search_prop__num_calls          = %lld\n", 
        (unsigned long long)(atomic_load(&(list->H5P__search_prop__num_calls))));
    fprintf(file_ptr, "list->search_max_nodes_visited             = %lld\n", 
        (unsigned long long)(atomic_load(&(list->search_max_nodes_visited))));
    fprintf(file_ptr, "list->search_avg_nodes_visited             = %lld\n", 
        (unsigned long long)(atomic_load(&(list->search_avg_nodes_visited))));
    fprintf(file_ptr, "list->num_search_nodes_visited             = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_search_nodes_visited))));
    fprintf(file_ptr, "list->num_search_success                   = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_search_success))));
    fprintf(file_ptr, "list->num_search_tbl_found_base            = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_search_tbl_found_base))));
    fprintf(file_ptr, "list->num_search_tbl_found_curr            = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_search_tbl_found_curr))));
    fprintf(file_ptr, "list->num_search_tbl_found_older_than_curr = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_search_tbl_found_older_than_curr))));

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


    free(name);

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__dump_stats_class() */



/****************************************************************************************
 * Function:    H5P__shutdown
 *
 * Purpose:     Function for correctly freeing all allocated memory at the end of testing
 *
 * Return:      VOID   
 *
 ****************************************************************************************
 */
void
H5P__shutdown(void)
{
    H5P_mt_list_t              * head_list;
    H5P_mt_list_sptr_t           fl_list_head;
    H5P_mt_list_sptr_t           next_list;
    H5P_mt_list_sptr_t           fl_list_tail;
    H5P_mt_class_t             * head_class;
    H5P_mt_class_sptr_t          fl_class_head;
    H5P_mt_class_sptr_t          next_class;
    H5P_mt_class_sptr_t          fl_class_tail;
    H5P_mt_prop_t              * head_prop;
    H5P_mt_prop_aptr_t           fl_prop_head;
    H5P_mt_prop_aptr_t           next_prop;
    H5P_mt_prop_aptr_t           fl_prop_tail;
    H5P_mt_active_thread_count_t thrd;
    uint32_t                     active_threads;
    bool                         done = FALSE;

    active_threads = atomic_load(&(H5P_mt_g.active_threads));

    /* As long as there are no active threads start shutdown */

    if ( active_threads == 0 )
    {
        /* Frees all property lists by iterating the list free list */

        fl_list_head = atomic_load(&(H5P_mt_g.list_fl_head));
        fl_list_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

        while ( fl_list_head.ptr )
        {
            do 
            {
                head_list = fl_list_head.ptr;
                next_list = atomic_load(&(head_list->fl_next));

                /* Ensure no other threads are in this struct */
                thrd = atomic_load(&(head_list->thrd));

                assert(thrd.count == 0);
                assert(thrd.opening == FALSE);
                assert(thrd.closing == FALSE);

                if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_head), 
                                                      &fl_list_head, next_list))
                {
                    /* tmp, just assert fail for now */
                    assert(FAIL);
                }
                else
                {
                    done = true;
                }

                /* If we are at the last entry in the free list update the tail */
                if ( fl_list_tail.ptr == fl_list_head.ptr )
                {
                    done = FALSE;

                    assert(next_list.ptr == NULL);

                    if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_tail),
                                                          &fl_list_tail, next_list))
                    {
                        /* tmp, just assert fail for now */
                        assert(FAIL);
                    }
                    else
                    {
                        done = TRUE;
                    }
                }

            } while ( ! done );

            head_list = H5P__clear_mt_list(head_list);

            free(head_list);

            fl_list_head = atomic_load(&(H5P_mt_g.list_fl_head));

            atomic_fetch_sub(&(H5P_mt_g.list_fl_len), 1);
            atomic_fetch_sub(&(H5P_mt_g.num_lists_freed), 1);

        } /* end while ( fl_list_head.ptr ) */

        /* Ensure the head and tail of the free list are NULL */
        fl_list_head = atomic_load(&(H5P_mt_g.list_fl_head));
        fl_list_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

        assert( NULL == fl_list_head.ptr );
        assert( NULL == fl_list_tail.ptr );

        done = FALSE;

        /* Frees all property classes by iterating the class free list */

        fl_class_head = atomic_load(&(H5P_mt_g.class_fl_head));
        fl_class_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

        while ( fl_class_head.ptr )
        {
            do 
            {
                head_class = fl_class_head.ptr;
                next_class = atomic_load(&(head_class->fl_next));

                /* Ensure no other threads are in this struct */
                thrd = atomic_load(&(head_class->thrd));

                assert(thrd.count == 0);
                assert(thrd.opening == FALSE);
                assert(thrd.closing == FALSE);

                if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_head), 
                                                      &fl_class_head, next_class))
                {
                    /* tmp, just assert fail for now */
                    assert(FAIL);
                }
                else
                {
                    done = true;
                }

                /* If we are at the last entry in the free list update the tail */
                if ( fl_class_tail.ptr == fl_class_head.ptr )
                {
                    done = FALSE;

                    assert(next_class.ptr == NULL);

                    if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_tail),
                                                            &fl_class_tail, next_class))
                    {
                        /* tmp, just assert fail for now */
                        assert(FAIL);
                    }
                    else
                    {
                        done = TRUE;
                    }
                }

            } while ( ! done );

            head_class = H5P__clear_mt_class(head_class);

            free(head_class);

            fl_class_head = atomic_load(&(H5P_mt_g.class_fl_head));

            atomic_fetch_sub(&(H5P_mt_g.class_fl_len), 1);
            atomic_fetch_sub(&(H5P_mt_g.num_classes_freed), 1);
            
        } /* end while ( fl_class_head.ptr ) */

        /* Ensure the head and tail of the free list are NULL */
        fl_class_head = atomic_load(&(H5P_mt_g.class_fl_head));
        fl_class_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

        assert( NULL == fl_class_head.ptr );
        assert( NULL == fl_class_tail.ptr );

        done = FALSE;

        /* Frees all property classes by iterating the class free list */

        fl_prop_head = atomic_load(&(H5P_mt_g.prop_fl_head));
        fl_prop_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

        while ( fl_prop_head.ptr )
        {
            do 
            {
                head_prop = fl_prop_head.ptr;
                next_prop = atomic_load(&(head_prop->next));

                if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.prop_fl_head), 
                                                      &fl_prop_head, next_prop))
                {
                    /* tmp, just assert fail for now */
                    assert(FAIL);
                }
                else
                {
                    done = true;
                }

                /* If we are at the last entry in the free list update the tail */
                if ( fl_prop_tail.ptr == fl_prop_head.ptr )
                {
                    done = FALSE;

                    assert(next_prop.ptr == NULL);

                    if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.prop_fl_tail),
                                                            &fl_prop_tail, next_prop))
                    {
                        /* tmp, just assert fail for now */
                        assert(FAIL);
                    }
                    else
                    {
                        done = TRUE;
                    }
                }

            } while ( ! done );

            head_prop = H5P__clear_mt_prop(head_prop);

            free(head_prop);

            fl_prop_head = atomic_load(&(H5P_mt_g.prop_fl_head));

            atomic_fetch_sub(&(H5P_mt_g.prop_fl_len), 1);
            atomic_fetch_sub(&(H5P_mt_g.num_props_freed), 1);
            
        } /* while ( fl_prop_head.ptr ) */


        /* Ensure the head and tail of the free list are NULL */
        fl_prop_head = atomic_load(&(H5P_mt_g.prop_fl_head));
        fl_prop_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

        assert( NULL == fl_prop_head.ptr );
        assert( NULL == fl_prop_tail.ptr );

    } /* end if ( active_threads == 0 ) */

    return;

} /* H5P__shutdown() */




//#endif
