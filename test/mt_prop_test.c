#define H5P_TESTING /* indicates that the testing APIs should be available */

#include "testhdf5.h"
#include "testframe.h"

#ifdef H5_HAVE_MULTITHREAD
#include <stdatomic.h>

#include "H5Pprivate.h"
#define H5P_FRIEND /* suppress error about including H5Ppkg */
#include "H5Ppkg.h"
#include "H5Ppkg_mt.h"
#include "H5Iprivate.h"

#define TEST_ROOT_NAME "test_root"
hid_t TEST_ROOT_ID_g = H5I_INVALID_HID;

#define CLASS1_NAME "Class 1"
hid_t CLASS1_ID_g = H5I_INVALID_HID;

#define CLASS2_NAME "Class 2"
hid_t CLASS2_ID_g = H5I_INVALID_HID;

#define NEG_SENTINEL_NAME "neg_sentinel"
#define POS_SENTINEL_NAME "pos_sentinel"

#define DEFAULT_MAX_NUM_THREADS 32

/****************************************************************************************
 *
 * Structure: prop_info_t
 *
 * Description:
 *
 * prop_info_t is a structure used in testing to store the chksum, name, and a pointer to
 * a H5P_mt_prop_t. The H5P_mt_prop_t is used as the basis for the properties created in
 * test_h5p_mt_functions, and are used to compare back to ensure that any changes made to
 * properties in classes and lists are made correctly.
 *
 * Fields:
 *
 * chksum (int64_t):
 *      Checksum of the property stored in this instance of prop_info_t.
 *
 * name (const char *):
 *      Name of the property stored in this instance of prop_info_t.
 *
 * prop (H5P_mt_prop_t *):
 *      Pointer to an instance of H5P_mt_prop_t used to created properties and compare
 *      them back to ensuring correct fields during testing.
 *
 ****************************************************************************************
 */
#define PROP_INFO_TAG 0x1111
typedef struct prop_info_t {
    int64_t        chksum;
    const char    *name;
    H5P_mt_prop_t *prop;

} prop_info_t;

/****************************************************************************************
 *
 * Structure: struct_params_t
 *
 * Description:
 *
 * This structure is used to count and store the classes and lists used during testing.
 *
 * NOTE: The test_root class is not stored in the class LFSLL.
 *
 * Fields:
 *
 * thread_id (int):
 *      The id generated for each thread in the test.
 *
 * num_classes (int32_t):
 *      Counts the number of testing classes stored in the LFSLL of test classes.
 *
 * test_classes_head (H5P_mt_class_sptr_t):
 *      H5P_mt_class_sptr_t struct where the .ptr field points to the head of the LFSLL
 *      of the classes used for testing.
 *
 * num_lists (int32_t):
 *      Counts the number of testing lists stored in the LFSLL of test lists.
 *
 * test_lists_head (H5P_mt_list_sptr_t):
 *      H5P_mt_class_sptr_t struct where the .ptr field points to the head of the LFSLL
 *      of the lists used for testing.
 *
 * num_threads (int);
 *      The total number of threads currently being ran through the tests. This is used
 *      at some parts in the test to confirm that certain stats are correct.
 *
 ****************************************************************************************
 */
typedef struct test_params_t {
    int thread_id;

    int32_t             num_classes;
    H5P_mt_class_sptr_t test_classes_head;

    int32_t            num_lists;
    H5P_mt_list_sptr_t test_lists_head;

    int num_threads;

} test_params_t;

/**
 * Arrays of prop_info_t* used to store all versions of properties used
 * in testing to be used to create the test properties from and compare
 * them to ensuring correct operations for class and lists respectively.
 */
static prop_info_t *class_prop_table;
static prop_info_t *class2_prop_table;
static prop_info_t *list_prop_table;
static prop_info_t *list2_prop_table;
static prop_info_t *list3_prop_table;

static herr_t init_globals(void);
static herr_t init_class_props(void);
static herr_t init_class2_props(void);
static herr_t init_list_props(void);
static herr_t init_list2_props(void);
static herr_t init_list3_props(void);

static hid_t create_test_root_class(void);

static herr_t st_test_1(TestParams_t *params);
static herr_t mt_test_1(TestParams_t *params);
static void   test_1_helper(int num_threads);
static void  *test_h5p_mt_functions(void *test_params);

static herr_t test_h5p_mt_class_1(test_params_t *test_params);
static herr_t test_h5p_mt_class_2(test_params_t *test_params);
static herr_t test_h5p_mt_list_1(test_params_t *test_params);
static herr_t test_h5p_mt_list_2(test_params_t *test_params);

static herr_t         check_stats(test_params_t *test_params);
static herr_t         check_global_stats(int num_threads);
static H5P_mt_prop_t *get_table_prop_ver(prop_info_t prop_table, uint8_t version);
static herr_t  class_ver_and_len_check(H5P_mt_class_t *class, uint64_t curr_version, uint64_t next_version,
                                       size_t nprops_added, size_t log_len, size_t phys_len,
                                       const char *where);
static herr_t  list_ver_and_len_check(H5P_mt_list_t *list, uint64_t curr_version, uint64_t next_version,
                                      size_t nprops_inherited, size_t nprops_added, size_t nprops,
                                      size_t log_pl_len, size_t phys_pl_len, const char *where);
static herr_t  check_class_ref_counts(H5P_mt_class_t *class, uint64_t pl, uint32_t plc, bool deleted,
                                      const char *where);
static herr_t  check_and_set_thrd_flags(void *param, bool opening_is, bool closing_is, bool set_opening,
                                        bool set_closing, const char *where);
static herr_t  compare_lfsll_to_table_props(H5P_mt_prop_t **test_prop, prop_info_t *prop_table,
                                            const char *where);
static herr_t  list_lkup_tbl_check(H5P_mt_list_t *list, size_t nprops_inherited, const char *where);
H5P_mt_prop_t *get_correct_prop_version_from_table(H5P_mt_prop_t *table_prop, uint64_t create_ver,
                                                   uint64_t delete_ver, const char *where);
static herr_t  prop_check(H5P_mt_prop_t *prop, H5P_mt_prop_t *table_prop, bool in_prop_class,
                          bool in_lkup_tbl);
static herr_t  sentinel_check(H5P_mt_prop_t *prop);

static herr_t close_test_structs(test_params_t *test_params);

static herr_t term_test_free_lists(int num_threads);

static herr_t reset_globals(TestParams_t H5_ATTR_UNUSED *params);

/** TODO: Will eventually need test callback functions */

/****************************************************************************************
 * Function:    init_globals
 *
 * Purpose:     This function allocates class_prop_table, class2_prop_table,
 *              list_prop_table, list2_prop_table, and list3_prop_table, and calls the
 *              functions to intialize them.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
init_globals(void)
{
    herr_t ret; /* Generic return value */

    herr_t ret_value = SUCCEED;

    /* Allocate the class_prop_table*/
    class_prop_table = (prop_info_t *)malloc(8 * sizeof(prop_info_t));
    CHECK_PTR(class_prop_table, "malloc");

    /* Initialize the class_prop_table */
    ret = init_class_props();
    CHECK_I(ret, "init_class_props");

    /* Allocate the class2_prop_table*/
    class2_prop_table = (prop_info_t *)malloc(5 * sizeof(prop_info_t));
    CHECK_PTR(class2_prop_table, "malloc");

    /* Initialize the class2_prop_table */
    ret = init_class2_props();
    CHECK_I(ret, "init_class2_props");

    /* Allocate the list_prop_table */
    list_prop_table = (prop_info_t *)malloc(8 * sizeof(prop_info_t));
    CHECK_PTR(list_prop_table, "malloc");

    /* Initialize the list_prop_table */
    ret = init_list_props();
    CHECK_I(ret, "init_list_props");

    /* Allocate the list2_prop_table */
    list2_prop_table = (prop_info_t *)malloc(8 * sizeof(prop_info_t));
    CHECK_PTR(list2_prop_table, "malloc");

    /* Initialize the list2_prop_table */
    ret = init_list2_props();
    CHECK_I(ret, "init_list2_props");

    /* Allocate the list3_prop_table */
    list3_prop_table = (prop_info_t *)malloc(3 * sizeof(prop_info_t));
    CHECK_PTR(list3_prop_table, "malloc");

    /* Initialize the list2_prop_table */
    ret = init_list3_props();
    CHECK_I(ret, "init_list3_props");

    return (ret_value);

} /* init_globals() */

/****************************************************************************************
 * Function:    init_class_props
 *
 * Purpose:     Initializes the prop_info_t structs in the class_prop_table used by
 *              class1 in testing. There are 4 different prop_info_t structures with the
 *              properties each containing a different type of value (integer, float,
 *              char, double) to ensure the type of value doesn't cause problems.
 *
 *              There are 8 total property structures for class1.
 *              Some properties have multiple versions to handle modifications made to
 *              the property during testing. The first version is pointed to by the prop
 *              pointer field of class_prop_table with the later versions being
 *              connected in a LFSLL with the the built in next field of the
 *              H5P_mt_prop_t structs for simplicity.
 *
 *              The properties in this table have their create_version and delete_version
 *              atomically set to the versions the properties in the class are created
 *              and deleted at.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
init_class_props(void)
{
    H5P_mt_prop_t      *test_prop;
    H5P_mt_prop_t      *test_prop_ver2;
    H5P_mt_prop_t      *new_prop;
    H5P_mt_prop_value_t test_value;
    H5P_mt_prop_value_t value;
    H5P_mt_prop_value_t new_value;
    H5P_mt_prop_aptr_t  next;
    const char         *name;

    herr_t ret_value = SUCCEED;

    /**
     * Initalize the fields of class_prop_table,
     * including allocating and initializing the property
     */

    /**
     * prop_info_t 1
     */

    class_prop_table[0].name = strdup("Property 1");
    name                     = class_prop_table[0].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 1") != 0) {
        TestErrPrintf("class_prop_table names don't match! name = %s, Property 1\n", name);
    }

    class_prop_table[0].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(class_prop_table[0].chksum, "H5_checksum_metadata");

    static int prop1_value = 42;
    value.ptr              = (&prop1_value);
    value.size             = sizeof(prop1_value);

    class_prop_table[0].prop = H5P__mt_create_prop(name, value.ptr, value.size, TRUE, 1, NULL, NULL, NULL,
                                                   NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(class_prop_table[0].prop, "H5P__mt_create_prop");

    test_prop = class_prop_table[0].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "H5P__mt_create_prop");
    VERIFY(test_prop->chksum, class_prop_table[0].chksum, "H5P__mt_create_prop");
    CHECK_PTR(test_prop->name, "H5P__mt_create_prop");
    if (HDstrcmp(test_prop->name, "Property 1") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 1\n", test_prop->name);
    }

    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 1 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 1);
    atomic_store(&(test_prop->delete_version), 0);

    /**
     * prop_info_t 1 version 2
     */

    static int prop1_ver2_value = 66;
    value.ptr                   = (&prop1_ver2_value);
    value.size                  = (sizeof(prop1_ver2_value));

    new_prop = H5P__mt_create_prop(class_prop_table[0].name, value.ptr, value.size, TRUE, 2, NULL, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 1\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 1 version 2 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(new_prop->create_version), 3);
    atomic_store(&(new_prop->delete_version), 6);

    next     = atomic_load(&(test_prop->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop->next), next);

    /**
     * prop_info_t 1 version 3
     */

    test_prop_ver2 = new_prop;

    static int prop1_ver3_value = 3791;
    value.ptr                   = (&prop1_ver3_value);
    value.size                  = (sizeof(prop1_ver3_value));

    new_prop = H5P__mt_create_prop(class_prop_table[0].name, value.ptr, value.size, TRUE, 3, NULL, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop_ver2->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop_ver2->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 1\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 1 version 3 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(new_prop->create_version), 8);
    atomic_store(&(new_prop->delete_version), 0);

    next     = atomic_load(&(test_prop_ver2->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop_ver2->next), next);

    /**
     * prop_info_t 2
     */

    class_prop_table[1].name = strdup("Property 2");
    name                     = class_prop_table[1].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 2") != 0) {
        TestErrPrintf("class_prop_table names don't match! name = %s, Property 2\n", name);
    }

    class_prop_table[1].chksum =
        H5_checksum_metadata(class_prop_table[1].name, strlen(class_prop_table[1].name), 0);
    CHECK_I(class_prop_table[0].chksum, "H5_checksum_metadata");

    static float prop2_value = 3.14F;
    value.ptr                = (&prop2_value);
    value.size               = sizeof(prop2_value);

    class_prop_table[1].prop = H5P__mt_create_prop(class_prop_table[1].name, value.ptr, value.size, TRUE, 1,
                                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(class_prop_table[1].prop, "H5P__mt_create_prop");

    test_prop = class_prop_table[1].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "H5P__mt_create_prop");
    VERIFY(test_prop->chksum, class_prop_table[1].chksum, "H5P__mt_create_prop");
    CHECK_PTR(test_prop->name, "H5P__mt_create_prop");
    if (HDstrcmp(test_prop->name, "Property 2") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 2\n", test_prop->name);
    }

    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 2 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 1);
    atomic_store(&(test_prop->delete_version), 5);

    /**
     * prop_info_t 2 version 2
     */

    static float prop2_ver2_value = 6.28F;
    value.ptr                     = (&prop2_ver2_value);
    value.size                    = (sizeof(prop2_ver2_value));

    new_prop = H5P__mt_create_prop(class_prop_table[1].name, value.ptr, value.size, TRUE, 2, NULL, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 2\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 2 version 2 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(new_prop->create_version), 9);
    atomic_store(&(new_prop->delete_version), 0);

    next     = atomic_load(&(test_prop->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop->next), next);

    /**
     * prop_info_t 3
     */

    class_prop_table[2].name = strdup("Property 3");
    name                     = class_prop_table[2].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 3") != 0) {
        TestErrPrintf("class_prop_table names don't match! name = %s, Property 3\n", name);
    }
    class_prop_table[2].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(class_prop_table[2].chksum, "H5_checksum_metadata");

    static char prop3_value[9] = "Heracles";
    value.ptr                  = (&prop3_value);
    value.size                 = sizeof(prop3_value);

    class_prop_table[2].prop = H5P__mt_create_prop(class_prop_table[2].name, value.ptr, value.size, TRUE, 1,
                                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(class_prop_table[2].prop, "H5P__mt_create_prop");

    test_prop = class_prop_table[2].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "H5P__mt_create_prop");
    VERIFY(test_prop->chksum, class_prop_table[2].chksum, "H5P__mt_create_prop");
    CHECK_PTR(test_prop->name, "H5P__mt_create_prop");
    if (HDstrcmp(test_prop->name, "Property 3") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 3\n", test_prop->name);
    }

    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 3 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 1);
    atomic_store(&(test_prop->delete_version), 0);

    /**
     * prop_info_t 4
     */

    class_prop_table[3].name = strdup("Property 4");
    name                     = class_prop_table[3].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 4") != 0) {
        TestErrPrintf("class_prop_table names don't match! name = %s, Property 4\n", name);
    }
    class_prop_table[3].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(class_prop_table[3].chksum, "H5_checksum_metadata");

    static double prop4_value = 1.61803;
    value.ptr                 = (&prop4_value);
    value.size                = sizeof(prop4_value);

    class_prop_table[3].prop = H5P__mt_create_prop(class_prop_table[3].name, value.ptr, value.size, TRUE, 1,
                                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(class_prop_table[3].prop, "H5P__mt_create_prop");

    test_prop = class_prop_table[3].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "H5P__mt_create_prop");
    VERIFY(test_prop->chksum, class_prop_table[3].chksum, "H5P__mt_create_prop");
    CHECK_PTR(test_prop->name, "H5P__mt_create_prop");
    if (HDstrcmp(test_prop->name, "Property 4") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 4\n", test_prop->name);
    }

    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 4 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 2);
    atomic_store(&(test_prop->delete_version), 0);

    /**
     * prop_info_t 4 version 2
     */

    static double prop4_ver2_value = 3.33333;
    value.ptr                      = (&prop4_ver2_value);
    value.size                     = sizeof(prop4_ver2_value);

    new_prop = H5P__mt_create_prop(class_prop_table[3].name, value.ptr, value.size, TRUE, 2, NULL, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 4\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 4 version 2 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(new_prop->create_version), 4);
    atomic_store(&(new_prop->delete_version), 7);

    next     = atomic_load(&(test_prop->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop->next), next);

    return (ret_value);

} /* end init_class_props() */

/****************************************************************************************
 * Function:    init_class2_props
 *
 * Purpose:     Initializes the prop_info_t structs in the class2_prop_table used by
 *              class2 in testing.
 *
 *              There are 5 total properties for class2.
 *              Some properties have multiple versions to handle modifications made to
 *              the property during testing. The first version is pointed to by the prop
 *              pointer field of class2_prop_table with the later versions being
 *              connected in a LFSLL with the the built in next field of the
 *              H5P_mt_prop_t structs for simplicity.
 *
 *              NOTE: class2 is the class created by copying class1, and the class that
 *              is derived from class1.
 *
 *              The properties in this table have their create_version and delete_version
 *              atomically set to the versions the properties in the class are created
 *              and deleted at.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
init_class2_props(void)
{
    H5P_mt_prop_t      *test_prop;
    H5P_mt_prop_t      *test_prop2;
    H5P_mt_prop_t      *new_prop;
    H5P_mt_prop_value_t test_value;
    H5P_mt_prop_value_t value;
    H5P_mt_prop_value_t new_value;
    H5P_mt_prop_aptr_t  next;
    const char         *name;

    herr_t ret_value = SUCCEED;

    /**
     * Initalize the fields of class2_prop_table,
     * including allocating and initializing the property
     */

    /**
     * prop_info_t 1
     */

    class2_prop_table[0].name = strdup("Property 1");
    name                      = class2_prop_table[0].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 1") != 0) {
        TestErrPrintf("class2_prop_table names don't match! name = %s, Property 1\n", name);
    }

    class2_prop_table[0].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(class2_prop_table[0].chksum, "H5_checksum_metadata");

    static int prop1_value = 3791;
    value.ptr              = (&prop1_value);
    value.size             = sizeof(prop1_value);

    class2_prop_table[0].prop = H5P__mt_create_prop(name, value.ptr, value.size, TRUE, 1, NULL, NULL, NULL,
                                                    NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(class2_prop_table[0].prop, "H5P__mt_create_prop");

    test_prop = class2_prop_table[0].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "H5P__mt_create_prop");
    VERIFY(test_prop->chksum, class2_prop_table[0].chksum, "H5P__mt_create_prop");
    CHECK_PTR(test_prop->name, "H5P__mt_create_prop");
    if (HDstrcmp(test_prop->name, "Property 1") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 1\n", test_prop->name);
    }

    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 1 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 1);
    atomic_store(&(test_prop->delete_version), 0);

    /**
     * prop_info_t 2
     */

    class2_prop_table[1].name = strdup("Property 2");
    name                      = class2_prop_table[1].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 2") != 0) {
        TestErrPrintf("class2_prop_table names don't match! name = %s, Property 2\n", name);
    }

    class2_prop_table[1].chksum =
        H5_checksum_metadata(class2_prop_table[1].name, strlen(class2_prop_table[1].name), 0);
    CHECK_I(class2_prop_table[0].chksum, "H5_checksum_metadata");

    static float prop2_value = 6.28F;
    value.ptr                = (&prop2_value);
    value.size               = sizeof(prop2_value);

    class2_prop_table[1].prop = H5P__mt_create_prop(class2_prop_table[1].name, value.ptr, value.size, TRUE, 1,
                                                    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(class2_prop_table[1].prop, "H5P__mt_create_prop");

    test_prop = class2_prop_table[1].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "H5P__mt_create_prop");
    VERIFY(test_prop->chksum, class2_prop_table[1].chksum, "H5P__mt_create_prop");
    CHECK_PTR(test_prop->name, "H5P__mt_create_prop");
    if (HDstrcmp(test_prop->name, "Property 2") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 2\n", test_prop->name);
    }

    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 2 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 1);
    atomic_store(&(test_prop->delete_version), 0);

    /**
     * prop_info_t 3
     */

    class2_prop_table[2].name = strdup("Property 3");
    name                      = class2_prop_table[2].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 3") != 0) {
        TestErrPrintf("class2_prop_table names don't match! name = %s, Property 3\n", name);
    }
    class2_prop_table[2].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(class2_prop_table[2].chksum, "H5_checksum_metadata");

    static char prop3_value[9] = "Heracles";
    value.ptr                  = (&prop3_value);
    value.size                 = sizeof(prop3_value);

    class2_prop_table[2].prop = H5P__mt_create_prop(class2_prop_table[2].name, value.ptr, value.size, TRUE, 1,
                                                    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(class2_prop_table[2].prop, "H5P__mt_create_prop");

    test_prop = class2_prop_table[2].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "H5P__mt_create_prop");
    VERIFY(test_prop->chksum, class2_prop_table[2].chksum, "H5P__mt_create_prop");
    CHECK_PTR(test_prop->name, "H5P__mt_create_prop");
    if (HDstrcmp(test_prop->name, "Property 3") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 3\n", test_prop->name);
    }

    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 3 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 1);
    atomic_store(&(test_prop->delete_version), 0);

    /**
     * prop_info_t 3 version 2
     */

    static char prop3_ver2_value[9] = "Poseiden";
    value.ptr                       = (&prop3_ver2_value);
    value.size                      = sizeof(prop3_ver2_value);

    new_prop = H5P__mt_create_prop(class2_prop_table[2].name, value.ptr, value.size, TRUE, 1, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 3\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 1 version 2 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(new_prop->create_version), 2);
    atomic_store(&(new_prop->delete_version), 0);

    next     = atomic_load(&(test_prop->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop->next), next);

    /**
     * prop_info_t 3 version 3
     */

    test_prop2 = new_prop;

    static char prop3_ver3_value[9] = "Heracles";
    value.ptr                       = (&prop3_ver3_value);
    value.size                      = sizeof(prop3_ver3_value);

    new_prop = H5P__mt_create_prop(class2_prop_table[2].name, value.ptr, value.size, TRUE, 1, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop2->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop2->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 3\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 1 version 2 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(new_prop->create_version), 3);
    atomic_store(&(new_prop->delete_version), 0);

    next     = atomic_load(&(test_prop2->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop2->next), next);

    return (ret_value);

} /* end init_class2_props() */

