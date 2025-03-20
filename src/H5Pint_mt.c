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

#define H5P_MT_SINGLE_THREAD_TESTING    1


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
H5P_mt_class_t * 
    H5P__mt_create_class(H5P_mt_class_t *parent, const char *name, 
                         H5P_plist_type_t type, uint64_t test_version);

H5P_mt_class_t * 
    H5P__mt_alloc_class(void);

H5P_mt_list_t * 
    H5P__mt_create_list(H5P_mt_class_t *parent, uint64_t test_version);

H5P_mt_list_t * 
    H5P__mt_alloc_list(void);

herr_t 
    H5P__init_lkup_tbl(H5P_mt_class_t *parent, uint64_t version, H5P_mt_list_t *list);

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
    H5P__mt_ins_or_mod_prop__main(void *param, const char *name, 
                                     void *value, size_t size);

herr_t 
    H5P__mt_ins_or_mod_prop__lfsll_ins(H5P_mt_prop_t *pl_head, H5P_mt_prop_t *new_prop, 
                                       uint32_t *deletes_ptr, uint32_t *nodes_visited_ptr, 
                                       uint32_t *thrd_cols_ptr);

//herr_t 
//    H5P__mt_delete_prop(void *param, H5P_mt_prop_t *target_prop);

herr_t 
    H5P__set_delete_version(void *param, H5P_mt_prop_t *target_prop);

H5P_mt_prop_t * 
    H5P__mt_search_prop(void *param, H5P_mt_prop_t *target_prop);

H5P_mt_list_table_entry_t * 
    H5P__mt_search_lkup_tbl(H5P_mt_list_table_entry_t * lkup_tbl, 
                            uint32_t left_entry,
                            uint32_t right_entry,
                            H5P_mt_prop_t * target_prop);

H5P_mt_prop_t * 
    H5P__mt_entry_find_version(H5P_mt_list_table_entry_t * entry,
                                           H5P_mt_prop_t * target_prop,
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
    H5P__mt_compare_prop(H5P_mt_prop_t *first_prop, H5P_mt_prop_t *second_prop);

herr_t 
    H5P__add_prop_onfl(H5P_mt_prop_t *prop);

herr_t
    H5P__mt_close_class(H5P_mt_class_t * class);

H5P_mt_class_t *
    H5P__clear_mt_class(H5P_mt_class_t *class);

herr_t
    H5P__mt_close_list(H5P_mt_list_t * list);

H5P_mt_list_t * 
    H5P__clear_mt_list(H5P_mt_list_t *list);

H5P_mt_prop_t * 
    H5P__clear_mt_prop(H5P_mt_prop_t *prop);


//herr_t 
//    H5P__clear_prop_free_list(void);

herr_t H5P__mt_version_check(void *param, uint64_t curr_version, 
                               uint64_t next_vesrion);

herr_t H5P__inc_thrd_count(void *param);

herr_t H5P__dec_thrd_count(void *param);

herr_t H5P__inc_ref_count(H5P_mt_class_t *parent, bool plc);

herr_t H5P__dec_ref_count(H5P_mt_class_t *parent, bool plc);

int64_t H5P__calc_checksum(const char *name);

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

//hid_t            H5P_CLS_ROOT_ID_g = H5I_INVALID_HID;
H5P_mt_class_t * H5P_mt_rootcls_g = NULL;
H5P_mt_t         H5P_mt_g;


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
    //H5P_mt_prop_t     * init_prop;
    H5P_mt_list_sptr_t  fl_list;
    H5P_mt_class_sptr_t fl_class;
    H5P_mt_class_t    * root_class;

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

    if ( NULL == ( root_class = H5P__mt_create_class(NULL, "root", H5P_TYPE_ROOT, 0) ) )
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "root class initialization failed");

    assert(root_class);
    assert(sizeof(root_class->thrd) == 16);
    assert(sizeof(root_class->ref_count) == 16);


    H5P_mt_rootcls_g = root_class;

#if 0
    /* Creating a property so the property free list isn't empty */
    if ( NULL == ( init_prop = H5P__mt_create_prop("init_fl_prop", NULL, 0, FALSE, 0,
                                                   NULL, NULL, NULL, NULL, NULL, 
                                                   NULL, NULL, NULL, NULL) ) )
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "free list property initialization failed");

    assert(init_prop);
    assert(sizeof(init_prop->next) == 16);

    init_prop->tag = H5P_MT_PROP_INVALID_TAG;
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
 *              Next, memory is allocated for the new H5P_mt_class_t and the fields are
 *              initialized. The new class's pl_head is initialized to the property 
 *              (H5P_mt_prop_t) that is returned by the function H5P__create_sentinels(),
 *              which returns the negative sentinel.
 *              The parent class's LFSLL is then iterated to find the valid properties.
 *              Two things make a property invalid:
 *                  1) any property with a create_version greater than the parent class's
 *                     version from which the new class is derived.
 *                  2) any property with a delete_version less than or equal to the 
 *                     parent class's version from which the new class is derived.
 *              When iterating the parent's LFSLL and the next valid property is found,
 *              a new property is created using the valid property's name field, and its
 *              value (H5P_mt_prop_value_t) field. That new property is then inserted 
 *              into the new class's LFSLL, with it's create_version at 1 to show that
 *              this property was added during the class's creation.
 *              
 *              The thrd.opening field of the new class is initialized TRUE, to prevent
 *              other threads from accessing the structure until it's completely set up.
 *              
 *              After the fields are initialized, the parent class increments it's 
 *              ref_count->plc field to count the new derived class.
 * 
 *              NOTE: the H5P__init_stats_class() function is called to initialize the
 *              stats fields of the H5P_mt_class_t structure to collect stats for
 *              testing purposes.
 * 
 *              NOTE: will probably add the code to insert the new class into the index
 *              at this spot in the function.
 * 
 *              Finally, the thrd fields are updated for the new class and parent class.
 *              The new class's thrd.opening is set to FALSE allowing other threads to 
 *              now access this structure. The parent class's thrd.count is decremented.
 *              
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
                     uint64_t test_version)
{
    H5P_mt_class_t             * new_class = NULL;
    hid_t                        parent_id;
    H5P_mt_class_ref_counts_t    ref_count;
    H5P_mt_active_thread_count_t thrd;
    //H5P_mt_class_sptr_t          fl_head;
    H5P_mt_class_sptr_t          fl_next;
    //H5P_mt_class_sptr_t          fl_update_head;
    H5P_mt_prop_t              * parent_prop;
    H5P_mt_prop_t              * valid_prop = NULL;
    H5P_mt_prop_t              * new_prop;
    H5P_mt_prop_value_t          value;
    uint64_t                     parent_version = 0;
    uint32_t                     phys_pl_len;
    uint32_t                     log_pl_len;
    uint32_t                     deletes       = 0;
    uint32_t                     nodes_visited = 0;
    uint32_t                     thrd_cols     = 0;
    bool                         inc_thrd_flag = FALSE;
    //bool                         done          = FALSE;

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


#if 0
    /* Allocate memory for the new property list class */
    new_class = (H5P_mt_class_t *)malloc(sizeof(H5P_mt_class_t));
    if ( NULL == new_class )
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "property list class allocation failed");
    }

#else
    /* Allocates a new property list class */
    new_class = H5P__mt_alloc_class();
    if ( NULL == new_class )
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, 
                    "Failed to create new property list class.");

#endif

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
            parent_prop = parent->pl_head;
            assert(parent_prop);
            assert(atomic_load(&(parent_prop->tag)) == H5P_MT_PROP_TAG);
            assert(parent_prop->sentinel);

            
            do
            {
                /* Gets the next valid prop or NULL if there isn't another valid */
                valid_prop = H5P__get_next_valid_prop(parent_prop, parent_version);

                if ( valid_prop )
                {
                    value = atomic_load(&(valid_prop->value));

                    /* Creates a new property from the parent's valid_prop */
                    new_prop = H5P__mt_create_prop(valid_prop->name, value.ptr, value.size,
                                                   TRUE, 1, NULL, NULL, NULL, NULL, NULL, 
                                                   NULL, NULL, NULL, NULL);
                    if ( NULL == new_prop )
                        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, NULL, 
                                    "Failed creating property for property list class.");


                    /* Inserts the new_prop into the new_class's LFSLL */
                    H5P__mt_ins_or_mod_prop__lfsll_ins(new_class->pl_head,
                                                    new_prop,
                                                    &deletes,
                                                    &nodes_visited,
                                                    &thrd_cols);

                    phys_pl_len++;
                    log_pl_len++;

                    parent_prop = valid_prop;
                }

            } while ( valid_prop );
                   
            /* update physical and logical lengths */
            atomic_store(&(new_class->phys_pl_len), phys_pl_len);
            atomic_store(&(new_class->log_pl_len), log_pl_len);

        
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

