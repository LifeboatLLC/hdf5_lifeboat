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

#ifdef H5_HAVE_MULTITHREAD

#define H5P_MT_DEBUG 0


/******************/
/* Local Typedefs */
/******************/

typedef enum { PCLASS, PLIST } type_t;

typedef union 
{
    H5P_mt_class_t *pclass;
    H5P_mt_list_t  *plist;

} type_union_t;

typedef struct 
{
    type_t       type;
    type_union_t data;

} param_t;

/********************/
/* Package Typedefs */
/********************/

/********************/
/* Local Prototypes */
/********************/
H5P_mt_class_t * H5P__create_class(H5P_mt_class_t *parent, uint64_t parent_version,
                                   const char *name, H5P_plist_type_t type);

int64_t H5P__calc_checksum(const char *name);

H5P_mt_prop_t *
    H5P__create_prop(const char *name, void *value_ptr, size_t value_size, 
                     bool in_prop_class, uint64_t create_version);

herr_t H5P__insert_prop_class(H5P_mt_prop_t *prop, H5P_mt_class_t *class);

herr_t H5P__delete_prop_class(H5P_mt_prop_t *prop, H5P_mt_class_t *class);

H5P_mt_prop_t * H5P__search_prop_class(H5P_mt_prop_t *prop, H5P_mt_class_t *class);

void find_mod_point(H5P_mt_class_t *class, H5P_mt_prop_t **first_ptr_ptr, 
                    H5P_mt_prop_t **second_ptr, int32_t *deletes_comp, 
                    int32_t *nodes_visited, int32_t *thrd_cols,
                    int64_t target_chksum);


herr_t H5P__insert_prop_list(H5P_mt_prop_t prop, H5P_mt_list_t list);
herr_t H5P__delete_prop_list(H5P_mt_prop_t prop, H5P_mt_list_t list);
H5P_mt_prop_t H5P__search_prop_list(H5P_mt_prop_t prop, H5P_mt_list_t list);


/*********************/
/* Package Variables */
/*********************/

H5P_mt_class_t *H5P_CLS_ROOT_g = NULL;

/*****************************/
/* Library Private Variables */
/*****************************/

/*******************/
/* Local Variables */
/*******************/

/*-------------------------------------------------------------------------
 * Function:    H5P_init
 *
 * Purpose:     Initialize the interface from some other layer.
 *
 *              At present, this function performs initializations needed
 *              for the multi-thread build of H5P.  Thus it need not be 
 *              called in other contexts.
 *
 * Return:      SUCCEED/FAIL    
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5P_init(void)
{
    H5P_mt_prop_t  * neg_sentinel;
    H5P_mt_prop_t  * pos_sentinel;

    H5P_mt_class_t * root_class;

    herr_t           ret_value = SUCCEED;


    /* Allocating and initializing the two sentinel nodes for the LFSLL in the class */
    neg_sentinel = H5P__create_prop("neg_sentinel", NULL, 0, FALSE, 0);
    pos_sentinel = H5P__create_prop("pos_sentinel", NULL, 0, FALSE, 0);

    /* Adjusting the sentinel bool to TRUE */
    neg_sentinel->sentinel = TRUE;
    pos_sentinel->sentinel = TRUE;

    /* Adjusting chksum to the min and max of int64_t for these to be sentinel nodes */
    neg_sentinel->chksum = LLONG_MIN;
    pos_sentinel->chksum = LLONG_MAX;

    atomic_store(&(neg_sentinel->next.ptr), pos_sentinel);


    /* Allocating and Initializing the root property list class */

    /* Allocate memory for the root property list class */
    root_class = (H5P_mt_class_t *)malloc(sizeof(H5P_mt_class_t));
    if ( ! root_class )
        printf(stderr, "New class allocation failed.");

    /* Initialize class fields */
    root_class->tag = H5P_MT_CLASS_TAG;

    root_class->parent_id      = 0;
    root_class->parent_ptr     = NULL;
    root_class->parent_version = 0;

    root_class->name = "root";
    atomic_store(&(root_class->id), H5I_INVALID_HID);
    root_class->type = H5P_TYPE_ROOT;

    atomic_store(&(root_class->curr_version), 1);
    atomic_store(&(root_class->next_version), 2);

    root_class->pl_head = neg_sentinel;
    atomic_store(&(root_class->log_pl_len),  0);
    atomic_store(&(root_class->phys_pl_len), 2);

    atomic_store(&(root_class->ref_count.pl),      0);
    atomic_store(&(root_class->ref_count.plc),     0);
    atomic_store(&(root_class->ref_count.deleted), FALSE);

    atomic_store(&(root_class->thrd.count),   1);
    atomic_store(&(root_class->thrd.opening), TRUE);
    atomic_store(&(root_class->thrd.closing), FALSE);

    atomic_store(&(root_class->fl_next.ptr), NULL);
    atomic_store(&(root_class->fl_next.sn),  0);


} /* H5P_init() */