/****************************************************************************************
 * Function:    init_list_props
 *
 * Purpose:     Initializes the prop_info_t structs in the list_prop_table used by list1
 *              in testing. There are 4 different prop_info_t structures with the
 *              properties each containing a different type of value (integer, float,
 *              char, double) to ensure the type of value doesn't cause problems.
 *
 *              There are 8 total property structures for list1.
 *              Some properties have multiple versions to handle modifications made to
 *              the property during testing. The first version is pointed to by the prop
 *              pointer field of list_prop_table with the later versions being
 *              connected in a LFSLL with the the built in next field of the
 *              H5P_mt_prop_t structs for simplicity.
 *
 *              NOTE: list1 is derived from class1, and due to how a list's lkup_tbl's
 *              base points to the property in the parent class's LFSLL, the first
 *              versions of prop_info_t 1, 2, and 3 are created to be copies of
 *              class_prop_table's first 3 prop_info_t structs (because they are
 *              inherited when list1 is created).
 *
 *              The properties in this table have their create_version and delete_version
 *              atomically set to the versions the properties in the list are created
 *              and deleted at.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
init_list_props(void)
{
    H5P_mt_prop_t      *class_table_prop;
    H5P_mt_prop_t      *test_prop;
    H5P_mt_prop_t      *test_prop_ver2;
    H5P_mt_prop_t      *new_prop;
    H5P_mt_prop_value_t test_value;
    H5P_mt_prop_value_t new_value;
    H5P_mt_prop_value_t value;
    H5P_mt_prop_aptr_t  next;
    const char         *name;

    herr_t ret_value = SUCCEED;

    /**
     * Initalize the fields of list_prop_table,
     * including allocating and initializing the property
     */

    /**
     * prop_info_t 1
     */

    /* Set the chksum and name of the property in the prop_info_t */
    list_prop_table[0].name = strdup(class_prop_table[0].name);
    name                    = list_prop_table[0].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 1") != 0) {
        TestErrPrintf("list_prop_table names don't match! name = %s, Property 1\n", name);
    }

    list_prop_table[0].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(list_prop_table[0].chksum, "H5_checksum_metadata");
    VERIFY(list_prop_table[0].chksum, class_prop_table[0].chksum, "H5_checksum_metadata");

    /* This prop is inherited, so get the pointer to the prop in the class_prop_table */
    class_table_prop = class_prop_table[0].prop;
    next             = atomic_load(&(class_table_prop->next));
    class_table_prop = next.ptr;
    next             = atomic_load(&(class_table_prop->next));
    class_table_prop = next.ptr;
    value            = atomic_load(&(class_table_prop->value));

    list_prop_table[0].prop = H5P__mt_create_prop(class_table_prop->name, value.ptr, value.size, TRUE,
                                                  atomic_load(&(class_table_prop->create_version)), NULL,
                                                  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(list_prop_table[0].prop, "init_list_props");
    assert(list_prop_table[0].prop);

    /* Verify the fields are correct */
    test_prop = list_prop_table[0].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "init_list_props");
    VERIFY(test_prop->chksum, list_prop_table[0].chksum, "init_list_props");
    VERIFY(test_prop->chksum, class_prop_table[0].chksum, "init_list_props");
    CHECK_PTR(test_prop->name, "init_list_props");
    if (HDstrcmp(test_prop->name, "Property 1") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 1\n", test_prop->name);
    }

    /* Ensure the value is correct */
    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 1 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 3);
    atomic_store(&(test_prop->delete_version), 0);

    /**
     * prop_info_t 1 version 2
     */

    static int prop1_ver2_value = 9001;
    value.ptr                   = (&prop1_ver2_value);
    value.size                  = (sizeof(prop1_ver2_value));

    new_prop = H5P__mt_create_prop(list_prop_table[0].name, value.ptr, value.size, FALSE, 2, NULL, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 1\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5p__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 1 version 2 value doesn't match! line %d\n", __LINE__);
    }

    new_prop->in_lkup_tbl = TRUE;
    atomic_store(&(new_prop->create_version), 3);
    atomic_store(&(new_prop->delete_version), 0);

    next     = atomic_load(&(test_prop->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop->next), next);

    /**
     * prop_info_t 1 version 3
     */

    test_prop_ver2 = new_prop;

    static int prop1_ver3_value = 1992;
    value.ptr                   = (&prop1_ver3_value);
    value.size                  = (sizeof(prop1_ver3_value));

    new_prop = H5P__mt_create_prop(list_prop_table[0].name, value.ptr, value.size, FALSE, 3, NULL, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop_ver2->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop_ver2->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 1\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5p__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 1 version 2 value doesn't match! line %d\n", __LINE__);
    }

    new_prop->in_lkup_tbl = TRUE;
    atomic_store(&(new_prop->create_version), 4);
    atomic_store(&(new_prop->delete_version), 6);

    next     = atomic_load(&(test_prop_ver2->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop_ver2->next), next);

    /**
     * prop_info_t 2
     */

    /* Set the chksum and name of the property in the prop_info_t */
    list_prop_table[1].name = strdup(class_prop_table[1].name);
    name                    = list_prop_table[1].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 2") != 0) {
        TestErrPrintf("list_prop_table names don't match! name = %s, Property 2\n", name);
    }

    list_prop_table[1].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(list_prop_table[1].chksum, "H5_checksum_metadata");
    VERIFY(list_prop_table[1].chksum, class_prop_table[1].chksum, "H5_checksum_metadata");

    /* This prop is inherited, so get the pointer to property in the class_prop_table */
    class_table_prop = class_prop_table[1].prop;
    next             = atomic_load(&(class_table_prop->next));
    class_table_prop = next.ptr;
    value            = atomic_load(&(class_table_prop->value));

    list_prop_table[1].prop = H5P__mt_create_prop(class_table_prop->name, value.ptr, value.size, TRUE,
                                                  atomic_load(&(class_table_prop->create_version)), NULL,
                                                  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(list_prop_table[1].prop, "init_list_props");
    assert(list_prop_table[1].prop);

    /* Verify the fields are correct */
    test_prop = list_prop_table[1].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "init_list_props");
    VERIFY(test_prop->chksum, list_prop_table[1].chksum, "init_list_props");
    VERIFY(test_prop->chksum, class_prop_table[1].chksum, "init_list_props");
    CHECK_PTR(test_prop->name, "init_list_props");
    if (HDstrcmp(test_prop->name, "Property 2") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 2\n", test_prop->name);
    }

    /* Ensure the value is correct */
    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "init_list_props");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 2 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 2);
    atomic_store(&(test_prop->delete_version), 0);

    /**
     * prop_info_t 3
     */

    /* Set the chksum and name of the property in the prop_info_t */
    list_prop_table[2].name = strdup(class_prop_table[2].name);
    name                    = list_prop_table[2].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 3") != 0) {
        TestErrPrintf("list_prop_table names don't match! name = %s, Property 3\n", name);
    }

    list_prop_table[2].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(list_prop_table[2].chksum, "H5_checksum_metadata");
    VERIFY(list_prop_table[2].chksum, class_prop_table[2].chksum, "H5_checksum_metadata");

    /* This prop is inherited, so get the pointer to the prop in the class_prop_table */
    class_table_prop = class_prop_table[2].prop;
    value            = atomic_load(&(class_table_prop->value));

    list_prop_table[2].prop = H5P__mt_create_prop(class_table_prop->name, value.ptr, value.size, TRUE,
                                                  atomic_load(&(class_table_prop->create_version)), NULL,
                                                  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(list_prop_table[2].prop, "init_list_props");
    assert(list_prop_table[2].prop);

    /* Verify the fields are correct */
    test_prop = list_prop_table[2].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "init_list_props");
    VERIFY(test_prop->chksum, list_prop_table[2].chksum, "init_list_props");
    VERIFY(test_prop->chksum, class_prop_table[2].chksum, "init_list_props");
    CHECK_PTR(test_prop->name, "init_list_props");
    if (HDstrcmp(test_prop->name, "Property 3") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 3\n", test_prop->name);
    }

    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "init_list_props");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 3 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 1);
    atomic_store(&(test_prop->delete_version), 5);

    /**
     * prop_info_t 3 version 2
     */

    static char prop3_ver2_value[80] =
        "'All we have to decide is what to do with the time that is given us.' - Gandalf";
    value.ptr  = (&prop3_ver2_value);
    value.size = sizeof(prop3_ver2_value);

    new_prop = H5P__mt_create_prop(list_prop_table[2].name, value.ptr, value.size, FALSE, 2, NULL, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 1\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5p__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 3 version 2 value doesn't match! line %d\n", __LINE__);
    }

    new_prop->in_lkup_tbl = TRUE;
    atomic_store(&(new_prop->create_version), 9);
    atomic_store(&(new_prop->delete_version), 0);

    next     = atomic_load(&(test_prop->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop->next), next);

    /**
     * prop_info_t 4
     */

    list_prop_table[3].name = strdup("Property 4");
    name                    = list_prop_table[3].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 4") != 0) {
        TestErrPrintf("list_prop_table names don't match! name = %s, Property 4\n", name);
    }

    list_prop_table[3].chksum =
        H5_checksum_metadata(list_prop_table[3].name, strlen(list_prop_table[3].name), 0);
    CHECK_I(list_prop_table[3].chksum, "H5_checksum_metadata");

    static double prop4_value = 0.61803;
    value.ptr                 = (&prop4_value);
    value.size                = sizeof(prop4_value);

    list_prop_table[3].prop = H5P__mt_create_prop(list_prop_table[3].name, value.ptr, value.size, FALSE, 1,
                                                  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(list_prop_table[3].prop, "H5P__mt_create_prop");

    test_prop = list_prop_table[3].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "H5P__mt_create_prop");
    VERIFY(test_prop->chksum, list_prop_table[3].chksum, "H5P__mt_create_prop");
    CHECK_PTR(test_prop->name, "H5P__mt_create_prop");
    if (HDstrcmp(test_prop->name, "Property 4") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 4\n", test_prop->name);
    }

    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 4 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 2);
    atomic_store(&(test_prop->delete_version), 7);

    /**
     * prop_info_t 4 version 2
     */

    static double prop4_ver2_value = 1.77245;
    value.ptr                      = (&prop4_ver2_value);
    value.size                     = (sizeof(prop4_ver2_value));

    new_prop = H5P__mt_create_prop(list_prop_table[3].name, value.ptr, value.size, FALSE, 3, NULL, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 4\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5p__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 4 version 2 value doesn't match! line %d\n", __LINE__);
    }

    atomic_store(&(new_prop->create_version), 8);
    atomic_store(&(new_prop->delete_version), 0);

    next     = atomic_load(&(test_prop->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop->next), next);

    return (ret_value);

} /* end init_list_props() */

/****************************************************************************************
 * Function:    init_list2_props
 *
 * Purpose:     Initializes the prop_info_t structs in the list2_prop_table used by list2
 *              in testing.
 *
 *              There are 8 total properties for list2.
 *              Some properties have multiple versions to handle modifications made to
 *              the property during testing. The first version is pointed to by the prop
 *              pointer field of list2_prop_table with the later versions being
 *              connected in a LFSLL with the the built in next field of the
 *              H5P_mt_prop_t structs for simplicity.
 *
 *              NOTE: list2 is the list created by copying list1, and at the version the
 *              copy occurs, list1's prop1 doesn't have a valid version. This tests that
 *              a list can handle copying another list when a property in the lkup_tbl is
 *              deleted.
 *
 *              The properties in this table have their create_version and delete_version
 *              atomically set to the versions the properties in the list are created
 *              and deleted at.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
init_list2_props(void)
{
    H5P_mt_prop_t      *list_table_prop;
    H5P_mt_prop_t      *test_prop;
    H5P_mt_prop_t      *test_prop_ver2;
    H5P_mt_prop_t      *new_prop;
    H5P_mt_prop_value_t test_value;
    H5P_mt_prop_value_t new_value;
    H5P_mt_prop_value_t value;
    H5P_mt_prop_aptr_t  next;
    const char         *name;

    herr_t ret_value = SUCCEED;

    /**
     * Initalize the fields of list2_prop_table,
     * including allocating and initializing the property
     */

    /**
     * prop_info_t 1
     */

    list2_prop_table[0].name = strdup(list_prop_table[0].name);
    name                     = list2_prop_table[0].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 1") != 0) {
        TestErrPrintf("list2_prop_table names don't match! name = %s, Property 1\n", name);
    }

    list2_prop_table[0].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(list2_prop_table[0].chksum, "H5_checksum_metadata");
    VERIFY(list2_prop_table[0].chksum, class_prop_table[0].chksum, "H5_checksum_metadata");

    /* This prop is inherited, but also deleted */
    list2_prop_table[0].prop = NULL;

    /**
     * prop_info_t 2
     */

    /* Set the chksum and name of the property in the prop_info_t */
    list2_prop_table[1].name = strdup(list_prop_table[1].name);
    name                     = list2_prop_table[1].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 2") != 0) {
        TestErrPrintf("list_prop_table names don't match! name = %s, Property 2\n", name);
    }

    list2_prop_table[1].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(list2_prop_table[1].chksum, "H5_checksum_metadata");
    VERIFY(list2_prop_table[1].chksum, list_prop_table[1].chksum, "H5_checksum_metadata");

    list_table_prop = list_prop_table[1].prop;
    value           = atomic_load(&(list_table_prop->value));

    list2_prop_table[1].prop = H5P__mt_create_prop(list_table_prop->name, value.ptr, value.size, TRUE,
                                                   atomic_load(&(list_table_prop->create_version)), NULL,
                                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(list2_prop_table[1].prop, "init_list2_props");
    assert(list2_prop_table[1].prop);

    /* Verify the fields are correct */
    test_prop = list2_prop_table[1].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "init_list_props");
    VERIFY(test_prop->chksum, list_prop_table[1].chksum, "init_list_props");
    VERIFY(test_prop->chksum, list_prop_table[1].chksum, "init_list_props");
    CHECK_PTR(test_prop->name, "init_list_props");
    if (HDstrcmp(test_prop->name, "Property 2") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 2\n", test_prop->name);
    }

    /* Ensure the value is correct */
    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "init_list_props");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 2 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 2);
    atomic_store(&(test_prop->delete_version), 0);

    /**
     * prop_info_t 2 version 2
     */

    static float prop2_ver2_value = 313.00987F;
    value.ptr                     = (&prop2_ver2_value);
    value.size                    = (sizeof(prop2_ver2_value));

    new_prop = H5P__mt_create_prop(list2_prop_table[1].name, value.ptr, value.size, FALSE, 2, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 2\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5p__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 2 version 2 value doesn't match! line %d\n", __LINE__);
    }

    new_prop->in_lkup_tbl = TRUE;
    atomic_store(&(new_prop->create_version), 4);
    atomic_store(&(new_prop->delete_version), 0);

    next     = atomic_load(&(test_prop->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop->next), next);

    /**
     * prop_info_t 2 version 3
     */

    test_prop_ver2 = new_prop;

    list_table_prop = list2_prop_table[1].prop;
    value           = atomic_load(&(list_table_prop->value));

    new_prop = H5P__mt_create_prop(list2_prop_table[1].name, value.ptr, value.size, FALSE, 3, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop_ver2->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop_ver2->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 2\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5p__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 2 version 3 value doesn't match! line %d\n", __LINE__);
    }

    new_prop->in_lkup_tbl = TRUE;
    atomic_store(&(new_prop->create_version), 5);
    atomic_store(&(new_prop->delete_version), 0);

    next     = atomic_load(&(test_prop_ver2->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop_ver2->next), next);

    /**
     * prop_info_t 3
     */

    /* Set the chksum and name of the property in the prop_info_t */
    list2_prop_table[2].name = strdup(list_prop_table[2].name);
    name                     = list2_prop_table[2].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 3") != 0) {
        TestErrPrintf("list_prop_table names don't match! name = %s, Property 3\n", name);
    }

    list2_prop_table[2].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(list2_prop_table[2].chksum, "H5_checksum_metadata");
    VERIFY(list2_prop_table[2].chksum, list_prop_table[2].chksum, "H5_checksum_metadata");

    list_table_prop = list_prop_table[2].prop;
    next            = atomic_load(&(list_table_prop->next));
    list_table_prop = next.ptr;
    value           = atomic_load(&(list_table_prop->value));

    list2_prop_table[2].prop = H5P__mt_create_prop(list_table_prop->name, value.ptr, value.size, FALSE, 1,
                                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(list2_prop_table[2].prop, "init_list_props");
    assert(list2_prop_table[2].prop);

    /* Verify the fields are correct */
    test_prop = list2_prop_table[2].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "init_list_props");
    VERIFY(test_prop->chksum, list2_prop_table[2].chksum, "init_list_props");
    VERIFY(test_prop->chksum, list2_prop_table[2].chksum, "init_list_props");
    CHECK_PTR(test_prop->name, "init_list_props");
    if (HDstrcmp(test_prop->name, "Property 3") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 3\n", test_prop->name);
    }

    /* Ensure the value is correct */
    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "init_list_props");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 3 value doesn't match! line=%d\n", __LINE__);
    }

    test_prop->in_lkup_tbl = TRUE;
    atomic_store(&(test_prop->create_version), 1);
    atomic_store(&(test_prop->delete_version), 0);

    /**
     * prop_info_t 4
     */

    /* Set the chksum and name of the property in the prop_info_t */
    list2_prop_table[3].name = strdup(list_prop_table[3].name);
    name                     = list2_prop_table[3].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 4") != 0) {
        TestErrPrintf("list_prop_table names don't match! name = %s, Property 4\n", name);
    }

    list2_prop_table[3].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(list2_prop_table[3].chksum, "H5_checksum_metadata");
    VERIFY(list2_prop_table[3].chksum, list_prop_table[3].chksum, "H5_checksum_metadata");

    list_table_prop = list_prop_table[3].prop;
    next            = atomic_load(&(list_table_prop->next));
    list_table_prop = next.ptr;
    value           = atomic_load(&(list_table_prop->value));

    list2_prop_table[3].prop = H5P__mt_create_prop(list_table_prop->name, value.ptr, value.size, FALSE, 1,
                                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(list2_prop_table[3].prop, "init_list_props");
    assert(list2_prop_table[3].prop);

    /* Verify the fields are correct */
    test_prop = list2_prop_table[3].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "init_list_props");
    VERIFY(test_prop->chksum, list2_prop_table[3].chksum, "init_list_props");
    VERIFY(test_prop->chksum, list2_prop_table[3].chksum, "init_list_props");
    CHECK_PTR(test_prop->name, "init_list_props");
    if (HDstrcmp(test_prop->name, "Property 4") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 4\n", test_prop->name);
    }

    /* Ensure the value is correct */
    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "init_list_props");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 4 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 1);
    atomic_store(&(test_prop->delete_version), 0);

    /**
     * prop_info_t 4 version 2
     */

    static double prop4_ver2_value = 101.003033345;
    value.ptr                      = (&prop4_ver2_value);
    value.size                     = (sizeof(prop4_ver2_value));

    new_prop = H5P__mt_create_prop(list2_prop_table[3].name, value.ptr, value.size, FALSE, 3, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 4\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5p__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 4 version 2 value doesn't match! line %d\n", __LINE__);
    }

    atomic_store(&(new_prop->create_version), 2);
    atomic_store(&(new_prop->delete_version), 0);

    next     = atomic_load(&(test_prop->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop->next), next);

    /**
     * prop_info_t 4 version 3
     */

    test_prop_ver2 = new_prop;

    list_table_prop = list2_prop_table[3].prop;
    value           = atomic_load(&(list_table_prop->value));

    new_prop = H5P__mt_create_prop(list2_prop_table[3].name, value.ptr, value.size, FALSE, 3, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(new_prop, "H5P__mt_create_prop");

    assert(new_prop);
    assert(atomic_load(&(new_prop->tag)) == H5P_MT_PROP_TAG);

    VERIFY(new_prop->chksum, test_prop_ver2->chksum, "H5P__mt_create_prop");
    if (HDstrcmp(new_prop->name, test_prop_ver2->name) != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 4\n", new_prop->name);
    }

    new_value = atomic_load(&(new_prop->value));
    VERIFY(new_value.size, value.size, "H5p__mt_create_prop");
    if (memcmp(new_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 4 version 3 value doesn't match! line %d\n", __LINE__);
    }

    atomic_store(&(new_prop->create_version), 3);
    atomic_store(&(new_prop->delete_version), 0);

    next     = atomic_load(&(test_prop_ver2->next));
    next.ptr = new_prop;
    atomic_store(&(test_prop_ver2->next), next);

    return (ret_value);

} /* end init_list2_props() */

/****************************************************************************************
 * Function:    init_list3_props
 *
 * Purpose:     Initializes the prop_info_t structs in the list3_prop_table used by list3
 *              in testing.
 *
 *              There are 3 total properties for list3.
 *
 *              NOTE: list3 is derived from class2 which in turn is derived from class1.
 *              This is done to further test classes ref counts and ensure they are
 *              incremented and decremented correctly. This also is for better testing
 *              closing lists and classes in different orders (explained further in the
 *              description of the close_test_structs() functions).
 *
 *              The properties in this table have their create_version and delete_version
 *              atomically set to the versions the properties in the list are created
 *              and deleted at.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
init_list3_props(void)
{
    H5P_mt_prop_t      *class_table_prop;
    H5P_mt_prop_t      *test_prop;
    H5P_mt_prop_value_t test_value;
    H5P_mt_prop_value_t value;
    const char         *name;

    herr_t ret_value = SUCCEED;

    /**
     * Initalize the fields of class2_prop_table,
     * including allocating and initializing the property
     */

    /**
     * prop_info_t 1
     */

    list3_prop_table[0].name = strdup("Property 1");
    name                     = list3_prop_table[0].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 1") != 0) {
        TestErrPrintf("list3_prop_table names don't match! name = %s, Property 1\n", name);
    }

    list3_prop_table[0].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(list3_prop_table[0].chksum, "H5_checksum_metadata");

    class_table_prop = class2_prop_table[0].prop;
    value            = atomic_load(&(class_table_prop->value));

    list3_prop_table[0].prop = H5P__mt_create_prop(name, value.ptr, value.size, TRUE,
                                                   atomic_load(&(class_table_prop->create_version)), NULL,
                                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(list3_prop_table[0].prop, "H5P__mt_create_prop");

    test_prop = list3_prop_table[0].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "H5P__mt_create_prop");
    VERIFY(test_prop->chksum, list3_prop_table[0].chksum, "H5P__mt_create_prop");
    CHECK_PTR(test_prop->name, "H5P__mt_create_prop");
    if (HDstrcmp(test_prop->name, "Property 1") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 1\n", test_prop->name);
    }

    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 1 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 1);
    atomic_store(&(test_prop->delete_version), 0);

    /**
     * prop_info_t 2
     */

    list3_prop_table[1].name = strdup("Property 2");
    name                     = list3_prop_table[1].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 2") != 0) {
        TestErrPrintf("list3_prop_table names don't match! name = %s, Property 2\n", name);
    }

    list3_prop_table[1].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(list3_prop_table[1].chksum, "H5_checksum_metadata");

    class_table_prop = class2_prop_table[1].prop;
    value            = atomic_load(&(class_table_prop->value));

    list3_prop_table[1].prop = H5P__mt_create_prop(name, value.ptr, value.size, TRUE,
                                                   atomic_load(&(class_table_prop->create_version)), NULL,
                                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(list3_prop_table[1].prop, "H5P__mt_create_prop");

    test_prop = list3_prop_table[1].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "H5P__mt_create_prop");
    VERIFY(test_prop->chksum, list3_prop_table[1].chksum, "H5P__mt_create_prop");
    CHECK_PTR(test_prop->name, "H5P__mt_create_prop");
    if (HDstrcmp(test_prop->name, "Property 2") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 2\n", test_prop->name);
    }

    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 2 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 1);
    atomic_store(&(test_prop->delete_version), 0);

    /**
     * prop_info_t 3
     */

    list3_prop_table[2].name = strdup("Property 3");
    name                     = list3_prop_table[2].name;
    CHECK_PTR(name, "strdup");
    if (HDstrcmp(name, "Property 3") != 0) {
        TestErrPrintf("list3_prop_table names don't match! name = %s, Property 3\n", name);
    }

    list3_prop_table[2].chksum = H5_checksum_metadata(name, strlen(name), 0);
    CHECK_I(list3_prop_table[2].chksum, "H5_checksum_metadata");

    class_table_prop = class2_prop_table[2].prop;
    value            = atomic_load(&(class_table_prop->value));

    list3_prop_table[2].prop = H5P__mt_create_prop(name, value.ptr, value.size, TRUE,
                                                   atomic_load(&(class_table_prop->create_version)), NULL,
                                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(list3_prop_table[2].prop, "H5P__mt_create_prop");

    test_prop = list3_prop_table[2].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "H5P__mt_create_prop");
    VERIFY(test_prop->chksum, list3_prop_table[2].chksum, "H5P__mt_create_prop");
    CHECK_PTR(test_prop->name, "H5P__mt_create_prop");
    if (HDstrcmp(test_prop->name, "Property 3") != 0) {
        TestErrPrintf("Property names don't match! name = %s, Property 3\n", test_prop->name);
    }

    test_value = atomic_load(&(test_prop->value));
    VERIFY(test_value.size, value.size, "H5P__mt_create_prop");
    if (memcmp(test_value.ptr, value.ptr, value.size) != 0) {
        TestErrPrintf("Property 3 value doesn't match! line=%d\n", __LINE__);
    }

    atomic_store(&(test_prop->create_version), 1);
    atomic_store(&(test_prop->delete_version), 0);

    return (ret_value);

} /* end init_list2_props() */

