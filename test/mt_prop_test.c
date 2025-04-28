//#include "h5test.h"
//#include "H5private.h"

#include "../src/H5Pint_mt.c"
#include "../src/H5Ppkg_mt.h"

#include <stdio.h>
#include <stdatomic.h>

#define MAX_NAME_LEN 50

//void simple_test_1(void);
//void serial_class_test(void);
//void serial_list_test(void);

typedef struct prop_info_t
{
    int64_t       chksum;
    char        * name;
    void        * value_ptr;
    size_t        value_size;
    void        * edited_value_ptr;
    size_t        edited_value_size;

} prop_info_t;

prop_info_t * prop_info_tbl[10];



void init_prop_info_tbl(void);
H5P_mt_class_t * 
    H5P_test_init(void);
H5P_mt_class_t *
    H5P_mt_create_class_test(H5P_mt_class_t *parent, char *name, 
                             H5P_plist_type_t type, uint64_t test_version);
H5P_mt_list_t *
    H5P_mt_create_list_test(H5P_mt_class_t *parent, H5P_mt_list_t *old_list, bool copy,
                            uint64_t version);
herr_t
    compare_prop_to_prop_info(H5P_mt_prop_t * prop, bool in_prop_class, bool in_lkup_tbl,
                            uint64_t ref_count);
prop_info_t *
    search_prop_info_tbl(int64_t chksum);
void serial_class_test(void);
void serial_list_test(void);




/****************************************************************************************
 * Function:    init_prop_info_tbl()
 * 
 * Purpose:     Allocates and initiates the prop_info_tbl which is an array of 
 *              prop_info_t that contain information about the properties that are 
 *              created in the test functions to be able to compare them ensuring they 
 *              are created correctly and are sorted in the correct order in the the 
 *              lkup_tbls and lfslls 
 * 
 * Return:      void
 * 
 ****************************************************************************************
 */
void
init_prop_info_tbl(void)
{
    /* prop_1 */
    prop_info_tbl[0]                     = malloc(sizeof(prop_info_t)); 
    prop_info_tbl[0]->chksum             = 268839092;
    prop_info_tbl[0]->name               = strdup("prop_1");
    prop_info_tbl[0]->value_ptr          = strdup("value for prop_1");
    prop_info_tbl[0]->value_size         = strlen("value for prop_1");
    prop_info_tbl[0]->edited_value_ptr   = strdup("edited value for prop_1");
    prop_info_tbl[0]->edited_value_size  = strlen("edited value for prop_1");

    /* prop_5 */
    prop_info_tbl[1]                     = malloc(sizeof(prop_info_t)); 
    prop_info_tbl[1]->chksum             = 937277374;
    prop_info_tbl[1]->name               = strdup("prop_5");
    prop_info_tbl[1]->value_ptr          = strdup("value for prop_5");
    prop_info_tbl[1]->value_size         = strlen("value for prop_5");
    prop_info_tbl[1]->edited_value_ptr   = strdup("edited value for prop_5");
    prop_info_tbl[1]->edited_value_size  = strlen("edited value for prop_5"); 

    /* prop_6 */
    prop_info_tbl[2]                     = malloc(sizeof(prop_info_t)); 
    prop_info_tbl[2]->chksum             = 2009604789;
    prop_info_tbl[2]->name               = strdup("prop_6");
    prop_info_tbl[2]->value_ptr          = strdup("value for prop_6");
    prop_info_tbl[2]->value_size         = strlen("value for prop_6");
    prop_info_tbl[2]->edited_value_ptr   = strdup("edited value for prop_6");
    prop_info_tbl[2]->edited_value_size  = strlen("edited value for prop_6");

    /* prop_7 */
    prop_info_tbl[3]                     = malloc(sizeof(prop_info_t)); 
    prop_info_tbl[3]->chksum             = 2153701089;
    prop_info_tbl[3]->name               = strdup("prop_7");
    prop_info_tbl[3]->value_ptr          = strdup("value for prop_7");
    prop_info_tbl[3]->value_size         = strlen("value for prop_7");
    prop_info_tbl[3]->edited_value_ptr   = strdup("edited value for prop_7");
    prop_info_tbl[3]->edited_value_size  = strlen("edited value for prop_7");

    /* prop_9 */
    prop_info_tbl[4]                     = malloc(sizeof(prop_info_t)); 
    prop_info_tbl[4]->chksum             = 2283177890;
    prop_info_tbl[4]->name               = strdup("prop_9");
    prop_info_tbl[4]->value_ptr          = strdup("value for prop_9");
    prop_info_tbl[4]->value_size         = strlen("value for prop_9");
    prop_info_tbl[4]->edited_value_ptr   = strdup("edited value for prop_9");
    prop_info_tbl[4]->edited_value_size  = strlen("edited value for prop_9"); 

    /* prop_8 */
    prop_info_tbl[5]                     = malloc(sizeof(prop_info_t)); 
    prop_info_tbl[5]->chksum             = 2719365105;
    prop_info_tbl[5]->name               = strdup("prop_8");
    prop_info_tbl[5]->value_ptr          = strdup("value for prop_8");
    prop_info_tbl[5]->value_size         = strlen("value for prop_8");
    prop_info_tbl[5]->edited_value_ptr   = strdup("edited value for prop_8");
    prop_info_tbl[5]->edited_value_size  = strlen("edited value for prop_8"); 

    /* prop_0 */
    prop_info_tbl[6]                     = malloc(sizeof(prop_info_t));    
    prop_info_tbl[6]->chksum             = 2734785588;
    prop_info_tbl[6]->name               = strdup("prop_0");
    prop_info_tbl[6]->value_ptr          = strdup("value for prop_0");
    prop_info_tbl[6]->value_size         = strlen("value for prop_0");
    prop_info_tbl[6]->edited_value_ptr   = strdup("edited value for prop_0");
    prop_info_tbl[6]->edited_value_size  = strlen("edited value for prop_0");

    /* prop_2 */
    prop_info_tbl[7]                     = malloc(sizeof(prop_info_t)); 
    prop_info_tbl[7]->chksum             = 2899762636;
    prop_info_tbl[7]->name               = strdup("prop_2");
    prop_info_tbl[7]->value_ptr          = strdup("value for prop_2");
    prop_info_tbl[7]->value_size         = strlen("value for prop_2");
    prop_info_tbl[7]->edited_value_ptr   = strdup("edited value for prop_2");
    prop_info_tbl[7]->edited_value_size  = strlen("edited value for prop_2");

    /* prop_4 */
    prop_info_tbl[8]                     = malloc(sizeof(prop_info_t)); 
    prop_info_tbl[8]->chksum             = 2968727381;
    prop_info_tbl[8]->name               = strdup("prop_4");
    prop_info_tbl[8]->value_ptr          = strdup("value for prop_4");
    prop_info_tbl[8]->value_size         = strlen("value for prop_4");
    prop_info_tbl[8]->edited_value_ptr   = strdup("edited value for prop_4");
    prop_info_tbl[8]->edited_value_size  = strlen("edited value for prop_4");

    /* prop_10*/
    prop_info_tbl[9]                     = malloc(sizeof(prop_info_t)); 
    prop_info_tbl[9]->chksum             = 3204791218;
    prop_info_tbl[9]->name               = strdup("prop_10");
    prop_info_tbl[9]->value_ptr          = strdup("value for prop_10");
    prop_info_tbl[9]->value_size         = strlen("value for prop_10");
    prop_info_tbl[9]->edited_value_ptr   = strdup("edited value for prop_10");
    prop_info_tbl[9]->edited_value_size  = strlen("edited value for prop_10");

    /* prop_3 */
    prop_info_tbl[10]                     = malloc(sizeof(prop_info_t)); 
    prop_info_tbl[10]->chksum             = 3974747559;
    prop_info_tbl[10]->name               = strdup("prop_3");
    prop_info_tbl[10]->value_ptr          = strdup("value for prop_3");
    prop_info_tbl[10]->value_size         = strlen("value for prop_3");
    prop_info_tbl[10]->edited_value_ptr   = strdup("edited value for prop_3");
    prop_info_tbl[10]->edited_value_size  = strlen("edited value for prop_3");

    /* prop_11 */
    prop_info_tbl[11]                     = malloc(sizeof(prop_info_t)); 
    prop_info_tbl[11]->chksum             = 1966212493;
    prop_info_tbl[11]->name               = strdup("prop_11");
    prop_info_tbl[11]->value_ptr          = strdup("value for prop_11");
    prop_info_tbl[11]->value_size         = strlen("value for prop_11");
    prop_info_tbl[11]->edited_value_ptr   = strdup("edited value for prop_11");
    prop_info_tbl[11]->edited_value_size  = strlen("edited value for prop_11");

} /* init_prop_info_tbl() */



