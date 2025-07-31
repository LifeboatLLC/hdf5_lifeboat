#include "h5test.h"
#include "testframe.h"

#ifdef H5_HAVE_MULTITHREAD
#include <stdatomic.h>

#include "H5Pprivate.h"
#include "H5Ppkg_mt.h"

#define TEST_ROOT   "test_root"
#define CLASS1_NAME "Class 1"
#define CLASS2_NAME "Class 2"
#define CLASS3_NAME "Class 3"

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

/**
 *
 */
static hid_t
create_test_root_class(void)
{
    H5P_mt_class_t              *test_root = NULL;
    H5P_mt_active_thread_count_t thrd;
    H5P_mt_active_thread_count_t update_thrd;
    bool                         done = FALSE;

    hid_t ret_value;

    if (NULL == (test_root = H5P__mt_create_class(NULL, "test_root", H5P_TYPE_ROOT, 0, NULL, NULL, NULL, NULL,
                                                  NULL, NULL))) {
        fprintf(stderr, "Failed creating test root class.");
        return -1;
    }

    if ((ret_value = H5I_register(H5I_GENPROP_CLS, test_root, FALSE)) < 0) {
        fprintf(stderr, "Failed registering test root class in index.");
        return -1;
    }

    return (ret_value);

} /* create_test_root_class() */

#endif

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