#if 0
    /**
     * NOTE: adding the class to the free list during creation is for testing 
     * reasons and will be removed when integrated into the HDF5 library.
     */
    do
    {
        fl_head = atomic_load(&(H5P_mt_g.class_fl_head));

        fl_next.ptr = fl_head.ptr;
        fl_next.sn  = fl_head.sn + 1;
        atomic_store(&(new_class->fl_next), fl_next);

        fl_update_head.ptr = new_class;
        fl_update_head.sn  = 0;

        if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_head), 
                                                &fl_head, fl_update_head))
        {
            /* failed, updated stats and try again */
            atomic_fetch_add(&(H5P_mt_g.class_fl_head_update_cols), 1);
        }
        else
        {
            /* success, update stats and continue */
            atomic_fetch_add(&(H5P_mt_g.class_fl_head_update), 1);
            atomic_fetch_add(&(H5P_mt_g.num_class_added_to_fl), 1);

            atomic_fetch_add(&(H5P_mt_g.class_fl_len), 1);

            done = TRUE;
        }

    } while ( ! done );
#endif



    /* Initializes the class's stats fields */
    H5P__init_stats_class(new_class);
    

    /**
     * TODO: will probably add the code to insert this class into the index here.
     */


    /* update thrd struct of the new_class to reflect it is no longer opening */
    thrd.count   = 0;
    thrd.opening = FALSE;
    thrd.closing = FALSE;

    atomic_store(&(new_class->thrd), thrd);

    /* update parent's ref count*/
    if ( parent != NULL )
    {
        H5P__inc_ref_count(parent, TRUE);
    }

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

    /* If allocating root class or no class structs are reallocable, alloc from memory */
    if ( ( atomic_load(&(head_class->tag)) != H5P_MT_CLASS_FL_REALLOC_TAG ) ||
           head_class == NULL )
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
 *              property list class (H5P_mt_class_t).
 * 
 *              NOTE: property list classes are referred to as classes and property lists
 *              are referred to as lists.
 *
 *              A multi-thread safe function to create a new list, derived from an 
 *              existing class. Lists create an array of H5P_mt_list_table_entry_t of 
 *              length nprops_inherited which point to the valid properties in the parent
 *              class's LFSLL.         
 * 
 * Details:     NOTE: for more information of the H5P_mt_list_t structure or specific
 *              fields, check the detailed comment for the struture in H5Ppkg_mt.h 
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
 *              The thrd.opening field of the new list is initialized TRUE, to prevent
 *              other threads from accessing the structure until it's completely set up.
 * 
 *              Next, memory is allocated for the new H5P_mt_list_t and the fields are 
 *              initialized. The list's pl_head field points to the negative sentinel 
 *              that is returned from the H5P__create_sentinels() function. The lkup_tbl
 *              is allocated and initialized by the function H5P__init_lkup_tbl().
 *              
 *              After the fields are initialized, the parent class increments it's 
 *              ref_count->pl field to count the new derived list.
 * 
 *              NOTE: the H5P__init_stats_list() function is called to initialize the
 *              stats fields of the H5P_mt_list_t structure to collect stats for
 *              testing purposes.
 * 
 *              NOTE: will probably add the code to insert the new list into the index
 *              at this spot in the function.
 * 
 *              Finally, the thrd fields are updated for the new list and parent class.
 *              The new list's thrd.opening is set to FALSE allowing other threads to 
 *              now access this structure. The parent class's thrd.count is decremented.
 *              
 *
 * Return:      Success: Returns a pointer to the new H5P_mt_list_t struct.
 * 
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_list_t *
H5P__mt_create_list(H5P_mt_class_t *parent, uint64_t test_version)
{
    H5P_mt_list_t              * new_list = NULL;
    hid_t                        parent_id;
    H5P_mt_active_thread_count_t list_thrd;
    //H5P_mt_list_sptr_t           fl_head;
    H5P_mt_list_sptr_t           fl_next;
    //H5P_mt_list_sptr_t           fl_update_next;
    uint64_t                     parent_version;
    bool                         inc_thrd_flag = FALSE;
    //bool                         done = FALSE;

    H5P_mt_list_t              * ret_value = NULL;

    FUNC_ENTER_PACKAGE


    assert(parent);
    assert( (atomic_load(&(parent->tag))) == H5P_MT_CLASS_TAG);

    parent_version = atomic_load(&(parent->curr_version));

    /**
     * NOTE: This is for testing to choose a specific version to create the list at to 
     * ensure only the valid properties for that version are stored in the lkup_tbl.
     */
    if ( test_version > 0 )
    {
        assert(test_version <= parent_version);

        parent_version = test_version;
    }

    
    /* Increment parent's thrd count */
    if ( H5P__inc_thrd_count(parent) < 0 )
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, 
                        "Couldn't increment parent's thread count.");
    else
        inc_thrd_flag = TRUE;

#if 0
    /* Allocate memory for the new property list */
    new_list = (H5P_mt_list_t *)malloc(sizeof(H5P_mt_list_t));
    if ( NULL == new_list )
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_CANTALLOC, NULL, "property list allocation failed");
    }
#else
    /* Allocates a new property list */
    new_list = H5P__mt_alloc_list();
    if ( NULL == new_list )
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, 
                    "Failed to create new property list.");

#endif


    /* Initialize list fields */
    atomic_store(&(new_list->tag), H5P_MT_LIST_TAG);

    parent_id = atomic_load(&(parent->id));    
    new_list->pclass_id = parent_id;

    new_list->pclass_ptr = parent;
    new_list->pclass_version = parent_version;

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

    /**
     * NOTE: will need to set this up correctly when I add callbacks.
     */
    atomic_store(&(new_list->class_init), TRUE);

    list_thrd.count   = 0;
    list_thrd.opening = TRUE;
    list_thrd.closing = FALSE;
    atomic_store(&(new_list->thrd), list_thrd);

    fl_next.ptr = NULL;
    fl_next.sn  = 0;
    atomic_store(&(new_list->fl_next), fl_next);

#if 0
    /**
     * NOTE: adding the list to the free list during creation is for testing 
     * reasons and will be removed when integrated into the HDF5 library.
     */
    do 
    {
        fl_head = atomic_load(&(H5P_mt_g.list_fl_head));

        fl_next.ptr = fl_head.ptr;
        fl_next.sn  = 0;
        atomic_store(&(new_list->fl_next), fl_next);

        fl_update_next.ptr = new_list;
        fl_update_next.sn  = 0;

        if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_head), 
                                              &fl_head, fl_update_next))
        {
            /* failed, updated stats and try again */
            atomic_fetch_add(&(H5P_mt_g.list_fl_head_update_cols), 1);
        }
        else
        {
            /* success, update stats and continue */
            atomic_fetch_add(&(H5P_mt_g.list_fl_head_update), 1);
            atomic_fetch_add(&(H5P_mt_g.num_list_added_to_fl), 1);

            atomic_fetch_add(&(H5P_mt_g.list_fl_len), 1);

            done = TRUE;
        }

    } while ( ! done );
#endif


    /* Allocate and nitialize the lkup_tbl */
    if ( 0 > (H5P__init_lkup_tbl(parent, parent_version, new_list) ) )
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, NULL, "Failed to create lkup_tbl.");
    


    /* Initialize all the stats for the list */
    H5P__init_stats_list(new_list);

    /**
     * TODO: will probably add the code to insert this class into the index here.
     */


    /* update thrd struct of the new_list */
    list_thrd.count   = 0;
    list_thrd.opening = FALSE;
    list_thrd.closing = FALSE;

    atomic_store(&(new_list->thrd), list_thrd);

    /* update parent's ref count and thrd count */
    H5P__inc_ref_count(parent, FALSE);
    