H5P_mt_class_t *
H5P__create_class(H5P_mt_class_t *parent, uint64_t parent_version, 
                  const char *name, H5P_plist_type_t type)
{
    H5P_mt_class_t * new_class;

    H5P_mt_class_t * ret_value = NULL;

    /* Allocate memory for the new property list class */
    new_class = (H5P_mt_class_t *)malloc(sizeof(H5P_mt_class_t));
    if ( ! new_class )
        printf(stderr, "New class allocation failed.");

    /* Initialize class fields */
    new_class->tag = H5P_MT_CLASS_TAG;

    new_class->parent_id      = parent->id;
    new_class->parent_ptr     = parent;
    new_class->parent_version = parent_version;

    new_class->name = name;
    atomic_store(&(new_class->id), H5I_INVALID_HID);
    new_class->type = type;

    atomic_store(&(new_class->curr_version), 1);
    atomic_store(&(new_class->next_version), 2);

    new_class->pl_head = NULL;
    atomic_store(&(new_class->log_pl_len),  0);
    atomic_store(&(new_class->phys_pl_len), 2);

    atomic_store(&(new_class->ref_count.pl),      0);
    atomic_store(&(new_class->ref_count.plc),     0);
    atomic_store(&(new_class->ref_count.deleted), FALSE);

    atomic_store(&(new_class->thrd.count),   1);
    atomic_store(&(new_class->thrd.opening), TRUE);
    atomic_store(&(new_class->thrd.closing), FALSE);

    atomic_store(&(new_class->fl_next.ptr), NULL);
    atomic_store(&(new_class->fl_next.sn),  0);


} /* H5P__create_class() */



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
        size_t tlen = (len > 360) ? 360 : len; 
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



H5P_mt_prop_t *
H5P__create_prop(const char *name, void *value_ptr, size_t value_size, 
                 bool in_prop_class, uint64_t version)
{
    H5P_mt_prop_t * new_prop;

    H5P_mt_prop_t * ret_value = NULL;

    assert(name);

    /* Allocate memory for the new property */
    new_prop = (H5P_mt_prop_t *)malloc(sizeof(H5P_mt_prop_t));
    if ( ! new_prop )
        printf(stderr, "New property allocation failed."); 

    /* Initalize property fields */
    new_prop->tag = H5P_MT_PROP_TAG;

    atomic_store(&(new_prop->next.ptr),     NULL);
    atomic_store(&(new_prop->next.deleted), FALSE);

    new_prop->sentinel      = FALSE;
    new_prop->in_prop_class = in_prop_class;

    /* ref_count is only used if the property is in a property class. */
    atomic_store(&(new_prop->ref_count), 0);

    new_prop->chksum = H5P__calc_checksum(name);

    new_prop->name = (char *)name;

    atomic_store(&(new_prop->value.ptr),  value_ptr);
    atomic_store(&(new_prop->value.size), value_size);
    
    atomic_store(&(new_prop->create_version), version);
    atomic_store(&(new_prop->delete_version), 0); /* Set to 0 because it's not deleted */

    ret_value = new_prop;

    return ret_value;

} /* H5P__create_prop() */