/****************************************************************************************
 * Function:    create_test_root_class
 *
 * Purpose:     Creates a new root class for testing. This is done to test the
 *              multithread create class function in the case of creating the root class
 *              and makes verifying fields and stats easier without the other classes
 *              that are automatically derived from the real hdf5 root class when
 *              H5open() is called.
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
    H5P_mt_class_t              *test_root = NULL;
    H5P_mt_active_thread_count_t thrd;
    H5P_mt_active_thread_count_t update_thrd;
    H5P_mt_class_ref_counts_t    refs;
    H5P_mt_prop_t               *neg_sentinel;
    H5P_mt_prop_t               *pos_sentinel;
    H5P_mt_prop_aptr_t           neg_next;
    H5P_mt_prop_aptr_t           pos_next;
    H5P_mt_prop_value_t          neg_value;
    H5P_mt_prop_value_t          pos_value;
    bool                         done = FALSE;

    hid_t ret_value;

    if (NULL == (test_root = H5P__mt_create_class(NULL, TEST_ROOT_NAME, H5P_TYPE_ROOT, 0, NULL, NULL, NULL,
                                                  NULL, NULL, NULL))) {
        fprintf(stderr, "create_test_root_class(): Failed creating test root class.");
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
    neg_sentinel = test_root->pl_head;
    neg_value    = atomic_load(&(neg_sentinel->value));
    neg_next     = atomic_load(&(neg_sentinel->next));
    pos_sentinel = neg_next.ptr;
    pos_value    = atomic_load(&(pos_sentinel->value));
    pos_next     = atomic_load(&(pos_sentinel->next));

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
    assert(neg_sentinel->callbacks_mt_safe == FALSE);
    assert(neg_sentinel->create == NULL);
    assert(neg_sentinel->set == NULL);
    assert(neg_sentinel->get == NULL);
    assert(neg_sentinel->encode == NULL);
    assert(neg_sentinel->decode == NULL);
    assert(neg_sentinel->del == NULL);
    assert(neg_sentinel->copy == NULL);
    assert(neg_sentinel->cmp == NULL);
    assert(neg_sentinel->close == NULL);

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
    assert(pos_sentinel->chksum == LLONG_MAX);
    assert(0 == strcmp(pos_sentinel->name, POS_SENTINEL_NAME));
    assert(pos_value.ptr == NULL);
    assert(pos_value.size == 0);
    assert(atomic_load(&(pos_sentinel->create_version)) == 1);
    assert(atomic_load(&(pos_sentinel->delete_version)) == 0);
    assert(pos_sentinel->callbacks_mt_safe == FALSE);
    assert(pos_sentinel->create == NULL);
    assert(pos_sentinel->set == NULL);
    assert(pos_sentinel->get == NULL);
    assert(pos_sentinel->encode == NULL);
    assert(pos_sentinel->decode == NULL);
    assert(pos_sentinel->del == NULL);
    assert(pos_sentinel->copy == NULL);
    assert(pos_sentinel->cmp == NULL);
    assert(pos_sentinel->close == NULL);

    /**
     * NOTE: stat fields are checked in later test functions to ensure they are
     * initialized correctly and are incremented and decremented correctly.
     */

    if ((ret_value = H5I_register(H5I_GENPROP_CLS, test_root, TRUE)) < 0) {
        fprintf(stderr, "create_test_root_class(): Failed registering test root class in index.");
        return -1;
    }

    /* Set the ID into the global and class's ID field */
    TEST_ROOT_ID_g = ret_value;
    atomic_store(&(test_root->id), ret_value);

    /* Now that the test_root has an ID in the index update and check test_root->thrd */
    thrd = atomic_load(&(test_root->thrd));
    assert(thrd.count == 0);
    assert(thrd.opening == TRUE);
    assert(thrd.closing == FALSE);

    update_thrd.count   = thrd.count;
    update_thrd.opening = FALSE;
    update_thrd.closing = FALSE;

    do {
        /* Atomically update test_root->thrd.opening field to now be FALSE */
        if (!atomic_compare_exchange_strong(&(test_root->thrd), &thrd, update_thrd)) {
            atomic_fetch_add(&(test_root->num_thrd_update_cols), 1);
        }
        else
            done = TRUE;

    } while (done == FALSE);

    /* Double check test_root->thrd was updated correctly */
    thrd = atomic_load(&(test_root->thrd));
    assert(thrd.count == 0);
    assert(thrd.opening == FALSE);
    assert(thrd.closing == FALSE);

    return (ret_value);

} /* create_test_root_class() */

/****************************************************************************************
 * Function:    st_test_1
 *
 * Purpose:     Initial single thread test of the multithread H5P structures and
 *              functions. This test is ran before any multithread tests to there are no
 *              inherent issues with the multithread H5P structures and functions, and
 *              to ensure the class, list, and global H5P stats increment and decrement
 *              as intended so debugging the multithread tests is easier.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
st_test_1(TestParams_t H5_ATTR_UNUSED *params)
{
    int    num_threads = 1;
    herr_t ret;

    /* Reset global stats before running first test */
    ret = H5P__reset_stats_global();
    CHECK_I(ret, "H5P__reset_stats_global");

    test_1_helper(num_threads);

    return SUCCEED;

} /* end st_test_1() */

/****************************************************************************************
 * Function:    mt_test_1
 *
 * Purpose:     Initial multithread test of the multithread H5P structures and functions.
 *              This test gets the max number of threads and calls the function
 *              test_1_helper().
 *
 *              NOTE: test_1_helper() is the same function called by st_test_1.
 *              test_1_helper() is a function containing several functions that test
 *              specific actions in H5P, and there are some sections in those tests that
 *              perform extra actions when running in single thread to test certain
 *              actions work exactly as intended. However they aren't performed in
 *              multithread, due to threads colliding and an exact order can't be
 *              guaranteed.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
mt_test_1(TestParams_t H5_ATTR_UNUSED *params)
{
    int max_num_threads = GetTestMaxNumThreads();
    // int test_express    = GetTestExpress();
    herr_t ret;

    /** TODO: adjust this to work with testframe's functions to get max threads */
    if (max_num_threads > DEFAULT_MAX_NUM_THREADS || max_num_threads < 0)
        max_num_threads = DEFAULT_MAX_NUM_THREADS;

    /* Run this test for thread counts between and including 2 <-> max_num_threads */
    for (int num_threads = 2; num_threads <= max_num_threads; num_threads++) {
        test_1_helper(num_threads);

        /* Reset global stats before incrementing num_thread and testing again */
        ret = H5P__reset_stats_global();
        CHECK_I(ret, "H5P__reset_stats_global");
    }

    return SUCCEED;

} /* end mt_test_1() */

/****************************************************************************************
 * Function:    test_1_helper
 *
 * Purpose:     Helper function for st_test_1 and mt_test_1.
 *
 *              This functions purpose is to initialize the test_params_t structures for
 *              every thread and to create all threads to run through the
 *              test_h5p_mt_functions() test suite with the appropriate test_params_t
 *              struct.
 *
 *              After all threads finish the test suite and join at the pthread_join(),
 *              this function then calls check_global stats() to ensure that all the
 *              threads correctly performed their actions.
 *
 *              Then close_test_structs() is looped to close all classes, lists, and
 *              properties used in testing, followed by term_test_free_lists() to
 *              correctly free all test structures.
 *
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static void
test_1_helper(int num_threads)
{
    char          banner[80];
    int           i;
    int           err_cnt = 0;
    pthread_t     threads[DEFAULT_MAX_NUM_THREADS];
    test_params_t params[DEFAULT_MAX_NUM_THREADS];
    herr_t        ret;

    assert(1 <= num_threads);
    assert(num_threads <= DEFAULT_MAX_NUM_THREADS);

    if (num_threads == 1) {
        sprintf(banner, "single thread smoke test");
    }
    else if (num_threads > 1) {
        sprintf(banner, "multi-thread test 1 -- %d threads", num_threads);
    }

    TESTING(banner);
    fflush(stdout);

    for (i = 0; i < num_threads; i++) {
        params[i].thread_id = i;

        params[i].num_classes           = 0;
        params[i].test_classes_head.ptr = NULL;
        params[i].test_classes_head.sn  = 0;

        params[i].num_lists           = 0;
        params[i].test_lists_head.ptr = NULL;
        params[i].test_lists_head.sn  = 0;

        params[i].num_threads = num_threads;
    }

    if (num_threads == 1) {
        test_h5p_mt_functions((void *)&params[0]);
    }
    else if (num_threads > 1) {
        /* Loop to create the threads and have each one run test_h5p_mt_functions */
        for (i = 0; i < num_threads; i++) {
            if (0 != pthread_create(&(threads[i]), NULL, &test_h5p_mt_functions, (void *)(&(params[i])))) {
                assert(FALSE);

                err_cnt++;

                TestErrPrintf("mt_test_1(): creation of thread %d failed.\n", i);
            }
        }

        /* Wait for all threads to complete */
        for (i = 0; i < num_threads; i++) {
            if (0 != pthread_join(threads[i], NULL)) {
                assert(FALSE);

                err_cnt++;

                TestErrPrintf("mt_test_1(): joining of thread %d failed.\n", i);
            }
            else {
                /* Collect error count from joined therads */
                // err_cnt += params[i].err_cnt;
            }
        }
    }

    /* Ensures the global stats correctly tracked all threads*/
    ret = check_global_stats(num_threads);
    CHECK_I(ret, "check_stats");

    /* Close all class and list structs used in testing and check stats */
    for (i = 0; i < num_threads; i++) {
        ret = close_test_structs(&(params[i]));
        CHECK_I(ret, "clear_free_lists");
    }

    /* Free all property, list, and class structs used in testing */
    ret = term_test_free_lists(num_threads);
    CHECK_I(ret, "term_test_free_lists");

    if (0 == err_cnt) {
        PASSED();
    }
    else {
        IncTestNumErrs();
        H5_FAILED();
    }

} /* end test_1_helper() */

/****************************************************************************************
 * Function:    test_h5p_mt_functions
 *
 * Purpose:     Base test function that contains a suite of test functions to test
 *              specific actions of the multithread H5P to ensure there are no issues
 *              with the structures or functions in single thread and in multithread when
 *              the multiple threads can't interact with each other.
 *
 *              NOTE: While the test suites are called class and list tests, the property
 *              structures and functions are also heavily tested in these (except for the
 *              callbacks which will be tested in another test).
 *
 *
 * Return:      SUCCESS/FAIL
 *
 ****************************************************************************************
 */
static void *
test_h5p_mt_functions(void *_test_params)
{
    test_params_t *test_params = (test_params_t *)_test_params;

    herr_t ret; /* Generic return value */

    /* First suite of class tests */
    ret = test_h5p_mt_class_1(test_params);
    CHECK_I(ret, "test_h5p_mt_class_1");

    /* Second suite of class tests */
    ret = test_h5p_mt_class_2(test_params);
    CHECK_I(ret, "test_h5p_mt_class_2");

    /* First suite of list tests */
    ret = test_h5p_mt_list_1(test_params);
    CHECK_I(ret, "test_h5p_mt_list_1");

    /* Second suite of list tests */
    ret = test_h5p_mt_list_2(test_params);
    CHECK_I(ret, "test_h5p_mt_list_2");

    /* Checks the stats of each class and list */
    ret = check_stats(test_params);
    if (0 != ret) {
        TestErrPrintf("check_stats failed on thread %d out of %d.\n", test_params->thread_id,
                      test_params->num_threads);
    }

    return SUCCEED;

} /* end test_h5p_mt_functions() */

/****************************************************************************************
 * Function:    test_h5p_mt_class_1
 *
 * Purpose:     Tests the multithread class structure, H5P_mt_class_t, and the functions
 *              for creating a new class from a root class, inserting, deleting,
 *              modifying, and searching for a property. After each of the steps the
 *              classes and properties affected are checked to ensure their fields were
 *              modified (or not modifed) correctly.
 *
 *              NOTE: some sections are only performed if there is only one thread
 *              running the test.
 *
 * Details:
 *
 *  1) Derive a new class1 from test_root.
 *  2) Insert default properties into a new class1.
 *      NOTE: default properties are properties that would be inserted into a class
 *      during H5open() and don't increment the class's current version.
 *  3) Register new class1 into the index, set the returned ID in the class1->id field,
 *     and set the opening flag to FALSE.
 *  4) Search for all default properties in class1, and ensure all properties were
 *     created correctly.
 *  5) Create and insert a new prop4 into class1, then search for the property.
 *  6) Modify default prop1, and search for the property (should return newest version).
 *  7) Modify added prop4, and search for the property (should return newest version).
 *      NOTE: default and added property don't affect all the same fields, thus both must
 *      be tested.
 *  8) Delete default prop2, and search for the property (should return NULL).
 *  9) Delete modified prop1, and search for the property (should return NULL).
 * 10) Delete added prop4, and search for the property (should return NULL).
 * 11) Insert a new version of a property where the next most recent version is deleted.
 * 12) Walk class1's LFSLL ensuring every H5P_mt_prop_t struct is in the correct order.
 *
 *
 * Return:      SUCCESS/FAIL
 *
 ****************************************************************************************
 */