done:

    /* update parent's thrd count */
    if ( inc_thrd_flag )
    {
        if ( 0 > H5P__dec_thrd_count(parent) )
        HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                    "Failure to decrement thrd_count.");
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

    if ( (atomic_load(&(head_list->tag)) != H5P_MT_LIST_FL_REALLOC_TAG ) ||
         head_list == NULL )
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
 *              The function first iterates through the LFSLL of the list's parent class,
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
 *              doesn't have a create or copy callback, the base.ptr is set to point to 
 *              the property in the parent class's LFSLL, and base.ver is set to the 
 *              intial version of the list (which is 1). base_delete_version is 
 *              initialized to 0, and curr.ptr and curr.ver are initialized to NULL and 0
 *              respectively. Lastly, the parent's property increments it's ref_count, to
 *              count the new list having a pointer to that property. 
 * 
 *              However, if the parent's property has a create or copy callback, a new
 *              property structure must be created, as a copy of the parent's. The new
 *              prop will be inserted into the list's LFSLL, and curr.ptr will point to
 *              the new_prop, curr.ver will be set to 1 (as will the new_prop struct).
 *              base.ptr will be set to NULL, because we must ensure that we don't do
 *              anything to change or edit the parent's prop.              
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
H5P__init_lkup_tbl(H5P_mt_class_t *parent, uint64_t version, H5P_mt_list_t *list)
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
            list->nprops_inherited++;

            parent_prop = valid_prop;
        }

    } while ( valid_prop );


    /* Allocate the lkup_tbl array */

    assert(list->nprops_inherited > 0);

    /* Allocates the number of entries needed in the lkup_tbl */
    list->lkup_tbl = (H5P_mt_list_table_entry_t *)malloc(list->nprops_inherited * 
                                                    sizeof(H5P_mt_list_table_entry_t));
    if ( NULL == list->lkup_tbl )
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

            entry = &list->lkup_tbl[nprops];

            entry->chksum = valid_prop->chksum;
            entry->name   = strdup(valid_prop->name);

            /**
             * If valid_prop has the create or copy callback create a new_prop,
             * insert it into the LFSLL, and have the curr.ptr point to it.
             */
            if ( valid_prop->create || valid_prop->copy )
            {
                base.ptr = NULL;
                base.ver = 1;
                atomic_store(&(entry->base), base);

                atomic_store(&(entry->base_delete_version), 0);

                valid_prop_value = atomic_load(&(valid_prop->value));

                new_prop = H5P__mt_create_prop(valid_prop->name, valid_prop_value.ptr,
                                               valid_prop_value.size, FALSE, 1, NULL,
                                               NULL, NULL, NULL, NULL, NULL, NULL, 
                                               NULL, NULL);
                if ( NULL == new_prop )
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, 
                                "Failed creating property for property list.");

                /* Inserts the new_prop into the new_class's LFSLL */
                H5P__mt_ins_or_mod_prop__lfsll_ins(list->pl_head,
                                                   new_prop,
                                                   &deletes,
                                                   &nodes_visited,
                                                   &thrd_cols);

                atomic_fetch_add(&(list->log_pl_len), 1);
                atomic_fetch_add(&(list->phys_pl_len), 1);

                curr.ptr = new_prop;
                curr.ver = 1;
                atomic_store(&(entry->curr), curr);

            }
            /* Else set the base.ptr to point to the prop in the parent's LFSLL */
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
            assert(nprops <= list->nprops_inherited);

            /* Iterate in the parent's LFSLL to look for the next valid_prop */
            parent_prop = valid_prop;
        }

    } while ( valid_prop );
    
    assert(nprops == list->nprops_inherited);

    atomic_store(&(list->nprops), nprops);

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__init_lkup_tbl() */



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

    pos_sentinel->chksum = LLONG_MAX;

    pos_sentinel->name = strdup("pos_sentinel");

    //pos_sentinel->name = (char *)"pos_sentinel";

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

    //neg_sentinel->name = (char *)"neg_sentinel";

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
    //size_t              name_len;

    H5P_mt_prop_t * ret_value = NULL;

    FUNC_ENTER_PACKAGE

    assert(name);

    /* Allocate memory for the new property */
    new_prop = (H5P_mt_prop_t *)malloc(sizeof(H5P_mt_prop_t));
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

/**
 * TODO: H5_checksum_metadata propduces different chksums than H5P__calc_checksum
 * and the tests need to be updated for the different chksums.
 */
#if 0

    name_len = strlen(name);

    new_prop->chksum = H5_checksum_metadata(name, name_len, 0);

#else

    new_prop->chksum = H5P__calc_checksum(name);

#endif

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

    if ( ( atomic_load(&(head_prop->tag)) != H5P_MT_PROP_FL_REALLOC_TAG ) ||
         head_prop == NULL )
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
 *              calls H5P__mt_create_prop() to create the new
 *              H5P_mt_prop_t struct. 
 * 
 *              The primary purpose of this function is creating the new property to be
 *              inserted, handling incrementing and decrementing thread count, updating 
 *              the version of the host structure of the LFSLL after the property was 
 *              inserted, and updating stats. 
 * 
 *              A helper function, H5P__mt_ins_or_mod_prop__lfsll_ins(), is called that
 *              performs the action insertion of the property in to the LFSLL.
 * 
 *              Lists have an extra step here. In case this is a 'modification' to an 
 *              existing property, the list must iterate its lkup_tbl and if it contains 
 *              another version of the new property, it must update the curr.ptr to point 
 *              to the new property, and update curr.ver to the version of the new 
 *              property.
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
H5P__mt_ins_or_mod_prop__main(void *param, const char *name, void *value, size_t size)
{
    H5P_mt_prop_t             * new_prop;
    H5P_mt_prop_t             * pl_head;
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
                        "Couldn't increment parent's thread count.");
        else
            inc_thrd_flag = TRUE;



        curr_version = atomic_load(&(class->curr_version));
        next_version = atomic_fetch_add(&(class->next_version), 1);

        /* Ensure another thread isn't modifying the LFSLL*/
        if ( (curr_version + 1 ) < next_version )
        {
            if ( 0 > H5P__mt_version_check(class, curr_version, next_version) )
                HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                            "Error with returned current version.");


            curr_version = atomic_load(&(class->curr_version));
        }


        /* This thread can now proceed and create the new property */
        new_prop = H5P__mt_create_prop(name, value, size, TRUE, next_version,
                                       NULL, NULL, NULL, NULL, NULL, 
                                       NULL, NULL, NULL, NULL);
        if ( NULL == new_prop )
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, 
                        "Failed to create new property.");

        pl_head = class->pl_head;
    } 
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
                        "Couldn't increment parent's thread count.");
        else
            inc_thrd_flag = TRUE;


        curr_version = atomic_load(&(list->curr_version));
        next_version = atomic_fetch_add(&(list->next_version), 1);

        /* Ensure another thread isn't modifying the LFSLL*/
        if ( (curr_version + 1 ) < next_version )
        {
            if ( 0 > H5P__mt_version_check(list, curr_version, next_version) )
                HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                            "Error with returned current version.");

            curr_version = atomic_load(&(class->curr_version));
        }


        /* This thread can now proceed and create the new property */
        new_prop = H5P__mt_create_prop(name, value, size, TRUE, next_version,
                                       NULL, NULL, NULL, NULL, NULL, 
                                       NULL, NULL, NULL, NULL);
        if ( NULL == new_prop )
            HGOTO_ERROR(H5E_PLIST, H5E_CANTCREATE, FAIL, 
                        "Failed to create new property.");

        pl_head = list->pl_head;
    } 
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
        entry = H5P__mt_search_lkup_tbl(list->lkup_tbl, 0, 
                                        (list->nprops_inherited - 1), new_prop);

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

                    done = TRUE;

                }

            } while ( ! done );
   
        } /* end if ( entry ) */
    
    } /* end if ( ! class ) */ 

    if ( class )
    {
        /* update stats */
        atomic_store(&(class->num_insert_nodes_visited), nodes_visited);
        atomic_fetch_add(&(class->num_insert_prop_cols), thrd_cols);
        atomic_fetch_add(&(class->num_insert_prop_success), 1);

        /* inc physical and logical length of the LFSLL, and nprops_added and nprops */
        atomic_fetch_add(&(class->log_pl_len),   1);
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

        /* inc physical and logical length of the LFSLL, and nprops_added and nprops */
        atomic_fetch_add(&(list->log_pl_len),   1);
        atomic_fetch_add(&(list->phys_pl_len),  1);
        atomic_fetch_add(&(list->nprops_added), 1);
        atomic_fetch_add(&(list->nprops),       1);

        /* Update curr_version */
        atomic_store(&(list->curr_version), next_version);

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

} /* H5P__mt_ins_or_mod_prop__main() */