herr_t 
H5P__insert_prop_class(H5P_mt_prop_t *new_prop, H5P_mt_class_t *class)
{
    bool done = FALSE;
    H5P_mt_prop_t * prev_prop;
    H5P_mt_prop_t * curr_prop;

    herr_t ret_value = SUCCEED;

    assert(class);
    assert(!class->ref_count.deleted);
    assert(class->tag == H5P_MT_CLASS_TAG);

    assert(new_prop);
    assert(new_prop->tag == H5P_MT_PROP_TAG);
    assert(!new_prop->next.deleted);

    /* update stats */
    /* H5P__insert_prop_class__num_calls */


    /** 
     * Iterates through the LFSLL of H5P_mt_prop_t (properites), to find the correct
     * location to insert the new property. chksum is used to determine insert location
     * by increasing value. 
     * 
     * If there is a chksum collision then property names in lexicographical order. 
     * 
     * If there again is a collision with names, then version number is used in 
     * decreasing order (newest version (higher number) first, oldest version last).
     */
    while (!done)
    {
        prev_prop == class->pl_head;
        assert(prev_prop->sentinel);
        
        atomic_store(&(curr_prop), prev_prop->next.ptr);
        assert(prev_prop != curr_prop);
        assert(curr_prop->tag == H5P_MT_PROP_TAG);

        if ( curr_prop->chksum > new_prop->chksum )
        {
            /* prep new_prop to be inserted between prev_prop and curr_prop */
            atomic_store(&(new_prop->next.ptr), curr_prop);
            
            /** Attempt to atomically insert new_prop 
             * 
             * NOTE: If this fails, another thread modified the LFSLL and we must 
             * update stats and restart to ensure new_prop is correctly inserted.
             */
            if ( !atomic_compare_exchange_strong(&(prev_prop->next.ptr), 
                                                 &curr_prop, new_prop) )
            {
                /* update stats */
                /* num_insert_prop_class_cols */

                continue;
            }
            /* The attempt was successful. Update lengths and stats */
            else
            {
                atomic_fetch_add(&(class->log_pl_len), 1);
                atomic_fetch_add(&(class->phys_pl_len), 1);

                done = TRUE;

                /* update stats */
                /* num_insert_prop_class_success */
            }
        }
        else if ( curr_prop->chksum == new_prop->chksum )
        {
            int32_t        cmp_result;
            H5P_mt_prop_t *next_prop;
            bool           str_cmp_done = FALSE;

            while ( ! str_cmp_done )
            { 
                /* update stats */
                /* num_insert_prop_class_chksum_cols */

                cmp_result = strcmp(curr_prop->name, new_prop->name);

                /* new_prop is less than curr_prop lexicographically */
                if ( cmp_result > 0 )
                {
                    /* prep new_prop to insert between prev_prop and curr_prop */
                    atomic_store(&(new_prop->next.ptr), curr_prop);

                    /** Attempt to atomically insert new_prop 
                     * 
                     * NOTE: If this fails, another thread modified the LFSLL and we must 
                     * update stats and restart to ensure new_prop is correctly inserted.
                    */
                    if ( !atomic_compare_exchange_strong(&(prev_prop->next.ptr), 
                                                         &curr_prop, new_prop) )
                    {
                        /* update stats */
                        /* num_insert_prop_class_cols */

                        break;

                    }
                    /* The attempt was successful. Update lengths and stats */
                    else 
                    {
                        atomic_fetch_add(&(class->log_pl_len), 1);
                        atomic_fetch_add(&(class->phys_pl_len), 1);

                        done = TRUE;
                        str_cmp_done = TRUE;

                        /* update stats */
                        /* num_insert_prop_class_success */
                    }
                }
                else if ( cmp_result < 0 )
                {

                    atomic_store(&(next_prop), curr_prop->next.ptr);

                    assert(next_prop->chksum >= curr_prop->chksum);

                    /** 
                     * If the next property in the LFSLL doesn't have the same chksum,
                     * then new_prop gets inserted between curr_prop and new_prop since
                     * chksum is used to sort first.
                     */
                    if ( next_prop->chksum != curr_prop->chksum )
                    {
                        /* Prep new_prop to be inserted between curr_prop and next_prop */
                        atomic_store(&(new_prop->next.ptr), next_prop);

                        /** Attempt to atomically insert new_prop 
                         * 
                         * NOTE: If this fails, another thread modified the LFSLL and we must 
                         * update stats and restart to ensure new_prop is correctly inserted.
                        */
                        if ( !atomic_compare_exchange_strong(&(curr_prop->next.ptr), 
                                                            &next_prop, new_prop) )
                        {
                            /* update stats */
                            /* num_insert_prop_class_success */

                            break;

                        }
                        /* The attempt was successful. Update lengths and stats */
                        else
                        {
                            atomic_fetch_add(&(class->log_pl_len), 1);
                            atomic_fetch_add(&(class->phys_pl_len), 1);

                            done = TRUE;
                            str_cmp_done = TRUE;

                            /* update stats */
                            /* num_insert_prop_class_success */
                        }
                    } /* end if ( next_prop->chksum != curr_prop->chksum ) */
  
                } 
                else /* cmp_results == 0 */
                {
                    /* update stats */
                    /* num_insert_prop_class_string_cols */

                    /**
                     * If the name's of curr_prop and new_prop are the same, we must
                     * move on to using version number to determine insert location.
                     */
                    if ( new_prop->create_version > curr_prop->create_version )
                    {
                        atomic_store(&(new_prop->next.ptr), curr_prop);

                        if ( !atomic_compare_exchange_strong(&(prev_prop->next.ptr), 
                                                             &curr_prop, new_prop) )
                        {
                            /* update stats */
                            /* num_insert_prop_class_cols */

                            break;
                        }
                        /* The attempt was successful. Update lengths and stats */
                        else 
                        {
                            atomic_fetch_add(&(class->log_pl_len), 1);
                            atomic_fetch_add(&(class->phys_pl_len), 1);

                            done = TRUE;
                            str_cmp_done = TRUE;

                            /* update stats */
                            /* num_insert_prop_class_success */
                        }
                        
                    } /* end if ( new_prop->create_version > curr_prop->create_version ) */
                    else
                    {
                        /* Property is already in the LFSLL, update stats and exit */
                        if ( new_prop->create_version == curr_prop->create_version )
                        {
                            /* update stats */
                            /* num_insert_prop_class_alread_in_LFSLL */
                            
                            done = TRUE;
                            str_cmp_done = TRUE;

                            break;
                        }
                        
                        atomic_store(&(next_prop), curr_prop->next.ptr);

                        assert(next_prop->chksum >= curr_prop->chksum);

                        if ( new_prop->chksum != next_prop->chksum )
                        {
                            /* Prep new_prop to be inserted between curr_prop and next_prop */
                            atomic_store(&(new_prop->next.ptr), next_prop);

                            /** Attempt to atomically insert new_prop 
                             * 
                             * NOTE: If this fails, another thread modified the LFSLL and we must 
                             * update stats and restart to ensure new_prop is correctly inserted.
                            */
                            if ( !atomic_compare_exchange_strong(&(curr_prop->next.ptr), 
                                                                &next_prop, new_prop) )
                            {
                                /* update stats */
                                /* num_insert_prop_class_success */

                                break;

                            }
                            /* The attempt was successful. Update lengths and stats */
                            else
                            {
                                atomic_fetch_add(&(class->log_pl_len), 1);
                                atomic_fetch_add(&(class->phys_pl_len), 1);

                                done = TRUE;
                                str_cmp_done = TRUE;

                                /* update stats */
                                /* num_insert_prop_class_success */
                            }
                        } /* end if ( new_prop->chksum != next_prop->chksum ) */

                    } /* end else */

                } /* end else ( cmp_results == 0 ) */ 

                /** 
                 * Must update prev_prop and curr_prop to compare next_prop
                 * with new_prop to find the correct insert location.
                 */
                atomic_store(&(prev_prop), curr_prop);
                atomic_store(&(curr_prop), next_prop);


            } /* end while ( ! str_cmp_done ) */
            

        } /* end else if ( curr_prop->chksum == new_prop->chksum ) */
    
    } /* end while (!done) */

}