/****************************************************************************************
 * Function:    H5P_test_init()
 * 
 * Purpose:     Calls H5P_mt_init() function to set up the multi-thread root class and 
 *              the free lists for properties, lists, and classes. Then does many asserts
 *              to ensure all the values in the root class struct are correct.
 * 
 * Return:      Pointer to root class
 * 
 ****************************************************************************************
 */
H5P_mt_class_t *
H5P_test_init(void)
{
    H5P_mt_prop_t           * prop;
    H5P_mt_prop_aptr_t        next;
    H5P_mt_prop_value_t       value;
    H5P_mt_class_t          * root_class;
    H5P_mt_class_ref_counts_t ref_count;



    H5P_mt_init();

    root_class = H5P_MT_CLS_ROOT_g;

    assert(root_class);
    assert(root_class->tag == H5P_MT_CLASS_TAG);

    assert(root_class->parent_id == H5I_INVALID_HID);
    assert(root_class->parent_ptr == NULL);
    assert(root_class->parent_version == 0);

    assert( 0 == strcmp(root_class->name, "root"));
    assert( H5I_INVALID_HID == atomic_load(&(root_class->id)));
    assert( H5P_TYPE_ROOT == root_class->type);
        
    assert( 1 == atomic_load(&(root_class->curr_version)));
    assert( 2 == atomic_load(&(root_class->next_version))); 

    ref_count = atomic_load(&(root_class->ref_count));
    assert( 0 == ref_count.plc);
    assert( 0 == ref_count.pl);
    assert( FALSE == ref_count.deleted);
    assert( FALSE == ref_count.dummy_bool_1);
    assert( FALSE == ref_count.dummy_bool_2);
    assert( FALSE == ref_count.dummy_bool_3);

    prop = root_class->pl_head;
    value = atomic_load(&(prop->value));
    
    assert(prop);
    assert(prop->tag == H5P_MT_PROP_TAG);
    assert(prop->chksum == LLONG_MIN);
    assert(prop->sentinel);
    assert(value.size == 0);
    assert(value.ptr == NULL);
    
    next = atomic_load(&(prop->next));
    prop = next.ptr;
    value = atomic_load(&(prop->value));

    assert(prop);
    assert(prop->tag == H5P_MT_PROP_TAG);
    assert(prop->chksum == LLONG_MAX);
    assert(prop->sentinel);
    assert(value.size == 0);
    assert(value.ptr == NULL);

    assert(0 == atomic_load(&(root_class->log_pl_len)));
    assert(2 == atomic_load(&(root_class->phys_pl_len)));

    
    return(root_class);

} /* H5P_test_init() */



/****************************************************************************************
 * Function:    H5P_mt_create_class_test()
 * 
 * Purpose:     Calls H5P__mt_create_class() function and then uses asserts to ensure all 
 *              the basic values in the class are correct.
 * 
 * Return:      Success: pointer to the class was created
 * 
 *              Failure: NULL
 * 
 ****************************************************************************************
 */
H5P_mt_class_t *
H5P_mt_create_class_test(H5P_mt_class_t *parent, char *name, H5P_plist_type_t type,
                         uint64_t test_version)
{
    H5P_mt_class_t          * new_class;
    H5P_mt_class_ref_counts_t ref_count;

    new_class = H5P__mt_create_class(parent, name, type, test_version, NULL, NULL, 
                                     NULL, NULL, NULL, NULL);

    if ( ! new_class )
    {
        fprintf(stderr, "Failed to initialize the class.");
        assert(H5P_MT_ASSERT_FAIL);
    }

    assert(new_class);
    assert(new_class->tag == H5P_MT_CLASS_TAG);

    assert(new_class->parent_id == parent->id);
    assert(new_class->parent_ptr == parent);

    if (test_version == 0)
        assert(new_class->parent_version == (atomic_load(&(parent->curr_version))));
    else
        assert(new_class->parent_version == test_version);

    assert( 0 == strcmp(new_class->name, name));
    assert( H5I_INVALID_HID == atomic_load(&(new_class->id)));
    assert( type == new_class->type);
        
    assert( 1 == atomic_load(&(new_class->curr_version)));
    assert( 2 == atomic_load(&(new_class->next_version))); 

    ref_count = atomic_load(&(new_class->ref_count));
    assert( 0 == ref_count.plc);
    assert( 0 == ref_count.pl);
    assert( FALSE == ref_count.deleted);
    assert( FALSE == ref_count.dummy_bool_1);
    assert( FALSE == ref_count.dummy_bool_2);
    assert( FALSE == ref_count.dummy_bool_3);
    
    return(new_class);


} /* H5P_mt_create_class_test() */