/****************************************************************************************
 * Function:    H5P__mt_ins_or_mod_prop__lfsll_ins
 *
 * Purpose:     Inserts a new property (H5P_mt_prop_t struct) into the LFSLL of a 
 *              property list (H5P_mt_list_t) or property list class (H5P_mt_class_t).
 *
 *              Helper function that passes pointers to H5P__find_mod_point() to find the 
 *              location to insert the new property. Then inserts the new property 
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



/**
 * TODO: confirm what to do if trying to delete a property in the lkup_tbl.
 */
#if 0
/**
 * 
 */
herr_t
H5P__mt_delete_prop(void *param, H5P_mt_prop_t *target_prop)
{
    H5P_mt_prop_t * first_prop;
    H5P_mt_prop_t * second_prop;
    H5P_mt_prop_aptr_t first_prop_next;
    H5P_mt_prop_aptr_t second_prop_next;
    H5P_mt_prop_t * pl_head;
    H5P_mt_list_table_entry_t  * entry;
    H5P_mt_list_prop_ref_t       curr;
    H5P_mt_list_prop_ref_t       base;
    uint64_t        curr_version    = 0;
    uint64_t        next_version    = 0;
    uint64_t        delete_version  = 0;
    int32_t         deletes         = 0;
    int32_t         nodes_visited   = 0;
    int32_t         thrd_cols       = 0;
    int32_t         cmp_result      = 0;
    bool            done            = FALSE;
    H5P_mt_type_t      type;
    H5P_mt_class_t   * class = NULL;
    H5P_mt_list_t    * list  = NULL;

    herr_t          ret_value = SUCCEED;


    assert(target_prop);
    assert(target_prop->tag == H5P_MT_PROP_TAG);


    type = *((H5P_mt_type_t *) param);

    if ( type == CLASS_TAG )
    {
        class = (H5P_mt_class_t *)param;

        assert(class);
        assert(class->tag == H5P_MT_CLASS_TAG);

        atomic_fetch_add(&(class->H5P__delete_prop_class__num_calls), 1);

        if ( (ret_value = (H5P__inc_thrd_count(class)) ) < 0 )
            return FAIL;

        curr_version = atomic_load(&(class->curr_version));
        next_version = atomic_fetch_add(&(class->next_version), 1);

        /* Ensure another thread isn't modifying the LFSLL*/
        if ( (curr_version + 1 ) < next_version )
            if ( 0 == (curr_version = 
                       H5P__mt_version_check(class, curr_version, next_version)) )
                return FAIL;

        pl_head = class->pl_head;
    }
    else if ( type == LIST_TAG )
    {
        list = (H5P_mt_list_t *)param;

        assert(list);
        assert(list->tag == H5P_MT_LIST_TAG);

        /* update stats */
        atomic_fetch_add(&(list->H5P__delete_prop_list__num_calls), 1);

        if ( (ret_value = (H5P__inc_thrd_count(list)) ) < 0 )
            return FAIL;

        curr_version = atomic_load(&(list->curr_version));
        next_version = atomic_fetch_add(&(list->next_version), 1);

        /* Ensure another thread isn't modifying the LFSLL*/
        if ( (curr_version + 1 ) < next_version )
            if ( 0 == (curr_version = 
                       H5P__mt_version_check(list, curr_version, next_version)) )
                return FAIL;

        pl_head = list->pl_head;
    }
    else
    {
        fprintf(stderr, "Error with passed type.\n");

        return FAIL;
    }



    /**
     * If this is a list, check the lkup_tbl for the target_prop 
     * 
     * NOTE: If the target prop is still the base.ptr, then we mark done and 
     * return SUCCEED, because we cannot delete the property in the parent's LFSLL.
     */
    do
    {
        if ( ! class )
        {
            for ( uint32_t i = 0; i < list->nprops_inherited; i++ )
            {
                entry = &list->lkup_tbl[i];

                curr = atomic_load(&(entry->curr));                

                if ( ! curr.ptr )
                {
                    base = atomic_load(&(entry->base));

                    cmp_result = H5P__mt_compare_prop(target_prop, base.ptr);

                    /**
                     * Since curr.ptr is NULL and base.ptr points to the property in
                     * the parent class's LFSLL, they must be same property.
                     */
                    if ( cmp_result == 3 )
                    {
                        assert( 0 < (atomic_load(&(entry->base_delete_version))));

                        done = TRUE;

                        break;
                    }
                }
                else
                {
                    cmp_result = H5P__mt_compare_prop(target_prop, curr.ptr);

                    /* We must find this property in the LFSLL to remove it */
                    if ( cmp_result == 2 )
                    {
                        break;
                    }

                }

            } /* end for ( uint32_t i = 0; i < list->nprops_inherited; i++ ) */

        } /* end if ( ! class ) */


        /* If not done iterate the LFSLL */

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
                                target_prop,
                                &cmp_result);


            assert(first_prop);
            assert(second_prop);

            assert(first_prop->tag == H5P_MT_PROP_TAG);
            assert(second_prop->tag == H5P_MT_PROP_TAG);

            if ( cmp_result == 3 )
            {
                delete_version = atomic_load(&(second_prop->delete_version));

                assert(delete_version > 0);

                first_prop_next = atomic_load(&(first_prop->next));

                assert(first_prop_next.ptr == second_prop);

                second_prop_next = atomic_load(&(second_prop->next));

                if ( ! atomic_compare_exchange_strong(&(first_prop->next),
                                                    &first_prop_next, second_prop_next))
                {
                    /** TODO: update stats */
                }
                else
                {
                    /** TODO: update stats */

                    done = TRUE;
                }
              
            } /* end if ( cmp_result == 3 ) */

        } /* end while ( ! done ) */

    } while ( ! done );

    assert(done);

    assert(deletes >= 0);
    assert(nodes_visited >= 0);
    assert(thrd_cols >= 0);

    if ( class )
    {
        /* update stats */
        atomic_fetch_add(&(class->class_delete_nodes_visited), nodes_visited);
        atomic_fetch_add(&(class->class_num_delete_prop_cols), thrd_cols);

        /* Update the class's current version */
        atomic_store(&(class->curr_version), next_version);

        ret_value = H5P__dec_thrd_count(class);
    }
    else
    {
        /* update stats */
        atomic_fetch_add(&(list->list_insert_nodes_visited), nodes_visited);
        atomic_fetch_add(&(list->list_num_insert_prop_cols), thrd_cols);

        /* Update the list's current version */
        atomic_store(&(list->curr_version), next_version);

        ret_value = H5P__dec_thrd_count(list);
    }

    return(ret_value);



} /* H5P__mt_delete_prop() */
#endif



