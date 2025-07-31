#include "h5test.h"
#include "testframe.h"

#ifdef H5_HAVE_MULTITHREAD
#include <stdatomic.h>

#include "H5Pprivate.h"
#include "H5Ppkg_mt.h"

#define TEST_ROOT_NAME   "test_root"
#define CLASS1_NAME "Class 1"
#define CLASS2_NAME "Class 2"
#define CLASS3_NAME "Class 3"

#define NEG_SENTINEL_NAME "neg_sentinel"
#define POS_SENTINEL_NAME "pos_sentinel"

/****************************************************************************************
 *
 * Structure:   prop_info_t
 *
 * Description:
 *
 * An array of instances of prop_info_t is used to create properties and to be compared
 * back to for sanity checking after creating or modifying.
 *
 * Fields:
 *
 * name (char *):
 *      Pointer to a dynamically allocated string containing the name of the property.
 *
 * value (_Atomic H5P_mt_prop_value_t):
 *      Atomic structure containing the pointer to the buffer containing the value of the
 *      property, and its size.
 *
 * edited_value (_Atomic H5P_mt_prop_value_t):
 *      Atomic structure containing the pointer to the buffer containing the value of the
 *      property after it has been modified, and its size.
 *
 ****************************************************************************************
 */
#if 0
typedef struct prop_cb_info_t
{
    H5P_prp_create_func_t       create;
    H5P_prp_set_func_t          set;
    H5P_prp_get_func_t          get;
    H5P_prp_encode_func_t       encode;
    H5P_prp_decode_func_t       decode;
    H5P_prp_delete_func_t       del;
    H5P_prp_copy_func_t         copy;
    H5P_prp_compare_func_t      cmp;
    H5P_prp_close_func_t        close;

} prop_cb_info_t;
#endif

typedef struct prop_info_t {
    char                       *name;
    _Atomic H5P_mt_prop_value_t value;
    _Atomic H5P_mt_prop_value_t edited_value;
    // prop_cb_info_t              callbacks;

} prop_info_t;

typedef struct mt_test_params_t {
    int thread_id;

} mt_test_params_t;

static herr_t init_globals(void);
static herr_t reset_globals(TestParams_t *params);

/** TODO: Will eventually need test callback functions */

static hid_t create_test_root_class(void);



/****************************************************************************************
 * Function:    create_test_root_class
 *
 * Purpose:     Creates a new root class for testing. This is done to test the 
 *              multithread create class function in the case of creating the root class
 *              and varify all of the fields prior to deriving classes from.
 *
 * Return:      Success: ID of the new test root class
 *
 *              Failure: -1
 *
 ****************************************************************************************
 */