/****************************************************************************************
 * Function:    H5P_mt_create_class_test()
 * 
 * Purpose:     Calls H5P__mt_create_list() function and then uses asserts to ensure all 
 *              the basic values in the list are correct.
 * 
 * Return:      Success: pointer to the list was created
 * 
 *              Failure: NULL
 * 
 ****************************************************************************************
 */
H5P_mt_list_t *
H5P_mt_create_list_test(H5P_mt_class_t *parent, H5P_mt_list_t *old_list, bool copy,
                        uint64_t version)
{
    H5P_mt_list_t          * new_list;

    new_list = H5P__mt_create_list(parent, old_list, copy, version);

    assert(new_list);
    assert(new_list->tag == H5P_MT_LIST_TAG);

    assert(new_list->pclass_id == parent->id);
    assert(new_list->pclass_ptr == parent);
    if ( version == 0 )
    {
        assert(new_list->pclass_version == parent->curr_version);
    }
    else
    {
        assert(new_list->pclass_version == version);
    }

    assert(atomic_load(&(new_list->plist_id)) == H5I_INVALID_HID);
    assert(atomic_load(&(new_list->curr_version)) == 1);
    assert(atomic_load(&(new_list->next_version)) == 2);

    assert(atomic_load(&(new_list->nprops_added)) == 0);
    assert(atomic_load(&(new_list->nprops)) == new_list->nprops_inherited);

    if ( version == 0 )
    {
        assert(new_list->nprops_inherited == parent->log_pl_len);
    }

    
    return(new_list);

} /* H5P_mt_create_list_test() */



/****************************************************************************************
 * Function:    compare_prop_to_prop_info()
 * 
 * Purpose:     Function that compares a property to the prop_info_t struct for that 
 *              property to ensure it's chksum, name, and value are correct
 * 
 * Return:      SUCCESS/FAIL
 * 
 ****************************************************************************************
 */
herr_t
compare_prop_to_prop_info(H5P_mt_prop_t * prop, bool in_prop_class, bool in_lkup_tbl,
                        uint64_t ref_count)
{
    H5P_mt_prop_value_t value;
    prop_info_t       * cmp_prop;

    herr_t               ret_value = SUCCEED;


    assert(prop);
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    
    assert( ! prop->sentinel);
    assert(prop->in_prop_class == in_prop_class);
    assert(prop->in_lkup_tbl == in_lkup_tbl);
    assert(atomic_load(&(prop->ref_count)) == ref_count);

    cmp_prop = search_prop_info_tbl(prop->chksum);
    
    assert(prop->chksum == cmp_prop->chksum);
    assert( 0 == strcmp(prop->name, cmp_prop->name));
    
    value = atomic_load(&(prop->value));

   
    
    if ( value.size == cmp_prop->value_size )
    {
        assert(0 == memcmp(value.ptr, cmp_prop->value_ptr, value.size));
    }
    else if ( value.size == cmp_prop->edited_value_size )
    {
        assert(0 == memcmp(value.ptr, cmp_prop->edited_value_ptr, value.size));
    }
    else
    {
        fprintf(stderr, "value doesn't match prop_info_tbl.");
        assert(H5P_MT_ASSERT_FAIL);
    }
    

    return(ret_value);

} /* compare_prop_to_prop_info() */



/****************************************************************************************
 * Function:    search_prop_info_tbl()
 * 
 * Purpose:     Searches the prop_info_tbl to get the entry with the supplied chksum
 * 
 * Return:      Success: pointer to the prop_info_t for the supplied chksum
 * 
 *              Failure: can't fail
 * 
 ****************************************************************************************
 */
prop_info_t *
search_prop_info_tbl(int64_t chksum)
{
    prop_info_t * prop_info;
    int32_t       left;
    int32_t       middle;
    int32_t       right;

    prop_info_t * ret_value = NULL;

    left  = 0;
    right = 11; /* size of prop_info_tbl array */

    while ( left <= right )
    {
        middle = left + (right - left) / 2;

        prop_info = prop_info_tbl[middle];

        if ( prop_info->chksum == chksum )
        {
            ret_value = prop_info;
            break;
        }
        else if ( prop_info->chksum < chksum )
        {
            left = middle + 1;
        }
        else
        {
            right = middle -1;
        }
    }
    
    return(ret_value);

} /* search_prop_info_tbl() */



/****************************************************************************************
 * Function:    serial_class_test()
 * 
 * Purpose:     Goes through the H5Pint_mt.c class functions and performs checks to 
 *              ensure all of the classes and their properties are correct after creating
 *              a class, inserting a new property, 'modifying' an existing property, and
 *              setting the delete version on a property. We also test searching for a 
 *              property, comparing classes that are the same and that are different,
 *              and closing and clearing classes.
 * 
 * Return:      void
 * 
 ****************************************************************************************
 */