/****************************************************************************************
 * Function:    H5P__set_delete_version
 *
 * Purpose:     Sets the delete_version of a property (H5P_mt_prop_t) in a property list
 *              (H5P_mt_list_t) or in a property list class (H5P_mt_class_t);
 *
 *              NOTE: Current implementation does not physically or logically delete 
 *              H5P_mt_prop_t structs from the LFSLL.
 *              
 *              H5P__set_delete_version() first determines if it dealing with a list or a 
 *              class. If it's a list we iterate the lkup_tbl with the function 
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
            if ( 0 > H5P__mt_version_check(class, curr_version, next_version) )
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
            if ( 0 > H5P__mt_version_check(list, curr_version, next_version) )
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
                                            (list->nprops_inherited -1), target_prop);

            if ( entry )
            {
                first_prop = H5P__mt_entry_find_version(entry, target_prop, &base_flag);

                /* Should not ever happen, means there was a version error */
                if ( NULL == first_prop )
                {
                    assert(H5P_MT_ASSERT_FAIL);
                    HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                                "Property version error.");
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

            /* update stats */
            if ( class )
                atomic_fetch_add(&(class->num_set_delete_prop_success), 1);
            else
                atomic_fetch_add(&(list->num_set_delete_prop_success), 1);


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
 *              (H5P_mt_class_t) for a target property (H5P_mt_prop_t). 
 * 
 *              This function follows the same procedure of the function above, 
 *              H5P__set_delete_version(), only instead of setting the delete_version of
 *              the target_property and returning SUCCEED or FAIL, this function returns
 *              the pointer to the property that matches target_prop.
 *
 * Return:      Success: Returns a pointer to the property.
 * 
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__mt_search_prop(void *param, H5P_mt_prop_t *target_prop)
{
    H5P_mt_prop_t             * first_prop;
    H5P_mt_prop_t             * second_prop;
    H5P_mt_prop_t             * pl_head;
    H5P_mt_list_table_entry_t * entry;
    H5P_mt_list_prop_ref_t      curr;
    uint32_t                    deletes       = 0;
    uint32_t                    nodes_visited = 0;
    uint32_t                    thrd_cols     = 0;
    bool                        done          = FALSE;
    bool                        base_flag     = FALSE;
    bool                        inc_thrd_flag = FALSE;
    H5P_mt_type_t               tag;
    H5P_mt_class_t            * class = NULL;
    H5P_mt_list_t             * list  = NULL;

    H5P_mt_prop_t * ret_value = NULL;

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
        atomic_fetch_add(&(class->H5P__search_prop__num_calls), 1);

        /* Increment thread count */
        if ( ( H5P__inc_thrd_count(class) ) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, 
                        "Couldn't increment parent's thread count.");
        else
            inc_thrd_flag = TRUE;


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
            

        pl_head = list->pl_head;
    }
    else
    {
        assert(H5P_MT_ASSERT_FAIL);
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, 
                    "Type passed in wasn't a class or list.");
    }



    /* If this is a list, check the lkup_tbl for the target_prop */
    do 
    {
        if ( ! class )
        {
            entry = H5P__mt_search_lkup_tbl(list->lkup_tbl, 0,
                                            (list->nprops_inherited - 1), target_prop);
            
            if ( entry )
            {
                first_prop = H5P__mt_entry_find_version(entry, target_prop, &base_flag);

                /* Should not ever happen, means there was a version error */
                if ( NULL == first_prop )
                {
                    assert(H5P_MT_ASSERT_FAIL);
                    HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                                "Property version error.");
                }

                /* If TRUE, the base of the entry is the most current version */
                if ( base_flag )
                {
                    ret_value = first_prop;

                    /* update stats */
                    atomic_fetch_add(&(list->num_search_tbl_found_base), 1);

                    done = TRUE;
                }
                /* The target_prop was either curr or an older version in the LFSLL */
                else
                {
                    ret_value = first_prop;

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
                        atomic_fetch_add(&(list->num_search_tbl_found_curr), 1);
                    }
                    else
                        atomic_fetch_add(&(list->num_search_tbl_found_older_than_curr), 1);

                    
                    done = TRUE;
                }

                atomic_fetch_add(&(list->num_search_success), 1);

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

            ret_value = second_prop;

            done = TRUE;
                
        } /* end while ( ! done ) */

        /** TODO: add num_search_chksum_cols stats checking */

    } while ( ! done );

    if ( class )
    {
        /* update stats */
        atomic_store(&(class->num_search_nodes_visited), nodes_visited);
        atomic_fetch_add(&(class->num_search_success), 1);
    }
    else
    {
        /* update stats */
        atomic_store(&(list->num_search_nodes_visited), nodes_visited);
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
 *              the same chksum as the target_prop.
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
                        H5P_mt_prop_t             * target_prop)
{
    H5P_mt_list_table_entry_t * entry;
    int32_t                     left;
    int32_t                     middle;
    int32_t                     right;

    H5P_mt_list_table_entry_t * ret_value = NULL;

    FUNC_ENTER_NOAPI_NOERR

    left  = (int32_t)left_entry;
    right = (int32_t)right_entry;


    /* Binary search the lkup_tbl for the target_prop */
    while ( left <= right )
    {
        middle = left + (right - left) / 2;

        entry = &lkup_tbl[middle];

        if ( entry->chksum == target_prop->chksum )
        {
            ret_value = entry;
            break;
        }            

        else if ( entry->chksum < target_prop->chksum )
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
 *              Failure: Can not fail
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
H5P__mt_entry_find_version(H5P_mt_list_table_entry_t * entry, 
                           H5P_mt_prop_t * target_prop,
                           bool *base_flag)
{
    H5P_mt_list_prop_ref_t curr;
    H5P_mt_list_prop_ref_t base;
    H5P_mt_prop_t        * prop;
    H5P_mt_prop_aptr_t     next;
    uint64_t               curr_create_ver;
    uint64_t               curr_delete_ver;
    uint64_t               target_create_ver;
    bool                   done = FALSE;
    bool                   is_base = FALSE;

    H5P_mt_prop_t        * ret_value;

    FUNC_ENTER_PACKAGE


    assert(entry->chksum == target_prop->chksum);

    curr = atomic_load(&(entry->curr));
    base = atomic_load(&(entry->base));

    if ( curr.ptr )
    {
        target_create_ver = atomic_load(&(target_prop->create_version));

        /**
         * Ensure the prop curr is the correct version,
         * If not iterate to the next version in the LFSLL.
         */
        if ( target_create_ver == curr.ver )
        {
            ret_value = curr.ptr;
        }
        else
        {
            prop = curr.ptr;

            do 
            {
                next = atomic_load(&(prop->next));
                prop = next.ptr;

                curr_delete_ver = atomic_load(&(prop->delete_version));
                curr_create_ver = atomic_load(&(prop->create_version));

                if ( curr_create_ver == target_create_ver )
                {
                    assert(curr_delete_ver == 0);

                    done = TRUE;

                    ret_value = prop;
                }
                else if ( curr_create_ver < target_create_ver )
                {
                    /* Should not ever happen */
                    assert(H5P_MT_ASSERT_FAIL);

                    HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                        "lkup_tbl's entry's curr version is older then target version");
                }
            
            } while ( ! done );
        }

    } /* end if ( curr.ptr ) */
    else
    {
        ret_value = base.ptr;

        is_base = TRUE;
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
 *              the calling function. This is the case when searching for target_prop,
 *              either for H5P__mt_search_prop() or when trying to set the delete_version
 *              of a property for H5P__set_delete_version().
 * 
 *              If while iteration the LFSLL and comparing the properties, second_prop
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

    FUNC_ENTER_PACKAGE

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

    next_prop = atomic_load(&(first_prop->next));
    second_prop = next_prop.ptr;

    assert(second_prop);
    assert(atomic_load(&(second_prop->tag)) == H5P_MT_PROP_TAG);

    /* Iterate through the LFSLL of properties */
    do 
    {
        cmp_result = H5P__mt_compare_prop(second_prop, target_prop);
        if ( cmp_result == -1 )
        {
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "Error comparing two properties.");
        } 
        /**
         * 0 = the two properties are the same.
         * 1 = second_prop comes after target_prop in the LFSLL.
         */ 
        else if ( ( cmp_result == 0 ) || ( cmp_result == 1 ) )
        {

            done = TRUE;
        }
       

        /* If ! done, update the pointers to iterate the LFSLL */
        if ( ! done )
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


done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__find_mod_point() */



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

    H5P_mt_prop_t    * ret_value;

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
        ( ( delete_ver == 0 ) || delete_ver >= version ) )
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
 * Return:      Success:  
 *                  0 if the two properties are the same.
 * 
 *                  1 if lfsll_prop should come after target_prop in the LFSLL
 * 
 *                  2 if target_prop should come after lfsll_prop in the LFSLL 
 * 
 *              Failure: -1
 ****************************************************************************************
 */
int32_t
H5P__mt_compare_prop(H5P_mt_prop_t *lfsll_prop, H5P_mt_prop_t *target_prop)
{
    int32_t  str_cmp;
    uint64_t lfsll_create_ver;
    uint64_t target_create_ver;

    int32_t  ret_value = 0;

    FUNC_ENTER_PACKAGE


    assert(lfsll_prop);
    assert(atomic_load(&(lfsll_prop->tag)) == H5P_MT_PROP_TAG);

    assert(target_prop);
    assert(atomic_load(&(target_prop->tag)) == H5P_MT_PROP_TAG);

    /* If the two props have the same chksum move on comparing names, else return 0 */
    if ( lfsll_prop->chksum == target_prop->chksum )
    {
        str_cmp = strcmp(lfsll_prop->name, target_prop->name);

        /* If str_cmp == 0 then the two properties have the same name */
        if ( str_cmp == 0 )
        {
            lfsll_create_ver  = atomic_load(&(lfsll_prop->create_version));
            target_create_ver = atomic_load(&(target_prop->create_version));

            if ( lfsll_create_ver < target_create_ver )
            {
                ret_value = 1;
            }
            else if ( lfsll_create_ver > target_create_ver )
            {
                ret_value = 2;
            }
            else
            {
                ret_value = 0; /* The two properties are the same */
            }
        }
        /* If the properties have the same chksum but different names through an error */
        else
        {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, (int32_t)(-1), 
                        "Two different properties have the same checksum.");

        }
    }
    else if ( lfsll_prop->chksum > target_prop->chksum )
    {
        ret_value = 1;
    }
    else if ( lfsll_prop->chksum < target_prop->chksum )
    {
        ret_value = 2;
    }


done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__mt_compare_prop() */



#if 1 
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
 * Function:    H5P__add_prop_onfl
 *
 * Purpose:     Adds a property (H5P_mt_prop_t) to the head of the property free list.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
H5P__add_prop_onfl(H5P_mt_prop_t *prop)
{
    H5P_mt_prop_aptr_t fl_head;
    H5P_mt_prop_aptr_t fl_update_head;
    H5P_mt_prop_aptr_t prop_next;
    bool               done = FALSE;

    herr_t            ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOERR

    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert( 0 == (atomic_load(&(prop->ref_count))));

    /** TODO: turn this into a atomic_compare_strong() */
    atomic_store(&(prop->tag), H5P_MT_PROP_VALID_ONFL_TAG);

    do 
    {
        prop_next = prop->next;

        fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));
        assert(fl_head.deleted == FALSE);

        /* Atomically update the prop being added to point to the current head */
        if ( ! atomic_compare_exchange_strong(&(prop->next),
                                              &prop_next, fl_head) )
        {
            /* update stats */
            atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update_cols), 1);
        }
        else
        {
            do 
            {
                fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));

                fl_update_head = fl_head;
                fl_update_head.ptr = prop;

                if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.prop_fl_head),
                                                      &fl_head, fl_update_head))
                {
                    /* update stats */
                    atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update_cols), 1);
                }
                else
                {
                    /* success, update stats and continue */
                    atomic_fetch_add(&(H5P_mt_g.prop_fl_head_update), 1);
                    atomic_fetch_add(&(H5P_mt_g.num_props_added_to_fl), 1);

                    atomic_fetch_add(&(H5P_mt_g.prop_fl_len), 1);

                    done = TRUE;
                }

            } while ( ! done );

        }

    } while ( ! done );

    /* update length of the free list */
    atomic_fetch_add(&(H5P_mt_g.prop_fl_len), 1);

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.num_props_added_to_fl), 1);


    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__add_prop_onfl() */