static hid_t
create_test_root_class(void)
{
    H5P_mt_class_t             * test_root = NULL;
    H5P_mt_active_thread_count_t thrd;
    H5P_mt_active_thread_count_t update_thrd;
    H5P_mt_class_ref_counts_t    refs;
    H5P_mt_prop_t              * neg_sentinel;
    H5P_mt_prop_t              * pos_sentinel;
    H5P_mt_prop_aptr_t           neg_next;
    H5P_mt_prop_aptr_t           pos_next;
    H5P_mt_prop_value_t          neg_value;
    H5P_mt_prop_value_t          pos_value;
    bool                         done = FALSE;

    hid_t ret_value;

    if (NULL == (test_root = H5P__mt_create_class(NULL, TEST_ROOT_NAME, H5P_TYPE_ROOT, 0, NULL, NULL, NULL, NULL,
                                                  NULL, NULL))) {
        fprintf(stderr, "Failed creating test root class.");
        return -1;
    }

    /* Assert checks to ensure the fields of the new test_root class are correct */
    assert(atomic_load(&(test_root->tag)) == H5P_MT_CLASS_TAG);
    assert(test_root->parent_id == H5I_INVALID_HID);
    assert(test_root->parent_ptr == NULL);
    assert(0 == strcmp(test_root->name, TEST_ROOT_NAME));
    assert(test_root->type == H5P_TYPE_ROOT);
    assert(atomic_load(&(test_root->curr_version)) == 1);
    assert(atomic_load(&(test_root->next_version)) == 2);
    assert(atomic_load(&(test_root->nprops_added)) == 0);
    assert(atomic_load(&(test_root->log_pl_len)) == 0);
    assert(atomic_load(&(test_root->phys_pl_len)) == 2);

    /* Assert checks to ensure the fields of test_root->ref_count are correct */
    refs = atomic_load(&(test_root->ref_count));

    assert(refs.pl == 0);
    assert(refs.plc == 0);
    assert(refs.deleted == FALSE);
    assert(refs.dummy_bool_1 == FALSE);
    assert(refs.dummy_bool_2 == FALSE);
    assert(refs.dummy_bool_3 == FALSE);
    
    /* Assert checks to ensure the LFSLL of test_root are correct */
    neg_sentinel = atomic_load(&(test_root->pl_head));
    neg_value = atomic_load(&(neg_sentinel->value));
    neg_next = atomic_load(&(neg_sentinel->next));
    pos_sentinel = neg_next.ptr;
    pos_value = atomic_load(&(pos_sentinel->value));
    pos_next = atomic_load(&(pos_sentinel->next));

    /* Assert checks for negative sentinel fields */
    assert(atomic_load(&(neg_sentinel->tag)) == H5P_MT_PROP_TAG);
    assert(neg_next.deleted == FALSE);
    assert(neg_next.dummy_bool_1 == FALSE);
    assert(neg_next.dummy_bool_2 == FALSE);
    assert(neg_next.dummy_bool_3 == FALSE);
    assert(neg_sentinel->sentinel == TRUE);
    assert(neg_sentinel->in_prop_class == TRUE);
    assert(atomic_load(&(neg_sentinel->ref_count)) == 0);
    assert(neg_sentinel->in_lkup_tbl == FALSE);
    assert(neg_sentinel->chksum == LLONG_MIN);
    assert(0 == strcmp(neg_sentinel->name, NEG_SENTINEL_NAME));
    assert(neg_value.ptr == NULL);
    assert(neg_value.size == 0);
    assert(atomic_load(&(neg_sentinel->create_version)) == 1);
    assert(atomic_load(&(neg_sentinel->delete_version)) == 0);
    assert(neg_sentinel->callbacks_mt_safe = FALSE);
    assert(neg_sentinel->create = NULL);
    assert(neg_sentinel->set = NULL);
    assert(neg_sentinel->get = NULL);
    assert(neg_sentinel->encode = NULL);
    assert(neg_sentinel->decode = NULL);
    assert(neg_sentinel->del = NULL);
    assert(neg_sentinel->copy = NULL);
    assert(neg_sentinel->cmp = NULL);
    assert(neg_sentinel->close = NULL);

    /* Assert checks for positive sentinel fields */
    assert(atomic_load(&(pos_sentinel->tag)) == H5P_MT_PROP_TAG);
    assert(pos_next.deleted == FALSE);
    assert(pos_next.dummy_bool_1 == FALSE);
    assert(pos_next.dummy_bool_2 == FALSE);
    assert(pos_next.dummy_bool_3 == FALSE);
    assert(pos_sentinel->sentinel == TRUE);
    assert(pos_sentinel->in_prop_class == TRUE);
    assert(atomic_load(&(pos_sentinel->ref_count)) == 0);
    assert(pos_sentinel->in_lkup_tbl == FALSE);
    assert(pos_sentinel->chksum == LLONG_MIN);
    assert(0 == strcmp(pos_sentinel->name, POS_SENTINEL_NAME));
    assert(pos_value.ptr == NULL);
    assert(pos_value.size == 0);
    assert(atomic_load(&(pos_sentinel->create_version)) == 1);
    assert(atomic_load(&(pos_sentinel->delete_version)) == 0);
    assert(pos_sentinel->callbacks_mt_safe = FALSE);
    assert(pos_sentinel->create = NULL);
    assert(pos_sentinel->set = NULL);
    assert(pos_sentinel->get = NULL);
    assert(pos_sentinel->encode = NULL);
    assert(pos_sentinel->decode = NULL);
    assert(pos_sentinel->del = NULL);
    assert(pos_sentinel->copy = NULL);
    assert(pos_sentinel->cmp = NULL);
    assert(pos_sentinel->close = NULL);

    /**
     * NOTE: stat fields are checked in later test functions to ensure they are 
     * initialized correctly and are incremented and decremented correctly.
     */

    if ((ret_value = H5I_register(H5I_GENPROP_CLS, test_root, FALSE)) < 0) {
        fprintf(stderr, "Failed registering test root class in index.");
        return -1;
    }

    /* Now that the test_root has an ID in the index update and check test_root->thrd */
    thrd = atomic_load(&(test_root->thrd));
    assert(thrd.count == 0);
    assert(thrd.opening == TRUE);
    assert(thrd.closing == FALSE);

    update_thrd.count = thrd.count;
    update_thrd.opening = FALSE;
    update_thrd.closing = FALSE;

    do
    {
        /* Atomically update test_root->thrd.opening field to now be FALSE */
        if (!atomic_compare_exchange_strong(&(test_root->thrd), &thrd, update_thrd))
        {
            atomic_fetch_add(&(test_root->num_thrd_update_cols), 1);
        }
        else
            done = TRUE;
    
    } while ( done == FALSE );


    /* Double check test_root->thrd was updated correctly */
    thrd = atomic_load(&(test_root->thrd));
    assert(thrd.count == 0);
    assert(thrd.opening == FALSE);
    assert(thrd.closing == FALSE);


    return (ret_value);

} /* create_test_root_class() */


#endif /* ifdef H5_HAVE_MULTITHREAD */

/**
 *
 */
int
main(int argc, char **argv)
{
    H5open();

    /* Initialize testing framework */
    if (TestInit(argv[0], NULL, NULL, NULL, NULL, 0, 0) < 0) {
        fprintf(stderr, "couldn't initialize testing framework\n");
        exit(EXIT_FAILURE);
    }

    /* Display testing information */
    TestInfo(stdout);
}