void serial_class_test(void)
{
    H5P_mt_class_t    * root_class;
    H5P_mt_class_t    * att_class;
    H5P_mt_class_t    * group_class;
    H5P_mt_class_t    * data_class;
    char              * class_name;
    H5P_mt_prop_t     * prop;
    char                tmp_name[MAX_NAME_LEN];
    char              * prop_name;
    H5P_mt_prop_aptr_t  next;
    H5P_mt_prop_value_t value;
    prop_info_t       * prop_info;
    size_t              name_len;
    int64_t             chksum;



    printf("\nMT PROP serial class test\n");

    /* Call the test function for initializing the MT H5P and getting the root class */
    root_class = H5P_test_init();


    /** 
     * First testing creating a new class from the root class 
     */

    class_name = malloc(MAX_NAME_LEN);

    snprintf(class_name, MAX_NAME_LEN, "attribute_class");

    att_class = H5P_mt_create_class_test(root_class, class_name, 
                                         H5P_TYPE_ATTRIBUTE_ACCESS, 0);

    prop = att_class->pl_head;

    assert(prop);
    assert(prop->tag == H5P_MT_PROP_TAG);
    assert(prop->sentinel);
    
    next = atomic_load(&(prop->next));
    prop = next.ptr;

    assert(prop);
    assert(prop->tag == H5P_MT_PROP_TAG);
    assert(prop->sentinel);

    assert(0 == atomic_load(&(att_class->log_pl_len)));
    assert(2 == atomic_load(&(att_class->phys_pl_len)));


    /** 
     * Test adding a few properties to the attribute class 
     */
    
    for ( uint32_t i = 0; i < 11; i++ )
    {
        snprintf(tmp_name, sizeof("prop_%d"), "prop_%d", i);

        prop_name = strdup(tmp_name);

        name_len = strlen(prop_name);

        chksum = H5_checksum_metadata(prop_name, name_len, 0);

        prop_info = search_prop_info_tbl(chksum);

        H5P__mt_ins_or_mod_prop__main(att_class, prop_info->name, prop_info->value_ptr, 
                                      prop_info->value_size, FALSE, FALSE, NULL, NULL, 
                                      NULL, NULL, NULL, NULL, NULL, NULL, NULL);

        prop = H5P__mt_search_prop(att_class, chksum, prop_name);

        assert(atomic_load(&(prop->create_version)) == ( i + 2 ) );

        free(prop_name);

    } /* end for() */

    assert(11 == atomic_load(&(att_class->log_pl_len)));
    assert(13 == atomic_load(&(att_class->phys_pl_len)));



    /**
     * Iterate the lfsll to ensure the properties were created correctly and
     * are sorted correctly.
     */

    prop = att_class->pl_head;

    for ( uint32_t i = 0; i < atomic_load(&(att_class->phys_pl_len)); i++ )
    {
        if ( i == 0 )
        {
            assert(prop->chksum == LLONG_MIN);
            assert(prop->sentinel);
        }
        else if ( i > 0 && i < (atomic_load(&(att_class->phys_pl_len)) - 1 ) )
        {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert( ! atomic_load(&(prop->sentinel)));
            assert(prop->in_prop_class);
            assert(prop->chksum == prop_info_tbl[i-1]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i-1]->name));
            
            value = atomic_load(&(prop->value));

            //printf("value.size: %ld\ntbl_value_size: %ld\n\n", value.size, prop_info_tbl[i-1]->value_size);

            assert(value.size == prop_info_tbl[i-1]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i-1]->value_ptr, value.size));
            //assert(1 == prop->create_version);
            //assert(0 == prop->delete_version);
            
        }        
        else
        {
            assert(prop->chksum == LLONG_MAX);
            assert(prop->sentinel);
        }

        next = atomic_load(&(prop->next));
        prop = next.ptr;

    } /* end for() */


    /** 
     * Test creating two properties that are "modified" prop_0 and prop_5 
     */

    /* New version of prop_5 */
    H5P__mt_ins_or_mod_prop__main(att_class, prop_info_tbl[1]->name, 
                                  prop_info_tbl[1]->edited_value_ptr, 
                                  prop_info_tbl[1]->edited_value_size,
                                  FALSE, FALSE, NULL, NULL, NULL, NULL,
                                  NULL, NULL, NULL, NULL, NULL);

    /* New version of prop_0 */
    H5P__mt_ins_or_mod_prop__main(att_class, prop_info_tbl[6]->name,
                                 prop_info_tbl[6]->edited_value_ptr,
                                 prop_info_tbl[6]->edited_value_size,
                                 FALSE, FALSE, NULL, NULL, NULL, NULL,
                                 NULL, NULL, NULL, NULL, NULL);

    
    assert(11 == atomic_load(&(att_class->log_pl_len)));
    assert(15 == atomic_load(&(att_class->phys_pl_len)));


    /**
     * Iterate the lfsll again and ensure the new properties are correct
     * and all props are stil in the correct order.
     */
    prop = att_class->pl_head;

    for ( uint32_t i = 0; i < atomic_load(&(att_class->phys_pl_len)); i++ )
    {
        if ( i == 0 )
        {
            assert(prop->chksum == LLONG_MIN);
            assert(prop->sentinel);
        }
        else if ( i == 1 )
        {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert( ! atomic_load(&(prop->sentinel)));
            assert(prop->in_prop_class);
            assert(prop->chksum == prop_info_tbl[i-1]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i-1]->name));
            
            value = atomic_load(&(prop->value));

            //printf(prop->name, "\n");
            //printf("value.size: %ld\ntbl_value_size: %ld\n\n", value.size, 
            //       prop_info_tbl[i-1]->value_size);

            assert(value.size == prop_info_tbl[i-1]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i-1]->value_ptr, value.size)); 
        }
        else if ( i == 2 )
        {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert( ! atomic_load(&(prop->sentinel)));
            assert(prop->in_prop_class);
            assert(prop->chksum == prop_info_tbl[i-1]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i-1]->name));
            
            value = atomic_load(&(prop->value));

            assert(value.size == prop_info_tbl[i-1]->edited_value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i-1]->edited_value_ptr, 
                               value.size));
        }
        else if ( i >= 3 && i < 8 )
        {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert( ! atomic_load(&(prop->sentinel)));
            assert(prop->in_prop_class);
            assert(prop->chksum == prop_info_tbl[i-2]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i-2]->name));
            
            value = atomic_load(&(prop->value));

            assert(value.size == prop_info_tbl[i-2]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i-2]->value_ptr, value.size));
        }
        else if ( i == 8 )
        {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert( ! atomic_load(&(prop->sentinel)));
            assert(prop->in_prop_class);
            assert(prop->chksum == prop_info_tbl[i-2]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i-2]->name));
            
            value = atomic_load(&(prop->value));

            assert(value.size == prop_info_tbl[i-2]->edited_value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i-2]->edited_value_ptr, 
                               value.size));
        }
        else if ( i > 8 && i < (atomic_load(&(att_class->phys_pl_len)) - 1 ) )
        {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert( ! atomic_load(&(prop->sentinel)));
            assert(prop->in_prop_class);
            assert(prop->chksum == prop_info_tbl[i-3]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i-3]->name));
            
            value = atomic_load(&(prop->value));

            assert(value.size == prop_info_tbl[i-3]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i-3]->value_ptr, value.size));
        }      
        else
        {
            assert(prop->chksum == LLONG_MAX);
            assert(prop->sentinel);
        }

        next = atomic_load(&(prop->next));
        prop = next.ptr;

    } /* end for() */


    /**
     * Test setting the delete version on property prop_4.
     */

    prop = H5P__mt_search_prop(att_class, prop_info_tbl[8]->chksum, 
                               prop_info_tbl[8]->name);


    if ( compare_prop_to_prop_info(prop, TRUE, FALSE, 0) < 0 )
    {
        fprintf(stderr, "property doesn't match with prop_info_tbl.");
        assert(H5P_MT_ASSERT_FAIL);
    }

    if ( H5P__set_delete_version(att_class, prop) < 0 )
    {
        fprintf(stderr, "Failed to set delete version on property.");
        assert(H5P_MT_ASSERT_FAIL);
    }

    assert(15 == atomic_load(&(prop->delete_version)));

    
    /**
     * Test creating a new class derived from att_class
     */

    snprintf(class_name, MAX_NAME_LEN, "group_class");

    group_class = H5P_mt_create_class_test(att_class, class_name,
                                           H5P_TYPE_GROUP_ACCESS, 0);

    assert(10 == atomic_load(&(group_class->log_pl_len)));
    assert(12 == atomic_load(&(group_class->phys_pl_len)));


    /**
     * Iterate the lfsll and ensure the properties are in the correct order
     * and that any non-valid properties didn't get copied over.
     */
    prop = group_class->pl_head;

    for ( uint32_t i = 0; i < atomic_load(&(group_class->phys_pl_len)); i++ )
    {
        if ( i == 0 )
        {
            assert(prop->chksum == LLONG_MIN);
            assert(prop->sentinel);
        }
        else if ( i == 1 || ( i >= 3 && i < 7 ) || i == 8 )
        {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert( ! atomic_load(&(prop->sentinel)));
            assert(prop->in_prop_class);
            assert(prop->chksum == prop_info_tbl[i-1]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i-1]->name));
            
            value = atomic_load(&(prop->value));

            //printf(prop->name, "\n");
            //printf("value.size: %ld\ntbl_value_size: %ld\n\n", value.size, 
            //       prop_info_tbl[i-1]->value_size);

            assert(value.size == prop_info_tbl[i-1]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i-1]->value_ptr, value.size)); 
        }
        else if ( i == 2 || i == 7 )
        {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert( ! atomic_load(&(prop->sentinel)));
            assert(prop->in_prop_class);
            assert(prop->chksum == prop_info_tbl[i-1]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i-1]->name));
            
            value = atomic_load(&(prop->value));

            assert(value.size == prop_info_tbl[i-1]->edited_value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i-1]->edited_value_ptr, 
                                value.size));
        }
        else if ( ( i > 8 && i < (atomic_load(&(group_class->phys_pl_len)) - 1 ) ) )
        {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert( ! atomic_load(&(prop->sentinel)));
            assert(prop->in_prop_class);
            assert(prop->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i]->name));
            
            value = atomic_load(&(prop->value));

            assert(value.size == prop_info_tbl[i]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i]->value_ptr, value.size));
        }     
        else
        {
            assert(prop->chksum == LLONG_MAX);
            assert(prop->sentinel);
        }

        next = atomic_load(&(prop->next));
        prop = next.ptr;

    } /* end for() */



    /**
     * Test creating a class derived from att_class at version 14, meaning it should
     * contain prop_4 since version 14 was before it was deleted.
     */
    snprintf(class_name, MAX_NAME_LEN, "datatype_class");

    data_class = H5P_mt_create_class_test(att_class, class_name,
                                           H5P_TYPE_DATATYPE_ACCESS, 14);

    assert(11 == atomic_load(&(data_class->log_pl_len)));
    assert(13 == atomic_load(&(data_class->phys_pl_len)));

    /**
     * Iterate the lfsll and ensure the properties are in the correct order
     */
    prop = data_class->pl_head;

    for ( uint32_t i = 0; i < atomic_load(&(data_class->phys_pl_len)); i++ )
    {
        if ( i == 0 )
        {
            assert(prop->chksum == LLONG_MIN);
            assert(prop->sentinel);
        }
        else if ( i == 1 || ( i >= 3 && i < 7 ) || 
                  ( i >= 8 && i < (atomic_load(&(data_class->phys_pl_len)) - 1 ) ) )
        {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert( ! atomic_load(&(prop->sentinel)));
            assert(prop->in_prop_class);
            assert(prop->chksum == prop_info_tbl[i-1]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i-1]->name));
            
            value = atomic_load(&(prop->value));

            //printf(prop->name, "\n");
            //printf("value.size: %ld\ntbl_value_size: %ld\n\n", value.size, 
            //       prop_info_tbl[i-1]->value_size);

            assert(value.size == prop_info_tbl[i-1]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i-1]->value_ptr, value.size)); 
        }
        else if ( i == 2 || i == 7 )
        {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert( ! atomic_load(&(prop->sentinel)));
            assert(prop->in_prop_class);
            assert(prop->chksum == prop_info_tbl[i-1]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i-1]->name));
            
            value = atomic_load(&(prop->value));

            assert(value.size == prop_info_tbl[i-1]->edited_value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i-1]->edited_value_ptr, 
                                value.size));
        }
        else if ( ( i > 8 && i < (atomic_load(&(data_class->phys_pl_len)) - 1 ) ) )
        {
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert( ! atomic_load(&(prop->sentinel)));
            assert(prop->in_prop_class);
            assert(prop->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i]->name));
            
            value = atomic_load(&(prop->value));

            assert(value.size == prop_info_tbl[i]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i]->value_ptr, value.size));
        }     
        else
        {
            assert(prop->chksum == LLONG_MAX);
            assert(prop->sentinel);
        }

        next = atomic_load(&(prop->next));
        prop = next.ptr;

    } /* end for() */



    /**
     * Test comparing two classes to see if they are equal
     */

    /* Should be equal */
    assert(0 == H5P__mt_cmp_list_or_class(att_class, att_class));

    /* Should not be equal */
    assert(1 == H5P__mt_cmp_list_or_class(att_class, group_class));



    /**
     * Testing closing the class structures, which puts them on the class free list.
     * H5P__shutdown() then tests the H5P__mt_clear_class() function and frees all of
     * the allocated memory in those structures and the structures themselves.
     */
    H5P__mt_close_class(data_class);
    H5P__mt_close_class(group_class);
    H5P__mt_close_class(att_class);
    H5P__mt_close_class(root_class);
    
    H5P__shutdown();


} /* serial_class_test() */