/****************************************************************************************
 * Function:    H5P__mt_close_class
 * 
 * Purpose:     Multithread safe function to close a property list class.
 * 
 *              First check that there are no threads currently active in the structure, 
 *              and that there aren't any existing classes or lists derived from this 
 *              class. 
 * 
 *              If those are true, we decrement the plc of this class's parent (if 
 *              applicable), and then add this class to the head of the class free list 
 *              instead of freeing the struct.
 * 
 *              We do this so if in the future we need to allocate another class struct,
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
H5P__mt_close_class(H5P_mt_class_t * class)
{
    H5P_mt_class_t             * parent = NULL;
    H5P_mt_class_sptr_t          fl_head;
    H5P_mt_class_sptr_t          fl_next;
    H5P_mt_class_sptr_t          fl_update_head;
    H5P_mt_class_sptr_t          class_next;
    H5P_mt_active_thread_count_t thrd;
    H5P_mt_active_thread_count_t closing_thrd;
    H5P_mt_class_ref_counts_t    ref_count;
    H5P_mt_class_ref_counts_t    update_rc;  /* rc = ref_count */
    bool                         done = FALSE;
    bool                         inc_thrd_flag = FALSE;
    
    herr_t                       ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(class);
    assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);

    do 
    { 
        thrd = atomic_load(&(class->thrd));
        ref_count = atomic_load(&(class->ref_count));

        assert(thrd.count == 0);
        assert(thrd.opening == FALSE);
        assert(thrd.closing == FALSE);

        assert(ref_count.pl == 0);
        assert(ref_count.plc == 0);
        assert( ! ref_count.deleted );

        closing_thrd = thrd;
        closing_thrd.closing = TRUE;

        if ( ! atomic_compare_exchange_strong(&(class->thrd), &thrd, closing_thrd))
        {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                        "Class's opening or closing flag is set and shouldn't be.");
        }

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


    /* Atomically adds the class to the class free list */
    do
    {
        fl_head = atomic_load(&(H5P_mt_g.class_fl_head));

        class_next = atomic_load(&(class->fl_next));

        /* update the class's fl_next fields */
        fl_next.ptr = fl_head.ptr;
        fl_next.sn  = class_next.sn + 1;
        atomic_store(&(class->fl_next), fl_next);

        /* update the head of the class free list */
        fl_update_head.ptr = class;
        fl_update_head.sn  = fl_head.sn + 1;

        if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.class_fl_head), 
                                                &fl_head, fl_update_head))
        {
            /* failed, updated stats and try again */
            atomic_fetch_add(&(H5P_mt_g.class_fl_head_update_cols), 1);
        }
        else
        {
            /* success, update stats and continue */
            atomic_fetch_add(&(H5P_mt_g.class_fl_head_update), 1);
            atomic_fetch_add(&(H5P_mt_g.num_class_added_to_fl), 1);

            atomic_fetch_add(&(H5P_mt_g.class_fl_len), 1);

            done = TRUE;
        }

    } while ( ! done );

done:

    thrd = atomic_load(&(class->thrd));
    if ( thrd.closing )
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
 * Purpose:     Frees all properties in a property list class and then frees the class. 
 *
 *              This function first makes sure that no other thread is accessing this
 *              class struct, and then sets the tag to H5P_MT_CLASS_INVALID_TAG to mark 
 *              that this class is no longer valid for threads to access. 
 * 
 *              This function sets its thrd.closing to TRUE, adds all properties to the 
 *              property free list, free itself, and then decrements it's parent's 
 *              plc count.
 * 
 *
 * Return:      SUCCEED/FAIL    
 *
 ****************************************************************************************
 */