static herr_t
test_h5p_mt_class_1(test_params_t *test_params)
{
    H5P_mt_class_t     *test_root = NULL;
    H5P_mt_class_t     *class1;
    H5P_mt_prop_t      *prop1;
    H5P_mt_prop_t      *prop2;
    H5P_mt_prop_t      *prop3;
    H5P_mt_prop_t      *table_prop;
    H5P_mt_prop_value_t table_value;
    H5P_mt_prop_t      *test_prop; /* to test a prop is what is expected */
    hid_t               class_id;
    herr_t              ret; /* Generic return value */

    herr_t ret_value = SUCCEED;

    /* Get the test root class from index */
    test_root = (H5P_mt_class_t *)H5I_object(TEST_ROOT_ID_g);
    CHECK_PTR(test_root, "H5I_object");

    assert(test_root);
    assert(atomic_load(&(test_root->tag)) == H5P_MT_CLASS_TAG);
    assert(0 == strcmp(test_root->name, TEST_ROOT_NAME));
    VERIFY(atomic_load(&(test_root->id)), TEST_ROOT_ID_g, "H5I_object");

    /* Check test_root's ref_count before creating any classes or lists */
    if (test_params->num_threads == 1) {
        ret = check_class_ref_counts(test_root, 0, 0, FALSE, "test_h5p_mt_class_1");
        CHECK_I(ret, "H5P__mt_create_list");
    }

    /**
     * Create a new class derived from the test root
     */

    class1 =
        H5P__mt_create_class(test_root, CLASS1_NAME, H5P_TYPE_USER, 0, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(class1, "H5P__mt_create_class");

    assert(class1);
    assert(atomic_load(&(class1->tag)) == H5P_MT_CLASS_TAG);

    if (HDstrcmp(class1->name, CLASS1_NAME) != 0) {
        TestErrPrintf("Class names don't match! name = %s, CLASS1_NAME=%s\n", class1->name, CLASS1_NAME);
    }

    /* Verify class1's parent info is correct */
    VERIFY(class1->parent_id, test_root->id, "H5P__mt_create_class");
    VERIFY(class1->parent_ptr, test_root, "H5P__mt_create_class");
    VERIFY(class1->parent_version, atomic_load(&(test_root->curr_version)), "H5P__mt_create_class");

    /* Ensure class fields are correct */
    ret = class_ver_and_len_check(class1, 1, 2, 0, 0, 2, "H5P__mt_create_class");
    CHECK_I(ret, "H5P__mt_create_class");

    /* Check test_root's ref_count */
    if (test_params->num_threads == 1) {
        ret = check_class_ref_counts(test_root, 0, 1, FALSE, "H5P__mt_create_class");
        CHECK_I(ret, "H5P__mt_create_class");
    }

    /**
     * Insert class1's default properties
     */

    /* create default prop1 */

    table_prop  = class_prop_table[0].prop;
    table_value = atomic_load(&(table_prop->value));

    ret = H5P__register_real(class1, class_prop_table[0].name, table_value.size, table_value.ptr, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__register_real");

    /* create default prop2 */

    table_prop  = class_prop_table[1].prop;
    table_value = atomic_load(&(table_prop->value));

    ret = H5P__register_real(class1, class_prop_table[1].name, table_value.size, table_value.ptr, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__register_real");

    /* create default prop3 */

    table_prop  = class_prop_table[2].prop;
    table_value = atomic_load(&(table_prop->value));

    ret = H5P__register_real(class1, class_prop_table[2].name, table_value.size, table_value.ptr, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__register_real");

    /**
     * All default properties are inserted,
     * register class1 in the index and set opening flag to FALSE
     */

    assert(atomic_load(&(class1->id)) == H5I_INVALID_HID);

    class_id = H5I_register(H5I_GENPROP_CLS, class1, TRUE);
    CHECK_I(class_id, "H5I_register");

    atomic_store(&(class1->id), class_id);

    /* Ensure class thrd flags are correct, then update the flags */
    if (0 > check_and_set_thrd_flags(class1, TRUE, FALSE, FALSE, FALSE, strdup("H5P__register_real"))) {
        fprintf(stderr, "test_h5p_mt_class_1(): class thrd flags mismatch.");
        return -1;
    }

    /**
     * Check class1 fields, and search for each default property and check their fields.
     */

    /* Ensure class fields are correct */
    if (0 > class_ver_and_len_check(class1, 1, 2, 0, 3, 5, "H5P__register_real")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_1(): class fields are incorrect.");

        return -1;
    }

    /* Search for default prop1 */
    if (NULL == (prop1 = H5P__mt_search__class(class1, class_prop_table[0].name))) {
        assert(prop1);

        fprintf(stderr, "test_h5p_mt_class_1(): Failed to find prop1.");
        return -1;
    }

    /* sanity check for prop1 */
    if (0 != prop_check(prop1, class_prop_table[0].prop, TRUE, FALSE)) {
        assert(test_prop == table_prop);

        fprintf(stderr, "test_h5p_mt_class_1(): prop1 and table mismatch.");
        return -1;
    }

    /* Search for default prop2 */
    if (NULL == (prop2 = H5P__mt_search__class(class1, class_prop_table[1].name))) {
        assert(prop2);

        fprintf(stderr, "test_h5p_mt_class_1(): Failed to find prop2.");
        return -1;
    }

    /* sanity check for prop2 */
    if (0 != prop_check(prop2, class_prop_table[1].prop, TRUE, FALSE)) {
        assert(test_prop == table_prop);

        fprintf(stderr, "test_h5p_mt_class_1(): prop2 and table mismatch.");
        return -1;
    }

    /* Search for default prop3 */
    if (NULL == (prop3 = H5P__mt_search__class(class1, class_prop_table[2].name))) {
        assert(prop3);

        fprintf(stderr, "test_h5p_mt_class_1(): Failed to find prop3.");
        return -1;
    }

    /* sanity check for prop3 */
    if (0 != prop_check(prop3, class_prop_table[2].prop, TRUE, FALSE)) {
        assert(test_prop == table_prop);

        fprintf(stderr, "test_h5p_mt_class_1(): prop3 and table mismatch.");
        return -1;
    }

    /**
     * Insert a new property
     */

    table_prop  = class_prop_table[3].prop;
    table_value = atomic_load(&(table_prop->value));

    /* Create and insert prop4 */
    if (H5P__mt_ins_or_mod_prop__class(class1, class_prop_table[3].name, table_value.ptr, table_value.size,
                                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL) < 0) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to create and insert prop4.");
        return -1;
    }

    /* Ensure class fields are correct */
    if (0 > class_ver_and_len_check(class1, 2, 3, 1, 4, 6, "H5P__mt_ins_or_mod_prop__class")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_1(): class fields are incorrect.");
        return -1;
    }

    /**
     * Search for the new property
     */

    /* Search for prop4 */
    if (NULL == (test_prop = H5P__mt_search__class(class1, class_prop_table[3].name))) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to find prop4.");
        return -1;
    }

    /* sanity check for prop4 */
    if (0 != prop_check(test_prop, table_prop, TRUE, FALSE)) {
        assert(test_prop == table_prop);

        fprintf(stderr, "test_h5p_mt_class_1(): prop4 and table mismatch.");
        return -1;
    }

    /**
     * Modify a default property
     */

    table_prop  = get_table_prop_ver(class_prop_table[0], 2);
    table_value = atomic_load(&(table_prop->value));

    assert(0 == strcmp(table_prop->name, class_prop_table[0].name));

    /* Create and insert a second version of property 1 */
    if (H5P__mt_ins_or_mod_prop__class(class1, class_prop_table[0].name, table_value.ptr, table_value.size,
                                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL) < 0) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to 'modify' prop1.");
        return -1;
    }

    /* Ensure class fields are correct */
    if (0 > class_ver_and_len_check(class1, 3, 4, 1, 4, 7, "H5P__mt_ins_or_mod_prop__class")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_1(): class fields are incorrect.");
        return -1;
    }

    /* Search for modified prop1, which has two versions (should find newest version) */

    if (NULL == (test_prop = H5P__mt_search__class(class1, class_prop_table[0].name))) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to find prop1 ver2.");
        return -1;
    }

    /* sanity check for modified prop1 */
    if (0 != prop_check(test_prop, table_prop, TRUE, FALSE)) {
        assert(test_prop == table_prop);

        fprintf(stderr, "test_h5p_mt_class_1(): prop1 ver2 table mismatch.");
        return -1;
    }

    /**
     * Modified an added property
     */

    table_prop  = get_table_prop_ver(class_prop_table[3], 2);
    table_value = atomic_load(&(table_prop->value));

    assert(0 == strcmp(table_prop->name, class_prop_table[3].name));

    /* Create and insert a second version of property 4 */
    if (H5P__mt_ins_or_mod_prop__class(class1, class_prop_table[3].name, table_value.ptr, table_value.size,
                                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL) < 0) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to 'modify' prop4.");
        return -1;
    }

    /* Ensure class fields are correct */
    if (0 > class_ver_and_len_check(class1, 4, 5, 1, 4, 8, "H5P__mt_ins_or_mod_prop__class")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_1(): class fields are incorrect.");
        return -1;
    }

    /* Search for modified prop4, which has two versions (should find newest version) */

    if (NULL == (test_prop = H5P__mt_search__class(class1, class_prop_table[3].name))) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to find prop4 ver2.");
        return -1;
    }

    /* sanity check for modified prop4 */
    if (0 != prop_check(test_prop, table_prop, TRUE, FALSE)) {
        assert(test_prop == table_prop);

        fprintf(stderr, "test_h5p_mt_class_1(): prop4 ver2 table mismatch.");
        return -1;
    }

    /**
     * Delete a default property
     */

    /* Set delete version on default prop2 */
    if (H5P__mt_delete_prop__class(class1, class_prop_table[1].name) < 0) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to delete default prop2.");
        return -1;
    }

    /* Ensure class fields are correct */
    if (0 > class_ver_and_len_check(class1, 5, 6, 1, 3, 8, "H5P__mt_delete_prop__class")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_1(): class fields are incorrect.");
        return -1;
    }

    /**
     * Search for deleted prop2. Should FAIL
     */

    if (NULL != (test_prop = H5P__mt_search__class(class1, class_prop_table[1].name))) {
        fprintf(stderr, "test_h5p_mt_class_1(): Returned a deleted prop2.");
        return -1;
    }

    /**
     * Delete a modified property (property with multiple vesrions)
     */

    /* Delete modified prop1*/
    if (H5P__mt_delete_prop__class(class1, class_prop_table[0].name) < 0) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to delete modified prop1.");
        return -1;
    }

    /* Ensure class fields are correct */
    if (0 > class_ver_and_len_check(class1, 6, 7, 1, 2, 8, "H5P__mt_delete_prop__class")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_1(): class fields are incorrect.");
        return -1;
    }

    /**
     * Search for deleted prop1. Should FAIL even
     * though a non-deleted older version exists
     */

    if (NULL != (test_prop = H5P__mt_search__class(class1, class_prop_table[0].name))) {
        fprintf(stderr, "test_h5p_mt_class_1(): Returned non-valid prop1.");
        return -1;
    }

    /**
     * Delete an added property
     */
    if (H5P__mt_delete_prop__class(class1, class_prop_table[3].name) < 0) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to delete modified prop1.");
        return -1;
    }

    /* Ensure class fields are correct */
    if (0 > class_ver_and_len_check(class1, 7, 8, 0, 1, 8, "H5P__mt_delete_prop__class")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_1(): class fields are incorrect.");
        return -1;
    }

    /**
     * Test inserting a new version of a previously deleted property,
     * insert prop1 and prop2 back in.
     */

    table_prop  = get_table_prop_ver(class_prop_table[0], 3);
    table_value = atomic_load(&(table_prop->value));

    ret = H5P__mt_ins_or_mod_prop__class(class1, table_prop->name, table_value.ptr, table_value.size, NULL,
                                         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__class");

    atomic_store(&(table_prop->create_version), atomic_load(&(class1->curr_version)));

    table_prop  = get_table_prop_ver(class_prop_table[1], 2);
    table_value = atomic_load(&(table_prop->value));

    ret = H5P__mt_ins_or_mod_prop__class(class1, table_prop->name, table_value.ptr, table_value.size, NULL,
                                         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__class");

    atomic_store(&(table_prop->create_version), atomic_load(&(class1->curr_version)));

    ret = class_ver_and_len_check(class1, 9, 10, 2, 3, 10, "H5P__mt_ins_or_mod_prop__class");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__class");

    /**
     * Walk class1's LFSLL ensuring all prop structs are correct and in order.
     */

    test_prop = class1->pl_head;

    if (0 > sentinel_check(test_prop)) {
        fprintf(stderr, "test_h5p_mt_class_1(): neg_sentinel failed check.");
        return -1;
    }

    ret = compare_lfsll_to_table_props(&test_prop, class_prop_table, "H5P__mt_ins_or_mod_prop__class");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__class");

    assert(test_prop->sentinel);
    if (0 > sentinel_check(test_prop)) {
        fprintf(stderr, "test_h5p_mt_class_1(): pos_sentinel failed check.");
        return -1;
    }

    /* Insert class1 into test_params class LFSLL */
    test_params->test_classes_head.ptr = class1;
    test_params->num_classes++;

    return (ret_value);

} /* end test_h5p_mt_class_1() */

/****************************************************************************************
 * Function:    test_h5p_mt_class_2
 *
 * Purpose:     Further tests the multithread class structure, H5P_mt_class_t, and the
 *              functions for copying a class, deriving a new class from a non-root
 *              class, comparison of two classes, closing a class, and reallocating a
 *              closed instance of a class structure from the class free list to be used
 *              for a new class. After each of the steps the classes and properties
 *              affected are checked to ensure their fields were modified (or not
 *              modifed) correctly.
 *
 *              NOTE: some sections are only performed if there is only one thread
 *              running the test.
 *
 * Details:
 *
 *  1) Create class2 as a copy of class1, and register class2 in the index and update
 *     class2->id and class2's opening flag
 *  2) Compare class1 and class2 (should be equal).
 *  3) Modify a property in class2 and compare class1 and class2 again (should not be
 *     equal).
 *  4) Modify the property in class2 back and compare class1 and class2 one last time
 *     (should be equal).
 *  5) Close class2 and ensure it was inserted into the class free list correctly.
 *  6) Change the closed class2's tag to be reallocable and derive a new class2 from
 *     class1. NOTE: step 6 is only done when testing with a single thread.
 *  7) Ensure the new class2 used the old class2 structure from the class free
 *     list and that the class free list is now empty (NOTE: the class free list will
 *     always contain two H5P_mt_class_sptr_t structs for the head and tail of that list,
 *     and if the pointers are NULL then the free list is "empty").
 *     NOTE: if running in multithread the new class2 is allocated from heap and not
 *     from the previous class2 structure.
 *  8) Check the property free list and ensure all the H5P_mt_prop_t, property structs,
 *     where correctly inserted, when the class2 struct was reallocated from the class
 *     free list.
 *     NOTE: step 8 is only done when testing with a single thread.
 *
 *
 * Return:      SUCCESS/FAIL
 *
 ****************************************************************************************
 */
static herr_t
test_h5p_mt_class_2(test_params_t *test_params)
{
    H5P_mt_class_t     *test_root;
    H5P_mt_class_t     *class1;
    H5P_mt_class_t     *class2;
    H5P_mt_class_t     *fl_class;
    H5P_mt_class_sptr_t fl_head;
    H5P_mt_class_sptr_t fl_tail;
    H5P_mt_class_sptr_t class_next;
    H5P_mt_prop_t      *table_prop;
    H5P_mt_prop_value_t table_value;
    H5P_mt_prop_t      *fl_prop;
    H5P_mt_prop_aptr_t  prop_fl_head;
    H5P_mt_prop_aptr_t  prop_fl_tail;
    hid_t               class2_id;
    herr_t              ret; /* Generic return value */

    herr_t ret_value = SUCCEED;

    /* Get class1 from the LFSLL of test classes */
    class1 = test_params->test_classes_head.ptr;

    assert(class1);
    assert(atomic_load(&(class1->tag)) == H5P_MT_CLASS_TAG);
    assert(0 == strcmp(class1->name, CLASS1_NAME));

    /* Get the test root class from class1 */
    test_root = class1->parent_ptr;

    assert(test_root);
    assert(atomic_load(&(test_root->tag)) == H5P_MT_CLASS_TAG);
    assert(0 == strcmp(test_root->name, TEST_ROOT_NAME));

    /**
     * Create a copy of class1, check test_root's derived class ref count,
     * and insert class2 into the index and set ID in class2->id and set
     * opening flag to FALSE.
     */

    if (NULL == (class2 = H5P__mt_copy_class(class1))) {
        assert(class2);

        fprintf(stderr, "test_h5p_mt_class_2(): Failed to create class2.");
        return -1;
    }

    assert(0 == strcmp(class2->name, CLASS1_NAME));

    /* Ensure class fields are correct */
    if (0 > class_ver_and_len_check(class2, 1, 2, 0, 3, 5, "H5P__mt_copy_class")) {
        assert(test_root == class2);

        fprintf(stderr, "test_h5p_mt_class_2(): class fields are incorrect.");
        return -1;
    }

    /* Check test_root's ref_count */
    if (test_params->num_threads == 1) {
        ret = check_class_ref_counts(test_root, 0, 2, FALSE, "H5P__mt_copy_class");
        CHECK_I(ret, "H5P__mt_copy_class");
    }

    assert(atomic_load(&(class2->id)) == H5I_INVALID_HID);

    if ((class2_id = H5I_register(H5I_GENPROP_CLS, class2, TRUE)) < 0) {
        fprintf(stderr, "test_h5p_mt_class_1(): failed registering class.");
        return -1;
    }

    atomic_store(&(class2->id), class2_id);

    /* Ensure class thrd flags are correct, then update the flags */
    if (0 > check_and_set_thrd_flags(class2, TRUE, FALSE, FALSE, FALSE, "H5P_register")) {
        assert(class1 == class2);

        fprintf(stderr, "test_h5p_mt_class_1(): class thrd flags mismatch.");
        return -1;
    }

    /**
     * Compare class1 and the copy, class2. (They should be equal)
     */

    if (0 != H5P__mt_cmp_class(class1, class2)) {
        assert(class1 == class2);

        fprintf(stderr, "test_h5p_mt_class_2(): class1 and class2 were not equal.");
        return -1;
    }

    /**
     * Modify a property in class2, and compare class1 and class2 again.
     * (They should not be equal)
     */

    table_prop  = get_table_prop_ver(class2_prop_table[2], 2);
    table_value = atomic_load(&(table_prop->value));

    /* Create and insert a second version of property 3 */
    if ((ret = H5P__mt_ins_or_mod_prop__class(class2, table_prop->name, table_value.ptr, table_value.size,
                                              NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) < 0) {

        fprintf(stderr, "test_h5p_mt_class_2(): Failed to 'modify' prop3.");
        assert(ret > 0);

        return -1;
    }

    /* Update table_prop version to match */
    atomic_store(&(table_prop->create_version), atomic_load(&(class2->curr_version)));

    /* Ensure class fields are correct */
    if (0 > class_ver_and_len_check(class2, 2, 3, 0, 3, 6, "H5P__mt_ins_or_mod_prop__class")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_2(): class2 fields are incorrect.");
        return -1;
    }

    /* Compare class1 and class2 (should not be equal) */
    if (1 != H5P__mt_cmp_class(class1, class2)) {
        assert(class1 == class2);

        fprintf(stderr, "test_h5p_mt_class_2(): class1 and class2 were equal.");
        return -1;
    }

    /**
     * Change the modified property back to default value and compare
     * class1 and class2 again. (Should be equal)
     */

    table_prop  = get_table_prop_ver(class2_prop_table[2], 3);
    table_value = atomic_load(&(table_prop->value));

    /* Create and insert prop3 with default value */
    if ((ret = H5P__mt_ins_or_mod_prop__class(class2, table_prop->name, table_value.ptr, table_value.size,
                                              NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) < 0) {
        fprintf(stderr, "test_h5p_mt_class_2(): Failed to 'modify' prop3.");
        assert(ret > 0);

        return -1;
    }

    /* Update table_prop version to match */
    atomic_store(&(table_prop->create_version), atomic_load(&(class2->curr_version)));

    /* Ensure class2's fields are correct */
    if (0 > class_ver_and_len_check(class2, 3, 4, 0, 3, 7, "H5P__mt_ins_or_mod_prop__class")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_2(): class2 fields are incorrect.");
        return -1;
    }

    /* Compare class1 and class2 (should be equal) */
    if (0 != H5P__mt_cmp_class(class1, class2)) {
        assert(class1 == class2);

        fprintf(stderr, "test_h5p_mt_class_2(): class1 and class2 were not equal.");
        return -1;
    }

    /**
     * Close class2 and check test_root's derived class ref count,
     * and ensure class2 was inserted into the class free list.
     */

    if (0 > H5Pclose_class(atomic_load(&(class2->id)))) {
        assert(class1 == class2);

        fprintf(stderr, "test_h5p_mt_class_2(): failed closing class2.");
        return -1;
    }

    /* Check test_root's ref_count */
    if (test_params->num_threads == 1) {
        ret = check_class_ref_counts(test_root, 0, 1, FALSE, "H5Pclose_class");
        CHECK_I(ret, "H5Pclose_class");
    }

    /* Check the class free list to ensure class2 was inserted correctly when closed */
    if (test_params->num_threads == 1) {
        fl_head  = atomic_load(&(H5P_mt_g.class_fl_head));
        fl_class = fl_head.ptr;

        assert(fl_class);
        assert(atomic_load(&(fl_class->tag)) == H5P_MT_CLASS_INVALID_TAG);
        VERIFY(atomic_load(&(fl_class->tag)), H5P_MT_CLASS_INVALID_TAG, "H5Pclose_class");

        /* Ensure fl_class's thrd flags are correct */
        ret = check_and_set_thrd_flags(fl_class, FALSE, TRUE, FALSE, TRUE, "H5Pclose_class");
        CHECK_I(ret, "H5Pclose_class");

        /* Ensure fl_class is still marked as deleted */
        ret = check_class_ref_counts(fl_class, 0, 0, TRUE, "H5Pclose_class");
        CHECK_I(ret, "H5Pclose_class");

        /**
         * Ensure class2(aka fl_class) is the head and tail
         * since it's the only class on the class free list
         */
        fl_tail = atomic_load(&(H5P_mt_g.class_fl_tail));
        VERIFY(fl_tail.ptr, fl_class, "H5Pclose_class");
        assert(fl_tail.ptr == fl_class);

        /**
         * From the class free list, modify the closed class's tag to be reallocable
         * and derive a new class2 from class1 (should allocate the H5P_mt_class_t
         * structure from the class free list)
         */
        atomic_store(&(fl_class->tag), H5P_MT_CLASS_FL_REALLOC_TAG);
    }

    class2 = H5P__mt_create_class(class1, CLASS2_NAME, H5P_TYPE_USER, 0, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(class2, "H5P__mt_create_class");
    assert(class2);
    assert(atomic_load(&(class2->tag)) == H5P_MT_CLASS_TAG);

    /* Check test_root's fields */
    if (test_params->num_threads == 1) {
        ret = check_class_ref_counts(test_root, 0, 1, FALSE, "H5P__mt_create_class");
        CHECK_I(ret, "H5P__mt_create_class");
    }

    /* Ensure class1's fields are correct */
    ret = check_class_ref_counts(class1, 0, 1, FALSE, "H5P__mt_create_class");
    CHECK_I(ret, "H5P__mt_create_class");

    /* Ensure class2's fields are correct */
    ret = class_ver_and_len_check(class2, 1, 2, 0, 3, 5, "H5P__mt_create_class");
    CHECK_I(ret, "H5P__mt_create_class");

    assert(atomic_load(&(class2->id)) == H5I_INVALID_HID);
    VERIFY(atomic_load(&(class2->id)), H5I_INVALID_HID, "H5P__mt_create_class");

    class2_id = H5I_register(H5I_GENPROP_CLS, class2, TRUE);
    CHECK_I(ret, "H5I_register");

    atomic_store(&(class2->id), class2_id);

    /* Ensure class thrd flags are correct, then update the flags */
    ret = check_and_set_thrd_flags(class2, TRUE, FALSE, FALSE, FALSE, "H5P_register");

    if (test_params->num_threads == 1) {
        /* Ensure the class free list is now empty */
        fl_head = atomic_load(&(H5P_mt_g.class_fl_head));
        fl_tail = atomic_load(&(H5P_mt_g.class_fl_tail));

        assert(!fl_head.ptr);
        assert(!fl_tail.ptr);
        VERIFY(fl_head.ptr, NULL, "H5P__mt_create_class");
        VERIFY(fl_tail.ptr, NULL, "H5P__mt_create_class");

        /**
         * Ensure the property structs in class2 were correctly inserted into the prop
         * free list when class2 was reallocated from the class free list.
         */

        prop_fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));
        prop_fl_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

        fl_prop = prop_fl_head.ptr;

        assert(fl_prop);
        assert(atomic_load(&(fl_prop->tag)) == H5P_MT_PROP_VALID_ONFL_TAG);

        VERIFY(atomic_load(&(H5P_mt_g.prop_fl_len)), 7, "H5P__mt_create_class");

        ret = sentinel_check(fl_prop);
        CHECK_I(ret, "H5P__mt_create_class");

        ret = compare_lfsll_to_table_props(&fl_prop, class2_prop_table, "H5P__mt_create_class");
        CHECK_I(ret, "H5P__mt_create_class");
        VERIFY(fl_prop->sentinel, TRUE, "H5P__mt_create_class");

        ret = sentinel_check(fl_prop);
        CHECK_I(ret, "H5P__mt_create_class");
        VERIFY(fl_prop, prop_fl_tail.ptr, "H5P__mt_create_class");
        assert(fl_prop == prop_fl_tail.ptr);
    }

    /* Insert class2 into test_params class LFSLL */
    test_params->num_classes++;

    class_next = atomic_load(&(class1->fl_next));
    assert(!class_next.ptr);

    class_next.ptr = class2;
    atomic_store(&(class1->fl_next), class_next);

    return (ret_value);

} /* end test_h5p_mt_class_2() */

/****************************************************************************************
 * Function:    test_h5p_mt_list_1
 *
 * Purpose:     Tests the multithread list structure, H5P_mt_list_t, and the functions
 *              for deriving a new list from a class, inserting, deleting, modifying,
 *              and searching for a property. After each of the steps the classes, lists,
 *              and properties affected are checked to ensure their fields were modified
 *              (or not modifed) correctly.
 *
 *              NOTE: some sections are only performed if there is only one thread
 *              running the test.
 *
 * Details:
 *
 *  1) Derive a new list1 from class1.
 *  2) Search for all unmodified properties in list1's lkup_tbl, which points to the
 *     property in the parent class's LFSLL (also check class's property's ref_count).
 *  3) Change a closed property's tag to be reallocable, and create and insert a new
 *     property into list1's LFSLL.
 *     NOTE: step 3 is only done if running tests in single thread. If running with
 *     multiple threads a property is allocated from heap instead.
 *  4) Search for the new property in list1's LFSLL.
 *  5) Modify one of list1's inherited properties, and search for that property to ensure
 *     the lkup_tbl's curr fields work correctly.
 *  6) Modify the same property again, and search for that new version to further test
 *     the curr fields are updated correctly
 *  7) Delete a property in the lkup_tbl where the most recent version is the base.
 *  8) Search for the deleted prop (should FAIL).
 *  9) Delete a property in the lkup_tbl where the most recent version is curr.
 * 10) Search for the deleted prop (should FAIL).
 * 11) Delete an non-inherited prop from list1's LFSLL.
 * 12) Search for the deleted prop (should FAIL).
 * 13) Walk the lkup_tbl and ensure it's correct.
 * 14) Walk the LFSLL and ensure it's correct.
 *
 *
 *
 * Return:      SUCCESS/FAIL
 *
 ****************************************************************************************
 */
static herr_t
test_h5p_mt_list_1(test_params_t *test_params)
{
    H5P_mt_class_t            *test_root;
    H5P_mt_class_t            *class1;
    H5P_mt_class_t            *class2;
    H5P_mt_class_sptr_t        class_next;
    H5P_mt_list_t             *list1;
    H5P_mt_list_table_entry_t *entry;
    H5P_mt_prop_t             *table_prop;
    H5P_mt_prop_value_t        table_value;
    H5P_mt_prop_aptr_t         prop_fl_head;
    H5P_mt_prop_aptr_t         prop_next;
    H5P_mt_prop_t             *test_prop;
    herr_t                     ret; /* Generic return value */

    herr_t ret_value = SUCCEED;

    /* Get class1 from the test LFSLL of classes */
    class1 = test_params->test_classes_head.ptr;

    assert(class1);
    assert(atomic_load(&(class1->tag)) == H5P_MT_CLASS_TAG);

    if (HDstrcmp(class1->name, CLASS1_NAME) != 0) {
        TestErrPrintf("Class names don't match! name = %s, CLASS1_NAME=%s\n", class1->name, CLASS1_NAME);
    }

    /* Get the test root class from class1 */
    test_root = class1->parent_ptr;

    assert(test_root);
    assert(atomic_load(&(test_root->tag)) == H5P_MT_CLASS_TAG);

    if (HDstrcmp(test_root->name, TEST_ROOT_NAME) != 0) {
        TestErrPrintf("Class names don't match! name = %s, TEST_ROOT_NAME=%s\n", test_root->name,
                      TEST_ROOT_NAME);
    }

    /* Get class2 from the test LFSLL of classes */
    class_next = atomic_load(&(class1->fl_next));
    class2     = class_next.ptr;

    assert(class2);
    assert(atomic_load(&(class2->tag)) == H5P_MT_CLASS_TAG);

    if (HDstrcmp(class2->name, CLASS2_NAME) != 0) {
        TestErrPrintf("Class names don't match! name = %s, CLASS2_NAME=%s\n", class2->name, CLASS2_NAME);
    }

    /**
     * Derive a new list1 from class1
     */

    list1 = H5P__mt_create_list(class1, NULL, FALSE, 0, TRUE);
    CHECK_PTR(list1, "H5P__mt_create_list");

    assert(list1);
    assert(atomic_load(&(list1->tag)) == H5P_MT_LIST_TAG);

    /* Ensure test_root's ref_count was not changed */
    if (test_params->num_threads == 1) {
        ret = check_class_ref_counts(test_root, 0, 1, FALSE, "H5P__mt_create_list");
        CHECK_I(ret, "H5P__mt_create_list");
    }

    /* Check class1's ref_count */
    ret = check_class_ref_counts(class1, 1, 1, FALSE, "H5P__mt_create_list");
    CHECK_I(ret, "H5P__mt_create_list");

    /* Verify list1's parent info is correct */
    VERIFY(list1->pclass_id, class1->id, "H5P__mt_create_list");
    VERIFY(list1->pclass_ptr, class1, "H5P__mt_create_list");
    VERIFY(list1->pclass_version, atomic_load(&(class1->curr_version)), "H5P__mt_create_list");
    VERIFY(atomic_load(&(list1->class_init)), TRUE, "H5P__mt_create_list");

    /* Ensure list fields are correct */
    ret = list_ver_and_len_check(list1, 1, 2, 3, 0, 3, 0, 2, "H5P__mt_create_list");

    /* Ensure thrd flags are correct */
    ret = check_and_set_thrd_flags(list1, FALSE, FALSE, FALSE, FALSE, "H5P__mt_create_list");

    /* Ensure the lkup_tbl is correct */
    ret = list_lkup_tbl_check(list1, list1->nprops_inherited, "H5P__mt_create_list");
    CHECK_I(ret, "H5P__mt_create_list");

    /* Ensure the LFSLL is empty other than the sentinel nodes */
    test_prop = list1->pl_head;
    assert(test_prop);
    assert(atomic_load(&(test_prop->tag)) == H5P_MT_PROP_TAG);
    ret = sentinel_check(test_prop);
    CHECK_I(ret, "H5P__mt_create_list");
    prop_next = atomic_load(&(test_prop->next));
    test_prop = prop_next.ptr;
    ret       = sentinel_check(test_prop);
    CHECK_I(ret, "H5P__mt_create_list");

    /**
     * Search for an inherited property to test searching the
     * lkup_tbl when the most current version is an entry's base,
     * and that the class's property's ref_count was incremented.
     */

    /* Search for and check prop1 */
    test_prop = H5P__mt_search__list(list1, list_prop_table[0].name);
    CHECK_PTR(test_prop, "H5P__mt_search__list");
    assert(test_prop);
    assert(atomic_load(&(test_prop->tag)) == H5P_MT_PROP_TAG);

    ret = prop_check(test_prop, list_prop_table[0].prop, TRUE, FALSE);
    CHECK_I(ret, "H5P__mt_search__list");

    VERIFY(atomic_load(&(test_prop->ref_count)), 1, "H5P__mt_create_list");

    /* Search for and check prop2 */
    test_prop = H5P__mt_search__list(list1, list_prop_table[1].name);
    CHECK_PTR(test_prop, "H5P__mt_search__list");
    assert(test_prop);
    assert(atomic_load(&(test_prop->tag)) == H5P_MT_PROP_TAG);

    ret = prop_check(test_prop, list_prop_table[1].prop, TRUE, FALSE);
    CHECK_I(ret, "H5P__mt_search__list");

    VERIFY(atomic_load(&(test_prop->ref_count)), 1, "H5P__mt_create_list");

    /* Search for and check prop3 */
    test_prop = H5P__mt_search__list(list1, list_prop_table[2].name);
    CHECK_PTR(test_prop, "H5P__mt_search__list");
    assert(test_prop);
    assert(atomic_load(&(test_prop->tag)) == H5P_MT_PROP_TAG);

    ret = prop_check(test_prop, list_prop_table[2].prop, TRUE, FALSE);
    CHECK_I(ret, "H5P__mt_search__list");

    VERIFY(atomic_load(&(test_prop->ref_count)), 1, "H5P__mt_create_list");

    /**
     * From the property free list, modify the head prop's tag to be reallocable,
     * and create and insert a new prop into list1. This is only done during the
     * single thread tests simple to test that the property free list can be
     * allocated from.
     */
    if (test_params->num_threads == 1) {
        prop_fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));
        test_prop    = prop_fl_head.ptr;

        assert(test_prop);
        assert(atomic_load(&(test_prop->tag)) == H5P_MT_PROP_VALID_ONFL_TAG);

        atomic_store(&(test_prop->tag), H5P_MT_PROP_FL_REALLOC_TAG);
    }

    table_prop  = list_prop_table[3].prop;
    table_value = atomic_load(&(table_prop->value));

    /* Create and insert a new property */
    ret = H5P__mt_ins_or_mod_prop__list(list1, table_prop->name, table_value.ptr, table_value.size, FALSE,
                                        FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Check list1's fields */
    ret = list_ver_and_len_check(list1, 2, 3, 3, 1, 4, 1, 3, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Update the list's table_prop create_version to match */
    atomic_store(&(table_prop->create_version), atomic_load(&(list1->curr_version)));

    /**
     * Search for the new prop4 in list1 which tests the property
     * not being in the lkup_tbl and having to search the LFSLL
     */

    test_prop = H5P__mt_search__list(list1, table_prop->name);
    CHECK_PTR(test_prop, "H5P__mt_search__list");

    ret = prop_check(test_prop, table_prop, table_prop->in_prop_class, table_prop->in_lkup_tbl);
    CHECK_I(ret, "H5P__mt_search__list");

    if (test_params->num_threads == 1) {
        /**
         * Ensure the property free list is correct after reallocating a
         * property structure from it.
         */

        VERIFY(atomic_load(&(H5P_mt_g.prop_fl_len)), 6, "H5P__mt_ins_or_mod_prop__list");

        prop_fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));
        test_prop    = prop_fl_head.ptr;

        assert(!test_prop->sentinel);
    }

    /**
     * Modify an inherited property
     */
    table_prop  = get_table_prop_ver(list_prop_table[0], 2);
    table_value = atomic_load(&(table_prop->value));

    assert(table_prop->chksum == list_prop_table[0].chksum);

    ret = H5P__mt_ins_or_mod_prop__list(list1, table_prop->name, table_value.ptr, table_value.size, FALSE,
                                        FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Ensure list1's fields are correct */
    ret = list_ver_and_len_check(list1, 3, 4, 3, 1, 4, 2, 4, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Update the table_prop create_verson to match */
    atomic_store(&(table_prop->create_version), atomic_load(&(list1->curr_version)));

    /**
     * Search for the modified prop1, which tests searching for a prop in
     * the lkup_tbl with the most recent version being an entry's curr
     */
    test_prop = H5P__mt_search__list(list1, table_prop->name);
    CHECK_PTR(test_prop, "H5P__mt_search__list");

    ret = prop_check(test_prop, table_prop, table_prop->in_prop_class, table_prop->in_lkup_tbl);
    CHECK_I(ret, "H5P__mt_search__list");

    /**
     * Modify prop1 again to test that the lkup_tbl's entry correctly
     * updates the curr field.
     */
    table_prop  = get_table_prop_ver(list_prop_table[0], 3);
    table_value = atomic_load(&(table_prop->value));

    assert(table_prop->chksum == list_prop_table[0].chksum);

    ret = H5P__mt_ins_or_mod_prop__list(list1, table_prop->name, table_value.ptr, table_value.size, FALSE,
                                        FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_porp__list");

    /* Ensure list1's fields are correct */
    ret = list_ver_and_len_check(list1, 4, 5, 3, 1, 4, 2, 5, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P_mt_ins_or_mod_prop__list");

    /* Update the table_prop create_version to match */
    atomic_store(&(table_prop->create_version), atomic_load(&(list1->curr_version)));

    /**
     * Search for the modified prop1, tests same as
     * previous but ensures curr was updated correctly.
     */

    test_prop = H5P__mt_search__list(list1, table_prop->name);
    CHECK_PTR(test_prop, "H5P__mt_search__list");

    ret = prop_check(test_prop, table_prop, table_prop->in_prop_class, table_prop->in_lkup_tbl);
    CHECK_I(ret, "H5P__mt_search__list");

    /**
     * Delete a property in the lkup_tbl where the most recent
     * version is the base.
     */

    /* Set the base_delete_version for prop3's lkup_tbl entry */
    ret = H5P__mt_delete_prop__list(list1, list_prop_table[2].name);
    CHECK_I(ret, "H5P__mt_delete_prop__list");

    /* Ensure list1's fields are correct */
    ret = list_ver_and_len_check(list1, 5, 6, 3, 1, 3, 2, 5, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_delete_prop__list");

    /* Ensure the entry's base_delete_version is set */
    entry = &list1->lkup_tbl[2];
    VERIFY(atomic_load(&(entry->base_delete_version)), atomic_load(&(list1->curr_version)),
           "H5P__mt_delete_prop__list");
    assert(atomic_load(&(entry->base_delete_version)) == atomic_load(&(list1->curr_version)));

    /**
     * Search for deleted prop3. Should FAIL.
     */

    test_prop = H5P__mt_search__list(list1, list_prop_table[2].name);
    CHECK_PTR_NULL(test_prop, "H5P__mt_search__list");

    /**
     * Delete a property in the lkup_tbl where the most recent
     * version is curr.
     */
    ret = H5P__mt_delete_prop__list(list1, list_prop_table[0].name);
    CHECK_I(ret, "H5P__mt_delete_prop__list");

    /* Ensure list1's fields are correct */
    ret = list_ver_and_len_check(list1, 6, 7, 3, 1, 2, 1, 5, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_delete_prop__list");

    /**
     * Search for deleted prop1. Should FAIL.
     */
    test_prop = H5P__mt_search__list(list1, list_prop_table[0].name);
    CHECK_PTR_NULL(test_prop, "H5P__mt_search__list");

    /**
     * Delete an added property (non-inherited) from list1.
     */
    ret = H5P__mt_delete_prop__list(list1, list_prop_table[3].name);
    CHECK_I(ret, "H5P__mt_delete_prop__list");

    /* Ensure list1's fields are correct */
    ret = list_ver_and_len_check(list1, 7, 8, 3, 0, 1, 0, 5, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_delete_prop__list");

    /**
     * Search for deleted prop4. Should FAIL.
     */
    test_prop = H5P__mt_search__list(list1, list_prop_table[3].name);
    CHECK_PTR_NULL(test_prop, "H5P__mt_search__list");

    /**
     * Right now list1 doesn't have any valid properties in the LFSLL.
     * Add a new one, and add a new version of prop3, an inherited
     * property that was deleted, for further testing.
     */

    table_prop  = get_table_prop_ver(list_prop_table[3], 2);
    table_value = atomic_load(&(table_prop->value));

    /* Create and insert a new version of deleted prop4 */
    ret = H5P__mt_ins_or_mod_prop__list(list1, table_prop->name, table_value.ptr, table_value.size, FALSE,
                                        FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Check list1's fields */
    ret = list_ver_and_len_check(list1, 8, 9, 3, 1, 2, 1, 6, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Update the table_prop's create_version to match */
    atomic_store(&(table_prop->create_version), atomic_load(&(list1->curr_version)));

    /**
     * Search for the new prop4 in list1
     */
    test_prop = H5P__mt_search__list(list1, table_prop->name);
    CHECK_PTR(test_prop, "H5P__mt_search__list");

    ret = prop_check(test_prop, table_prop, table_prop->in_prop_class, table_prop->in_lkup_tbl);
    CHECK_I(ret, "H5P__mt_search__list");

    /* Create and insert a new version of the deleted inherited prop3 */
    table_prop  = get_table_prop_ver(list_prop_table[2], 2);
    table_value = atomic_load(&(table_prop->value));

    ret = H5P__mt_ins_or_mod_prop__list(list1, table_prop->name, table_value.ptr, table_value.size, FALSE,
                                        FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Check list1's fields */
    ret = list_ver_and_len_check(list1, 9, 10, 3, 1, 3, 2, 7, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Update the table_prop's create_version to match */
    atomic_store(&(table_prop->create_version), atomic_load(&(list1->curr_version)));

    /**
     * Search for the new prop3 in list1
     */
    test_prop = H5P__mt_search__list(list1, table_prop->name);
    CHECK_PTR(test_prop, "H5P__mt_search__list");

    /**
     * Walk the entire lkup_tbl ensuring everything is correct.
     */

    ret = list_lkup_tbl_check(list1, list1->nprops_inherited, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /**
     * Walk list1's LFSLL ensuring all prop structs are correct and in order.
     */

    test_prop = list1->pl_head;

    ret = sentinel_check(test_prop);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    ret = compare_lfsll_to_table_props(&test_prop, list_prop_table, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");
    assert(test_prop->sentinel);

    ret = sentinel_check(test_prop);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Insert list1 into test_params list LFSLL */
    test_params->test_lists_head.ptr = list1;
    test_params->num_lists++;

    return (ret_value);

} /* end test_h5p_mt_list_1() */

/****************************************************************************************
 * Function:    test_h5p_mt_list_2
 *
 * Purpose:     Further tests the multithread list structure, H5P_mt_list_t, and the
 *              functions for copying a list, comparison of two lists, closing a list,
 *              and reallocating a closed instance of a list structure from the list
 *              free list to be used for a new list. After each of the steps the lists
 *              and properties affected are checked to ensure their fields were modified
 *              (or not modifed) correctly.
 *
 *              NOTE: some sections are only performed if there is only one thread
 *              running the test.
 *
 * Details:
 *
 *  1) Create list2 as a copy of list1, check class1's and class1's properties'
 *     ref_counts.
 *  2) Compare list1 and list2 (should be equal).
 *  3) Modify a property in list2's LFSLL and compare list1 and list2 again (should not
 *     be equal).
 *  4) Modify the property in list2 back and compare list1 and list2 again (should be
 *     equal).
 *  3) Modify a property in list2's lkup_tbl and compare list1 and list2 again (should
 *     not be equal).
 *  4) Modify the property in list2 back and compare list1 and list2 again (should be
 *     equal).
 *  5) Close list2 and ensure it was inserted into the list free list correctly.
 *  6) Change the closed list2's tag to be reallocable and derive a new list3 from
 *     class2.
 *     NOTE: step 6 and 7 are only done when testing with a single thread. If testing
 *     with multiple threads a new structure is allocated from the heap.
 *  7) Ensure the new list3 used the old list2 structure from the list free list and that
 *     the list free list is now empty (NOTE: the list free list will always contain two
 *     H5P_mt_list_sptr_t structs for the head and tail of that list, and if the pointers
 *     are NULL then the free list is "empty").
 *  8) Check the property free list and ensure all the H5P_mt_prop_t, property structs,
 *     where correctly inserted, when the list2 struct was reallocated from the list
 *     free list.
 *     NOTE: step 8 is only done when testing with a single thread.
 *
 *
 * Return:      SUCCESS/FAIL
 *
 ****************************************************************************************
 */
static herr_t
test_h5p_mt_list_2(test_params_t *test_params)
{
    H5P_mt_class_t     *test_root;
    H5P_mt_class_t     *class1;
    H5P_mt_class_t     *class2;
    H5P_mt_class_sptr_t class_next;
    H5P_mt_list_t      *list1;
    H5P_mt_list_t      *list2;
    H5P_mt_list_t      *list3;
    H5P_mt_list_t      *fl_list;
    H5P_mt_list_sptr_t  fl_head;
    H5P_mt_list_sptr_t  fl_tail;
    H5P_mt_list_sptr_t  list_next;
    H5P_mt_prop_t      *test_prop;
    H5P_mt_prop_t      *table_prop;
    H5P_mt_prop_t      *fl_prop;
    H5P_mt_prop_value_t table_value;
    H5P_mt_prop_aptr_t  prop_fl_head;
    H5P_mt_prop_aptr_t  prop_fl_tail;
    H5P_mt_prop_aptr_t  prop_fl_next;
    herr_t              ret; /* Generic return value */

    herr_t ret_value = SUCCEED;

    /* Get class1 from the LFSLL of test classes */
    class1 = test_params->test_classes_head.ptr;

    assert(class1);
    assert(atomic_load(&(class1->tag)) == H5P_MT_CLASS_TAG);
    assert(0 == strcmp(class1->name, CLASS1_NAME));

    /* Get the test root class from class1 */
    test_root = class1->parent_ptr;

    assert(test_root);
    assert(atomic_load(&(test_root->tag)) == H5P_MT_CLASS_TAG);
    assert(0 == strcmp(test_root->name, TEST_ROOT_NAME));

    /* Get class2 from the LFSLL of test classes */
    class_next = atomic_load(&(class1->fl_next));
    class2     = class_next.ptr;

    assert(class2);
    assert(atomic_load(&(class2->tag)) == H5P_MT_CLASS_TAG);
    assert(0 == strcmp(class2->name, CLASS2_NAME));

    /* Get list1 from the LFSLL of test lists */
    list1 = test_params->test_lists_head.ptr;

    assert(list1);
    assert(atomic_load(&(list1->tag)) == H5P_MT_LIST_TAG);
    assert(list1->pclass_ptr == class1);

    /**
     * Create a copy of list1, check class1's ref_counts
     */

    list2 = H5P__mt_create_list(class1, list1, TRUE, 0, TRUE);
    CHECK_PTR(list2, "H5P__mt_create_list");

    assert(list2);
    assert(atomic_load(&(list2->tag)) == H5P_MT_LIST_TAG);

    /* Ensure list2's fields are correct */
    ret = list_ver_and_len_check(list2, 1, 2, 3, 1, 3, 2, 4, "H5P__mt_create_list");
    CHECK_I(ret, "H5P__mt_create_list");

    /* Check class1's ref_counts */
    ret = check_class_ref_counts(class1, 2, 1, FALSE, "H5P__mt_create_list");
    CHECK_I(ret, "H5P__mt_create_list");

    /* Check list2's thrd flags */
    ret = check_and_set_thrd_flags(list2, FALSE, FALSE, FALSE, FALSE, "H5P__mt_create_list");
    CHECK_I(ret, "H5P__mt_create_list");

    /**
     * Search for and check prop1, which was deleted from list1
     * before list2 was copied, thus prop1 shouldn't exist in list2.
     */

    test_prop = H5P__mt_search__list(list2, list2_prop_table[0].name);
    CHECK_PTR_NULL(test_prop, "H5P__mt_search__list");

    /* Search for and check prop2 */
    table_prop = list2_prop_table[1].prop;

    test_prop = H5P__mt_search__list(list2, table_prop->name);
    CHECK_PTR(test_prop, "H5P__mt_search__list");
    assert(test_prop);
    assert(atomic_load(&(test_prop->tag)) == H5P_MT_PROP_TAG);

    ret = prop_check(test_prop, table_prop, table_prop->in_prop_class, table_prop->in_lkup_tbl);
    CHECK_I(ret, "H5P__mt_search__list");

    VERIFY(atomic_load(&(test_prop->ref_count)), 2, "H5P__mt_create_list");

    /* Search for and check prop3 */
    table_prop = list2_prop_table[2].prop;

    test_prop = H5P__mt_search__list(list2, table_prop->name);
    CHECK_PTR(test_prop, "H5P__mt_search__list");
    assert(test_prop);
    assert(atomic_load(&(test_prop->tag)) == H5P_MT_PROP_TAG);

    ret = prop_check(test_prop, table_prop, table_prop->in_prop_class, table_prop->in_lkup_tbl);
    CHECK_I(ret, "H5P__mt_search__list");

    /**
     * prop3 was modified in list1, so search prop3 in class1 to
     * ensure the property's ref_count is corret.
     */
    table_prop = class_prop_table[2].prop;

    test_prop = H5P__mt_search__class(class1, table_prop->name);
    CHECK_PTR(test_prop, "H5P__mt_search__list");
    assert(test_prop);
    assert(atomic_load(&(test_prop->tag)) == H5P_MT_PROP_TAG);

    /* The base was deleted when copied from list1, thus ref_count should be unchaged */
    VERIFY(atomic_load(&(test_prop->ref_count)), 1, "H5P__mt_create_list");

    /**
     * Compare list1 and the copy, list2. (They should be equal)
     */
    ret = H5P__mt_cmp_list(list1, list2);
    CHECK_I(ret, "H5P__mt_cmp_list");
    VERIFY(ret, 0, "H5P__mt_cmp_list");

    /**
     * Modify a property in list2's LFSLL, and compare list1 and list2 again.
     * (They should not be equal)
     */

    table_prop  = get_table_prop_ver(list2_prop_table[3], 2);
    table_value = atomic_load(&(table_prop->value));

    /* Create and insert a second version of prop 4 */
    ret = H5P__mt_ins_or_mod_prop__list(list2, table_prop->name, table_value.ptr, table_value.size, FALSE,
                                        FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Update table_prop version to match */
    atomic_store(&(table_prop->create_version), atomic_load(&(list2->curr_version)));

    /* Ensure list2's fields are correct */
    ret = list_ver_and_len_check(list2, 2, 3, 3, 1, 3, 2, 5, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Compare list1 and list2 (should not be equal) */
    ret = H5P__mt_cmp_list(list1, list2);
    VERIFY(ret, 1, "H5P__mt_cmp_list");

    /**
     * Change the modified property's value back and compare
     * list1 and list2 again. (Should be equal)
     */

    table_prop  = get_table_prop_ver(list2_prop_table[3], 3);
    table_value = atomic_load(&(table_prop->value));

    /* Create and insert prop3 with original value */
    ret = H5P__mt_ins_or_mod_prop__list(list2, table_prop->name, table_value.ptr, table_value.size, FALSE,
                                        FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Update table_prop version to match */
    atomic_store(&(table_prop->create_version), atomic_load(&(list2->curr_version)));

    /* Ensure list2's fields are correct */
    ret = list_ver_and_len_check(list2, 3, 4, 3, 1, 3, 2, 6, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Compare list1 and list2 (Should be equal) */
    ret = H5P__mt_cmp_list(list1, list2);
    VERIFY(ret, 0, "H5P__mt_cmp_list");

    /**
     * Modify a property in list2's lkup_tbl and compare list1 and list2.
     * (Should not be equal).
     */

    table_prop = get_table_prop_ver(list2_prop_table[1], 2);
    CHECK_PTR(table_prop, "get_table_prop_ver");
    table_value = atomic_load(&(table_prop->value));

    /* Create and insert a second version of prop2 */
    ret = H5P__mt_ins_or_mod_prop__list(list2, table_prop->name, table_value.ptr, table_value.size, FALSE,
                                        FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Update table_prop version to match */
    atomic_store(&(table_prop->create_version), atomic_load(&(list2->curr_version)));

    /* Ensure list2's fields are correct */
    ret = list_ver_and_len_check(list2, 4, 5, 3, 1, 3, 3, 7, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Compare list1 and list2 (should not be equal) */
    ret = H5P__mt_cmp_list(list1, list2);
    VERIFY(ret, 1, "H5P__mt_cmp_list");

    /**
     * Change the modified property's value back and compare list1, and list2 again.
     * (Should be equal).
     */

    table_prop = get_table_prop_ver(list2_prop_table[1], 3);
    CHECK_PTR(table_prop, "get_table_prop_ver");
    table_value = atomic_load(&(table_prop->value));

    /* Create and insert prop2 with original value */
    ret = H5P__mt_ins_or_mod_prop__list(list2, table_prop->name, table_value.ptr, table_value.size, FALSE,
                                        FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Update table_prop version to match */
    atomic_store(&(table_prop->create_version), atomic_load(&(list2->curr_version)));

    /* Ensure list2's fields are correct */
    ret = list_ver_and_len_check(list2, 5, 6, 3, 1, 3, 3, 8, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Compare list1 and list2 (Should be equal) */
    ret = H5P__mt_cmp_list(list1, list2);
    VERIFY(ret, 0, "H5P__mt_cmp_list");

    /**
     * Close list2, check class1's derived ref counts, and ensure list2 was
     * inserted into the list free list.
     */

    ret = H5Pclose(atomic_load(&(list2->plist_id)));
    CHECK_I(ret, "H5Pclose");

    /* Check class1's ref_counts */
    ret = check_class_ref_counts(class1, 1, 1, FALSE, "H5Pclose");
    CHECK_I(ret, "H5Pclose");

    /* Check the list free list to ensure list2 was inserted correctly when closed */
    if (test_params->num_threads == 1) {
        fl_head = atomic_load(&(H5P_mt_g.list_fl_head));
        fl_list = fl_head.ptr;

        assert(fl_list);
        assert(atomic_load(&(fl_list->tag)) == H5P_MT_LIST_INVALID_TAG);
        VERIFY(atomic_load(&(fl_list->tag)), H5P_MT_LIST_INVALID_TAG, "H5Pclose");

        /* Ensure head_list's thrd flags are correct */
        ret = check_and_set_thrd_flags(fl_list, FALSE, TRUE, FALSE, TRUE, "H5Pclose");
        CHECK_I(ret, "H5Pclose");

        /**
         * Ensure list2 (aka head_list) is the head and tail
         * since it's the only list on the list free list.
         */
        fl_tail = atomic_load(&(H5P_mt_g.list_fl_tail));
        VERIFY(fl_tail.ptr, fl_list, "H5Pclose");
        assert(fl_tail.ptr == fl_list);

        /**
         * From the list free list, modify the closed list's tag to be reallocable
         * and derive a new list3 from class2 (should allocate the H5P_mt_list_t
         * structure from the list free list)
         */
        atomic_store(&(fl_list->tag), H5P_MT_LIST_FL_REALLOC_TAG);
    }

    list3 = H5P__mt_create_list(class2, NULL, FALSE, 0, TRUE);
    CHECK_PTR(list3, "H5P__mt_create_list");
    assert(list3);
    assert(atomic_load(&(list3->tag)) == H5P_MT_LIST_TAG);

    /* Ensure test_root's fields did not change */
    if (test_params->num_threads == 1) {
        ret = check_class_ref_counts(test_root, 0, 1, FALSE, "H5P__mt_create_list");
        CHECK_I(ret, "H5P__mt_create_list");
    }

    /* Ensure class1's fields did not change */
    ret = check_class_ref_counts(class1, 1, 1, FALSE, "H5P__mt_create_list");
    CHECK_I(ret, "H5P__mt_create_list");

    /* Ensure class2's fields are correct */
    ret = check_class_ref_counts(class2, 1, 0, FALSE, "H5P__mt_create_list");
    CHECK_I(ret, "H5P__mt_create_list");

    if (test_params->num_threads == 1) {
        /* Ensure the list free list is now empty */
        fl_head = atomic_load(&(H5P_mt_g.list_fl_head));
        fl_tail = atomic_load(&(H5P_mt_g.list_fl_tail));

        assert(!fl_head.ptr);
        assert(!fl_tail.ptr);
        VERIFY(fl_head.ptr, NULL, "H5P__mt_create_list");
        VERIFY(fl_tail.ptr, NULL, "H5P__mt_create_list");

        /**
         * Ensure the property structs in list2 were correctly inserted into the prop
         * free list when list2 was reallocated from the list free list.
         */
        prop_fl_head = atomic_load(&(H5P_mt_g.prop_fl_head));
        prop_fl_tail = atomic_load(&(H5P_mt_g.prop_fl_tail));

        fl_prop = prop_fl_head.ptr;

        assert(fl_prop);
        assert(atomic_load(&(fl_prop->tag)) == H5P_MT_PROP_VALID_ONFL_TAG);

        VERIFY(atomic_load(&(H5P_mt_g.prop_fl_len)), 14, "H5P__mt_create_list");

        assert(!fl_prop->sentinel);

        /**
         * Walks the property free list and ensures the properties are correct until a
         * sentinel prop is found. When a class or list is closed the properties are
         * inserted into the free in the same order so the sentinel property is the last
         * property from the previously closed class2.
         */
        ret = compare_lfsll_to_table_props(&fl_prop, class2_prop_table, "H5P__mt_create_list");
        CHECK_I(ret, "H5P__mt_create_list");
        VERIFY(fl_prop->sentinel, TRUE, "H5P__mt_create_list");

        /* Ensure the sentinel prop tge free list stopped at is correct */
        ret = sentinel_check(fl_prop);
        CHECK_I(ret, "H5P__mt_create_list");

        /* Iterate to the next property in the free list, which is list2's neg_sentinel */
        prop_fl_next = atomic_load(&(fl_prop->next));
        fl_prop      = prop_fl_next.ptr;

        ret = sentinel_check(fl_prop);
        CHECK_I(ret, "H5P__mt_create_list");

        /**
         * Continue walking the property free list, now
         * checking the properties from the closed list2.
         */
        ret = compare_lfsll_to_table_props(&fl_prop, list2_prop_table, "H5P__mt_create_list");
        CHECK_I(ret, "H5P__mt_create_list");
        VERIFY(fl_prop->sentinel, TRUE, "H5P__mt_create_list");

        /* Finally check list2's pos_sentinel, which should be the tail of the free list */
        ret = sentinel_check(fl_prop);
        CHECK_I(ret, "H5P__mt_create_list");
        VERIFY(fl_prop, prop_fl_tail.ptr, "H5P__mt_create_list");
        assert(fl_prop == prop_fl_tail.ptr);
    }

    /* Insert list1 into test_params list LFSLL */
    test_params->num_lists++;

    list_next = atomic_load(&(list1->fl_next));
    assert(!list_next.ptr);

    list_next.ptr = list3;
    atomic_store(&(list1->fl_next), list_next);

    return (ret_value);

} /* end test_h5p_mt_list_2() */

/****************************************************************************************
 * Function:    get_table_prop_ver
 *
 * Purpose:     Searches the prop_table to grab the version of the property specified,
 *              and returns that version.
 *
 *
 * Return:      Success: Specified version of a property from the prop_table
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
get_table_prop_ver(prop_info_t prop_table, uint8_t version)
{
    H5P_mt_prop_t     *table_prop;
    H5P_mt_prop_aptr_t next;

    H5P_mt_prop_t *ret_value = NULL;

    table_prop = prop_table.prop;
    assert(table_prop);
    assert(atomic_load(&(table_prop->tag)) == H5P_MT_PROP_TAG);

    for (int i = 1; i < version; i++) {
        next       = atomic_load(&(table_prop->next));
        table_prop = next.ptr;

        assert(table_prop);
        assert(atomic_load(&(table_prop->tag)) == H5P_MT_PROP_TAG);
    }

    VERIFY(table_prop->chksum, prop_table.chksum, "get_table_prop_ver");

    ret_value = table_prop;

    return (ret_value);

} /* end get_table_prop_ver() */

/****************************************************************************************
 * Function:    class_ver_and_len_check
 *
 * Purpose:     Verifies that the curr_version, next_version, and the fields counting
 *              properties are all correct for an instance of a class.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
class_ver_and_len_check(H5P_mt_class_t *class, uint64_t curr_version, uint64_t next_version,
                        size_t nprops_added, size_t log_len, size_t phys_len, const char *where)
{

    herr_t ret_value = SUCCEED;

    assert(class);
    assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);

    VERIFY(curr_version, atomic_load(&(class->curr_version)), where);
    assert(curr_version == atomic_load(&(class->curr_version)));
    VERIFY(next_version, atomic_load(&(class->next_version)), where);
    assert(next_version == atomic_load(&(class->next_version)));
    VERIFY(nprops_added, atomic_load(&(class->nprops_added)), where);
    assert(nprops_added == atomic_load(&(class->nprops_added)));
    VERIFY(log_len, atomic_load(&(class->log_pl_len)), where);
    assert(log_len == atomic_load(&(class->log_pl_len)));
    VERIFY(phys_len, atomic_load(&(class->phys_pl_len)), where);
    assert(phys_len == atomic_load(&(class->phys_pl_len)));

    return (ret_value);

} /* end class_ver_and_len_check() */

/****************************************************************************************
 * Function:    list_ver_and_len_check
 *
 * Purpose:     Verifies that the curr_version, next_version, and the fields counting
 *              properties are all correct for an instance of a list.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
list_ver_and_len_check(H5P_mt_list_t *list, uint64_t curr_version, uint64_t next_version,
                       size_t nprops_inherited, size_t nprops_added, size_t nprops, size_t log_len,
                       size_t phys_len, const char *where)
{
    herr_t ret_value = SUCCEED;

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

    VERIFY(atomic_load(&(list->curr_version)), curr_version, where);
    assert(curr_version == atomic_load(&(list->curr_version)));
    VERIFY(atomic_load(&(list->next_version)), next_version, where);
    assert(next_version == atomic_load(&(list->next_version)));
    VERIFY(list->nprops_inherited, nprops_inherited, where);
    assert(nprops_inherited == list->nprops_inherited);
    VERIFY(atomic_load(&(list->nprops_added)), nprops_added, where);
    assert(nprops_added == atomic_load(&(list->nprops_added)));
    VERIFY(atomic_load(&(list->nprops)), nprops, where);
    assert(nprops == atomic_load(&(list->nprops)));
    VERIFY(atomic_load(&(list->log_pl_len)), log_len, where);
    assert(log_len == atomic_load(&(list->log_pl_len)));
    VERIFY(atomic_load(&(list->phys_pl_len)), phys_len, where);
    assert(phys_len == atomic_load(&(list->phys_pl_len)));

    return (ret_value);

} /* end list_ver_and_len_check() */

/****************************************************************************************
 * Function:    check_class_ref_counts
 *
 * Purpose:     Verifies that a class's list ref counts (pl), class ref counts (plc), and
 *              the class's deleted flag are all correct for an instance of a class.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
check_class_ref_counts(H5P_mt_class_t *class, uint64_t pl, uint32_t plc, bool deleted, const char *where)
{
    H5P_mt_class_ref_counts_t ref_counts;

    herr_t ret_value = SUCCEED;

    ref_counts = atomic_load(&(class->ref_count));
    VERIFY(ref_counts.pl, pl, where);
    assert(ref_counts.pl == pl);
    VERIFY(ref_counts.plc, plc, where);
    assert(ref_counts.plc == plc);
    VERIFY(ref_counts.deleted, deleted, where);
    assert(deleted == ref_counts.deleted);

    return (ret_value);

} /* end check_class_ref_counts() */

/****************************************************************************************
 * Function:    check_and_set_thrd_flags
 *
 * Purpose:     Verifies that a class's, or list's, thrd flags are correct, and sets them
 *              if they are correct and need to be changed.
 *
 *              NOTE: setting the flags is only used for classes after a class is
 *              inserted into the index. All other instances where the thrd flags are
 *              changed are done in the multithread H5P functions.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
check_and_set_thrd_flags(void *param, bool opening_is, bool closing_is, bool set_opening, bool set_closing,
                         const char *where)
{
    uint32_t tag;
    H5P_mt_class_t *class             = NULL;
    H5P_mt_list_t               *list = NULL;
    H5P_mt_active_thread_count_t thrd;

    herr_t ret_value = SUCCEED;

    tag = *(uint32_t *)param;

    /* If param is a class */
    if (tag == H5P_MT_CLASS_TAG || tag == H5P_MT_CLASS_INVALID_TAG || tag == H5P_MT_CLASS_FL_REALLOC_TAG) {
        class = (H5P_mt_class_t *)param;
    }
    /* If param is a list */
    else if (tag == H5P_MT_LIST_TAG || tag == H5P_MT_LIST_INVALID_TAG || tag == H5P_MT_LIST_FL_REALLOC_TAG) {
        list = (H5P_mt_list_t *)param;
    }
    else {
        TestErrPrintf("%s: param isn't a class or a list\n", where);

        assert(class);

        return (-1);
    }

    /* Get the thrd struct field from the struct we have */
    if (class) {
        thrd = atomic_load(&(class->thrd));
    }
    else {
        thrd = atomic_load(&(list->thrd));
    }

    /* Ensure thrd fields are as they should be */
    VERIFY(thrd.opening, opening_is, where);
    assert(thrd.opening == opening_is);
    VERIFY(thrd.closing, closing_is, where);
    assert(thrd.closing == closing_is);

    /* Update thrd fields */

    /* If the set flags are different from current flags, then update the flags */
    if (opening_is != set_opening || closing_is != set_closing) {
        thrd.opening = set_opening;
        thrd.closing = set_closing;

        if (class) {
            atomic_store(&(class->thrd), thrd);
        }
        else {
            atomic_store(&(list->thrd), thrd);
        }
    }

    return (ret_value);

} /* end check_and_set_thrd_flags() */

/****************************************************************************************
 * Function:    compare_lfsll_to_table_props
 *
 * Purpose:     Iterates a LFSLL and compares the properties in it with the properties
 *              in a prop_table.
 *
 *              NOTE: it is assumed the parameter test_prop is a sentinel node and will
 *              iterate to the next prop in the LFSLL before comparing.
 *
 *              NOTE: this functions is additionally used for checking the property free
 *              list, but due to stopping when a sentinel node is reached, during
 *              test_h5p_mt_list_2() it is ran twice so it can continue after
 *              encountering the sentinel nodes in the middle of the free list.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
compare_lfsll_to_table_props(H5P_mt_prop_t **test_prop, prop_info_t *prop_table, const char *where)
{
    H5P_mt_prop_t     *prop;
    H5P_mt_prop_t     *table_prop = NULL;
    H5P_mt_prop_aptr_t next;
    H5P_mt_prop_aptr_t table_next;
    int                table_i;
    uint64_t           create_version;
    uint64_t           delete_version;
    herr_t             ret; /* Generic return value */

    herr_t ret_value = SUCCEED;

    assert(test_prop);
    assert(where);

    prop = *test_prop;
    assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG ||
           atomic_load(&(prop->tag)) == H5P_MT_PROP_VALID_ONFL_TAG);

    table_i = 0;

    next = atomic_load(&(prop->next));
    prop = next.ptr;

    while (!prop->sentinel) {
        if (prop->chksum > prop_table[table_i].chksum) {
            table_i++;

            continue;
        }

        table_prop = prop_table[table_i].prop;

        create_version = atomic_load(&(prop->create_version));
        delete_version = atomic_load(&(prop->delete_version));

        assert(prop->chksum == table_prop->chksum);

        table_prop = get_correct_prop_version_from_table(table_prop, create_version, delete_version, where);
        CHECK_PTR(table_prop, where);
        assert(table_prop);
        assert(atomic_load(&(table_prop->tag)) == H5P_MT_PROP_TAG ||
               atomic_load(&(table_prop->tag)) == H5P_MT_PROP_VALID_ONFL_TAG);

        if (prop->in_lkup_tbl != table_prop->in_lkup_tbl) {
            table_next = atomic_load(&(table_prop->next));
            table_prop = table_next.ptr;
        }

        ret = prop_check(prop, table_prop, table_prop->in_prop_class, table_prop->in_lkup_tbl);
        CHECK_I(ret, where);

        next = atomic_load(&(prop->next));
        prop = next.ptr;
        assert(prop);
        assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG ||
               atomic_load(&(prop->tag)) == H5P_MT_PROP_VALID_ONFL_TAG);

    } /* end while ( ! test_prop->sentinel ) */

    *test_prop = prop;

    return (ret_value);

} /* end compare_lfsll_to_table_props() */

/****************************************************************************************
 * Function:    list_lkup_tbl_check
 *
 * Purpose:     Iterates a list's lkup_tbl and verifies the fields are correct in each
 *              entry and are in the correct order.
 *
 *              If an entry has a curr.ptr it ensures the curr fields are correct and
 *              the property that curr.ptr points to is the most recent version. If that
 *              property's version differs from first_ver_of_curr, the LFSLL is iterated
 *              to find the first version of that property in the LFSLL and ensure the
 *              version matches with first_ver_of_curr.
 *
 *              If an entry has a base.ptr it ensures base fields are correct and that
 *              the property that base.ptr points to is correct.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
list_lkup_tbl_check(H5P_mt_list_t *list, size_t nprops_inherited, const char *where)
{
    H5P_mt_list_table_entry_t *entry;
    H5P_mt_list_prop_ref_t     base;
    H5P_mt_list_prop_ref_t     curr;
    H5P_mt_prop_t             *table_prop;
    H5P_mt_prop_t             *list_prop;
    H5P_mt_prop_t             *test_prop = NULL;
    H5P_mt_prop_t             *next_prop;
    H5P_mt_prop_aptr_t         next;
    size_t                     i;
    uint64_t                   create_version;
    uint64_t                   delete_version;
    herr_t                     ret; /* Generic return value */

    herr_t ret_value = SUCCEED;

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

    /* Iterate the length of the lkup_tbl*/
    for (i = 0; i < nprops_inherited; i++) {
        entry = &list->lkup_tbl[i];
        CHECK_PTR(entry, where);

        /* Grab the table prop for comparison */
        table_prop = list_prop_table[i].prop;
        assert(table_prop);
        assert(atomic_load(&(table_prop->tag)) == H5P_MT_PROP_TAG);

        /* Ensure the entry's chksum and name matches */
        VERIFY(entry->chksum, table_prop->chksum, where);
        assert(entry->chksum == table_prop->chksum);
        if (HDstrcmp(entry->name, table_prop->name) != 0) {
            TestErrPrintf("lkup_tbl entry name mismatch! entry name = %s, table_prop name = %s\n",
                          entry->name, table_prop->name);
        }

        base = atomic_load(&(entry->base));
        curr = atomic_load(&(entry->curr));

        /* If curr.ptr isn't NULL check all related fields */
        if (curr.ptr) {
            /* Ensure curr.ver is the correct version */
            list_prop      = curr.ptr;
            create_version = atomic_load(&(list_prop->create_version));
            delete_version = atomic_load(&(list_prop->delete_version));

            VERIFY(create_version, curr.ver, where);
            assert(create_version == curr.ver);

            next      = atomic_load(&(list_prop->next));
            next_prop = next.ptr;

            /* If curr.ver is the first version of a non-base version */
            if (curr.ver == atomic_load(&(entry->first_ver_of_curr))) {
                /* Ensure the next property in the LFSLL has a different chksum. */
                CHECK(list_prop->chksum, next_prop->chksum, where);
                assert(list_prop->chksum != next_prop->chksum);
            }
            /* If curr.ver is not the first version of a non-base version */
            else {
                /* Iterate the LFSLL to find the oldest version of list_prop */
                while (next_prop->chksum == list_prop->chksum) {
                    test_prop = next_prop;
                    next      = atomic_load(&(next_prop->next));
                    next_prop = next.ptr;
                }

                /* Ensure the first version matches first_ver_of_curr */
                VERIFY(atomic_load(&(entry->first_ver_of_curr)), atomic_load(&(test_prop->create_version)),
                       where);
                assert(atomic_load(&(entry->first_ver_of_curr)) == atomic_load(&(test_prop->create_version)));
            }

            table_prop =
                get_correct_prop_version_from_table(table_prop, create_version, delete_version, where);
            CHECK_PTR(table_prop, where);
            assert(table_prop);
            assert(atomic_load(&(table_prop->tag)) == H5P_MT_PROP_TAG);

            create_version = atomic_load(&(table_prop->create_version));
            delete_version = atomic_load(&(table_prop->delete_version));

            /* Compare the property with the table_prop */
            ret = prop_check(list_prop, table_prop, table_prop->in_prop_class, table_prop->in_lkup_tbl);
            CHECK_I(ret, where);

        } /* end if ( curr.ptr ) */
        if (base.ptr) {
            /* Ensure base.ver is the correct version */
            list_prop = base.ptr;

            VERIFY(base.ver, 1, where);
            assert(base.ver == 1);

            table_prop = list_prop_table[i].prop;

            ret = prop_check(list_prop, table_prop, table_prop->in_prop_class, table_prop->in_lkup_tbl);
            CHECK_I(ret, where);

        } /* end else */

    } /* end for ( i; i < nprops_inherited; i++ ) */

    return (ret_value);

} /* end list_lkup_tbl_check() */

/****************************************************************************************
 * Function:    get_correct_prop_version_from_table
 *
 * Purpose:     Iterates the LFSLL of versions of properties in the test prop_table to
 *              return the correct version needed to compare.
 *
 *
 * Return:      Success: Pointer to correct property version
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
get_correct_prop_version_from_table(H5P_mt_prop_t *table_prop, uint64_t create_ver, uint64_t delete_ver,
                                    const char *where)
{
    H5P_mt_prop_aptr_t next;

    while (create_ver != atomic_load(&(table_prop->create_version))) {
        next = atomic_load(&(table_prop->next));
        assert(next.ptr);

        table_prop = next.ptr;

        assert(table_prop);
        assert(atomic_load(&(table_prop->tag)) == H5P_MT_PROP_TAG);
    }

    VERIFY(delete_ver, atomic_load(&(table_prop->delete_version)), where);
    assert(delete_ver == atomic_load(&(table_prop->delete_version)));

    return (table_prop);

} /* end get_correct_prop_version_from_table() */

/****************************************************************************************
 * Function:    prop_check
 *
 * Purpose:     Verifies that a property is correct by comparing it to the table_prop and
 *              doing additional checks to ensure flags are set correctly.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
prop_check(H5P_mt_prop_t *prop, H5P_mt_prop_t *table_prop, bool in_prop_class, bool in_lkup_tbl)
{
    herr_t ret_value = SUCCEED;

    if (0 != H5P__mt_prop_cmp(prop, table_prop)) {
        assert(atomic_load(&(prop->tag)) == 0);

        fprintf(stderr, "prop_check(): prop mismatch.");
        return -1;
    }

    if (in_prop_class != prop->in_prop_class || in_lkup_tbl != prop->in_lkup_tbl) {
        assert(in_prop_class == prop->in_prop_class);
        assert(in_lkup_tbl == prop->in_lkup_tbl);

        fprintf(stderr, "prop_check(): prop class or lkup_tbl flag mismatch.");
        return -1;
    }

    if (TRUE == prop->sentinel) {
        assert(prop->sentinel == FALSE);

        fprintf(stderr, "prop_check(): prop is marked as sentinel.");
        return -1;
    }

    return (ret_value);

} /* end prop_check() */

/****************************************************************************************
 * Function:    sentinel_check
 *
 * Purpose:     Verifies that a sentinel property is correct
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
sentinel_check(H5P_mt_prop_t *prop)
{
    H5P_mt_prop_value_t value;

    herr_t ret_value = SUCCEED;

    /* Ensures sentinel is marked as sentinel */
    if (!prop->sentinel) {
        assert(prop->sentinel);

        fprintf(stderr, "sentinel_check(): sentinel isn't marked as sentinel.");
        return -1;
    }

    /* Ensures correct chksum */
    if (prop->chksum != LLONG_MIN && prop->chksum != LLONG_MAX) {
        assert(prop->chksum == LLONG_MIN);
        assert(prop->chksum == LLONG_MAX);

        fprintf(stderr, "sentinel_check(): incorrect sentinel chksum.");
        return -1;
    }

    /* Ensures correct name */
    if (0 != strcmp(prop->name, "neg_sentinel") && 0 != strcmp(prop->name, "pos_sentinel")) {
        fprintf(stderr, "sentinel_check(): incorrect sentinel name.");
        return -1;
    }

    value = atomic_load(&(prop->value));

    /* Ensures value size is 0 */
    if (value.size != 0) {
        assert(value.size == 0);

        fprintf(stderr, "sentinel_check(): incorrect sentinel size.");
        return -1;
    }

    /* Ensures value ptr is NULL */
    if (value.ptr) {
        assert(!value.ptr);

        fprintf(stderr, "sentinel_check(): sentinel value not NULL.");
        return -1;
    }

    return (ret_value);

} /* end sentinel_check() */

/****************************************************************************************
 * Function:    check_stats
 *
 * Purpose:     After a thread completes the class and list tests, the thread calls this
 *              function to ensure all of the classes and lists have the correct stats.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
check_stats(test_params_t *test_params)
{
    H5P_mt_class_t     *test_root;
    H5P_mt_class_t     *class1;
    H5P_mt_class_t     *class2;
    H5P_mt_class_sptr_t class_next;
    H5P_mt_list_t      *list1;
    H5P_mt_list_t      *list3;
    H5P_mt_list_sptr_t  list_next;

    herr_t ret_value = SUCCEED;

    /* Get class1 from the LFSLL of test classes */
    class1 = test_params->test_classes_head.ptr;

    assert(class1);
    assert(atomic_load(&(class1->tag)) == H5P_MT_CLASS_TAG);
    assert(0 == strcmp(class1->name, CLASS1_NAME));

    /* Get the test root class from class1 */
    test_root = class1->parent_ptr;

    assert(test_root);
    assert(atomic_load(&(test_root->tag)) == H5P_MT_CLASS_TAG);
    assert(0 == strcmp(test_root->name, TEST_ROOT_NAME));

    /* Get class2 from the LFSLL of test classes */
    class_next = atomic_load(&(class1->fl_next));
    class2     = class_next.ptr;

    assert(class2);
    assert(atomic_load(&(class2->tag)) == H5P_MT_CLASS_TAG);
    assert(0 == strcmp(class2->name, CLASS2_NAME));

    /* Get list1 from the LFSLL of test lists */
    list1 = test_params->test_lists_head.ptr;

    assert(list1);
    assert(atomic_load(&(list1->tag)) == H5P_MT_LIST_TAG);
    assert(list1->pclass_ptr == class1);

    /* Get list3 from the LFSLL of test lists */
    list_next = atomic_load(&(list1->fl_next));
    list3     = list_next.ptr;

    assert(list3);
    assert(atomic_load(&(list3->tag)) == H5P_MT_LIST_TAG);
    assert(list3->pclass_ptr == class2);

    /**
     * Check class1's stats
     */

    /* Insert stats  */
    assert(atomic_load(&(class1->H5P__insert_prop_class__num_calls)) == 5);
    assert(atomic_load(&(class1->insert_max_nodes_visited)) == 5);
    assert(atomic_load(&(class1->insert_avg_nodes_visited)) == 4);
    assert(atomic_load(&(class1->num_insert_nodes_visited)) > 0);
    assert(atomic_load(&(class1->num_insert_prop__cols)) == 0);
    assert(atomic_load(&(class1->num_insert_prop__success)) == 5);
    assert(atomic_load(&(class1->num_insert_prop__chksum_cols)) == 0);

    /* Delete stats */
    assert(atomic_load(&(class1->H5P__delete_prop__class__num_calls)) == 3);
    assert(atomic_load(&(class1->set_delete__max_nodes_visited)) == 5);
    assert(atomic_load(&(class1->set_delete__avg_nodes_visited)) == 4);
    assert(atomic_load(&(class1->num_set_delete__nodes_visited)) > 0);
    assert(atomic_load(&(class1->num_set_delete__cols)) == 0);
    assert(atomic_load(&(class1->num_set_delete__success)) == 3);
    assert(atomic_load(&(class1->num_set_delete_chksum_cols)) == 0);

    /* Search stats */
    assert(atomic_load(&(class1->H5P__search_prop__class__num_calls)) == 9);
    assert(atomic_load(&(class1->search_class__max_nodes_visited)) == 6);
    assert(atomic_load(&(class1->search_class__avg_nodes_visited)) == 3);
    assert(atomic_load(&(class1->num_search_class__nodes_visited)) > 0);
    assert(atomic_load(&(class1->num_search_class__success)) == 7);
    assert(atomic_load(&(class1->num_search_chksum_cols)) == 0);

    /* Version stats */
    assert(atomic_load(&(class1->num_wait_for_curr_version_to_inc)) == 0);

    /* H5P_mt_active_thread_count_t stats */
    assert(atomic_load(&(class1->num_thrd_update_cols)) == 0);
    assert(atomic_load(&(class1->num_thrd_count_update)) == 52);
    assert(atomic_load(&(class1->num_thrd_closing_flag_set)) == 0);
    assert(atomic_load(&(class1->num_thrd_opening_flag_set)) == 0);

    /* H5P_mt_class_ref_counts_t stats */
    assert(atomic_load(&(class1->num_ref_count_cols)) == 0);
    assert(atomic_load(&(class1->num_ref_count_update)) == 4);
    assert(atomic_load(&(class1->num_ref_count_inc_while_deleted)) == 0);
    assert(atomic_load(&(class1->num_ref_count_marked_deleted)) == 0);
    assert(atomic_load(&(class1->num_ref_count_unmarked_deleted)) == 0);

    /* Property ref_count_stats */
    assert(atomic_load(&(class1->num_prop_ref_count_update)) == 4);

    /**
     * Check class2's stats
     */

    /* Insert stats  */
    assert(atomic_load(&(class2->H5P__insert_prop_class__num_calls)) == 0);
    assert(atomic_load(&(class2->insert_max_nodes_visited)) == 0);
    assert(atomic_load(&(class2->insert_avg_nodes_visited)) == 0);
    assert(atomic_load(&(class2->num_insert_nodes_visited)) == 0);
    assert(atomic_load(&(class2->num_insert_prop__cols)) == 0);
    assert(atomic_load(&(class2->num_insert_prop__success)) == 0);
    assert(atomic_load(&(class2->num_insert_prop__chksum_cols)) == 0);

    /* Delete stats */
    assert(atomic_load(&(class2->H5P__delete_prop__class__num_calls)) == 0);
    assert(atomic_load(&(class2->set_delete__max_nodes_visited)) == 0);
    assert(atomic_load(&(class2->set_delete__avg_nodes_visited)) == 0);
    assert(atomic_load(&(class2->num_set_delete__nodes_visited)) == 0);
    assert(atomic_load(&(class2->num_set_delete__cols)) == 0);
    assert(atomic_load(&(class2->num_set_delete__success)) == 0);
    assert(atomic_load(&(class2->num_set_delete_chksum_cols)) == 0);

    /* Search stats */
    assert(atomic_load(&(class2->H5P__search_prop__class__num_calls)) == 0);
    assert(atomic_load(&(class2->search_class__max_nodes_visited)) == 0);
    assert(atomic_load(&(class2->search_class__avg_nodes_visited)) == 0);
    assert(atomic_load(&(class2->num_search_class__nodes_visited)) == 0);
    assert(atomic_load(&(class2->num_search_class__success)) == 0);
    assert(atomic_load(&(class2->num_search_chksum_cols)) == 0);

    /* Version stats */
    assert(atomic_load(&(class2->num_wait_for_curr_version_to_inc)) == 0);

    /* H5P_mt_active_thread_count_t stats */
    assert(atomic_load(&(class2->num_thrd_update_cols)) == 0);
    assert(atomic_load(&(class2->num_thrd_count_update)) == 2);
    assert(atomic_load(&(class2->num_thrd_closing_flag_set)) == 0);
    assert(atomic_load(&(class2->num_thrd_opening_flag_set)) == 0);

    /* H5P_mt_class_ref_counts_t stats */
    assert(atomic_load(&(class2->num_ref_count_cols)) == 0);
    assert(atomic_load(&(class2->num_ref_count_update)) == 1);
    assert(atomic_load(&(class2->num_ref_count_inc_while_deleted)) == 0);
    assert(atomic_load(&(class2->num_ref_count_marked_deleted)) == 0);
    assert(atomic_load(&(class2->num_ref_count_unmarked_deleted)) == 0);

    /* Property ref_count_stats */
    assert(atomic_load(&(class2->num_prop_ref_count_update)) == 3);

    /**
     * Check list1's stats
     */

    /* Insert stats  */
    assert(atomic_load(&(list1->H5P__insert_prop_list__num_calls)) == 5);
    assert(atomic_load(&(list1->insert_max_nodes_visited)) == 3);
    assert(atomic_load(&(list1->insert_avg_nodes_visited)) == 2);
    assert(atomic_load(&(list1->num_insert_nodes_visited)) == 3);
    assert(atomic_load(&(list1->num_insert_prop_cols)) == 0);
    assert(atomic_load(&(list1->num_insert_prop_success)) == 5);
    assert(atomic_load(&(list1->num_insert_update_entry)) == 3);
    assert(atomic_load(&(list1->num_insert_update_entry_cols)) == 0);
    assert(atomic_load(&(list1->num_insert_prop__chksum_cols)) == 0);

    /* Delete stats */
    assert(atomic_load(&(list1->H5P__delete_prop__list__num_calls)) == 3);
    assert(atomic_load(&(list1->num_deletes_from_lfsll)) == 1);
    assert(atomic_load(&(list1->set_delete__max_nodes_visited)) == 3);
    assert(atomic_load(&(list1->set_delete__avg_nodes_visited)) == 3);
    assert(atomic_load(&(list1->num_set_delete__nodes_visited)) == 3);
    assert(atomic_load(&(list1->num_set_delete__cols)) == 0);
    assert(atomic_load(&(list1->num_set_delete__success)) == 3);
    assert(atomic_load(&(list1->num_set_delete_chksum_cols)) == 0);
    assert(atomic_load(&(list1->num_set_delete__base_delete_version)) == 1);
    assert(atomic_load(&(list1->num_set_delete__curr_entry)) == 1);
    assert(atomic_load(&(list1->num_set_delete__older_curr)) == 0);

    /* Search stats */
    assert(atomic_load(&(list1->H5P__search_prop__list__num_calls)) == 11);
    assert(atomic_load(&(list1->search_list__max_nodes_visited)) == 0);
    assert(atomic_load(&(list1->search_list__avg_nodes_visited)) == 0);
    assert(atomic_load(&(list1->num_search_list__nodes_visited)) == 0);
    assert(atomic_load(&(list1->num_search_list__success)) == 8);
    assert(atomic_load(&(list1->num_search_chksum_cols)) == 0);
    assert(atomic_load(&(list1->num_search_list__found_base)) == 3);
    assert(atomic_load(&(list1->num_search_list__found_curr)) == 3);
    assert(atomic_load(&(list1->num_target_prop_found_but_deleted)) == 2);

    /* Version check stats */
    assert(atomic_load(&(list1->num_wait_for_curr_version_to_inc)) == 0);

    /* H5P_mt_active_thread_count_t stats */
    assert(atomic_load(&(list1->num_thrd_update_cols)) == 0);
    assert(atomic_load(&(list1->num_thrd_count_update)) == 50);
    assert(atomic_load(&(list1->num_thrd_closing_flag_set)) == 0);
    assert(atomic_load(&(list1->num_thrd_opening_flag_set)) == 0);

    /* initializing lkup_tbl stats */
    assert(atomic_load(&(list1->num_inherited_with_create_cb)) == 0);
    assert(atomic_load(&(list1->num_inherited_with_create_cb)) == 0);

    /**
     * Check list3's stats
     */

    /* Insert stats  */
    assert(atomic_load(&(list3->H5P__insert_prop_list__num_calls)) == 0);
    assert(atomic_load(&(list3->insert_max_nodes_visited)) == 0);
    assert(atomic_load(&(list3->insert_avg_nodes_visited)) == 0);
    assert(atomic_load(&(list3->num_insert_nodes_visited)) == 0);
    assert(atomic_load(&(list3->num_insert_prop_cols)) == 0);
    assert(atomic_load(&(list3->num_insert_prop_success)) == 0);
    assert(atomic_load(&(list3->num_insert_update_entry)) == 0);
    assert(atomic_load(&(list3->num_insert_update_entry_cols)) == 0);
    assert(atomic_load(&(list3->num_insert_prop__chksum_cols)) == 0);

    /* Delete stats */
    assert(atomic_load(&(list3->H5P__delete_prop__list__num_calls)) == 0);
    assert(atomic_load(&(list3->num_deletes_from_lfsll)) == 0);
    assert(atomic_load(&(list3->set_delete__max_nodes_visited)) == 0);
    assert(atomic_load(&(list3->set_delete__avg_nodes_visited)) == 0);
    assert(atomic_load(&(list3->num_set_delete__nodes_visited)) == 0);
    assert(atomic_load(&(list3->num_set_delete__cols)) == 0);
    assert(atomic_load(&(list3->num_set_delete__success)) == 0);
    assert(atomic_load(&(list3->num_set_delete_chksum_cols)) == 0);
    assert(atomic_load(&(list3->num_set_delete__base_delete_version)) == 0);
    assert(atomic_load(&(list3->num_set_delete__curr_entry)) == 0);
    assert(atomic_load(&(list3->num_set_delete__older_curr)) == 0);

    /* Search stats */
    assert(atomic_load(&(list3->H5P__search_prop__list__num_calls)) == 0);
    assert(atomic_load(&(list3->search_list__max_nodes_visited)) == 0);
    assert(atomic_load(&(list3->search_list__avg_nodes_visited)) == 0);
    assert(atomic_load(&(list3->num_search_list__nodes_visited)) == 0);
    assert(atomic_load(&(list3->num_search_list__success)) == 0);
    assert(atomic_load(&(list3->num_search_chksum_cols)) == 0);
    assert(atomic_load(&(list3->num_search_list__found_base)) == 0);
    assert(atomic_load(&(list3->num_search_list__found_curr)) == 0);
    assert(atomic_load(&(list3->num_target_prop_found_but_deleted)) == 0);

    /* Version check stats */
    assert(atomic_load(&(list3->num_wait_for_curr_version_to_inc)) == 0);

    /* H5P_mt_active_thread_count_t stats */
    assert(atomic_load(&(list3->num_thrd_update_cols)) == 0);
    assert(atomic_load(&(list3->num_thrd_count_update)) == 0);
    assert(atomic_load(&(list3->num_thrd_closing_flag_set)) == 0);
    assert(atomic_load(&(list3->num_thrd_opening_flag_set)) == 0);

    /* initializing lkup_tbl stats */
    assert(atomic_load(&(list3->num_inherited_with_create_cb)) == 0);
    assert(atomic_load(&(list3->num_lkup_tbl_copy_entries_blank)) == 0);

    return (ret_value);

} /* end check_stats() */

/****************************************************************************************
 * Function:    check_global_stats
 *
 * Purpose:     After all threads in a test suite have finished the tests and have
 *              checked their respective classes and lists, the global stats are checked
 *              to ensure they are correct for the number of threads that ran the tests.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
check_global_stats(int _num_threads)
{
    uint32_t num_threads = (uint32_t)_num_threads;

    /* Property free list stats */
    if (num_threads == 1) {
        assert(atomic_load(&(H5P_mt_g.prop_fl_head_update)) == 2);
        assert(atomic_load(&(H5P_mt_g.prop_fl_tail_update)) == 15);
        assert(atomic_load(&(H5P_mt_g.prop_fl_next_update)) == 14);
        assert(atomic_load(&(H5P_mt_g.num_props_added_to_fl)) == 15);
    }
    else {
        assert(atomic_load(&(H5P_mt_g.prop_fl_head_update)) == 0);
        assert(atomic_load(&(H5P_mt_g.prop_fl_tail_update)) == 0);
        assert(atomic_load(&(H5P_mt_g.prop_fl_next_update)) == 0);
        assert(atomic_load(&(H5P_mt_g.num_props_added_to_fl)) == 0);
    }
    assert(atomic_load(&(H5P_mt_g.prop_fl_head_update_cols)) == 0);
    assert(atomic_load(&(H5P_mt_g.prop_fl_tail_update_cols)) == 0);
    assert(atomic_load(&(H5P_mt_g.prop_fl_next_update_cols)) == 0);
    assert(atomic_load(&(H5P_mt_g.prop_fl_head_freed_due_to_max_len)) == 0);
    assert(atomic_load(&(H5P_mt_g.prop_fl_head_free_skipped_due_to_empty)) == 0);
    assert(atomic_load(&(H5P_mt_g.prop_fl_head_free_skipped_no_reallocable)) == 0);

    /* Class free list stats */
    if (num_threads == 1) {
        assert(atomic_load(&(H5P_mt_g.class_fl_head_update)) == 2);
        assert(atomic_load(&(H5P_mt_g.class_fl_tail_update)) == 2);
        assert(atomic_load(&(H5P_mt_g.class_fl_next_update)) == 0);
    }
    else {
        assert(atomic_load(&(H5P_mt_g.class_fl_head_update)) == 1);
        assert(atomic_load(&(H5P_mt_g.class_fl_tail_update)) == num_threads);
        assert(atomic_load(&(H5P_mt_g.class_fl_next_update)) == num_threads - 1);
    }
    assert(atomic_load(&(H5P_mt_g.class_fl_head_update_cols)) == 0);
    assert(atomic_load(&(H5P_mt_g.class_fl_tail_update_cols)) == 0);
    assert(atomic_load(&(H5P_mt_g.class_fl_next_update_cols)) == 0);
    assert(atomic_load(&(H5P_mt_g.num_class_added_to_fl)) == num_threads);
    assert(atomic_load(&(H5P_mt_g.class_fl_head_freed_due_to_max_len)) == 0);
    assert(atomic_load(&(H5P_mt_g.class_fl_head_free_skipped_due_to_empty)) == 0);
    if (num_threads < 17) {
        assert(atomic_load(&(H5P_mt_g.class_fl_head_free_skipped_no_reallocable)) == 0);
    }
    else {
        assert(atomic_load(&(H5P_mt_g.class_fl_head_free_skipped_no_reallocable)) == (num_threads - 16));
    }

    /* List free list stats */
    if (num_threads == 1) {
        assert(atomic_load(&(H5P_mt_g.list_fl_head_update)) == 2);
        assert(atomic_load(&(H5P_mt_g.list_fl_tail_update)) == 2);
        assert(atomic_load(&(H5P_mt_g.list_fl_next_update)) == 0);
    }
    else {
        assert(atomic_load(&(H5P_mt_g.list_fl_head_update)) == 1);
        assert(atomic_load(&(H5P_mt_g.list_fl_tail_update)) == num_threads);
        assert(atomic_load(&(H5P_mt_g.list_fl_next_update)) == num_threads - 1);
    }
    assert(atomic_load(&(H5P_mt_g.list_fl_head_update_cols)) == 0);
    assert(atomic_load(&(H5P_mt_g.list_fl_tail_update_cols)) == 0);
    assert(atomic_load(&(H5P_mt_g.list_fl_next_update_cols)) == 0);
    assert(atomic_load(&(H5P_mt_g.num_list_added_to_fl)) == num_threads);
    assert(atomic_load(&(H5P_mt_g.list_fl_head_freed_due_to_max_len)) == 0);
    assert(atomic_load(&(H5P_mt_g.list_fl_head_free_skipped_due_to_empty)) == 0);
    assert(atomic_load(&(H5P_mt_g.list_fl_head_free_skipped_no_reallocable)) == 0);

    /* stats for creating or copying classes */
    assert(atomic_load(&(H5P_mt_g.H5P__mt_create_class__num_calls)) == (2 * num_threads));
    assert(atomic_load(&(H5P_mt_g.H5P__mt_copy_class__num_calls)) == (1 * num_threads));
    if (num_threads == 1) {
        assert(atomic_load(&(H5P_mt_g.num_class_structs_allocated_from_heap)) == 2);
        assert(atomic_load(&(H5P_mt_g.num_class_structs_allocated_from_fl)) == 1);
    }
    else {
        assert(atomic_load(&(H5P_mt_g.num_class_structs_allocated_from_heap)) == (3 * num_threads));
        assert(atomic_load(&(H5P_mt_g.num_class_structs_allocated_from_fl)) == 0);
    }

    /* stats for creating or copying lists */
    assert(atomic_load(&(H5P_mt_g.H5P__mt_create_list__num_calls)) == (3 * num_threads));
    if (num_threads == 1) {
        assert(atomic_load(&(H5P_mt_g.num_list_structs_allocated_from_heap)) == 2);
        assert(atomic_load(&(H5P_mt_g.num_list_structs_allocated_from_fl)) == 1);
    }
    else {
        assert(atomic_load(&(H5P_mt_g.num_list_structs_allocated_from_heap)) == (3 * num_threads));
        assert(atomic_load(&(H5P_mt_g.num_list_structs_allocated_from_fl)) == 0);
    }
    assert(atomic_load(&(H5P_mt_g.H5P__init_lkup_tbl__num_calls)) == (2 * num_threads));
    assert(atomic_load(&(H5P_mt_g.H5P__init_lkup_tbl_copy__num_calls)) == (1 * num_threads));

    /* stats for creating props */
    assert(atomic_load(&(H5P_mt_g.H5P__mt_create_prop__num_calls)) == (27 * num_threads));
    if (num_threads == 1) {
        assert(atomic_load(&(H5P_mt_g.num_prop_structs_allocated_from_heap)) == 38);
        assert(atomic_load(&(H5P_mt_g.num_prop_structs_allocated_from_fl)) == 1);
    }
    else {
        assert(atomic_load(&(H5P_mt_g.num_prop_structs_allocated_from_heap)) ==
               (38 * num_threads + num_threads));
        assert(atomic_load(&(H5P_mt_g.num_prop_structs_allocated_from_fl)) == 0);
    }

    /* stats for property inserts */
    assert(atomic_load(&(H5P_mt_g.num_props_inserted_classes)) == (7 * num_threads));
    assert(atomic_load(&(H5P_mt_g.num_props_inserted_lists)) == (9 * num_threads));
    assert(atomic_load(&(H5P_mt_g.H5P__mt_ins_or_mod_prop__lfsll_ins__num_calls)) == (27 * num_threads));

    /* stats for number of deletes */
    assert(atomic_load(&(H5P_mt_g.num_props_deleted_classes)) == (3 * num_threads));
    assert(atomic_load(&(H5P_mt_g.num_props_deleted_lists)) == (3 * num_threads));

    /* stats for searches */
    assert(atomic_load(&(H5P_mt_g.num_searches_classes)) == (7 * num_threads));
    assert(atomic_load(&(H5P_mt_g.num_searches_while_an_op_occurs_class)) == (0 * num_threads));
    assert(atomic_load(&(H5P_mt_g.num_searches_lists)) == (11 * num_threads));
    assert(atomic_load(&(H5P_mt_g.num_searches_while_an_op_occurs_list)) == (0 * num_threads));

    /* Property chksum cols stats */
    assert(atomic_load(&(H5P_mt_g.num_chksum_cols)) == (0 * num_threads));

    /* H5P__mt_enforce_serialization stats */
    assert(atomic_load(&(H5P_mt_g.H5P__mt_enforce_serialization__num_calls)) == (0 * num_threads));

    /* stats for closing classes with active derivied lists and classes */
    assert(atomic_load(&(H5P_mt_g.close_class_but_pl_not_zero)) == (0 * num_threads));
    assert(atomic_load(&(H5P_mt_g.close_class_but_plc_not_zero)) == (0 * num_threads));

    /* stats for the clear functions */
    if (num_threads == 1) {
        assert(atomic_load(&(H5P_mt_g.num_props_freed)) == 1);
        assert(atomic_load(&(H5P_mt_g.num_classes_freed)) == 1);
        assert(atomic_load(&(H5P_mt_g.num_lists_freed)) == 1);
    }
    else {
        assert(atomic_load(&(H5P_mt_g.num_props_freed)) == 0);
        assert(atomic_load(&(H5P_mt_g.num_classes_freed)) == 0);
        assert(atomic_load(&(H5P_mt_g.num_lists_freed)) == 0);
    }

    /* H5P_mt_class_t and H5P_mt_list_t comparison stats */
    if (num_threads == 1) {
        assert(atomic_load(&(H5P_mt_g.max_derived_classes)) >= 2);
    }
    else {
        assert(atomic_load(&(H5P_mt_g.max_derived_classes)) >= 2);
        assert(atomic_load(&(H5P_mt_g.max_derived_classes)) <= (2 * num_threads));
    }
    assert(atomic_load(&(H5P_mt_g.max_derived_lists)) == 2);
    assert(atomic_load(&(H5P_mt_g.max_class_num_phys_props)) == 10);
    assert(atomic_load(&(H5P_mt_g.max_list_num_phys_props)) == 8);
    assert(atomic_load(&(H5P_mt_g.max_class_version_number)) == 9);
    assert(atomic_load(&(H5P_mt_g.max_list_version_number)) == 9);

    return SUCCEED;

} /* end check_global_stats() */

/****************************************************************************************
 * Function:    close_test_structs
 *
 * Purpose:     Resets the fl_next fields of a thread's classes and lists and closes the
 *              structures in a specific order to test that closing works correctly.
 *
 * Details:
 *
 *  1) Class1 is closed first to ensure it is removed from the index, and marking it as
 *     deleted, but not fully closing it due to still having a derived list and class.
 *  2) H5Pget_class() is called to ensure that class1's same id is correctly inserted
 *     back into the index and that it is unmarked as deleted.
 *  3) Class1 is closed again, removing it from the index and marking it as deleted.
 *  4) List1 is closed, which should be removed from the index and closed decrementing
 *     class1's pl ref_count.
 *  5) List3 is closed, which should be removed from the index and closed decrementing
 *     class2's pl ref_count.
 *     NOTE: list2 was closed previously during testing.
 *  6) Class2 is closed, which should remove it from the index and fully closing it,
 *     since it no longer has any derived classes or lists. Once class2 is closed, class1
 *     should automatically finish being closed, since it is marked deleted an it's last
 *     derived class just closed.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
close_test_structs(test_params_t *test_params)
{
    H5P_mt_class_t     *class1;
    H5P_mt_class_t     *class2;
    H5P_mt_class_t     *h5i_class_ret = NULL;
    H5P_mt_class_sptr_t cfl_next;
    H5P_mt_list_t      *list1;
    H5P_mt_list_t      *list3;
    H5P_mt_list_t      *h5i_list_ret = NULL;
    H5P_mt_list_sptr_t  lfl_next;
    H5P_mt_class_sptr_t class_next = {NULL, 0};
    H5P_mt_list_sptr_t  list_next  = {NULL, 0};
    hid_t               id_check;
    herr_t              ret;

    herr_t ret_value = SUCCEED;

    class1   = test_params->test_classes_head.ptr;
    cfl_next = atomic_load(&(class1->fl_next));
    class2   = cfl_next.ptr;
    list1    = test_params->test_lists_head.ptr;
    lfl_next = atomic_load(&(list1->fl_next));
    list3    = lfl_next.ptr;

    assert(class1);
    assert(atomic_load(&(class1->tag)) == H5P_MT_CLASS_TAG);
    assert(class2);
    assert(atomic_load(&(class2->tag)) == H5P_MT_CLASS_TAG);
    assert(list1);
    assert(atomic_load(&(list1->tag)) == H5P_MT_LIST_TAG);
    assert(list3);
    assert(atomic_load(&(list3->tag)) == H5P_MT_LIST_TAG);

    /* Reset the structs fl_next fields to not cause issues */
    atomic_store(&(class1->fl_next), class_next);
    atomic_store(&(class2->fl_next), class_next);
    atomic_store(&(list1->fl_next), list_next);
    atomic_store(&(list3->fl_next), list_next);

    /* Ensure class1's ref_count fields are correct before closing */
    ret = check_class_ref_counts(class1, 1, 1, FALSE, "close_test_structs");
    CHECK_I(ret, "check_class_ref_counts");

    /**
     * Close class1, which should be removed from the index, but the instance
     * should stay around due to still having a derived list and class.
     */

    ret = H5Pclose_class(atomic_load(&(class1->id)));
    CHECK_I(ret, "H5Pclose_class");

    /* Ensure class1 is marked deleted */
    ret = check_class_ref_counts(class1, 1, 1, TRUE, "H5P__mt_close_class");

    /* Ensure class1 is not in the index */
    h5i_class_ret = H5I_object(atomic_load(&(class1->id)));
    CHECK_PTR_NULL(h5i_class_ret, "H5I_object");

    /* Ensure class1 is not marked closing */
    ret = check_and_set_thrd_flags(class1, FALSE, FALSE, FALSE, FALSE, "H5P__mt_close_class");
    CHECK_I(ret, "H5P__mt_close_class");

    /**
     * Ensure that getting class1 through an H5P function unmarks it as deleted.
     */

    id_check = H5Pget_class(atomic_load(&(list1->plist_id)));
    VERIFY(id_check, atomic_load(&(class1->id)), "H5Pget_class");
    assert(id_check == atomic_load(&(class1->id)));

    /* Ensure class1 is no longer marked as deleted */
    ret = check_class_ref_counts(class1, 1, 1, FALSE, "H5Pget_class");

    /* Ensure class1 is back in the index */
    h5i_class_ret = H5I_object(id_check);
    CHECK_PTR(h5i_class_ret, "H5I_object");

    /* Ensure the returned class is the same as class1 */
    VERIFY(h5i_class_ret, class1, "H5I_object");

    ret = H5P__mt_cmp_class(h5i_class_ret, class1);
    CHECK_I(ret, "H5P__mt_cmp_class");
    VERIFY(ret, 0, "H5P__mt_cmp_class");

    /**
     * Close class1 again
     */

    ret = H5Pclose_class(atomic_load(&(class1->id)));
    CHECK_I(ret, "H5Pclose_class");

    /* Ensure class1 is marked deleted */
    ret = check_class_ref_counts(class1, 1, 1, TRUE, "H5Pclose_class");

    /* Ensure class1 is not in the index */
    h5i_class_ret = H5I_object(atomic_load(&(class1->id)));
    CHECK_PTR_NULL(h5i_class_ret, "H5I_object");

    /* Ensure class1 is not marked closing */
    ret = check_and_set_thrd_flags(class1, FALSE, FALSE, FALSE, FALSE, "H5Pclose_class");
    CHECK_I(ret, "H5Pclose_class");

    /**
     * Close list1, which should be removed from the index and closed, and
     * check class1, which should no longer have a derived list, but still
     * has a derived class so shouldn't be closed yet.
     */

    ret = H5Pclose(atomic_load(&(list1->plist_id)));
    CHECK_I(ret, "H5Pclose");

    /* Ensure class1 is correct */
    ret = check_class_ref_counts(class1, 0, 1, TRUE, "H5Pclose");

    /* Ensure class1 still is not in the index */
    h5i_class_ret = H5I_object(atomic_load(&(class1->id)));
    CHECK_PTR_NULL(h5i_class_ret, "H5I_object");

    /* Ensure list1 is no longer in the index */
    h5i_list_ret = H5I_object(atomic_load(&(list1->plist_id)));

    /* Ensure class1 is not marked closing */
    ret = check_and_set_thrd_flags(class1, FALSE, FALSE, FALSE, FALSE, "H5Pclose");
    CHECK_I(ret, "H5Pclose");

    /* Ensure list1 is marked closing */
    ret = check_and_set_thrd_flags(list1, FALSE, TRUE, FALSE, TRUE, "H5Pclose");
    CHECK_I(ret, "H5Pclose");

    /* Ensure class2's ref_count is correct before closing */
    ret = check_class_ref_counts(class2, 1, 0, FALSE, "close_test_structs");
    CHECK_I(ret, "check_class_ref_counts");

    /**
     * Close list3, which should be removed from the index and closed, and
     * check class2, which should no longer have a derived list, but hasn't
     * been closed so should be active.
     */

    ret = H5Pclose(atomic_load(&(list3->plist_id)));
    CHECK_I(ret, "H5Pclose");

    /* Ensure class2 is correct */
    ret = check_class_ref_counts(class2, 0, 0, FALSE, "H5Pclose");

    /* Ensure class2 still is in the index */
    h5i_class_ret = H5I_object(atomic_load(&(class2->id)));
    CHECK_PTR(h5i_class_ret, "H5I_object");
    VERIFY(h5i_class_ret, class2, "H5Pclose");

    /* Ensure list3 is no longer in the index */
    h5i_list_ret = H5I_object(atomic_load(&(list3->plist_id)));
    CHECK_PTR_NULL(h5i_list_ret, "H5I_object");

    /* Ensure class2 is not marked closing */
    ret = check_and_set_thrd_flags(class2, FALSE, FALSE, FALSE, FALSE, "H5Pclose");
    CHECK_I(ret, "H5Pclose");

    /* Ensure list3 is marked closing */
    ret = check_and_set_thrd_flags(list3, FALSE, TRUE, FALSE, TRUE, "H5Pclose");
    CHECK_I(ret, "H5Pclose");

    /**
     * Close class2, which now that it has no derived lists or classes
     * should be removed from the index and closed. Also, since class1
     * is marked as deleted, once class2 is closed, class1 should also
     * be closed automatically.
     */

    ret = H5Pclose_class(atomic_load(&(class2->id)));
    CHECK_I(ret, "H5Pclose_class");

    /* Ensure class2 is marked deleted */
    ret = check_class_ref_counts(class2, 0, 0, TRUE, "H5Pclose_class");

    /* Ensure class2 is not in the index */
    h5i_class_ret = H5I_object(atomic_load(&(class2->id)));
    CHECK_PTR_NULL(h5i_class_ret, "H5I_object");

    /* Ensure class2 is marked closing */
    ret = check_and_set_thrd_flags(class2, FALSE, TRUE, FALSE, TRUE, "H5Pclose_class");
    CHECK_I(ret, "H5Pclose_class");

    /* Ensure class1 is marked deleted */
    ret = check_class_ref_counts(class1, 0, 0, TRUE, "H5Pclose_class");

    /* Ensure class1 is not in the index */
    h5i_class_ret = H5I_object(atomic_load(&(class1->id)));
    CHECK_PTR_NULL(h5i_class_ret, "H5I_object");

    /* Ensure class1 is marked closing */
    ret = check_and_set_thrd_flags(class1, FALSE, TRUE, FALSE, TRUE, "H5Pclose_class");
    CHECK_I(ret, "H5Pclose_class");

    /* Check all of class1's stats that have changed */
    assert(atomic_load(&(class1->num_thrd_count_update)) == 64);
    assert(atomic_load(&(class1->num_thrd_closing_flag_set)) == 1);
    assert(atomic_load(&(class1->num_ref_count_update)) == 8);
    assert(atomic_load(&(class1->num_ref_count_marked_deleted)) == 2);
    assert(atomic_load(&(class1->num_ref_count_unmarked_deleted)) == 1);

    /* Check all of class2's stats that have changed */
    assert(atomic_load(&(class2->num_thrd_count_update)) == 6);
    assert(atomic_load(&(class2->num_thrd_closing_flag_set)) == 1);
    assert(atomic_load(&(class2->num_ref_count_update)) == 3);
    assert(atomic_load(&(class2->num_ref_count_marked_deleted)) == 1);

    /* Check all of list1's stats that have changed */
    assert(atomic_load(&(list1->num_thrd_closing_flag_set)) == 1);

    /* Check all of list3's stats that have changed */
    assert(atomic_load(&(list3->num_thrd_closing_flag_set)) == 1);

    return (ret_value);

} /* end close_test_structs() */

/****************************************************************************************
 * Function:    term_test_free_lists
 *
 * Purpose:     Calls H5P__mt_term_free_lists() which clears and frees all the class,
 *              list, and property structures from the tests, and then checks all the
 *              global stats that were changed from closing and then freeing the
 *              structures changed.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
term_test_free_lists(int _num_threads)
{
    uint64_t num_threads = (uint64_t)_num_threads;
    herr_t   ret;

    ret = H5P__mt_term_free_lists();
    CHECK_I(ret, "H5P__mt_term_free_lists");

    /* Check all the global stats that have changed */
    if (num_threads == 1) {
        assert(atomic_load(&(H5P_mt_g.prop_fl_len)) == 0);
        assert(atomic_load(&(H5P_mt_g.class_fl_len)) == 0);
        assert(atomic_load(&(H5P_mt_g.list_fl_len)) == 0);
        assert(atomic_load(&(H5P_mt_g.prop_fl_head_update)) == 40);
        assert(atomic_load(&(H5P_mt_g.prop_fl_tail_update)) == 40);
        assert(atomic_load(&(H5P_mt_g.prop_fl_next_update)) == 38);
        assert(atomic_load(&(H5P_mt_g.num_props_added_to_fl)) == 39);
        assert(atomic_load(&(H5P_mt_g.class_fl_head_update)) == 5);
        assert(atomic_load(&(H5P_mt_g.class_fl_tail_update)) == 5);
        assert(atomic_load(&(H5P_mt_g.class_fl_next_update)) == 1);
        assert(atomic_load(&(H5P_mt_g.num_class_added_to_fl)) == 3);
        assert(atomic_load(&(H5P_mt_g.list_fl_head_update)) == 5);
        assert(atomic_load(&(H5P_mt_g.list_fl_tail_update)) == 5);
        assert(atomic_load(&(H5P_mt_g.list_fl_next_update)) == 1);
        assert(atomic_load(&(H5P_mt_g.num_list_added_to_fl)) == 3);
        assert(atomic_load(&(H5P_mt_g.close_class_but_pl_not_zero)) == 2);
        assert(atomic_load(&(H5P_mt_g.close_class_but_plc_not_zero)) == 2);
        assert(atomic_load(&(H5P_mt_g.class_un_marked_as_deleted)) == 1);
        assert(atomic_load(&(H5P_mt_g.num_classes_freed)) == 3);
        assert(atomic_load(&(H5P_mt_g.num_lists_freed)) == 3);
        assert(atomic_load(&(H5P_mt_g.num_props_freed)) == 39);
    }
    else {
        assert(atomic_load(&(H5P_mt_g.prop_fl_len)) == 0);
        assert(atomic_load(&(H5P_mt_g.class_fl_len)) == 0);
        assert(atomic_load(&(H5P_mt_g.list_fl_len)) == 0);
        assert(atomic_load(&(H5P_mt_g.prop_fl_head_update)) == (39 * num_threads + 1));
        assert(atomic_load(&(H5P_mt_g.prop_fl_tail_update)) == (39 * num_threads + 1));
        assert(atomic_load(&(H5P_mt_g.prop_fl_next_update)) == (39 * num_threads - 1));
        assert(atomic_load(&(H5P_mt_g.num_props_added_to_fl)) == (39 * num_threads));
        assert(atomic_load(&(H5P_mt_g.class_fl_head_update)) == (3 * num_threads + 1));
        assert(atomic_load(&(H5P_mt_g.class_fl_tail_update)) == (3 * num_threads + 1));
        assert(atomic_load(&(H5P_mt_g.class_fl_next_update)) == (3 * num_threads - 1));
        assert(atomic_load(&(H5P_mt_g.num_class_added_to_fl)) == (3 * num_threads));
        assert(atomic_load(&(H5P_mt_g.list_fl_head_update)) == (3 * num_threads + 1));
        assert(atomic_load(&(H5P_mt_g.list_fl_tail_update)) == (3 * num_threads + 1));
        assert(atomic_load(&(H5P_mt_g.list_fl_next_update)) == (3 * num_threads - 1));
        assert(atomic_load(&(H5P_mt_g.num_list_added_to_fl)) == (3 * num_threads));
        assert(atomic_load(&(H5P_mt_g.close_class_but_pl_not_zero)) == (2 * num_threads));
        assert(atomic_load(&(H5P_mt_g.close_class_but_plc_not_zero)) == (2 * num_threads));
        assert(atomic_load(&(H5P_mt_g.class_un_marked_as_deleted)) == num_threads);
        assert(atomic_load(&(H5P_mt_g.num_classes_freed)) == (3 * num_threads));
        assert(atomic_load(&(H5P_mt_g.num_lists_freed)) == (3 * num_threads));
        assert(atomic_load(&(H5P_mt_g.num_props_freed)) == (39 * num_threads));
    }

    return SUCCEED;

} /* end term_test_free_lists() */

/****************************************************************************************
 * Function:    reset_globals
 *
 * Purpose:     Resets the global stats so that all stats are blank before each test
 *              begins.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
reset_globals(TestParams_t H5_ATTR_UNUSED *params)
{
    herr_t ret;

    ret = H5P__reset_stats_global();
    CHECK_I(ret, "H5P__reset_stats_global");

    return SUCCEED;

} /* end reset_globals() */

#endif /* ifdef H5_HAVE_MULTITHREAD */

/****************************************************************************************
 * Function:    main
 *
 * Purpose:     main function to set up and run H5P multithread tests.
 *
 * Return:      number of errors during tests
 *
 ****************************************************************************************
 */
int
main(int argc, char **argv)
{
    // test_params_t test_params;
    int num_errs = 0;

    H5open();

    if (0 > init_globals()) {
        fprintf(stderr, "Failed initializing H5P testing globals.");
        exit(EXIT_FAILURE);
    }

    if (0 > create_test_root_class()) {
        fprintf(stderr, "Failed allocating and initializing H5P test root class.");
        exit(EXIT_FAILURE);
    }

    /* Initialize testing framework */
    if (TestInit(argv[0], NULL, NULL, NULL, NULL, 0, 0) < 0) {
        fprintf(stderr, "couldn't initialize testing framework\n");
        exit(EXIT_FAILURE);
    }

    /**
     * Hide most output from testing framework (except for some errors)
     * and replace with our own
     */
    SetTestVerbosity(VERBO_DEF - 1);

    /* Display testing information */
    TestInfo(stdout);

#ifdef H5_HAVE_MULTITHREAD

#if 0
    test_params = (test_params_t){/* thread_id         = */ 0,
                                  /* num_classes       = */ 0,
                                  /* test_classes_head = */ {NULL, 0},
                                  /* test_classes_tail = */ {NULL, 0},
                                  /* num_lists         = */ 0,
                                  /* test_lists_head   = */ {NULL, 0},
                                  /* test_lists_tail   = */ {NULL, 0}};

    /* Add tests */
    AddTest("test_h5p_mt_functions", test_h5p_mt_functions, NULL, clear_free_lists,
            &test_params, sizeof(test_params_t), 0, 
            "Single thread check of all H5P multithread functions");

    AddTest("test_h5p_mt_functions_mt", test_h5p_mt_functions_mt, NULL, 
            clear_free_lists, &test_params, sizeof(test_params_t), 0, 
            "Simple multithread check of all H5P multithread functions");
#else
    /* Add tests */
    AddTest("test_h5p_mt_functions", st_test_1, NULL, reset_globals, NULL, 0, 0,
            "Single thread check of all H5P multithread functions");

    AddTest("test_h5p_mt_functions_mt", mt_test_1, NULL, reset_globals, NULL, 0, 0,
            "Simple multithread check of all H5P multithread functions");

#endif

    /* Parse command line arguments */
    if (TestParseCmdLine(argc, argv) < 0) {
        fprintf(stderr, "Error occurred while parsing command-line arguments\n");
        goto exit;
    }

    /* Perform tests */
    if (PerformTests() < 0) {
        fprintf(stderr, "Error occurred while running tests\n");
        goto exit;
    }

    /* Display test summary if requested */
    if (GetTestSummary()) {
        TestSummary(stdout);
    }

#else
    fprintf(stderr, "Multithread isn't enabled in library configuration -- no tests to run\n");
#endif

exit:

    /* Retrieve number of testing errors before shutting down test infrastructure */
    num_errs = GetTestNumErrs();

    /* Release test infrastructure */
    if (TestShutdown() < 0) {
        fprintf(stderr, "Error while shutting down test infrastructure\n");
        num_errs++;
    }

    H5close();

    exit(num_errs > 0 ? EXIT_FAILURE : EXIT_SUCCESS);

} /* end main() */