void
find_mod_point(H5P_mt_class_t *class, H5P_mt_prop_t **first_ptr_ptr, 
               H5P_mt_prop_t **second_ptr_ptr, int32_t *deletes_comp, 
               int32_t *nodes_visited, int32_t *thrd_cols, 
               int64_t target_chksum)
{
    bool done = FALSE;
    bool retry = FALSE;
    int32_t cols = 0;
    int32_t delets = 0;
    int32_t nodes_visited = 0;
    H5P_mt_prop_t * first_prop;
    H5P_mt_prop_t * second_prop;
    H5P_mt_prop_t * next_prop;


    assert(class);
    assert(class->tag == H5P_MT_CLASS_TAG);
    assert(!class->ref_count.deleted);
    assert(first_ptr_ptr);
    assert(NULL == *first_ptr_ptr);
    assert(second_ptr_ptr);
    assert(NULL == *second_ptr_ptr);
    assert(deletes_comp);
    assert(nodes_visited);
    assert(thrd_cols);
    assert(target_chksum > LLONG_MIN && target_chksum < LLONG_MAX);

    /* update stats */
    /* H5P__insert_prop_class__num_calls */


    /** 
     * Iterates through the LFSLL of H5P_mt_prop_t (properites), to find the correct
     * location to insert the new property. chksum is used to determine insert location
     * by increasing value. 
     * 
     * If there is a chksum collision then property names in lexicographical order. 
     * 
     * If there again is a collision with names, then version number is used in 
     * decreasing order (newest version (higher number) first, oldest version last).
     */
    do
    {
        assert(!done);

        retry = FALSE;

        first_prop = class->pl_head;
        assert(first_prop->sentinel);
        assert(first_prop->tag == H5P_MT_PROP_TAG);
                
        second_prop = atomic_load(&(first_prop->next.ptr));
        assert(first_prop != second_prop);
        assert(second_prop->tag == H5P_MT_PROP_TAG);

        do 
        {
            while (second_prop->next.deleted)
            {
                if ( second_prop->ref_count != 0 )
                {
                    uint64_t version;

                    version = atomic_load(&(class->curr_version));
                    version--;
                    atomic_store(&(second_prop->delete_version), version);
                }
                else /* second_prop->ref_count == 0 */
                {

                    assert(first_prop->next.ptr == second_prop);
                    assert(second_prop->next.ptr != NULL);

                    next_prop = atomic_load(&(second_prop->next.ptr));
                    assert(next_prop->tag == H5P_MT_PROP_TAG);

                    if ( ! atomic_compare_exchange_strong(&(first_prop->next.ptr), 
                                                            &second_prop, next_prop) )
                    {
                        thrd_cols++;
                        retry = TRUE;
                        break;
                    }
                    else 
                    {
                        atomic_fetch_sub(&(class->phys_pl_len), 1);
                        deletes_comp++;
                        nodes_visited++;

                        second_prop = next_prop;
                        next_prop = atomic_load(&(second_prop->next.ptr));

                        assert(first_prop);
                        assert(first_prop->tag == H5P_MT_PROP_TAG);
                        assert(second_prop);
                        assert(second_prop->tag == H5P_MT_PROP_TAG);
                    }
                } /* end else ( second_prop->ref_count == 0 ) */

            } /* end while ( second_prop->next.deleted ) */

            
        
        } while ( ( ! done ) && ( ! retry ) ); 



        if ( second_prop->chksum > target_chksum )
        {


#if 0
            /* prep new_prop to be inserted between prev_prop and curr_prop */
            atomic_store(&(new_prop->next.ptr), curr_prop);
            
            /** Attempt to atomically insert new_prop 
             * 
             * NOTE: If this fails, another thread modified the LFSLL and we must 
             * update stats and restart to ensure new_prop is correctly inserted.
             */
            if ( !atomic_compare_exchange_strong(&(prev_prop->next.ptr), 
                                                 &curr_prop, new_prop) )
            {
                /* update stats */
                /* num_insert_prop_class_cols */

                continue;
            }
            /* The attempt was successful. Update lengths and stats */
            else
            {
                atomic_fetch_add(&(class->log_pl_len), 1);
                atomic_fetch_add(&(class->phys_pl_len), 1);

                done = TRUE;

                /* update stats */
                /* num_insert_prop_class_success */
            }
#endif
        }
        else if ( curr_prop->chksum == new_prop->chksum )
        {
            int32_t        cmp_result;
            H5P_mt_prop_t *next_prop;
            bool           str_cmp_done = FALSE;

            while ( ! str_cmp_done )
            { 
                /* update stats */
                /* num_insert_prop_class_chksum_cols */

                cmp_result = strcmp(curr_prop->name, new_prop->name);

                /* new_prop is less than curr_prop lexicographically */
                if ( cmp_result > 0 )
                {
                    /* prep new_prop to insert between prev_prop and curr_prop */
                    atomic_store(&(new_prop->next.ptr), curr_prop);

                    /** Attempt to atomically insert new_prop 
                     * 
                     * NOTE: If this fails, another thread modified the LFSLL and we must 
                     * update stats and restart to ensure new_prop is correctly inserted.
                    */
                    if ( !atomic_compare_exchange_strong(&(prev_prop->next.ptr), 
                                                         &curr_prop, new_prop) )
                    {
                        /* update stats */
                        /* num_insert_prop_class_cols */

                        break;

                    }
                    /* The attempt was successful. Update lengths and stats */
                    else 
                    {
                        atomic_fetch_add(&(class->log_pl_len), 1);
                        atomic_fetch_add(&(class->phys_pl_len), 1);

                        done = TRUE;
                        str_cmp_done = TRUE;

                        /* update stats */
                        /* num_insert_prop_class_success */
                    }
                }
                else if ( cmp_result < 0 )
                {

                    atomic_store(&(next_prop), curr_prop->next.ptr);

                    assert(next_prop->chksum >= curr_prop->chksum);

                    /** 
                     * If the next property in the LFSLL doesn't have the same chksum,
                     * then new_prop gets inserted between curr_prop and new_prop since
                     * chksum is used to sort first.
                     */
                    if ( next_prop->chksum != curr_prop->chksum )
                    {
                        /* Prep new_prop to be inserted between curr_prop and next_prop */
                        atomic_store(&(new_prop->next.ptr), next_prop);

                        /** Attempt to atomically insert new_prop 
                         * 
                         * NOTE: If this fails, another thread modified the LFSLL and we must 
                         * update stats and restart to ensure new_prop is correctly inserted.
                        */
                        if ( !atomic_compare_exchange_strong(&(curr_prop->next.ptr), 
                                                            &next_prop, new_prop) )
                        {
                            /* update stats */
                            /* num_insert_prop_class_success */

                            break;

                        }
                        /* The attempt was successful. Update lengths and stats */
                        else
                        {
                            atomic_fetch_add(&(class->log_pl_len), 1);
                            atomic_fetch_add(&(class->phys_pl_len), 1);

                            done = TRUE;
                            str_cmp_done = TRUE;

                            /* update stats */
                            /* num_insert_prop_class_success */
                        }
                    } /* end if ( next_prop->chksum != curr_prop->chksum ) */
  
                } 
                else /* cmp_results == 0 */
                {
                    /* update stats */
                    /* num_insert_prop_class_string_cols */

                    /**
                     * If the name's of curr_prop and new_prop are the same, we must
                     * move on to using version number to determine insert location.
                     */
                    if ( new_prop->create_version > curr_prop->create_version )
                    {
                        atomic_store(&(new_prop->next.ptr), curr_prop);

                        if ( !atomic_compare_exchange_strong(&(prev_prop->next.ptr), 
                                                             &curr_prop, new_prop) )
                        {
                            /* update stats */
                            /* num_insert_prop_class_cols */

                            break;
                        }
                        /* The attempt was successful. Update lengths and stats */
                        else 
                        {
                            atomic_fetch_add(&(class->log_pl_len), 1);
                            atomic_fetch_add(&(class->phys_pl_len), 1);

                            done = TRUE;
                            str_cmp_done = TRUE;

                            /* update stats */
                            /* num_insert_prop_class_success */
                        }
                        
                    } /* end if ( new_prop->create_version > curr_prop->create_version ) */
                    else
                    {
                        /* Property is already in the LFSLL, update stats and exit */
                        if ( new_prop->create_version == curr_prop->create_version )
                        {
                            /* update stats */
                            /* num_insert_prop_class_alread_in_LFSLL */
                            
                            done = TRUE;
                            str_cmp_done = TRUE;

                            break;
                        }
                        
                        atomic_store(&(next_prop), curr_prop->next.ptr);

                        assert(next_prop->chksum >= curr_prop->chksum);

                        if ( new_prop->chksum != next_prop->chksum )
                        {
                            /* Prep new_prop to be inserted between curr_prop and next_prop */
                            atomic_store(&(new_prop->next.ptr), next_prop);

                            /** Attempt to atomically insert new_prop 
                             * 
                             * NOTE: If this fails, another thread modified the LFSLL and we must 
                             * update stats and restart to ensure new_prop is correctly inserted.
                            */
                            if ( !atomic_compare_exchange_strong(&(curr_prop->next.ptr), 
                                                                &next_prop, new_prop) )
                            {
                                /* update stats */
                                /* num_insert_prop_class_success */

                                break;

                            }
                            /* The attempt was successful. Update lengths and stats */
                            else
                            {
                                atomic_fetch_add(&(class->log_pl_len), 1);
                                atomic_fetch_add(&(class->phys_pl_len), 1);

                                done = TRUE;
                                str_cmp_done = TRUE;

                                /* update stats */
                                /* num_insert_prop_class_success */
                            }
                        } /* end if ( new_prop->chksum != next_prop->chksum ) */

                    } /* end else */

                } /* end else ( cmp_results == 0 ) */ 

                /** 
                 * Must update prev_prop and curr_prop to compare next_prop
                 * with new_prop to find the correct insert location.
                 */
                atomic_store(&(prev_prop), curr_prop);
                atomic_store(&(curr_prop), next_prop);


            } /* end while ( ! str_cmp_done ) */
            

        } /* end else if ( curr_prop->chksum == new_prop->chksum ) */
    
    } /* end while (!done) */
}





#endif