H5P_mt_class_t *
H5P__clear_mt_class(H5P_mt_class_t *class)
{
    //H5P_mt_class_t             * parent;
    H5P_mt_active_thread_count_t thrd;
    //H5P_mt_active_thread_count_t closing_thrd;
    H5P_mt_prop_t              * first_prop;
    H5P_mt_prop_aptr_t           next_ptr;
    H5P_mt_class_ref_counts_t    ref_count;
    hid_t                        null_list_id = 0;
    uint32_t                     phys_pl_len;
    uint32_t                     i;
    //bool                         inc_thrd_flag = FALSE;
    //bool                         done = FALSE;
    //bool                         plc = TRUE;

    H5P_mt_class_t             * ret_value;

    FUNC_ENTER_PACKAGE

#if 0
    parent = class->parent_ptr; 

    /* If parent is NULL, then this is the root class */
    if ( ! parent )
    {
        done = TRUE;
    }
    else
    {
        /* Increment parent's thrd count */
        if ( H5P__inc_thrd_count(parent) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, 
                        "Couldn't increment parent's thread count.");
        else
            inc_thrd_flag = TRUE;

        done = TRUE;
    }

    assert(done);
#endif


    /* Free the structures inside the class and then the class itself */

    thrd = atomic_load(&(class->thrd));

    assert(thrd.count == 0);
    assert(thrd.opening == FALSE);
    assert(thrd.closing == FALSE);

    ref_count = atomic_load(&(class->ref_count));
    assert(ref_count.pl  == 0);
    assert(ref_count.plc == 0);


    /* Set lists fields for deletion */

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

        H5P__add_prop_onfl(first_prop);

        if ( first_prop->sentinel || next_ptr.deleted  )
        {
            atomic_fetch_sub(&(class->phys_pl_len), 1);
        }
        else
        {
            atomic_fetch_sub(&(class->phys_pl_len), 1);
            atomic_fetch_sub(&(class->log_pl_len), 1);
        }

    } /* end for() */


    /* Ensure the LFSLL is empty, then free the class */
    first_prop = class->pl_head;
    assert( ! first_prop);

    assert( 0 == (atomic_load(&(class->log_pl_len))));
    assert( 0 == (atomic_load(&(class->phys_pl_len))));

    if ( 0 > (H5P__reset_stats_class(class)) )
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                    "Failed resetting stats fields.");

    //free(class);
        
#if 0
        /* If parent is NULL, then this is the root class */
        if ( ! parent )
        {
            done = TRUE;
        }
        /* Otherwise, decrement the parent's ref_count and active threads count */
        else
        {
            H5P__dec_ref_count(parent, plc);

        } /* end else () */
#endif


    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.num_classes_freed), 1);

done:

#if 0
    /* update parent's thrd count */
    if ( parent != NULL && inc_thrd_flag )
    {
        if ( 0 > H5P__dec_thrd_count(parent) )
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                        "Failure to decrement parent's thrd_count.");
    }
#endif

    ret_value = class;

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__clear_mt_class() */



/****************************************************************************************
 * Function:    H5P__mt_close_list
 * 
 * Purpose:     Multithread safe function to close a property list
 *               
 *              First check that there are no threads currently active in the structure.
 * 
 *              If there isn't, we decrement the pl ref count of this lists's parent, 
 *              then add this list to the head of the list free list instead of freeing 
 *              the struct.
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
    H5P_mt_list_sptr_t           fl_head;
    H5P_mt_list_sptr_t           fl_next;
    H5P_mt_list_sptr_t           fl_update_head;
    H5P_mt_list_sptr_t           list_next;
    H5P_mt_active_thread_count_t local_thrd;
    H5P_mt_active_thread_count_t closing_thrd;
    H5P_mt_list_table_entry_t  * entry;
    H5P_mt_list_prop_ref_t       base_ref;
    H5P_mt_prop_t              * base_prop;
    H5P_mt_prop_t              * prop;
    H5P_mt_prop_aptr_t           next_prop;
    H5P_mt_prop_value_t          prop_value;
    bool                         done = FALSE;
    bool                         inc_thrd_flag = FALSE;
    
    herr_t                       ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

    /* Ensure struct isn't opening or closing, and that it's empty of threads */
    do
    {
        local_thrd = atomic_load(&(list->thrd));

        /* If closing is TRUE, throw an error */
        if ( local_thrd.closing )
        {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                        "Closing flag is set when it shouldn't be.");
        }
        /* If opening is TRUE, sleep and try again */
        else if ( local_thrd.opening )
        {
            atomic_fetch_add(&(list->num_thrd_opening_flag_set), 1);
            sleep(1);
        }
        /* If there are any threads in the struct, wait for them to drain out */
        else if ( local_thrd.count > 0 )
        {
            sleep(1);
        }
        else
        {
            done = TRUE;
        }

    } while ( ! done );


    /* Atomically set the closing to be TRUE */
    do
    {
        closing_thrd = local_thrd;
        closing_thrd.closing = TRUE;

        if ( ! atomic_compare_exchange_strong(&(list->thrd), &local_thrd, closing_thrd))
        {
            assert(H5P_MT_ASSERT_FAIL);
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                        "List's opening or closing flag is set and shouldn't be.");
        }
        else
        {
            done = TRUE;
        }
        
    } while ( ! done );

    assert(done);

    local_thrd = atomic_load(&(list->thrd));
    assert(local_thrd.opening == FALSE);
    assert(local_thrd.closing == TRUE);
    assert(local_thrd.count == 0);


    /* Check the property list initialization function completed */
    if ( atomic_load(&(list->class_init)) )
    {
        parent = list->pclass_ptr;
        assert(parent);
        assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_TAG);

        /* Call the class close callback, if needed, up the family tree */
        while ( parent ) 
        {
            /* update parent's thrd count */
            if ( H5P__inc_thrd_count(parent) < 0) 
                HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                            "Couldn't increment parent's thread count.");
            
            inc_thrd_flag = TRUE;


            if ( parent->close_func )
            {
                (parent->close_func)(list->plist_id, parent->close_data);
            }

            /* Decrement the thrd count as we move on to this class's parent */
            if ( 0 > H5P__dec_thrd_count(parent) )
                HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                            "Failure to decrement parent's thrd_count.");

            inc_thrd_flag = FALSE;

            parent = parent->parent_ptr;
        
        } /* end while ( parent) */ 

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

        base_ref = atomic_load(&(entry->base));

        if ( base_ref.ptr )
        {
            base_prop = base_ref.ptr;

            if ( base_prop->close )
            {
                prop_value = atomic_load(&(base_prop->value));

                (base_prop->close)(base_prop->name, prop_value.size, prop_value.ptr);

                /* Decrement the parent's prop's ref_count */
                assert( 0 != (atomic_load(&(base_prop->ref_count))));
                atomic_fetch_sub(&(base_prop->ref_count), 1);
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
        if ( prop->close )
        {
            prop_value = atomic_load(&(prop->value));

            (prop->close)(prop->name, prop_value.size, prop_value.ptr);
        }

        next_prop = atomic_load(&(prop->next));
        prop = next_prop.ptr;
    }

    /* Decrement parent's ref count of derived lists */
    if ( H5P__dec_ref_count(parent, FALSE) < 0 )
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                    "Couldn't decrement parent's pl ref count."); 



    done = FALSE;

    do 
    {
        fl_head = atomic_load(&(H5P_mt_g.list_fl_head));

        assert(fl_head.ptr == NULL);
        assert(fl_head.sn  == 0);

        list_next = atomic_load(&(list->fl_next));

        fl_next.ptr = fl_head.ptr;
        fl_next.sn  = list_next.sn + 1;
        atomic_store(&(list->fl_next), fl_next);

        fl_update_head.ptr = list;
        fl_update_head.sn  = fl_head.sn + 1;

        assert(fl_update_head.ptr == list);
        assert(fl_update_head.sn  == 1);

        if ( ! atomic_compare_exchange_strong(&(H5P_mt_g.list_fl_head), 
                                                &fl_head, fl_update_head))
        {
            /* failed, updated stats and try again */
            atomic_fetch_add(&(H5P_mt_g.list_fl_head_update_cols), 1);
        }
        else
        {
            /* success, update stats and continue */
            atomic_fetch_add(&(H5P_mt_g.list_fl_head_update), 1);
            atomic_fetch_add(&(H5P_mt_g.num_list_added_to_fl), 1);

            atomic_fetch_add(&(H5P_mt_g.list_fl_len), 1);

            done = TRUE;
        }

    } while ( ! done );

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
 * Purpose:     Frees all properties in a property list and then frees the list itself. 
 *
 *              This function first makes sure that no other thread is accessing this
 *              list struct, and then itsets the tag to H5P_MT_LIST_INVALID_TAG to mark 
 *              that this list is no longer valid for threads to access. 
 * 
 *              This function sets it's thrd.closing to TRUE, frees the lkup_tbl, adds
 *              all properties to the property free list, frees itself, and then 
 *              decrements it's parent's pl count.
 * 
 *
 * Return:      SUCCEED/FAIL    
 *
 ****************************************************************************************
 */