/****************************************************************************************
 * Function:    serial_list_test()
 * 
 * Purpose:     Goes through the H5Pint_mt.c list functions and performs checks to 
 *              ensure all of the lists and their properties are correct after creating
 *              a list, inserting a new property, 'modifying' an existing property, and
 *              setting the delete version on a property. We also test searching for a 
 *              property, copying a list from another list, comparing lists that are the
 *              same and that are different, and closing and clearing lists.
 * 
 *              This function starts by creating an attribute class (att_class) to create
 *              lists from. 
 * 
 * Return:      void
 * 
 ****************************************************************************************
 */
void serial_list_test(void)
{
    H5P_mt_class_t            * root_class;
    H5P_mt_class_t            * att_class;
    //H5P_mt_class_t            * group_class;
    //H5P_mt_class_t            * data_class;
    char                      * class_name;
    H5P_mt_list_t             * list1;
    H5P_mt_list_t             * list2;
    H5P_mt_list_t             * list1_copy;
    H5P_mt_list_table_entry_t * entry;
    H5P_mt_list_prop_ref_t      base;
    H5P_mt_list_prop_ref_t      curr;
    H5P_mt_prop_t             * prop;
    char                        tmp_name[MAX_NAME_LEN];
    char                      * prop_name;
    H5P_mt_prop_aptr_t          next;
    H5P_mt_prop_value_t         value;
    prop_info_t               * prop_info;
    size_t                      name_len;
    int64_t                     chksum;


    printf("\nMT PROP serial list test\n");

    /* Call the test function for initializing the MT H5P and getting the root class */
    root_class = H5P_test_init();


    /** 
     * First create the classes to derive the lists from. 
     */

     class_name = malloc(MAX_NAME_LEN);

     snprintf(class_name, MAX_NAME_LEN, "attribute_class");
 
     att_class = H5P_mt_create_class_test(root_class, class_name, 
                                          H5P_TYPE_ATTRIBUTE_ACCESS, 0);

    /* Add properties to att_class */
    for ( uint32_t i = 0; i < 11; i++ )
    {
        snprintf(tmp_name, sizeof("prop_%d"), "prop_%d", i);

        prop_name = strdup(tmp_name);

        name_len = strlen(prop_name);

        chksum = H5_checksum_metadata(prop_name, name_len, 0);

        prop_info = search_prop_info_tbl(chksum);

        H5P__mt_ins_or_mod_prop__main(att_class, prop_info->name, prop_info->value_ptr, 
                                      prop_info->value_size, FALSE, FALSE, NULL, NULL, 
                                      NULL, NULL, NULL, NULL, NULL, NULL, NULL);

        prop = H5P__mt_search_prop(att_class, chksum, prop_name);

        assert(atomic_load(&(prop->create_version)) == ( i + 2 ) );

        free(prop_name);

    } /* end for() */


    /* New version of prop_5 */
    H5P__mt_ins_or_mod_prop__main(att_class, prop_info_tbl[1]->name, 
        prop_info_tbl[1]->edited_value_ptr, 
        prop_info_tbl[1]->edited_value_size,
        FALSE, FALSE, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL);

    /* New version of prop_0 */
    H5P__mt_ins_or_mod_prop__main(att_class, prop_info_tbl[6]->name,
            prop_info_tbl[6]->edited_value_ptr,
            prop_info_tbl[6]->edited_value_size,
            FALSE, FALSE, NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL, NULL);

    prop = H5P__mt_search_prop(att_class, prop_info_tbl[8]->chksum, 
                               prop_info_tbl[8]->name);

    /* deleting prop_4 */
    if ( H5P__set_delete_version(att_class, prop) < 0 )
    {
        fprintf(stderr, "Failed to set delete version on property.");
        assert(H5P_MT_ASSERT_FAIL);
    }




    /**
     * Test deriving a list from att_class
     */

    list1 = H5P_mt_create_list_test(att_class, NULL, FALSE, 0);

    assert(list1);

    assert(list1->nprops_inherited == 10);
    assert(list1->log_pl_len == 0);
    assert(list1->phys_pl_len == 2);

    for ( uint32_t i = 0; i < list1->nprops_inherited; i++ )
    {
        entry = &list1->lkup_tbl[i];

        if ( i == 0 || ( i >= 2 && i < 6 ) || i == 7 )
        {
            assert(entry->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(entry->name, prop_info_tbl[i]->name));
            assert(0 == atomic_load(&(entry->base_delete_version)));
    
            base = atomic_load(&(entry->base));
    
            assert(base.ver == 1);
            
            prop = base.ptr;
    
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert(! atomic_load(&(prop->sentinel)));
            assert(prop->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i]->name));
            assert(prop->in_prop_class);
            assert(1 == atomic_load(&(prop->ref_count)));
    
            value = atomic_load(&(prop->value));
    
            assert(value.size == prop_info_tbl[i]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i]->value_ptr, value.size));
        }
        else if ( i == 1 || i == 6 )
        {
            assert(entry->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(entry->name, prop_info_tbl[i]->name));
            assert(0 == atomic_load(&(entry->base_delete_version)));
    
            base = atomic_load(&(entry->base));
    
            assert(base.ver == 1);
            
            prop = base.ptr;
    
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert(! atomic_load(&(prop->sentinel)));
            assert(prop->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i]->name));
            assert(prop->in_prop_class);
            assert(1 == atomic_load(&(prop->ref_count)));
    
            value = atomic_load(&(prop->value));
    
            assert(value.size == prop_info_tbl[i]->edited_value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i]->edited_value_ptr, 
                               value.size));
        }
        else if ( i > 7 )
        {
            assert(entry->chksum == prop_info_tbl[i+1]->chksum);
            assert(0 == strcmp(entry->name, prop_info_tbl[i+1]->name));
            assert(0 == atomic_load(&(entry->base_delete_version)));
    
            base = atomic_load(&(entry->base));
    
            assert(base.ver == 1);
            
            prop = base.ptr;
    
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert(! atomic_load(&(prop->sentinel)));
            assert(prop->chksum == prop_info_tbl[i+1]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i+1]->name));
            assert(prop->in_prop_class);
            assert(1 == atomic_load(&(prop->ref_count)));
    
            value = atomic_load(&(prop->value));
    
            assert(value.size == prop_info_tbl[i+1]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i+1]->value_ptr, value.size));
        }

    } /* end for() */


    /**
     * Test deriving a list from att_class at version 14, meaning it should
     * contain prop_4 since version 14 was before it was deleted.
     */
    list2 = H5P_mt_create_list_test(att_class, NULL, FALSE, 14);

    assert(list2);

    assert(list2->nprops_inherited == 11);
    assert(list2->log_pl_len == 0);
    assert(list2->phys_pl_len == 2);


    for ( uint32_t i = 0; i < list2->nprops_inherited; i++ )
    {
        entry = &list2->lkup_tbl[i];

        if ( i == 0 || ( i >= 2 && i < 6 ) || i >= 7 )
        {
            assert(entry->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(entry->name, prop_info_tbl[i]->name));
            assert(0 == atomic_load(&(entry->base_delete_version)));
    
            base = atomic_load(&(entry->base));
    
            assert(base.ver == 1);
            
            prop = base.ptr;
    
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert(! atomic_load(&(prop->sentinel)));
            assert(prop->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i]->name));
            assert(prop->in_prop_class);
            
            /* If prop_4 ref_count should only be 1 because it isn't in list1 */
            if ( prop->chksum == 2968727381 )
            {
                assert(1 == atomic_load(&(prop->ref_count)));
            }
            else 
            {
                assert(2 == atomic_load(&(prop->ref_count)));
            }
    
            value = atomic_load(&(prop->value));
    
            assert(value.size == prop_info_tbl[i]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i]->value_ptr, value.size));
        }
        else if ( i == 1 || i == 6 )
        {
            assert(entry->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(entry->name, prop_info_tbl[i]->name));
            assert(0 == atomic_load(&(entry->base_delete_version)));
    
            base = atomic_load(&(entry->base));
    
            assert(base.ver == 1);
            
            prop = base.ptr;
    
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert(! atomic_load(&(prop->sentinel)));
            assert(prop->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i]->name));
            assert(prop->in_prop_class);
    
            value = atomic_load(&(prop->value));
    
            assert(value.size == prop_info_tbl[i]->edited_value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i]->edited_value_ptr, 
                               value.size));
        }

    } /* end for() */



    /**
     * Test adding a new property to list1
     */
    H5P__mt_ins_or_mod_prop__main(list1, prop_info_tbl[11]->name, 
                                 prop_info_tbl[11]->value_ptr,
                                 prop_info_tbl[11]->value_size,
                                 FALSE, FALSE, NULL, NULL, NULL, NULL,
                                 NULL, NULL, NULL, NULL, NULL);

    assert(11 == atomic_load(&(list1->nprops)));
    assert(1 == atomic_load(&(list1->nprops_added)));
    assert(1 == atomic_load(&(list1->log_pl_len)));
    assert(3 == atomic_load(&(list1->phys_pl_len)));
                            
    prop = list1->pl_head;
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(prop->sentinel);
    assert(prop->chksum == LLONG_MIN);

    next = atomic_load(&(prop->next));
    prop = next.ptr;

    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(! prop->sentinel);
    assert(! prop->in_prop_class);
    assert(0 == atomic_load(&(prop->ref_count)));
    assert(! prop->in_lkup_tbl);

    assert(prop->chksum == prop_info_tbl[11]->chksum);
    assert(0 == strcmp(prop->name, prop_info_tbl[11]->name));

    value = atomic_load(&(prop->value));

    assert(value.size == prop_info_tbl[11]->value_size);
    assert(0 == memcmp(value.ptr, prop_info_tbl[11]->value_ptr, 
                       value.size));

    next = atomic_load(&(prop->next));
    prop = next.ptr;

    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(prop->sentinel);
    assert(prop->chksum == LLONG_MAX);



    /**
     * Test adding a new version of prop_6 an existing property to list1
     * and test setting the delete_version on the prop_11 in the lfsll 
     * and prop_7 in the lkup_tbl.
     */
    H5P__mt_ins_or_mod_prop__main(list1, prop_info_tbl[2]->name, 
                                 prop_info_tbl[2]->edited_value_ptr,
                                 prop_info_tbl[2]->edited_value_size,
                                 FALSE, FALSE, NULL, NULL, NULL, NULL,
                                 NULL, NULL, NULL, NULL, NULL);

    assert(12 == atomic_load(&(list1->nprops)));
    assert(2 == atomic_load(&(list1->nprops_added)));
    assert(2 == atomic_load(&(list1->log_pl_len)));
    assert(4 == atomic_load(&(list1->phys_pl_len)));

    prop = H5P__mt_search_prop(list1, prop_info_tbl[11]->chksum, prop_info_tbl[11]->name);

    H5P__set_delete_version(list1, prop);

    prop = H5P__mt_search_prop(list1, prop_info_tbl[3]->chksum, prop_info_tbl[3]->name);

    H5P__set_delete_version(list1, prop);

    assert(11 == atomic_load(&(list1->nprops)));
    assert(2 == atomic_load(&(list1->nprops_added)));
    assert(1 == atomic_load(&(list1->log_pl_len)));
    assert(4 == atomic_load(&(list1->phys_pl_len)));


    for ( uint32_t i = 0; i < 4; i++ )
    {
        if ( i == 2 )
        {
            entry = &list1->lkup_tbl[i];

            assert(entry->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(entry->name, prop_info_tbl[i]->name));
            assert(0 == atomic_load(&(entry->base_delete_version)));

            curr = atomic_load(&(entry->curr));

            assert(curr.ptr);

            prop = curr.ptr;

            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert(! prop->sentinel);
            assert(! prop->in_prop_class);
            assert(0 == atomic_load(&(prop->ref_count)));
            assert(prop->in_lkup_tbl);

            assert(prop->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i]->name));
            
            value = atomic_load(&(prop->value));

            assert(value.size == prop_info_tbl[i]->edited_value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i]->edited_value_ptr, 
                               value.size));
        }
        else if ( i == 3 )
        {
            entry = &list1->lkup_tbl[i];

            assert(entry->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(entry->name, prop_info_tbl[i]->name));
            assert(5 == atomic_load(&(entry->base_delete_version)));
        }

    } /* end for () */


                            
    prop = list1->pl_head;

    next = atomic_load(&(prop->next));
    prop = next.ptr;

    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(! prop->sentinel);
    assert(! prop->in_prop_class);
    assert(0 == atomic_load(&(prop->ref_count)));
    assert(! prop->in_lkup_tbl);

    assert(prop->chksum == prop_info_tbl[11]->chksum);
    assert(0 == strcmp(prop->name, prop_info_tbl[11]->name));

    value = atomic_load(&(prop->value));

    assert(value.size == prop_info_tbl[11]->value_size);
    assert(0 == memcmp(value.ptr, prop_info_tbl[11]->value_ptr, 
                       value.size));
    assert(4 == atomic_load(&(prop->delete_version)));

    next = atomic_load(&(prop->next));
    prop = next.ptr;

    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(! prop->sentinel);
    assert(! prop->in_prop_class);
    assert(0 == atomic_load(&(prop->ref_count)));
    assert(prop->in_lkup_tbl);

    assert(prop->chksum == prop_info_tbl[2]->chksum);
    assert(0 == strcmp(prop->name, prop_info_tbl[2]->name));

    value = atomic_load(&(prop->value));

    assert(value.size == prop_info_tbl[2]->edited_value_size);
    assert(0 == memcmp(value.ptr, prop_info_tbl[2]->edited_value_ptr, 
                       value.size));




    /**
     * Test creating a copy of list1.
     */
    list1_copy = H5P__mt_create_list(att_class, list1, TRUE, 0);

    assert(list1_copy);

    assert(10 == list1_copy->nprops_inherited);
    assert(11 == atomic_load(&(list1_copy->nprops)));
    assert(2 == atomic_load(&(list1_copy->nprops_added)));
    assert(1 == atomic_load(&(list1_copy->log_pl_len)));
    assert(3 == atomic_load(&(list1_copy->phys_pl_len)));

    for ( uint32_t i = 0; i < list1_copy->nprops_inherited; i++ )
    {
        entry = &list1_copy->lkup_tbl[i];

        /* entry's that haven't been deleted or modifed */
        if ( i == 0 || ( i > 3 && i < 6 ) || i == 7 )
        {
            assert(entry->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(entry->name, prop_info_tbl[i]->name));
            assert(0 == atomic_load(&(entry->base_delete_version)));
    
            base = atomic_load(&(entry->base));
    
            assert(base.ver == 1);
            
            prop = base.ptr;
    
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert(! atomic_load(&(prop->sentinel)));
            assert(prop->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i]->name));
            assert(prop->in_prop_class);
            assert(3 == atomic_load(&(prop->ref_count)));
    
            value = atomic_load(&(prop->value));
    
            assert(value.size == prop_info_tbl[i]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i]->value_ptr, value.size));
        }
        /* entries that were modified in the att_class */
        else if ( i == 1 || i == 6 )
        {
            assert(entry->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(entry->name, prop_info_tbl[i]->name));
            assert(0 == atomic_load(&(entry->base_delete_version)));
    
            base = atomic_load(&(entry->base));
    
            assert(base.ver == 1);
            
            prop = base.ptr;
    
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert(! atomic_load(&(prop->sentinel)));
            assert(prop->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i]->name));
            assert(prop->in_prop_class);
            assert(3 == atomic_load(&(prop->ref_count)));
    
            value = atomic_load(&(prop->value));
    
            assert(value.size == prop_info_tbl[i]->edited_value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i]->edited_value_ptr, 
                               value.size));
        }
        /* entry that was edited in list1 prior to copying it */
        else if ( i == 2 )
        {
            assert(entry->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(entry->name, prop_info_tbl[i]->name));
            assert(0 == atomic_load(&(entry->base_delete_version)));
    
            base = atomic_load(&(entry->base));
    
            assert(base.ver == 1);
            
            prop = base.ptr;
    
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert(! atomic_load(&(prop->sentinel)));
            assert(prop->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i]->name));
            assert(prop->in_prop_class);
            assert(3 == atomic_load(&(prop->ref_count)));

            value = atomic_load(&(prop->value));
    
            assert(value.size == prop_info_tbl[i]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i]->value_ptr, 
                               value.size));

            curr = atomic_load(&(entry->curr));
    
            assert(curr.ver == 1);
            
            prop = curr.ptr;
    
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert(! atomic_load(&(prop->sentinel)));
            assert(prop->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i]->name));
            assert( ! prop->in_prop_class);
            assert(0 == atomic_load(&(prop->ref_count)));
    
            value = atomic_load(&(prop->value));
    
            assert(value.size == prop_info_tbl[i]->edited_value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i]->edited_value_ptr, 
                               value.size));
        }
        /* entry that was deleted from the lkup_tbl is list 1 prior to copying it */
        else if ( i == 3 )
        {
            assert(entry->chksum == prop_info_tbl[i]->chksum);
            assert(0 == strcmp(entry->name, prop_info_tbl[i]->name));
            assert(1 == atomic_load(&(entry->base_delete_version)));
    
            base = atomic_load(&(entry->base));
    
            /**
             * The base_delete_version was set when the list was copied, 
             * so the copy set it to 0 and set base.ptr to NULL.
             */
            assert(base.ver == 0);
            
            prop = base.ptr;
            
            /* */
            assert(prop == NULL);
        }
        else if ( i > 7 )
        {
            assert(entry->chksum == prop_info_tbl[i+1]->chksum);
            assert(0 == strcmp(entry->name, prop_info_tbl[i+1]->name));
            assert(0 == atomic_load(&(entry->base_delete_version)));
    
            base = atomic_load(&(entry->base));
    
            assert(base.ver == 1);
            
            prop = base.ptr;
    
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
            assert(! atomic_load(&(prop->sentinel)));
            assert(prop->chksum == prop_info_tbl[i+1]->chksum);
            assert(0 == strcmp(prop->name, prop_info_tbl[i+1]->name));
            assert(prop->in_prop_class);
            assert(3 == atomic_load(&(prop->ref_count)));
    
            value = atomic_load(&(prop->value));
    
            assert(value.size == prop_info_tbl[i+1]->value_size);
            assert(0 == memcmp(value.ptr, prop_info_tbl[i+1]->value_ptr, value.size));
        }

    } /* end for () */

    prop = list1_copy->pl_head;
    
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(prop->sentinel);
    assert(prop->chksum == LLONG_MIN);

    next = atomic_load(&(prop->next));
    prop = next.ptr;

    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);
    assert(! prop->sentinel);
    assert(! prop->in_prop_class);
    assert(0 == atomic_load(&(prop->ref_count)));
    assert(prop->in_lkup_tbl);

    assert(prop->chksum == prop_info_tbl[2]->chksum);
    assert(0 == strcmp(prop->name, prop_info_tbl[2]->name));

    value = atomic_load(&(prop->value));

    assert(value.size == prop_info_tbl[2]->edited_value_size);
    assert(0 == memcmp(value.ptr, prop_info_tbl[2]->edited_value_ptr, 
                       value.size));



    /**
     * Test comparing two classes to see if they are equal
     */

    /* Should be equal */
    assert(0 == H5P__mt_cmp_list_or_class(list1, list1));

    /* Should not be equal */
    assert(1 == H5P__mt_cmp_list_or_class(list1, list2));

    assert(0 == H5P__mt_cmp_list_or_class(list1, list1_copy));
    


    /**
     * Testing closing the class structures, which puts them on the class free list.
     * H5P__shutdown() then tests the H5P__mt_clear_class() function and frees all of
     * the allocated memory in those structures and the structures themselves.
     */
    H5P__mt_close_list(list1);
    H5P__mt_close_list(list2);
    H5P__mt_close_class(att_class);
    H5P__mt_close_class(root_class);

    H5P__shutdown();


} /* serial_list_test() */



int main(void)
{

    init_prop_info_tbl();
    serial_class_test();
    serial_list_test();

    printf("\nTesting MT PROP finished!\n");

    return(0);

} /* end main() */


//#endif /* H5_HAVE_MULTITHREAD */