H5P_mt_list_t *
H5P__clear_mt_list(H5P_mt_list_t *list)
{
    //H5P_mt_class_t             * parent;
    H5P_mt_active_thread_count_t thrd;
    //H5P_mt_active_thread_count_t closing_thrd;
    H5P_mt_list_prop_ref_t       prop_ref;
    H5P_mt_list_table_entry_t  * entry;
    H5P_mt_prop_t              * first_prop;
    H5P_mt_prop_aptr_t           next_ptr;
    hid_t                        null_list_id = 0;
    uint32_t                     phys_pl_len;
    uint32_t                     i;
    //bool                         inc_thrd_flag = FALSE;
    //bool                         done = FALSE;
    //bool                         plc = FALSE;

    H5P_mt_list_t             * ret_value;

    FUNC_ENTER_PACKAGE

#if 0
    /* Increment the thead count for active threads in the parent class */
    parent = list->pclass_ptr;

    /* Increment parent's thrd count */
    if ( H5P__inc_thrd_count(parent) < 0 )
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, NULL, 
                    "Couldn't increment parent's thread count.");

    
    inc_thrd_flag = TRUE;
#endif

    /* Free the structures inside the list and then the list itself */

    thrd = atomic_load(&(list->thrd));

    assert(thrd.count == 0);
    assert(thrd.opening == FALSE);
    assert(thrd.closing == FALSE);

    /* Set lists fields for deletion */

    /** TODO: turn this into an atomic_compare_strong() */
    atomic_store(&(list->tag), H5P_MT_LIST_INVALID_TAG);

    list->pclass_id  = H5I_INVALID_HID;
    list->pclass_ptr = NULL;
    
    atomic_store(&(list->plist_id), null_list_id);

    /* Set lkup_tbl up to be deleted */
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

    list->lkup_tbl = NULL;

    free(list->lkup_tbl);

    /* Iterate the LFSLL and free all properties including sentinels */
    phys_pl_len = atomic_load(&(list->phys_pl_len));

    for ( i = 0; i < ( phys_pl_len ); i++ )
    {
        first_prop = list->pl_head;
        assert(first_prop);
        assert(atomic_load(&(first_prop->tag)) == H5P_MT_PROP_TAG);

        next_ptr = atomic_load(&(first_prop->next));
        
        list->pl_head = next_ptr.ptr;

        if ( H5P__add_prop_onfl(first_prop) < 0 )
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL,
                        "Failed to add property to free list.");

        if ( first_prop->sentinel || next_ptr.deleted  )
        {
            atomic_fetch_sub(&(list->phys_pl_len), 1);
        }
        else
        {
            atomic_fetch_sub(&(list->phys_pl_len), 1);
            atomic_fetch_sub(&(list->log_pl_len), 1);
        }

    } /* end for() */


    /* Ensure the list is empty, then free the list */
    first_prop = list->pl_head;
    assert( ! first_prop);

    assert( 0 == (atomic_load(&(list->log_pl_len))));
    assert( 0 == (atomic_load(&(list->phys_pl_len))));


    if ( 0 > (H5P__reset_stats_list(list)) )
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                    "Failed resetting stats fields.");

    //free(list);
#if 0
            H5P__dec_ref_count(parent, plc);
#endif
        

    /* update stats */
    atomic_fetch_add(&(H5P_mt_g.num_lists_freed), 1);


done:
#if 0
    /* update parent's thrd count */
    if ( parent != NULL && inc_thrd_flag )
    {
        if ( 0 > H5P__dec_thrd_count(parent) )
            HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, NULL, 
                        "Failure to decrement parent's thrd_count.");
    }
#endif

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


#if 0
/****************************************************************************************
 * Function:    H5P__clear_prop_free_list
 *
 * Purpose:     Iterates the property free list and calls H5P__clear_mt_prop to free each
 *              property.
 *
 * Return:      SUCCEED/FAIL    
 *
 ****************************************************************************************
 */
herr_t
H5P__clear_prop_free_list(void)
{
    H5P_mt_prop_aptr_t fl_head;
    H5P_mt_prop_t    * head_prop;
    H5P_mt_prop_t    * target_prop;
    uint64_t           len_check;

    herr_t             ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE


    fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));
    head_prop = fl_head.ptr;

    if (( ! head_prop ) || ( 0ULL == (atomic_load(&(H5P_mt_g.prop_fl_len))) ))
    {
        assert(H5P_MT_ASSERT_FAIL);
        HDONE_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                    "Nothing in the property free list to free.");
    }


    while ( head_prop )
    {
        target_prop = head_prop;
        
        assert((target_prop->tag == H5P_MT_PROP_INVALID_TAG) || 
               (target_prop->tag == H5P_MT_PROP_VALID_ONFL_TAG));

        fl_head = atomic_load(&(target_prop->next));
        head_prop = fl_head.ptr;

        if ( 0 > (H5P__clear_mt_prop(target_prop)) )
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, 
                "Error in clearing a property in the free list.");

        /* update stats */

        len_check = atomic_fetch_sub(&(H5P_mt_g.prop_fl_len), 1);
        assert(len_check > 0);
    }

    atomic_store(&(H5P_mt_g.prop_fl_head), NULL);
    atomic_store(&(H5P_mt_g.prop_fl_tail), NULL);


    done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5P__clear_prop_free_list() */
#endif


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
 *              Failure: -1
 *
 ****************************************************************************************
 */
herr_t
H5P__mt_version_check(void *param, uint64_t curr_version, uint64_t next_version)
{
    H5P_mt_type_t    tag;
    H5P_mt_class_t * class = NULL;
    H5P_mt_list_t  * list  = NULL;

    herr_t           ret_value = SUCCEED;

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
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "Tag is not a list or a class.");
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
        HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, 
                    "Threads were not operated in correct version order.");
    }

    assert((curr_version + 1) == next_version);

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
                    atomic_fetch_add(&(class->num_thrd_count_update_cols), 1);
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
                    atomic_fetch_add(&(list->num_thrd_count_update_cols), 1);
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
                atomic_fetch_add(&(class->num_thrd_count_update_cols), 1);
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
                atomic_fetch_add(&(list->num_thrd_count_update_cols), 1);
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
H5P__dec_ref_count(H5P_mt_class_t *parent, bool plc)
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

    /* Version check stats */
    atomic_init(&(class->num_wait_for_curr_version_to_inc),   0ULL);

    /* H5P_mt_active_thread_count_t stats */
    atomic_init(&(class->num_thrd_count_update_cols),         0ULL);
    atomic_init(&(class->num_thrd_count_update),              0ULL);
    atomic_init(&(class->num_thrd_closing_flag_set),          0ULL);
    atomic_init(&(class->num_thrd_opening_flag_set),          0ULL);

    /* H5P_mt_class_ref_counts_t stats */
    atomic_init(&(class->num_ref_count_cols),                 0ULL);
    atomic_init(&(class->num_ref_count_update),               0ULL);

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

    /* Version check stats */
    atomic_store(&(class->num_wait_for_curr_version_to_inc),   0ULL);

    /* H5P_mt_active_thread_count_t stats */
    atomic_store(&(class->num_thrd_count_update_cols),         0ULL);
    atomic_store(&(class->num_thrd_count_update),              0ULL);
    atomic_store(&(class->num_thrd_closing_flag_set),          0ULL);
    atomic_store(&(class->num_thrd_opening_flag_set),          0ULL);

    /* H5P_mt_class_ref_counts_t stats */
    atomic_store(&(class->num_ref_count_cols),                 0ULL);
    atomic_store(&(class->num_ref_count_update),               0ULL);

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

    /* Version check stats */
    atomic_init(&(list->num_wait_for_curr_version_to_inc),     0ULL);

    /* H5P_mt_active_thread_count_t stats */
    atomic_init(&(list->num_thrd_count_update_cols),           0ULL);
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

    /* Version check stats */
    atomic_store(&(list->num_wait_for_curr_version_to_inc),     0ULL);

    /* H5P_mt_active_thread_count_t stats */
    atomic_store(&(list->num_thrd_count_update_cols),           0ULL);
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

    fprintf(file_ptr, "class->num_thrd_count_update_cols         = %lld\n", 
        (unsigned long long)(atomic_load(&(class->num_thrd_count_update_cols))));
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

    fprintf(file_ptr, "list->num_thrd_count_update_cols           = %lld\n", 
        (unsigned long long)(atomic_load(&(list->num_thrd_count_update_cols))));
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
