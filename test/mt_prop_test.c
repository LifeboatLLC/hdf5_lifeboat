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

#define CLASS3_NAME "Class 3"
hid_t CLASS3_ID_g = H5I_INVALID_HID;

#define CLASS4_NAME "Class 4"
hid_t CLASS4_ID_g = H5I_INVALID_HID;

#define NEG_SENTINEL_NAME "neg_sentinel"
#define POS_SENTINEL_NAME "pos_sentinel"

#define DEFAULT_MAX_NUM_THREADS 64

/* Structures for st_test_1 and mt_test_1 */

/****************************************************************************************
 *
 * Structure: prop_info_t
 *
 * Description:
 *
 * prop_info_t is a structure used is some global arrays to store the chksum, name, and
 * a pointer to an instance of H5P_mt_prop_t structs for each class and list created
 * during st_test_1 and mt_test_1. The H5P_mt_prop_t is used as the basis for the
 * properties created during these tests, and are used to compare the properties created
 * or modified in the test classes and lists to ensure that they are correct at each step.
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
 *      Pointer to an instance of H5P_mt_prop_t used to create properties and compare
 *      them back to ensuring correct fields during testing.
 *
 ****************************************************************************************
 */
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
 * This structure is used by each thread to count and store the classes and lists for
 * testing since each thread will create their own classes and lists during these tests,
 * and it helps limit other threads from accidently accessing another threads structure.
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

/* Structures for mt_test_2 */

/****************************************************************************************
 *
 * Structure: test_op_info_t
 *
 * Description:
 *
 * An array of test_op_info_t structs is used in thread_params_t for every thread
 * to store the information of the operations they performed on classes or lists, so when
 * the primary thread checks all other thread operations it can use this to aid it seeing
 * exactly what operations were done and what structures they were done on.
 *
 *
 * Fields:
 *
 * class ( H5P_mt_class_t * ):
 *      Pointer to the class that had the operation performed on. If the class didn't
 *      exist yet or the operation was performed on a list this will be NULL.
 *
 * list ( H5P_mt_list_t * ):
 *      Pointer to the list that had the operation performed on. If the list didn't
 *      exist yet or the operation was performed on a class this will be NULL.
 *
 * id ( hid_t ):
 *      ID of the list or class the operation was performed on.
 *
 * class_name ( const char * ):
 *      The name of the class the operation was performed on. If the operation was
 *      performed on a list this will be NULL.
 *
 * parent_name ( const char *):
 *      The name of the parent of the list or class this operation was performed on.
 * 
 * prop_name ( const char * ):
 *      The name of the property the operation was performed on, if a property was 
 *      involved, else NULL.
 * 
 * prop ( H5P_mt_prop_t * ):
 *      A pointer to the property the operation was performed on, if a property was
 *      involved, else NULL.
 *
 * op ( operation_type_t ):
 *      An enum that details what operation was performed.
 *          1) CREATE: Creating this list or class
 *          2) COPY: Creating this list or class as a copy of an existing list or class.
 *          3) SEARCH: Searching for a property and getting its value from a list or class.
 *          4) SEARCH_VER: Searching for a property and getting its value from a list or
 *                         class at a version that may be older than its most recent one.
 *          5) MOD_CREATE_PROP: Creating a new property in this list or class.
 *          6) MOD_MOD_PROP: Modifying a property in this list or class
 *          7) MOD_DELETE_PROP: Deleting a property in this list or class.
 *          8) CMP: Comparing this list or class to another of the same type.
 *          9) DELETE: Deleting this list or class.
 *         10) NO_OP_YET: No operation has been performed yet.
 *
 * op_num ( int ):
 *      The operation number this thread performed this operation at. Used in conjunction
 *      with test_list_op_info_t.op_num so the order of operations this thread checked
 *      can be followed more easily.
 *
 * obj_ver ( uint64_t ):
 *      The class's or list's curr_version when the operation began.
 * 
 * op_ver ( uint64_t ):
 *      The class's or list's curr_version after an operation was performed that 
 *      increments their version. Generally this should be only one higher than the 
 *      obj_ver, but it can be greater if other threads performed operations on the
 *      same object that also increment the version between the obj_ver being grabbed and
 *      this operation being completed.
 *
 * result ( op_result ):
 *      An enum that details the result of the operation performed.
 *          0) OP_SUCCESS: The operation was successful.
 *          1) CLASS_ALREADY_EXISTS: op failed due to the class already existing.
 *          2) CLASS_DOESNT_EXIST: op failed due to the class not existing.
 *          3) CLASS_DELETED: op failed due to the class being deleted.
 *          4) BAD_CLASS_VER: op failed because the version of the class wasn't valid.
 *          5) LIST_ALREADY_EXISTS: op failed due to the list already existing.
 *          6) LIST_DOESNT_EXIST: op failed due to the list not existing.
 *          7) LIST_DELETED: op failed due to the list being deleted.
 *          8) BAD_LIST_VER: op failed because the verion of the list wasn't valid.
 *          9) PARENT_DOESNT_EXIST: op failed due to the parent not existing yet.
 *         10) PARENT_DELETED: op failed due to the parent being deleted.
 *         11) OG_DOESNT_EXIST: a copy op failed due to original not existing.
 *         12) OG_DELETED: a copy op failed due to the original being deleted.
 *         13) PROP_DOESNT_EXIST: op failed due to the property not existing.
 *         14) PROP_ALREAD_EXISTS: op failed due to the property already existing.
 *         15) PROP_DELETED: op failed due to the property being deleted.
 *         16) CMP_CLS_WRONGLY_FAILED: op failed due to two classes not being equal when
 *                                     they should have been.
 *         17) CMP_CLS_WRONGLY_SUCCEED: op failed due to two classes being equal when
 *                                      they should not have been.
 *         18) CMP_LST_WRONGLY_FAILED: op failed due to two lists not being equal when
 *                                     they should have been.
 *         19) CMP_LST_WRONGLY_SUCCEED: op failed due to two lists being equal when
 *                                      they should not have been.
 *         20) NOT_ATTEMPTED: No operation has been attempted as yet.
 * 
 * obj_isa_copy ( bool ):
 *      A boolean used to show whether the class or list is a copy of another one.
 *
 * sorted ( bool ):
 *      A boolean used at the end after all threads have performed their operations and
 *      all operations by all threads are being checked. If TRUE, this operation has
 *      already been sorted to have occurred correctly and logged, so it isn't checked
 *      multiple times.
 *
 ****************************************************************************************
 */
typedef enum {
    CREATE,
    COPY,
    SEARCH,
    SEARCH_VER,
    MOD_CREATE_PROP,
    MOD_MOD_PROP,
    MOD_DELETE_PROP,
    CMP,
    DELETE,
    NO_OP_YET

} operation_type_t;

typedef enum {
    OP_SUCCESS,
    CLASS_ALREADY_EXISTS,
    CLASS_DOESNT_EXIST,
    CLASS_DELETED,
    BAD_CLASS_VER,
    LIST_ALREADY_EXISTS,
    LIST_DOESNT_EXIST,
    LIST_DELETED,
    BAD_LIST_VER,
    PARENT_DOESNT_EXIST,
    PARENT_DELETED,
    OG_DOESNT_EXIST,
    OG_DELETED,
    PROP_DOESNT_EXIST,
    PROP_ALREADY_EXISTS,
    PROP_DELETED,
    ALL_PROPS_EXIST,
    CMP_CLS_WRONGLY_FAILED,
    CMP_CLS_WRONGLY_SUCCEEDED,
    CMP_LST_WRONGLY_FAILED,
    CMP_LST_WRONGLY_SUCCEEDED,
    NOT_ATTEMPTED

} op_result_t;

typedef struct test_op_info_t {
    H5P_mt_class_t  *class;        /* Class the op was performed on or NULL if list */
    H5P_mt_list_t   *list;        /* List the op was performed on or NULL if class */
    hid_t            id;          /* ID of the class or list */
    int              test_id;     /* Local ID of the class or list */
    const char      *class_name;  /* class name if op was performed on a class */
    const char      *parent_name; /* Name of the parent, used for identification and op tracking */
    const char      *prop_name;   /* Name of the prop the op was performed on */
    H5P_mt_prop_t   *prop;        /* Property the op was performed on or NULL */
    operation_type_t op;          /* enum describing what operation was performed */
    uint32_t         op_num;      /* The number of operations this thread has performed */
    uint64_t         obj_ver;     /* The version of the list or class before the operation */
    uint64_t         op_ver;      /* The version of the list or class after the op (search & modify)*/
    op_result_t      result;      /* enum describing the result of the operation */
    bool             obj_isa_copy;
    bool             sorted; /* Flag used during the operation double check phase */

} test_op_info_t;

/****************************************************************************************
 *
 * Structure: thread_params_t
 *
 * Description:
 *
 * An instance of this structure is allocated for each thread to track how many and which
 * operations that thread has performed.
 *
 * Fields:
 *
 * thread_id ( int ):
 *      A local id given to each thread to make thread identification easier.
 *
 * num_threads ( int ):
 *      The number of total threads being ran in the test.
 *
 * ops_performed ( int ):
 *      The number of operations this thread has performed during this iteration of tests
 *
 * op_table ( test_op_info_t * ):
 *      Pointer to an array of test_op_info_t used to track info on the operations
 *      performed, and the objects they were performed on.
 *
 ****************************************************************************************
 */
typedef struct thread_params_t {
    int thread_id;
    int num_threads;

    uint32_t ops_performed;

    test_op_info_t *op_table;

} thread_params_t;

/****************************************************************************************
 *
 * Structure: prop_table_entry_t
 *
 * Description:
 *
 * An array of prop_table_entry_ts exists in both the globals class_table and list_table.
 * Each entry in the array is for a potential property that can exist in that list or
 * class, and contains the name of the property and it's current status (explained in
 * more detail in the fields section below).
 *
 * Fields:
 * 
 * chksum ( int64_t ):
 *      The chksum of the property ( see the description of H5P_mt_prop_t for more 
 *      details).
 *
 * name ( const char * ):
 *      A pointer to the name of the property ( see the description of H5P_mt_prop_t 
 *      for more details).
 *
 * status ( _Atomic status_t ):
 *      An enum of the different possible statuses that the property can have during the
 *      tests.
 *          DOESNT_EXIST The property hasn't been created for the class or list yet.
 *
 *          IN_PROGRESS  The property is in progress of being created for the list or
 *                       class.
 *
 *          EXISTS       The property has been created and exists for the most recent
 *                       version of the class or list.
 *
 *          DELETED      The property was created but has since been deleted for the most
 *                       recent version of the class or list.
 *
 *          EXISTS_BUT_CLOSED Only used by classes, see description in
 *                       class_table_entry_t.
 *
 *          CLOSING_IN_PROGRESS Only used by classes and lists, see description in 
 *                       class_table_entry_t or list_table_entry_t.
 *
 ****************************************************************************************
 */
typedef enum {
    DOESNT_EXIST,
    IN_PROGRESS,
    EXISTS,
    DELETED,
    EXISTS_BUT_CLOSED, /* Only used by class_table_entry_t */
    CLOSING_IN_PROGRESS

} status_t;

typedef struct prop_table_entry_t {
    int64_t          chksum;
    const char      *name;
    _Atomic status_t status;

} prop_table_entry_t;

/****************************************************************************************
 *
 * Structure: class_table_entry_t
 *
 * Description:
 *
 * A global array of class_table_entry_t (named class_table) is used to store an entry
 * for every possible class that can be created for the tests. These entries provide the
 * threads the information needed to perform operations on a class.
 *
 * Fields:
 *
 * class_sptr ( _Atomic H5P_mt_class_sptr_t )
 *      An atomic H5P_mt_class_sptr_t that will store the pointer for the H5P_mt_class_t 
 *      structure this entry is used for, when that class is created.
 *
 * name ( const char * ):
 *      A pointer to the name of the class ( see the description of H5P_mt_class_t 
 *      for more details).
 *
 * id ( _Atomic hid_t ):
 *      An atomic hid_t to store the H5I assigned ID of the class in the index.
 *
 * status ( _Atomic status_t ):
 *      An atomic structure that is used to show the current status of the
 *      H5P_mt_class_t.
 *          DOESNT EXIST The class hasn't been created yet.
 *          IN_PROGRESS  The class is in progress to be created.
 *          EXISTS       The class has been created and currently exists.
 *          DELETED      The class was created but has since been deleted.
 *          EXISTS_BUT_CLOSED
 *                       EXISTS_BUT_CLOSED is a status to avoid a class being closed more
 *                       than once. Due to the classes and operations being chosen at
 *                       random. If a class's status is EXISTS_BUT_CLOSED the thread will
 *                       not attempt to close the class, and simply mark the result as
 *                       the class already having been deleted. This is due to when a
 *                       class is closed multiple times an assert in H5I will fail.
 *
 *                       NOTE: The current implementation of multithread H5P has derived
 *                       lists and classes increment their parent's ID ref_count in the
 *                       index. Meaning that a class called to be closed with existing
 *                       derived objects, will NOT be closed or deleted from the index,
 *                       due to its ID's ref count not decrementing to 0. To compensate
 *                       for this when a list or class is closed, it will also call to
 *                       decrement its parent's ID ref_count in the index. Thus, if a
 *                       class with existing derivied objects is closed it will not be
 *                       deleted from the index, but when all its derived objects are
 *                       deleted from the index, it will then finalize being closed and
 *                       be deleted from the index.
 *
 *                       NOTE: The parent class must still be closed, but must only be
 *                       closed once. If all derived objects are deleted from the index,
 *                       but the class hasn't been closed its ID ref count will be 1 and
 *                       won't be closed and deleted from the index.
 *
 *          CLOSING_IN_PROGRESS
 *                       The class is in the process of being closed.
 * 
 * op_count ( _Atomic uint64_t ):
 *      An atomic uint64_t to track the number of operations perfomed on this class.
 * 
 * ver_closed ( _Atomic uint64_t ):
 *      An atomic variable to show what version the class was at when it was closed.
 * 
 * ver_deleted ( _Atomic uint64_t ):
 *      An atomic variable to show what version the class was at when it was deleted.
 * 
 *      NOTE: Due to derived objects incrementing their parent's ID ref count, when a
 *      class is closed, it may not be deleted. So this could be different from 
 *      ver_closed.
 * 
 * test_class_id ( int ):
 *      A local ID given to the class for easy identification during testing. This is 
 *      useful for debugging when a class hasn't been created yet and its id is still
 *      H5I_INVALID_HID (-1). This isn't atomic due to being hard coded and never 
 *      changes.
 *
 * parent_name ( const char * ):
 *      Pointer to a dynamically allocated string containing the name of the parent of
 *      this class. This field is not atomic, due to it being allocated and initialized
 *      during the setup for the multithread tests and doesn't ever change.
 *
 * parent_id ( _Atomic hid_t ):
 *      The id of this class's parent class. This field is atomic due to the reason
 *      stated in the field id description, but shouldn't actually need to be.
 *
 * parent_entry ( _Atomic(class_table_entry_t * ) ):
 *      Atomic pointer to the instance of class_table_entry_t that contains this class's
 *      parent class.
 *
 * copy ( bool ):
 *      A boolean field to represent if this class is a copy of another class. When a
 *      thread goes to create a copy of a class it will select a class where this field
 *      is TRUE.
 * 
 * og_id ( int ):
 *      The local ID of the original class_table_entry_t if this instance of 
 *      class_table_entry_t is a copy. This isn't atomic due to being hard coded and 
 *      never changes.
 *
 * prop_table ( prop_table_entry_t * ):
 *      The array of prop_table_entry_t pointers that store all of the possible
 *      properties that can be created for this class.
 *
 * num_prop_entries ( int32_t ):
 *      The number of potential properties in the prop_table.
 *
 ****************************************************************************************
 */
typedef struct class_table_entry_t class_table_entry_t; /* Forward declaration */

typedef struct class_table_entry_t {
    _Atomic H5P_mt_class_sptr_t class_sptr;
    const char                 *name;
    _Atomic hid_t               id;
    _Atomic status_t            status;
    _Atomic uint64_t            op_count;
    _Atomic uint64_t            ver_closed;
    _Atomic uint64_t            ver_deleted;

    int                            test_class_id; /* id given for easier identificaiton */
    const char                    *parent_name;
    _Atomic hid_t                  parent_id;
    _Atomic(class_table_entry_t *) parent_entry;

    bool             copy;
    int              og_id; 
    _Atomic uint64_t ver_copied;

    prop_table_entry_t *prop_table;
    uint32_t            num_prop_entries;

} class_table_entry_t;

/****************************************************************************************
 *
 * Structure: list_table_entry_t
 *
 * Description:
 *
 * A global array of list_table_entry_t (named list_table) is used to store an entry
 * for every possible list that can be created for the tests. These entries provide the
 * threads the information needed to perform operations on a list.
 *
 * Fields:
 *
 * list_sptr ( _Atomic H5P_mt_list_sptr_t )
 *      An atomic H5P_mt_list_sptr_t that will store the pointer for the H5P_mt_list_t
 *      structure this entry is used for, when that list is created.
 *
 * id ( _Atomic hid_t ):
 *      An atomic hid_t to store the H5I assigned ID of the list in the index.
 *
 * status ( _Atomic status_t ):
 *      An atomic structure that is used to show the current status of the
 *      H5P_mt_list_t.
 *          DOESNT EXIST The list hasn't been created yet.
 *          IN_PROGRESS  The list is in progress to be created.
 *          EXISTS       The list has been created and currently exists.
 *          DELETED      The list was created but has since been deleted.
 *          EXISTS_BUT_CLOSED Only used by classes
 *          CLOSING_IN_PROGRESS
 *                       The list is in the process of being closed.
 * 
 * op_count ( _Atomic uint64_t ):
 *      An atomic uint64_t to track the number of operations perfomed on this list.
 * 
 * ver_deleted ( _Atomic uint64_t ):
 *      An atomic variable to show what version the list was at when it was deleted.
 * 
 *      NOTE: Due to lists being deleted immediately after being closed, there is no
 *      field for ver_closed for lists.
 * 
 * test_list_id ( int ):
 *      A local ID given to the list for easy identification during testing. This is 
 *      useful for debugging when a list hasn't been created yet and its id is still
 *      H5I_INVALID_HID (-1). This isn't atomic due to being hard coded and never 
 *      changes.
 *
 * parent_name ( const char * ):
 *      Pointer to a dynamically allocated string containing the name of the parent of
 *      this list. This field is not atomic, due to it being allocated and initialized
 *      during the setup for the multithread tests and doesn't ever change.
 *
 * parent_id ( _Atomic hid_t ):
 *      The id of this list's parent class.
 *
 * parent_entry ( _Atomic(class_table_entry_t * ) ):
 *      Atomic pointer to the instance of class_table_entry_t that contains this list's
 *      parent class.
 *
 * copy ( bool ):
 *      A boolean field to represent if this list is a copy of another list. When a
 *      thread goes to create a copy of a list it will select a list where this field
 *      is TRUE.
 * 
 * og_id ( int ):
 *      The local ID of the original list_table_entry_t if this instance of 
 *      list_table_entry_t is a copy. This isn't atomic due to being hard coded and 
 *      never changes.
 *
 * prop_table ( prop_table_entry_t * ):
 *      The array of prop_table_entry_t pointers that store all of the possible
 *      properties that can be created for this list.
 *
 * num_prop_entries ( int32_t ):
 *      The number of potential properties in the prop_table.
 *
 ****************************************************************************************
 */
typedef struct list_table_entry_t list_table_entry_t; /* Forward declaration */

typedef struct list_table_entry_t {
    _Atomic H5P_mt_list_sptr_t list_sptr;
    _Atomic hid_t              id;
    _Atomic status_t           status;
    _Atomic uint64_t           op_count;
    _Atomic uint64_t           ver_deleted;

    int                            test_list_id; /* id given for easier identification */
    const char                    *parent_name;
    _Atomic hid_t                  parent_id;
    _Atomic(class_table_entry_t *) parent_entry;

    bool             copy;
    int              og_id; 
    _Atomic uint64_t ver_copied;

    prop_table_entry_t *prop_table;
    uint32_t            num_prop_entries;

} list_table_entry_t;


/**
 * Globals for st_test_1 and mt_test_1
 */
static prop_info_t *class_prop_table;
static prop_info_t *class2_prop_table;
static prop_info_t *list_prop_table;
static prop_info_t *list2_prop_table;
static prop_info_t *list3_prop_table;

/**
 * Globals for mt_prop_test2
 */

/* The current number of operations performed by all threads */
_Atomic uint64_t OPS_PERFORMED        = 0;
/* The total number of operations each thread will perform during each test iteration */
static uint64_t  TOTAL_OPS_PER_THREAD = 500;

/**
 * The global arrays that store all info for all 
 * possible classes and lists during the test 
 */
static class_table_entry_t class_table[24];
static list_table_entry_t  list_table[28];

#define CLASS_TABLE_SIZE 24
#define LIST_TABLE_SIZE  28

/**
 * Functions for st_test_1 and mt_test_1
 */

/* Initialization functions */
static herr_t init_globals(void);
static herr_t init_class_props(void);
static herr_t init_class2_props(void);
static herr_t init_list_props(void);
static herr_t init_list2_props(void);
static herr_t init_list3_props(void);

/* Function for creating a new root class for the tests */
static hid_t create_test_root_class(void);

/* Test functions */
static herr_t st_test_1(TestParams_t *params);
static herr_t mt_test_1(TestParams_t *params);
static void   test_1_helper(int num_threads);
static void  *test_h5p_mt_functions(void *test_params);

static herr_t test_h5p_mt_class_1(test_params_t *test_params);
static herr_t test_h5p_mt_class_2(test_params_t *test_params);
static herr_t test_h5p_mt_list_1(test_params_t *test_params);
static herr_t test_h5p_mt_list_2(test_params_t *test_params);

/**
 * Helper functions that are used to check structures or get correct
 * property structures from the correct table for comparisons.
 */
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

/**
 * Functions for checking stats after the tests are ran,
 * and property closing and freeing structures used
 */
static herr_t check_stats(test_params_t *test_params);
static herr_t check_global_stats(int num_threads);
static herr_t close_test_structs(test_params_t *test_params);
static herr_t term_test_free_lists(int num_threads);
static herr_t reset_globals(TestParams_t H5_ATTR_UNUSED *params);

/** TODO: Will eventually need test callback functions */

/**
 * Functions for mt_test_2
 */
static herr_t init_globals_2(void);
static herr_t reset_globals_2(void);

static herr_t create_starting_classes_and_lists(void);
prop_table_entry_t *search_prop_table(prop_table_entry_t *prop_table, uint32_t num_prop_entries,
                                      int64_t chksum, const char *name);

static herr_t mt_test_2(TestParams_t *params);
static void   test_2_helper(int num_threads);
static void  *h5p_full_cols_mt_test(void *thread_params);

static herr_t rand_op(thread_params_t *thread_params);
static herr_t create_list(thread_params_t *thread_params);
static herr_t create_class(thread_params_t *thread_params);
static herr_t copy_list(thread_params_t *thread_params);
static herr_t copy_class(thread_params_t *thread_params);
static herr_t read_list(thread_params_t *thread_params);
static herr_t read_class(thread_params_t *thread_params);
static herr_t write_list(thread_params_t *thread_params);
static herr_t write_class(thread_params_t *thread_params);
#if 0 /* No compare */
static herr_t cmp_list(thread_params_t *thread_params);
static herr_t cmp_class(thread_params_t *thread_params);
#endif
static herr_t close_list(thread_params_t *thread_params);
static herr_t close_class(thread_params_t *thread_params);

static herr_t search_list(thread_params_t *thread_params);
static herr_t search_list_ver(thread_params_t *thread_params);
static herr_t search_class(thread_params_t *thread_params);
static herr_t search_class_ver(thread_params_t *thread_params);
static herr_t mod_list_create_prop(thread_params_t *thread_params);
static herr_t mod_list_mod_prop(thread_params_t *thread_params);
static herr_t mod_list_delete_prop(thread_params_t *thread_params);
static herr_t mod_class_create_prop(thread_params_t *thread_params);
static herr_t mod_class_mod_prop(thread_params_t *thread_params);
static herr_t mod_class_delete_prop(thread_params_t *thread_params);
#if 0 /* No compare */
static herr_t cmp_list_equal(thread_params_t *thread_params);
static herr_t cmp_list_not_equal(thread_params_t *thread_params);
static herr_t cmp_class_equal(thread_params_t *thread_params);
static herr_t cmp_class_not_equal(thread_params_t *thread_params);
#endif

uint64_t               prop_ver_check_list(H5P_mt_list_t *list, prop_table_entry_t *prop_entry,
                                            uint64_t obj_ver, uint64_t curr_ver, bool *deleted,
                                            bool success);
static herr_t          check_operations(thread_params_t *thread_params, uint64_t num_threads);
test_op_info_t        *create_operation_log(uint64_t op_count);
static inline int      op_rank(operation_type_t op);
static test_op_info_t *sort_op_log(test_op_info_t *op_log, uint64_t op_count);
H5P_mt_prop_t         *get_prop_from_lfsll(H5P_mt_prop_t *pl_head, const char *name, uint64_t version);
H5P_mt_prop_t         *get_prop_from_lkup_tbl(H5P_mt_list_t *list, const char *name, uint64_t version,
                                              bool *base_flag);
herr_t verify_prop_in_class(H5P_mt_class_t *class, const char *name, uint64_t version, operation_type_t op,
                            op_result_t result);
herr_t verify_prop_in_list(H5P_mt_list_t *list, const char *name, uint64_t version, operation_type_t op,
                           op_result_t result);

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

    list_table_prop = list_prop_table[0].prop;
    next            = atomic_load(&(list_table_prop->next));
    list_table_prop = next.ptr;
    next            = atomic_load(&(list_table_prop->next));
    list_table_prop = next.ptr;
    value           = atomic_load(&(list_table_prop->value));

    /* This prop is inherited, but also deleted */
    list2_prop_table[0].prop = H5P__mt_create_prop(list_table_prop->name, value.ptr, value.size, FALSE,
                                                   atomic_load(&(list_table_prop->create_version)), NULL,
                                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_PTR(list2_prop_table[0].prop, "init_list2_props");
    assert(list2_prop_table[0].prop);

    /* Verify the fields are correct */
    test_prop = list2_prop_table[0].prop;
    VERIFY(atomic_load(&(test_prop->tag)), H5P_MT_PROP_TAG, "init_list_props");
    VERIFY(test_prop->chksum, list_prop_table[0].chksum, "init_list_props");
    VERIFY(test_prop->chksum, list2_prop_table[0].chksum, "init_list_props");
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

    test_prop->in_lkup_tbl = TRUE;
    atomic_store(&(test_prop->create_version), 1);
    atomic_store(&(test_prop->delete_version), 1);

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
    VERIFY(test_prop->chksum, list2_prop_table[1].chksum, "init_list_props");
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
    VERIFY(test_prop->chksum, list_prop_table[2].chksum, "init_list_props");
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
    VERIFY(test_prop->chksum, list_prop_table[3].chksum, "init_list_props");
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
    if (max_num_threads > DEFAULT_MAX_NUM_THREADS || max_num_threads < 0) {
        max_num_threads = DEFAULT_MAX_NUM_THREADS;
    }

    /* Run this test for thread counts between and including 2 <-> max_num_threads */
    for (int num_threads = 2; num_threads <= max_num_threads; num_threads++) {
        /* Reset global stats before running tests */
        ret = H5P__reset_stats_global();
        CHECK_I(ret, "H5P__reset_stats_global");

        test_1_helper(num_threads);
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
    H5P_mt_class_t *test_root = NULL;
    char            banner[80];
    int             i;
    int             err_cnt = 0;
    pthread_t       threads[DEFAULT_MAX_NUM_THREADS];
    test_params_t   params[DEFAULT_MAX_NUM_THREADS];
    herr_t          ret;

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
    CHECK_I(ret, "check_global_stats");

    /* Close all class and list structs used in testing and check stats */
    for (i = 0; i < num_threads; i++) {
        ret = close_test_structs(&(params[i]));
        CHECK_I(ret, "clear_free_lists");
    }

    /* Get the test root class from index */
    test_root = (H5P_mt_class_t *)H5I_object(TEST_ROOT_ID_g);
    CHECK_PTR(test_root, "H5I_object");
    assert(test_root);
    assert(atomic_load(&(test_root->tag)) == H5P_MT_CLASS_TAG);

    // ret = H5P__mt_close_class(test_root);

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
    uint64_t            version;
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
    version = atomic_load(&(class1->curr_version));

    /* Ensure class fields are correct */
    if (0 > class_ver_and_len_check(class1, version, (version + 1), 0, 3, 5, "H5P__register_real")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_1(): class fields are incorrect.");

        return -1;
    }

    /* Search for default prop1 */
    if (NULL == (prop1 = H5P__mt_search__class(class1, class_prop_table[0].name, version))) {
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
    if (NULL == (prop2 = H5P__mt_search__class(class1, class_prop_table[1].name, version))) {
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
    if (NULL == (prop3 = H5P__mt_search__class(class1, class_prop_table[2].name, version))) {
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
                                       TRUE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL) < 0) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to create and insert prop4.");
        return -1;
    }

    version = atomic_load(&(class1->curr_version));

    /* Ensure class fields are correct */
    if (0 >
        class_ver_and_len_check(class1, version, (version + 1), 1, 4, 6, "H5P__mt_ins_or_mod_prop__class")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_1(): class fields are incorrect.");
        return -1;
    }

    /**
     * Search for the new property
     */

    /* Search for prop4 */
    if (NULL == (test_prop = H5P__mt_search__class(class1, class_prop_table[3].name, version))) {
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
                                       FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL) < 0) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to 'modify' prop1.");
        return -1;
    }

    version = atomic_load(&(class1->curr_version));

    /* Ensure class fields are correct */
    if (0 >
        class_ver_and_len_check(class1, version, (version + 1), 1, 4, 7, "H5P__mt_ins_or_mod_prop__class")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_1(): class fields are incorrect.");
        return -1;
    }

    /* Search for modified prop1, which has two versions (should find newest version) */

    if (NULL == (test_prop = H5P__mt_search__class(class1, class_prop_table[0].name, version))) {
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
                                       FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL) < 0) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to 'modify' prop4.");
        return -1;
    }

    version = atomic_load(&(class1->curr_version));

    /* Ensure class fields are correct */
    if (0 >
        class_ver_and_len_check(class1, version, (version + 1), 1, 4, 8, "H5P__mt_ins_or_mod_prop__class")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_1(): class fields are incorrect.");
        return -1;
    }

    /* Search for modified prop4, which has two versions (should find newest version) */

    if (NULL == (test_prop = H5P__mt_search__class(class1, class_prop_table[3].name, version))) {
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

    version = atomic_load(&(class1->curr_version));

    /* Ensure class fields are correct */
    if (0 > class_ver_and_len_check(class1, version, (version + 1), 1, 3, 8, "H5P__mt_delete_prop__class")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_1(): class fields are incorrect.");
        return -1;
    }

    /**
     * Search for deleted prop2. Should FAIL
     */

    H5E_BEGIN_TRY
    {
        test_prop = H5P__mt_search__class(class1, class_prop_table[1].name, version);
    }
    H5E_END_TRY
    CHECK_PTR_NULL(test_prop, "H5P__mt_search__class");
    assert(!test_prop);

    /**
     * Delete a modified property (property with multiple vesrions)
     */

    /* Delete modified prop1*/
    if (H5P__mt_delete_prop__class(class1, class_prop_table[0].name) < 0) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to delete modified prop1.");
        return -1;
    }

    version = atomic_load(&(class1->curr_version));

    /* Ensure class fields are correct */
    if (0 > class_ver_and_len_check(class1, version, (version + 1), 1, 2, 8, "H5P__mt_delete_prop__class")) {
        assert(class1 == test_root);

        fprintf(stderr, "test_h5p_mt_class_1(): class fields are incorrect.");
        return -1;
    }

    /**
     * Search for deleted prop1. Should FAIL even
     * though a non-deleted older version exists
     */

    H5E_BEGIN_TRY
    {
        test_prop = H5P__mt_search__class(class1, class_prop_table[0].name, version);
    }
    H5E_END_TRY
    CHECK_PTR_NULL(test_prop, "H5P__mt_search__class");
    assert(!test_prop);

    /**
     * Delete an added property
     */
    if (H5P__mt_delete_prop__class(class1, class_prop_table[3].name) < 0) {
        fprintf(stderr, "test_h5p_mt_class_1(): Failed to delete modified prop1.");
        return -1;
    }

    version = atomic_load(&(class1->curr_version));

    /* Ensure class fields are correct */
    if (0 > class_ver_and_len_check(class1, version, (version + 1), 0, 1, 8, "H5P__mt_delete_prop__class")) {
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

    ret = H5P__mt_ins_or_mod_prop__class(class1, table_prop->name, table_value.ptr, table_value.size, TRUE,
                                         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__class");

    atomic_store(&(table_prop->create_version), atomic_load(&(class1->curr_version)));

    table_prop  = get_table_prop_ver(class_prop_table[1], 2);
    table_value = atomic_load(&(table_prop->value));

    ret = H5P__mt_ins_or_mod_prop__class(class1, table_prop->name, table_value.ptr, table_value.size, TRUE,
                                         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__class");

    atomic_store(&(table_prop->create_version), atomic_load(&(class1->curr_version)));

    version = atomic_load(&(class1->curr_version));

    ret = class_ver_and_len_check(class1, version, (version + 1), 2, 3, 10, "H5P__mt_ins_or_mod_prop__class");
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
    H5P_mt_class_t     *test_class;
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
    uint64_t            class1_ver;
    uint64_t            class2_ver;
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
    class1_ver = atomic_load(&(class1->curr_version));
    class2_ver = atomic_load(&(class2->curr_version));

    if (0 != H5P__mt_cmp_class(class1, class1_ver, class2, class2_ver)) {
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
                                              FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) <
        0) {

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
    class1_ver = atomic_load(&(class1->curr_version));
    class2_ver = atomic_load(&(class2->curr_version));

    if (1 != H5P__mt_cmp_class(class1, class1_ver, class2, class2_ver)) {
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
                                              FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) <
        0) {
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
    class1_ver = atomic_load(&(class1->curr_version));
    class2_ver = atomic_load(&(class2->curr_version));

    if (0 != H5P__mt_cmp_class(class1, class1_ver, class2, class2_ver)) {
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

    /* Ensure class2 was removed from the index */
    H5E_BEGIN_TRY
    {
        test_class = H5I_object(atomic_load(&(class2->id)));
    }
    H5E_END_TRY

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

        CHECK_PTR_NULL(test_class, "H5I_object");
        assert(!test_class);

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
    uint64_t                   version;
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

    version = atomic_load(&(list1->curr_version));

    /* Ensure list fields are correct */
    ret = list_ver_and_len_check(list1, version, (version + 1), 3, 0, 3, 0, 2, "H5P__mt_create_list");

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
    test_prop = H5P__mt_search__list(list1, list_prop_table[0].name, version);
    CHECK_PTR(test_prop, "H5P__mt_search__list");
    assert(test_prop);
    assert(atomic_load(&(test_prop->tag)) == H5P_MT_PROP_TAG);

    ret = prop_check(test_prop, list_prop_table[0].prop, TRUE, FALSE);
    CHECK_I(ret, "H5P__mt_search__list");

    VERIFY(atomic_load(&(test_prop->ref_count)), 1, "H5P__mt_create_list");

    /* Search for and check prop2 */
    test_prop = H5P__mt_search__list(list1, list_prop_table[1].name, version);
    CHECK_PTR(test_prop, "H5P__mt_search__list");
    assert(test_prop);
    assert(atomic_load(&(test_prop->tag)) == H5P_MT_PROP_TAG);

    ret = prop_check(test_prop, list_prop_table[1].prop, TRUE, FALSE);
    CHECK_I(ret, "H5P__mt_search__list");

    VERIFY(atomic_load(&(test_prop->ref_count)), 1, "H5P__mt_create_list");

    /* Search for and check prop3 */
    test_prop = H5P__mt_search__list(list1, list_prop_table[2].name, version);
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
                                        FALSE, TRUE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    version = atomic_load(&(list1->curr_version));

    /* Check list1's fields */
    ret =
        list_ver_and_len_check(list1, version, (version + 1), 3, 1, 4, 1, 3, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Update the list's table_prop create_version to match */
    atomic_store(&(table_prop->create_version), atomic_load(&(list1->curr_version)));

    /**
     * Search for the new prop4 in list1 which tests the property
     * not being in the lkup_tbl and having to search the LFSLL
     */

    test_prop = H5P__mt_search__list(list1, table_prop->name, version);
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
                                        FALSE, FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    version = atomic_load(&(list1->curr_version));

    /* Ensure list1's fields are correct */
    ret =
        list_ver_and_len_check(list1, version, (version + 1), 3, 1, 4, 2, 4, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Update the table_prop create_verson to match */
    atomic_store(&(table_prop->create_version), version);

    /**
     * Search for the modified prop1, which tests searching for a prop in
     * the lkup_tbl with the most recent version being an entry's curr
     */
    test_prop = H5P__mt_search__list(list1, table_prop->name, version);
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
                                        FALSE, FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_porp__list");

    version = atomic_load(&(list1->curr_version));

    /* Ensure list1's fields are correct */
    ret =
        list_ver_and_len_check(list1, version, (version + 1), 3, 1, 4, 2, 5, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P_mt_ins_or_mod_prop__list");

    /* Update the table_prop create_version to match */
    atomic_store(&(table_prop->create_version), version);

    /**
     * Search for the modified prop1, tests same as
     * previous but ensures curr was updated correctly.
     */

    test_prop = H5P__mt_search__list(list1, table_prop->name, version);
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

    version = atomic_load(&(list1->curr_version));

    /* Ensure list1's fields are correct */
    ret =
        list_ver_and_len_check(list1, version, (version + 1), 3, 1, 3, 2, 5, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_delete_prop__list");

    /* Ensure the entry's base_delete_version is set */
    entry = &list1->lkup_tbl[2];
    VERIFY(atomic_load(&(entry->base_delete_version)), atomic_load(&(list1->curr_version)),
           "H5P__mt_delete_prop__list");
    assert(atomic_load(&(entry->base_delete_version)) == atomic_load(&(list1->curr_version)));

    /**
     * Search for deleted prop3. Should FAIL.
     */
    H5E_BEGIN_TRY
    {
        test_prop = H5P__mt_search__list(list1, list_prop_table[2].name, version);
    }
    H5E_END_TRY
    CHECK_PTR_NULL(test_prop, "H5P__mt_search__list");
    assert(!test_prop);

    /**
     * Delete a property in the lkup_tbl where the most recent
     * version is curr.
     */
    ret = H5P__mt_delete_prop__list(list1, list_prop_table[0].name);
    CHECK_I(ret, "H5P__mt_delete_prop__list");

    version = atomic_load(&(list1->curr_version));

    /* Ensure list1's fields are correct */
    ret =
        list_ver_and_len_check(list1, version, (version + 1), 3, 1, 2, 1, 5, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_delete_prop__list");

    /**
     * Search for deleted prop1. Should FAIL.
     */
    H5E_BEGIN_TRY
    {
        test_prop = H5P__mt_search__list(list1, list_prop_table[0].name, version);
    }
    H5E_END_TRY
    CHECK_PTR_NULL(test_prop, "H5P__mt_search__list");
    assert(!test_prop);

    /**
     * Delete an added property (non-inherited) from list1.
     */
    ret = H5P__mt_delete_prop__list(list1, list_prop_table[3].name);
    CHECK_I(ret, "H5P__mt_delete_prop__list");

    version = atomic_load(&(list1->curr_version));

    /* Ensure list1's fields are correct */
    ret =
        list_ver_and_len_check(list1, version, (version + 1), 3, 0, 1, 0, 5, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_delete_prop__list");

    /**
     * Search for deleted prop4. Should FAIL.
     */
    H5E_BEGIN_TRY
    {
        test_prop = H5P__mt_search__list(list1, list_prop_table[3].name, version);
    }
    H5E_END_TRY
    CHECK_PTR_NULL(test_prop, "H5P__mt_search__list");
    assert(!test_prop);

    /**
     * Right now list1 doesn't have any valid properties in the LFSLL.
     * Add a new one, and add a new version of prop3, an inherited
     * property that was deleted, for further testing.
     */

    table_prop  = get_table_prop_ver(list_prop_table[3], 2);
    table_value = atomic_load(&(table_prop->value));

    /* Create and insert a new version of deleted prop4 */
    ret = H5P__mt_ins_or_mod_prop__list(list1, table_prop->name, table_value.ptr, table_value.size, FALSE,
                                        FALSE, FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    version = atomic_load(&(list1->curr_version));

    /* Check list1's fields */
    ret =
        list_ver_and_len_check(list1, version, (version + 1), 3, 1, 2, 1, 6, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Update the table_prop's create_version to match */
    atomic_store(&(table_prop->create_version), version);

    /**
     * Search for the new prop4 in list1
     */
    test_prop = H5P__mt_search__list(list1, table_prop->name, version);
    CHECK_PTR(test_prop, "H5P__mt_search__list");

    ret = prop_check(test_prop, table_prop, table_prop->in_prop_class, table_prop->in_lkup_tbl);
    CHECK_I(ret, "H5P__mt_search__list");

    /* Create and insert a new version of the deleted inherited prop3 */
    table_prop  = get_table_prop_ver(list_prop_table[2], 2);
    table_value = atomic_load(&(table_prop->value));

    ret = H5P__mt_ins_or_mod_prop__list(list1, table_prop->name, table_value.ptr, table_value.size, FALSE,
                                        FALSE, FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    version = atomic_load(&(list1->curr_version));

    /* Check list1's fields */
    ret =
        list_ver_and_len_check(list1, version, (version + 1), 3, 1, 3, 2, 7, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Update the table_prop's create_version to match */
    atomic_store(&(table_prop->create_version), version);

    /**
     * Search for the new prop3 in list1
     */
    test_prop = H5P__mt_search__list(list1, table_prop->name, version);
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
 *  5) Modify a property in list2's lkup_tbl and compare list1 and list2 again (should
 *     not be equal).
 *  6) Modify the property in list2 back and compare list1 and list2 again (should be
 *     equal).
 *  7) Close list2 and ensure it was inserted into the list free list correctly.
 *  8) Change the closed list2's tag to be reallocable and derive a new list3 from
 *     class2.
 *     NOTE: step 6 and 7 are only done when testing with a single thread. If testing
 *     with multiple threads a new structure is allocated from the heap.
 *  9) Ensure the new list3 used the old list2 structure from the list free list and that
 *     the list free list is now empty (NOTE: the list free list will always contain two
 *     H5P_mt_list_sptr_t structs for the head and tail of that list, and if the pointers
 *     are NULL then the free list is "empty").
 * 10) Check the property free list and ensure all the H5P_mt_prop_t, property structs,
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
    uint64_t            version;
    uint64_t            list1_ver;
    uint64_t            list2_ver;
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

    version = atomic_load(&(list2->curr_version));

    /* Ensure list2's fields are correct */
    ret = list_ver_and_len_check(list2, version, (version + 1), 3, 1, 3, 2, 5, "H5P__mt_create_list");
    CHECK_I(ret, "H5P__mt_create_list");

    /* Check class1's ref_counts */
    ret = check_class_ref_counts(class1, 2, 1, FALSE, "H5P__mt_create_list");
    CHECK_I(ret, "H5P__mt_create_list");

    /* Check list2's thrd flags */
    ret = check_and_set_thrd_flags(list2, FALSE, FALSE, FALSE, FALSE, "H5P__mt_create_list");
    CHECK_I(ret, "H5P__mt_create_list");

    /**
     * Search for and check prop1, which was deleted from list1 before
     * list2 was copied, thus prop1 should also be deleted in list2.
     */

    test_prop = H5P__mt_search__list(list2, list2_prop_table[0].name, version);
    CHECK_PTR_NULL(test_prop, "H5P__mt_search__list");
    assert(!test_prop);

    /* Search for and check prop2 */
    table_prop = list2_prop_table[1].prop;

    test_prop = H5P__mt_search__list(list2, table_prop->name, version);
    CHECK_PTR(test_prop, "H5P__mt_search__list");
    assert(test_prop);
    assert(atomic_load(&(test_prop->tag)) == H5P_MT_PROP_TAG);

    ret = prop_check(test_prop, table_prop, table_prop->in_prop_class, table_prop->in_lkup_tbl);
    CHECK_I(ret, "H5P__mt_search__list");

    VERIFY(atomic_load(&(test_prop->ref_count)), 2, "H5P__mt_create_list");

    /* Search for and check prop3 */
    table_prop = list2_prop_table[2].prop;

    test_prop = H5P__mt_search__list(list2, table_prop->name, version);
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

    version = atomic_load(&(class1->curr_version));

    test_prop = H5P__mt_search__class(class1, table_prop->name, version);
    CHECK_PTR(test_prop, "H5P__mt_search__class");
    assert(test_prop);
    assert(atomic_load(&(test_prop->tag)) == H5P_MT_PROP_TAG);

    /* The base was deleted when copied from list1, thus ref_count should be unchaged */
    VERIFY(atomic_load(&(test_prop->ref_count)), 1, "H5P__mt_create_list");

    /**
     * Compare list1 and the copy, list2. (They should be equal)
     */
    list1_ver = atomic_load(&(list1->curr_version));
    list2_ver = atomic_load(&(list2->curr_version));

    ret = H5P__mt_cmp_list(list1, list1_ver, list2, list2_ver);
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
                                        FALSE, FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    version = atomic_load(&(list2->curr_version));

    /* Update table_prop version to match */
    atomic_store(&(table_prop->create_version), version);

    /* Ensure list2's fields are correct */
    ret =
        list_ver_and_len_check(list2, version, (version + 1), 3, 1, 3, 2, 6, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Compare list1 and list2 (should not be equal) */
    list1_ver = atomic_load(&(list1->curr_version));
    list2_ver = atomic_load(&(list2->curr_version));

    ret = H5P__mt_cmp_list(list1, list1_ver, list2, list2_ver);
    VERIFY(ret, 1, "H5P__mt_cmp_list");

    /**
     * Change the modified property's value back and compare
     * list1 and list2 again. (Should be equal)
     */

    table_prop  = get_table_prop_ver(list2_prop_table[3], 3);
    table_value = atomic_load(&(table_prop->value));

    /* Create and insert prop3 with original value */
    ret = H5P__mt_ins_or_mod_prop__list(list2, table_prop->name, table_value.ptr, table_value.size, FALSE,
                                        FALSE, FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    version = atomic_load(&(list2->curr_version));

    /* Update table_prop version to match */
    atomic_store(&(table_prop->create_version), version);

    /* Ensure list2's fields are correct */
    ret =
        list_ver_and_len_check(list2, version, (version + 1), 3, 1, 3, 2, 7, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Compare list1 and list2 (Should be equal) */
    list1_ver = atomic_load(&(list1->curr_version));
    list2_ver = atomic_load(&(list2->curr_version));

    ret = H5P__mt_cmp_list(list1, list1_ver, list2, list2_ver);
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
                                        FALSE, FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    version = atomic_load(&(list2->curr_version));

    /* Update table_prop version to match */
    atomic_store(&(table_prop->create_version), version);

    /* Ensure list2's fields are correct */
    ret =
        list_ver_and_len_check(list2, version, (version + 1), 3, 1, 3, 3, 8, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Compare list1 and list2 (should not be equal) */
    list1_ver = atomic_load(&(list1->curr_version));
    list2_ver = atomic_load(&(list2->curr_version));

    ret = H5P__mt_cmp_list(list1, list1_ver, list2, list2_ver);
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
                                        FALSE, FALSE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    version = atomic_load(&(list2->curr_version));

    /* Update table_prop version to match */
    atomic_store(&(table_prop->create_version), version);

    /* Ensure list2's fields are correct */
    ret =
        list_ver_and_len_check(list2, version, (version + 1), 3, 1, 3, 3, 9, "H5P__mt_ins_or_mod_prop__list");
    CHECK_I(ret, "H5P__mt_ins_or_mod_prop__list");

    /* Compare list1 and list2 (Should be equal) */
    list1_ver = atomic_load(&(list1->curr_version));
    list2_ver = atomic_load(&(list2->curr_version));

    ret = H5P__mt_cmp_list(list1, list1_ver, list2, list2_ver);
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

        VERIFY(atomic_load(&(H5P_mt_g.prop_fl_len)), 15, "H5P__mt_create_list");

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

        /* Ensure the sentinel prop the free list stopped at is correct */
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
    assert(atomic_load(&(class1->H5P__search_prop__class__num_calls)) == 12);
    assert(atomic_load(&(class1->search_class__max_nodes_visited)) == 8);
    assert(atomic_load(&(class1->search_class__avg_nodes_visited)) == 4);
    assert(atomic_load(&(class1->num_search_class__nodes_visited)) > 0);
    assert(atomic_load(&(class1->num_search_class__success)) == 7);
    assert(atomic_load(&(class1->num_search_chksum_cols)) == 0);

    /* Version stats */
    assert(atomic_load(&(class1->num_wait_for_curr_version_to_inc)) == 0);

    /* H5P_mt_active_thread_count_t stats */
    assert(atomic_load(&(class1->num_thrd_update_cols)) == 0);
    assert(atomic_load(&(class1->num_thrd_count_update)) == 58);
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
    assert(atomic_load(&(list1->H5P__search_prop__list__num_calls)) == 12);
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
    assert(atomic_load(&(list1->num_thrd_count_update)) == 52);
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
        assert(atomic_load(&(H5P_mt_g.prop_fl_tail_update)) == 16);
        assert(atomic_load(&(H5P_mt_g.prop_fl_next_update)) == 15);
        assert(atomic_load(&(H5P_mt_g.num_props_added_to_fl)) == 16);
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
    
    if ( num_threads <= 32 )
    {
        assert(atomic_load(&(H5P_mt_g.list_fl_head_free_skipped_no_reallocable)) == 0);
    }
    else
    {
        assert(atomic_load(&(H5P_mt_g.list_fl_head_free_skipped_no_reallocable)) == 
                                                                    num_threads - 32);
    }

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
    assert(atomic_load(&(H5P_mt_g.H5P__mt_create_prop__num_calls)) == (28 * num_threads));
    if (num_threads == 1) {
        assert(atomic_load(&(H5P_mt_g.num_prop_structs_allocated_from_heap)) == 39);
        assert(atomic_load(&(H5P_mt_g.num_prop_structs_allocated_from_fl)) == 1);
    }
    else {
        assert(atomic_load(&(H5P_mt_g.num_prop_structs_allocated_from_heap)) ==
               (39 * num_threads + num_threads));
        assert(atomic_load(&(H5P_mt_g.num_prop_structs_allocated_from_fl)) == 0);
    }

    /* stats for property inserts */
    assert(atomic_load(&(H5P_mt_g.num_props_inserted_classes)) == (7 * num_threads));
    assert(atomic_load(&(H5P_mt_g.num_props_inserted_lists)) == (9 * num_threads));
    assert(atomic_load(&(H5P_mt_g.H5P__mt_ins_or_mod_prop__lfsll_ins__num_calls)) == (28 * num_threads));

    /* stats for number of deletes */
    assert(atomic_load(&(H5P_mt_g.num_props_deleted_classes)) == (3 * num_threads));
    assert(atomic_load(&(H5P_mt_g.num_props_deleted_lists)) == (3 * num_threads));

    /* stats for searches */
    assert(atomic_load(&(H5P_mt_g.num_searches_classes)) == (12 * num_threads));
    assert(atomic_load(&(H5P_mt_g.num_searches_classes_prop_not_found)) == (5 * num_threads));
    assert(atomic_load(&(H5P_mt_g.num_searches_while_an_op_occurs_class)) == (3 * num_threads));
    assert(atomic_load(&(H5P_mt_g.num_searches_lists)) == (15 * num_threads));
    assert(atomic_load(&(H5P_mt_g.num_searches_lists_prop_not_found)) == (5 * num_threads));

    assert(atomic_load(&(H5P_mt_g.num_searches_while_an_op_occurs_list)) == (1 * num_threads));

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
    assert(atomic_load(&(H5P_mt_g.max_list_num_phys_props)) == 9);
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
 *  1) Class1 is closed first to ensure it is not removed from the index, because when
 *     a derived class or list is made the parent class's index ref count is
 *     incremented. Thus, this class should still be in the index after calling close,
 *     due to having an existing derived class and list.
 *  4) List1 is closed, which should be removed from the index and closed decrementing
 *     class1's pl ref_count.
 *  5) List3 is closed, which should be removed from the index and closed decrementing
 *     class2's pl ref_count.
 *     NOTE: list2 was closed previously during testing.
 *  6) Class2 is closed, which should remove it from the index and fully close it,
 *     since it no longer has any derived classes or lists. Once class2 is closed, class1
 *     should automatically finish being closed, since it is marked deleted and its last
 *     derived class just closed.
 *
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
     * Close class1, which will decrement class1's ID's ref_count in the index,
     * but since it has a derived list and class it won't get removed from the
     * index yet.
     */

    ret = H5Pclose_class(atomic_load(&(class1->id)));
    CHECK_I(ret, "H5Pclose_class");

    /* Ensure class1 is not marked deleted */
    ret = check_class_ref_counts(class1, 1, 1, FALSE, "H5Pclose_class");

    /* Ensure class1 is still in the index */
    h5i_class_ret = H5I_object(atomic_load(&(class1->id)));
    CHECK_PTR(h5i_class_ret, "H5I_object");

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
    ret = check_class_ref_counts(class1, 0, 1, FALSE, "H5Pclose");

    /* Ensure class1 isstill in the index */
    h5i_class_ret = (H5P_mt_class_t *)H5I_object(atomic_load(&(class1->id)));
    CHECK_PTR(h5i_class_ret, "H5I_object");

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
    h5i_class_ret = (H5P_mt_class_t *)H5I_object(atomic_load(&(class2->id)));
    CHECK_PTR(h5i_class_ret, "H5I_object");
    VERIFY(h5i_class_ret, class2, "H5Pclose");

    /* Ensure list3 is no longer in the index */
    h5i_list_ret = H5I_object(atomic_load(&(list3->plist_id)));
    CHECK_PTR_NULL(h5i_list_ret, "H5I_object");

    /* Ensure class2's ref_count is correct */
    ret = check_class_ref_counts(class2, 0, 0, FALSE, "close_test_structs");
    CHECK_I(ret, "check_class_ref_counts");

    /* Ensure class2 is not marked closing */
    ret = check_and_set_thrd_flags(class2, FALSE, FALSE, FALSE, FALSE, "H5Pclose");
    CHECK_I(ret, "H5Pclose");

    /* Ensure list3 is marked closing */
    ret = check_and_set_thrd_flags(list3, FALSE, TRUE, FALSE, TRUE, "H5Pclose");
    CHECK_I(ret, "H5Pclose");

    /**
     * Close class2, which now that it has no derived lists or classes
     * should be removed from the index and closed. Also, since
     * H5Pclose_class has been called on class1 already, once class2 is
     * closed, class1 should also be closed automatically.
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
    assert(atomic_load(&(class1->num_thrd_count_update)) == 66);
    assert(atomic_load(&(class1->num_thrd_closing_flag_set)) == 1);
    assert(atomic_load(&(class1->num_ref_count_update)) == 7);
    assert(atomic_load(&(class1->num_ref_count_marked_deleted)) == 1);

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

    /** TODO: add a check to ensure the free lists are empty */

    /* Check all the global stats that have changed */
    if (num_threads == 1) {
        assert(atomic_load(&(H5P_mt_g.prop_fl_len)) == 0);
        assert(atomic_load(&(H5P_mt_g.class_fl_len)) == 0);
        assert(atomic_load(&(H5P_mt_g.list_fl_len)) == 0);
        assert(atomic_load(&(H5P_mt_g.prop_fl_head_update)) == 41);
        assert(atomic_load(&(H5P_mt_g.prop_fl_tail_update)) == 41);
        assert(atomic_load(&(H5P_mt_g.prop_fl_next_update)) == 39);
        assert(atomic_load(&(H5P_mt_g.num_props_added_to_fl)) == 40);
        assert(atomic_load(&(H5P_mt_g.class_fl_head_update)) == 5);
        assert(atomic_load(&(H5P_mt_g.class_fl_tail_update)) == 5);
        assert(atomic_load(&(H5P_mt_g.class_fl_next_update)) == 1);
        assert(atomic_load(&(H5P_mt_g.num_class_added_to_fl)) == 3);
        assert(atomic_load(&(H5P_mt_g.list_fl_head_update)) == 5);
        assert(atomic_load(&(H5P_mt_g.list_fl_tail_update)) == 5);
        assert(atomic_load(&(H5P_mt_g.list_fl_next_update)) == 1);
        assert(atomic_load(&(H5P_mt_g.num_list_added_to_fl)) == 3);
        assert(atomic_load(&(H5P_mt_g.num_classes_freed)) == 3);
        assert(atomic_load(&(H5P_mt_g.num_lists_freed)) == 3);
        assert(atomic_load(&(H5P_mt_g.num_props_freed)) == 40);
    }
    else {
        assert(atomic_load(&(H5P_mt_g.prop_fl_len)) == 0);
        assert(atomic_load(&(H5P_mt_g.class_fl_len)) == 0);
        assert(atomic_load(&(H5P_mt_g.list_fl_len)) == 0);
        assert(atomic_load(&(H5P_mt_g.prop_fl_head_update)) == (40 * num_threads + 1));
        assert(atomic_load(&(H5P_mt_g.prop_fl_tail_update)) == (40 * num_threads + 1));
        assert(atomic_load(&(H5P_mt_g.prop_fl_next_update)) == (40 * num_threads - 1));
        assert(atomic_load(&(H5P_mt_g.num_props_added_to_fl)) == (40 * num_threads));
        assert(atomic_load(&(H5P_mt_g.class_fl_head_update)) == (3 * num_threads + 1));
        assert(atomic_load(&(H5P_mt_g.class_fl_tail_update)) == (3 * num_threads + 1));
        assert(atomic_load(&(H5P_mt_g.class_fl_next_update)) == (3 * num_threads - 1));
        assert(atomic_load(&(H5P_mt_g.num_class_added_to_fl)) == (3 * num_threads));
        assert(atomic_load(&(H5P_mt_g.list_fl_head_update)) == (3 * num_threads + 1));
        assert(atomic_load(&(H5P_mt_g.list_fl_tail_update)) == (3 * num_threads + 1));
        assert(atomic_load(&(H5P_mt_g.list_fl_next_update)) == (3 * num_threads - 1));
        assert(atomic_load(&(H5P_mt_g.num_list_added_to_fl)) == (3 * num_threads));
        assert(atomic_load(&(H5P_mt_g.num_classes_freed)) == (3 * num_threads));
        assert(atomic_load(&(H5P_mt_g.num_lists_freed)) == (3 * num_threads));
        assert(atomic_load(&(H5P_mt_g.num_props_freed)) == (40 * num_threads));
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

/** mt_test_2 functions start */

/****************************************************************************************
 * Function:    init_globals_2
 *
 * Purpose:     Initializes the global variables, class_table and list_table.
 *
 *              This function contains several arrays that store the names of all the
 *              potential classes, and the names of all the potential properties for all
 *              classes and lists that can be created during the tests.
 *
 *              Array entry names that end in an a or b, is a class that is a copy of the
 *              class with the same number.
 *              (class_4a is a copy of class_4)
 *              (list_4a is a list derived from the copy version of the class, class_4a)
 *
 *              Array entry names that end in a z, ex. list_3z is a list that is a copy
 *              of the list with the same number.
 *              (list_3z is a copy of list_3).
 *
 *              NOTE: To make tracking operations easier, only a select number of
 *              specified classes and lists are set up to be copies of another class or
 *              list. This makes it easier to differentiate between creating a new object
 *              or copying an existing one. Thus, during create_class() and create_list()
 *              if the randomly chosen entry is marked as a copy a new one is selected.
 *              During copy_class() and copy_list() only ones set as copy are selected.
 *
 *              NOTE: To help with debugging, the names of the property list classes are
 *              names of video game consoles, and the names of the properties are names
 *              from a list of top rated games released on that video game console. If a 
 *              game was released on multiple console I only included it on either the 
 *              first console it was released on, or if it was released at the same time 
 *              I just picked one console for it. This is to prevent multiple classes or 
 *              lists from having the same property names, unless a list or class inherits 
 *              it from its parent class, or the list or class is a copy.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
init_globals_2(void)
{
    class_table_entry_t *class_table_entry;
    list_table_entry_t  *list_table_entry;
    prop_table_entry_t  *prop_table_entry;
    H5P_mt_class_sptr_t  class_sptr = {NULL, 0};
    H5P_mt_list_sptr_t   list_sptr  = {NULL, 0};
    int                  class_id   = 0;
    int                  list_id    = 100;

    herr_t ret_value = SUCCEED;

    /* Names of all possible classes that can be created during the test */
    
    char class_names[CLASS_TABLE_SIZE][20] = {
        "NES",         "Sega Genesis", "Gameboy",          "SNES",          "SNES",           "Sega Saturn",
        "Playstation", "Playstation",  "Nintendo 64",      "Gameboy Color", "Sega Dreamcast", "PS2",
        "PS2",         "GameCube",     "Gameboy Advanced", "Xbox",          "Xbox 360",       "PS3",
        "Wii",         "Wii U",        "PS4/Xbox One",     "PS4/Xbox One",  "PS4/Xbox One",   "Switch"};



    /* Names of all possible properties in each possible class */

    char class_1[10][25] = {/* NES */
        "Super Mario Bros. 3", "Mega Man 2",        "The Legend of Zelda",
        "Mega Man 3",          "Punch-Out!!",       "Contra",
        "Super Mario Bros.",   "Kirby's Adventure", "Castlevania III",
        "Dragon Quest IV"};

    char class_2[10][25] = {/* Genesis */
        "Sonic 3",     "Streets of Rage II",     "Gunstar Heroes", "Strider",
        "Shinobi III", "Castlevania Bloodlines", "Ristar",         "Rocket Knight Adventures",
        "Aladdin",     "Mortal Kombat"};

    char class_3[10][25] = {/* Gameboy */
        "Zelda: Link's Awakening", "Tetris",         "Donkey Kong",        "Wario Land",
        "Pokemon Red & Blue",      "Pokemon Yellow", "Super Mario Land 2", "Wario Land II",
        "Kirby's Dream Land 2",    "Castlevania II"};

    char class_4[10][25] = {/* SNES */
        "Super Mario World", "A link to the Past",  "Super Metroid",         "Chrono Trigger",
        "Final Fantasy III", "Super Mario World 2", "Super Mario All-Stars", "Super Mario RPG",
        "DK Country 2",      "EarthBound"};

    char class_4a[15][25] = {/* SNES copy */
        "Super Mario World",     "A link to the Past", "Super Metroid",
        "Chrono Trigger",        "Final Fantasy III",  "Super Mario World 2",
        "Super Mario All-Stars", "Super Mario RPG",    "DK Country 2",
        "EarthBound",            "Terranigma",         "DK Country 3",
        "Tetris Attack",         "Illusion of Gaia",   "F-Zero"};

    char class_5[1][25] = {"Sega Rally Championship"}; /* Saturn */

    char class_6[10][25] = {/* Playstation */
        "Metal Gear Solid",  "Castlevania: SotN", "Resident Evil 2",
        "Xenogears",         "Suikoden II",       "Tekken 3",
        "Final Fantasy VII", "Resident Evil",     "Final Fantasy Tactics",
        "Final Fantas IX"};

    char class_6a[20][25] = {/* Playstation copy */
        "Metal Gear Solid",  "Castlevania: SotN", "Resident Evil 2",
        "Xenogears",         "Suikoden II",       "Tekken 3",
        "Final Fantasy VII", "Resident Evil",     "Final Fantasy Tactics",
        "Final Fantas IX",   "Valkyrie Profile",  "Mega Man X5",
        "Wipeout 3",         "Einhander",         "Tenchu",
        "Lunar 2",           "Dino Crisis 2",     "Oddworld",
        "Arc the Lad",       "Lunar"};

    char class_7[10][25] = {/* N64 */
        "Zelda: Ocarina of Time", "Zelda: Majora's Mask", "Super Mario 64",
        "GoldenEye 007",          "Banjo-Kazooie",        "Star Fox 64",
        "Paper Mario",            "Perfect Dark",         "Super Smash Bros.",
        "F-Zero X"};

    char class_8[20][25] = {/* GBC */
        "Zelda: Link's Awakening", "Tetris",                    "Donkey Kong",
        "Wario Land",               "Pokemon Red & Blue",       "Pokemon Yellow",
        "Super Mario Land 2",       "Wario Land II",            "Kirby's Dream Land 2",
        "Castlevania II",           "Link's Awakening DX",      "Zelda: Oracle of Ages",
        "Pokemon Gold & Silver",    "Pokemon Crystal",          "Zelda: Oracle of Seasons",
        "Wario Land 3",             "Super Mario Bros. Deluxe", "Dragon Quest I & II",
        "Pokemon Pinball",          "Shantae"}; 

    char class_9[5][25] = {"Soulcalibur", "Sonic Adventure", "Shenmue", "Phantasy Star Online",
                           "Sonic Adventure 2"}; /* Dreamcast */

    char class_10[20][25] = {"Metal Gear Solid",
                             "Castlevania: SotN",
                             "Resident Evil 2",
                             "Xenogears",
                             "Suikoden II",
                             "Tekken 3",
                             "Final Fantasy VII",
                             "Resident Evil",
                             "Final Fantasy Tactics",
                             "Final Fantas IX",
                             "GTA: San Andreas",
                             "MGS 3: Snake Eater",
                             "Shadow of the Colossus",
                             "MGS 2: Sons of Liberty",
                             "Silent Hill 2",
                             "Final Fantasy X",
                             "Gran Turismo 3",
                             "Okami",
                             "God of War 2",
                             "Bully"}; /* PS2 */

    char class_10a[30][25] = {"Metal Gear Solid",
                              "Castlevania: SotN",
                              "Resident Evil 2",
                              "Xenogears",
                              "Suikoden II",
                              "Tekken 3",
                              "Final Fantasy VII",
                              "Resident Evil",
                              "Final Fantasy Tactics",
                              "Final Fantas IX",
                              "GTA: San Andreas",
                              "MGS 3: Snake Eater",
                              "Shadow of the Colossus",
                              "MGS 2: Sons of Liberty",
                              "Silent Hill 2",
                              "Final Fantasy X",
                              "Gran Turismo 3",
                              "Okami",
                              "God of War 2",
                              "Bully",
                              "Kingdom Hearts",
                              "GTA: Vice City",
                              "GTA III",
                              "God of War",
                              "Sly 3",
                              "Jak 3",
                              "Soulcalibur II",
                              "Beyond Good and Evil",
                              "Jak and Daxter",
                              "Gran Turismo 4"}; /* PS2 copy */

    char class_11[10][25] = {"Paper Mario: TTYD",       "Metroid Prime",
                             "Zelda: The Wind Waker",   "Resident Evil 4",
                             "Zelda: Collector's Ed",   "Super Smash Bros. Melee",
                             "Zelda: Master Quest",     "Zelda: Twilight Princess",
                             "Mario Kart: Double Dash", "Pikmin 2"}; /* GameCube */

    char class_12[30][25] = {"Zelda: Link's Awakening",
                             "Tetris",
                             "Donkey Kong",
                             "Wario Land",
                             "Pokemon Red & Blue",
                             "Pokemon Yellow",
                             "Super Mario Land 2",
                             "Wario Land II",
                             "Kirby's Dream Land 2",
                             "Castlevania II",
                             "Link's Awakening DX",
                             "Zelda: Oracle of Ages",
                             "Pokemon Gold & Silver",
                             "Pokemon Crystal",
                             "Zelda: Oracle of Seasons",
                             "Wario Land 3",
                             "Super Mario Bros. Deluxe",
                             "Dragon Quest I & II",
                             "Pokemon Pinball",
                             "Shantae",
                             "Metroid: Zero Mission",
                             "Final Fantasy VI",
                             "Metroid Fusion",
                             "Zelda: The Minish Cap",
                             "Pokemon Emerald",
                             "Zelda: Four Swords",
                             "Super Mario Advance 4",
                             "Super Mario Advance 2",
                             "Mario & Luigi",
                             "Golden Sun: The Lost Age"}; /* GBA */

    char class_13[10][25] = {"Halo: Combat Evolved",
                             "Halo 2",
                             "Splinter Cell: Chaos Thry",
                             "Star Wars: KotOR",
                             "Ninja Gaiden",
                             "Riddick: EfBB",
                             "Fable",
                             "Rainbow Six 3",
                             "Jet Set Radio Future",
                             "Crimson Skies"}; /* Xbox */

    char class_14[20][25] = {"Halo: Combat Evolved",
                             "Halo 2",
                             "Splinter Cell: Chaos Thry",
                             "Star Wars: KotOR",
                             "Ninja Gaiden",
                             "Riddick: EfBB",
                             "Fable",
                             "Rainbow Six 3",
                             "Jet Set Radio Future",
                             "Crimson Skies",
                             "Dark Souls",
                             "Mass Effect 2",
                             "BioShock",
                             "Portal",
                             "TES 5: Skyrim",
                             "Red Dead Redemption",
                             "Halo Reach",
                             "CoD 4: Modern Warfar",
                             "Batman: Arkham City",
                             "Rise of the Tomb Raider"}; /* 360 */

    char class_15[10][25] = {"The Last of Us",   "GTA 5",       "Portal 2",
                             "Uncharted 2",      "Persona 5",   "Super Street Fighter 4",
                             "The Walking Dead", "Wolfenstein", "XCOM: Enemy Unknown",
                             "Journey"}; /* PS3 */

    char class_16[10][25] = {"Metroid Prime Trilogy",   "Super Mario Galaxy", "Super Mario Galaxy 2",
                             "Xenoblade Chronicles",    "DK Country Returns", "Wii Sports Resort",
                             "Super Smash Bros. Brawl", "Metroid Prime 3",    "Mario Kart Wii",
                             "Zelda: Skyward Sword"}; /* Wii */

    char class_17[20][25] = {"Metroid Prime Trilogy",
                             "Super Mario Galaxy",
                             "Super Mario Galaxy 2",
                             "Xenoblade Chronicles",
                             "DK Country Returns",
                             "Wii Sports Resort",
                             "Super Smash Bros. Brawl",
                             "Metroid Prime 3",
                             "Mario Kart Wii",
                             "Zelda: Skyward Sword",
                             "Mario Kart 8",
                             "DKC Tropical Freeze",
                             "Splatoon",
                             "Bayonetta 2",
                             "Smash Bros. for Wii U",
                             "Pikmin 3",
                             "Super Mario 3D World",
                             "Xenoblade Chronicles X",
                             "Rayman Legends",
                             "Super Mario Maker"}; /* Wii U */

    char class_18[10][25] = {"The Witcher 3",
                             "Red Dead Redemption 2",
                             "MGS 5: The Phantom Pain",
                             "Minecraft",
                             "Control",
                             "GTA 5 Online",
                             "Fortnite",
                             "Overwatch",
                             "Destiny 2",
                             "Apex Legends"}; /* Games on both */

    char class_18a[20][25] = {"The Witcher 3",
                              "Red Dead Redemption 2",
                              "MGS 5: The Phantom Pain",
                              "Minecraft",
                              "Control",
                              "GTA 5 Online",
                              "Fortnite",
                              "Overwatch",
                              "Destiny 2",
                              "Apex Legends",
                              "God of War (2018)",
                              "The Last of Us Part 2",
                              "Bloodborne",
                              "Persona 5 Royal",
                              "Horizon Zero Dawn",
                              "Marvel's Spider-Man",
                              "Uncharted: Lost Legacy",
                              "Ghost of Tsushima",
                              "Uncharted 4",
                              "Final Fantasy 7 Remake"}; /* PS4 */

    char class_18b[20][25] = {"The Witcher 3",
                              "Red Dead Redemption 2",
                              "MGS 5: The Phantom Pain",
                              "Minecraft",
                              "Control",
                              "GTA 5 Online",
                              "Fortnite",
                              "Overwatch",
                              "Destiny 2",
                              "Apex Legends",
                              "Gears 5",
                              "Ori",
                              "Forza Horizon 4",
                              "Titanfall 2",
                              "Fantasia: Music Evolved",
                              "Halo Wars 2",
                              "Gears Tactics",
                              "Titanfall",
                              "Halo 5",
                              "Halo: Master Chief Co."}; /* Xbox One */

    char class_19[10][25] = {"Breath of the Wild",     "Metroid Dread",        "Super Mario Odyssey",
                             "Smash Bros. Ultimate",   "Tears of the Kingdom", "Mario Kart 8 Deluxe",
                             "Xenoblade Chronicles 3", "Dragon Quest XI",      "Super Mario Bros. Wonder",
                             "Fire Emblem: 3 Houses"}; /* Switch */

    /* List prop names */

    char list_1[20][25] = {"Super Mario Bros. 3",
                           "Mega Man 2",
                           "The Legend of Zelda",
                           "Mega Man 3",
                           "Punch-Out!!",
                           "Contra",
                           "Super Mario Bros.",
                           "Kirby's Adventure",
                           "Castlevania III",
                           "Dragon Quest IV",
                           "DuckTales",
                           "Batman: The Video Game",
                           "Dragon Quest III",
                           "TMNT III",
                           "Castlevania",
                           "Tetris",
                           "Super C",
                           "Mega Man 4",
                           "Ninja Gaiden II",
                           "TMNT II"}; /* NES */

    char list_2[20][25] = {"Sonic 3",
                           "Streets of Rage II",
                           "Gunstar Heroes",
                           "Strider",
                           "Shinobi III",
                           "Castlevania Bloodlines",
                           "Ristar",
                           "Rocket Knight Adventures",
                           "Aladdin",
                           "Mortal Kombat",
                           "Street Fighter II",
                           "Sonic 2",
                           "TMNT: Hyperstone Heist",
                           "Power Rangers",
                           "Vectorman",
                           "Phantasy Star II",
                           "Golden Axe",
                           "Earthworm Jim",
                           "Comix Zone",
                           "Kid Chameleon"}; /* Genesis */

    char list_3[20][25] = {"Zelda: Link's Awakening",
                           "Tetris",
                           "Donkey Kong",
                           "Wario Land",
                           "Pokemon Red & Blue",
                           "Pokemon Yellow",
                           "Super Mario Land 2",
                           "Wario Land II",
                           "Kirby's Dream Land 2",
                           "Castlevania II",
                           "Mario's Picross",
                           "Mole Mania",
                           "Final Fantasy Adventure",
                           "Mega Man V",
                           "Kid Dracula",
                           "Gargoyle's Quest",
                           "FF Legend II",
                           "Kirby's Dream Land",
                           "Donkey Kong Land 2",
                           "Donkey Kong Land III"}; /* Gameboy */

    char list_3z[20][25] = {"Zelda: Link's Awakening",
                            "Tetris",
                            "Donkey Kong",
                            "Wario Land",
                            "Pokemon Red & Blue",
                            "Pokemon Yellow",
                            "Super Mario Land 2",
                            "Wario Land II",
                            "Kirby's Dream Land 2",
                            "Castlevania II",
                            "Mario's Picross",
                            "Mole Mania",
                            "Final Fantasy Adventure",
                            "Mega Man V",
                            "Kid Dracula",
                            "Gargoyle's Quest",
                            "FF Legend II",
                            "Kirby's Dream Land",
                            "Donkey Kong Land 2",
                            "Donkey Kong Land III"}; /* Gameboy list copy */

    char list_4[20][25] = {"Super Mario World",
                           "A link to the Past",
                           "Super Metroid",
                           "Chrono Trigger",
                           "Final Fantasy III",
                           "Super Mario World 2",
                           "Super Mario All-Stars",
                           "Super Mario RPG",
                           "DK Country 2",
                           "EarthBound",
                           "Mega Man X",
                           "TMNT IV",
                           "Donkey Kong Country",
                           "Super Castlevania IV",
                           "Contra III",
                           "Secret of Mana",
                           "Kirby Super Star",
                           "Super Street Fighter II",
                           "Mega Man X2",
                           "Final Fantasy II"}; /* SNES */

    char list_4a[25][25] = {"Super Mario World",
                            "A link to the Past",
                            "Super Metroid",
                            "Chrono Trigger",
                            "Final Fantasy III",
                            "Super Mario World 2",
                            "Super Mario All-Stars",
                            "Super Mario RPG",
                            "DK Country 2",
                            "EarthBound",
                            "Terranigma",
                            "DK Country 3",
                            "Tetris Attack",
                            "Illusion of Gaia",
                            "F-Zero",
                            "Mega Man X",
                            "TMNT IV",
                            "Donkey Kong Country",
                            "Super Castlevania IV",
                            "Contra III",
                            "Secret of Mana",
                            "Kirby Super Star",
                            "Super Street Fighter II",
                            "Mega Man X2",
                            "Final Fantasy II"}; /* SNES class copy */

    char list_5[2][25] = {"Sega Rally Championship", "Panzer Dragoon Saga"}; /* Saturn */

    char list_5z[2][25] = {"Sega Rally Championship", "Panzer Dragoon Saga"}; /* Saturn list copy */

    char list_6[20][25] = {"Metal Gear Solid",
                           "Castlevania: SotN",
                           "Resident Evil 2",
                           "Xenogears",
                           "Suikoden II",
                           "Tekken 3",
                           "Final Fantasy VII",
                           "Resident Evil",
                           "Final Fantasy Tactics",
                           "Final Fantas IX",
                           "Gran Turismo 2",
                           "Vagrant Story",
                           "Legacy of Kain 2",
                           "Crash Bandicoot 3",
                           "Crash Team Racing",
                           "Parasite Eve",
                           "Spyro",
                           "Final Fantasy Chronicles",
                           "Silent Hill",
                           "The Legend of Dragoon"}; /* Playstation */

    char list_6a[30][25] = {"Metal Gear Solid",
                            "Castlevania: SotN",
                            "Resident Evil 2",
                            "Xenogears",
                            "Suikoden II",
                            "Tekken 3",
                            "Final Fantasy VII",
                            "Resident Evil",
                            "Final Fantasy Tactics",
                            "Final Fantas IX",
                            "Gran Turismo 2",
                            "Vagrant Story",
                            "Legacy of Kain 2",
                            "Crash Bandicoot 3",
                            "Crash Team Racing",
                            "Parasite Eve",
                            "Spyro",
                            "Final Fantasy Chronicles",
                            "Silent Hill",
                            "The Legend of Dragoon",
                            "Valkyrie Profile",
                            "Mega Man X5",
                            "Wipeout 3",
                            "Einhander",
                            "Tenchu",
                            "Lunar 2",
                            "Dino Crisis 2",
                            "Oddworld",
                            "Arc the Lad",
                            "Lunar"}; /* Playstation class copy */

    char list_7[20][25] = {"Zelda: Ocarina of Time",
                           "Zelda: Majora's Mask",
                           "Super Mario 64",
                           "GoldenEye 007",
                           "Banjo-Kazooie",
                           "Star Fox 64",
                           "Paper Mario",
                           "Perfect Dark",
                           "Super Smash Bros.",
                           "F-Zero X",
                           "Diddy Kong Racing",
                           "Mario Party 2",
                           "Banjo-Tooie",
                           "Conker's Bad Fur Day",
                           "Rogue Squadron",
                           "Mario Party 3",
                           "Mario Kart 64",
                           "Mystical Ninja",
                           "Wave Race 64",
                           "Mario Tennis"}; /* N64 */

    char list_8[30][25] = {"Zelda: Link's Awakening",
                           "Tetris",
                           "Donkey Kong",
                           "Wario Land",
                           "Pokemon Red & Blue",
                           "Pokemon Yellow",
                           "Super Mario Land 2",
                           "Wario Land II",
                           "Kirby's Dream Land 2",
                           "Castlevania II",
                           "Link's Awakening DX",
                           "Zelda: Oracle of Ages",
                           "Pokemon Gold & Silver",
                           "Pokemon Crystal",
                           "Zelda: Oracle of Seasons",
                           "Wario Land 3",
                           "Super Mario Bros. Deluxe",
                           "Dragon Quest I & II",
                           "Pokemon Pinball",
                           "Shantae",
                           "Dragon Quest Monsters",
                           "Dragon Quest Monsters 2",
                           "Mario Golf",
                           "Pokemon Puzzle",
                           "Tetris DX",
                           "Game & Watch Gallery 2",
                           "Hamtaro",
                           "R-Type DX",
                           "Game & Watch Gallery 3",
                           "Mickey's Racing"}; /* GBC */

    char list_9[10][25] = {"Soulcalibur",
                           "Sonic Adventure",
                           "Shenmue",
                           "Phantasy Star Online",
                           "Sonic Adventure 2",
                           "Spider-man",
                           "Daytona USA",
                           "Marvel vs. Capcom 2",
                           "Rez",
                           "Crazy Taxi"}; /* Dreamcast */

    char list_9z[10][25] = {"Soulcalibur",
                            "Sonic Adventure",
                            "Shenmue",
                            "Phantasy Star Online",
                            "Sonic Adventure 2",
                            "Spider-man",
                            "Daytona USA",
                            "Marvel vs. Capcom 2",
                            "Rez",
                            "Crazy Taxi"}; /* Dreamcast list copy */

    char list_10[30][25] = {"Metal Gear Solid",
                            "Castlevania: SotN",
                            "Resident Evil 2",
                            "Xenogears",
                            "Suikoden II",
                            "Tekken 3",
                            "Final Fantasy VII",
                            "Resident Evil",
                            "Final Fantasy Tactics",
                            "Final Fantas IX",
                            "GTA: San Andreas",
                            "MGS 3: Snake Eater",
                            "Shadow of the Colossus",
                            "MGS 2: Sons of Liberty",
                            "Silent Hill 2",
                            "Final Fantasy X",
                            "Gran Turismo 3",
                            "Okami",
                            "God of War 2",
                            "Bully",
                            "Katamari Damacy",
                            "Devil May Cry 3",
                            "PoP: Sands of Time",
                            "Burnout 3: Takedown",
                            "Tony Hawk's Pro Skater 3",
                            "Ico",
                            "Jak 2: Renegade",
                            "Rachet & Clank UYA",
                            "TimeSplitters 2",
                            "Kingdom Hearts 2"}; /* PS2 */

    char list_10z[40][25] = {"Metal Gear Solid",
                             "Castlevania: SotN",
                             "Resident Evil 2",
                             "Xenogears",
                             "Suikoden II",
                             "Tekken 3",
                             "Final Fantasy VII",
                             "Resident Evil",
                             "Final Fantasy Tactics",
                             "Final Fantas IX",
                             "GTA: San Andreas",
                             "MGS 3: Snake Eater",
                             "Shadow of the Colossus",
                             "MGS 2: Sons of Liberty",
                             "Silent Hill 2",
                             "Final Fantasy X",
                             "Gran Turismo 3",
                             "Okami",
                             "God of War 2",
                             "Bully",
                             "Katamari Damacy",
                             "Devil May Cry 3",
                             "PoP: Sands of Time",
                             "Burnout 3: Takedown",
                             "Tony Hawk's Pro Skater 3",
                             "Ico",
                             "Jak 2: Renegade",
                             "Rachet & Clank UYA",
                             "TimeSplitters 2",
                             "Kingdom Hearts 2",
                             "Disgaea",
                             "Devil May Cry",
                             "Virtua Fighter 4",
                             "NBA Street Vol. 2",
                             "Viewtiful Joe",
                             "Manhunt",
                             "Odin Sphere",
                             "Twisted Metal: Black",
                             "Suikoden III",
                             "Breath of Fire"}; /* PS2 list copy */

    char list_10a[40][25] = {"Metal Gear Solid",
                             "Castlevania: SotN",
                             "Resident Evil 2",
                             "Xenogears",
                             "Suikoden II",
                             "Tekken 3",
                             "Final Fantasy VII",
                             "Resident Evil",
                             "Final Fantasy Tactics",
                             "Final Fantas IX",
                             "GTA: San Andreas",
                             "MGS 3: Snake Eater",
                             "Shadow of the Colossus",
                             "MGS 2: Sons of Liberty",
                             "Silent Hill 2",
                             "Final Fantasy X",
                             "Gran Turismo 3",
                             "Okami",
                             "God of War 2",
                             "Bully",
                             "Kingdom Hearts",
                             "GTA: Vice City",
                             "GTA III",
                             "God of War",
                             "Sly 3",
                             "Jak 3",
                             "Soulcalibur II",
                             "Beyond Good and Evil",
                             "Jak and Daxter",
                             "Gran Turismo 4",
                             "Katamari Damacy",
                             "Devil May Cry 3",
                             "PoP: Sands of Time",
                             "Burnout 3: Takedown",
                             "Tony Hawk's Pro Skater 3",
                             "Ico",
                             "Jak 2: Renegade",
                             "Rachet & Clank UYA",
                             "TimeSplitters 2",
                             "Kingdom Hearts 2"}; /* PS2 class copy */

    char list_11[20][25] = {"Paper Mario: TTYD",       "Metroid Prime",
                            "Zelda: The Wind Waker",   "Resident Evil 4",
                            "Zelda: Collector's Ed",   "Super Smash Bros. Melee",
                            "Zelda: Master Quest",     "Zelda: Twilight Princess",
                            "Mario Kart: Double Dash", "Pikmin 2",
                            "Metroid Prime 2",         "Eternal Darkness",
                            "Animal Crossing",         "F-Zero GX",
                            "Luigi's Mansion",         "Rogue Squadron II",
                            "Fire Emblem: PoR",        "Tales of Symphonia",
                            "Skies of Arcadia",        "Pikmin"}; /* GameCube */

    char list_12[40][25] = {"Zelda: Link's Awakening",
                            "Tetris",
                            "Donkey Kong",
                            "Wario Land",
                            "Pokemon Red & Blue",
                            "Pokemon Yellow",
                            "Super Mario Land 2",
                            "Wario Land II",
                            "Kirby's Dream Land 2",
                            "Castlevania II",
                            "Link's Awakening DX",
                            "Zelda: Oracle of Ages",
                            "Pokemon Gold & Silver",
                            "Pokemon Crystal",
                            "Zelda: Oracle of Seasons",
                            "Wario Land 3",
                            "Super Mario Bros. Deluxe",
                            "Dragon Quest I & II",
                            "Pokemon Pinball",
                            "Shantae",
                            "Metroid: Zero Mission",
                            "Final Fantasy VI",
                            "Metroid Fusion",
                            "Zelda: The Minish Cap",
                            "Pokemon Emerald",
                            "Zelda: Four Swords",
                            "Super Mario Advance 4",
                            "Super Mario Advance 2",
                            "Mario & Luigi",
                            "Golden Sun: The Lost Age",
                            "Castlevania: AoS",
                            "Fire Emblem",
                            "Golden Sun",
                            "WarioWare, Inc.",
                            "Advance Wars",
                            "FireRed & LeafGreen",
                            "Fire Emblem: TSS",
                            "Super Mario Advance 3",
                            "Advance Wars 2",
                            "Wario Land 4"}; /* GBA */

    char list_13[20][25] = {"Halo: Combat Evolved",
                            "Halo 2",
                            "Splinter Cell: Chaos Thry",
                            "Star Wars: KotOR",
                            "Ninja Gaiden",
                            "Riddick: EfBB",
                            "Fable",
                            "Rainbow Six 3",
                            "Jet Set Radio Future",
                            "Crimson Skies",
                            "Jade Empire",
                            "Project Gotham Racing 2",
                            "MechAssault",
                            "TES 3: Morrowind",
                            "Full Spectrum Warrior",
                            "Top Spin",
                            "Breakdown",
                            "Steel Battalion",
                            "Otogi: Myth of Demons",
                            "Psychonauts"}; /* Xbox */

    char list_14[30][25] = {"Halo: Combat Evolved",
                            "Halo 2",
                            "Splinter Cell: Chaos Thry",
                            "Star Wars: KotOR",
                            "Ninja Gaiden",
                            "Riddick: EfBB",
                            "Fable",
                            "Rainbow Six 3",
                            "Jet Set Radio Future",
                            "Crimson Skies",
                            "Dark Souls",
                            "Mass Effect 2",
                            "BioShock",
                            "Portal",
                            "TES 5: Skyrim",
                            "Red Dead Redemption",
                            "Halo Reach",
                            "CoD 4: Modern Warfar",
                            "Batman: Arkham City",
                            "Rise of the Tomb Raider",
                            "BioShock Infinite",
                            "Dead Space 2",
                            "Braid",
                            "Super Meat Boy",
                            "AC: Black Flag",
                            "Gears of War 2",
                            "Left 4 Dead 2",
                            "Borderlands 2",
                            "Diablo 3",
                            "Dishonored"}; /* 360 */

    char list_15[20][25] = {"The Last of Us",
                            "GTA 5",
                            "Portal 2",
                            "Uncharted 2",
                            "Persona 5",
                            "Super Street Fighter 4",
                            "The Walking Dead",
                            "Wolfenstein",
                            "XCOM: Enemy Unknown",
                            "Journey",
                            "Metal Gear Solid 4",
                            "Yakuza 5",
                            "Resistance 3",
                            "Burnout Paradise",
                            "Deus Ex",
                            "Hotline Miami",
                            "LittleBigPlanet 2",
                            "Heavy Rain",
                            "Uncharted 3",
                            "God of War III"}; /* PS3 */

    char list_16[20][25] = {"Metroid Prime Trilogy",    "Super Mario Galaxy",
                            "Super Mario Galaxy 2",     "Xenoblade Chronicles",
                            "DK Country Returns",       "Wii Sports Resort",
                            "Super Smash Bros. Brawl",  "Metroid Prime 3",
                            "Mario Kart Wii",           "Zelda: Skyward Sword",
                            "Kirby's Return DreamLand", "Wii Sports",
                            "Fire Emblem Radiant Dawn", "Kirby's Epic Yarn",
                            "Kirby's Dream Collection", "The Last Story",
                            "WarioWare Smooth Moves",   "Rhythm Heaven Fever",
                            "Sin & Punishment",         "Rayman Origins"}; /* Wii */

    char list_17[30][25] = {"Metroid Prime Trilogy",
                            "Super Mario Galaxy",
                            "Super Mario Galaxy 2",
                            "Xenoblade Chronicles",
                            "DK Country Returns",
                            "Wii Sports Resort",
                            "Super Smash Bros. Brawl",
                            "Metroid Prime 3",
                            "Mario Kart Wii",
                            "Zelda: Skyward Sword",
                            "Mario Kart 8",
                            "DKC Tropical Freeze",
                            "Splatoon",
                            "Bayonetta 2",
                            "Smash Bros. for Wii U",
                            "Pikmin 3",
                            "Super Mario 3D World",
                            "Xenoblade Chronicles X",
                            "Rayman Legends",
                            "Super Mario Maker",
                            "Yoshi's Woolly World",
                            "Shovel Knight",
                            "Bayonetta",
                            "Captain Toad",
                            "Lego City",
                            "Tokyo Mirage Sessions",
                            "Nintendo Land",
                            "Hyrule Warriors",
                            "Sonic & All-Stars Racing",
                            "Monster Hunter 3"}; /* Wii U */

    char list_18[18][25] = {"The Witcher 3",
                            "Red Dead Redemption 2",
                            "MGS 5: The Phantom Pain",
                            "Minecraft",
                            "Control",
                            "GTA 5 Online",
                            "Fortnite",
                            "Overwatch",
                            "Destiny 2",
                            "Apex Legends",
                            "Fallout 4",
                            "Monster Hunter World",
                            "Resident Evil 2 Remake",
                            "AC Odyssey",
                            "Devil May Cry 5",
                            "Wolfenstein 2",
                            "Sekiro",
                            "Rainbow Six Siege"}; /* On both */

    char list_18a[28][25] = {"The Witcher 3",
                             "Red Dead Redemption 2",
                             "MGS 5: The Phantom Pain",
                             "Minecraft",
                             "Control",
                             "GTA 5 Online",
                             "Fortnite",
                             "Overwatch",
                             "Destiny 2",
                             "Apex Legends",
                             "God of War (2018)",
                             "The Last of Us Part 2",
                             "Bloodborne",
                             "Persona 5 Royal",
                             "Horizon Zero Dawn",
                             "Marvel's Spider-Man",
                             "Uncharted: Lost Legacy",
                             "Ghost of Tsushima",
                             "Uncharted 4",
                             "Final Fantasy 7 Remake",
                             "The Last Guardian",
                             "Ratchet and Clank",
                             "Detroit: Become Human",
                             "inFamous: Second Son",
                             "Yakuza 0",
                             "Street Fighter 5",
                             "Yakuza 6",
                             "Concrete Genie"}; /* PS4 */

    char list_18b[21][25] = {"The Witcher 3",
                             "Red Dead Redemption 2",
                             "MGS 5: The Phantom Pain",
                             "Minecraft",
                             "Control",
                             "GTA 5 Online",
                             "Fortnite",
                             "Overwatch",
                             "Destiny 2",
                             "Apex Legends",
                             "Gears 5",
                             "Ori",
                             "Forza Horizon 4",
                             "Titanfall 2",
                             "Fantasia: Music Evolved",
                             "Halo Wars 2",
                             "Gears Tactics",
                             "Titanfall",
                             "Halo 5",
                             "Halo: Master Chief Co.",
                             "Forza Motorsport 7"}; /* Xbox One */

    char list_19[20][25] = {"Breath of the Wild",
                            "Metroid Dread",
                            "Super Mario Odyssey",
                            "Smash Bros. Ultimate",
                            "Tears of the Kingdom",
                            "Mario Kart 8 Deluxe",
                            "Xenoblade Chronicles 3",
                            "Dragon Quest XI",
                            "Super Mario Bros. Wonder",
                            "Fire Emblem: 3 Houses",
                            "Hades",
                            "Shin Megami Tensei V",
                            "Unicorn Overlord",
                            "Monster Hunter Rise",
                            "Pikmin 4",
                            "Hollow Knight",
                            "Animal Crossing Switch",
                            "Balatro",
                            "Luigi's Mansion 3",
                            "Ori"}; /* Switch */


    char(*class_prop_names[24])[25] = {class_1,   class_2,  class_3,  class_4,   class_4a,  class_5,
                                       class_6,   class_6a, class_7,  class_8,   class_9,   class_10,
                                       class_10a, class_11, class_12, class_13,  class_14,  class_15,
                                       class_16,  class_17, class_18, class_18a, class_18b, class_19};

    /** This is to easily see the names of each class and if they're a copy.
     *  Helps with debugging
     * 
     *    NES      Genesis    Gameboy     SNES    SNES(a)
     *  Saturn      PS1       PS1(a)       N64      GBC
     *  Dreamcast   PS2       PS2(a)     GameCube   GBA
     *    Xbox      360        PS3         Wii      WiiU
     *  PS4/One  PS4/One(a)  PS4/One(b)   Switch
     */

    uint32_t num_names[24] = {10, 10, 10, 10, 15, 1,  10, 20, 10, 20, 5,  20,
                              30, 10, 30, 10, 10, 10, 10, 20, 10, 20, 20, 10};

    char(*list_prop_names[28])[25] = {list_1,  list_2,   list_3,   list_3z, list_4,   list_4a,  list_5,
                                      list_5z, list_6,   list_6a,  list_7,  list_8,   list_9,   list_9z,
                                      list_10, list_10z, list_10a, list_11, list_12,  list_13,  list_14,
                                      list_15, list_16,  list_17,  list_18, list_18a, list_18b, list_19};

    /** This is to easily see the names of the classes 
     *  the lists are derived from and if they're a copy.
     *  Helps with debugging.
     * 
     *    NES      Genesis  Gameboy  Gameboy(z)  SNES
     *  SNES(a)    Saturn   Saturn(z)   PS1      PS1(a)
     *    N64       GBC    Dreamcast Dreamcast(z) PS2
     *  PS2(z)     PS2(a)   GameCube    GBA      Xbox
     *   360        PS3       Wii       WiiU    PS4/One
     * PS4/One(a) PS4/One(b) Switch
     */

    uint32_t num_names2[28] = {20, 20, 20, 20, 20, 25, 2,  2,  20, 30, 20, 30, 10, 10,
                               30, 40, 40, 20, 40, 20, 30, 20, 20, 30, 18, 28, 21, 20};

    uint32_t j = 0;

    /* Init and alloc Class Table defaults */
    for (int i = 0; i < CLASS_TABLE_SIZE; i++) {
        class_table_entry = &class_table[i];

        atomic_init(&(class_table_entry->class_sptr), class_sptr);

        class_table_entry->name = strdup(class_names[i]);

        atomic_init(&(class_table_entry->id), H5I_INVALID_HID);
        atomic_init(&(class_table_entry->status), DOESNT_EXIST);
        atomic_init(&(class_table_entry->op_count), 0);
        atomic_init(&(class_table_entry->ver_closed), 0);
        atomic_init(&(class_table_entry->ver_deleted), 0);

        class_id++;
        class_table_entry->test_class_id = class_id;
        atomic_init(&(class_table_entry->parent_id), H5I_INVALID_HID);

        class_table_entry->copy       = FALSE;
        class_table_entry->og_id      = 0;
        class_table_entry->ver_copied = 0;

        switch (i + 1) {
            case 1:
            case 2:
            case 3:
            case 4:
                class_table_entry->parent_name = strdup("test_root");
                atomic_init(&(class_table_entry->parent_id), TEST_ROOT_ID_g);
                atomic_init(&(class_table_entry->parent_entry), NULL);
                break;
            case 5:
                class_table_entry->parent_name = strdup("test_root");
                atomic_init(&(class_table_entry->parent_id), TEST_ROOT_ID_g);
                atomic_init(&(class_table_entry->parent_entry), NULL);
                class_table_entry->copy = TRUE;
                class_table_entry->og_id = 4;
                break;
            case 6:
            case 7:
                class_table_entry->parent_name = strdup("test_root");
                atomic_init(&(class_table_entry->parent_id), TEST_ROOT_ID_g);
                atomic_init(&(class_table_entry->parent_entry), NULL);
                break;
            case 8:
                class_table_entry->parent_name = strdup("test_root");
                atomic_init(&(class_table_entry->parent_id), TEST_ROOT_ID_g);
                atomic_init(&(class_table_entry->parent_entry), NULL);
                class_table_entry->copy = TRUE;
                class_table_entry->og_id = 7;
                break;
            case 9:
                class_table_entry->parent_name = strdup("test_root");
                atomic_init(&(class_table_entry->parent_id), TEST_ROOT_ID_g);
                atomic_init(&(class_table_entry->parent_entry), NULL);
                break;
            case 10:
                class_table_entry->parent_name = strdup("Gameboy");
                atomic_init(&(class_table_entry->parent_entry), &class_table[2]);
                break;
            case 11:
                class_table_entry->parent_name = strdup("test_root");
                atomic_init(&(class_table_entry->parent_id), TEST_ROOT_ID_g);
                atomic_init(&(class_table_entry->parent_entry), NULL);
                break;
            case 12:
                class_table_entry->parent_name = strdup("Playstation");
                atomic_init(&(class_table_entry->parent_entry), &class_table[6]);
                break;
            case 13:
                class_table_entry->parent_name = strdup("Playstation");
                atomic_init(&(class_table_entry->parent_entry), &class_table[6]);
                class_table_entry->copy = TRUE;
                class_table_entry->og_id = 12;
                break;
            case 14:
                class_table_entry->parent_name = strdup("test_root");
                atomic_init(&(class_table_entry->parent_id), TEST_ROOT_ID_g);
                atomic_init(&(class_table_entry->parent_entry), NULL);
                break;
            case 15:
                class_table_entry->parent_name = strdup("Gameboy Color");
                atomic_init(&(class_table_entry->parent_entry), &class_table[9]);
                break;
            case 16:
                class_table_entry->parent_name = strdup("test_root");
                atomic_init(&(class_table_entry->parent_id), TEST_ROOT_ID_g);
                atomic_init(&(class_table_entry->parent_entry), NULL);
                break;
            case 17:
                class_table_entry->parent_name = strdup("Xbox");
                atomic_init(&(class_table_entry->parent_entry), &class_table[15]);
                break;
            case 18:
            case 19:
                class_table_entry->parent_name = strdup("test_root");
                atomic_init(&(class_table_entry->parent_id), TEST_ROOT_ID_g);
                atomic_init(&(class_table_entry->parent_entry), NULL);
                break;
            case 20:
                class_table_entry->parent_name = strdup("Wii");
                atomic_init(&(class_table_entry->parent_entry), &class_table[18]);
                break;
            case 21:
                class_table_entry->parent_name = strdup("test_root");
                atomic_init(&(class_table_entry->parent_id), TEST_ROOT_ID_g);
                atomic_init(&(class_table_entry->parent_entry), NULL);
                break;
            case 22:
            case 23:
                class_table_entry->parent_name = strdup("test_root");
                atomic_init(&(class_table_entry->parent_id), TEST_ROOT_ID_g);
                atomic_init(&(class_table_entry->parent_entry), NULL);
                class_table_entry->copy = TRUE;
                class_table_entry->og_id = 21;
                break;
            case 24:
                class_table_entry->parent_name = strdup("test_root");
                atomic_init(&(class_table_entry->parent_id), TEST_ROOT_ID_g);
                atomic_init(&(class_table_entry->parent_entry), NULL);
                break;
            default:
                assert(FALSE);

        } /* end switch() */

        j = num_names[i];
        class_table_entry->num_prop_entries = j;

        class_table_entry->prop_table = (prop_table_entry_t *)malloc(j * sizeof(prop_table_entry_t));

        for (uint32_t k = 0; k < j; k++) {
            prop_table_entry = &class_table_entry->prop_table[k];

            prop_table_entry->name = strdup(class_prop_names[i][k]);

            prop_table_entry->chksum =
                H5_checksum_metadata(prop_table_entry->name, strlen(prop_table_entry->name), 0);

            atomic_init(&(prop_table_entry->status), DOESNT_EXIST);
        }

    } /* end for() */

    /* Init List Table */
    for (int i = 0; i < LIST_TABLE_SIZE; i++) {

        list_table_entry = &list_table[i];

        atomic_init(&(list_table_entry->list_sptr), list_sptr);
        list_table_entry->parent_name = strdup(class_names[i]);
        atomic_init(&(list_table_entry->id), H5I_INVALID_HID);

        atomic_init(&(list_table_entry->status), DOESNT_EXIST);
        atomic_init(&(list_table_entry->op_count), 0);
        atomic_init(&(list_table_entry->ver_deleted), 0);

        list_id++;
        list_table_entry->test_list_id = list_id;
        atomic_init(&(list_table_entry->parent_id), H5I_INVALID_HID);

        list_table_entry->copy       = FALSE;
        list_table_entry->og_id      = 0;
        list_table_entry->ver_copied = 0;

        switch (i + 1) {
            case 1:
                list_table_entry->parent_name = strdup("NES");
                atomic_init(&(list_table_entry->parent_entry), &class_table[0]);
                break;
            case 2:
                list_table_entry->parent_name = strdup("Sega Genesis");
                atomic_init(&(list_table_entry->parent_entry), &class_table[1]);
                break;
            case 3:
                list_table_entry->parent_name = strdup("Gameboy");
                atomic_init(&(list_table_entry->parent_entry), &class_table[2]);
                break;
            case 4:
                list_table_entry->parent_name = strdup("Gameboy");
                atomic_init(&(list_table_entry->parent_entry), &class_table[2]);
                list_table_entry->copy = TRUE;
                list_table_entry->og_id = 103;
                break;
            case 5:
                list_table_entry->parent_name = strdup("SNES");
                atomic_init(&(list_table_entry->parent_entry), &class_table[3]);
                break;
            case 6:
                list_table_entry->parent_name = strdup("SNES");
                atomic_init(&(list_table_entry->parent_entry), &class_table[4]);
                break;
            case 7:
                list_table_entry->parent_name = strdup("Sega Saturn");
                atomic_init(&(list_table_entry->parent_entry), &class_table[5]);
                break;
            case 8:
                list_table_entry->parent_name = strdup("Sega Saturn");
                atomic_init(&(list_table_entry->parent_entry), &class_table[5]);
                list_table_entry->copy = TRUE;
                list_table_entry->og_id = 107;
                break;
            case 9:
                list_table_entry->parent_name = strdup("Playstation");
                atomic_init(&(list_table_entry->parent_entry), &class_table[6]);
                break;
            case 10:
                list_table_entry->parent_name = strdup("Playstation");
                atomic_init(&(list_table_entry->parent_entry), &class_table[7]);
                break;
            case 11:
                list_table_entry->parent_name = strdup("Nintendo 64");
                atomic_init(&(list_table_entry->parent_entry), &class_table[8]);
                break;
            case 12:
                list_table_entry->parent_name = strdup("Gameboy Color");
                atomic_init(&(list_table_entry->parent_entry), &class_table[9]);
                break;
            case 13:
                list_table_entry->parent_name = strdup("Sega Dreamcast");
                atomic_init(&(list_table_entry->parent_entry), &class_table[10]);
                break;
            case 14:
                list_table_entry->parent_name = strdup("Sega Dreamcast");
                atomic_init(&(list_table_entry->parent_entry), &class_table[10]);
                list_table_entry->copy = TRUE;
                list_table_entry->og_id = 113;
                break;
            case 15:
                list_table_entry->parent_name = strdup("PS2");
                atomic_init(&(list_table_entry->parent_entry), &class_table[11]);
                break;
            case 16:
                list_table_entry->parent_name = strdup("PS2");
                atomic_init(&(list_table_entry->parent_entry), &class_table[11]);
                list_table_entry->copy = TRUE;
                list_table_entry->og_id = 115;
                break;
            case 17:
                list_table_entry->parent_name = strdup("PS2");
                atomic_init(&(list_table_entry->parent_entry), &class_table[12]);
                break;
            case 18:
                list_table_entry->parent_name = strdup("GameCube");
                atomic_init(&(list_table_entry->parent_entry), &class_table[13]);
                break;
            case 19:
                list_table_entry->parent_name = strdup("Gameboy Advanced");
                atomic_init(&(list_table_entry->parent_entry), &class_table[14]);
                break;
            case 20:
                list_table_entry->parent_name = strdup("Xbox");
                atomic_init(&(list_table_entry->parent_entry), &class_table[15]);
                break;
            case 21:
                list_table_entry->parent_name = strdup("Xbox 360");
                atomic_init(&(list_table_entry->parent_entry), &class_table[16]);
                break;
            case 22:
                list_table_entry->parent_name = strdup("PS3");
                atomic_init(&(list_table_entry->parent_entry), &class_table[17]);
                break;
            case 23:
                list_table_entry->parent_name = strdup("Wii");
                atomic_init(&(list_table_entry->parent_entry), &class_table[18]);
                break;
            case 24:
                list_table_entry->parent_name = strdup("Wii U");
                atomic_init(&(list_table_entry->parent_entry), &class_table[19]);
                break;
            case 25:
                list_table_entry->parent_name = strdup("PS4/Xbox One");
                atomic_init(&(list_table_entry->parent_entry), &class_table[20]);
                break;
            case 26:
                list_table_entry->parent_name = strdup("PS4/Xbox One");
                atomic_init(&(list_table_entry->parent_entry), &class_table[21]);
                break;
            case 27:
                list_table_entry->parent_name = strdup("PS4/Xbox One");
                atomic_init(&(list_table_entry->parent_entry), &class_table[22]);
                break;
            case 28:
                list_table_entry->parent_name = strdup("Switch");
                atomic_init(&(list_table_entry->parent_entry), &class_table[23]);
                break;

        } /* end switch()*/

        j = num_names2[i];
        list_table_entry->num_prop_entries = j;

        list_table_entry->prop_table = (prop_table_entry_t *)malloc(j * sizeof(prop_table_entry_t));

        for (uint32_t k = 0; k < j; k++) {
            prop_table_entry = &list_table_entry->prop_table[k];

            prop_table_entry->name = strdup(list_prop_names[i][k]);

            prop_table_entry->chksum =
                H5_checksum_metadata(prop_table_entry->name, strlen(prop_table_entry->name), 0);

            atomic_init(&(prop_table_entry->status), DOESNT_EXIST);
        }

    } /* end for() */

    return (ret_value);

} /* end init_globals_2() */

/****************************************************************************************
 * Function:    reset_globals_2
 *
 * Purpose:     After an iteration of mt_test_2 has completed all operations for the 
 *              number of threads this functions resets the globals class_table and 
 *              list_table so they are at their default for the next iteration of 
 *              mt_test_2 with the number of threads being incremented.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
reset_globals_2(void)
{
    class_table_entry_t *class_table_entry;
    list_table_entry_t  *list_table_entry;
    prop_table_entry_t  *prop_table_entry;
    H5P_mt_class_sptr_t  class_sptr = {NULL, 0};
    H5P_mt_list_sptr_t   list_sptr  = {NULL, 0};

    /* Init Class Table */
    for (int i = 0; i < CLASS_TABLE_SIZE; i++) {
        class_table_entry = &class_table[i];

        atomic_store(&(class_table_entry->class_sptr), class_sptr);

        atomic_store(&(class_table_entry->id), H5I_INVALID_HID);
        atomic_store(&(class_table_entry->status), DOESNT_EXIST);
        atomic_store(&(class_table_entry->op_count), 0);
        atomic_store(&(class_table_entry->ver_closed), 0);
        atomic_store(&(class_table_entry->ver_deleted), 0);

        atomic_store(&(class_table_entry->parent_id), H5I_INVALID_HID);

        atomic_store(&(class_table_entry->ver_copied), 0);

        for (uint32_t j = 0; j < class_table_entry->num_prop_entries; j++) {
            prop_table_entry = &class_table_entry->prop_table[j];
            atomic_store(&(prop_table_entry->status), DOESNT_EXIST);
        }
    }

    for (int i = 0; i < LIST_TABLE_SIZE; i++) {
        list_table_entry = &list_table[i];

        atomic_store(&(list_table_entry->list_sptr), list_sptr);

        atomic_store(&(list_table_entry->id), H5I_INVALID_HID);
        atomic_store(&(list_table_entry->status), DOESNT_EXIST);
        atomic_store(&(list_table_entry->op_count), 0);
        atomic_store(&(list_table_entry->ver_deleted), 0);

        atomic_store(&(list_table_entry->parent_id), H5I_INVALID_HID);

        atomic_store(&(list_table_entry->ver_copied), 0);

        for (uint32_t j = 0; j < list_table_entry->num_prop_entries; j++) {
            prop_table_entry = &list_table_entry->prop_table[j];
            atomic_store(&(prop_table_entry->status), DOESNT_EXIST);
        }
    }

    return SUCCEED;

} /* end reset_globals_2(void) */

/****************************************************************************************
 * Function:    create_starting_classes_and_lists
 *
 * Purpose:     Creates the first four classes, all derived from the test_root class,
 *              with each class having an increasing number of properties (0-3) created
 *              and inserted as their default properties. Additionally, a list is derived
 *              from each of those classes.
 *
 *              This is done so there are a few classes and lists already created at the
 *              beginning of the test for operations to be performed on.
 *
 *              NOTE: As mentioned in the description for init_globals_2 a number of
 *              specified entries in class_table and list_table are for being created as
 *              copies. The fourth list_entry is one such entry, thus it is skipped and
 *              instead the fifth entry is created due to it being a list derived from
 *              the class_table's fourth entry.
 * 
 *              NOTE: Due to these classes and lists being created prior to the multiple
 *              threads being created these operations aren't stored in any 
 *              thread_params_t. As such during check_operations when checking on these
 *              objects, an extra operation is added to the operation count for these 
 *              class and list entries as their create operation.
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
create_starting_classes_and_lists(void)
{
    class_table_entry_t         *class_entry; /* current entry in the class_table */
    list_table_entry_t          *list_entry;  /* current entry in the list_table */
    status_t                     status;      /* class_entry and list_entry status */
    status_t                     prop_status; /* prop_entry status */
    H5P_mt_class_t              *parent;      /* parent for the starting classes */
    H5P_mt_class_t              *class;       /* current class being created */
    H5P_mt_active_thread_count_t thrd;        /* current class's thrd field */
    H5P_mt_list_t               *list;        /* current list being created */
    H5P_mt_class_sptr_t          class_sptr;  /* class_entry's sptr to the class obj */
    H5P_mt_list_sptr_t           list_sptr;   /* list_entry's sptr to the list obj */
    prop_table_entry_t          *prop_entry;  /* current entry in a prop_table */
    H5P_mt_prop_t               *prop;        /* current property being created */
    H5P_mt_list_table_entry_t   *lkup_tbl_entry; /* lkup_tbl for a list being created */
    H5P_mt_list_prop_ref_t       base;        /* base in a list's lkup_tbl */
    hid_t                        id;          /* class's or list's index hid */
    size_t                       phys_pl_len = 0; /* Physicaly length of new LFSLL */
    size_t                       log_pl_len  = 0; /* Logicaly length of new LFSLL */
    hid_t                        class1_id;   /* hid for class1 */
    hid_t                        class2_id;   /* hid for class2 */
    hid_t                        class3_id;   /* hid for class3 */
    hid_t                        class4_id;   /* hid for class4 */
    size_t                       nprops_inherited; /* number of props a list inherits */
    int                          value = 1;   /* Default value for default properties */
    
    herr_t                       ret_value = SUCCEED;
    
    /** 
     * These last 4 fields are just for parameters in the function 
     * H5P__mt_ins_or_mod_prop__lfsll_ins() and are used for stats tracking, with
     * checksum_cols being used in cases when a chksum collision of properties'
     * checksum occurs, which doesn't happen during these tests.
     */
    uint32_t                     deletes     = 0; /* Tracks number of deletes */
    uint32_t                     visited     = 0; /* Tracks number of nodes visited */
    uint32_t                     thrd_cols   = 0; /* Tracks number of thread cols */
    bool                         chksum_cols = FALSE;


    /* The test_root class is the parent for all starting classes */
    parent = (H5P_mt_class_t *)H5I_object(TEST_ROOT_ID_g);
    CHECK_PTR(parent, "H5I_object");

    /* Loop to create the four default classes */
    for (int i = 0; i < 4; i++) {
        /* Get the entry from the class_table */
        class_entry = &class_table[i];

        class_sptr = atomic_load(&(class_entry->class_sptr));
        assert(class_sptr.ptr == NULL);
        assert(class_sptr.sn == 0);

        assert(class_entry->name);
        assert(H5I_INVALID_HID == atomic_load(&(class_entry->id)));
        status = atomic_load(&(class_entry->status));
        assert(status == DOESNT_EXIST);

        /* Update status to in progress */
        status = IN_PROGRESS;
        atomic_store(&(class_entry->status), status);
        atomic_fetch_add(&(class_entry->op_count), 1);

        /**
         * Create the class 
         * H5P__mt_create_class() is used instead of H5Pcreate_class due to 
         * H5P__mt_create_class() being used to create the default classes 
         * during regular HDF5 initialization.
         */
        class = H5P__mt_create_class(parent, class_entry->name, H5P_TYPE_USER, 0,
                                     NULL, NULL, NULL, NULL, NULL, NULL);
        CHECK_PTR(class, "H5P__mt_create_class");
        assert(class);
        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);
      

        /* Set up the default properties */
        phys_pl_len = atomic_load(&(class->phys_pl_len));
        log_pl_len  = atomic_load(&(class->log_pl_len));

        /* Add some default properties to the class from the prop_table */
        for (int j = 0; j < i; j++) {
            prop_entry = &class_entry->prop_table[j];
            assert(atomic_load(&(prop_entry->status)) == DOESNT_EXIST);

            prop = H5P__mt_create_prop(prop_entry->name, &value, sizeof(value), TRUE, 1, NULL, NULL, NULL,
                                       NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_PTR(prop, "H5P__mt_create_prop");
            assert(prop);
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG);

            H5P__mt_ins_or_mod_prop__lfsll_ins(class->pl_head, prop, &deletes, &visited, &thrd_cols,
                                               &chksum_cols);

            atomic_store(&(prop_entry->status), EXISTS);

            phys_pl_len++;
            log_pl_len++;
        }

        /* Update physical and logical length of the LFSLL for the class */
        atomic_store(&(class->phys_pl_len), phys_pl_len);
        atomic_store(&(class->log_pl_len), log_pl_len);

        /* Register the class in the index */
        atomic_store(&(class->id), H5I_register(H5I_GENPROP_CLS, class, FALSE));

        id = atomic_load(&(class->id));
        assert(id != H5I_INVALID_HID);

        /* Update necessary fields */
        atomic_store(&(class_entry->id), id);
        atomic_store(&(class_entry->parent_id), atomic_load(&(parent->id)));

        status = EXISTS;
        atomic_store(&(class_entry->status), status);

        class_sptr.ptr = class;
        class_sptr.sn  = class_sptr.sn + 1;
        atomic_store(&(class_entry->class_sptr), class_sptr);

        /* Set class's opening flag to FALSE now that it's complete */
        thrd = atomic_load(&(class->thrd));
        assert(thrd.opening);
        assert(!thrd.closing);

        thrd.opening = FALSE;

        atomic_store(&(class->thrd), thrd);

        /* Store the ids for each starting class for testing and creating lists */
        switch (i) {
            case 0:
                class1_id = id;
                break;
            case 1:
                class2_id = id;
                break;
            case 2:
                class3_id = id;
                break;
            case 3:
                class4_id = id;
                break;
            default:
                assert(FALSE);
        }

    } /* end for() loop to create starting classes */


    /* Loop to create the four default lists */
    for (int i = 0; i < 4; i++) {
        /**
         * This also tests that the created classes were correctly
         * added to the index, and can be retrieved via their IDs.
         */

        switch (i) {
            case 0:
                class = (H5P_mt_class_t *)H5I_object(class1_id);
                CHECK_PTR(class, "H5I_object");
                assert(class);
                assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);
                assert(0 == strcmp(class->name, "NES"));
                class_entry = &class_table[i];
                assert(atomic_load(&(class->id)) == atomic_load(&(class_entry->id)));
                assert(0 == strcmp(class->name, class_entry->name));
                break;
            case 1:
                class = (H5P_mt_class_t *)H5I_object(class2_id);
                CHECK_PTR(class, "H5I_object");
                assert(class);
                assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);
                assert(0 == strcmp(class->name, "Sega Genesis"));
                class_entry = &class_table[i];
                assert(atomic_load(&(class->id)) == atomic_load(&(class_entry->id)));
                assert(0 == strcmp(class->name, class_entry->name));
                break;
            case 2:
                class = (H5P_mt_class_t *)H5I_object(class3_id);
                CHECK_PTR(class, "H5I_object");
                assert(class);
                assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);
                assert(0 == strcmp(class->name, "Gameboy"));
                class_entry = &class_table[i];
                assert(atomic_load(&(class->id)) == atomic_load(&(class_entry->id)));
                assert(0 == strcmp(class->name, class_entry->name));
                break;
            case 3:
                class = (H5P_mt_class_t *)H5I_object(class4_id);
                CHECK_PTR(class, "H5I_object");
                assert(class);
                assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);
                assert(0 == strcmp(class->name, "SNES"));
                class_entry = &class_table[i];
                assert(atomic_load(&(class->id)) == atomic_load(&(class_entry->id)));
                assert(0 == strcmp(class->name, class_entry->name));
                break;
            default:
                assert(FALSE);

        } /* end switch (i) */

        /**
         * The 4th entry in the list_table is setup to be a copy of the 3rd entry's list.
         * Thus the 4th entry is skipped, and the 5th entry is created instead.
         */
        if (i == 3) {
            list_entry = &list_table[i + 1];
        }
        else {
            list_entry = &list_table[i];
        }

        assert(H5I_INVALID_HID == atomic_load(&(list_entry->id)));
        status = atomic_load(&(list_entry->status));
        assert(status == DOESNT_EXIST);

        /* Update status to in progress */
        status = IN_PROGRESS;
        atomic_store(&(list_entry->status), status);
        atomic_fetch_add(&(list_entry->op_count), 1);

        /**
         * Create the list 
         * H5P__mt_create_list() is used instead of H5Pcreate_class due to 
         * H5P__mt_create_list() being used to create the default lists 
         * during regular HDF5 initialization.
         */
        list = H5P__mt_create_list(class, NULL, FALSE, 0, FALSE);
        CHECK_PTR(list, "H5P__mt_create_list");
        assert(list);
        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

        /* Update the necessary fields */
        id = atomic_load(&(list->plist_id));
        assert(H5I_INVALID_HID != id);

        atomic_store(&(list_entry->id), id);
        atomic_store(&(list_entry->parent_id), atomic_load(&(class->id)));

        status = EXISTS;
        atomic_store(&(list_entry->status), status);

        list_sptr.ptr = list;
        list_sptr.sn  = list_sptr.sn + 1;
        atomic_store(&(list_entry->list_sptr), list_sptr);

        /* Update list_entry's prop_table */
        nprops_inherited = list->nprops_inherited;

        /* If nprops_inherited > 0, update list_entry's prop_table */
        if (nprops_inherited > 0) {
            for (size_t j = 0; j < nprops_inherited; j++) {
                lkup_tbl_entry = &list->lkup_tbl[j];
                base           = atomic_load(&(lkup_tbl_entry->base));

                if (base.ptr && (0 == atomic_load(&(lkup_tbl_entry->base_delete_version)))) {
                    prop_entry = search_prop_table(list_entry->prop_table, list_entry->num_prop_entries,
                                                   lkup_tbl_entry->chksum, lkup_tbl_entry->name);

                    CHECK_PTR(prop_entry, "search_prop_table");
                    assert(prop_entry);

                    prop_status = atomic_load(&(prop_entry->status));
                    assert(prop_status == DOESNT_EXIST);

                    prop_status = EXISTS;

                    /* Not done as a cas, due to list not existing before now */
                    atomic_store(&(prop_entry->status), prop_status);

                } /* end if() */

            } /* end for (size_t j = 0; j < nprops_inherited; j++ ) */

        } /* end if ( nprops_inherited > 0 ) */

        /* Double check there aren't any properties in the list's LFSLL */
        assert(nprops_inherited == atomic_load(&(list->nprops)));

    } /* end for() loop to create starting lists */

    return (ret_value);

} /* end create_starting_classes_and_lists() */

#if 0
/****************************************************************************************
 * Function:    generate_prop_value
 *
 * Purpose:     Randomly creates a value for a property. The value can be of type, int,
 *              double, float, or char *. This allows involved testing of properties with
 *              different value types ensuring there are no problems.
 *
 *
 * Return:      Success: Returns a new H5P_mt_prop_value_t for a H5P_mt_prop_t
 *
 *              Can only fail during the strdup() functions and will assert stop if a
 *              failure occurs there.
 *
 ****************************************************************************************
 */
uint64_t
generate_prop_value( *prop_entry, uint64_t version)
{
    uint64_t      value;
    
    H5P_mt_prop_value_t ret_value = {NULL, 0};

    assert(prop_entry);

    value = version;

    ret_value.size = sizeof(value);

    memcpy(ret_value.ptr, &value, ret_value.size);

    return(ret_value);

} /* end generate_prop_value() */
#endif

/****************************************************************************************
 * Function:    search_prop_table
 *
 * Purpose:     Searches a prop_table in a class_table_entry_t or list_table_entry_t for
 *              a specified entry in the prop_table.
 *
 * Return:      Success: Pointer to the target prop_table_entry_t
 *
 *              Failure: NULL
 *
 ****************************************************************************************
 */
prop_table_entry_t *
search_prop_table(prop_table_entry_t *prop_table, uint32_t num_prop_entries, int64_t chksum, const char *name)
{
    prop_table_entry_t *prop_entry;

    prop_table_entry_t *ret_value = NULL;

    assert(prop_table);
    assert(name);

    for (uint32_t i = 0; i < num_prop_entries; i++) {
        prop_entry = &prop_table[i];

        if (prop_entry->chksum == chksum) {
            if (0 == strcmp(prop_entry->name, name)) {
                return (prop_entry);
            }
        }
    }

    return (ret_value);

} /* end search_prop_table() */

/****************************************************************************************
 * Function:    mt_test_2
 *
 * Purpose:     Primary function for the multithread tests, which gets the global
 *              structures ready and a small number of classes and lists created with a
 *              couple default properties so some H5P objects exist at the start of the
 *              tests. Also, gets the max number of threads for the tests, and loops
 *              calling test_2_helper() with the number of threads increasing from one to
 *              max_num_threads. At the end of the loop cleanup functions are called so
 *              globals are reset to default for the next iteration of the tests and all
 *              H5P objects created during the test (which were closed at the end of
 *              test_2_helper(), thus are on the H5P free lists) are properly freed.
 *
 *              NOTE: At the moment max_num_threads is hard coded at
 *              DEFAULT_MAX_NUM_THREADS which is set to 64 threads due to my CPU having
 *              32 cores and wanting to push above that amount to cause bugs to show up
 *              more easily.
 *              
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
mt_test_2(TestParams_t H5_ATTR_UNUSED *params)
{
    int max_num_threads = GetTestMaxNumThreads();
    // int test_express    = GetTestExpress();
    H5P_mt_class_t           *test_root;
    H5P_mt_class_ref_counts_t ref_count;
    herr_t                    ret;

    /* Get the test root class from the index */
    test_root = (H5P_mt_class_t *)H5I_object(TEST_ROOT_ID_g);
    CHECK_PTR(test_root, "H5I_object");
    assert(test_root);
    assert(atomic_load(&(test_root->tag)) == H5P_MT_CLASS_TAG);

    /* Reset the test_root's stats for the new test */
    ret = H5P__reset_stats_class(test_root);
    assert(test_root);
    assert(atomic_load(&(test_root->tag)) == H5P_MT_CLASS_TAG);

    ref_count = atomic_load(&(test_root->ref_count));
    assert(ref_count.pl == 0);
    assert(ref_count.plc == 0);
    assert(ref_count.deleted == FALSE);
    assert(atomic_load(&(test_root->num_thrd_count_update)) == 0);
    assert(atomic_load(&(test_root->num_ref_count_update)) == 0);

    /* Initialize the global structures for testing */
    ret = init_globals_2();
    CHECK_I(ret, "init_globals_2");

    ret = create_starting_classes_and_lists();
    CHECK_I(ret, "create_starting_classes_and_lists");

    /** TODO: adjust this to work with testframe's functions to get max threads */
    if (max_num_threads > DEFAULT_MAX_NUM_THREADS || max_num_threads < 0) {
        max_num_threads = DEFAULT_MAX_NUM_THREADS;
    }

    for (int num_threads = 1; num_threads <= max_num_threads; num_threads++) {
        test_2_helper(num_threads);

        /* Clear all test lists, classes, and properties */
        ret = H5P__mt_term_free_lists();
        CHECK_I(ret, "H5P__mt_term_free_lists");

        /* Reset global stats before incrementing num_thread and testing again */
        ret = H5P__reset_stats_global();
        CHECK_I(ret, "H5P__reset_stats_global");

        /** TODO: add reset funtion for globals */
        ret = reset_globals_2();
        CHECK_I(ret, "reset_globals_2");

        ret = create_starting_classes_and_lists();
        CHECK_I(ret, "create_starting_classes_and_lists");

        atomic_store(&(OPS_PERFORMED), 0);
    }

    return SUCCEED;

} /* end full_mt_h5p_test() */

/****************************************************************************************
 * Function:    test_2_helper
 *
 * Purpose:     Allocates and intializes a thread_params_t struct for each thread in this
 *              iteration of tests.
 *
 *              NOTE: The struct thread_params_t is a structure assigned to each thread
 *              to track the number of operations that thread has performed and an array
 *              of test_op_info_t (*op_table) for information related to every operation
 *              performed to be stored. So after all operations are performed the 
 *              op_table of all threads can be searched and sorted to check that all 
 *              operations were done correctly and if there were thread collisions, that 
 *              the threads wound up completeting their operations in some sort of order 
 *              (order is based on the version number of the property list or property 
 *              list class they were assigned when entering that object, and not by type
 *              operation).
 *
 *              This function then creates the number of threads specified and has them
 *              call h5p_full_cols_mt_test().
 *
 *              After all threads have joined up, check_operations is called to check
 *              all of the stats and that all operations were performed correctly.
 *
 *              Lastly, all H5P objects that were created during the test are closed, 
 *              putting them on their respective H5P free list.
 *
 * Return:      static void
 *
 ****************************************************************************************
 */
static void
test_2_helper(int num_threads)
{
    char            banner[80];
    int             i;
    int             err_cnt = 0;
    pthread_t            threads[DEFAULT_MAX_NUM_THREADS]; /* Array of pthreads */
    /* Array of thread_params_t (one struct for each thread) */
    thread_params_t      thread_params[DEFAULT_MAX_NUM_THREADS]; 
    class_table_entry_t *class_entry; /* Current entry in the class_table */
    list_table_entry_t  *list_entry;  /* Current entry in the list_table */
    H5P_mt_class_sptr_t  class_sptr;  /* class_entry's sptr to the class obj */
    H5P_mt_list_sptr_t   list_sptr;   /* list_entry's sptr to the list obj */
    status_t             status;      /* status for the current class or list entry */
    herr_t               ret;         /* Basic return value */

    assert(1 <= num_threads);
    assert(num_threads <= DEFAULT_MAX_NUM_THREADS);

    sprintf(banner, "multi-thread test 2 -- %d threads", num_threads);

    TESTING(banner);
    fflush(stdout);

    /* Allocate and initialize the thread_param_t structs for each thread */
    for (i = 0; i < num_threads; i++) {
        thread_params[i].thread_id     = i;
        thread_params[i].num_threads   = num_threads;
        thread_params[i].ops_performed = 0;

        thread_params[i].op_table = (test_op_info_t *)malloc(TOTAL_OPS_PER_THREAD * (sizeof(test_op_info_t)));

        if (NULL == thread_params[i].op_table) {
            assert(FALSE);
            err_cnt++;

            TestErrPrintf("mt_test_2(): allocation of test_op_info_t failed.\n");
        }
        for (uint64_t j = 0; j < TOTAL_OPS_PER_THREAD; j++) {
            thread_params[i].op_table[j].class        = NULL;
            thread_params[i].op_table[j].list         = NULL;
            thread_params[i].op_table[j].id           = H5I_INVALID_HID;
            thread_params[i].op_table[j].test_id      = -1;
            thread_params[i].op_table[j].class_name   = NULL;
            thread_params[i].op_table[j].parent_name  = NULL;
            thread_params[i].op_table[j].prop_name    = NULL;
            thread_params[i].op_table[j].prop         = NULL;
            thread_params[i].op_table[j].op           = NO_OP_YET;
            thread_params[i].op_table[j].op_num       = 0;
            thread_params[i].op_table[j].obj_ver      = 0;
            thread_params[i].op_table[j].op_ver       = 0;
            thread_params[i].op_table[j].result       = NOT_ATTEMPTED;
            thread_params[i].op_table[j].obj_isa_copy = FALSE;
            thread_params[i].op_table[j].sorted       = FALSE;
        }

    } /* end for ( i = 0; i < num_threads; i++ ) */

    if (num_threads == 1) {
        h5p_full_cols_mt_test((void *)&thread_params[0]);
    }
    else if (num_threads > 1) {
        /* Loop to create the threads and have each one run h5p_full_cols_mt_test() */
        for (i = 0; i < num_threads; i++) {
            if (0 !=
                pthread_create(&(threads[i]), NULL, &h5p_full_cols_mt_test, (void *)(&(thread_params[i])))) {
                assert(FALSE);

                err_cnt++;

                TestErrPrintf("mt_test_2(): creation of thread %d failed.\n", i);
            }
        }

        /* Wait for all threads to complete */
        for (i = 0; i < num_threads; i++) {
            if (0 != pthread_join(threads[i], NULL)) {
                assert(FALSE);

                err_cnt++;

                TestErrPrintf("mt_test_2(): joining of thread %d failed.\n", i);
            }
            else {
                /* Collect error count from joined therads */
                // err_cnt += params[i].err_cnt;
            }
        }
    }

    ret = check_operations(thread_params, (uint64_t)(num_threads));
    CHECK_I(ret, "check_operations");
    assert(ret == SUCCEED);

    /* Close all of the test lists */
    for (i = 0; i < LIST_TABLE_SIZE; i++) {
        list_entry = &list_table[i];
        status     = atomic_load(&(list_entry->status));

        if (status == EXISTS) {
            ret = H5Pclose(atomic_load(&(list_entry->id)));

            CHECK_I(ret, "H5Pclose");
            assert(ret == 0);
        }
        else if (status == IN_PROGRESS) {
            assert(FALSE);
        }
        else if (status == DOESNT_EXIST) {
            list_sptr = atomic_load(&(list_entry->list_sptr));

            assert(!list_sptr.ptr);
        }

    } /* end for ( i = 0; i < LIST_TABLE_SIZE; i++ ) */

    /**
     * Close all of the test classes in backwards entry order,
     * to ensure the first 4 classes, the test default classes,
     * aren't closed until all others have been.
     */
    for (i = CLASS_TABLE_SIZE; i >= 0; i--) {
        class_entry = &class_table[i];
        status      = atomic_load(&(class_entry->status));

        if (status == EXISTS) {
            ret = H5Pclose_class(atomic_load(&(class_entry->id)));

            CHECK_I(ret, "H5Pclose_class");
            assert(ret == 0);
        }
        else if (status == IN_PROGRESS) {
            assert(FALSE);
        }
        else if (status == DOESNT_EXIST) {
            class_sptr = atomic_load(&(class_entry->class_sptr));

            assert(!class_sptr.ptr);
        }

    } /* end for ( i = 0; i < CLASS_TABLE_SIZE; i++ ) */

    if (0 == err_cnt) {
        PASSED();
    }
    else {
        IncTestNumErrs();
        H5_FAILED();
    }

} /* end test_2_helper() */

/****************************************************************************************
 * Function:    h5p_full_cols_mt_test
 *
 * Purpose:     This function loops having the threads call rand_op(), which
 *              has the threads perform a random operation on a H5P multithread
 *              structure. Upon completing the random operation the threads will
 *              increment the number of operations performed, both by that thread and
 *              total operations by all threads, and continue the loop till the total
 *              number of operations equals TOTAL_OPS_PER_THREAD.
 *
 * Return:      static void *
 *
 ****************************************************************************************
 */
static void *
h5p_full_cols_mt_test(void *_thread_params)
{
    thread_params_t *thread_params = (thread_params_t *)_thread_params;

    herr_t ret; /* Generic return value */

    assert(thread_params);
    assert(thread_params->ops_performed == 0);

    while (thread_params->ops_performed < TOTAL_OPS_PER_THREAD) {
        /* Perform a random H5P operation */
        ret = rand_op(thread_params);
        CHECK_I(ret, "rand_op");

        thread_params->ops_performed++;

        atomic_fetch_add(&(OPS_PERFORMED), 1);
    }

    return SUCCEED;

} /* end h5p_full_cols_mt_test() */

/****************************************************************************************
 * Function:    rand_op
 *
 * Purpose:     Using a simple implementation to get a random number 0-99 that is roughly
 *              1% per number. The random number is used to decide which operation will
 *              be performed next.
 *              NOTE: The probability for each operation is very loosely based on the
 *              frequency that those operations are used during normal HDF5 library
 *              operations, while still allowing for the possibly that thread collisions
 *              can occur in each operation.
 *                   0-9  (~10%): create_list  - creates a new property list.
 *                  10-17 ( ~8%): create_class - creates a new property list class.
 *                  18-21 ( ~4%): copy_list    - creates a copy of an existing list.
 *                  22-24 ( ~3%): copy_class   - creates a copy of an existing class.
 *                  25-44 (~20%): read_list    - searches for a property in a list.
 *                  45-59 (~15%): read_class   - searches for a property in a class.
 *                  60-74 (~15%): write_list   - modifies a list.
 *                  75-84 (~10%): write_class  - modifies a class.
 *                  85-89 ( ~5%): cmp_list     - compares two lists.
 *                  90-94 ( ~5%): cmp_class    - compares two classes.
 *                  95-97 ( ~3%): close_list   - closes a list.
 *                  98-99 ( ~2%): close_class  - closes a class.
 *
 *              TODO: removed the compare tests to limit scope to fix bugs on the more
 *              important functions easier.
 *                   0-9  (~10%): create_list  - creates a new property list.
 *                  10-19 (~10%): create_class - creates a new property list class.
 *                  20-24 ( ~5%): copy_list    - creates a copy of an existing list.
 *                  25-28 ( ~4%): copy_class   - creates a copy of an existing class.
 *                  29-50 (~22%): read_list    - searches for a property in a list.
 *                  51-67 (~17%): read_class   - searches for a property in a class.
 *                  68-83 (~16%): write_list   - modifies a list.
 *                  84-94 (~11%): write_class  - modifies a class.
 *                  95-97 ( ~3%): close_list   - closes a list.
 *                  98-99 ( ~2%): close_class  - closes a class.
 * 
 * 
 *              NOTE: modifying a list or class means to create a new property, modify
 *              an existing property, or to delete a property from that list or class.
 *
 *              NOTE: close and delete in relation of lists and classes mean different
 *              things. Close is referring to the act of calling the close function for
 *              that object which attempts to decrement the ref count of its ID in the
 *              index. For lists, as long as the close is successful, will always result
 *              in the list being deleted. However, for classes it may not, due to when
 *              a H5P object is derived from a class that derived object increments their
 *              parent's index ID's ref count. Thus, if a class is closed but it has
 *              existing derived objects, its ID ref count won't be decremented to 0 and
 *              won't be deleted. But if that class is closed again an assert in H5I will
 *              fail. To prevent this the status EXISTS_BUT_CLOSED is set to prevent
 *              multiple close attempts on a class. But when that class's derived objects
 *              are closed the class will automatically finish being closed and be
 *              deleted.
 *
 *              NOTE: For more details on the operations see their comment descriptions
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
rand_op(thread_params_t *thread_params)
{
    int    operation;
    herr_t ret; /* Generic return value */

    operation = rand() % 100;

    switch (operation) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            ret = create_list(thread_params);
            assert(ret == SUCCEED);
            break;
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
#if 0 /* Yes compare */
            ret = create_class(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 18:
        case 19:
#if 1 /* No compare */
            ret = create_class(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 20:
        case 21:
#if 0 /* Yes compare */
            ret = copy_list(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 22:
        case 23:
        case 24:
#if 0 /* Yes compare */
            ret = copy_class(thread_params);
            assert(ret == SUCCEED);
            break;
#else /* No compare */
            ret = copy_list(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 25:
        case 26:
        case 27:
        case 28:
#if 1 /* No compare */
            ret = copy_class(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 29:
        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
        case 38:
        case 39:
        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
#if 0 /* Yes compare */
            ret = read_list(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 45:
        case 46:
        case 47:
        case 48:
        case 49:
        case 50:
#if 1 /* No compare */
            ret = read_list(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 51:
        case 52:
        case 53:
        case 54:
        case 55:
        case 56:
        case 57:
        case 58:
        case 59:
#if 0 /* Yes compare */
            ret = read_class(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 60:
        case 61:
        case 62:
        case 63:
        case 64:
        case 65:
        case 66:
        case 67:
#if 1 /* No compare */
            ret = read_class(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 68:
        case 69:
        case 70:
        case 71:
        case 72:
        case 73:
        case 74:
#if 0 /* Yes compare */
            ret = write_list(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 75:
        case 76:
        case 77:
        case 78:
        case 79:
        case 80:
        case 81:
        case 82:
        case 83:
#if 1 /* No compare */
            ret = write_list(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 84:
#if 0 /* Yes compare */
            ret = write_class(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 85:
        case 86:
        case 87:
        case 88:
        case 89:
#if 0 /* Yes compare */
            ret = cmp_list(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 90:
        case 91:
        case 92:
        case 93:
        case 94:
#if 0 /* Yes compare */
            ret = cmp_class(thread_params);
            assert(ret == SUCCEED);
            break;
#else /* No compare */
            ret = write_class(thread_params);
            assert(ret == SUCCEED);
            break;
#endif
        case 95:
        case 96:
        case 97:
            ret = close_list(thread_params);
            assert(ret == SUCCEED);
            break;
        case 98:
        case 99:
            ret = close_class(thread_params);
            assert(ret == SUCCEED);
            break;
        default:
            assert(FALSE);

    } /* end switch ( operation ) */

    return SUCCEED;

} /* end rand_op() */

/****************************************************************************************
 * Function:    create_list
 *
 * Purpose:     Attempts to create a new property list.
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a list_entry 
 *              from the list_table is randomly chosen (if the list_entry is marked as a 
 *              copy it's iterated to a non-copy). If that list has been created during 
 *              this iteration of tests, it is searched for in the index, and the results 
 *              are set as necessary. 
 *               *  If the list has been created and is found in the index the result is
 *                  LIST_ALREADY_EXISTS.
 *               *  If the list has been created but is not in the index, and the 
 *                  list_entry has a status of IN_PROGRESS, we loop and randomly grab a 
 *                  different list_entry to attempt creating.
 *               *  If the list has been created but is not in the index, and is not
 *                  IN_PROGRESS, it must be deleted. The status and the list struct tag
 *                  are checked to ensure it is deleted and the result is LIST_DELETED.
 * 
 *               *  If the list hasnt been created, and has a status of DOESNT_EXIST, 
 *                  we atomically update the status to IN_PROGRESS and set the create
 *                  flag to TRUE to move on and attempt to create it. 
 *               *  If the list hasn't been created, and has a status of 
 *                  CLOSING_IN_PROGRESS, we loop and check the status of this list_entry
 *                  again. Since, the list hasn't been created the thread attempting to
 *                  close it will obviously fail and will update the status back to
 *                  DOESNT_EXIST.
 *               *  If the list hasn't been created, and has a status of IN_PROGRESS, we
 *                  loop and randomly grab a different list_entry to attempt creating.
 * 
 *              NOTE: threads store the information of each operation they perform in the
 *              entry of the op_table for that operation number. This entry is called 
 *              op_info (see test_op_info_t description for more details)
 * 
 *              If the create flag is TRUE, the list_entry's necessary parent info is
 *              grabbed, and H5Pcreate() is called to attempt to create the list.
 *              
 *              If creating it failed, we search the index for the parent, which should 
 *              not be in the index, else creating the list should have succeeded. Then 
 *              we check the parent_entry from the class_table to see if the parent has
 *              ever been created.
 *               *  If the parent has been created, we check if it's been deleted and if
 *                  it has, the result is marked as PARENT_DELETED and the obj_ver is set
 *                  to the version the parent class was deleted at. 
 *               *  If the parent has been created, and not deleted, and the status is 
 *                  either EXISTS or IN_PROGRESS, the parent class was created just after
 *                  our attempt to create the list. Loop the create section to attempt
 *                  creating the list again. As long as the parent isn't deleted before 
 *                  this thread calls H5Pcreate() it should succeed this time.
 * 
 *               *  If the parent has NOT been created, we mark the result as 
 *                  PARENT_DOESNT_EXIST and then must atomically update the status of 
 *                  this list_entry back to DOESNT_EXIST
 * 
 *              If creating the list succeeded, we search the index for the list to 
 *              ensure it was correctly created and inserted, and update the list_entry
 *              with the list's hid, and a pointer to the list.
 *              Next all properties in the list are iterated (the lkup_tbl and LFSLL) and
 *              the corresponding list_entry->prop_table entry (prop_entry) is grabbed to
 *              atomically update the status from DOESNT_EXIST to EXISTS.
 *              After all properties in the list have had their respective prop_entry 
 *              statuses updated, the list_entry's status is atomically updated from
 *              IN_PROGRESS to EXISTS.              
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
create_list(thread_params_t *thread_params)
{
    list_table_entry_t  *list_entry;    
    class_table_entry_t *parent_entry;  
    H5P_mt_class_sptr_t  parent_sptr;   
    H5P_mt_list_sptr_t   list_sptr;     
    status_t             list_status;   
    status_t             update_status; 
    status_t             parent_status;
    H5P_mt_list_t       *list       = NULL; 
    H5P_mt_list_t       *check_list = NULL; /* Used as a double check for the list obj */
    H5P_mt_class_t      *parent     = NULL; 
    H5P_mt_prop_t       *valid_prop = NULL; 
    H5P_mt_prop_t       *prev_prop  = NULL; 
    int                  r;             /* Used to randomly select the list_entry */
    int                  loop_count = 0; /* Num times looped waiting for another thread */
    test_op_info_t      *op_info    = NULL; 
    uint32_t             op_num;        
    bool                 done          = FALSE; 
    bool                 loop_check    = FALSE; /** TODO: Used for debugging will remove */
    bool                 create        = FALSE; 
    bool                 get_diff_list = FALSE;
    bool                 base_flag     = FALSE; /* See H5P__mt_entry_find_version's description */
    bool                 skip          = FALSE; /* Flag used for updating prop status */
    bool                 try_again     = FALSE; /* Used for a do-while loop */
    size_t               nprops_inherited; /* Num props inherited from parent */
    size_t               nprops;        /* Total number of props */
    H5P_mt_list_table_entry_t *lkup_tbl_entry; 
    prop_table_entry_t  *prop_entry = NULL; 
    status_t             prop_status;   /* status of the prop_entry */
    status_t             update_prop_status; /* used to atomically update prop_entry's status */
    hid_t                list_id;       
    hid_t                parent_id;     
    uint64_t             version;       /* Version of the list */ 
    uint64_t             check_ver;     /* Used for checking list's version */
    uint64_t             ver_del;
    uint64_t             visited = 0;   /* See H5P__get_next_valid_prop's description */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = CREATE;
    op_info->op_num = op_num;

    /**
     * do-while get_diff_list is TRUE.
     * get_diff_list's default is FALSE, but gets flipped to 
     * TRUE if its status is IN_PROGRESS meaning another thread
     * is already attempting to create it. 
     */
    do {
        /* Get a random entry from the list table */
        r = rand() % LIST_TABLE_SIZE;

        list_entry = &list_table[r];

        /* If the entry is marked to be copy grab the next entry */
        while (list_entry->copy) {
            r++;
            list_entry = &list_table[r];
        }

        get_diff_list = FALSE;

        op_info->parent_name = list_entry->parent_name;
        op_info->test_id     = list_entry->test_list_id;

        done = FALSE;
        do {
            list_sptr = atomic_load(&(list_entry->list_sptr));
            list      = list_sptr.ptr;

            /* If not NULL the list has been created, perform some checks */
            if (list) {
                /* Check if the list is in the index */
                H5E_BEGIN_TRY
                {
                    check_list = (H5P_mt_list_t *)H5I_object(atomic_load(&(list->plist_id)));
                }
                H5E_END_TRY

                list_status = atomic_load(&(list_entry->status));

                /* Ensure the returned check_list is the same as list */
                if (check_list) {
                    assert(list == check_list);

                    /* Check the status of the list_entry */
                    assert(list_status != DOESNT_EXIST);

                    /* Update op_info */
                    op_info->list    = list;
                    op_info->id      = atomic_load(&(list->plist_id));
                    op_info->obj_ver = atomic_load(&(list->curr_version));
                    op_info->result  = LIST_ALREADY_EXISTS;

                    done = TRUE;
                }
                else /* If list isn't in the index */
                {
                    /* Check the status of the list_entry */
                    if (list_status == IN_PROGRESS) {
                        
                        /** 
                         * If status is IN_PROGRESS grab a different list to 
                         * avoid creating a duplicate list that isn't tracked.
                         */
                        get_diff_list = TRUE;
                        done          = TRUE;
                    }
                    else {
                        /**
                         * If the list isn't in the index, not IN_PROGRESS, and has 
                         * been created it must have been DELETED. Double check.
                         */
                        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);
                        assert(list_status == DELETED || list_status == CLOSING_IN_PROGRESS);
                        op_info->list    = list;
                        op_info->id      = atomic_load(&(list->plist_id));
                        op_info->obj_ver = atomic_load(&(list->curr_version));
                        op_info->result  = LIST_DELETED;

                        done = TRUE;
                    }

                } /* end else ( ! check_list ) */

            } /* end if ( list ) */
            /* else the list hasn't been created yet */
            else {
                
                /* Check list_status */
                list_status = atomic_load(&(list_entry->status));

                if (list_status == DOESNT_EXIST) {
                    update_status = IN_PROGRESS;

                    /* Attempt to atomically update list_status */
                    if (!atomic_compare_exchange_strong(&(list_entry->status), &list_status, update_status)) {
                        /* Atomic update failed, for now just try again once */
                        if (loop_check) {
                            assert(FALSE);
                        }
                        else {
                            loop_check = TRUE;
                        }
                    }
                    else {
                        /* Atomic update successful */
                        list_status = atomic_load(&(list_entry->status));
                        assert(list_status == IN_PROGRESS);

                        create = TRUE;
                        done   = TRUE;
                    }

                } /* end if ( list_status == DOESNT_EXIST ) */
                /**
                 * If the list hasn't been created yet and its status
                 * is CLOSING_IN_PROGRESS, the thread trying to close
                 * it will fail shortly. Loop and check again.
                 */
                else if (list_status == CLOSING_IN_PROGRESS) {
                    loop_count++;
                }
                /**
                 * Another thread is creating this list,
                 * loop and randomly grab a different list.
                 */
                else if (list_status == IN_PROGRESS) {
                    get_diff_list = TRUE;
                    done          = TRUE;
                }
                else {
                    /* No other status should be possible */
                    assert(FALSE);
                }
            } /* end else ( ! list ) */

        } while (!done);

    } while (get_diff_list);

    assert(done);
    assert(!get_diff_list);

    assert(list_status == IN_PROGRESS || ( create == FALSE &&
            op_info->result != NOT_ATTEMPTED));

    atomic_fetch_add(&(list_entry->op_count), 1);

    /* Set back to FALSE for next do-while that uses done */
    done = FALSE;

    /* If TRUE attempt to create the list */
    if (create) {
        /**
         * do-while try_again is TRUE. try_again is set to FALSE, and is only flipped to 
         * TRUE if the attempt to create the list fails, but the parent class has a 
         * status of EXISTS. This occurs because at the time of the attempt to create the
         * list the parent wasn't in the index yet, but another thread was in the process 
         * of creating it and has now finished. So, another attempt to create the list 
         * should succeed.
         */
        do {
            try_again = FALSE;

            /* Grab the parent info */
            parent_entry = atomic_load(&(list_entry->parent_entry));
            parent_id    = atomic_load(&(parent_entry->id));
            atomic_store(&(list_entry->parent_id), parent_id);

            /* Double check list pointer and list status */
            list_sptr = atomic_load(&(list_entry->list_sptr));
            list      = list_sptr.ptr;

            assert(!list);

            list_status = atomic_load(&(list_entry->status));
            assert(list_status == IN_PROGRESS || list_status == CLOSING_IN_PROGRESS);

            /* Attempt to create the list */
            H5E_BEGIN_TRY
            {
                list_id = H5Pcreate(parent_id);
            }
            H5E_END_TRY

            /* If creating the list failed, find why */
            if (list_id == H5I_INVALID_HID) {

                H5E_BEGIN_TRY
                {
                    parent = (H5P_mt_class_t *)H5I_object(parent_id);
                }
                H5E_END_TRY

                /* If parent is in index, creating the list shouldn't have failed */
                CHECK_PTR_NULL(parent, "create_list: H5I_object");
                assert(!parent);

                parent_status = atomic_load(&(parent_entry->status));
                parent_sptr = atomic_load(&(parent_entry->class_sptr));
                parent      = parent_sptr.ptr;

                /* If the parent has been created */
                if ( parent )
                {
                    /** 
                     * Check if its been deleted.
                     * NOTE: EXISTS_BUT_CLOSED and CLOSING_IN_PROGRESS statuses here are 
                     * treated as DELETED, but the thread performing the deleting hasn't 
                     * updated its status yet. Otherwise it would've be in the index, and 
                     * the list would've been created
                     */
                    if (parent_status == DELETED || 
                            parent_status == CLOSING_IN_PROGRESS ||
                            parent_status == EXISTS_BUT_CLOSED) {
                        /**
                         * If the version the parent was deleted at is still 0, loop and
                         * check again. We wait for it to not be 0, so we have an 
                         * accurate timeline of operations during check_operations().
                         */
                        ver_del = atomic_load(&(parent_entry->ver_deleted));

                        while ( ver_del == 0 )
                        {
                            sleep(1);

                            ver_del = atomic_load(&(parent_entry->ver_deleted));
                        }

                        assert(ver_del > 0);
                        assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_INVALID_TAG);

                        op_info->obj_ver = ver_del;
                        op_info->result  = PARENT_DELETED;
                    }
                    /**
                     * If the parent has been created, and hasn't been deleted, then
                     * when attempting to create the list, the parent was also in 
                     * the process of being created, but wasn't in the index yet.
                     * Try creating the list again, it should succeed.
                     */
                    else {
                        assert(parent_status == EXISTS || parent_status == IN_PROGRESS);

                        try_again = TRUE;
                    }
                }
                else
                {
                    assert(parent_status == DOESNT_EXIST || parent_status == IN_PROGRESS ||
                            parent_status == CLOSING_IN_PROGRESS);
                    
                    op_info->result = PARENT_DOESNT_EXIST;
                }

                /* If try_again == FALSE, then the list_entry's status needs reset */
                if (!try_again) {
                    /* Set list status back to DOESNT_EXIST */
                    update_status = DOESNT_EXIST;

                    done = FALSE;
                    do {
                        list_status = atomic_load(&(list_entry->status));

                        /**
                         * If CLOSING_IN_PROGRESS but the list hasn't been created 
                         * yet, the thread attempting the close, will fail and reset 
                         * the status back to IN_PROGRESS, wait until that is done 
                         * then try atomically updating status.
                         */
                        while (list_status == CLOSING_IN_PROGRESS) 
                        {
                            sleep(1);

                            list_status = atomic_load(&(list_entry->status));
                        }

                        /* If IN_PROGRESS atomically update status back to DOESNT_EXIST */
                        if (list_status == IN_PROGRESS) {
                            if (!atomic_compare_exchange_strong(&(list_entry->status), 
                                                        &list_status, update_status)) {
                                if (loop_check) {
                                    assert(FALSE);
                                }
                                else {
                                    loop_check = TRUE;
                                }
                            }
                            else {
                                /* Atomic update successful */
                                list_status = atomic_load(&(list_entry->status));
                                assert(list_status == DOESNT_EXIST);

                                done = TRUE;
                            }
                        }
                        else if ( list_status != CLOSING_IN_PROGRESS ) {
                            /**
                             * If the status is CLOSING_IN_PROGRESS, another thread 
                             * updated the status to it immediately after this thread 
                             * checked for that status. So, if that happens this will 
                             * loop and then see the status is CLOSING_IN_PROGRESS and 
                             * will wait for it to be changed. But the status 
                             * shouldn't be possible to be anything else we haven't 
                             * checked already.
                             */
                            assert(FALSE);
                        }

                    } while (!done);

                } /* end if ( ! try_again ) */

                assert(op_info->result != NOT_ATTEMPTED || try_again);

            }    /* end if ( list_id == H5I_INVALID_HID ) */
            else 
            {
                /* List was created, update list_entry and op_info */

                atomic_store(&(list_entry->id), list_id);
                atomic_store(&(list_entry->parent_id), parent_id);

                list = (H5P_mt_list_t *)H5I_object(list_id);
                assert(list);
                assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

                list_sptr.ptr = list;
                list_sptr.sn  = list_sptr.sn + 1;

                atomic_store(&(list_entry->list_sptr), list_sptr);

                /* Update list_entry's prop table */
                version = atomic_load(&(list->curr_version));
                assert(version = 1);

                nprops_inherited = list->nprops_inherited;
                nprops           = atomic_load(&(list->nprops));

                prev_prop = list->pl_head;
                assert(prev_prop);
                assert(atomic_load(&(prev_prop->tag)) == H5P_MT_PROP_TAG);
                assert(prev_prop->sentinel);

                size_t i = 0;
                /* Iterate all props at creation and update their status accordingly */
                do {
                    /* Iterate the lkup_tbl first */
                    if (i < nprops_inherited) {
                        lkup_tbl_entry = &list->lkup_tbl[i];

                        valid_prop = H5P__mt_entry_find_version(lkup_tbl_entry, version, &base_flag);

                        i++;
                    }
                    else if (i >= nprops_inherited && i < nprops ) {
                        valid_prop = H5P__get_next_valid_prop(prev_prop, version, &visited);

                        i++;

                        if (valid_prop) {
                            /* If in_lkup_tbl, it was already updated so skip */
                            if (valid_prop->in_lkup_tbl) {
                                skip = TRUE;
                            }
                            else {
                                skip = FALSE;
                            }
                        }
                        else {
                            assert(i == nprops);
                        }
                    }
                    else {
                        assert(i == nprops);
                        valid_prop = NULL;
                    }

                    /* If there is another valid property */
                    if (valid_prop && (skip == FALSE)) {
                        assert(atomic_load(&(valid_prop->tag)) == H5P_MT_PROP_TAG);
                        
                        /* Grab the respective prop_entry for the property */
                        prop_entry = search_prop_table(list_entry->prop_table, 
                                                       list_entry->num_prop_entries,
                                                       valid_prop->chksum, valid_prop->name);
                        CHECK_PTR(prop_entry, "search_prop_table");
                        assert(prop_entry);

                        /* Attempt to atomically update the status of the prop_entry */
                        loop_check = FALSE;
                        done       = FALSE;
                        do {
                            prop_status = atomic_load(&(prop_entry->status));

                            if (prop_status == DOESNT_EXIST) {
                                update_prop_status = EXISTS;

                                if (!atomic_compare_exchange_strong(&(prop_entry->status), &prop_status,
                                                                    update_prop_status)) {
                                    /**
                                     * TODO: For now we only try again once if cas fails
                                     */
                                    if (loop_check) {
                                        assert(FALSE);
                                    }
                                    else {
                                        loop_check = TRUE;
                                    }
                                }
                                else {
                                    done = TRUE;
                                }

                            } /* end if ( prop_status == DOESNT_EXIST ) */
                            else {

                                /**
                                 * If the prop_status is not DOESNT_EXIST and the lists's
                                 * version has been updated, then the prop was deleted by 
                                 * another thread (only worry about deleting props, 
                                 * because creating new ones waits till list status is 
                                 * EXISTS and modifying props doesn't update prop 
                                 * status). We don't need to update the prop_status, 
                                 * because the deleting thread has, or will, do that.
                                 */
                                check_ver = atomic_load(&(list->curr_version));

                                if (check_ver > version) {
                                    done = TRUE;
                                }
                                else {
                                    assert(FALSE);
                                }
                            }

                        } while (!done);

                        prev_prop = valid_prop;

                    } /* end if ( valid_prop && ( skip == FALSE ) ) */

                } while ( valid_prop );

                /* Update op_info */
                op_info->list    = list;
                op_info->id      = list_id;
                op_info->obj_ver = version;
                op_info->result  = OP_SUCCESS;

                /* Update list status */
                update_status = EXISTS;
                loop_check    = FALSE;
                done          = FALSE;
                do {
                    list_status = atomic_load(&(list_entry->status));

                    /**
                     * Wait to see if the thread attempting to close this list attempted 
                     * it before the list was created, meaning it failed and will update 
                     * the status back to IN_PROGRESS, and this thread can then update 
                     * status. Or if it did delete the list, then this thread will move 
                     * on to done since the closing thread would've updated the status 
                     * to DELETED.
                     */
                    while ( list_status == CLOSING_IN_PROGRESS )
                    {
                        sleep(1);

                        list_status = atomic_load(&(list_entry->status));
                    }

                    /* If the list was closed mark done */
                    if (atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG) {
                        assert(list_status == DELETED);
                        done = TRUE;
                    } 
                    else 
                    {
                        /* Attempt to atomically update the status */
                        if (!atomic_compare_exchange_strong(&(list_entry->status), &list_status,
                                                            update_status)) {
                            if (loop_check) {
                                assert(FALSE);
                            }
                            else {
                                loop_check = TRUE;
                            }
                        }
                        else {
                            list_status = atomic_load(&(list_entry->status));
                            assert(list_status == EXISTS);

                            done = TRUE;
                        }
                    }

                } while (!done);

                assert(op_info->result != NOT_ATTEMPTED);

            } /* end else ( list was created, update list_entry and op_info ) */

        } while (try_again);

    } /* end if ( create ) */

    assert(op_info->result != NOT_ATTEMPTED);


    return SUCCEED;

} /* end create_list() */

/****************************************************************************************
 * Function:    create_class
 *
 * Purpose:     Attempts to create a new property class.
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a class_entry 
 *              from the class_table is randomly chosen (if the class_entry is marked as 
 *              a copy it's iterated to a non-copy). If that class has been created 
 *              during this iteration of tests, it is searched for in the index, and the 
 *              results are set as necessary. 
 *               *  If the class has been created and is found in the index the result is
 *                  CLASS_ALREADY_EXISTS.
 *               *  If the class has been created but is not in the index, and the 
 *                  class_entry has a status of IN_PROGRESS, we loop and randomly grab a 
 *                  different class_entry to attempt creating.
 *               *  If the class has been created but is not in the index, and is not
 *                  IN_PROGRESS, it must be deleted. The status and the class struct tag
 *                  are checked to ensure it is deleted and the result is CLASS_DELETED.
 * 
 *               *  If the class hasnt been created, and has a status of DOESNT_EXIST, 
 *                  we atomically update the status to IN_PROGRESS and set the create
 *                  flag to TRUE to move on and attempt to create it. 
 *               *  If the class hasn't been created, and has a status of 
 *                  CLOSING_IN_PROGRESS, we loop and check the status of this class_entry
 *                  again. Since, the class hasn't been created the thread attempting to
 *                  close it will obviously fail and will update the status back to
 *                  DOESNT_EXIST.
 *               *  If the class hasn't been created, and has a status of IN_PROGRESS, we
 *                  loop and randomly grab a different class_entry to attempt creating.
 * 
 *              If the create flag is TRUE, the class_entry's necessary parent info is
 *              grabbed, and H5Pcreate_class() is called to attempt to create the class.
 *              
 *              If creating it failed, we search the index for the parent, which should 
 *              not be in the index, else creating the class should have succeeded. Then 
 *              we check the parent_entry from the class_table to see if the parent has
 *              ever been created.
 *               *  If the parent has been created, we check if it's been deleted and if
 *                  it has, the result is marked as PARENT_DELETED and the obj_ver is set
 *                  to the version the parent class was deleted at. 
 *               *  If the parent has been created, and not deleted, and the status is 
 *                  either EXISTS or IN_PROGRESS, the parent class was created just after
 *                  our attempt to create the class. Loop the create section to attempt
 *                  creating the class again. As long as the parent isn't deleted before 
 *                  this thread calls H5Pcreate_class() it should succeed this time.
 * 
 *               *  If the parent has NOT been created, we mark the result as 
 *                  PARENT_DOESNT_EXIST and then must atomically update the status of 
 *                  this class_entry back to DOESNT_EXIST
 * 
 *              If creating the class succeeded, we search the index for the class to 
 *              ensure it was correctly created and inserted, and update the class_entry
 *              with the class's hid, and a pointer to the class.
 *              Next all properties in the class are iterated and the corresponding 
 *              class_entry->prop_table entry (prop_entry) is grabbed to atomically 
 *              update the status from DOESNT_EXIST to EXISTS.
 *              After all properties in the class have had their respective prop_entry 
 *              statuses updated, the class_entry's status is atomically updated from
 *              IN_PROGRESS to EXISTS.  
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
create_class(thread_params_t *thread_params)
{
    class_table_entry_t *class_entry;
    class_table_entry_t *parent_entry;
    H5P_mt_class_sptr_t  class_sptr;
    H5P_mt_class_sptr_t  parent_sptr;
    status_t             class_status;
    status_t             update_status;
    status_t             parent_status;
    H5P_mt_class_t     *class       = NULL;
    H5P_mt_class_t     *check_class = NULL; /* A double check for the class obj */
    H5P_mt_class_t     *parent      = NULL;
    int                 r;                  /* Used to randomly select the class_entry */
    test_op_info_t     *op_info = NULL;
    uint32_t            op_num;
    bool                done           = FALSE;
    bool                loop_check     = FALSE; /** TODO: for debugging will remove */
    bool                create         = FALSE; /* Flag to attempt to create the class */
    bool                get_diff_class = FALSE;
    bool                try_again      = FALSE; /* Flag to try creating the class again */
    size_t              log_pl_len;
    prop_table_entry_t *prop_entry = NULL;
    status_t            prop_status;
    status_t            update_prop_status;
    hid_t               class_id;
    hid_t               parent_id;
    H5P_mt_prop_t      *prev_prop;
    H5P_mt_prop_t      *valid_prop;
    uint64_t            version;        /* Version of the class */
    uint64_t            check_ver;      /* A double check of the class's version */
    uint64_t            ver_del;
    uint64_t            visited    = 0;
    uint64_t            loop_count = 0; /* Times looped waiting for another thread */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = CREATE;
    op_info->op_num = op_num;

    /**
     * do-while get_diff_class is TRUE.
     * get_diff_class's default is FALSE, but gets flipped to 
     * TRUE if its status is IN_PROGRESS meaning another thread
     * is already attempting to create it. 
     */
    do {
        /* Get a random entry from the class table */
        r = rand() % CLASS_TABLE_SIZE;

        class_entry = &class_table[r];

        /* If the entry is marked to be a copy grab the next entry */
        while (class_entry->copy) {
            r++;
            class_entry = &class_table[r];
        }

        get_diff_class = FALSE;

        op_info->class_name  = class_entry->name;
        op_info->parent_name = class_entry->parent_name;
        op_info->test_id     = class_entry->test_class_id;

        done = FALSE;
        do {
            class_sptr = atomic_load(&(class_entry->class_sptr));
            class      = class_sptr.ptr;

            /* If not NULL the class has been created, perform some checks */
            if (class) {
                /* Check if the class is in the index */
                H5E_BEGIN_TRY
                {
                    check_class = (H5P_mt_class_t *)H5I_object(atomic_load(&(class->id)));
                }
                H5E_END_TRY

                class_status = atomic_load(&(class_entry->status));

                /* Ensure the returned check_class is the same as class */
                if (check_class) {
                    assert(class == check_class);

                    /* Check the status of the class_entry */
                    assert(class_status != DOESNT_EXIST);

                    /* Update op_info */
                    op_info->class   = class;
                    op_info->id      = atomic_load(&(class->id));
                    op_info->obj_ver = atomic_load(&(class->curr_version));
                    op_info->result  = CLASS_ALREADY_EXISTS;

                    done = TRUE;
                }
                else /* If class isn't in the index */
                {
                    /* Check the status of the class_entry */
                    if (class_status == IN_PROGRESS) {

                        /** 
                         * If status is IN_PROGRESS grab a different class to 
                         * avoid creating a duplicate class that isn't tracked.
                         */
                        get_diff_class = TRUE;
                        done           = TRUE;                    
                    }
                    else {
                        /**
                         * If class isn't in the index, not IN_PROGRESS, and has 
                         * been created it must have been DELETED. Double check.
                         */
                        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);
                        assert(class_status == DELETED || class_status == CLOSING_IN_PROGRESS ||
                               class_status == EXISTS_BUT_CLOSED);
                        op_info->class   = class;
                        op_info->id      = atomic_load(&(class->id));
                        op_info->obj_ver = atomic_load(&(class->curr_version));
                        op_info->result  = CLASS_DELETED;

                        done = TRUE;
                    }

                } /* end else ( ! check_class )*/

            } /* end if ( class ) */
            /* else the class hasn't been created yet */
            else {
                /* Check class_status */
                class_status = atomic_load(&(class_entry->status));

                /**
                 * If the class hasn't been created yet and its status
                 * is CLOSING_IN_PROGRESS, the thread trying to close
                 * it will fail shortly, loop and try again.
                 */
                if (class_status == CLOSING_IN_PROGRESS) {
                    loop_count++;
                }
                else if (class_status == DOESNT_EXIST) {
                    update_status = IN_PROGRESS;

                    /* Attempt to atomically update class_status */
                    if (!atomic_compare_exchange_strong(&(class_entry->status), &class_status,
                                                        update_status)) {
                        if (loop_check) {
                            assert(FALSE);
                        }
                        else {
                            loop_check = TRUE;
                        }
                    }
                    else {
                        /* Atomic update successful */
                        class_status = atomic_load(&(class_entry->status));
                        assert(class_status == IN_PROGRESS);

                        create = TRUE;
                        done   = TRUE;
                    }

                } /* end else if ( class_status == DOESNT_EXIST ) */
                /**
                 * Another thread is creating this class,
                 * loop and randomly a different class.
                 */
                else if (class_status == IN_PROGRESS) {
                    get_diff_class = TRUE;
                    done           = TRUE;
                }
                else {
                    /* No other status should be possible */
                    assert(FALSE);
                }

            } /* end else ( ! class ) */

        } while (!done);

    } while (get_diff_class);

    assert(done);
    assert(!get_diff_class);

    assert(class_status == IN_PROGRESS || (create == FALSE && 
            op_info->result != NOT_ATTEMPTED));

    atomic_fetch_add(&(class_entry->op_count), 1);

    /* Set back to FALSE for next do-while that uses done */
    done = FALSE;

    /* If TRUE attempt to create the class */
    if (create) {
        /**
         * do-while try_again is TRUE. try_again is set to FALSE, and is only flipped to 
         * TRUE if the attempt to create the class fails, and the parent class has a 
         * status of EXISTS. This occurs because at the time of the attempt to create the
         * class the parent wasn't in the index yet, but another thread was in the 
         * process of creating it and has now finished. So, another attempt to create the
         * class should succeed.
         */
        do {
            try_again = FALSE;

            /* Grab the parent info */
            parent_entry = atomic_load(&(class_entry->parent_entry));

            /* If parent_entry is NULL, then test_root is the parent */
            if (parent_entry) {
                parent_id = atomic_load(&(parent_entry->id));
            }
            else {
                parent_id = TEST_ROOT_ID_g;
            }
            atomic_store(&(class_entry->parent_id), parent_id);

            /* Double check class pointer and class status */
            class_sptr = atomic_load(&(class_entry->class_sptr));
            class      = class_sptr.ptr;

            assert(!class);

            class_status = atomic_load(&(class_entry->status));
            assert(class_status == IN_PROGRESS || class_status == CLOSING_IN_PROGRESS);

            /* Attempt to create the class */
            H5E_BEGIN_TRY
            {
                class_id = H5Pcreate_class(atomic_load(&(class_entry->parent_id)), 
                                            class_entry->name, 
                                            NULL, NULL, NULL, NULL, NULL, NULL);
            }
            H5E_END_TRY

            /* If creating the class failed, find why */
            if (class_id == H5I_INVALID_HID) {
                H5E_BEGIN_TRY
                {
                    parent = (H5P_mt_class_t *)H5I_object(parent_id);
                }
                H5E_END_TRY

                /* If parent is in index, creating the class shouldn't have failed */
                CHECK_PTR_NULL(parent, "create_class: H5I_object");
                assert(!parent);

                parent_status = atomic_load(&(parent_entry->status));
                parent_sptr = atomic_load(&(parent_entry->class_sptr));
                parent      = parent_sptr.ptr;

                /* If the parent has been created */
                if ( parent )
                {
                    /** 
                     * Check if its been deleted.
                     * NOTE: EXISTS_BUT_CLOSED and CLOSING_IN_PROGRESS statuses here are 
                     * treated as DELETED, but the thread performing the deleting hasn't 
                     * updated its status yet. Otherwise it would've be in the index, and 
                     * the class would've been created
                     */
                    if (parent_status == DELETED || 
                            parent_status == CLOSING_IN_PROGRESS ||
                            parent_status == EXISTS_BUT_CLOSED) {
                        
                        /**
                         * If the version the parent was deleted at is still 0, loop and
                         * check again. We wait for it to not be 0, so we have an 
                         * accurate timeline of operations during check_operations().
                         */
                        ver_del = atomic_load(&(parent_entry->ver_deleted));

                        while ( ver_del == 0 )
                        {
                            sleep(1);

                            ver_del = atomic_load(&((parent_entry->ver_deleted)));
                        }

                        assert(ver_del > 0);
                        assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_INVALID_TAG);

                        op_info->obj_ver = ver_del;
                        op_info->result  = PARENT_DELETED;
                    }
                    /**
                     * If the parent has been created, and hasn't been deleted, then
                     * when attempting to create the class, the parent was also in 
                     * the process of being created, but wasn't in the index yet.
                     * Try creating the class again, it should succeed.
                     */
                    else {
                        assert(parent_status == EXISTS || parent_status == IN_PROGRESS);

                        try_again = TRUE;
                    }
                }
                /* If the parent hasn't been created yet, mark result as such */
                else
                {
                    assert(parent_status == DOESNT_EXIST || parent_status == IN_PROGRESS ||
                            parent_status == CLOSING_IN_PROGRESS);
                    
                    op_info->result = PARENT_DOESNT_EXIST;
                }

                /* If try_again == FALSE, then the class_entry's status needs reset */
                if (!try_again) {
                    /* Set class status back to DOESNT_EXIST */
                    update_status = DOESNT_EXIST;
                    done          = FALSE;
                    do {
                        class_status = atomic_load(&(class_entry->status));

                        /**
                         * If CLOSING_IN_PROGRESS but the class hasn't been created 
                         * yet, the thread attempting the close, will fail and reset 
                         * the status back to IN_PROGRESS, wait until that is done 
                         * then try atomically updating status.
                         */
                        if (class_status == CLOSING_IN_PROGRESS) {
                            while (class_status == CLOSING_IN_PROGRESS) {
                                sleep(1);

                                class_status = atomic_load(&(class_entry->status));
                            }
                        }

                        /* If IN_PROGRESS atomically update status back to DOESNT_EXIST */
                        if (class_status == IN_PROGRESS) {
                            if (!atomic_compare_exchange_strong(&(class_entry->status), 
                                                        &class_status, update_status)) {
                                if (loop_check) {
                                    assert(FALSE);
                                }
                                else {
                                    loop_check = TRUE;
                                }
                            }
                            else {
                                /* Atomic update successful */
                                class_status = atomic_load(&(class_entry->status));
                                assert(class_status == DOESNT_EXIST);

                                done = TRUE;
                            }
                        }
                        else if (class_status != CLOSING_IN_PROGRESS) {
                            /**
                             * If the status is CLOSING_IN_PROGRESS, another thread 
                             * updated the status to it immediately after this thread 
                             * checked for that status. So, if that happens this will 
                             * loop and then see the status is CLOSING_IN_PROGRESS and 
                             * will wait for it to be changed. But the status 
                             * shouldn't be possible to be anything else we haven't 
                             * checked already.
                             */
                            assert(FALSE);
                        }

                    } while (!done);
                } /* end if ( ! try_again ) */

            }    /* end if ( class_id == H5I_INVALID_HID ) */
            else 
            {
                /* Class was created, update class_entry and op_info */

                atomic_store(&(class_entry->id), class_id);
                atomic_store(&(class_entry->parent_id), parent_id);

                class = (H5P_mt_class_t *)H5I_object(class_id);
                assert(class);
                assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);

                class_sptr.ptr = class;
                class_sptr.sn  = class_sptr.sn + 1;

                atomic_store(&(class_entry->class_sptr), class_sptr);

                /* Update class_entry's prop_table */
                version = atomic_load(&(class->curr_version));
                assert(version == 1);

                log_pl_len = atomic_load(&(class->log_pl_len));

                /* If not greater than 0, there are no properties to update */
                if (log_pl_len > 0) {
                    prev_prop = class->pl_head;
                    assert(prev_prop);
                    assert(atomic_load(&(prev_prop->tag)) == H5P_MT_PROP_TAG);
                    assert(prev_prop->sentinel);

                    /* Iterate the LFSLL and update prop statuses accordingly */
                    do {
                        /* Get the next valid property in the LFSLL */
                        valid_prop = H5P__get_next_valid_prop(prev_prop, version, &visited);

                        /* If there is another valid property */
                        if (valid_prop) {
                            assert(atomic_load(&(valid_prop->tag)) == H5P_MT_PROP_TAG);
                            assert(atomic_load(&(valid_prop->create_version)) == 1);

                            /* Get the entry for that property in the prop_table */
                            prop_entry =
                                search_prop_table(class_entry->prop_table, 
                                                  class_entry->num_prop_entries,
                                                  valid_prop->chksum, valid_prop->name);

                            CHECK_PTR(prop_entry, "search_prop_table");
                            assert(prop_entry);

                            /* Attempt to atomically update the status of the prop_entry */
                            loop_check = FALSE;
                            done       = FALSE;
                            do {
                                prop_status = atomic_load(&(prop_entry->status));

                                if (prop_status == DOESNT_EXIST) {
                                    update_prop_status = EXISTS;

                                    if (!atomic_compare_exchange_strong(&(prop_entry->status), 
                                                            &prop_status, update_prop_status)) {
                                        /**
                                         * TODO: For now we only try again once if cas fails
                                         */
                                        if (loop_check) {
                                            assert(FALSE);
                                        }
                                        else {
                                            loop_check = TRUE;
                                        }
                                    }
                                    else {
                                        done = TRUE;
                                    }

                                } /* end if ( prop_status == DOESNT_EXIST ) */
                                else {

                                    /**
                                     * If the prop_status is not DOESNT_EXIST and the 
                                     * class's version has been updated, then the prop 
                                     * was deleted by another thread (only worry about 
                                     * deleting props, because creating new ones waits 
                                     * till class status is EXISTS and modifying props 
                                     * doesn't update prop status). We don't need to 
                                     * update the prop_status, because the deleting 
                                     * thread has, or will, do that.
                                     */
                                    check_ver = atomic_load(&(class->curr_version));

                                    if (check_ver > version) {
                                        done = TRUE;
                                    }
                                    else {
                                        assert(FALSE);
                                    }
                                }

                            } while (!done);

                            prev_prop = valid_prop;

                        } /* end if ( valid_prop ) */

                    } while (valid_prop);

                } /* end if ( log_pl_len > 0 ) */

                /* Update op_info */
                op_info->class   = class;
                op_info->id      = class_id;
                op_info->obj_ver = version;
                op_info->result  = OP_SUCCESS;

                /* Update class status to EXISTS */
                update_status = EXISTS;
                loop_check    = FALSE;
                done          = FALSE;
                do {
                    class_status = atomic_load(&(class_entry->status));

                    /**
                     * Wait to see if the thread attempting to close this class
                     * attempted it before the class was created, meaning it 
                     * failed and then this thread will update status. Or if
                     * it did delete the class, then this thread will move on
                     * to done since the closing thread would've updated the 
                     * status to DELETED or EXISTS_BUT_CLOSED.
                     */
                    while ( class_status == CLOSING_IN_PROGRESS )
                    {
                        sleep(1);

                        class_status = atomic_load(&(class_entry->status));
                    }

                    class_status = atomic_load(&(class_entry->status));

                    /* If the class was closed mark done */
                    if ( class_status == DELETED || class_status == EXISTS_BUT_CLOSED ) {

                        done = TRUE;
                    }
                    else
                    {
                        assert(class_status == IN_PROGRESS);

                        /* Attempt to atomically update the status */
                        if (!atomic_compare_exchange_strong(&(class_entry->status), &class_status,
                                                            update_status)) {
                            if (loop_check) {
                                assert(FALSE);
                            }
                            else {
                                loop_check = TRUE;
                            }
                        }
                        else {
                            class_status = atomic_load(&(class_entry->status));
                            assert(class_status == EXISTS);

                            done = TRUE;
                        }
                    }

                } while (!done);

                assert(op_info->result != NOT_ATTEMPTED);

            } /* end else ( class was created, update class_entry and op_info ) */

        } while (try_again);

    } /* end if ( create ) */

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end create_class() */

/****************************************************************************************
 * Function:    copy_list
 *
 * Purpose:     Attempts to create a copy of an existing property list.
 *
 * Details:     This function is almost identical to create_list() so this will just 
 *              explain what is different. For more details see create_list().
 * 
 *              The first difference is how the random list is obtained from the 
 *              list_table. There are only a small subset of entries from the list_table
 *              that are setup to be copies of another list. So, the random entry must be
 *              grabbed from that small subset.
 * 
 *              The only other real difference is instead of investigating if the parent
 *              class was created or deleted, copy_list() investigates the original list 
 *              trying to be copied. So, if creating the copy fails, the original list is 
 *              checked if it's been created yet or if it has been deleted. 
 * 
 *              NOTE: og is used as an abbreviation for original.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
copy_list(thread_params_t *thread_params)
{
    list_table_entry_t  *list_entry;    /* The entry for the copy of the list */
    list_table_entry_t  *og_list_entry; /* The entry for the list to be copied */
    class_table_entry_t *parent_entry;
    H5P_mt_list_sptr_t   list_sptr;
    H5P_mt_list_sptr_t   og_list_sptr;
    status_t             list_status;
    status_t             update_status;
    status_t             og_list_status;    /* Status of the original list */
    H5P_mt_list_t       *list       = NULL; /* Pointer to copy of the original list */
    H5P_mt_list_t       *og_list    = NULL; /* Pointer to the original list */
    H5P_mt_list_t       *check_list = NULL; /* A double check for the list obj */
    int                  r;                 /* Used to randomly select the list_entry */
    int                  loop_count = 0;
    int                  index;             /* Index in the list_table */
    int                  num_list_copies = 4; /* Num of lists that can be copied */
    test_op_info_t      *op_info         = NULL;
    uint32_t             op_num;
    bool                 done          = FALSE;
    bool                 loop_check    = FALSE; /** TODO: Used for debugging will remove */
    bool                 create        = FALSE;
    bool                 get_diff_list = FALSE;
    bool                 base_flag     = FALSE; /* See H5P__mt_entry_find_version's description */
    bool                 skip          = FALSE; /* Flag used for updating prop status */
    bool                 try_again     = FALSE; /* Used for a do-while loop */
    size_t               nprops_inherited;
    size_t               nprops;
    H5P_mt_list_table_entry_t *lkup_tbl_entry;
    prop_table_entry_t  *prop_entry = NULL;
    status_t             prop_status;
    status_t             update_prop_status;
    hid_t                list_id;       /* hid of the list */
    hid_t                og_list_id;    /* hid of the original list */
    hid_t                parent_id;     /* hid of the parent class */
    H5P_mt_prop_t       *valid_prop = NULL;
    H5P_mt_prop_t       *prev_prop  = NULL;
    uint64_t             version;       /* Version of the list */ 
    uint64_t             check_ver;     /* Used for checking list's version */
    uint64_t             ver_del;       /* Version the og list was deleted */
    uint64_t             og_version;    /* Version of the original list */
    uint64_t             visited = 0;   /* See H5P__get_next_valid_prop's description */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = COPY;
    op_info->op_num = op_num;

    /**
     * do-while get_diff_list is TRUE.
     * get_diff_list's default is FALSE, but gets flipped to 
     * TRUE if its status is IN_PROGRESS meaning another thread
     * is already attempting to create it. 
     */
    do {
        /* Get a random entry from the list table that can be a copy */
        r = rand() % num_list_copies;

        for (index = 0; index < LIST_TABLE_SIZE; index++) {
            list_entry = &list_table[index];

            if (list_entry->copy) {
                if (r > 0) {
                    r--;
                }

                if (r == 0) {
                    break;
                }
            }
        }

        get_diff_list = FALSE;

        op_info->parent_name  = list_entry->parent_name;
        op_info->test_id      = list_entry->test_list_id;
        op_info->obj_isa_copy = TRUE;

        done = FALSE;
        do {
            list_sptr = atomic_load(&(list_entry->list_sptr));
            list      = list_sptr.ptr;

            /* If not NULL the list has been created, perform some checks */
            if (list) {
                /* Check if the list is in the index */
                H5E_BEGIN_TRY
                {
                    check_list = (H5P_mt_list_t *)H5I_object(atomic_load(&(list->plist_id)));
                }
                H5E_END_TRY

                list_status = atomic_load(&(list_entry->status));

                /* Ensure the returned check_list is the same as list */
                if (check_list) {
                    assert(list == check_list);

                    /* Check the status of the list_entry */
                    assert(list_status != DOESNT_EXIST);

                    /* Update op_info */
                    op_info->list    = list;
                    op_info->id      = atomic_load(&(list->plist_id));
                    op_info->obj_ver = atomic_load(&(list->curr_version));
                    op_info->result  = LIST_ALREADY_EXISTS;

                    done = TRUE;
                }
                else /* If list isn't in the index */
                {
                    /* Check the status of the list_entry */
                    if (list_status == IN_PROGRESS) {
                        
                        /* If status is IN_PROGRESS grab a different list */
                        get_diff_list = TRUE;
                        done          = TRUE;
                    }
                    else {
                        /**
                         * If list isn't in the index, not IN_PROGRESS, and has 
                         * been created it must have been DELETED. Double check.
                         */
                        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);
                        assert(list_status == DELETED || list_status == CLOSING_IN_PROGRESS);
                        op_info->list    = list;
                        op_info->id      = atomic_load(&(list->plist_id));
                        op_info->obj_ver = atomic_load(&(list->curr_version));
                        op_info->result  = LIST_DELETED;

                        done = TRUE;
                    }

                } /* end else ( ! check_list ) */

            } /* end if ( list ) */
            /* else the list hasn't been created yet */
            else {
                /* Check list_status */
                list_status = atomic_load(&(list_entry->status));

                if (list_status == DOESNT_EXIST) {
                    update_status = IN_PROGRESS;

                    /* Attempt to atomically update list_status */
                    if (!atomic_compare_exchange_strong(&(list_entry->status), &list_status, update_status)) {
                        /* Atomic update failed, for now just try again once */
                        if (loop_check) {
                            assert(FALSE);
                        }
                        else {
                            loop_check = TRUE;
                        }
                    }
                    else {
                        /* Atomic update successful */
                        list_status = atomic_load(&(list_entry->status));
                        assert(list_status == IN_PROGRESS);                        
                        
                        create = TRUE;
                        done   = TRUE;
                    }

                } /* end if ( list_status == DOESNT_EXIST ) */
                else if (list_status == CLOSING_IN_PROGRESS) {
                    /**
                     * If the list hasn't been created yet and its status
                     * is CLOSING_IN_PROGRESS, the thread trying to close
                     * it will fail. Loop and check again.
                     */
                    loop_count++;
                }
                /**
                 * Another thread is creating this list,
                 * loop and randomly grab a different list.
                 */
                else if (list_status == IN_PROGRESS) {
                    get_diff_list = TRUE;
                    done          = TRUE;
                }
                else {
                    /* No other status should be possible */
                    assert(FALSE);
                }

            } /* end else ( ! list ) */

        } while (!done);

    } while (get_diff_list);

    assert(done);
    assert(!get_diff_list);

    assert(list_status == IN_PROGRESS || ( create == FALSE &&
            op_info->result != NOT_ATTEMPTED));

    atomic_fetch_add(&(list_entry->op_count), 1);

    /* Set back to FALSE for next do-while that uses done */
    done = FALSE;

    /* If TRUE attempt to create the list */
    if (create) {
        /**
         * do-while try_again is TRUE. try_again is set to FALSE, and is only flipped to 
         * TRUE if the attempt to create the list fails, and the original list has a 
         * status of EXISTS. This occurs because at the time of the attempt to create the
         * list the original wasn't in the index yet, but another thread was in the process 
         * of creating it and has now finished. So, another attempt to create the list 
         * should succeed.
         */
        do {
            try_again = FALSE;

            /* Grab the original list to copy */

            /**
             * NOTE: Subtract 101 because lists use a test_list_id starting 
             * at 101 to differentiate between classes which start at 1.
             */
            int og_index = list_entry->og_id - 101; 
            og_list_entry = &list_table[og_index];

            og_list_id = atomic_load(&(og_list_entry->id));

            /* Grab the parent info */
            parent_entry = atomic_load(&(list_entry->parent_entry));
            parent_id    = atomic_load(&(parent_entry->id));
            atomic_store(&(list_entry->parent_id), parent_id);

            /* Double check list pointer and list status */
            list_sptr = atomic_load(&(list_entry->list_sptr));
            list      = list_sptr.ptr;

            assert(!list);

            og_list_sptr = atomic_load(&(og_list_entry->list_sptr));
            og_list      = og_list_sptr.ptr;

            list_status = atomic_load(&(list_entry->status));
            assert(list_status == IN_PROGRESS || CLOSING_IN_PROGRESS );

            /* Attempt to create a copy of the list */
            H5E_BEGIN_TRY
            {
                /* Grab og_list's version */
                if (og_list) {
                    og_version = atomic_load(&(og_list->curr_version));
                }

                list_id = H5Pcopy(og_list_id);
            }
            H5E_END_TRY

            /* If creating the copy failed, find why */
            if (list_id == H5I_INVALID_HID) {
                H5E_BEGIN_TRY
                {
                    og_list = (H5P_mt_list_t *)H5I_object(og_list_id);
                }
                H5E_END_TRY

                /* If og_list is in index, copying the list shouldn't haved failed */
                CHECK_PTR_NULL(og_list, "copy_list: H5I_object");
                assert(!og_list);

                og_list_status = atomic_load(&(og_list_entry->status));
                og_list_sptr   = atomic_load(&(og_list_entry->list_sptr));
                og_list        = og_list_sptr.ptr;

                /* If the original list has been created */
                if (og_list) {

                    /* Check if its been deleted */
                    if ( og_list_status == DELETED || 
                            og_list_status == CLOSING_IN_PROGRESS ) {

                        /**
                         * If the version the og_list was deleted at is still 0, loop and
                         * check again. We wait for it to not be 0, so we have an 
                         * accurate timeline of operations during check_operations().
                         */
                        ver_del = atomic_load(&(og_list_entry->ver_deleted));

                        while ( ver_del == 0 )
                        {
                            sleep(1);

                            ver_del = atomic_load(&((og_list_entry->ver_deleted)));
                        }
                        
                        assert(atomic_load(&(og_list->tag)) == H5P_MT_LIST_INVALID_TAG);

                        op_info->obj_ver = atomic_load(&(og_list_entry->ver_deleted));
                        op_info->result  = OG_DELETED;
                    }
                    /**
                     * If the original has been created, and hasn't been deleted, then
                     * when attempting to create a copy, the original was also in the 
                     * process of being created, but wasn't in the index yet.
                     * Try copying the list again, it should succeed.
                     */
                    else {
                        assert(og_list_status == EXISTS || og_list_status == IN_PROGRESS);

                        try_again = TRUE;
                    }
                }
                else {
                    assert(og_list_status == DOESNT_EXIST || og_list_status == IN_PROGRESS ||
                           og_list_status == CLOSING_IN_PROGRESS);
                    op_info->result = OG_DOESNT_EXIST;
                }

                /* If try_again == FALSE, then the list_entry's status needs reset */
                if (!try_again) {
                    /* Set list status back to DOESNT_EXIST */
                    update_status = DOESNT_EXIST;

                    done = FALSE;
                    do {
                        list_status = atomic_load(&(list_entry->status));

                        /**
                         * If CLOSING_IN_PROGRESS but the list hasn't been created 
                         * yet, the thread attempting the close, will fail and reset 
                         * the status back to IN_PROGRESS, wait until that is done 
                         * then try atomically updating status.
                         */
                        while (list_status == CLOSING_IN_PROGRESS) {
                            sleep(1);

                            list_status = atomic_load(&(list_entry->status));
                        }

                        /* If IN_PROGRESS atomically update status back to DOESNT_EXIST */
                        if (list_status == IN_PROGRESS) {
                            if (!atomic_compare_exchange_strong(&(list_entry->status), 
                                                        &list_status, update_status)) {
                                if (loop_check) {
                                    assert(FALSE);
                                }
                                else {
                                    loop_check = TRUE;
                                }
                            }
                            else {
                                /* Atomic update successful */
                                list_status = atomic_load(&(list_entry->status));
                                assert(list_status == DOESNT_EXIST);

                                done = TRUE;
                            }
                        }
                        else if ( list_status != CLOSING_IN_PROGRESS ) {
                            /**
                             * If the status is CLOSING_IN_PROGRESS, another thread 
                             * updated the status to it immediately after this thread 
                             * checked for that status. So, if that happens this will 
                             * loop and then see the status is CLOSING_IN_PROGRESS and 
                             * will wait for it to be changed. But the status 
                             * shouldn't be possible to be anything else we haven't 
                             * checked already.
                             */
                            assert(FALSE);
                        }

                    } while (!done);

                } /* end if ( ! try_again ) */

                assert(op_info->result != NOT_ATTEMPTED || try_again);

            }    /* end if ( list_id == H5I_INVALID_HID ) */
            else 
            {
                /* List was created, update list_entry and op_info */

                atomic_store(&(list_entry->id), list_id);
                atomic_store(&(list_entry->parent_id), parent_id);

                assert(atomic_load(&(list_entry->id)) != H5I_INVALID_HID);

                list = (H5P_mt_list_t *)H5I_object(list_id);
                assert(list);
                assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

                list_sptr.ptr = list;
                list_sptr.sn  = list_sptr.sn + 1;

                atomic_store(&(list_entry->list_sptr), list_sptr);

                /* Update list_entry with the version it was copied from */
                atomic_store(&(list_entry->ver_copied), og_version);

                /* Update prop table */
                version = atomic_load(&(list->curr_version));
                assert(version = 1);

                nprops_inherited = list->nprops_inherited;
                nprops           = atomic_load(&(list->nprops));

                prev_prop = list->pl_head;
                assert(prev_prop);
                assert(atomic_load(&(prev_prop->tag)) == H5P_MT_PROP_TAG);
                assert(prev_prop->sentinel);

                size_t i = 0;
                /* Iterate the lkup_tbl and LFSLL and update accordingly */
                do {
                    /* Iterate the lkup_tbl first */
                    if (i < nprops_inherited) {
                        lkup_tbl_entry = &list->lkup_tbl[i];

                        valid_prop = H5P__mt_entry_find_version(lkup_tbl_entry, version, &base_flag);

                        i++;
                    }
                    else if ( i >= nprops_inherited && i < nprops )
                    {
                        valid_prop = H5P__get_next_valid_prop(prev_prop, version, &visited);

                        i++;

                        if (valid_prop) {
                            /* If in_lkup_tbl it was already updated */
                            if (valid_prop->in_lkup_tbl) {
                                skip = TRUE;
                            }
                            else {
                                skip = FALSE;
                            }
                        }
                        else {
                            assert(i == nprops);
                        }
                    }
                    else {
                        assert(i == nprops);
                        valid_prop = NULL;
                    }

                    /* If there is another valid property */
                    if (valid_prop && (skip == FALSE)) {
                        assert(atomic_load(&(valid_prop->tag)) == H5P_MT_PROP_TAG);

                        /* Grab the respective prop_entry for the property */
                        prop_entry = search_prop_table(list_entry->prop_table, list_entry->num_prop_entries,
                                                       valid_prop->chksum, valid_prop->name);
                        CHECK_PTR(prop_entry, "search_prop_table");
                        assert(prop_entry);

                        /* Attempt to atomically update the status of the prop_entry */
                        loop_check = FALSE;
                        done       = FALSE;
                        do {
                            prop_status = atomic_load(&(prop_entry->status));

                            if (prop_status == DOESNT_EXIST) {
                                update_prop_status = EXISTS;

                                if (!atomic_compare_exchange_strong(&(prop_entry->status), &prop_status,
                                                                    update_prop_status)) {
                                    /**
                                     * TODO: For now we only try again once if cas fails
                                     */
                                    if (loop_check) {
                                        assert(FALSE);
                                    }
                                    else {
                                        loop_check = TRUE;
                                    }
                                }
                                else {
                                    done = TRUE;
                                }

                            } /* end if ( prop_status == DOESNT_EXIST ) */
                            else {

                                /**
                                 * If the prop_status is not DOESNT_EXIST and the lists's
                                 * version has been updated, then the prop was deleted by 
                                 * another thread (only worry about deleting props, 
                                 * because creating new ones waits till list status is 
                                 * EXISTS and modifying props doesn't update prop 
                                 * status). We don't need to update the prop_status, 
                                 * because the deleting thread has, or will, do that.
                                 */
                                check_ver = atomic_load(&(list->curr_version));

                                if (check_ver > version) {
                                    done = TRUE;
                                }
                                else {
                                    assert(FALSE);
                                }
                            }

                        } while (!done);

                        prev_prop = valid_prop;

                    } /* end if ( valid_prop && ( skip == FALSE ) )*/

                } while ( valid_prop );

                /* Update op_info */
                op_info->list    = list;
                op_info->id      = list_id;
                op_info->obj_ver = version;
                op_info->result  = OP_SUCCESS;

                /* Update list status */
                update_status = EXISTS;
                loop_check    = FALSE;
                done          = FALSE;
                do {
                    list_status = atomic_load(&(list_entry->status));

                    /**
                     * Wait to see if the thread attempting to close this list attempted 
                     * it before the list was created, meaning it failed and will update 
                     * the status back to IN_PROGRESS, and this thread can then update 
                     * status. Or if it did delete the list, then this thread will move 
                     * on to done since the closing thread would've updated the status 
                     * to DELETED.
                     */
                    while ( list_status == CLOSING_IN_PROGRESS )
                    {
                        sleep(1);

                        list_status = atomic_load(&(list_entry->status));
                    }

                    /* If the list was closed mark done */
                    if (atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG) {
                        assert(list_status == DELETED);
                        done = TRUE;
                    } 
                    else 
                    {
                        /* Attempt to atomically update the status */
                        if (!atomic_compare_exchange_strong(&(list_entry->status), &list_status,
                                                            update_status)) {
                            if (loop_check) {
                                assert(FALSE);
                            }
                            else {
                                loop_check = TRUE;
                            }
                        }
                        else {
                            list_status = atomic_load(&(list_entry->status));
                            assert(list_status == EXISTS);

                            done = TRUE;
                        }
                    }

                } while (!done);

                assert(op_info->result != NOT_ATTEMPTED);

            } /* end else ( list was created, update list_entry and op_info ) */

        } while (try_again);

        assert(op_info->result != NOT_ATTEMPTED);

    } /* end if ( create ) */

    assert(op_info->result != NOT_ATTEMPTED);


    return SUCCEED;

} /* end copy_list() */

/****************************************************************************************
 * Function:    copy_class
 *
 * Purpose:     Attempts to create a copy of an existing property class.
 *
 * Details:     This function is almost identical to create_class() so this will just 
 *              explain what is different. For more details see create_class().
 * 
 *              The first difference is how the random class is obtained from the 
 *              class_table. There are only a small subset of entries from the class_table
 *              that are setup to be copies of another class. So, the random entry must
 *              grabbed from that small subset.
 * 
 *              The only other real difference is instead of investigating if the parent
 *              class was created or deleted, copy_class() investigates the original 
 *              class trying to be copied. So, if creating the copy fails, the original 
 *              class is checked if it's been created yet or if it has been deleted. 
 * 
 *              NOTE: og is used as an abbreviation for original.
 *
 *
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
copy_class(thread_params_t *thread_params)
{

    class_table_entry_t *class_entry;
    class_table_entry_t *og_class_entry;
    class_table_entry_t *parent_entry;
    H5P_mt_class_sptr_t  class_sptr;
    H5P_mt_class_sptr_t  og_class_sptr;
    status_t             class_status;
    status_t             update_status;
    status_t             og_class_status;
    H5P_mt_class_t     *class       = NULL; /* Pointer to the copy class obj */
    H5P_mt_class_t     *og_class    = NULL; /* Pointer to the original class obj */
    H5P_mt_class_t     *check_class = NULL;
    int                 r;                  /* Used to randomly select the class_entry */
    int                 index;              /* Index in the list_table */
    int                 num_class_copies = 5; /* Num of lists that can be copied */
    int                 loop_count       = 0;
    test_op_info_t     *op_info          = NULL;
    uint32_t            op_num;
    bool                done           = FALSE;
    bool                loop_check     = FALSE; /** TODO: Used for debugging will remove */
    bool                create         = FALSE;
    bool                get_diff_class = FALSE;
    bool                try_again      = FALSE; /* Used for a do-while loop */
    size_t              log_pl_len;
    prop_table_entry_t *prop_entry = NULL;
    status_t            prop_status;
    status_t            update_prop_status;
    hid_t               class_id;           /* hid of the class */
    hid_t               og_class_id;        /* hid of the original class */
    hid_t               parent_id;          /* hid of the parent class */
    H5P_mt_prop_t      *valid_prop = NULL;
    H5P_mt_prop_t      *prev_prop  = NULL;
    uint64_t            version;            /* Version of the class */
    uint64_t            check_ver;          /* Used for checking class's version */
    uint64_t            og_version;         /* Version of the original class */
    uint64_t            ver_del;
    uint64_t            visited = 0;  /* See H5P__get_next_valid_prop's description */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = COPY;
    op_info->op_num = op_num;

    /**
     * do-while get_diff_class is TRUE.
     * get_diff_class's default is FALSE, but gets flipped to 
     * TRUE if its status is IN_PROGRESS meaning another thread
     * is already attempting to create it. 
     */
    do {
        /* Get a random entry from the class table that can be a copy */
        r = rand() % num_class_copies;

        for (index = 0; index < LIST_TABLE_SIZE; index++) {
            class_entry = &class_table[index];

            if (class_entry->copy) {
                if (r > 0) {
                    r--;
                }

                if (r == 0) {
                    break;
                }
            }
        }

        get_diff_class = FALSE;

        op_info->class_name   = class_entry->name;
        op_info->parent_name  = class_entry->parent_name;
        op_info->test_id      = class_entry->test_class_id;
        op_info->obj_isa_copy = TRUE;

        done = FALSE;
        do {
            class_sptr = atomic_load(&(class_entry->class_sptr));
            class      = class_sptr.ptr;

            /* If not NULL the class has been created, perform some checks */
            if (class) {
                /* Check if the class is in the index */
                H5E_BEGIN_TRY
                {
                    check_class = (H5P_mt_class_t *)H5I_object(atomic_load(&(class->id)));
                }
                H5E_END_TRY

                class_status = atomic_load(&(class_entry->status));

                /* Ensure the returned check_class is the same as class */
                if (check_class) {
                    assert(class == check_class);

                    /* Check the status of the class_entry */
                    assert(class_status != DOESNT_EXIST);

                    /* Update op_info */
                    op_info->class   = class;
                    op_info->id      = atomic_load(&(class->id));
                    op_info->obj_ver = atomic_load(&(class->curr_version));
                    op_info->result  = CLASS_ALREADY_EXISTS;

                    done = TRUE;
                }
                else /* If class isn't in the index */
                {
                    /* Check the status of the class_entry */
                    if (class_status == IN_PROGRESS) {

                        /* If status is IN_PROGRESS grab a different class */
                        get_diff_class = TRUE;
                        done           = TRUE;    
                    }
                    else {
                        /**
                         * If class isn't in the index, not IN_PROGRESS, and has 
                         * been created it must have been DELETED. Double check.
                         */
                        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);
                        assert(class_status == DELETED || class_status == CLOSING_IN_PROGRESS ||
                                class_status == EXISTS_BUT_CLOSED);
                        op_info->class   = class;
                        op_info->id      = atomic_load(&(class->id));
                        op_info->obj_ver = atomic_load(&(class->curr_version));
                        op_info->result  = CLASS_DELETED;

                        done = TRUE;
                    }

                } /* end else ( ! check_class )*/

            } /* end if ( class ) */
            /* else the class hasn't been created yet */
            else {
                /* Check class_status */
                class_status = atomic_load(&(class_entry->status));

                /**
                 * If the class hasn't been created yet and its status
                 * is CLOSING_IN_PROGRESS, the thread trying to close
                 * it will fail shortly, loop and try again.
                 */                
                if (class_status == CLOSING_IN_PROGRESS) {
                    loop_count++;
                }
                else if (class_status == DOESNT_EXIST) {
                    update_status = IN_PROGRESS;

                    /* Attempt to atomically update class_status */
                    if (!atomic_compare_exchange_strong(&(class_entry->status), &class_status,
                                                        update_status)) {
                        if (loop_check) {
                            assert(FALSE);
                        }
                        else {
                            loop_check = TRUE;
                        }
                    }
                    else {
                        /* Atomic update successful */
                        class_status = atomic_load(&(class_entry->status));
                        assert(class_status == IN_PROGRESS);

                        create = TRUE;
                        done   = TRUE;
                    }

                } /* end else if ( class_status == DOESNT_EXIST ) */
                /**
                 * Another thread is creating this class,
                 * loop and randomly a different class.
                 */
                else if (class_status == IN_PROGRESS) {
                    get_diff_class = TRUE;
                    done           = TRUE;
                }
                else {
                    /* No other status should be possible */
                    assert(FALSE);
                }

            } /* end else ( ! class ) */

        } while (!done);

    } while (get_diff_class);

    assert(done);
    assert(!get_diff_class);

    assert(class_status == IN_PROGRESS || (create == FALSE && 
            op_info->result != NOT_ATTEMPTED));

    atomic_fetch_add(&(class_entry->op_count), 1);

    /* Set back to FALSE for next do-while that uses done */
    done = FALSE;

    /* If create == TRUE attempt to create a copy of a list */
    if (create) {
        /**
         * do-while try_again is TRUE. try_again is set to FALSE, and is only flipped to 
         * TRUE if the attempt to create the class fails, and the original class has a 
         * status of EXISTS. This occurs because at the time of the attempt to create the
         * class the original wasn't in the index yet, but another thread was in the 
         * process of creating it and has now finished. So, another attempt to create the
         * class should succeed.
         */
        do {
            try_again = FALSE;

            /* Grab the original class to copy */
            int og_index = class_entry->og_id - 1; /* Subtract 1 for index starting at 0 */
            og_class_entry = &class_table[og_index];

            og_class_id = atomic_load(&(og_class_entry->id));

            assert(0 == strcmp(atomic_load(&(og_class_entry->name)), 
                        atomic_load(&(class_entry->name))));

            parent_entry = atomic_load(&(class_entry->parent_entry));

            /* If parent_entry is NULL, then test_root is the parent */
            if (parent_entry) {
                parent_id = atomic_load(&(parent_entry->id));
            }
            else {
                parent_id = TEST_ROOT_ID_g;
            }
            atomic_store(&(class_entry->parent_id), parent_id);

            /* Double check class pointer and class status */
            class_sptr = atomic_load(&(class_entry->class_sptr));
            class      = class_sptr.ptr;

            assert(!class);

            og_class_sptr = atomic_load(&(og_class_entry->class_sptr));
            og_class      = og_class_sptr.ptr;

            class_status = atomic_load(&(class_entry->status));
            assert(class_status == IN_PROGRESS || class_status == CLOSING_IN_PROGRESS);

            /* Attempt to create a copy of the class */
            H5E_BEGIN_TRY
            {
                /* Grab og_class's version */
                if (og_class) {
                    og_version = atomic_load(&(og_class->curr_version));
                }

                class_id = H5Pcreate_class(atomic_load(&(class_entry->parent_id)), 
                                            class_entry->name, 
                                            NULL, NULL, NULL, NULL, NULL, NULL);
            }
            H5E_END_TRY

            /* If creating the copy failed, find why */
            if (class_id == H5I_INVALID_HID) {

                H5E_BEGIN_TRY
                {
                    og_class = (H5P_mt_class_t *)H5I_object(og_class_id);
                }
                H5E_END_TRY

                /* If original is in index, creating the class shouldn't have failed */
                CHECK_PTR_NULL(og_class, "copy_list: H5I_object");
                assert(!og_class);

                og_class_status = atomic_load(&(og_class_entry->status));
                og_class_sptr = atomic_load(&(og_class_entry->class_sptr));
                og_class      = og_class_sptr.ptr;

                /* If the og_class has been created */
                if ( og_class )
                {
                    /** 
                     * Check if its been deleted.
                     * NOTE: EXISTS_BUT_CLOSED and CLOSING_IN_PROGRESS statuses here are 
                     * treated as DELETED, but the thread performing the deleting hasn't 
                     * updated its status yet. Otherwise it would've be in the index, and 
                     * the class would've been created
                     */                  
                    if (og_class_status == DELETED || 
                            og_class_status == EXISTS_BUT_CLOSED ||
                            og_class_status == CLOSING_IN_PROGRESS ) {
                        /**
                         * If the version the original was deleted at is still 0, loop 
                         * and check again. We wait for it to not be 0, so we have an 
                         * accurate timeline of operations during check_operations().
                         */
                        ver_del = atomic_load(&(og_class_entry->ver_deleted));

                        while ( ver_del == 0 )
                        {
                            sleep(1);

                            ver_del = atomic_load(&(og_class_entry->ver_deleted));
                        }
                        assert(atomic_load(&(og_class->tag)) == H5P_MT_CLASS_INVALID_TAG);

                        op_info->obj_ver = atomic_load(&(og_class_entry->ver_deleted));
                        op_info->result  = OG_DELETED;
                    }
                    /**
                     * If the og_class has been created, and hasn't been deleted, then
                     * when attempting to create the class, the og_class was also in 
                     * the process of being created, but wasn't in the index yet.
                     * Try copying the class again, it should succeed.
                     */
                    else {
                        assert(og_class_status == EXISTS || og_class_status == IN_PROGRESS);

                        try_again = TRUE;
                    }
                }
                /* If the original hasn't been created yet, mark result as such */
                else
                {
                    assert(og_class_status == DOESNT_EXIST || og_class_status == IN_PROGRESS ||
                            og_class_status == CLOSING_IN_PROGRESS);
                    
                    op_info->result = OG_DOESNT_EXIST;
                }

                /* If try_again == FALSE, then the class_entry's status needs reset */
                if (!try_again) {
                    /* Set class status back to DOESNT_EXIST */
                    update_status = DOESNT_EXIST;
                    done = FALSE;
                    do {
                        class_status = atomic_load(&(class_entry->status));

                        /**
                         * If CLOSING_IN_PROGRESS but the class hasn't been created 
                         * yet, the thread attempting the close, will fail and reset 
                         * the status back to IN_PROGRESS, wait until that is done 
                         * then try atomically updating status.
                         */
                        while (class_status == CLOSING_IN_PROGRESS) {
                            sleep(1);

                            class_status = atomic_load(&(class_entry->status));
                        }

                        /* If IN_PROGRESS atomically update status back to DOESNT_EXIST */
                        if (class_status == IN_PROGRESS) {
                            if (!atomic_compare_exchange_strong(&(class_entry->status), 
                                                        &class_status, update_status)) {
                                if (loop_check) {
                                    assert(FALSE);
                                }
                                else {
                                    loop_check = TRUE;
                                }
                            }
                            else {
                                /* Atomic update successful */
                                class_status = atomic_load(&(class_entry->status));
                                assert(class_status == DOESNT_EXIST);

                                done = TRUE;
                            }
                        }
                        else if (class_status != CLOSING_IN_PROGRESS) {
                            /**
                             * If the status is CLOSING_IN_PROGRESS, another thread 
                             * updated the status to it immediately after this thread 
                             * checked for that status. So, if that happens this will 
                             * loop and then see the status is CLOSING_IN_PROGRESS and 
                             * will wait for it to be changed. But the status 
                             * shouldn't be possible to be anything else we haven't 
                             * checked already.
                             */
                            assert(FALSE);
                        }

                    } while (!done);

                } /* end if ( ! try_again ) */

            }    /* end if ( class_id == H5I_INVALID_HID ) */
            else 
            {
                /* Class was created, update class_entry and op_info */

                atomic_store(&(class_entry->id), class_id);
                atomic_store(&(class_entry->parent_id), parent_id);

                class = (H5P_mt_class_t *)H5I_object(class_id);
                assert(class);
                assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);

                class_sptr.ptr = class;
                class_sptr.sn  = class_sptr.sn + 1;

                atomic_store(&(class_entry->class_sptr), class_sptr);

                /* Update class_entry with the version it was copied from */
                atomic_store(&(class_entry->ver_copied), og_version);

                /* Update prop_table */
                version = atomic_load(&(class->curr_version));
                assert(version == 1);

                log_pl_len = atomic_load(&(class->log_pl_len));

                /* If not greater than 0, there are no properties to update */
                if (log_pl_len > 0) {
                    prev_prop = class->pl_head;
                    assert(prev_prop);
                    assert(atomic_load(&(prev_prop->tag)) == H5P_MT_PROP_TAG);
                    assert(prev_prop->sentinel);

                    /* Iterate the LFSLL and update prop statuses accordingly */
                    do {
                        /* Get the next valid property in the LFSLL */
                        valid_prop = H5P__get_next_valid_prop(prev_prop, version, &visited);

                        /* If there is another valid property */
                        if (valid_prop) {
                            assert(atomic_load(&(valid_prop->tag)) == H5P_MT_PROP_TAG);
                            assert(atomic_load(&(valid_prop->create_version)) == 1);

                            /* Get the entry for that property in the prop_table */
                            prop_entry =
                                search_prop_table(class_entry->prop_table, class_entry->num_prop_entries,
                                                  valid_prop->chksum, valid_prop->name);

                            CHECK_PTR(prop_entry, "search_prop_table");
                            assert(prop_entry);

                            /* Attempt to atomically update the status of the prop_entry */
                            loop_check = FALSE;
                            done       = FALSE;
                            do {
                                prop_status = atomic_load(&(prop_entry->status));

                                if (prop_status == DOESNT_EXIST) {
                                    update_prop_status = EXISTS;

                                    if (!atomic_compare_exchange_strong(&(prop_entry->status), &prop_status,
                                                                        update_prop_status)) {
                                        /**
                                         * TODO: For now we only try again once if cas fails
                                         */
                                        if (loop_check) {
                                            assert(FALSE);
                                        }
                                        else {
                                            loop_check = TRUE;
                                        }
                                    }
                                    else {
                                        done = TRUE;
                                    }

                                } /* end if ( prop_status == DOESNT_EXIST ) */
                                else {
                                    /**
                                     * If the prop_status is not DOESNT_EXIST and the 
                                     * class's version has been updated, then the prop
                                     * was deleted by another thread, and we don't need
                                     * to update the prop_status, because the deleting
                                     * thread has, or will, do that. Nothing else should
                                     * be possible where the prop_status isn't 
                                     * DOESNT_EXIST and the class's version has been 
                                     * incremented.
                                     */
                                    check_ver = atomic_load(&(class->curr_version));
                                    if (check_ver > version) {
                                        done = TRUE;
                                    }
                                    else {
                                        assert(FALSE);
                                    }
                                }

                            } while (!done);

                            prev_prop = valid_prop;

                        } /* end if ( valid_prop ) */

                    } while (valid_prop);

                } /* end if ( log_pl_len > 0 ) */

                /* Update op_info */
                op_info->class   = class;
                op_info->id      = class_id;
                op_info->obj_ver = version;
                op_info->result  = OP_SUCCESS;

                /* Update class status to EXISTS */
                update_status = EXISTS;
                loop_check    = FALSE;
                done          = FALSE;
                do {
                    class_status = atomic_load(&(class_entry->status));

                    /**
                     * Wait to see if the thread attempting to close this class
                     * attempted it before the class was created, meaning it 
                     * failed and then this thread will update status. Or if
                     * it did delete the class, then this thread will move on
                     * to done since the closing thread would've updated the 
                     * status to DELETED or EXISTS_BUT_CLOSED.
                     */
                    while ( class_status == CLOSING_IN_PROGRESS )
                    {
                        sleep(1);

                        class_status = atomic_load(&(class_entry->status));
                    }

                    /* If the class was closed mark done */
                    if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG ) {

                        assert(class_status == DELETED || class_status == EXISTS_BUT_CLOSED);

                        done = TRUE;
                    }
                    else
                    {
                        /* Attempt to atomically update the status */
                        if (!atomic_compare_exchange_strong(&(class_entry->status), &class_status,
                                                            update_status)) {
                            if (loop_check) {
                                assert(FALSE);
                            }
                            else {
                                loop_check = TRUE;
                            }
                        }
                        else {
                            class_status = atomic_load(&(class_entry->status));
                            assert(class_status == EXISTS);

                            done = TRUE;
                        }
                    }

                } while (!done);

                assert(op_info->result != NOT_ATTEMPTED);

            } /* end else ( Class was created, update class_entry and op_info ) */

        } while (try_again);

    } /* end if ( create ) */

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end copy_class() */

/****************************************************************************************
 * Function:    read_list
 *
 * Purpose:     Randomly determines if to perform a read at the most current version of 
 *              the list or to perform a read an older version of the list.
 * 
 *              0-2 (~75%): search_list     - searches the most current version for a 
 *                                             random property.
 *                3 (~25%): search_list_ver - searches any version of the list for a
 *                                             random property.
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
read_list(thread_params_t *thread_params)
{
    int operation;

    herr_t ret; /* Generic return value */

    operation = rand() % 4;

    switch (operation) {
        case 0:
        case 1:
        case 2:
            ret = search_list(thread_params);
            CHECK_I(ret, "search_list");
            assert(ret == SUCCEED);
            break;
        case 3:
            ret = search_list_ver(thread_params);
            CHECK_I(ret, "search_list_ver");
            assert(ret == SUCCEED);
            break;
        default:
            assert(FALSE);
    }

    return SUCCEED;

} /* end read_list() */

/****************************************************************************************
 * Function:    read_class
 *
 * Purpose:     Randomly determines if to perform a read at the most current version of 
 *              the class or to perform a read an older version of the class.
 * 
 *              0-2 (~75%): search_class     - searches the most current version for a 
 *                                              random property.
 *                3 (~25%): search_class_ver - searches any version of the class for a
 *                                              random property.
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
read_class(thread_params_t *thread_params)
{
    int operation;

    herr_t ret; /* Generic return value */

    operation = rand() % 4;

    switch (operation) {
        case 0:
        case 1:
        case 2:
            ret = search_class(thread_params);
            CHECK_I(ret, "search_class");
            assert(ret == SUCCEED);
            break;
        case 3:
            ret = search_class_ver(thread_params);
            CHECK_I(ret, "search_class_ver");
            assert(ret == SUCCEED);
            break;
        default:
            assert(FALSE);
    }

    return SUCCEED;

} /* end read_class() */

/****************************************************************************************
 * Function:    write_list
 *
 * Purpose:     Randomly determines a type of write operation to perform on a list
 *              (aka to modify a list).
 * 
 *              0-2 (~30%): mod_list_create_prop
 *                              - Attempts to create a new property in the list.
 *              3-7 (~50%): mod_list_mod_prop 
 *                              - Attempts to modify an existing property in the list.
 *                                NOTE: this does create a new H5P_mt_prop_t.
 *              8-9 (~20%): mod_list_delete_prop 
 *                              - Attempts to delete a property from the list.
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
write_list(thread_params_t *thread_params)
{
    int operation;

    herr_t ret; /* Generic return value */

    operation = rand() % 10;

    switch (operation) {
        case 0:
        case 1:
        case 2:
            ret = mod_list_create_prop(thread_params);
            CHECK_I(ret, "mod_list_create_prop");
            assert(ret == SUCCEED);
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            ret = mod_list_mod_prop(thread_params);
            CHECK_I(ret, "mod_list_mod_prop");
            assert(ret == SUCCEED);
            break;
        case 8:
        case 9:
            ret = mod_list_delete_prop(thread_params);
            CHECK_I(ret, "mod_list_delete_prop");
            assert(ret == SUCCEED);
            break;
        default:
            assert(FALSE);
    }

    return SUCCEED;

} /* end write_list() */

/****************************************************************************************
 * Function:    write_class
 *
 * Purpose:     Randomly determines a type of write operation to perform on a class
 *              (aka to modify a class).
 * 
 *              0-2 (~30%): mod_class_create_prop
 *                              - Attempts to create a new property in the class.
 *              3-7 (~50%): mod_class_mod_prop 
 *                              - Attempts to modify an existing property in the class.
 *                                NOTE: this does create a new H5P_mt_prop_t.
 *              8-9 (~20%): mod_class_delete_prop 
 *                              - Attempts to delete a property from the class.
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
write_class(thread_params_t *thread_params)
{
    int operation;

    herr_t ret; /* Generic return value */

    operation = rand() % 10;

    switch (operation) {
        case 0:
        case 1:
        case 2:
            ret = mod_class_create_prop(thread_params);
            CHECK_I(ret, "mod_class_create_prop");
            assert(ret == SUCCEED);
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            ret = mod_class_mod_prop(thread_params);
            CHECK_I(ret, "mod_class_mod_prop");
            assert(ret == SUCCEED);
            break;
        case 8:
        case 9:
            ret = mod_class_delete_prop(thread_params);
            CHECK_I(ret, "mod_class_delete_prop");
            assert(ret == SUCCEED);
            break;
        default:
            assert(FALSE);
    }

    return SUCCEED;

} /* end write_class() */

#if 0
/****************************************************************************************
 * Function:    cmp_list
 *
 * Purpose:     Due to the randomness of these tests performing a compare and getting
 *              a success would be nearly impossible. So, to ensure that a successful
 *              compare can occur, if cmp_list_equal() is selected, a randomly chosen
 *              list is compared to itself.
 * 
 *              NOTE: Comparing lists and classes are tested extensively during in
 *              mt_test_1().
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
cmp_list(thread_params_t *thread_params)
{
    int operation;

    herr_t ret; /* Generic return value */

    operation = rand() % 2;

    switch (operation) {
        case 0:
            ret = cmp_list_equal(thread_params);
            CHECK_I(ret, "cmp_list_equal");
            assert(ret == SUCCEED);
            break;
        case 1:
            ret = cmp_list_not_equal(thread_params);
            CHECK_I(ret, "cmp_list_not_equal");
            assert(ret == SUCCEED);
            break;
        default:
            assert(FALSE);
    }

    return SUCCEED;

} /* end cmp_list() */

/****************************************************************************************
 * Function:    cmp_class
 *
 * Purpose:     Due to the randomness of these tests performing a compare and getting
 *              a success would be nearly impossible. So, to ensure that a successful
 *              compare can occur, if cmp_class_equal() is selected, a randomly chosen
 *              class is compared to itself.
 * 
 *              NOTE: Comparing lists and classes are tested extensively during in
 *              mt_test_1().
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
cmp_class(thread_params_t *thread_params)
{
    int operation;

    herr_t ret; /* Generic return value */

    operation = rand() % 2;

    switch (operation) {
        case 0:
            ret = cmp_class_equal(thread_params);
            CHECK_I(ret, "cmp_class_equal");
            assert(ret == SUCCEED);
            break;
        case 1:
            ret = cmp_class_not_equal(thread_params);
            CHECK_I(ret, "cmp_class_not_equal");
            assert(ret == SUCCEED);
            break;
        default:
            assert(FALSE);
    }

    return SUCCEED;

} /* end cmp_class() */
#endif

/****************************************************************************************
 * Function:    close_list
 *
 * Purpose:     Attempts to close a property list.
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a list_entry 
 *              from the list_table is randomly chosen, and it's status is checked.
 *               *  If status is EXISTS, IN_PROGRESS, or DOESNT_EXIST we atomically 
 *                  update the status to CLOSING_IN_PROGRESS and will continue to attempt
 *                  closing the list.
 *               *  If the status is CLOSING_IN_PROGRESS we sleep and loop till the 
 *                  status is updated to something else. 
 *               *  If the status is DELETED, we don't update the status and continue to
 *                  attempt closing the list.
 *              NOTE: we don't update the status if it's DELETED, because we already know
 *              that any operation on a deleted list will fail, we just ensure that is 
 *              TRUE and mark the result as such.
 * 
 *              Then we call H5Pclose() to attempt closing the list. 
 * 
 *              If closing the list succeeded, we grab it's curr_version and update the
 *              op_info->obj_ver and the list_entry->ver_deleted, and update the result
 *              as OP_SUCCESS. Then atomically update the list's status to DELETED.
 *              Next we must check if deleting this list also deleted it's parent.
 * 
 *              NOTE: this is because when a list or class is derived they increment 
 *              their parent's index ref count. Thus, when they're closed they also must 
 *              decrement their parent's index ref count. If the parent was closed prior
 *              then closing this list may have finished the class being closed and 
 *              deleted from the index if it had no other derived objects. So, we must 
 *              check the parent and atomically update it's status to DELETED if 
 *              necessary. (NOTE: we must loop checking up the tree of parents).
 * 
 *              If closing the list failed, check if the list has ever been created.
 *               *  If the list has been created but closing it failed, it must have 
 *                  already been deleted. Ensure that is correct and update obj_ver to
 *                  the version the list was deleted at and set result as LIST_DELETED.
 *               *  If the list hasn't been created, then we must atomically update the 
 *                  list_entry's status back to DOESNT_EXIST.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
close_list(thread_params_t *thread_params)
{
    list_table_entry_t  *list_entry;
    class_table_entry_t *parent_entry;
    H5P_mt_list_sptr_t   list_sptr;
    H5P_mt_class_sptr_t  parent_sptr;
    status_t             list_status;
    status_t             update_status;
    status_t             start_status;
    status_t             parent_status;
    H5P_mt_list_t       *list   = NULL;
    H5P_mt_list_t       *check_list = NULL;
    H5P_mt_class_t      *parent = NULL;
    H5P_mt_class_ref_counts_t ref_count;
    int                  r;
    test_op_info_t      *op_info = NULL;
    uint32_t             op_num;
    bool                 done       = FALSE;
    bool                 loop_check = FALSE;
    bool                 all_parents_checked = FALSE;
    hid_t                list_id;
    herr_t               ret; /* Generic return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = DELETE;
    op_info->op_num = op_num;

    /* Get a random entry from the list table */
    r = rand() % LIST_TABLE_SIZE;

    list_entry = &list_table[r];

    atomic_fetch_add(&(list_entry->op_count), 1);

    op_info->parent_name = list_entry->parent_name;
    op_info->test_id     = list_entry->test_list_id;

    if (list_entry->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    /* Update list_status */
    update_status = CLOSING_IN_PROGRESS;
    do {
        list_status  = atomic_load(&(list_entry->status));
        start_status = list_status;

        list_sptr = atomic_load(&(list_entry->list_sptr));
        list      = list_sptr.ptr;

        /* Attempt to atomically update status */
        if (list_status == EXISTS || list_status == IN_PROGRESS || list_status == DOESNT_EXIST) {
            if (!atomic_compare_exchange_strong(&(list_entry->status), &list_status, update_status)) {
                list_status = atomic_load(&(list_entry->status));

                /* Atomic update failed, loop and try once more */
                if (loop_check) {
                    assert(FALSE);
                }
                else {
                    loop_check = TRUE;
                }
            }
            else {
                /* Atomic update successful */
                list_status = atomic_load(&(list_entry->status));
                assert(list_status == CLOSING_IN_PROGRESS);

                done = TRUE;
            }
        }
        /* If CLOSING_IN_PROGRESS, wait for the other thread to finish */
        else if ( list_status == CLOSING_IN_PROGRESS )
        {
            sleep(1);
        }
        /* If DELETED no need to update. */
        else {
            assert(list_status == DELETED);
            done = TRUE;
        }

    } while (!done);

    list_id       = atomic_load(&(list_entry->id));
    op_info->id   = list_id;
    op_info->list = list;

    assert(list_status == CLOSING_IN_PROGRESS || list_status == DELETED );

    /* Attempt to close the list */
    H5E_BEGIN_TRY
    {
        ret = H5Pclose(list_id);
    }
    H5E_END_TRY

    /* Double check the list is not in the index */
    H5E_BEGIN_TRY
    {
        check_list = H5I_object(list_id);
    }
    H5E_END_TRY

    CHECK_PTR_NULL(check_list, "H5I_object");
    assert(!check_list);

    /* If closing the list was successful */
    if (ret == SUCCEED) {
        
        /* If somehow we don't have it, grab pointer to the list */
        if ( ! list )
        {
            list_sptr = atomic_load(&(list_entry->list_sptr));
            list      = list_sptr.ptr;
        }

        op_info->list    = list;
        op_info->obj_ver = atomic_load(&(list->curr_version));
        op_info->result  = OP_SUCCESS;

        atomic_store(&(list_entry->ver_deleted), op_info->obj_ver);

        /* Check and update list_status */
        update_status = DELETED;
        done          = FALSE;
        do {
            list_status = atomic_load(&(list_entry->status));

            if (list_status == CLOSING_IN_PROGRESS) {
                if (!atomic_compare_exchange_strong(&(list_entry->status), &list_status, update_status)) {
                    list_status = atomic_load(&(list_entry->status));
                    assert(list_status == EXISTS || list_status == IN_PROGRESS);

                    if (loop_check) {
                        assert(FALSE);
                    }
                    else {
                        loop_check = TRUE;
                    }
                }
                else {
                    /* Atomic update successful */
                    assert(DELETED == atomic_load(&(list_entry->status)));

                    atomic_store(&(list_entry->ver_deleted), op_info->obj_ver);
                    done = TRUE;
                }
            }
            else {
                /**
                 * If status is anything other than CLOSING_IN_PROGRESS
                 * something went wrong. Investigate.
                 */
                assert(FALSE);
            }

        } while (!done);

        /* Check if closing this list closed its parent */
        parent_entry = atomic_load(&(list_entry->parent_entry));
        update_status = DELETED;
        do
        {
            if ( parent_entry )
            {
                parent_sptr = atomic_load(&(parent_entry->class_sptr));
                parent      = parent_sptr.ptr;
                        
                done          = FALSE;
                do {
                    parent_status = atomic_load(&(parent_entry->status));

                    /* Wait for the other thread to finish */
                    if ( parent_status == CLOSING_IN_PROGRESS )
                    {
                        /** 
                         * TODO: add stat to track this
                         */
                        done = FALSE;
                    }
                    else if ( atomic_load(&(parent->tag)) == H5P_MT_CLASS_INVALID_TAG )
                    {
                        assert(atomic_load(&(parent_entry->ver_closed)) != 0);

                        if ( parent_status == DELETED )
                        {
                            assert(atomic_load(&(parent_entry->ver_deleted)) != 0);

                            done = TRUE;
                        }
                        else
                        {
                            if (!atomic_compare_exchange_strong(&(parent_entry->status), 
                                                                &parent_status, update_status)) {
                                if (loop_check) {
                                    assert(FALSE);
                                }
                                else {
                                    loop_check = TRUE;
                                }
                            }
                            else {
                                atomic_store(&(parent_entry->ver_deleted),
                                        atomic_load(&(parent->curr_version)));

                                done = TRUE;
                            }
                        }
                    }
                    else if ( parent_status == EXISTS || parent_status == IN_PROGRESS )
                    {
                        assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_TAG);

                        done = TRUE;
                    }
                    else if ( parent_status == EXISTS_BUT_CLOSED )
                    {
                        assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_TAG);
                        assert(atomic_load(&(parent_entry->ver_closed)) != 0);

                        ref_count = atomic_load(&(parent->ref_count));

                        /* Parent still has existing derived objects */
                        if ( ref_count.pl > 0 || ref_count.plc > 0 )
                        {
                            done = TRUE;
                        }
                        else
                        {
                            assert(FALSE);
                        }
                    }
                    else
                    {
                        assert(FALSE);
                    }

                } while (!done);

                /* Check the parent's parent */
                parent_entry = atomic_load(&(parent_entry->parent_entry));

            } /* end if ( parent_entry ) */
            else
            {
                all_parents_checked = TRUE;
            }

        } while ( ! all_parents_checked );

        /* Double check that if the parent is deleted it is marked as such */
        parent_entry = atomic_load(&(list_entry->parent_entry));

        parent_status = atomic_load(&(parent_entry->status));
        parent_sptr   = atomic_load(&(parent_entry->class_sptr));
        parent        = parent_sptr.ptr;

        if ( atomic_load(&(parent->tag)) == H5P_MT_CLASS_INVALID_TAG )
        {
            assert(parent_status == DELETED);
        }

    } /* end if ( ret == SUCCEED ) */
    /* If closing the list failed, find why */
    else {

        list_status = atomic_load(&(list_entry->status));

        /**
         * If the list has been created but closing it failed,
         * it was already closed. Double check
         */
        if (list) {
            
            if ( atomic_load(&(list->tag)) != H5P_MT_LIST_INVALID_TAG )
            {
                fprintf(stderr, "start_status: %d\n", start_status);
                assert(FALSE);
            }

            op_info->obj_ver = atomic_load(&(list->curr_version));
            op_info->result  = LIST_DELETED;

        } /* end if ( list ) */
        else {

            op_info->result = LIST_DOESNT_EXIST;
            
            /* Attempt to atomically set status back to start_status */
            if ( list_status == CLOSING_IN_PROGRESS )
            {
                do {
                    if (!atomic_compare_exchange_strong(&(list_entry->status), 
                                                        &list_status, start_status)) 
                    {
                        if (loop_check) {
                            assert(FALSE);
                        }
                        else {
                            loop_check = TRUE;
                        }
                    }
                    else {
                        list_status = atomic_load(&(list_entry->status));
                        assert(list_status == start_status);

                        done = TRUE;
                    }

                } while (!done);
            }
        }

    } /* end else ( ret != SUCCEED ) */

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end close_list() */

/****************************************************************************************
 * Function:    close_class
 *
 * Purpose:     Attempts to close a property list class.
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a class_entry
 *              from the class_table is randomly chosen, and it's status is checked.
 *               *  If status is EXISTS, IN_PROGRESS, or DOESNT_EXIST we atomically 
 *                  update the status to CLOSING_IN_PROGRESS and will continue to attempt
 *                  closing the class.
 *               *  If the status is CLOSING_IN_PROGRESS we loop and randomly a get a
 *                  different class. 
 *               *  If the status is DELETED, we don't update the status and continue to
 *                  attempt closing the list.
 *               *  If the status is EXISTS_BUT_CLOSED, we set result as CLASS_DELETED
 *                  and continue to the next operation.
 *              NOTE: we don't update the status if it's DELETED, because we already know
 *              that any operation on a deleted class will fail, we just ensure that is 
 *              TRUE and mark the result as such.
 *              NOTE: If the status is EXISTS_BUT_CLOSING we don't try closing it again, 
 *              because if a class is closed multiple times, before being deleted from 
 *              the index, an assert will fail in H5I. 
 *              NOTE: The first 4 classes that were created prior to the test iteration
 *              starting and are the test's "default classes" are skipped from being
 *              chosen to be closed. During regular hdf5 library operation the default
 *              classes are not closed until library shutdown, and if they are closed,
 *              the H5I assert that fails for closing a class multiple times will fail.
 * 
 *              Then we call H5Pclose_class() to attempt closing the class, and call
 *              H5I_object() on the class_id to see if the class is still in the index.
 * 
 *              If closing the class succeeded, we grab it's curr_version and update the
 *              op_info->obj_ver and the class_entry->ver_closed, and update the result
 *              as OP_SUCCESS. 
 *               *  If the class was still in the index, then it must have had existing 
 *                  derived objects when closed, so its index ID's ref count didn't 
 *                  decrement to 0. We double check that it does have derived objects and
 *                  atomically update its status to EXISTS_BUT_CLOSED.
 *               *  If the class was not in the index, we ensure it was deleted and
 *                  atomically update its status to DELETED. We must also check if 
 *                  deleting this class deleted it's parent, just like with lists, and if
 *                  the parent was deleted atomically update its status to DELETED. 
 *                  NOTE: when a class or parent is deleted, the version it was deleted 
 *                  at is atomically stored in class_entry->ver_deleted.
 * 
 *              If closing the class failed, check if the class has ever been created.
 *               *  If the class has been created but closing it failed, it must have 
 *                  already been deleted. Ensure that is correct and update obj_ver to
 *                  the version the class was deleted at and set result as CLASS_DELETED.
 *               *  If the class hasn't been created, then we must atomically update the 
 *                  class_entry's status back to DOESNT_EXIST.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
close_class(thread_params_t *thread_params)
{
    class_table_entry_t      *class_entry;
    class_table_entry_t      *parent_entry = NULL;
    H5P_mt_class_sptr_t       class_sptr;
    H5P_mt_class_sptr_t       parent_sptr;
    H5P_mt_class_ref_counts_t ref_count;
    status_t                  class_status;
    status_t                  update_status;
    status_t                  start_status;
    status_t                  check_status;
    status_t                  parent_status;
    H5P_mt_class_t *class  = NULL;
    H5P_mt_class_t *parent = NULL;
    H5P_mt_class_t *check_class = NULL;
    int             r;
    test_op_info_t *op_info = NULL;
    uint32_t        op_num;
    bool            done           = FALSE;
    bool            try_close      = TRUE;
    bool            loop_check     = FALSE;
    bool            get_diff_class = FALSE;
    bool            all_parents_checked = FALSE;
    hid_t           class_id;
    herr_t          ret; /* Generic return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = DELETE;
    op_info->op_num = op_num;

    do {
        /* Get a random entry from the class table */
        /**
         * NOTE: If we get the first 3 entries in the class_table we loop again to get a
         * different entry. The first three entries are the test's equivalent to default
         * classes. The default classes are not closed during regular HDF5 operations and
         * are only closed upon shut down after all derived objects are closed. Closing a
         * default class like a non-default class will cause an assert failure in H5I.
         */
        do {
            r = rand() % CLASS_TABLE_SIZE;
        
        } while ( r < 4 );

        class_entry = &class_table[r];

        op_info->class_name  = class_entry->name;
        op_info->parent_name = class_entry->parent_name;
        op_info->test_id     = class_entry->test_class_id;

        if (class_entry->copy) {
            op_info->obj_isa_copy = TRUE;
        }

        class_id    = atomic_load(&(class_entry->id));
        op_info->id = class_id;

        get_diff_class = FALSE;

        /**
         * NOTE: due to an assert being triggered if a class is closed
         * multiple times, while having existing derived lists and/or
         * classes, we must update the status to EXISTS_BUT_CLOSED
         * prior to attempting closing the class, to avoid triggering
         * an assertion.
         */
        update_status = CLOSING_IN_PROGRESS;
        do {
            class_status = atomic_load(&(class_entry->status));
            start_status = class_status;

            class_sptr = atomic_load(&(class_entry->class_sptr));
            class      = class_sptr.ptr;

            op_info->class = class;

            /* If status shows it's already been closed, update op_info and move on */
            if (class_status == EXISTS_BUT_CLOSED) {
                op_info->class   = class;
                op_info->obj_ver = atomic_load(&(class->curr_version));
                op_info->result  = CLASS_DELETED;

                done      = TRUE;
                try_close = FALSE;
            }
            /* If CLOSING_IN_PROGRESS, try closing another class */
            else if (class_status == CLOSING_IN_PROGRESS) {
                get_diff_class = TRUE;
                done           = TRUE;
            }
            else if (class_status == DELETED) {
                /**
                 * NOTE: If already deleted, no need to update status
                 * and can attempt to delete the class. Since it's no
                 * longer in the index no assertion will fail.
                 */
                done = TRUE;
            }
            /* Attempt to atomically update status */
            else {
                if (!atomic_compare_exchange_strong(&(class_entry->status), &class_status, update_status)) {

                    /* Atomic update failed, loop and try once more */
                    if (loop_check) {
                        assert(FALSE);
                    }
                    else {
                        loop_check = TRUE;
                    }
                }
                else {
                    /* Atomic update successful */
                    check_status = atomic_load(&(class_entry->status));
                    assert(check_status == CLOSING_IN_PROGRESS);

                    done = TRUE;
                }
            }

        } while (!done);

    } while (get_diff_class);

    atomic_fetch_add(&(class_entry->op_count), 1);

    /* If try_close is TRUE, try to close the class */
    if (try_close) {

        check_status = atomic_load(&(class_entry->status));
        assert(check_status == CLOSING_IN_PROGRESS || check_status == DELETED);

        /* Attempt to close the class */
        H5E_BEGIN_TRY
        {
            ret = H5Pclose_class(class_id);
        }
        H5E_END_TRY

        /* Double check the class is not in the index */
        H5E_BEGIN_TRY
        {
            check_class = H5I_object(class_id);
        }
        H5E_END_TRY

        if (ret == SUCCEED) {

            assert(start_status == EXISTS || start_status == IN_PROGRESS);

            /* If somehow we don't have it, grab pointer to the class */
            if ( ! class )
            {
                class_sptr = atomic_load(&(class_entry->class_sptr));
                class      = class_sptr.ptr;
            }

            op_info->class   = class;
            op_info->obj_ver = atomic_load(&(class->curr_version));
            op_info->result  = OP_SUCCESS;

            atomic_store(&(class_entry->ver_closed), op_info->obj_ver);

            /* Check if class has been closed or deleted */
            if (check_class) {
                if ( atomic_load(&(check_class->tag)) == H5P_MT_CLASS_TAG )
                {
                    ref_count = atomic_load(&(check_class->ref_count));
                    assert(ref_count.pl > 0 || ref_count.plc > 0);
                    
                    update_status = EXISTS_BUT_CLOSED;
                }
                else
                {
                    update_status = DELETED;
                }
            }
            else {
                update_status = DELETED;
            }

            /* Update status */
            done = FALSE;
            do {
                /* If DELETED, ensure the struct tag reflects that */
                if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG )
                {
                    update_status = DELETED;

                    atomic_store(&(class_entry->ver_deleted), 
                                    atomic_load(&(class->curr_version)));
                }

                class_status = atomic_load(&(class_entry->status));
                assert(class_status == CLOSING_IN_PROGRESS);

                if (!atomic_compare_exchange_strong(&(class_entry->status), &class_status, update_status)) {
                    check_status = atomic_load(&(class_entry->status));

                    assert(check_status == CLOSING_IN_PROGRESS);

                    /* Atomic update failed, loop and try once more */
                    if (loop_check) {
                        assert(FALSE);
                    }
                    else {
                        loop_check = TRUE;
                    }
                }
                else {
                    /* Atomic update successful */
                    check_status = atomic_load(&(class_entry->status));

                    assert(check_status == DELETED || check_status == EXISTS_BUT_CLOSED);

                    done = TRUE;
                }

            } while (!done);

            /* Check if closing this class closed it's parent */
            if (check_status == DELETED) {

                parent_entry = atomic_load(&(class_entry->parent_entry));

                do
                {
                    /**
                     * If NULL, the test_root is the parent and it can't be
                     * closed during these tests.
                     */
                    if (parent_entry) {

                        parent_sptr = atomic_load(&(parent_entry->class_sptr));
                        parent      = parent_sptr.ptr;

                        update_status = DELETED;
                        done          = FALSE;
                        do {
                            parent_status = atomic_load(&(parent_entry->status));

                            /* Wait for the other thread to finish */
                            if ( parent_status == CLOSING_IN_PROGRESS )
                            {
                                //fprintf(stderr,
                                    //"\nWhile closing class, parent is also being closed\n");

                                /** 
                                 * TODO: add stat to track this
                                 */
                                done = FALSE;
                            }
                            else if ( atomic_load(&(parent->tag)) == H5P_MT_CLASS_INVALID_TAG )
                            {
                                assert(atomic_load(&(parent_entry->ver_closed)) != 0);

                                if ( parent_status == DELETED )
                                {
                                    assert(atomic_load(&(parent_entry->ver_deleted)) != 0);

                                    done = TRUE;
                                }
                                else
                                {
                                    if (!atomic_compare_exchange_strong(&(parent_entry->status), 
                                                        &parent_status, update_status)) {
                                        if (loop_check) {
                                            assert(FALSE);
                                        }
                                        else {
                                            loop_check = TRUE;
                                        }
                                    }
                                    else {
                                        //fprintf(stderr,
                                            //"\nWhile closing list, also deleted parent \n");

                                        atomic_store(&(parent_entry->ver_deleted), 
                                                atomic_load(&(parent->curr_version)));

                                        done = TRUE;
                                    }
                                }

                            }
                            else if ( parent_status == EXISTS || 
                                        parent_status == IN_PROGRESS )
                            {
                                assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_TAG);

                                done = TRUE;
                            }
                            else if ( parent_status == EXISTS_BUT_CLOSED )
                            {
                                assert(atomic_load(&(parent->tag)) == H5P_MT_CLASS_TAG);
                                assert(atomic_load(&(parent_entry->ver_closed)) != 0);

                                ref_count = atomic_load(&(parent->ref_count));

                                /* Parent still has existing derived objects */
                                if ( ref_count.pl > 0 || ref_count.plc > 0 )
                                {
                                    done = TRUE;
                                }
                                else
                                {
                                    assert(FALSE);
                                }
                            }
                            else
                            {
                                assert(FALSE);
                            }

                        } while (!done);

                        /* Check the parent's parent */
                        parent_entry = atomic_load(&(parent_entry->parent_entry));

                    } /* end if ( parent_entry ) */
                    else
                    {
                        all_parents_checked = TRUE;
                    }

                } while ( ! all_parents_checked );

            } /* end if (check_status == DELETED) */

            /* Double check that if the parent is deleted it is marked as such */
            parent_entry = atomic_load(&(class_entry->parent_entry));

            if ( parent_entry )
            {
                parent_status = atomic_load(&(parent_entry->status));
                parent_sptr   = atomic_load(&(parent_entry->class_sptr));
                parent        = parent_sptr.ptr;

                if ( atomic_load(&(parent->tag)) == H5P_MT_CLASS_INVALID_TAG )
                {
                    assert(parent_status == DELETED);
                }
            }

        } /* end if ( ret == SUCCEED ) */
        else {
            /* If closing the class failed, find why */
            if (class) {
                assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);

                op_info->result = CLASS_DELETED;
            }
            else {
                op_info->result = CLASS_DOESNT_EXIST;

                /* Update class status back to start_status */
                done = FALSE;
                do {
                    class_status = atomic_load(&(class_entry->status));

                    /**
                     * The class DOESNT_EXIST, so only creating the class can change
                     * its status, and create checks if the status is EXISTS_BUT_CLOSED
                     * and if the class has ever been created, and will sleep a few times
                     * to give this thread time to update the status back, before it
                     * continues. Thus the status shouldn't have been able to be changed.
                     */
                    assert(class_status == CLOSING_IN_PROGRESS);

                    if (!atomic_compare_exchange_strong(&(class_entry->status), &class_status,
                                                        start_status)) {
                        if (loop_check) {
                            assert(FALSE);
                        }
                        else {
                            loop_check = TRUE;
                        }
                    }
                    else {
                        check_status = atomic_load(&(class_entry->status));

                        assert(check_status == start_status);
                        assert(check_status == DOESNT_EXIST ||
                               check_status == IN_PROGRESS);

                        done = TRUE;
                    }

                } while (!done);
            }

        } /* end else ( ret != SUCCEED ) */

    } /* end if ( try_close ) */
    else {
        check_status = atomic_load(&(class_entry->status));

        assert(check_status == EXISTS_BUT_CLOSED);
    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end close_class() */

/****************************************************************************************
 * Function:    search_list
 *
 * Purpose:     Attempts to search for a property in a property list.
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a list_entry 
 *              from the list_table is randomly chosen and a prop_entry from the 
 *              list_entry's prop_table is also randomly chosen. If the list has ever 
 *              been created, we set obj_ver as the curr_version of the list and attempt 
 *              to search it for the property as long as the list's struct tag is valid.
 *              
 *              If searching the list was successful, we grab the property from the list
 *              and use the version of the property as the op_ver and set result as 
 *              OP_SUCCESS.
 *              NOTE: the functions used to grab the property from the list are specific
 *              test functions that grab the property regardless of validity of the list
 *              or validity of the property. That way if the search was successful, but 
 *              the list is deleted immediately afterwards, these functions will still
 *              search the list without assert failures or errors. 
 *              NOTE: the reason we grab the property at all is to assign op_info->prop
 *              to point to that property, and to set op_info->op_ver as the version the
 *              property was created at. This is done because if the version of the list
 *              is updated after grabbing the obj_ver (specifically this property we are 
 *              searching for was created after we set obj_ver), we will see that 
 *              difference here and account for it during check_operations().
 *              
 *              If searching the list failed we search the index for the list as a double
 *              check. If the list was in the index, the property wasn't valid either by
 *              not existing or by being deleted. We get the status of the property and
 *              check.
 *               *  If the status is DOESNT_EXIST or IN_PROGRESS, we mark result as 
 *                  PROP_DOESNT_EXIST.
 *               *  If the status is EXISTS or DELETED, we search the list for versions
 *                  of the property. If the only versions are after the version we 
 *                  searched the property was created after the version we started 
 *                  searching and thus wasn't a valid property. Result is marked
 *                  PROP_DOESNT_EXIST.
 *               *  If the property has a version during the version we searched it must
 *                  be deleted, so that is double checked and result is marked 
 *                  PROP_DELETED.
 * 
 *              If the searching the list failed and the list wasn't in the index, we 
 *              check the status of the list.
 *               *  If the list was created, but wasn't in the index and has a status
 *                  of IN_PROGRESS, then we searched the list too early while another
 *                  thread was creating it, but before it finished and hadn't inserted 
 *                  the list into the index. Set obj_ver and op_ver to 0, op_info->list 
 *                  to NULL, and the result is marked LIST_DOESNT_EXIST.
 *               *  If the status is DELETED or CLOSING_IN_PROGRESS we grab the version
 *                  the list was deleted at and set that as obj_ver and mark the result
 *                  as LIST_DELETED.
 *              NOTE: If the list was created but wasn't in the index, we must also set
 *              the obj_ver and op_ver to 0 and set the op_info->list to NULL. While
 *              technically the list doesn exist at version 1, since it wasn't in the 
 *              index yet the rest of the library doesn't have access to it so it doesn't
 *              exist to the rest of the library. And we set the obj_ver and op_ver to 0
 *              and the list to NULL so this operation gets sorted corrected during
 *              check_operations.
 * 
 *              If before we attempt to search the list its struct tag is invalid, we
 *              double check its status is DELETED or CLOSING_IN_PROGRESS its obj_ver
 *              is set to the version it was deleted at, and result is marked as 
 *              LIST_DELETED.
 * 
 *              Lastly if the list was not created at all yet, but the time the 
 *              list_entry was grabbed, the result is marked LIST_DOESNT_EXIST.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
search_list(thread_params_t *thread_params)
{
    list_table_entry_t *list_entry;
    H5P_mt_list_sptr_t  list_sptr;
    H5P_mt_list_t      *list;
    //H5P_mt_list_table_entry_t *lkup_entry;
    //H5P_mt_list_prop_ref_t prop_ref;
    hid_t               list_id;
    prop_table_entry_t *prop_entry;
    H5P_mt_prop_t      *prop = NULL;
    //H5P_mt_prop_aptr_t  next;
    status_t            list_status;
    status_t            prop_status;
    test_op_info_t *op_info = NULL;
    uint32_t        op_num;
    int             r;
    uint32_t        nprops;
    uint64_t        value = 0;
    uint64_t        prop_ver;
    //uint64_t        first_curr;
    uint64_t        curr_ver;
    uint64_t        ver_del;
    //uint64_t        base_del_ver;
    bool            base_flag = FALSE;
    bool            deleted   = FALSE;
    herr_t          ret; /* Generic return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = SEARCH;
    op_info->op_num = op_num;

    /* Get a random entry from the list table */
    r = rand() % LIST_TABLE_SIZE;

    list_entry = &list_table[r];

    atomic_fetch_add(&(list_entry->op_count), 1);

    op_info->parent_name = list_entry->parent_name;
    op_info->test_id     = list_entry->test_list_id;

    if (list_entry->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    list_id     = atomic_load(&(list_entry->id));
    op_info->id = list_id;

    /* Randomly select a possible property in the list to search for */
    nprops = atomic_load(&(list_entry->num_prop_entries));
    r      = rand() % (int)nprops;

    prop_entry = &list_entry->prop_table[r];

    /* Update op_info */
    list_sptr = atomic_load(&(list_entry->list_sptr));
    list      = list_sptr.ptr;

    op_info->list      = list;
    op_info->prop_name = prop_entry->name;

    if (list) {
        list_status = atomic_load(&(list_entry->status));

        if ( atomic_load(&(list->tag)) == H5P_MT_LIST_TAG ) {

            /* Attempt to search the newest version of the list for a property */
            H5E_BEGIN_TRY
            {
                op_info->obj_ver = atomic_load(&(list->curr_version));
                op_info->op_ver = op_info->obj_ver;

                if ( atomic_load(&(list->tag)) == H5P_MT_LIST_TAG )
                {
                    ret = H5Pget(list_id, prop_entry->name, &value);
                }
                else
                {
                    ret = FAIL;
                }
            }
            H5E_END_TRY

            if (ret == SUCCEED) {
                /**
                 * NOTE: The exact value of the property is checked after all operations
                 * are done, to not spend more time than necessary not performing an operation
                 * on a list or class to increase likelihood of thread collisions occuring
                 * in H5P for better testing.
                 */
                assert(value > 0);

                curr_ver = atomic_load(&(list->curr_version));

                prop = get_prop_from_lkup_tbl(list, prop_entry->name, curr_ver,
                                                &base_flag);

                if ( ! prop )
                {
                    prop = get_prop_from_lfsll(list->pl_head, prop_entry->name, 
                                                curr_ver);
                }
                CHECK_PTR(prop, "get_prop_from_lkup_tbl/get_prop_from_lfsll");
                assert(prop);

                /* In case the version was updated between grabbing op_ver and now */
                prop_ver = atomic_load(&(prop->create_version));

                if ( prop_ver > op_info->op_ver )
                {
                    op_info->op_ver = prop_ver;
                }

                /* Update op_info */
                op_info->prop   = prop;
                op_info->result = OP_SUCCESS;
            }
            /* Searching for the property failed, find why */
            else {
                list_status = atomic_load(&(list_entry->status));

                H5E_BEGIN_TRY
                {
                    list = (H5P_mt_list_t *)H5I_object(list_id);
                }
                H5E_END_TRY

                /* If the search failed and the list is in the index check prop_status */
                if (list) {
                    prop_status = atomic_load(&(prop_entry->status));

                    curr_ver = atomic_load(&(list->curr_version));

                    if (prop_status == DOESNT_EXIST || prop_status == IN_PROGRESS) {
                        op_info->result = PROP_DOESNT_EXIST;
                    }
                    /**
                     * If we failed to find the property, but the status
                     * is EXISTS, check that the prop was created after the
                     * version we searched.
                     * If status is DELETED ensure that is correct for the 
                     * version we searched.
                     */
                    else if (prop_status == EXISTS || prop_status == DELETED) 
                    {

                        prop_ver = prop_ver_check_list(list, prop_entry, 
                                        op_info->obj_ver, curr_ver, &deleted, FALSE);

                        if ( deleted )
                        {
                            assert(prop_ver > 0);
                            op_info->op_ver = prop_ver;
                            op_info->result  = PROP_DELETED;
                        }
                        else
                        {
                            assert(prop_ver == 0);
                            op_info->result = PROP_DOESNT_EXIST;
                        }

                    } /* end else if (prop_status == EXISTS || prop_status == DELETED) */

                } /* end if ( list ) */
                /* If the search failed and the list isn't in the index check list_status */
                else {
                    list_status = atomic_load(&(list_entry->status));

                    list = op_info->list;

                    if ( list_status == IN_PROGRESS )
                    {
                        op_info->obj_ver = 0;
                        op_info->op_ver  = 0;
                        op_info->list    = NULL;
                        op_info->result = LIST_DOESNT_EXIST;
                    }
                    else if ( list_status == DELETED || 
                                list_status == CLOSING_IN_PROGRESS )
                    {
                        /**
                         * If list_entry's->ver_deleted is 0, the closing is still 
                         * in process so sleep and loop till the thread performing 
                         * the close updates ver_deleted, because we must know 
                         * which version the list was deleted at.
                         */
                        ver_del = atomic_load(&(list_entry->ver_deleted));

                        while ( ver_del == 0 )
                        {
                            sleep(1);

                            ver_del = atomic_load(&(list_entry->ver_deleted));
                        }

                        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);

                        op_info->obj_ver = ver_del;
                        op_info->result  = LIST_DELETED;
                    }
                    else {
                        fprintf(stderr, "\nList has been created, but isn't in the index\n");
                        fprintf(stderr, "status should be IN_PROGRESS\n");
                        fprintf(stderr, "list_status: %d\n", list_status);
                        assert(FALSE);
                    }
                }

            } /* end else ( ret != SUCCEED ) */

        } /* end if ( atomic_load(&(list->tag)) == H5P_MT_LIST_TAG )  */
        else {

            if (list_status == DELETED || list_status == CLOSING_IN_PROGRESS) {

                /**
                 * If list_entry's->ver_deleted is 0, the closing is still 
                 * in process so sleep and loop till the thread performing 
                 * the close updates ver_deleted, because we must know 
                 * which version the list was deleted at.
                 */
                ver_del = atomic_load(&(list_entry->ver_deleted));

                while ( ver_del == 0 )
                {
                    sleep(1);

                    ver_del = atomic_load(&(list_entry->ver_deleted));
                }

                assert(atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);

                op_info->obj_ver = ver_del;
                op_info->result = LIST_DELETED;
            }
            else
            {
                assert(FALSE);
            }            
        }

    } /* end if ( list ) */
    else {
        op_info->result = LIST_DOESNT_EXIST;
    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end search_list() */

/****************************************************************************************
 * Function:    search_list_ver
 *
 * Purpose:     Attempts to search for a property in a property list.
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a list_entry 
 *              from the list_table is randomly chosen and a prop_entry from the 
 *              list_entry's prop_table is also randomly chosen. If the list has ever 
 *              been created, we set obj_ver as the curr_version of the list and if the
 *              curr_version is > 1 we randomly grab an older version of the list and
 *              attempt to search that version for the property as long as the list's 
 *              struct tag is valid. The version we attempt to search is set as the 
 *              op_ver.
 *              
 *              If searching the list was successful, the returned property is set as 
 *              op_info->prop and the result is marked OP_SUCCESS.
 *              NOTE: The function used to search at older versions returns the property
 *              itself, so we don't need to search for the property again.
 *              
 *              If searching the list failed we search the index for the list as a double
 *              check. If the list was in the index, the property wasn't valid either by
 *              not existing or by being deleted (at the version we searched). We get the 
 *              status of the property and check.
 *               *  If the status is DOESNT_EXIST or IN_PROGRESS, we mark result as 
 *                  PROP_DOESNT_EXIST.
 *               *  If the status is EXISTS or DELETED, we search the list for versions
 *                  of the property. If the only versions are after the version we 
 *                  searched the property was created after the version we started 
 *                  searching and thus wasn't a valid property. Result is marked
 *                  PROP_DOESNT_EXIST.
 *               *  If the property has a version during the version we searched it must
 *                  be deleted, so that is double checked and result is marked 
 *                  PROP_DELETED.
 * 
 *              If the searching the list failed and the list wasn't in the index, we 
 *              check the status of the list.
 *               *  If the list was created, but wasn't in the index and has a status
 *                  of IN_PROGRESS, then we searched the list too early while another
 *                  thread was creating it, but before it finished and hadn't inserted 
 *                  the list into the index. Set obj_ver and op_ver to 0, op_info->list 
 *                  to NULL, and the result is marked LIST_DOESNT_EXIST.
 *               *  If the status is DELETED or CLOSING_IN_PROGRESS we grab the version
 *                  the list was deleted at and set that as obj_ver and mark the result
 *                  as LIST_DELETED.
 * 
 *              If before we attempt to search the list its struct tag is invalid, we
 *              double check its status is DELETED or CLOSING_IN_PROGRESS its obj_ver
 *              is set to the version it was deleted at, and result is marked as 
 *              LIST_DELETED.
 * 
 *              Lastly if the list was not created at all yet, but the time the 
 *              list_entry was grabbed, the result is marked LIST_DOESNT_EXIST.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
search_list_ver(thread_params_t *thread_params)
{
    list_table_entry_t *list_entry;
    H5P_mt_list_sptr_t  list_sptr;
    H5P_mt_list_t      *list;
    //H5P_mt_list_table_entry_t *lkup_entry;
    //H5P_mt_list_prop_ref_t prop_ref;
    hid_t               list_id;
    prop_table_entry_t *prop_entry;
    H5P_mt_prop_t      *prop = NULL;
    //H5P_mt_prop_aptr_t  next;
    status_t            list_status;
    status_t            prop_status;
    test_op_info_t     *op_info = NULL;
    uint32_t            op_num;
    int                 r;
    uint64_t            curr_ver;
    uint64_t            search_ver;
    uint64_t            prop_ver;
    //uint64_t            first_curr;
    //uint64_t            base_del_ver;
    uint64_t            ver_del;
    uint32_t            nprops;
    bool                deleted = FALSE;

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = SEARCH_VER;
    op_info->op_num = op_num;

    /* Get a random entry from the list table */
    r = rand() % LIST_TABLE_SIZE;

    list_entry = &list_table[r];

    atomic_fetch_add(&(list_entry->op_count), 1);

    op_info->parent_name = list_entry->parent_name;
    op_info->test_id     = list_entry->test_list_id;

    if (list_entry->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    list_id     = atomic_load(&(list_entry->id));
    op_info->id = list_id;

    /* Randomly select a possible property in the list to search for */
    nprops = atomic_load(&(list_entry->num_prop_entries));
    r      = rand() % (int)nprops;

    prop_entry = &list_entry->prop_table[r];

    /* Update op_info */
    list_sptr = atomic_load(&(list_entry->list_sptr));
    list      = list_sptr.ptr;

    op_info->list      = list;
    op_info->prop_name = prop_entry->name;

    /**
     * If list has been created, attempt to search for a prop in a
     * version older than the most current one (if there is one).
     *
     * NOTE: we must ensure the list exists before attempting to
     * search, because we need to attempt to grab an older version
     * of the list. If the list doesn't exist, trying to grab an
     * older version will cause a seg fault.
     */
    if (list) {

        list_status = atomic_load(&(list_entry->status));

        if ( atomic_load(&(list->tag)) == H5P_MT_LIST_TAG ) {

            op_info->obj_ver = atomic_load(&(list->curr_version));

            /* Randomly get which list version to search */
            curr_ver = op_info->obj_ver;

            if (curr_ver > 1) {
                r          = rand();
                search_ver = (uint64_t)r % curr_ver;
                if (search_ver == curr_ver) {
                    search_ver--;
                }
                if ( search_ver == 0 )
                {
                    search_ver++;
                }
                assert(search_ver < curr_ver);
            }
            else {
                search_ver = curr_ver;
            }

            op_info->op_ver = search_ver;

            H5E_BEGIN_TRY
            {
                /**
                 * NOTE: Must check tag again, due to the chance of the 
                 * list being deleted while entering the H5E_BEGIN_TRY
                 */
                if ( atomic_load(&(list->tag)) == H5P_MT_LIST_TAG )
                {
                    prop = H5P__mt_search__list(list, prop_entry->name, search_ver);
                }
            }
            H5E_END_TRY

            /* If search was successful */
            if (prop) {
                op_info->prop   = prop;
                op_info->result = OP_SUCCESS;
            }
            /* If search failed, find why */
            else {
                H5E_BEGIN_TRY
                {
                    list = (H5P_mt_list_t *)H5I_object(list_id);
                }
                H5E_END_TRY

                /* If the list is in the index check the prop_status */
                if (list) {
                    prop_status = atomic_load(&(prop_entry->status));

                    curr_ver = atomic_load(&(list->curr_version));

                    if (prop_status == DOESNT_EXIST || prop_status == IN_PROGRESS) {
                        op_info->result = PROP_DOESNT_EXIST;
                    }
                    /**
                     * If we failed to create the new version, but the status
                     * is EXISTS, check that the prop was created after the
                     * version we attempted to modify.
                     * If status is DELETED ensure that is correct for the 
                     * version we attempted to modify.
                     */
                    else if (prop_status == EXISTS || prop_status == DELETED) 
                    {
                        prop_ver = prop_ver_check_list(list, prop_entry, 
                                        op_info->op_ver, curr_ver, &deleted, FALSE);

                        if ( deleted )
                        {
                            assert(prop_ver > 0);
                            op_info->result  = PROP_DELETED;
                        }
                        else
                        {
                            assert(prop_ver == 0);
                            op_info->result = PROP_DOESNT_EXIST;
                        }

                    } /* end else if (prop_status == EXISTS || prop_status == DELETED) */
                }
                else /* ( ! list ) */
                {
                    /* If the list isn't in the index */
                    assert(!list);

                    list_status = atomic_load(&(list_entry->status));

                    list = op_info->list;

                    if ( list_status == IN_PROGRESS )
                    {
                        op_info->obj_ver = 0;
                        op_info->op_ver  = 0;
                        op_info->list    = NULL;
                        op_info->result = LIST_DOESNT_EXIST;
                    }
                    else if ( list_status == DELETED || 
                                list_status == CLOSING_IN_PROGRESS )
                    {
                        /**
                         * If list_entry's->ver_deleted is 0, the closing is still 
                         * in process so sleep and loop till the thread performing 
                         * the close updates ver_deleted, because we must know 
                         * which version the list was deleted at.
                         */
                        ver_del = atomic_load(&(list_entry->ver_deleted));

                        while ( ver_del == 0 )
                        {
                            sleep(1);

                            ver_del = atomic_load(&(list_entry->ver_deleted));
                        }

                        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);

                        op_info->obj_ver = ver_del;
                        op_info->result  = LIST_DELETED;
                    }
                    else {
                        fprintf(stderr, "\nList has been created, but isn't in the index\n");
                        fprintf(stderr, "status should be IN_PROGRESS\n");
                        fprintf(stderr, "list_status: %d\n", list_status);
                        assert(FALSE);
                    }
                }

            } /* end else */
            
        } /* end if ( atomic_load(&(list->tag)) == H5P_MT_LIST_TAG ) */
        else {

            if (list_status == DELETED || list_status == CLOSING_IN_PROGRESS) {
                /**
                 * If list_entry's->ver_deleted is 0, the closing is still 
                 * in process so sleep and loop till the thread performing 
                 * the close updates ver_deleted, because we must know 
                 * which version the list was deleted at.
                 */
                ver_del = atomic_load(&(list_entry->ver_deleted));

                while ( ver_del == 0 )
                {
                    sleep(1);

                    ver_del = atomic_load(&(list_entry->ver_deleted));
                }

                assert(atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);

                op_info->obj_ver = ver_del;
                op_info->result = LIST_DELETED;
            }
            else
            {
                assert(FALSE);
            }
        }

    } /* end if ( list ) */
    else {
        op_info->result = LIST_DOESNT_EXIST;
    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end search_list_ver() */

/****************************************************************************************
 * Function:    search_class
 *
 * Purpose:     Attempts to search for a property in a property list class.
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a class_entry 
 *              from the class_table is randomly chosen and a prop_entry from the 
 *              class_entry's prop_table is also randomly chosen. If the class has ever 
 *              been created, we set obj_ver as the curr_version of the class and attempt 
 *              to search it for the property as long as the class's struct tag is valid.
 *              
 *              If searching the class was successful, we grab the property from the 
 *              class and use the version of the property as the op_ver and set result as
 *              OP_SUCCESS.
 *              
 *              If searching the class failed we search the index for the class as a 
 *              double check. If the class was in the index, the property wasn't valid 
 *              either by not existing or by being deleted. We get the status of the 
 *              property and check.
 *               *  If the status is DOESNT_EXIST or IN_PROGRESS, we mark result as 
 *                  PROP_DOESNT_EXIST.
 *               *  If the status is EXISTS or DELETED, we search the class for versions
 *                  of the property. If the only versions are after the version we 
 *                  searched the property was created after the version we started 
 *                  searching and thus wasn't a valid property. Result is marked
 *                  PROP_DOESNT_EXIST.
 *               *  If the property has a version during the version we searched it must
 *                  be deleted, so that is double checked and result is marked 
 *                  PROP_DELETED.
 * 
 *              If the searching the class failed and the class wasn't in the index, we 
 *              check the status of the class.
 *               *  If the class was created, but wasn't in the index and has a status
 *                  of IN_PROGRESS, then we searched the class too early while another
 *                  thread was creating it, but before it finished and hadn't inserted
 *                  the class into the index. Set obj_ver and op_ver to 0, op_info->class
 *                  to NULL, and result is marked CLASS_DOESNT_EXIST.
 *               *  If the status is DELETED or CLOSING_IN_PROGRESS we grab the version
 *                  the class was deleted at and set that as obj_ver and mark the result
 *                  as CLASS_DELETED.
 * 
 *              If before we attempt to search the class its struct tag is invalid, we
 *              double check its status is DELETED, CLOSING_IN_PROGRESS, or 
 *              EXISTS_BUT_CLOSED and its obj_ver is set to the version it was deleted 
 *              at, and result is marked as CLASS_DELETED.
 *              NOTE: We include EXISTS_BUT_CLOSED here because we know the class has 
 *              been deleted due to the struct tag having been changed to invalid. 
 *              Meaning that this class was closed while having existing derived objects,
 *              but the last derived object has just been closed, deleting this class and
 *              the thread that did that hasn't gotten to update the status on this class
 *              yet.
 * 
 *              Lastly if the class was not created at all yet, but the time the 
 *              list_entry was grabbed, the result is marked CLASS_DOESNT_EXIST.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
search_class(thread_params_t *thread_params)
{
    class_table_entry_t *class_entry;
    H5P_mt_class_sptr_t  class_sptr;
    H5P_mt_class_t *class;
    hid_t               class_id;
    prop_table_entry_t *prop_entry;
    H5P_mt_prop_t      *prop;
    H5P_mt_prop_aptr_t  next;
    status_t            class_status;
    status_t            prop_status;
    test_op_info_t     *op_info = NULL;
    uint32_t            op_num;
    int                 r;
    uint32_t            nprops;
    uint64_t            value = 0;
    uint64_t            prop_ver;
    uint64_t            curr_ver;
    uint64_t            ver_del;
    herr_t              ret; /* Generic return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = SEARCH;
    op_info->op_num = op_num;

    /* Get a random entry from the list table */
    r = rand() % CLASS_TABLE_SIZE;

    class_entry = &class_table[r];

    atomic_fetch_add(&(class_entry->op_count), 1);

    op_info->class_name  = class_entry->name;
    op_info->parent_name = class_entry->parent_name;
    op_info->test_id     = class_entry->test_class_id;

    if (class_entry->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    class_id    = atomic_load(&(class_entry->id));
    op_info->id = class_id;

    /* Randomly select a possible property in the class to search for */
    nprops = atomic_load(&(class_entry->num_prop_entries));
    r      = rand() % (int)nprops;

    prop_entry = &class_entry->prop_table[r];

    /* Update op_info */
    class_sptr = atomic_load(&(class_entry->class_sptr));
    class      = class_sptr.ptr;

    op_info->class     = class;
    op_info->prop_name = prop_entry->name;

    if (class) {
        class_status = atomic_load(&(class_entry->status));

        if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG ) {

            /* Attempt to search the newest version of the class for a property */
            H5E_BEGIN_TRY
            {
                op_info->obj_ver = atomic_load(&(class->curr_version));
                op_info->op_ver  = op_info->obj_ver;

                if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG )
                {
                    ret = H5P__class_get(class, prop_entry->name, &value);
                }
                else
                {
                    ret = FAIL;
                }
                
            }
            H5E_END_TRY

            if (ret == SUCCEED) {
                /**
                 * NOTE: The exact value of the property is checked after all operations
                 * are done, to not spend more time than necessary not performing an operation
                 * on a list or class to increase likelihood of thread collisions occuring
                 * in H5P for better testing.
                 */
                assert(value > 0);

                curr_ver = atomic_load(&(class->curr_version));

                prop = get_prop_from_lfsll(class->pl_head, prop_entry->name, 
                                            curr_ver);
                CHECK_PTR(prop, "get_prop_from_lfsll");
                assert(prop);

                /* In case the version was updated between grabbing op_ver and now */
                if ( curr_ver > op_info->op_ver )
                {
                    if ( op_info->op_ver < atomic_load(&(prop->create_version)) )
                    {
                        op_info->op_ver = curr_ver;
                    }
                }

                /* Update op_info */
                op_info->prop   = prop;
                op_info->result = OP_SUCCESS;
            }
            /* Searching for the property failed, find why */
            else {
                class_status = atomic_load(&(class_entry->status));

                H5E_BEGIN_TRY
                {
                    class = (H5P_mt_class_t *)H5I_object(class_id);
                }
                H5E_END_TRY

                /* If the search failed and the class is in the index check prop_status */
                if (class) {
                    prop_status = atomic_load(&(prop_entry->status));

                    curr_ver = atomic_load(&(class->curr_version));

                    if (prop_status == DOESNT_EXIST || prop_status == IN_PROGRESS) {
                        op_info->result = PROP_DOESNT_EXIST;
                    }
                    /**
                     * If we failed to find the property, but the status
                     * is EXISTS, check that the prop was created after the
                     * version we searched
                     * If status is DELETED ensure that is correct for the 
                     * version we searched.
                     */
                    else if (prop_status == EXISTS || prop_status == DELETED) {
                        prop = get_prop_from_lfsll(class->pl_head, prop_entry->name,
                                                curr_ver);

                        prop_ver = atomic_load(&(prop->create_version));

                        /* If the property's create_version is > the version we searched */
                        if ( prop_ver > op_info->obj_ver )
                        {
                            /* Check for older versions in the LFSLL */
                            while ( prop_ver > op_info->obj_ver && 
                                    prop->chksum == prop_entry->chksum )
                            {
                                next = atomic_load(&(prop->next));
                                prop = next.ptr;

                                prop_ver = atomic_load(&(prop->create_version));
                            }

                            /**
                             * If iterated to another property, the prop we searched
                             * for at the version we searched at doesn't exist.
                             */
                            if ( prop->chksum != prop_entry->chksum )
                            {
                                op_info->op_ver = op_info->obj_ver;
                                op_info->result = PROP_DOESNT_EXIST;
                            }
                        }
                        
                        /**
                         * If TRUE, there was a version of the prop at the version we
                         * searched, but it must have been deleted. Double check.
                         */
                        if ( prop_ver <= op_info->obj_ver &&
                             prop->chksum == prop_entry->chksum )
                        {
                            prop_ver = atomic_load(&(prop->delete_version));
                            if ( prop_ver > 0 && prop_ver <= op_info->obj_ver )
                            {
                                op_info->op_ver = op_info->obj_ver;
                            }
                            else if ( prop_ver > 0 && prop_ver <= curr_ver )
                            {
                                op_info->op_ver = curr_ver;
                            }
                            else
                            {
                                assert(FALSE);
                            }

                            op_info->result = PROP_DELETED;
                        }  
                        
                        assert(op_info->result != NOT_ATTEMPTED);
                    
                    } /* end else if (prop_status == EXISTS || prop_status == DELETED) */
                    else
                    {
                        assert(FALSE);
                    }

                } /* end if ( class ) */
                /* If the search failed and the class isn't in the index check class_status */
                else {
                    class = op_info->class;

                    class_status = atomic_load(&(class_entry->status));

                    if (class_status == IN_PROGRESS) {
                        op_info->obj_ver = 0;
                        op_info->op_ver  = 0;
                        op_info->class   = NULL;
                        op_info->result  = CLASS_DOESNT_EXIST;
                    }
                    else if ( class_status == DELETED || 
                                class_status == EXISTS_BUT_CLOSED ||
                                class_status == CLOSING_IN_PROGRESS )
                    {
                        /**
                         * If class_entry's->ver_deleted is 0, the closing is still 
                         * in process so sleep and loop till the thread performing 
                         * the close updates ver_deleted, because we must know 
                         * which version the class was deleted at.
                         */
                        ver_del = atomic_load(&(class_entry->ver_deleted));

                        while ( ver_del == 0 )
                        {
                            sleep(1);

                            ver_del = atomic_load(&(class_entry->ver_deleted));
                        }

                        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);

                        op_info->obj_ver = ver_del;
                        op_info->result  = CLASS_DELETED;
                    }
                    else {
                        assert(FALSE);
                    }
                }

            } /* end else */

        } /* if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG )  */
        else {
            assert(class_status == DELETED || class_status == CLOSING_IN_PROGRESS ||
                    class_status == EXISTS_BUT_CLOSED);

            /**
             * If class_entry's->ver_deleted is 0, the closing is still 
             * in process so sleep and loop till the thread performing 
             * the close updates ver_deleted, because we must know 
             * which version the class was deleted at.
             */
            ver_del = atomic_load(&(class_entry->ver_deleted));

            while ( ver_del == 0 )
            {
                sleep(1);

                ver_del = atomic_load(&(class_entry->ver_deleted));
            }

            assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);

            op_info->obj_ver = ver_del;
            op_info->result  = CLASS_DELETED;
        }

    } /* end if ( class ) */
    else {
        op_info->result = CLASS_DOESNT_EXIST;
    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end search_class() */

/****************************************************************************************
 * Function:    search_class_ver
 *
 * Purpose:     Attempts to search for a property in a property list class.
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a class_entry 
 *              from the class_table is randomly chosen and a prop_entry from the 
 *              class_entry's prop_table is also randomly chosen. If the class has ever 
 *              been created, we set obj_ver as the curr_version of the class and if the 
 *              curr_version is > 1 we randomly assign an older version of the class and
 *              attempt to search that version for the property as long as the class's 
 *              struct tag is valid. The version we attempt to search is set as the 
 *              op_ver.
 *              
 *              If searching the class was successful, we grab the property from the 
 *              class and use the version of the property as the op_ver and set result as
 *              OP_SUCCESS.
 *              
 *              If searching the class failed we search the index for the class as a 
 *              double check. If the class was in the index, the property wasn't valid 
 *              either by not existing or by being deleted. We get the status of the 
 *              property and check.
 *               *  If the status is DOESNT_EXIST or IN_PROGRESS, we mark result as 
 *                  PROP_DOESNT_EXIST.
 *               *  If the status is EXISTS or DELETED, we search the class for versions
 *                  of the property. If the only versions are after the version we 
 *                  searched the property was created after the version we started 
 *                  searching and thus wasn't a valid property. Result is marked
 *                  PROP_DOESNT_EXIST.
 *               *  If the property has a version during the version we searched it must
 *                  be deleted, so that is double checked and result is marked 
 *                  PROP_DELETED.
 * 
 *              If the searching the class failed and the class wasn't in the index, we 
 *              check the status of the class.
 *               *  If the class was created, but wasn't in the index and has a status
 *                  of IN_PROGRESS, then we searched the class too early while another
 *                  thread was creating it, but before it finished and hadn't inserted
 *                  the class into the index. Set obj_ver and op_ver to 0, op_info->class
 *                  to NULL, and result is marked CLASS_DOESNT_EXIST.
 *               *  If the status is DELETED or CLOSING_IN_PROGRESS we grab the version
 *                  the class was deleted at and set that as obj_ver and mark the result
 *                  as CLASS_DELETED.
 * 
 *              If before we attempt to search the class its struct tag is invalid, we
 *              double check its status is DELETED, CLOSING_IN_PROGRESS, or 
 *              EXISTS_BUT_CLOSED and its obj_ver is set to the version it was deleted 
 *              at, and result is marked as CLASS_DELETED.
 *              NOTE: We include EXISTS_BUT_CLOSED here because we know the class has 
 *              been deleted due to the struct tag having been changed to invalid. 
 *              Meaning that this class was closed while having existing derived objects,
 *              but the last derived object has just been closed, deleting this class and
 *              the thread that did that hasn't gotten to update the status on this class
 *              yet.
 * 
 *              Lastly if the class was not created at all yet, but the time the 
 *              list_entry was grabbed, the result is marked CLASS_DOESNT_EXIST.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
search_class_ver(thread_params_t *thread_params)
{
    class_table_entry_t *class_entry;
    H5P_mt_class_sptr_t  class_sptr;
    H5P_mt_class_t *class;
    hid_t               class_id;
    prop_table_entry_t *prop_entry;
    H5P_mt_prop_t      *prop = NULL;
    H5P_mt_prop_aptr_t  next;
    status_t            class_status;
    status_t            prop_status;
    test_op_info_t     *op_info = NULL;
    uint32_t            op_num;
    int                 r;
    uint64_t            curr_ver;
    uint64_t            prop_ver;
    uint64_t            search_ver;
    uint64_t            ver_del;
    uint32_t            nprops;

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = SEARCH_VER;
    op_info->op_num = op_num;

    /* Get a random entry from the list table */
    r = rand() % CLASS_TABLE_SIZE;

    class_entry = &class_table[r];

    atomic_fetch_add(&(class_entry->op_count), 1);

    op_info->class_name  = class_entry->name;
    op_info->parent_name = class_entry->parent_name;
    op_info->test_id     = class_entry->test_class_id;

    if (class_entry->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    class_id    = atomic_load(&(class_entry->id));
    op_info->id = class_id;

    /* Randomly select a possible property in the class to search for */
    nprops = atomic_load(&(class_entry->num_prop_entries));
    r      = rand() % (int)nprops;

    prop_entry = &class_entry->prop_table[r];

    /* Update op_info */
    class_sptr = atomic_load(&(class_entry->class_sptr));
    class      = class_sptr.ptr;

    op_info->class     = class;
    op_info->prop_name = prop_entry->name;

    /**
     * If class has been created, attempt to search for a prop in a
     * version older than the most current one (if there is one).
     *
     * NOTE: we must ensure the class exists before attempting to
     * search,  because we need to attempt to grab an older version
     * of the class. If the class doesn't exist, trying to grab an
     * older version will cause a seg fault.
     */
    if (class) {
        class_status = atomic_load(&(class_entry->status));

        if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG ) {
            op_info->obj_ver = atomic_load(&(class->curr_version));

            /* Randomly get which class version to search */
            curr_ver = op_info->obj_ver;

            if (curr_ver > 1) {
                r          = rand();
                search_ver = (uint64_t)r % curr_ver;
                if (search_ver == curr_ver) {
                    search_ver--;
                }
                if (search_ver == 0)
                {
                    search_ver++;
                }
                assert(search_ver < curr_ver);
            }
            else {
                search_ver = curr_ver;
            }

            op_info->op_ver = search_ver;


            H5E_BEGIN_TRY
            {
                /**
                 * NOTE: Must check tag again, due to the chance of the 
                 * class being deleted while entering the H5E_BEGIN_TRY
                 */
                if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG )
                {
                    prop = H5P__mt_search__class(class, prop_entry->name, search_ver);
                }
            }
            H5E_END_TRY

            /* If search was successful */
            if (prop) {
                op_info->prop   = prop;
                op_info->result = OP_SUCCESS;
            }
            /* If search failed, find why */
            else {
                H5E_BEGIN_TRY
                {
                    class = (H5P_mt_class_t *)H5I_object(class_id);
                }
                H5E_END_TRY

                if (class) {
                    prop_status = atomic_load(&(prop_entry->status));

                    curr_ver = atomic_load(&(class->curr_version));

                    if (prop_status == DOESNT_EXIST || prop_status == IN_PROGRESS ) {
                        op_info->result = PROP_DOESNT_EXIST;
                    }
                    /**
                     * If we failed to find the property, but the status
                     * is EXISTS, check that the prop was created after the
                     * version we searched.
                     * If status is DELETED ensure that is correct for the 
                     * version we searched.
                     */
                    else if (prop_status == EXISTS || prop_status == DELETED) {
                        prop = get_prop_from_lfsll(class->pl_head, prop_entry->name,
                                                curr_ver);

                        prop_ver = atomic_load(&(prop->create_version));

                        /* If the property's create_version is > the version we searched */
                        if ( prop_ver > op_info->op_ver )
                        {
                            /* Check for older versions in the LFSLL */
                            while ( prop_ver > op_info->op_ver && 
                                    prop->chksum == prop_entry->chksum )
                            {
                                next = atomic_load(&(prop->next));
                                prop = next.ptr;

                                prop_ver = atomic_load(&(prop->create_version));
                            }

                            /**
                             * If iterated to another property, the prop we searched
                             * for at the version we searched at doesn't exist.
                             */
                            if ( prop->chksum != prop_entry->chksum )
                            {
                                op_info->result = PROP_DOESNT_EXIST;
                            }
                        }
                        
                        /**
                         * If TRUE, there was a version of the prop at the version we
                         * searched, but it must have been deleted. Double check.
                         */
                        if ( prop_ver <= op_info->op_ver &&
                             prop->chksum == prop_entry->chksum )
                        {
                            prop_ver = atomic_load(&(prop->delete_version));
                            if ( prop_ver > 0 && prop_ver <= op_info->op_ver )
                            {
                                op_info->result = PROP_DELETED;
                            }
                            else
                            {
                                assert(FALSE);
                            }

                            
                        } 
                        
                        assert(op_info->result != NOT_ATTEMPTED);
                    
                    } /* end else if (prop_status == EXISTS || prop_status == DELETED) */
                    else
                    {
                        assert(FALSE);
                    }
                }
                else /* ( ! class ) */
                {
                    assert( ! class );

                    class = op_info->class;
                    class_status = atomic_load(&(class_entry->status));

                    if (class_status == IN_PROGRESS ) {
                        op_info->obj_ver = 0;
                        op_info->op_ver  = 0;
                        op_info->class   = NULL;
                        op_info->result  = CLASS_DOESNT_EXIST;
                    }
                    else if ( class_status == DELETED || 
                                class_status == EXISTS_BUT_CLOSED ||
                                class_status == CLOSING_IN_PROGRESS )
                    {
                        /**
                         * If class_entry's->ver_deleted is 0, the closing is still 
                         * in process so sleep and loop till the thread performing 
                         * the close updates ver_deleted, because we must know 
                         * which version the class was deleted at.
                         */
                        ver_del = atomic_load(&(class_entry->ver_deleted));

                        while ( ver_del == 0 )
                        {
                            sleep(1);

                            ver_del = atomic_load(&(class_entry->ver_deleted));
                        }

                        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);

                        op_info->obj_ver = ver_del;
                        op_info->result  = CLASS_DELETED;
                    }
                    else {
                        fprintf(stderr, "\nClass has been created, but isn't in the index\n");
                        fprintf(stderr, "status should be IN_PROGRESS\n");
                        fprintf(stderr, "class_status: %d\n", class_status);
                        assert(FALSE);
                    }
                }

            } /* end else */
        
        } /* end if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG ) */
        else {
            if (class_status == DELETED || class_status == CLOSING_IN_PROGRESS ||
                        class_status == EXISTS_BUT_CLOSED) {
                /**
                 * If class_entry's->ver_deleted is 0, the closing is still 
                 * in process so sleep and loop till the thread performing 
                 * the close updates ver_deleted, because we must know 
                 * which version the class was deleted at.
                 */
                ver_del = atomic_load(&(class_entry->ver_deleted));

                while ( ver_del == 0 )
                {
                    sleep(1);

                    ver_del = atomic_load(&(class_entry->ver_deleted));
                }

                assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);

                op_info->obj_ver = ver_del;
                op_info->result  = CLASS_DELETED;
            }
            else {
                assert(FALSE);
            }

            op_info->obj_ver = atomic_load(&(class->curr_version));
            op_info->result = CLASS_DELETED;
        }

    } /* end if ( class ) */
    else {
        op_info->result = CLASS_DOESNT_EXIST;
    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end search_class_ver() */

/****************************************************************************************
 * Function:    mod_list_create_prop
 *
 * Purpose:     Attempts to create a new property and insert it into a property list.
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a list_entry 
 *              from the list_table is randomly chosen and a prop_entry from the 
 *              list_entry's prop_table is also randomly chosen. If the list has ever 
 *              been created, we iterate the list_entry's prop_table to the next property
 *              that has the status of DOESNT_EXIST and atomically update its status to
 *              IN_PROGRESS and attempt to create it.
 *              NOTE: If there are no entries in the prop_table with the status of 
 *              DOESNT_EXIST, we randomly grab an entry in the prop_table and attempt to
 *              create it. This is done because the iterating the prop_table and creating
 *              only the ones that haven't been created yet allows more operations to 
 *              actually succeed allowing for more potential thread collisions in the
 *              H5P objects for testing. But after all have been created it allows for 
 *              testing of creating a new property to fail if it already exists in the
 *              list and if the property has been deleted for it to test that creating
 *              it again will succeed
 * 
 *              Immediately before we attempt to the create the property, we atomically
 *              set obj_ver to the list's curr_version and op_ver to the list's 
 *              next_version. This is so the value of the property is equal (or close to)
 *              its create_version, making checking that properties have the correct 
 *              value easier.
 *              
 *              If creating the property was successful, we atomically update the status 
 *              of the prop_entry to EXISTS, and grab the property from the list. We 
 *              must double check the op_ver for if another operation incremented the 
 *              list's version after we grabbed it but before creating the property. Then
 *              set the op_info->prop to point to the property and mark result as 
 *              OP_SUCCESS.
 *              
 *              If creating the property failed we search the index for the list as a 
 *              double check. If the list was in the index, the property must have 
 *              already existed, so check it's status. 
 *               *  If the status is EXISTS, IN_PROGRESS, or DELETED we double check that
 *                  the property was valid (meaning created and not deleted) at the 
 *                  version of the list we attempted to create the property at, and mark
 *                  the result as PROP_ALREADY_EXISTS.
 *              NOTE: If the status is DELETED, but we failed to create the property, it
 *              must have been deleted directly after our create attempt. We double check
 *              that, but otherwise the create would have succeeded.
 * 
 *              If creating the property failed and the list wasn't in the index, we 
 *              check the status of the list.
 *               *  If the list was created, but wasn't in the index and has a status
 *                  of IN_PROGRESS, then another thread was creating it, but hadn't 
 *                  finished and hadn't inserted the list into the index. Set obj_ver and 
 *                  op_ver to 0, op_info->list to NULL, and the result is marked 
 *                  LIST_DOESNT_EXIST.
 *               *  If the status is DELETED or CLOSING_IN_PROGRESS we grab the version
 *                  the list was deleted at and set that as obj_ver and mark the result
 *                  as LIST_DELETED.
 * 
 *              If before we attempt to create the property the list's struct tag was 
 *              invalid, we double check it was deleted and its obj_ver is set to the 
 *              version it was deleted at, and result is marked as LIST_DELETED.
 * 
 *              Lastly if the list was not created at all yet, but the time the 
 *              list_entry was grabbed, the result is marked LIST_DOESNT_EXIST.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
mod_list_create_prop(thread_params_t *thread_params)
{
    list_table_entry_t *list_entry;
    H5P_mt_list_sptr_t  list_sptr;
    H5P_mt_list_t      *list;
    H5P_mt_prop_t      *prop;
    hid_t               list_id;
    prop_table_entry_t *prop_entry;
    status_t            list_status;
    status_t            prop_status;
    status_t            update_status;
    //status_t            prop_start_status;
    test_op_info_t     *op_info;
    uint32_t            op_num;
    int                 r = 0;
    uint32_t            nprops;
    uint32_t            i = 0;
    uint64_t            curr_ver;
    uint64_t            prop_ver;
    uint64_t            ver_del;
    bool                done       = FALSE;
    bool                loop_check = FALSE;
    bool                base_flag  = FALSE;
    herr_t              ret; /* Generic return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = MOD_CREATE_PROP;
    op_info->op_num = op_num;

    /* Get a random entry from the list table */
    r = rand() % LIST_TABLE_SIZE;

    list_entry = &list_table[r];

    atomic_fetch_add(&(list_entry->op_count), 1);

    op_info->parent_name = list_entry->parent_name;
    op_info->test_id     = list_entry->test_list_id;

    if (list_entry->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    list_id     = atomic_load(&(list_entry->id));
    op_info->id = list_id;

    list_sptr = atomic_load(&(list_entry->list_sptr));
    list      = list_sptr.ptr;

    op_info->list = list;

    list_status = atomic_load(&(list_entry->status));

    if (list) {
        /** NOTE:
         * If the list exists and is IN_PROGRESS it may still be
         * updating the prop_table statuses. Wait and check again.
         */
        while (list_status == IN_PROGRESS) {
            sleep(1);

            list_status = atomic_load(&(list_entry->status));
        }

        /* Get the next new property to create */
        nprops = atomic_load(&(list_entry->num_prop_entries));

        while (i < nprops) {
            prop_entry  = &list_entry->prop_table[i];
            prop_status = atomic_load(&(prop_entry->status));
            //prop_start_status = prop_status;

            if (prop_status == DOESNT_EXIST) {
                update_status = IN_PROGRESS;

                /* Attempt to atomically update prop_status */
                if (!atomic_compare_exchange_strong(&(prop_entry->status), &prop_status, update_status)) {
                    /* Atomic update failed, check same prop_entry again */
                    if (loop_check) {
                        assert(FALSE);
                    }
                    else {
                        loop_check = TRUE;
                    }
                }
                else {
                    /* Atomic update successful */
                    done = TRUE;
                    break;
                }
            }
            else {
                i++;
            }

        } /* end while ( i < nprops ) */

        /**
         * If we've iterated the entire table of possible properties for the
         * list, grab a random prop_entry and attempt to create it. This will
         * test that if a prop already exists in a list creating it should fail,
         * and if that prop is deleted, it should be able to be created as a new
         * property again.
         */
        if (!done) {
            r = rand() % (int)nprops;

            prop_entry  = &list_entry->prop_table[r];
            prop_status = atomic_load(&(prop_entry->status));

            done = TRUE;
        }

        op_info->prop_name = prop_entry->name;

/** 
 * TODO:
 * add a stat to track how many times this occurs, it shows that there 
 * is more than one thread attempting to modify the list in someway. 
 */
#if 0
        assert(op_info->obj_ver + 1 == op_info->op_ver);
#endif
        /* Attempt to create the new property */
        H5E_BEGIN_TRY
        {
            op_info->obj_ver = atomic_load(&(list->curr_version));
            op_info->op_ver  = atomic_load(&(list->next_version));

            ret = H5Pinsert2(list_id, prop_entry->name, sizeof(op_info->op_ver), &op_info->op_ver, NULL, NULL,
                             NULL, NULL, NULL, NULL);
        }
        H5E_END_TRY

        if (ret == SUCCEED) {

            /* Update prop_status */
            update_status = EXISTS;
            done          = FALSE;
            do {
                prop_status = atomic_load(&(prop_entry->status));

                /* Attempt to atomically update prop_status */
                if (!atomic_compare_exchange_strong(&(prop_entry->status), &prop_status, update_status)) {
                    assert(prop_status == IN_PROGRESS);
                }
                else {
                    done = TRUE;
                }

            } while (!done);

            curr_ver = atomic_load(&(list->curr_version));

            prop = get_prop_from_lkup_tbl(list, prop_entry->name, curr_ver,
                                            &base_flag);

            if ( ! prop )
            {
                prop = get_prop_from_lfsll(list->pl_head, prop_entry->name,
                                            curr_ver);
            }
            CHECK_PTR(prop, "get_prop_from_lkup_tbl/get_prop_from_lfsll");
            assert(prop);

            /** 
             * If curr_ver is greater then another thread performed an operation on the
             * list after this thread grabbed the version and maybe before creating the 
             * prop. Double check
             */
            if ( curr_ver > op_info->op_ver )
            {
                if ( op_info->op_ver < atomic_load(&(prop->create_version)) )
                {
                    /** TODO: add stat for this */
                    op_info->op_ver = curr_ver;
                }
            }

            op_info->prop   = prop;
            op_info->result = OP_SUCCESS;
        }
        /* Creating the prop failed, check why */
        else {
            H5E_BEGIN_TRY
            {
                list = (H5P_mt_list_t *)H5I_object(list_id);
            }
            H5E_END_TRY

            list_status = atomic_load(&(list_entry->status));

            /* If list is in the index, check if the prop is already in the list */
            if (list) {
                prop_status = atomic_load(&(prop_entry->status));

                curr_ver = atomic_load(&(list->curr_version));

                /* Check the lkup_tbl for the prop */
                prop = get_prop_from_lkup_tbl(list, prop_entry->name, curr_ver,
                                                &base_flag);

                /* If not in the lkup_tbl check the LFSLL */
                if ( ! prop )
                {
                    prop = get_prop_from_lfsll(list->pl_head, prop_entry->name, curr_ver);
                }

                if ( prop )
                {
                    /* Ensure the property does exist at the version we tried */
                    if ( prop_status == EXISTS || prop_status == IN_PROGRESS ||
                         prop_status == DELETED )
                    {
                        prop_ver = atomic_load(&(prop->delete_version));

                        if ( prop_ver > op_info->obj_ver )
                        {
                            assert(prop_ver == 0 || prop_ver >= curr_ver);
                            op_info->op_ver = curr_ver;
                        }
                        else
                        {
                            op_info->op_ver = op_info->obj_ver;
                        }
                        
                        op_info->result  = PROP_ALREADY_EXISTS;
                    }
                    else
                    {
                        /* No other status should be possible */
                        assert(FALSE);
                    }
                }
                else
                {
                    /* If the prop doesn't exist, creating it should've succeed */
                    assert(FALSE);
                }

            }
            /* The list wasn't in the index */
            else {
                list = op_info->list;

                if (list_status == DELETED || list_status == CLOSING_IN_PROGRESS) {
                    /**
                     * If list_entry's->ver_deleted is 0, the closing is still 
                     * in process so sleep and loop till the thread performing 
                     * the close updates ver_deleted, because we must know 
                     * which version the list was deleted at.
                     */
                    ver_del = atomic_load(&(list_entry->ver_deleted));

                    while ( ver_del == 0 )
                    {
                        sleep(1);

                        ver_del = atomic_load(&(list_entry->ver_deleted));
                    }

                    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);

                    op_info->obj_ver = ver_del;
                    op_info->result  = LIST_DELETED;
                }
                else if (list_status == IN_PROGRESS) {
                    op_info->obj_ver = 0;
                    op_info->op_ver  = 0;
                    op_info->list    = NULL;
                    op_info->result  = LIST_DOESNT_EXIST;
                }
                else {
                    assert(FALSE);
                }
            }
        }

    } /* end if ( list ) */
    else {
        op_info->result = LIST_DOESNT_EXIST;
    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end mod_list_create_prop() */

/****************************************************************************************
 * Function:    mod_list_mod_prop
 *
 * Purpose:     Attempts to modify a property in a property list (create a new version of
 *              the property structure with the updated value).
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a list_entry 
 *              from the list_table is randomly chosen. If the list has ever been 
 *              created, we randomly grab a prop_entry from the list_entry's prop_table
 *              and attempt to set a new value.
 * 
 *              Immediately before we attempt to set the new value, we atomically
 *              set obj_ver to the list's curr_version and op_ver to the list's 
 *              next_version. This is so the value of the property is equal (or close to)
 *              its create_version, making checking that properties have the correct 
 *              value easier. 
 *              NOTE: op_ver is the new value.
 *              
 *              If creating the property was successful, we grab the property from the 
 *              list. We must double check the op_ver for if another operation 
 *              incremented the list's version after we grabbed it but before modifying 
 *              the property. Then set the op_info->prop to point to the property and 
 *              mark result as OP_SUCCESS.
 *              
 *              If creating the property failed we search the index for the list as a 
 *              double check. If the list was in the index, check the prop's status. 
 *               *  If status is DOESNT_EXIST or IN_PROGRESS and we failed to modify it
 *                  the property didn't exist in the list yet. Set op_ver to obj_ver 
 *                  because the operation didn't occur so this didn't increment the 
 *                  list's version, and mark result as PROP_DOESNT_EXIST.
 *               *  If the status is EXISTS or DELETED we double check that the property 
 *                  was not valid (meaning deleted or not created) at the version of the 
 *                  list we attempted to modify the property at. Set the op_ver to 
 *                  obj_ver and mark the result as PROP_DELETED or PROP_DOESNT_EXISTS as
 *                  appropriate.
 * 
 *              If modifying the property failed and the list wasn't in the index, we 
 *              check the status of the list.
 *               *  If the list was created, but wasn't in the index and has a status
 *                  of IN_PROGRESS, then another thread was creating it, but hadn't 
 *                  finished and hadn't inserted the list into the index. Set obj_ver and 
 *                  op_ver to 0, op_info->list to NULL, and the result is marked 
 *                  LIST_DOESNT_EXIST.
 *               *  If the status is DELETED or CLOSING_IN_PROGRESS we grab the version
 *                  the list was deleted at and set that as obj_ver and mark the result
 *                  as LIST_DELETED.
 * 
 *              Lastly if the list was not created at all yet, but the time the 
 *              list_entry was grabbed, the result is marked LIST_DOESNT_EXIST.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
mod_list_mod_prop(thread_params_t *thread_params)
{
    list_table_entry_t *list_entry;
    H5P_mt_list_sptr_t  list_sptr;
    H5P_mt_list_t      *list;
    //H5P_mt_list_table_entry_t *lkup_entry;
    //H5P_mt_list_prop_ref_t prop_ref;
    H5P_mt_prop_t      *prop = NULL;
    //H5P_mt_prop_aptr_t  next;
    hid_t               list_id;
    prop_table_entry_t *prop_entry;
    status_t            list_status;
    status_t            prop_status;
    test_op_info_t     *op_info;
    uint32_t            op_num;
    int                 r;
    uint32_t            nprops;
    uint64_t            curr_ver;
    uint64_t            prop_ver;
    //uint64_t            first_curr;
    //uint64_t            base_del_ver;
    uint64_t            ver_del;
    bool                base_flag = FALSE;
    bool                deleted   = FALSE;
    herr_t              ret; /* Generic return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = MOD_MOD_PROP;
    op_info->op_num = op_num;

    /* Get a random entry from the list table */
    r = rand() % LIST_TABLE_SIZE;

    list_entry = &list_table[r];

    atomic_fetch_add(&(list_entry->op_count), 1);

    op_info->parent_name = list_entry->parent_name;
    op_info->test_id     = list_entry->test_list_id;

    if (list_entry->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    list_id     = atomic_load(&(list_entry->id));
    op_info->id = list_id;

    list_sptr = atomic_load(&(list_entry->list_sptr));
    list      = list_sptr.ptr;

    op_info->list = list;

    list_status = atomic_load(&(list_entry->status));

    if (list) {
        /* Randomly select a possible property to attempt to create a new version of */
        nprops = atomic_load(&(list_entry->num_prop_entries));
        r      = rand() % (int)nprops;

        prop_entry  = &list_entry->prop_table[r];
        prop_status = atomic_load(&(prop_entry->status));

        op_info->prop_name = prop_entry->name;

        /* Attempt to create the new version of the property */
        H5E_BEGIN_TRY
        {
            op_info->obj_ver   = atomic_load(&(list->curr_version));
            op_info->op_ver    = atomic_load(&(list->next_version));

            ret = H5Pset(list_id, prop_entry->name, &op_info->op_ver);
        }
        H5E_END_TRY

        if (ret == SUCCEED) {

            curr_ver = atomic_load(&(list->curr_version));

            prop = get_prop_from_lkup_tbl(list, prop_entry->name, curr_ver,
                                            &base_flag);

            if ( ! prop )
            {
                prop = get_prop_from_lfsll(list->pl_head, prop_entry->name, 
                                            curr_ver);
            }
            CHECK_PTR(prop, "get_prop_from_lkup_tbl/get_prop_from_lfsll");
            assert(prop);
            
            assert(base_flag == FALSE);

            /* If create_version is greater than op_ver, update op_ver */
            prop_ver = atomic_load(&(prop->create_version));

            if ( prop_ver > op_info->op_ver )
            {
                op_info->op_ver = prop_ver;
            }

            op_info->prop   = prop;
            op_info->result = OP_SUCCESS;
        }
        /* Failed creating a new version of the property, find why */
        else {
            H5E_BEGIN_TRY
            {
                list = (H5P_mt_list_t *)H5I_object(list_id);
            }
            H5E_END_TRY

            /* If the list is in the index check the prop_status */
            if (list) {
                prop_status = atomic_load(&(prop_entry->status));

                curr_ver = atomic_load(&(list->curr_version));

                if (prop_status == DOESNT_EXIST || prop_status == IN_PROGRESS) {
                    op_info->op_ver = op_info->obj_ver;
                    op_info->result = PROP_DOESNT_EXIST;
                }
                /**
                 * If we failed to create the new version, but the status
                 * is EXISTS, check that the prop was created after the
                 * version we attempted to modify.
                 * If status is DELETED ensure that is correct for the 
                 * version we attempted to modify.
                 */
                else if (prop_status == EXISTS || prop_status == DELETED) 
                {
#if 1
                    prop_ver = prop_ver_check_list(list, prop_entry, 
                                    op_info->obj_ver, curr_ver, &deleted, FALSE);

                    if ( deleted )
                    {
                        assert(prop_ver > 0);
                        op_info->op_ver = prop_ver;
                        op_info->result  = PROP_DELETED;

                        prop = get_prop_from_lkup_tbl(list, prop_entry->name, curr_ver,
                                                      &base_flag);
                        if ( ! prop )
                        {
                            prop = get_prop_from_lfsll(list->pl_head, prop_entry->name, 
                                                        curr_ver);
                        }
                        CHECK_PTR(prop, "get_prop_from_lkup_tbl/get_prop_from_lfsll");
                        assert(prop);

                        op_info->prop = prop;
                    }
                    else
                    {
                        assert(prop_ver == 0);
                        op_info->op_ver = op_info->obj_ver;
                        op_info->result = PROP_DOESNT_EXIST;
                    }    
#else
                    /* Check the list's lkup_tbl for the property */
                    for ( size_t i = 0; i < list->nprops_inherited; i++ )
                    {
                        lkup_entry = &list->lkup_tbl[i];

                        if ( lkup_entry->chksum == prop_entry->chksum )
                        {
                            first_curr = atomic_load(&(lkup_entry->first_ver_of_curr));

                            /**
                             * If there is a first version of curr and it's > the
                             * version we tried to modify, the base is our version.
                             */
                            if ( first_curr == 0 ||
                                    first_curr > op_info->obj_ver )
                            {
                                base_del_ver = atomic_load(&(lkup_entry->base_delete_version));

                                /**
                                 * If the base is deleted for the version we 
                                 * tried to modify, our prop is in fact deleted.
                                 */
                                if ( base_del_ver > 0 && 
                                        base_del_ver <= op_info->obj_ver )
                                {
                                    op_info->op_ver = op_info->obj_ver;
                                    op_info->result = PROP_DELETED;
                                    break;
                                }
                                else
                                {
                                    prop_ref = atomic_load(&(lkup_entry->base));
                                    prop = prop_ref.ptr;
                                    break;
                                }                                    
                            }
                            else
                            {
                                prop_ref = atomic_load(&(lkup_entry->curr));
                                prop = prop_ref.ptr;
                                break;
                            }
                        
                        } /* end if ( lkup_entry->chksum == prop_entry->chksum ) */
                    
                    } /* for ( size_t i = 0; i < list->nprops_inherited; i++ ) */ 

                    /* If we didn't already find the deleted prop in the lkup_tbl */
                    if ( op_info->result == NOT_ATTEMPTED )
                    {
                        /* If there wasn't an entry for the prop in the lkup_tbl */
                        if ( ! prop )
                        {
                            prop = get_prop_from_lfsll(list->pl_head, prop_entry->name, 
                                                        curr_ver);
                        }
                        CHECK_PTR(prop, "grabbed prop from lkup_tbl/get_prop_from_lfsll");
                        assert(prop);
                        
                        prop_ver = atomic_load(&(prop->create_version));

                        /* If the property's create_version is > the version we tried to modify */
                        if ( prop_ver > op_info->obj_ver )
                        {
                            /* Check for older versions in the LFSLL */
                            while ( prop_ver > op_info->obj_ver && 
                                    prop->chksum == prop_entry->chksum )
                            {
                                next = atomic_load(&(prop->next));
                                prop = next.ptr;

                                prop_ver = atomic_load(&(prop->create_version));
                            }

                            /**
                             * If iterated to another property, the prop we tried 
                             * to modify at the version we tried doesn't exist.
                             */
                            if ( prop->chksum != prop_entry->chksum )
                            {
                                op_info->op_ver = op_info->obj_ver;
                                op_info->result = PROP_DOESNT_EXIST;
                            }
                        }

                        /**
                         * If TRUE, there was a version of the prop at the version we
                         * tried to modify, but it must have been deleted. Double check.
                         */
                        if ( prop_ver <= op_info->obj_ver &&
                                prop->chksum == prop_entry->chksum )
                        {
                            prop_ver = atomic_load(&(prop->delete_version));
                            if ( prop_ver > 0 && prop_ver <= op_info->obj_ver )
                            {
                                op_info->op_ver = op_info->obj_ver;
                            }
                            else if ( prop_ver > 0 && prop_ver <= curr_ver )
                            {
                                op_info->op_ver = curr_ver;
                            }
                            else
                            {
                                assert(FALSE);
                            }

                            op_info->result = PROP_DELETED;
                        }
                    
                    } /* end if ( op_info->result == NOT_ATTEMPTED ) */
                    else
                    {
                        assert(op_info->result == PROP_DELETED);
                    }
#endif
                } /* end else if (prop_status == EXISTS || prop_status == DELETED) */
            }
            else {
                list = op_info->list;

                list_status = atomic_load(&(list_entry->status));

                if (list_status == DELETED || list_status == CLOSING_IN_PROGRESS) {
                    /**
                     * If list_entry's->ver_deleted is 0, the closing is still 
                     * in process so sleep and loop till the thread performing 
                     * the close updates ver_deleted, because we must know 
                     * which version the list was deleted at.
                     */
                    ver_del = atomic_load(&(list_entry->ver_deleted));

                    while ( ver_del == 0 )
                    {
                        sleep(1);

                        ver_del = atomic_load(&(list_entry->ver_deleted));
                    }

                    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);

                    op_info->obj_ver = ver_del;
                    op_info->result  = LIST_DELETED;
                }
                else if (list_status == IN_PROGRESS) {
                    op_info->obj_ver = 0;
                    op_info->op_ver  = 0;
                    op_info->list    = NULL;
                    op_info->result  = LIST_DOESNT_EXIST;
                }
                else {
                    assert(FALSE);
                }
            }

        } /* end else ( ret != SUCCEED ) */

    } /* end if ( list ) */
    else {
        op_info->result = LIST_DOESNT_EXIST;
    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end mod_list_mod_prop() */

/****************************************************************************************
 * Function:    mod_list_delete_prop
 *
 * Purpose:     Attempts to delete a property in a property list.
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a list_entry 
 *              from the list_table is randomly chosen. If the list has ever been 
 *              created, we randomly grab a prop_entry from the list_entry's prop_table
 *              and attempt to delete it.
 * 
 *              Immediately before we attempt to delete the prop, we atomically set 
 *              obj_ver to the list's curr_version and op_ver to the list's next_version. 
 *              
 *              If deleting the property was successful, we atomically update the 
 *              prop_entry's status to DELETED and grab the property from the list. We 
 *              must double check the op_ver for if another operation incremented the 
 *              list's version after we grabbed it but before deleting the property.
 *              Then set the op_info->prop to point to the property and mark result as 
 *              OP_SUCCESS.
 *              
 *              If creating the property failed we search the index for the list as a 
 *              double check. If the list was in the index, check the prop's status. 
 *               *  If status is DOESNT_EXIST or IN_PROGRESS and we failed to modify it
 *                  the property didn't exist in the list yet. Set op_ver to obj_ver 
 *                  because the operation didn't occur so this didn't increment the 
 *                  list's version, and mark result as PROP_DOESNT_EXIST.
 *               *  If the status is EXISTS or DELETED we double check that the property 
 *                  was not valid (meaning deleted or not created) at the version of the 
 *                  list we attempted to modify the property at. Set the op_ver to 
 *                  obj_ver and mark the result as PROP_DELETED or PROP_DOESNT_EXISTS as
 *                  appropriate.
 * 
 *              If modifying the property failed and the list wasn't in the index, we 
 *              check the status of the list.
 *               *  If the list was created, but wasn't in the index and has a status
 *                  of IN_PROGRESS, then another thread was creating it, but hadn't 
 *                  finished and hadn't inserted the list into the index. Set obj_ver and 
 *                  op_ver to 0, op_info->list to NULL, and the result is marked 
 *                  LIST_DOESNT_EXIST.
 *               *  If the status is DELETED or CLOSING_IN_PROGRESS we grab the version
 *                  the list was deleted at and set that as obj_ver and mark the result
 *                  as LIST_DELETED.
 * 
 *              Lastly if the list was not created at all yet, but the time the 
 *              list_entry was grabbed, the result is marked LIST_DOESNT_EXIST.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
mod_list_delete_prop(thread_params_t *thread_params)
{
    list_table_entry_t *list_entry;
    H5P_mt_list_sptr_t  list_sptr;
    H5P_mt_list_t      *list;
    //H5P_mt_list_table_entry_t *lkup_entry;
    //H5P_mt_list_prop_ref_t prop_ref;
    H5P_mt_prop_t      *prop = NULL;
    //H5P_mt_prop_aptr_t  next;
    hid_t               list_id;
    prop_table_entry_t *prop_entry;
    status_t            list_status;
    status_t            prop_status;
    status_t            update_status;
    test_op_info_t     *op_info;
    uint32_t            op_num;
    int                 r;
    uint32_t            nprops;
    uint64_t            curr_ver;
    uint64_t            prop_ver;
    //uint64_t            first_curr;
    //uint64_t            base_del_ver;
    uint64_t            ver_del;
    bool                done      = FALSE;
    bool                base_flag = FALSE;
    bool                deleted   = FALSE;
    bool                loop_check = FALSE;
    herr_t              ret; /* Generic return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = MOD_DELETE_PROP;
    op_info->op_num = op_num;

    /* Get a random entry from the list table */
    r = rand() % LIST_TABLE_SIZE;   

    list_entry = &list_table[r];

    atomic_fetch_add(&(list_entry->op_count), 1);

    op_info->parent_name = list_entry->parent_name;
    op_info->test_id     = list_entry->test_list_id;

    if (list_entry->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    list_id     = atomic_load(&(list_entry->id));
    op_info->id = list_id;

    list_sptr = atomic_load(&(list_entry->list_sptr));
    list      = list_sptr.ptr;

    op_info->list = list;

    list_status = atomic_load(&(list_entry->status));

    if (list) {
        /* Randomly select a possible property to attempt to delete */
        nprops = atomic_load(&(list_entry->num_prop_entries));
        r      = rand() % (int)nprops;

        prop_entry  = &list_entry->prop_table[r];
        prop_status = atomic_load(&(prop_entry->status));

        op_info->prop_name = prop_entry->name;

        /* Attempt to delete the property */
        H5E_BEGIN_TRY
        {
            op_info->obj_ver = atomic_load(&(list->curr_version));
            op_info->op_ver  = atomic_load(&(list->next_version));
            
            ret = H5Premove(list_id, prop_entry->name);
        }
        H5E_END_TRY

        if (ret == SUCCEED) {
            curr_ver = atomic_load(&(list->curr_version));

            /* Update prop_status */
            update_status = DELETED;
            done          = FALSE;
            do {
                prop_status = atomic_load(&(prop_entry->status));

                /* Attempt to atomically update prop_status */
                if (!atomic_compare_exchange_strong(&(prop_entry->status), 
                                                    &prop_status, update_status)) {

                    assert(prop_status == IN_PROGRESS || prop_status == EXISTS ||
                            prop_status == DOESNT_EXIST);
                    if (loop_check) {
                        assert(FALSE);
                    }
                    else {
                        loop_check = TRUE;
                    }
                }
                else {
                    prop_status = atomic_load(&(prop_entry->status));
                    assert(prop_status == DELETED);

                    done = TRUE;
                }

            } while (!done);

#if 1
            prop_ver = prop_ver_check_list(list, prop_entry, 
                            op_info->op_ver, curr_ver, &deleted, TRUE);

            assert(deleted);

            prop = get_prop_from_lkup_tbl(list, prop_entry->name, prop_ver, &base_flag);

            if ( ! prop )
            {
                prop = get_prop_from_lfsll(list->pl_head, prop_entry->name, prop_ver);
            }
            CHECK_PTR(prop, "grabbed prop from lkup_tbl/get_prop_from_lfsll");
            assert(prop);
#else

            //prop = get_prop_from_lkup_tbl(list, prop_entry->name, curr_ver,
            //                                &base_flag);
            /* Check the list's lkup_tbl for the property */
            for ( size_t i = 0; i < list->nprops_inherited; i++ )
            {
                lkup_entry = &list->lkup_tbl[i];

                if ( lkup_entry->chksum == prop_entry->chksum )
                {
                    first_curr = atomic_load(&(lkup_entry->first_ver_of_curr));

                    /**
                     * If there is a first version of curr and it's > the
                     * version we searched for, the base is our version.
                     */
                    if ( first_curr == 0 ||
                            first_curr > op_info->obj_ver )
                    {
                        base_del_ver = atomic_load(&(lkup_entry->base_delete_version));

                        /**
                         * If the base is deleted for the version we 
                         * searched our prop is in fact deleted.
                         */
                        if ( base_del_ver > 0 && 
                                base_del_ver <= op_info->obj_ver )
                        {
                            op_info->op_ver = op_info->obj_ver;
                            op_info->result = PROP_DELETED;
                            break;
                        }
                        else if ( base_del_ver > 0 &&
                                  base_del_ver <= curr_ver )
                        {
                            op_info->op_ver = curr_ver;
                            op_info->result = PROP_DELETED;
                            break;
                        }
                        else
                        {
                            prop_ref = atomic_load(&(lkup_entry->base));
                            prop = prop_ref.ptr;
                            break;
                        }                                    
                    }
                    else
                    {
                        prop_ref = atomic_load(&(lkup_entry->curr));
                        prop = prop_ref.ptr;
                        break;
                    }
                
                } /* end if ( lkup_entry->chksum == prop_entry->chksum ) */
            
            } /* for ( size_t i = 0; i < list->nprops_inherited; i++ ) */ 

            /* If we didn't find the deleted prop in the lkup_tbl */
            if ( op_info->result == NOT_ATTEMPTED )
            {
                /* If we don't have a prop pointer from the lkup_tbl*/
                if ( ! prop )
                {
                    prop = get_prop_from_lfsll(list->pl_head, prop_entry->name, 
                                                curr_ver);
                }
                CHECK_PTR(prop, "get_prop_from_lkup_tbl/get_prop_from_lfsll");
                assert(prop);

                prop_ver = atomic_load(&(prop->delete_version));
                assert(prop_ver > 0);

                if ( prop_ver > op_info->op_ver && prop_ver <= curr_ver )
                {
                        /** TODO: add stat for this */
                        op_info->op_ver = prop_ver;
                }
            }
#endif
            op_info->op_ver = prop_ver;
            op_info->prop   = prop;
            op_info->result = OP_SUCCESS;
        }
        /* Failed deleting the property, find why */
        else {
            H5E_BEGIN_TRY
            {
                list = (H5P_mt_list_t *)H5I_object(list_id);
            }
            H5E_END_TRY

            /* If the list is in the index check the prop_status */
            if (list) {
                prop_status = atomic_load(&(prop_entry->status));

                curr_ver = atomic_load(&(list->curr_version));

                if (prop_status == DOESNT_EXIST || prop_status == IN_PROGRESS) {
                    op_info->op_ver = op_info->obj_ver;
                    op_info->result = PROP_DOESNT_EXIST;
                }
                /**
                 * If we failed to delete the property, but the status
                 * is EXISTS, check that the prop was created after the
                 * version we attempted.
                 * If status is DELETED ensure that is correct for the 
                 * version we attempted.
                 */
                else if (prop_status == EXISTS || prop_status == DELETED) 
                {
#if 1
                    prop_ver = prop_ver_check_list(list, prop_entry, 
                                    op_info->obj_ver, curr_ver, &deleted, FALSE);

                    if ( deleted )
                    {
                        assert(prop_ver > 0);
                        op_info->op_ver = prop_ver;
                        op_info->result  = PROP_DELETED;
                    }
                    else
                    {
                        assert(prop_ver == 0);
                        op_info->op_ver = op_info->obj_ver;
                        op_info->result = PROP_DOESNT_EXIST;
                    }
#else
                    /* Check the list's lkup_tbl for the property */
                    for ( size_t i = 0; i < list->nprops_inherited; i++ )
                    {
                        lkup_entry = &list->lkup_tbl[i];

                        if ( lkup_entry->chksum == prop_entry->chksum )
                        {
                            first_curr = atomic_load(&(lkup_entry->first_ver_of_curr));

                            /**
                             * If there is a first version of curr and it's > the
                             * version we searched for, the base is our version.
                             */
                            if ( first_curr == 0 ||
                                    first_curr > op_info->obj_ver )
                            {
                                base_del_ver = atomic_load(&(lkup_entry->base_delete_version));

                                /**
                                 * If the base is deleted for the version we 
                                 * searched our prop is in fact deleted.
                                 */
                                if ( base_del_ver > 0 && 
                                        base_del_ver <= op_info->obj_ver )
                                {
                                    op_info->op_ver = op_info->obj_ver;
                                    op_info->result = PROP_DELETED;
                                    break;
                                }
                                else if ( base_del_ver > 0 &&
                                        base_del_ver <= curr_ver )
                                {
                                    op_info->op_ver = curr_ver;
                                    op_info->result = PROP_DELETED;
                                    break;
                                }
                                else
                                {
                                    prop_ref = atomic_load(&(lkup_entry->base));
                                    prop = prop_ref.ptr;
                                    break;
                                }                                    
                            }
                            else
                            {
                                prop_ref = atomic_load(&(lkup_entry->curr));
                                prop = prop_ref.ptr;
                                break;
                            }
                        
                        } /* end if ( lkup_entry->chksum == prop_entry->chksum ) */
                    
                    } /* for ( size_t i = 0; i < list->nprops_inherited; i++ ) */ 

                    /* If we didn't find the prop deleted in the lkup_tbl */
                    if ( op_info->result == NOT_ATTEMPTED )
                    {
                        /* If there wasn't an entry for the prop in the lkup_tbl */
                        if ( ! prop )
                        {
                            prop = get_prop_from_lfsll(list->pl_head, prop_entry->name, 
                                                        curr_ver);
                        }
                        CHECK_PTR(prop, "grabbed prop from lkup_tbl/get_prop_from_lfsll");
                        assert(prop);
                        
                        prop_ver = atomic_load(&(prop->create_version));

                        /* If the property's create_version is > the version we searched */
                        if ( prop_ver > op_info->obj_ver )
                        {
                            /* Check for older versions in the LFSLL */
                            while ( prop_ver > op_info->obj_ver && 
                                    prop->chksum == prop_entry->chksum )
                            {
                                next = atomic_load(&(prop->next));
                                prop = next.ptr;

                                prop_ver = atomic_load(&(prop->create_version));
                            }

                            /**
                             * If iterated to another property, the prop we searched
                             * for at the version we searched at doesn't exist.
                             */
                            if ( prop->chksum != prop_entry->chksum )
                            {
                                op_info->op_ver = op_info->obj_ver;
                                op_info->result = PROP_DOESNT_EXIST;
                            }
                        }

                        /**
                         * If TRUE, there was a version of the prop at the version we
                         * searched, but it must have been deleted. Double check.
                         */
                        if ( prop_ver <= op_info->obj_ver &&
                                prop->chksum == prop_entry->chksum )
                        {
                            prop_ver = atomic_load(&(prop->delete_version));
                            if ( prop_ver > 0 && prop_ver <= op_info->obj_ver )
                            {
                                op_info->op_ver = op_info->obj_ver;
                            }
                            else if ( prop_ver > 0 && prop_ver <= curr_ver )
                            {
                                op_info->op_ver = prop_ver;
                            }
                            else
                            {
                                assert(FALSE);
                            }

                            op_info->result = PROP_DELETED;
                        }
                    
                    } /* end if ( op_info->result == NOT_ATTEMPTED ) */
                    else
                    {
                        assert(op_info->result == PROP_DELETED);
                    }
#endif
                } /* end else if (prop_status == EXISTS || prop_status == DELETED) */
            }
            else {
                list = op_info->list;

                list_status = atomic_load(&(list_entry->status));

                if (list_status == DELETED || list_status == CLOSING_IN_PROGRESS) {
                    /**
                     * If list_entry's->ver_deleted is 0, the closing is still 
                     * in process so sleep and loop till the thread performing 
                     * the close updates ver_deleted, because we must know 
                     * which version the list was deleted at.
                     */
                    ver_del = atomic_load(&(list_entry->ver_deleted));

                    while ( ver_del == 0 )
                    {
                        sleep(1);

                        ver_del = atomic_load(&(list_entry->ver_deleted));
                    }

                    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);

                    op_info->obj_ver = ver_del;
                    op_info->result  = LIST_DELETED;
                }
                else if (list_status == IN_PROGRESS) {
                    op_info->result = LIST_DOESNT_EXIST;
                }
                else {
                    assert(FALSE);
                }
            }
        }

    } /* end if ( list ) */
    else {
        op_info->result = LIST_DOESNT_EXIST;
    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end mod_list_delete_prop() */

/****************************************************************************************
 * Function:    mod_class_create_prop
 *
 * Purpose:     Attempts to create a new property and insert it into a property class.
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a class_entry 
 *              from the class_table is randomly chosen and a prop_entry from the 
 *              class_entry's prop_table is also randomly chosen. If the class has ever 
 *              been created, we iterate the class_entry's prop_table to the next property
 *              that has the status of DOESNT_EXIST and atomically update its status to
 *              IN_PROGRESS and attempt to create it.
 *              NOTE: If there are no entries in the prop_table with the status of 
 *              DOESNT_EXIST, we randomly grab an entry in the prop_table and attempt to
 *              create it. This is done because the iterating the prop_table and creating
 *              only the ones that haven't been created yet allows more operations to 
 *              actually succeed allowing for more potential thread collisions in the
 *              H5P objects for testing. But after all have been created it allows for 
 *              testing of creating a new property to fail if it already exists in the
 *              class and if the property has been deleted for it to test that creating
 *              it again will succeed
 * 
 *              Immediately before we attempt to the create the property, we atomically
 *              set obj_ver to the class's curr_version and op_ver to the class's 
 *              next_version. This is so the value of the property is equal (or close to)
 *              its create_version, making checking that properties have the correct 
 *              value easier.
 *              
 *              If creating the property was successful, we atomically update the status 
 *              of the prop_entry to EXISTS, and grab the property from the class. We 
 *              must double check the op_ver for if another operation incremented the 
 *              class's version after we grabbed it but before creating the property. Then
 *              set the op_info->prop to point to the property and mark result as 
 *              OP_SUCCESS.
 *              
 *              If creating the property failed we search the index for the class as a 
 *              double check. If the class was in the index, the property must have 
 *              already existed, so check it's status. 
 *               *  If the status is EXISTS, IN_PROGRESS, or DELETED we double check that
 *                  the property was valid (meaning created and not deleted) at the 
 *                  version of the class we attempted to create the property at, and mark
 *                  the result as PROP_ALREADY_EXISTS.
 *              NOTE: If the status is DELETED, but we failed to create the property, it
 *              must have been deleted directly after our create attempt. We double check
 *              that, but otherwise the create would have succeeded.
 * 
 *              If creating the property failed and the class wasn't in the index, we 
 *              check the status of the class.
 *               *  If the class was created, but wasn't in the index and has a status
 *                  of IN_PROGRESS, then another thread was creating it, but hadn't 
 *                  finished and hadn't inserted the class into the index. Set obj_ver and 
 *                  op_ver to 0, op_info->class to NULL, and the result is marked 
 *                  CLASS_DOESNT_EXIST.
 *               *  If the status is DELETED or CLOSING_IN_PROGRESS we grab the version
 *                  the class was deleted at and set that as obj_ver and mark the result
 *                  as CLASS_DELETED.
 * 
 *              If before we attempt to create the property the class's struct tag was 
 *              invalid, we double check it was deleted and its obj_ver is set to the 
 *              version it was deleted at, and result is marked as CLASS_DELETED.
 * 
 *              Lastly if the class was not created at all yet, but the time the 
 *              class_entry was grabbed, the result is marked CLASS_DOESNT_EXIST.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
mod_class_create_prop(thread_params_t *thread_params)
{
    class_table_entry_t *class_entry;
    H5P_mt_class_sptr_t  class_sptr;
    H5P_mt_class_t *class;
    H5P_mt_prop_t      *prop;
    hid_t               class_id;
    prop_table_entry_t *prop_entry;
    status_t            class_status;
    status_t            prop_status;
    status_t            update_status;
    test_op_info_t     *op_info;
    uint32_t            op_num;
    int                 r = 0;
    uint32_t            nprops;
    uint32_t            i = 0;
    uint64_t            curr_ver;
    uint64_t            create_ver;
    uint64_t            delete_ver;
    uint64_t            ver_del;
    bool                done       = FALSE;
    bool                loop_check = FALSE;
    herr_t              ret; /* Generic return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = MOD_CREATE_PROP;
    op_info->op_num = op_num;

    /* Get a random entry from the class table */
    r = rand() % CLASS_TABLE_SIZE;

    class_entry = &class_table[r];

    atomic_fetch_add(&(class_entry->op_count), 1);

    op_info->class_name  = class_entry->name;
    op_info->parent_name = class_entry->parent_name;
    op_info->test_id     = class_entry->test_class_id;

    if (class_entry->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    class_id    = atomic_load(&(class_entry->id));
    op_info->id = class_id;

    class_sptr = atomic_load(&(class_entry->class_sptr));
    class      = class_sptr.ptr;

    op_info->class = class;

    class_status = atomic_load(&(class_entry->status));

    if (class) {
        /** NOTE:
         * If the class exists and is IN_PROGRESS it may still be
         * updating the prop_table statuses. Wait and check again.
         */
        while (class_status == IN_PROGRESS) {
            sleep(1);

            class_status = atomic_load(&(class_entry->status));
        }

        /* Get the next new property to create */
        nprops = atomic_load(&(class_entry->num_prop_entries));

        while (i < nprops) {
            prop_entry  = &class_entry->prop_table[i];
            prop_status = atomic_load(&(prop_entry->status));

            if (prop_status == DOESNT_EXIST) {
                update_status = IN_PROGRESS;

                /* Attempt to atomically update prop_status */
                if (!atomic_compare_exchange_strong(&(prop_entry->status), &prop_status, update_status)) {
                    /* Atomic update failed, check same prop_entry again */
                    if (loop_check) {
                        assert(FALSE);
                    }
                    else {
                        loop_check = TRUE;
                    }
                }
                else {
                    /* Atomic update successful */
                    done = TRUE;
                    break;
                }
            }
            else {
                i++;
            }

        } /* end while ( i < nprops ) */

        /**
         * If we've iterated the entire table of possible properties for the
         * class, grab a random prop_entry and attempt to create it. This will
         * test that if a prop already exists in a class creating it should fail,
         * and if that prop is deleted, it should be able to be created as a new
         * property again.
         */
        if (!done) {
            r = rand() % (int)nprops;

            prop_entry  = &class_entry->prop_table[r];
            prop_status = atomic_load(&(prop_entry->status));

            done = TRUE;
        }

        op_info->prop_name = prop_entry->name;

/** 
 * TODO:
 * add a stat to track how many times this occurs, it shows that there 
 * is more than one thread attempting to modify the class in someway. 
 */
#if 0
        assert(op_info->obj_ver + 1 == op_info->op_ver);
#endif

        /* Attempt to create the new property */
        H5E_BEGIN_TRY
        {
            op_info->obj_ver   = atomic_load(&(class->curr_version));
            op_info->op_ver    = atomic_load(&(class->next_version));

            ret = H5Pregister2(class_id, prop_entry->name, sizeof(op_info->op_ver), &op_info->op_ver, 
                                NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        }
        H5E_END_TRY

        if (ret == SUCCEED) {
            
            /* Update prop_status */
            update_status = EXISTS;
            done          = FALSE;
            do {
                prop_status = atomic_load(&(prop_entry->status));

                /* Attempt to atomically update prop_status */
                if (!atomic_compare_exchange_strong(&(prop_entry->status), &prop_status, update_status)) {
                    assert(prop_status == IN_PROGRESS);
                    if (loop_check) {
                        assert(FALSE);
                    }
                    else {
                        loop_check = TRUE;
                    }
                }
                else {
                    assert(atomic_load(&(prop_entry->status)) == EXISTS);
                    done = TRUE;
                }

            } while (!done);

            curr_ver = atomic_load(&(class->curr_version));

            prop = get_prop_from_lfsll(class->pl_head, prop_entry->name, 
                                        curr_ver);
            CHECK_PTR(prop, "get_prop_from_lfsll");
            assert(prop);

            /** 
             * If curr_ver is greater then another thread performed an operation on the
             * class after this thread grabbed the version and maybe before creating the 
             * prop. Double check
             */
            if ( curr_ver > op_info->op_ver )
            {
                if ( op_info->op_ver < atomic_load(&(prop->create_version)) )
                {
                    /** TODO: add stat for this */
                    op_info->op_ver = curr_ver;
                }
            }

            op_info->prop   = prop;
            op_info->result = OP_SUCCESS;
        }
        /* Creating the prop failed, check if the class is deleted */
        else {
            H5E_BEGIN_TRY
            {
                class = (H5P_mt_class_t *)H5I_object(class_id);
            }
            H5E_END_TRY

            /* If class is in the index, check if the prop is already in the class */
            if (class) {
                prop_status = atomic_load(&(prop_entry->status));

                curr_ver = atomic_load(&(class->curr_version));

                /* Check the LFSLL for the prop */
                prop = get_prop_from_lfsll(class->pl_head, prop_entry->name, curr_ver);

                if ( prop )
                {
                    /* Ensure the property does exist at the version we tried */
                    if ( prop_status == EXISTS || prop_status == IN_PROGRESS ||
                         prop_status == DELETED )
                    {
                        create_ver = atomic_load(&(prop->create_version));
                        delete_ver = atomic_load(&(prop->delete_version));

                        if ( create_ver > op_info->obj_ver )
                        {
                            assert(delete_ver == 0 || delete_ver >= curr_ver);
                            op_info->op_ver = curr_ver;
                        }
                        else 
                        {
                            op_info->op_ver = op_info->obj_ver;
                        }

                        op_info->result  = PROP_ALREADY_EXISTS;
                    }
                    else
                    {
                        /* If the prop doesn't exist, creating it should've succeed */
                        assert(FALSE);
                    }
                }
                else
                {
                    /* If the prop doesn't exist, creating it should've succeed */
                    assert(FALSE);
                }

            }
            else {
                class = op_info->class;

                class_status = atomic_load(&(class_entry->status));

                if (class_status == DELETED || class_status == CLOSING_IN_PROGRESS ||
                            class_status == EXISTS_BUT_CLOSED) {
                    /**
                     * If class_entry's->ver_deleted is 0, the closing is still 
                     * in process so sleep and loop till the thread performing 
                     * the close updates ver_deleted, because we must know 
                     * which version the class was deleted at.
                     */
                    ver_del = atomic_load(&(class_entry->ver_deleted));

                    while ( ver_del == 0 )
                    {
                        sleep(1);

                        ver_del = atomic_load(&(class_entry->ver_deleted));
                    }

                    assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);

                    op_info->obj_ver = ver_del;
                    op_info->result  = CLASS_DELETED;
                }
                else if (class_status == IN_PROGRESS) {
                    op_info->result = CLASS_DOESNT_EXIST;
                }
                else {
                    assert(FALSE);
                }
            }
        }

    } /* end if ( class ) */
    else {
        op_info->result = CLASS_DOESNT_EXIST;
    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end mod_class_create_prop() */

/****************************************************************************************
 * Function:    mod_class_mod_prop
 *
 * Purpose:     Attempts to modify a property in a property class (create a new version 
 *              of the property structure with the updated value).
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a class_entry 
 *              from the class_table is randomly chosen. If the class has ever been 
 *              created, we randomly grab a prop_entry from the class_entry's prop_table
 *              and attempt to set a new value.
 * 
 *              Immediately before we attempt to set the new value, we atomically
 *              set obj_ver to the class's curr_version and op_ver to the class's 
 *              next_version. This is so the value of the property is equal (or close to)
 *              its create_version, making checking that properties have the correct 
 *              value easier. 
 *              NOTE: op_ver is the new value.
 *              
 *              If creating the property was successful, we grab the property from the 
 *              class. We must double check the op_ver for if another operation 
 *              incremented the class's version after we grabbed it but before modifying 
 *              the property. Then set the op_info->prop to point to the property and 
 *              mark result as OP_SUCCESS.
 *              
 *              If creating the property failed we search the index for the class as a 
 *              double check. If the class was in the index, check the prop's status. 
 *               *  If status is DOESNT_EXIST or IN_PROGRESS and we failed to modify it
 *                  the property didn't exist in the class yet. Set op_ver to obj_ver 
 *                  because the operation didn't occur so this didn't increment the 
 *                  class's version, and mark result as PROP_DOESNT_EXIST.
 *               *  If the status is EXISTS or DELETED we double check that the property 
 *                  was not valid (meaning deleted or not created) at the version of the 
 *                  class we attempted to modify the property at. Set the op_ver to 
 *                  obj_ver and mark the result as PROP_DELETED or PROP_DOESNT_EXISTS as
 *                  appropriate.
 * 
 *              If modifying the property failed and the class wasn't in the index, we 
 *              check the status of the class.
 *               *  If the class was created, but wasn't in the index and has a status
 *                  of IN_PROGRESS, then another thread was creating it, but hadn't 
 *                  finished and hadn't inserted the class into the index. Set obj_ver and 
 *                  op_ver to 0, op_info->class to NULL, and the result is marked 
 *                  CLASS_DOESNT_EXIST.
 *               *  If the status is DELETED or CLOSING_IN_PROGRESS we grab the version
 *                  the class was deleted at and set that as obj_ver and mark the result
 *                  as CLASS_DELETED.
 * 
 *              Lastly if the class was not created at all yet, but the time the 
 *              class_entry was grabbed, the result is marked CLASS_DOESNT_EXIST.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
mod_class_mod_prop(thread_params_t *thread_params)
{
    class_table_entry_t *class_entry;
    H5P_mt_class_sptr_t  class_sptr;
    H5P_mt_class_t      *class;
    H5P_mt_prop_t       *prop;
    H5P_mt_prop_aptr_t   next;
    hid_t                class_id;
    prop_table_entry_t  *prop_entry;
    status_t             class_status;
    status_t             prop_status;
    test_op_info_t      *op_info;
    uint32_t             op_num;
    int                  r;
    uint32_t             nprops;
    uint64_t             curr_ver;
    uint64_t             prop_ver;
    uint64_t             ver_del;
    herr_t               ret; /* Generic return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = MOD_MOD_PROP;
    op_info->op_num = op_num;

    /* Get a random entry from the class table */
    r = rand() % CLASS_TABLE_SIZE;

    class_entry = &class_table[r];

    atomic_fetch_add(&(class_entry->op_count), 1);

    op_info->class_name  = class_entry->name;
    op_info->parent_name = class_entry->parent_name;
    op_info->test_id     = class_entry->test_class_id;

    if (class_entry->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    class_id    = atomic_load(&(class_entry->id));
    op_info->id = class_id;

    class_sptr = atomic_load(&(class_entry->class_sptr));
    class      = class_sptr.ptr;

    op_info->class = class;

    if (class) {
        class_status = atomic_load(&(class_entry->status));

        if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG ) {

            /* Randomly select a possible property to attempt to create a new version of */
            nprops = atomic_load(&(class_entry->num_prop_entries));
            r      = rand() % (int)nprops;

            prop_entry  = &class_entry->prop_table[r];
            prop_status = atomic_load(&(prop_entry->status));

            op_info->prop_name = prop_entry->name;

/** 
 * TODO:
 * add a stat to track how many times this occurs, it shows that there 
 * is more than one thread attempting to modify the class in someway. 
 */
#if 0
        assert(op_info->obj_ver + 1 == op_info->op_ver);
#endif

            /* Attempt to create the new version of the property */
            H5E_BEGIN_TRY
            {
                op_info->obj_ver   = atomic_load(&(class->curr_version));
                op_info->op_ver    = atomic_load(&(class->next_version));

                if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG )
                {
                    ret = H5P__class_set(class, prop_entry->name, &op_info->op_ver);
                }
                else
                {
                    ret = FAIL;
                }

            }
            H5E_END_TRY

            if (ret == SUCCEED) {
                curr_ver = atomic_load(&(class->curr_version));
                
                prop = get_prop_from_lfsll(class->pl_head, prop_entry->name, 
                                            curr_ver);
                CHECK_PTR(prop, "get_prop_from_lfsll");
                assert(prop);

                prop_ver = atomic_load(&(prop->create_version));

                /** 
                 * If curr_ver is greater then another thread performed an operation on the
                 * class after this thread grabbed the version and maybe before creating the 
                 * prop. Double check
                 */
                if ( prop_ver > op_info->op_ver )
                {
                    op_info->op_ver = prop_ver;
                }

                op_info->prop   = prop;
                op_info->result = OP_SUCCESS;
            }
            /* Failed creating a new version of the property, find why */
            else {
                H5E_BEGIN_TRY
                {
                    class = (H5P_mt_class_t *)H5I_object(class_id);
                }
                H5E_END_TRY

                /* If the class is in the index check the prop_status */
                if (class) {
                    prop_status = atomic_load(&(prop_entry->status));

                    curr_ver = atomic_load(&(class->curr_version));

                    if (prop_status == DOESNT_EXIST || prop_status == IN_PROGRESS) {
                        op_info->op_ver = op_info->obj_ver;
                        op_info->result = PROP_DOESNT_EXIST;
                    }
                    /**
                     * If we failed to find the property, but the status
                     * is EXISTS, check that the prop was created after the
                     * version we searched
                     * If status is DELETED ensure that is correct for the 
                     * version we searched.
                     */
                    else if (prop_status == EXISTS || prop_status == DELETED) {
                        prop = get_prop_from_lfsll(class->pl_head, prop_entry->name,
                                                curr_ver);

                        prop_ver = atomic_load(&(prop->create_version));

                        /* If the property's create_version is > the version we searched */
                        if ( prop_ver > op_info->obj_ver )
                        {
                            /* Check for older versions in the LFSLL */
                            while ( prop_ver > op_info->obj_ver && 
                                    prop->chksum == prop_entry->chksum )
                            {
                                next = atomic_load(&(prop->next));
                                prop = next.ptr;

                                prop_ver = atomic_load(&(prop->create_version));
                            }

                            /**
                             * If iterated to another property, the prop we searched
                             * for at the version we searched at doesn't exist.
                             */
                            if ( prop->chksum != prop_entry->chksum )
                            {
                                op_info->op_ver = op_info->obj_ver;
                                op_info->result = PROP_DOESNT_EXIST;
                            }
                        }
                        
                        /**
                         * If TRUE, there was a version of the prop at the version we
                         * searched, but it must have been deleted. Double check.
                         */
                        if ( prop_ver <= op_info->obj_ver &&
                             prop->chksum == prop_entry->chksum )
                        {
                            prop_ver = atomic_load(&(prop->delete_version));
                            if ( prop_ver > 0 && prop_ver <= op_info->obj_ver )
                            {
                                op_info->op_ver = op_info->obj_ver;
                            }
                            else if ( prop_ver > 0 && prop_ver <= curr_ver )
                            {
                                op_info->op_ver = curr_ver;
                            }
                            else
                            {
                                assert(FALSE);
                            }

                            op_info->prop   = prop;
                            op_info->result = PROP_DELETED;
                        }  
                        
                        assert(op_info->result != NOT_ATTEMPTED);
                    
                    } /* end else if (prop_status == EXISTS || prop_status == DELETED) */
                    else
                    {
                        assert(FALSE);
                    }
                }
                else {
                    class = op_info->class;

                    class_status = atomic_load(&(class_entry->status));

                    if ( class_status == DELETED || 
                                class_status == CLOSING_IN_PROGRESS ||
                                class_status == EXISTS_BUT_CLOSED )
                    {
                        /**
                         * If class_entry's->ver_deleted is 0, the closing is still 
                         * in process so sleep and loop till the thread performing 
                         * the close updates ver_deleted, because we must know 
                         * which version the class was deleted at.
                         */
                        ver_del = atomic_load(&(class_entry->ver_deleted));

                        while ( ver_del == 0 )
                        {
                            sleep(1);

                            ver_del = atomic_load(&(class_entry->ver_deleted));
                        }

                        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);

                        op_info->obj_ver = ver_del;
                        op_info->result  = CLASS_DELETED;
                    }
                    else if (class_status == IN_PROGRESS) {
                        op_info->result = CLASS_DOESNT_EXIST;
                    }
                    else {
                        assert(FALSE);
                    }
                }
            }

        } /* end if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG ) */
        else {
            assert(class_status == DELETED || class_status == CLOSING_IN_PROGRESS ||
                    class_status == EXISTS_BUT_CLOSED);
            
            /**
             * If class_entry's->ver_deleted is 0, the closing is still 
             * in process so sleep and loop till the thread performing 
             * the close updates ver_deleted, because we must know 
             * which version the class was deleted at.
             */
            ver_del = atomic_load(&(class_entry->ver_deleted));

            while ( ver_del == 0 )
            {
                sleep(1);

                ver_del = atomic_load(&(class_entry->ver_deleted));
            }

            assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);

            op_info->obj_ver = ver_del;
            op_info->result  = CLASS_DELETED;
        }

    } /* end if ( class ) */
    else {
        op_info->result = CLASS_DOESNT_EXIST;
    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end mod_class_mod_prop() */

/****************************************************************************************
 * Function:    mod_class_delete_prop
 *
 * Purpose:     Attempts to delete a property in a property class.
 *
 * Details:     First, the entry (op_info) for this operation is grabbed from the 
 *              thread_params->op_table and the op and op_num is set. Then a class_entry 
 *              from the class_table is randomly chosen. If the class has ever been 
 *              created, we randomly grab a prop_entry from the class_entry's prop_table
 *              and attempt to delete it.
 * 
 *              Immediately before we attempt to delete the prop, we atomically set 
 *              obj_ver to the class's curr_version and op_ver to the class's 
 *              next_version. 
 *              
 *              If deleting the property was successful, we atomically update the 
 *              prop_entry's status to DELETED and grab the property from the class. We 
 *              must double check the op_ver for if another operation incremented the 
 *              class's version after we grabbed it but before deleting the property.
 *              Then set the op_info->prop to point to the property and mark result as 
 *              OP_SUCCESS.
 *              
 *              If creating the property failed we search the index for the class as a 
 *              double check. If the class was in the index, check the prop's status. 
 *               *  If status is DOESNT_EXIST or IN_PROGRESS and we failed to modify it
 *                  the property didn't exist in the class yet. Set op_ver to obj_ver 
 *                  because the operation didn't occur so this didn't increment the 
 *                  class's version, and mark result as PROP_DOESNT_EXIST.
 *               *  If the status is EXISTS or DELETED we double check that the property 
 *                  was not valid (meaning deleted or not created) at the version of the 
 *                  class we attempted to modify the property at. Set the op_ver to 
 *                  obj_ver and mark the result as PROP_DELETED or PROP_DOESNT_EXISTS as
 *                  appropriate.
 * 
 *              If modifying the property failed and the class wasn't in the index, we 
 *              check the status of the class.
 *               *  If the class was created, but wasn't in the index and has a status
 *                  of IN_PROGRESS, then another thread was creating it, but hadn't 
 *                  finished and hadn't inserted the class into the index. Set obj_ver and 
 *                  op_ver to 0, op_info->class to NULL, and the result is marked 
 *                  CLASS_DOESNT_EXIST.
 *               *  If the status is DELETED or CLOSING_IN_PROGRESS we grab the version
 *                  the class was deleted at and set that as obj_ver and mark the result
 *                  as CLASS_DELETED.
 * 
 *              Lastly if the class was not created at all yet, but the time the 
 *              class_entry was grabbed, the result is marked CLASS_DOESNT_EXIST.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
mod_class_delete_prop(thread_params_t *thread_params)
{
    class_table_entry_t *class_entry;
    H5P_mt_class_sptr_t  class_sptr;
    H5P_mt_class_t *class;
    H5P_mt_prop_t      *prop;
    H5P_mt_prop_aptr_t  next;
    hid_t               class_id;
    prop_table_entry_t *prop_entry;
    status_t            class_status;
    status_t            prop_status;
    status_t            update_status;
    test_op_info_t     *op_info;
    uint32_t            op_num;
    int                 r;
    uint32_t            nprops;
    uint64_t            curr_ver;
    uint64_t            prop_ver;
    uint64_t            ver_del;
    bool                done = FALSE;
    bool                loop_check = FALSE;
    herr_t              ret; /* Generic return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = MOD_DELETE_PROP;
    op_info->op_num = op_num;

    /* Get a random entry from the class table */
    r = rand() % CLASS_TABLE_SIZE;

    class_entry = &class_table[r];

    atomic_fetch_add(&(class_entry->op_count), 1);

    op_info->class_name  = class_entry->name;
    op_info->parent_name = class_entry->parent_name;
    op_info->test_id     = class_entry->test_class_id;

    if (class_entry->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    class_id    = atomic_load(&(class_entry->id));
    op_info->id = class_id;

    class_sptr = atomic_load(&(class_entry->class_sptr));
    class      = class_sptr.ptr;

    op_info->class = class;

    class_status = atomic_load(&(class_entry->status));

    if (class) {
        /* Randomly select a possible property to attempt to delete */
        nprops = atomic_load(&(class_entry->num_prop_entries));
        r      = rand() % (int)nprops;

        prop_entry  = &class_entry->prop_table[r];
        prop_status = atomic_load(&(prop_entry->status));

        op_info->prop_name = prop_entry->name;

/** 
 * TODO:
 * add a stat to track how many times this occurs, it shows that there 
 * is more than one thread attempting to modify the class in someway. 
 */
#if 0
        assert(op_info->obj_ver + 1 == op_info->op_ver);
#endif

        /* Attempt to delete the property */
        H5E_BEGIN_TRY
        {
            op_info->obj_ver = atomic_load(&(class->curr_version));
            op_info->op_ver  = atomic_load(&(class->next_version));

            ret = H5Punregister(class_id, prop_entry->name);
        }
        H5E_END_TRY

        if (ret == SUCCEED) {

            /* Update prop_status */
            update_status = DELETED;
            done          = FALSE;
            do {
                prop_status = atomic_load(&(prop_entry->status));

                /* Attempt to atomically update prop_status */
                if (!atomic_compare_exchange_strong(&(prop_entry->status), &prop_status, update_status)) {
                    assert(prop_status == IN_PROGRESS || prop_status == EXISTS);
                    if (loop_check) {
                        assert(FALSE);
                    }
                    else {
                        loop_check = TRUE;
                    }
                }
                else {
                    prop_status = atomic_load(&(prop_entry->status));
                    assert(prop_status == DELETED);
                    done = TRUE;
                }

            } while (!done);

            curr_ver = atomic_load(&(class->curr_version));

            prop = get_prop_from_lfsll(class->pl_head, prop_entry->name, 
                                        curr_ver);
            CHECK_PTR(prop, "get_prop_from_lfsll");
            assert(prop);

            /** 
             * If curr_ver is greater then another thread performed an operation on the
             * class after this thread grabbed the version and maybe before creating the 
             * prop. Double check
             */
            if ( curr_ver > op_info->op_ver )
            {
                prop_ver = atomic_load(&(prop->delete_version));

                if ( op_info->op_ver < prop_ver )
                {
                    /** TODO: add stat for this */
                    op_info->op_ver = prop_ver;
                }
            }

            op_info->prop   = prop;
            op_info->result = OP_SUCCESS;
        }
        /* Failed deleting the property, find why */
        else {
            H5E_BEGIN_TRY
            {
                class = (H5P_mt_class_t *)H5I_object(class_id);
            }
            H5E_END_TRY

            /* If class is in index, check prop_status */
            if (class) {
                prop_status = atomic_load(&(prop_entry->status));

                curr_ver = atomic_load(&(class->curr_version));
                if (prop_status == DOESNT_EXIST || prop_status == IN_PROGRESS) {
                    op_info->op_ver = op_info->obj_ver;
                    op_info->result = PROP_DOESNT_EXIST;
                }
                /**
                 * If we failed to find the property, but the status
                 * is EXISTS, check that the prop was created after the
                 * version we searched
                 * If status is DELETED ensure that is correct for the 
                 * version we searched.
                 */
                else if (prop_status == EXISTS || prop_status == DELETED) {
                    prop = get_prop_from_lfsll(class->pl_head, prop_entry->name,
                                            curr_ver);

                    prop_ver = atomic_load(&(prop->create_version));

                    /* If the property's create_version is > the version we searched */
                    if ( prop_ver > op_info->obj_ver )
                    {
                        /* Check for older versions in the LFSLL */
                        while ( prop_ver > op_info->obj_ver && 
                                prop->chksum == prop_entry->chksum )
                        {
                            next = atomic_load(&(prop->next));
                            prop = next.ptr;

                            prop_ver = atomic_load(&(prop->create_version));
                        }

                        /**
                         * If iterated to another property, the prop we searched
                         * for at the version we searched at doesn't exist.
                         */
                        if ( prop->chksum != prop_entry->chksum )
                        {
                            op_info->op_ver = op_info->obj_ver;
                            op_info->result = PROP_DOESNT_EXIST;
                        }
                    }
                    
                    /**
                     * If TRUE, there was a version of the prop at the version we
                     * searched, but it must have been deleted. Double check.
                     */
                    if ( prop_ver <= op_info->obj_ver &&
                            prop->chksum == prop_entry->chksum )
                    {
                        prop_ver = atomic_load(&(prop->delete_version));
                        if ( prop_ver > 0 && prop_ver <= op_info->obj_ver )
                        {
                            op_info->op_ver = op_info->obj_ver;
                        }
                        else if ( prop_ver > 0 && prop_ver <= curr_ver )
                        {
                            op_info->op_ver = curr_ver;
                        }
                        else
                        {
                            assert(FALSE);
                        }

                        op_info->result = PROP_DELETED;
                    }  
                    
                    assert(op_info->result != NOT_ATTEMPTED);
                
                } /* end else if (prop_status == EXISTS || prop_status == DELETED) */
                else {
                    fprintf(stderr, "prop_status shouldn't be: %d\n", prop_status);
                    assert(FALSE);
                }
            }
            else {
                class = op_info->class;

                class_status = atomic_load(&(class_entry->status));

                if (class_status == DELETED || class_status == CLOSING_IN_PROGRESS ||
                            class_status == EXISTS_BUT_CLOSED) {        
                    /**
                     * If class_entry's->ver_deleted is 0, the closing is still 
                     * in process so sleep and loop till the thread performing 
                     * the close updates ver_deleted, because we must know 
                     * which version the class was deleted at.
                     */
                    ver_del = atomic_load(&(class_entry->ver_deleted));

                    while ( ver_del == 0 )
                    {
                        sleep(1);

                        ver_del = atomic_load(&(class_entry->ver_deleted));
                    }

                    assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);

                    op_info->obj_ver = ver_del;
                    op_info->result  = CLASS_DELETED;
                }
                else if (class_status == IN_PROGRESS) {
                    op_info->result = CLASS_DOESNT_EXIST;
                }
                else {
                    assert(FALSE);
                }
            }
        }

    } /* end if ( class ) */
    else {
        op_info->result = CLASS_DOESNT_EXIST;
    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end mod_class_delete_prop() */

#if 0
/**
 *
 */
static herr_t
cmp_list_equal(thread_params_t *thread_params)
{
    list_table_entry_t *list_entry;
    status_t            list_status;
    hid_t               list_id1;
    hid_t               list_id2;
    H5P_mt_list_t      *list;
    H5P_mt_list_sptr_t  list_sptr;
    int                 r;
    test_op_info_t     *op_info;
    uint32_t            op_num;
    uint64_t            curr_ver;
    uint64_t            next_ver;
    htri_t              htri_ret; /* Generic htri_t return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = CMP;
    op_info->op_num = op_num;

    /* Get a random entry from the list table */
    r = rand() % LIST_TABLE_SIZE;

    list_entry = &list_table[r];

    atomic_fetch_add(&(list_entry->op_count), 1);

    op_info->parent_name = list_entry->parent_name;
    op_info->test_id     = list_entry->test_list_id;

    if (list_entry->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    list_status = atomic_load(&(list_entry->status));

    /* If the list exists, have two pointers point to it and compare */
    if (list_status == EXISTS) {
        list_id1 = atomic_load(&(list_entry->id));

        list = (H5P_mt_list_t *)H5I_object(list_id1);
        CHECK_PTR(list, "H5I_object");
        assert(list);
        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

        /* Update op_info */
        op_info->list    = list;
        op_info->id      = atomic_load(&(list->plist_id));
        op_info->obj_ver = atomic_load(&(list->curr_version));

        list_id2 = list_id1;

        htri_ret = H5Pequal(list_id1, list_id2);
        VERIFY(htri_ret, 1, "H5Pequal");
        assert(htri_ret == 1);

        op_info->result = OP_SUCCESS;
    }
    else if (list_status == DOESNT_EXIST || list_status == IN_PROGRESS) {
        op_info->result = LIST_DOESNT_EXIST;
    }
    else {
        list_sptr     = atomic_load(&(list_entry->list_sptr));
        list          = list_sptr.ptr;
        if ( list )
        {
            op_info->list = list;
            op_info->id   = atomic_load(&(list->plist_id));

            if (atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG) {
                assert(list_status == DELETED || list_status == CLOSING_IN_PROGRESS);
                op_info->obj_ver = atomic_load(&(list->curr_version));
            }
            else {
                assert(list_status == CLOSING_IN_PROGRESS);

                curr_ver = atomic_load(&(list->curr_version));
                next_ver = atomic_load(&(list->next_version));

                if (curr_ver + 1 != next_ver) {
                    op_info->obj_ver = next_ver - 1;
                }
            }
            op_info->result = LIST_DELETED;
        }
        else
        {
            op_info->result = LIST_DOESNT_EXIST;
        }
    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end cmp_list_equal() */

/**
 *
 */
static herr_t
cmp_list_not_equal(thread_params_t *thread_params)
{
    list_table_entry_t *list_entry1;
    list_table_entry_t *list_entry2;
    status_t            list_status1;
    status_t            list_status2;
    hid_t               list_id1;
    hid_t               list_id2;
    H5P_mt_list_t      *list;
    H5P_mt_list_sptr_t  list_sptr;
    int                 r1;
    int                 r2;
    test_op_info_t     *op_info;
    uint32_t            op_num;
    uint64_t            curr_ver;
    uint64_t            next_ver;
    bool                done = FALSE;
    htri_t              htri_ret; /* Generic htri_t return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = CMP;
    op_info->op_num = op_num;

    /* Get a random entry from the list table */
    r1 = rand() % LIST_TABLE_SIZE;

    list_entry1 = &list_table[r1];

    atomic_fetch_add(&(list_entry1->op_count), 1);

    op_info->parent_name = list_entry1->parent_name;
    op_info->test_id     = list_entry1->test_list_id;

    if (list_entry1->copy) {
        op_info->obj_isa_copy = TRUE;
    }
    
    list_status1 = atomic_load(&(list_entry1->status));

    /* If the list exists, have two pointers point to it and compare */
    if (list_status1 == EXISTS) {
        list_id1 = atomic_load(&(list_entry1->id));

        list = (H5P_mt_list_t *)H5I_object(list_id1);
        CHECK_PTR(list, "H5I_object");
        assert(list);
        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG);

        /* Update op_info */
        op_info->list    = list;
        op_info->id      = atomic_load(&(list->plist_id));
        op_info->obj_ver = atomic_load(&(list->curr_version));

        r2 = 0;
        do {
            list_entry2  = &list_table[r2];
            list_status2 = atomic_load(&(list_entry2->status));

            if (r2 == r1 || list_status2 != EXISTS) {
                r2++;
            }
            else if ( list_entry1->copy )
            {
                if ( list_entry1->og_id != list_entry2->test_list_id )
                {
                    done = TRUE;
                }
                else
                {
                    r2++;
                }
            }
            else if ( list_entry2->copy)
            {
                if ( list_entry2->og_id != list_entry1->test_list_id )
                {
                    done = TRUE;
                }
                else
                {
                    r2++;
                }
            }
            else {
                done = TRUE;
            }

        } while (!done);

        list_id2 = atomic_load(&(list_entry2->id));

        H5E_BEGIN_TRY
        {
            htri_ret = H5Pequal(list_id1, list_id2);
        }
        H5E_END_TRY

        //VERIFY(htri_ret, 0, "H5Pequal");
        assert(htri_ret != 1);

        op_info->result = OP_SUCCESS;
    }
    else if (list_status1 == DOESNT_EXIST || list_status1 == IN_PROGRESS) {
        op_info->result = LIST_DOESNT_EXIST;
    }
    else {
        list_sptr     = atomic_load(&(list_entry1->list_sptr));
        list          = list_sptr.ptr;

        if ( list )
        {
            op_info->list = list;
            op_info->id   = atomic_load(&(list->plist_id));

            if (atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG) {
                assert(list_status1 == DELETED || list_status1 == CLOSING_IN_PROGRESS);
                op_info->obj_ver = atomic_load(&(list->curr_version));
            }
            else {
                assert(list_status1 == CLOSING_IN_PROGRESS);

                curr_ver = atomic_load(&(list->curr_version));
                next_ver = atomic_load(&(list->next_version));

                if (curr_ver + 1 != next_ver) {
                    op_info->obj_ver = next_ver - 1;
                }
            }
            op_info->result = LIST_DELETED;
        }
        else
        {
            op_info->result = LIST_DOESNT_EXIST;
        }
    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end cmp_list_not_equal() */

/**
 *
 */
static herr_t
cmp_class_equal(thread_params_t *thread_params)
{
    class_table_entry_t *class_entry;
    status_t             class_status;
    hid_t                class_id1;
    hid_t                class_id2;
    H5P_mt_class_t *class;
    H5P_mt_class_sptr_t class_sptr;
    int                 r;
    test_op_info_t     *op_info;
    uint32_t            op_num;
    uint64_t            curr_ver;
    uint64_t            next_ver;
    htri_t              htri_ret; /* Generic htri_t return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = CMP;
    op_info->op_num = op_num;

    /* Get a random entry from the class table */
    r = rand() % CLASS_TABLE_SIZE;

    class_entry = &class_table[r];

    atomic_fetch_add(&(class_entry->op_count), 1);

    op_info->class_name  = class_entry->name;
    op_info->parent_name = class_entry->parent_name;
    op_info->test_id     = class_entry->test_class_id;

    if (class_entry->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    class_status = atomic_load(&(class_entry->status));

    /* If the class exists, have two pointers point to it and compare */
    if (class_status == EXISTS || class_status == EXISTS_BUT_CLOSED) {
        class_id1 = atomic_load(&(class_entry->id));

        class = (H5P_mt_class_t *)H5I_object(class_id1);

        if ( ! class )
        {
            class_sptr = atomic_load(&(class_entry->class_sptr));
            class      = class_sptr.ptr;

            if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG )
            {
                op_info->obj_ver = atomic_load(&(class->curr_version));
                op_info->result  = CLASS_DELETED;
            }
            else
            {
                assert(FALSE);
            }        
        }
        else
        {
            assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG);

            /* Update op_info */
            op_info->class   = class;
            op_info->id      = atomic_load(&(class->id));
            op_info->obj_ver = atomic_load(&(class->curr_version));

            class_id2 = class_id1;

            htri_ret = H5Pequal(class_id1, class_id2);
            VERIFY(htri_ret, 1, "H5Pequal");
            assert(htri_ret == 1);

            op_info->result = OP_SUCCESS;
        }
    }
    else if (class_status == DOESNT_EXIST || class_status == IN_PROGRESS) {
        op_info->result = CLASS_DOESNT_EXIST;
    }
    else {
        class_sptr     = atomic_load(&(class_entry->class_sptr));
        class          = class_sptr.ptr;

        if ( class )
        {
            op_info->class = class;
            op_info->id    = atomic_load(&(class->id));

            if (atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG) {
                assert(class_status == DELETED || class_status == CLOSING_IN_PROGRESS);
                op_info->obj_ver = atomic_load(&(class->curr_version));
            }
            else {
                assert(class_status == CLOSING_IN_PROGRESS);

                curr_ver = atomic_load(&(class->curr_version));
                next_ver = atomic_load(&(class->next_version));

                if (curr_ver + 1 != next_ver) {
                    op_info->obj_ver = next_ver - 1;
                }
            }
            op_info->result = CLASS_DELETED;
        }
        else
        {
            op_info->result = CLASS_DOESNT_EXIST;
        }

    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end cmp_class_equal() */

/**
 *
 */
static herr_t
cmp_class_not_equal(thread_params_t *thread_params)
{
    class_table_entry_t *class_entry1;
    class_table_entry_t *class_entry2;
    status_t             class_status1;
    status_t             class_status2;
    hid_t                class_id1;
    hid_t                class_id2;
    H5P_mt_class_t *class;
    H5P_mt_class_sptr_t class_sptr;
    int                 r1;
    int                 r2;
    test_op_info_t     *op_info;
    uint32_t            op_num;
    uint64_t            curr_ver;
    uint64_t            next_ver;
    bool                done = FALSE;
    htri_t              htri_ret; /* Generic htri_t return value */

    assert(thread_params);

    op_num  = thread_params->ops_performed;
    op_info = &thread_params->op_table[op_num];

    op_info->op     = CMP;
    op_info->op_num = op_num;

    /* Get a random entry from the list table */
    r1 = rand() % CLASS_TABLE_SIZE;

    class_entry1 = &class_table[r1];

    atomic_fetch_add(&(class_entry1->op_count), 1);

    op_info->class_name  = class_entry1->name;
    op_info->parent_name = class_entry1->parent_name;
    op_info->test_id     = class_entry1->test_class_id;

    if (class_entry1->copy) {
        op_info->obj_isa_copy = TRUE;
    }

    class_status1 = atomic_load(&(class_entry1->status));

    /* If the list exists, have two pointers point to it and compare */
    if (class_status1 == EXISTS || class_status1 == EXISTS_BUT_CLOSED) {
        class_id1 = atomic_load(&(class_entry1->id));

        class = (H5P_mt_class_t *)H5I_object(class_id1);
        
        if ( ! class)
        {
            class_sptr = atomic_load(&(class_entry1->class_sptr));
            class      = class_sptr.ptr;

            if ( atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG )
            {
                op_info->obj_ver = atomic_load(&(class->curr_version));
                op_info->result  = CLASS_DELETED;
            }
            else
            {
                assert(FALSE);
            }
        }
        else
        {
            /* Update op_info */
            op_info->class   = class;
            op_info->id      = atomic_load(&(class->id));
            op_info->obj_ver = atomic_load(&(class->curr_version));

            r2 = 0;
            do {
                class_entry2  = &class_table[r2];
                class_status2 = atomic_load(&(class_entry2->status));

                if (r2 == r1 || class_status2 != EXISTS) {
                    r2++;
                }
                else if ( class_entry1->copy )
                {
                    if ( class_entry1->og_id != class_entry2->test_class_id )
                    {
                        done = TRUE;
                    }
                    else
                    {
                        r2++;
                    }
                }
                else if ( class_entry2->copy)
                {
                    if ( class_entry2->og_id != class_entry1->test_class_id )
                    {
                        done = TRUE;
                    }
                    else
                    {
                        r2++;
                    }
                }
                else {
                    done = TRUE;
                }

            } while (!done);

            class_id2 = atomic_load(&(class_entry2->id));

            H5E_BEGIN_TRY
            {
                htri_ret = H5Pequal(class_id1, class_id2);
            }
            H5E_END_TRY

            assert(htri_ret != 1);

            op_info->result = OP_SUCCESS;
        }
    }
    else if (class_status1 == DOESNT_EXIST || class_status1 == IN_PROGRESS) {
        op_info->result = CLASS_DOESNT_EXIST;
    }
    else {
        class_sptr     = atomic_load(&(class_entry1->class_sptr));
        class          = class_sptr.ptr;

        if ( class )
        {
            op_info->class = class;
            op_info->id    = atomic_load(&(class->id));

            if (atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG) {
                assert(class_status1 == DELETED || class_status1 == CLOSING_IN_PROGRESS);
                op_info->obj_ver = atomic_load(&(class->curr_version));
            }
            else {
                assert(class_status1 == CLOSING_IN_PROGRESS);

                curr_ver = atomic_load(&(class->curr_version));
                next_ver = atomic_load(&(class->next_version));

                if (curr_ver + 1 != next_ver) {
                    op_info->obj_ver = next_ver - 1;
                }
            }
            op_info->result = CLASS_DELETED;
        }
        else
        {
            op_info->result = CLASS_DOESNT_EXIST;
        }

    }

    assert(op_info->result != NOT_ATTEMPTED);

    return SUCCEED;

} /* end cmp_class_not_equal() */
#endif

/****************************************************************************************
 * Function:    prop_ver_check_list
 *
 * Purpose:     Checks the version of the specified property and the list to determine
 *              the correct version the operation was performed on.
 * 
 *                                              
 * Return:      0 if the property didn't exist at the specified version, or the version
 *              the operation was performed on.
 *
 ****************************************************************************************
 */
uint64_t
prop_ver_check_list(H5P_mt_list_t *list, prop_table_entry_t *prop_entry, 
                    uint64_t obj_ver, uint64_t curr_ver, bool *_deleted, bool success)
{
    H5P_mt_list_table_entry_t *lkup_entry = NULL;
    H5P_mt_list_prop_ref_t prop_ref;
    H5P_mt_prop_t *prop = NULL;
    H5P_mt_prop_aptr_t next;
    uint64_t first_curr_ver;
    uint64_t base_del_ver;
    uint64_t create_ver;
    uint64_t delete_ver;
    bool deleted = FALSE;

    uint64_t ret_value = 0;

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG ||
           atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);
    assert(prop_entry);
    assert(obj_ver > 0);
    assert(curr_ver > 0);

    /* Check the list's lkup_tbl for the property */
    for ( size_t i = 0; i < list->nprops_inherited; i++ )
    {
        lkup_entry = &list->lkup_tbl[i];

        if ( lkup_entry->chksum == prop_entry->chksum )
        {
            first_curr_ver = atomic_load(&(lkup_entry->first_ver_of_curr));

            /**
             * If there is a first version of curr and it's > the
             * version we searched for, the base is our version.
             */
            if ( first_curr_ver == 0 || first_curr_ver > obj_ver )
            {
                base_del_ver = atomic_load(&(lkup_entry->base_delete_version));

                /**
                 * If the base is deleted for the version we 
                 * searched our prop is in fact deleted.
                 */
                if ( base_del_ver > 0 )
                {
                    if ( success )
                    {
                        if ( base_del_ver >= obj_ver && base_del_ver <= curr_ver)
                        {
                            ret_value = base_del_ver;
                            deleted = TRUE;
                            break;
                        }
                        else 
                        {
                            assert(FALSE);
                        }                 
                    }
                    else
                    {
                        if ( base_del_ver <= obj_ver)
                        {
                            ret_value = base_del_ver;
                            deleted = TRUE;
                            break;
                        }
                        else if ( base_del_ver <= curr_ver )
                        {
                            ret_value = curr_ver;
                            deleted = TRUE;
                            break;
                        }
                        else
                        {
                            /**
                             * If the base pointer is the one needed for this version, and
                             * it's NOT deleted, something is wrong. 
                             * If op == MOD_MOD_PROP the prop must be in the LFSLL, meaning
                             * we either need the curr pointer or there isn't an entry in
                             * the lkup_tbl for this property, or the op failed and the
                             * property was deleted.
                             * If op ==  MOD_DELETE_PROP and there is an entry in the 
                             * lkup_tbl it must be DELETED, or be a curr pointer with a
                             * version greater than the one we deleted. 
                             */
                            assert(FALSE);
                        }  
                    }

                } /* if ( base_del_ver > 0 ) */

            } /* if ( first_curr_ver == 0 || first_curr_ver > obj_ver ) */
            else
            {
                prop_ref = atomic_load(&(lkup_entry->curr));
                prop     = prop_ref.ptr;
                break;
            }
        
        } /* end if ( lkup_entry->chksum == prop_entry->chksum ) */

    } /* for ( size_t i = 0; i < list->nprops_inherited; i++ ) */

    /* If we didn't find the prop deleted in the lkup_tbl */
    if ( ret_value == 0 )
    {
        /* If there wasn't an entry for the prop in the lkup_tbl */
        if ( ! prop )
        {
            prop = get_prop_from_lfsll(list->pl_head, prop_entry->name, curr_ver);
        }
        CHECK_PTR(prop, "grabbed prop from lkup_tbl/get_prop_from_lfsll");
        assert(prop);

        create_ver = atomic_load(&(prop->create_version));

        /* If the property's create_version is > the version we searched */
        if ( create_ver > obj_ver )
        {
            /* Check for older versions in the LFSLL */
            while ( create_ver > obj_ver && prop->chksum == prop_entry->chksum )
            {
                next = atomic_load(&(prop->next));
                prop = next.ptr;

                create_ver = atomic_load(&(prop->create_version));
            }

            /**
             * If iterated to another property, the prop we searched
             * for at the version we searched at doesn't exist.
             */
            if ( prop->chksum != prop_entry->chksum )
            {
                ret_value = 0;
            }
        }

        /**
         * If TRUE, there was a version of the prop at the version we
         * searched, but it must have been deleted. Double check.
         */
        if ( create_ver <= obj_ver && prop->chksum == prop_entry->chksum )
        {
            delete_ver = atomic_load(&(prop->delete_version));
            
            if ( delete_ver > 0 && delete_ver <= obj_ver )
            {
                deleted = TRUE;
                ret_value = obj_ver;
            }
            else if ( delete_ver > 0 && delete_ver <= curr_ver )
            {
                deleted = TRUE;
                ret_value = delete_ver;
            }
            else
            {
                assert(FALSE);
                //ret_value = 0;
            }
        }

    } /* end if ( delete == FALSE )*/

    if ( ret_value > 0 )
    {
        assert(deleted);
    }

    *_deleted = deleted;

    return(ret_value);

} /* end prop_ver_check_list() */




/****************************************************************************************
 * Function:    create_operation_log
 *
 * Purpose:     Creates a array of test_op_info_t structs that is used to store every 
 *              operation on a specific class or list that every thread performed.
 * 
 *                                              
 * Return:      Pointer to an array of test_op_info_t
 *
 ****************************************************************************************
 */
test_op_info_t *
create_operation_log(uint64_t op_count)
{
    test_op_info_t *ret_value = NULL;

    ret_value = (test_op_info_t *)malloc(op_count * sizeof(test_op_info_t));

    for (uint64_t i = 0; i < op_count; i++) {
        ret_value[i].class        = NULL;
        ret_value[i].list         = NULL;
        ret_value[i].id           = H5I_INVALID_HID;
        ret_value[i].class_name   = NULL;
        ret_value[i].parent_name  = NULL;
        ret_value[i].prop_name    = NULL;
        ret_value[i].op           = NO_OP_YET;
        ret_value[i].op_num       = 0;
        ret_value[i].obj_ver      = 0;
        ret_value[i].op_ver       = 0;
        ret_value[i].result       = OP_SUCCESS;
        ret_value[i].obj_isa_copy = FALSE;
        ret_value[i].sorted       = FALSE;
    }

    return (ret_value);

} /* end create_operation_log() */

/****************************************************************************************
 * Function:    op_rank
 *
 * Purpose:     Ranks the operation with an interger value to aid in sorting all 
 *              operations performed on a class or list.
 * 
 *                                              
 * Return:      int (cannot fail)
 *
 ****************************************************************************************
 */
static inline int
op_rank(operation_type_t op)
{
    switch (op) {
        case CREATE:
        case COPY:
            return 0;
        case SEARCH:
        case SEARCH_VER:
            return 3;
        case MOD_CREATE_PROP:
        case MOD_MOD_PROP:
        case MOD_DELETE_PROP:
            return 2;
        case CMP:
            return 3;
        case DELETE:
            return 1;
        default:
            assert(FALSE);
    }

} /* end op_rank() */

/****************************************************************************************
 * Function:    sort_op_log
 *
 * Purpose:     Sorts the operations performed on a class or list. Uses op_rank() which
 *              ranks the operations to aid with the sorting.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static test_op_info_t *
sort_op_log(test_op_info_t *op_log, uint64_t op_count)
{
    int              i;
    int              j;
    test_op_info_t   key;
    uint64_t         key_ver;
    uint64_t         j_ver;
    operation_type_t key_op;
    int              key_rank;
    int              j_rank;

    for (i = 1; (uint64_t)i < op_count; i++) {
        key      = op_log[i];
        key_ver  = key.obj_ver;
        key_op   = key.op;
        key_rank = op_rank(key_op);

        j = i - 1;

        while (j >= 0) {
            j_ver = op_log[j].obj_ver;

            /* Primary: sort by obj_ver */
            if (j_ver > key_ver) {
                op_log[j + 1] = op_log[j];
            }
            /* Secondary: sort by op */
            else if (j_ver == key_ver) {
                j_rank = op_rank(op_log[j].op);

                if (j_rank > key_rank) {
                    op_log[j + 1] = op_log[j];
                }
                /* Tertiary: sort by result */
                else if (j_rank == key_rank && j_rank == 0) {
                    if (op_log[j].result > key.result) {
                        op_log[j + 1] = op_log[j];
                    }
                    else {
                        break;
                    }
                }
                else {
                    break;
                }
            }
            else {
                break;
            }

            j--;
        }

        op_log[j + 1] = key;

    } /* end for ( i = 1; i < op_count; i++ ) */

    return (op_log);

} /* end sort_op_log() */

/****************************************************************************************
 * Function:    get_prop_from_lfsll
 *
 * Purpose:     Gets a property from a LFSLL regardless of whether the class or list 
 *              struct is valid and regardless of whether the property is valid. 
 * 
 *                                              
 * Return:      Pointer to the H5P_mt_prop_t
 *              NULL if the property isn't in the LFSLL
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
get_prop_from_lfsll(H5P_mt_prop_t *pl_head, const char *name, uint64_t version)
{
    H5P_mt_prop_t     *prop = NULL; /* Prop being searched for */
    H5P_mt_prop_aptr_t next;
    int64_t            chksum; /* Chksum for the prop from name */
    uint64_t           prop_version;
    bool               done = FALSE;

    H5P_mt_prop_t *ret_value = NULL;

    assert(name);
    assert(pl_head);
    assert(atomic_load(&(pl_head->tag)) == H5P_MT_PROP_TAG ||
           atomic_load(&(pl_head->tag)) == H5P_MT_PROP_INVALID_TAG);

    chksum = H5_checksum_metadata(name, strlen(name), 0);

    prop = pl_head;

    do {
        next = atomic_load(&(prop->next));
        prop = next.ptr;
        assert(prop);

        if (prop->chksum == chksum) {
            if (0 == strcmp(name, prop->name)) {
                prop_version = atomic_load(&(prop->create_version));

                if (prop_version <= version) {
                    ret_value = prop;
                    done      = TRUE;
                }
            }
        }
        else if (prop->chksum > chksum) {
            done = TRUE;
        }

    } while (!done);

    assert(done);

    return (ret_value);

} /* end get_prop_from_lfsll() */

/****************************************************************************************
 * Function:    get_prop_from_lkup_tbl
 *
 * Purpose:     Gets a property from a lkup_tbl regardless of whether the list struct is 
 *              valid and regardless of whether the property is valid. If the property is
 *              the base and it's in the parent class's LFSLL the _base_flag is set to 
 *              TRUE so the calling functions knows.
 * 
 *                                              
 * Return:      Pointer to the H5P_mt_prop_t
 *              or NULL if the property isn't in the lkup_tbl
 *
 ****************************************************************************************
 */
H5P_mt_prop_t *
get_prop_from_lkup_tbl(H5P_mt_list_t *list, const char *name, uint64_t version, bool *_base_flag)
{
    H5P_mt_list_table_entry_t *entry;
    H5P_mt_list_prop_ref_t     prop_ref;
    H5P_mt_prop_t             *prop = NULL;
    H5P_mt_prop_aptr_t         next;
    size_t                     i;
    int64_t                    chksum;
    uint64_t                   first_curr;
    uint64_t                   create_ver;
    bool                       base_flag = FALSE;

    H5P_mt_prop_t *ret_value = NULL;

    assert(list);
    assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG ||
           atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);
    assert(name);

    chksum = H5_checksum_metadata(name, strlen(name), 0);

    /* Iterate the list's lkup_tbl for the property */

    for (i = 0; i < list->nprops_inherited; i++) {
        entry = &list->lkup_tbl[i];

        if (entry->chksum == chksum) {
            /* If chksums equal ensure names are also equal */
            assert(0 == strcmp(entry->name, name));

            first_curr = atomic_load(&(entry->first_ver_of_curr));

            /**
             * If the first version of the list where curr.ptr wasn't NULL is
             * <= version, then the property is in the list's LFSLL and isn't
             * base.ptr pointing to the property from the parent's LFSLL.
             */
            if (first_curr > 0 && first_curr <= version) {
                prop_ref = atomic_load(&(entry->curr));
                prop     = prop_ref.ptr;

                do {
                    create_ver = atomic_load(&(prop->create_version));

                    if (create_ver > version) {
                        next = atomic_load(&(prop->next));
                        prop = next.ptr;
                    }

                } while (create_ver > version);
            }
            /* Else, the property is the base version from the parent's LFSLL */
            else {
                prop_ref = atomic_load(&(entry->base));
                prop     = prop_ref.ptr;

                base_flag = TRUE;
            }

            assert(prop);
            assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG ||
                   atomic_load(&(prop->tag)) == H5P_MT_PROP_INVALID_TAG);
            VERIFY(prop->chksum, chksum, "verify_prop_in_list");
            assert(prop->chksum == chksum);
            assert(0 == strcmp(prop->name, name));

            break;

        } /* end if ( entry->chksum == chksum ) */
        else if (entry->chksum > chksum) {
            /* If entry->chksum > chksum, the property isn't in the lkup_tbl */
            break;
        }

    } /* end for ( nprops = 0; nprops < list->nprops_inherited; nprops++ ) */

    *_base_flag = base_flag;

    ret_value = prop;

    return (ret_value);

} /* end get_prop_from_lkup_tbl() */

/****************************************************************************************
 * Function:    verify_prop_in_class
 *
 * Purpose:     Verifies the result of a class operation involving a property 
 *              (SEARCH, SEARCH_VER, MOD_CREATE_PROP, MOD_MOD_PROP, MOD_DELETE_PROP).
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
verify_prop_in_class(H5P_mt_class_t *class, const char *name, uint64_t version, operation_type_t op,
                     op_result_t result)
{
    H5P_mt_class_t     *parent     = NULL;
    H5P_mt_prop_t      *prop       = NULL;
    H5P_mt_prop_t      *check_prop = NULL;
    H5P_mt_prop_value_t value;
    int64_t             chksum;
    uint64_t            create_ver;
    uint64_t            check_ver; /* check_prop create version */
    uint64_t            delete_ver;
    uint64_t            parent_ver;
    int                 ret;

    herr_t ret_value = SUCCEED;

    assert(class);
    assert(name);
    assert(op == SEARCH || op == SEARCH_VER || op == MOD_CREATE_PROP || op == MOD_MOD_PROP ||
           op == MOD_DELETE_PROP);
    assert(result == OP_SUCCESS || result == PROP_ALREADY_EXISTS || result == PROP_DELETED ||
           result == PROP_DOESNT_EXIST);

    /* Gets the property from the class at the specified version, if it exists */
    chksum = H5_checksum_metadata(name, strlen(name), 0);

    prop = get_prop_from_lfsll(class->pl_head, name, version);

    /* If there was a property at the provided verison (regardless of validity) */
    if (prop) {
        assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG ||
               atomic_load(&(prop->tag)) == H5P_MT_PROP_INVALID_TAG);
        VERIFY(prop->chksum, chksum, "verify_prop_in_class");
        assert(prop->chksum == chksum);
        assert(0 == strcmp(prop->name, name));

        create_ver = atomic_load(&(prop->create_version));
        delete_ver = atomic_load(&(prop->delete_version));
        value      = atomic_load(&(prop->value));

        ret = memcmp(&create_ver, value.ptr, value.size);

        parent_ver = class->parent_version;
        parent = class->parent_ptr;

        /**
         * The only way value.ptr could be larger than check_ver is if this property
         * was inherited from the parent class at a version greater than 1.
         * The property in the parent if created at a version greater than 1 would 
         * have that version as the value and then the derived class or list would
         * create that property at version 1, when the derived object was created 
         * and inherited the property. 
         * NOTE: A list would only create the property if had a create callback,
         * else the list would just point to it's parent property from it's lkup_tbl.
         */
        while (ret < 0) {
            check_prop = get_prop_from_lfsll(parent->pl_head, name, parent_ver);

            check_ver = atomic_load(&(check_prop->create_version));
            value     = atomic_load(&(check_prop->value));

            ret = memcmp(&check_ver, value.ptr, value.size);

            parent_ver = parent->parent_version;
            parent = parent->parent_ptr;
        }

        if (result == OP_SUCCESS) {
            if (op == MOD_DELETE_PROP) {
                VERIFY(delete_ver, version, "verify_prop_in_class");
                assert(delete_ver == version);
            }

            /**
             * If op is anything else, the property has
             * already been confirmed to be correct above.
             */
        }
        else if (result == PROP_DELETED) {
            /* The op MOD_CREATE_PROP is the only one that can NOT have this result */
            assert(op != MOD_CREATE_PROP);

            assert(delete_ver <= version);
        }
        else if (result == PROP_ALREADY_EXISTS) {
            /* The op MOD_CREATE_PROP is the only one that CAN have this result */
            VERIFY(op, MOD_CREATE_PROP, "verify_prop_in_class");
            assert(op == MOD_CREATE_PROP);

            assert(create_ver <= version);
        }
        else {
            assert(FALSE);
        }

    } /* end if ( prop ) */
    else {
        /* The op MOD_CREATE_PROP is the only one that can NOT have this result */
        CHECK(op, MOD_CREATE_PROP, "verify_prop_in_class");
        assert(op != MOD_CREATE_PROP);

        CHECK_PTR_NULL(prop, "verify_prop_in_class");
        assert(!prop);
    }

    return (ret_value);

} /* end verify_prop_in_class() */

/****************************************************************************************
 * Function:    verify_prop_in_list
 *
 * Purpose:     Verifies the result of a list operation involving a property 
 *              (SEARCH, SEARCH_VER, MOD_CREATE_PROP, MOD_MOD_PROP, MOD_DELETE_PROP).
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
herr_t
verify_prop_in_list(H5P_mt_list_t *list, const char *name, uint64_t version, operation_type_t op,
                    op_result_t result)
{
    H5P_mt_list_table_entry_t *entry;
    H5P_mt_class_t            *parent1    = NULL;
    H5P_mt_class_t            *parent2    = NULL;
    H5P_mt_prop_t             *prop       = NULL;
    H5P_mt_prop_t             *check_prop = NULL;
    H5P_mt_prop_value_t        value;
    int64_t                    chksum;
    uint64_t                   check_ver = 0;
    uint64_t                   create_ver = 0;
    uint64_t                   delete_ver;
    bool                       base_flag = FALSE;
    int                        ret; /* Generic return value */

    herr_t ret_value = SUCCEED;

    assert(list);
    assert(name);
    assert(op == SEARCH || op == SEARCH_VER || op == MOD_CREATE_PROP || op == MOD_MOD_PROP ||
           op == MOD_DELETE_PROP);
    assert(result == OP_SUCCESS || result == PROP_ALREADY_EXISTS || result == PROP_DELETED ||
           result == PROP_DOESNT_EXIST);

    chksum = H5_checksum_metadata(name, strlen(name), 0);

    prop = get_prop_from_lkup_tbl(list, name, version, &base_flag);

    /* If TRUE, grab the lkup_tbl entry */
    if (base_flag) {
        for (size_t i = 0; i < list->nprops_inherited; i++) {
            entry = &list->lkup_tbl[i];

            if (entry->chksum == chksum) {
                assert(entry->chksum == prop->chksum);

                break;
            }
        }
    }

    /* If no prop yet, search the list's LFSLL */
    if (!prop) {
        prop = get_prop_from_lfsll(list->pl_head, name, version);
    }

    if (prop) {
        assert(prop);
        assert(atomic_load(&(prop->tag)) == H5P_MT_PROP_TAG ||
               atomic_load(&(prop->tag)) == H5P_MT_PROP_INVALID_TAG);
        VERIFY(prop->chksum, chksum, "verify_prop_in_list");
        assert(prop->chksum == chksum);
        assert(0 == strcmp(prop->name, name));

        create_ver = atomic_load(&(prop->create_version));

        if (prop->in_prop_class) {
            delete_ver = atomic_load(&(entry->base_delete_version));
        }
        else {
            delete_ver = atomic_load(&(prop->delete_version));
        }
        value = atomic_load(&(prop->value));

        ret = memcmp(&create_ver, value.ptr, value.size);

        /** 
         * If in_prop_class is TRUE, the property is the base in the list's
         * lkup_tbl, and thus is actually in the parent class's LFSLL. 
         * double check.
         */
        if ( prop->in_prop_class )
        {
            parent1 = list->pclass_ptr;

            check_prop = get_prop_from_lfsll(parent1->pl_head, name, list->pclass_version);

            check_ver = atomic_load(&(check_prop->create_version));
            value     = atomic_load(&(check_prop->value));

            ret = memcmp(&check_ver, value.ptr, value.size);

            /**
             * The only way value.ptr could be larger than check_ver is if this property
             * was inherited from the parent class at a version greater than 1.
             * The property in the parent if created at a version greater than 1 would 
             * have that version as the value and then the derived class or list would
             * create that property at version 1, when the derived object was created 
             * and inherited the property. 
             * NOTE: A list would only create the property if had a create callback,
             * else the list would just point to it's parent property from it's lkup_tbl.
             */
            while ( ret < 0 )
            {
                parent2 = parent1->parent_ptr;

                check_prop = get_prop_from_lfsll(parent2->pl_head, name, 
                                                    parent1->parent_version);
                
                check_ver = atomic_load(&(check_prop->create_version));
                value     = atomic_load(&(check_prop->value));

                ret = memcmp(&check_ver, value.ptr, value.size);

                parent1 = parent2;
            }
        
        } /* end if ( prop->in_prop_class ) */

        if (result == OP_SUCCESS) {
            if (op == MOD_DELETE_PROP) {
                VERIFY(delete_ver, version, "verify_prop_in_list");
                assert(delete_ver == version);
            }
            /**
             * If op is anything else, the property has
             * already been confirmed to be correct above.
             */
        }
        else if (result == PROP_DELETED) {
            /* The op MOD_CREATE_PROP is the only one that can NOT have this result */
            assert(op != MOD_CREATE_PROP);

            assert(delete_ver <= version);
        }
        else if (result == PROP_ALREADY_EXISTS) {
            /* The op MOD_CREATE_PROP is the only one that CAN have this result */
            VERIFY(op, MOD_CREATE_PROP, "verify_prop_in_list");
            assert(op == MOD_CREATE_PROP);

            if ( create_ver > version )
            {
                assert(create_ver <= list->pclass_version);
            }
        }
        else {
            assert(FALSE);
        }
    } /* end if ( prop ) */
    else {
        /* The op MOD_CREATE_PROP is the only one that can NOT have this result */
        CHECK(op, MOD_CREATE_PROP, "verify_prop_in_list");
        assert(op != MOD_CREATE_PROP);

        CHECK_PTR_NULL(prop, "verify_prop_in_list");
        assert(!prop);
    }

    return (ret_value);

} /* end verify_prop_in_list() */

/****************************************************************************************
 * Function:    check_operations
 *
 * Purpose:     Iterates through every entry in the class table and list_table and 
 *              creates an array of all operations that every thread performed on each
 *              entry and sorts the array of operations in order performed. Then 
 *              iterating that array and checking that each operation was performed 
 *              correctly and all the operation's information is accurate.
 * 
 *                                              
 * Return:      SUCCEED/FAIL
 *
 ****************************************************************************************
 */
static herr_t
check_operations(thread_params_t *thread_params, uint64_t num_threads)
{
    uint64_t             op_count;
    uint64_t             class_op_count;
    uint64_t             list_op_count;
    class_table_entry_t *class_entry;
    class_table_entry_t *og_class_entry = NULL;
    class_table_entry_t *parent_entry;
    list_table_entry_t  *list_entry;
    list_table_entry_t  *og_list_entry;
    H5P_mt_class_sptr_t  class_sptr;
    H5P_mt_list_sptr_t   list_sptr;
    H5P_mt_class_t  *class;
    H5P_mt_class_t  *parent;
    thread_params_t *thread_ops;
    test_op_info_t  *op_info = NULL;
    op_result_t      result;
    H5P_mt_list_t   *list;
    test_op_info_t  *op_log = NULL;
    test_op_info_t  *curr_op;
    operation_type_t op;
    //status_t         class_status;
    //status_t         list_status;
    //status_t         parent_status;
    //uint64_t         ver_copied;
    uint64_t         ver_closed;
    uint64_t         ver_deleted;

    bool created = FALSE;
    int  ret;

    assert(thread_params);
    assert(OPS_PERFORMED == (TOTAL_OPS_PER_THREAD * num_threads));

    /**
     * Iterate through every possible class and ensure all operations performed on
     * it were actually performed, and were performed in some trackable order.
     */
    for (int i = 0; i < CLASS_TABLE_SIZE; i++) {
        class_entry = &class_table[i];
        class_sptr  = atomic_load(&(class_entry->class_sptr));
        class       = class_sptr.ptr;

        class_op_count = atomic_load(&(class_entry->op_count));

        if (class_op_count > 0) {
            op_log = create_operation_log(class_op_count);
            assert(op_log);
        }

        op_count = 0;

        for (uint64_t op_num = 0; op_num < TOTAL_OPS_PER_THREAD; op_num++) {
            for (uint64_t thread = 0; thread < num_threads; thread++) {
                thread_ops = &thread_params[thread];
                VERIFY(thread_ops->ops_performed, TOTAL_OPS_PER_THREAD, "check_operations");
                assert(thread_ops->ops_performed == TOTAL_OPS_PER_THREAD);

                op_info = &thread_ops->op_table[op_num];

                /* If the op hasn't been sorted yet and was done on a class */
                if (op_info->sorted == FALSE && op_info->test_id == class_entry->test_class_id) {
                    assert(op_count <= class_op_count);

                    if (class) {
                        /* Ensure basic fields match */
                        if (0 != strcmp(class->name, op_info->class_name)) {
                            TestErrPrintf("class%d name mismatch\n", i);
                            assert(FALSE);
                        }
                        if (op_info->id != H5I_INVALID_HID) {
                            if (atomic_load(&(class->id)) != op_info->id) {
                                TestErrPrintf("class%d id mismatch\n", i);
                                assert(FALSE);
                            }
                        }
                        if (atomic_load(&(class->id)) != atomic_load(&(class_entry->id))) {
                            TestErrPrintf("class%d id mismatch\n", i);
                            assert(FALSE);
                        }
                        if (class->parent_id != atomic_load(&(class_entry->parent_id))) {
                            TestErrPrintf("class%d parent id mismatch\n", i);
                            assert(FALSE);
                        }
                        parent = class->parent_ptr;
                        if (0 != strcmp(parent->name, class_entry->parent_name) ||
                            0 != strcmp(parent->name, op_info->parent_name)) {
                            TestErrPrintf("class%d parent name mismatch\n", i);
                            assert(FALSE);
                        }

                    } /* end if ( class )*/

                    op_info->sorted  = TRUE;
                    op_log[op_count] = *op_info;
                    op_count++;

                } /* end if ( op_info->sorted == FALSE &&
                              op_info->test_id == class_entry->test_class_id )*/

            } /* end for ( uint64_t thread = 0; thread < num_threads; thread++ ) */

            /* If all ops on a class have been accounted for, no need to keep searching */
            if (op_count == class_op_count) {
                break;
            }
            else if (op_count > class_op_count) {
                assert(FALSE);
            }

        } /* end for ( uint64_t op = 0; op < TOTAL_OPS_PER_THREAD; op++ ) */

        /**
         * Must account for the first four classes being created prior to the tests
         * which were not performed by any of the test threads, thus weren't included
         * in the thread_params.
         */
        if (i < 4) {
            op_log[class_op_count - 1].class       = class;
            op_log[class_op_count - 1].id          = atomic_load(&(class_entry->id));
            op_log[class_op_count - 1].test_id     = i;
            op_log[class_op_count - 1].class_name  = class->name;
            op_log[class_op_count - 1].parent_name = class_entry->parent_name;
            op_log[class_op_count - 1].op          = CREATE;
            op_log[class_op_count - 1].obj_ver     = 1;
            op_log[class_op_count - 1].sorted      = TRUE;

            op_count++;
        }

        /* Ensure the number of operations is correct */
        VERIFY(op_count, class_entry->op_count, "check_operations");
        assert(op_count == class_entry->op_count);

        /* Sort op_log in ascending version order and by operation */
        if (op_count > 1) {
            op_log = sort_op_log(op_log, op_count);
            CHECK_PTR(op_log, "sort_op_log");
            assert(op_log);
        }

        assert(op_info);

        if (class_entry->copy) {
            int k = 0;
            do {
                k++;
                og_class_entry = &class_table[i - k];

            } while (og_class_entry->copy);
        }

        //class_status = atomic_load(&(class_entry->status));

        created = FALSE;

        /* Iterate through each op in the class log */
        for (int j = 0; (uint64_t)(j) < op_count; j++) {
            curr_op = &op_log[j];

            op     = curr_op->op;
            result = curr_op->result;

            class = curr_op->class;
            
            //ver_copied  = atomic_load(&(class_entry->ver_copied));
            ver_closed  = atomic_load(&(class_entry->ver_closed));
            ver_deleted = atomic_load(&(class_entry->ver_deleted));

            /**
             * If the class wasn't created yet, all ops
             * (except CREATE/COPY) should've failed.
             */
            if (created == FALSE) {
                /* If creating the class */
                if (op == CREATE && !class_entry->copy) {
                    if (result == OP_SUCCESS) {
                        CHECK_PTR(class, "check_operations");
                        assert(class);
                        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG ||
                               atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);

                        created = TRUE;
                    }
                    else if (result == PARENT_DOESNT_EXIST || result == PARENT_DELETED) {
                        if (result == PARENT_DELETED) {
                            parent_entry = class_entry->parent_entry;
                            ver_deleted = atomic_load(&(parent_entry->ver_deleted));

                            assert(ver_deleted <= curr_op->obj_ver);
                        }
                    }
                    /* No other results should happen */
                    else {
                        assert(FALSE);
                    }

                } /* end if ( ! class_entry->copy && op == CREATE ) */
                /* If creating a copy */
                else if (op == COPY && class_entry->copy) {
                    if (result == OP_SUCCESS) {
                        CHECK_PTR(class, "check_operations");
                        assert(class);
                        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_TAG ||
                               atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);

                        created = TRUE;
                    }
                    else if (result == OG_DOESNT_EXIST || result == OG_DELETED) {
                        if (result == OG_DELETED) {
                            ret = strcmp(class_entry->name, og_class_entry->name);

                            VERIFY(ret, 0, "strcmp");
                            assert(ret == 0);

                            ver_deleted = atomic_load(&(og_class_entry->ver_deleted));

                            assert(ver_deleted <= curr_op->obj_ver);
                        }
                    }
                    /* No other results should happen */
                    else {
                        assert(FALSE);
                    }

                }    /* end else if ( class_entry->copy && op == COPY ) */
                else /* All other ops should have failed */
                {
                    CHECK(result, OP_SUCCESS, "check_operations: OP_SUCCESS when it shouldn't be");
                    assert(result != OP_SUCCESS);
                }

            }    /* end if ( class_created == FALSE ) */
            else /* class_created == TRUE */
            {
                /* Trying to create an already created class should have failed */
                if (op == CREATE || op == COPY) {
                    assert(result == CLASS_ALREADY_EXISTS || result == CLASS_DELETED);

                    CHECK(curr_op->obj_ver, 0, "check_operations");
                    assert(curr_op->obj_ver > 0);
                }
                else if (op == SEARCH || op == SEARCH_VER || op == MOD_CREATE_PROP || op == MOD_MOD_PROP ||
                         op == MOD_DELETE_PROP) {
                    if (result == CLASS_DELETED) {
                        assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);
                        assert(curr_op->obj_ver == ver_deleted);
                        assert(ver_deleted == atomic_load(&(class->curr_version)));
                    }
                    else {
                        ret = verify_prop_in_class(class, curr_op->prop_name, curr_op->op_ver, op, result);
                        CHECK_I(ret, "verify_prop_in_class");
                        assert(ret == SUCCEED);
                    }
                }
                else if (op == CMP) {
                    if (result == CLASS_DELETED) {
                        assert(curr_op->obj_ver >= ver_deleted);
                    }
                }
                else if (op == DELETE) {
                    if (result == OP_SUCCESS) {
                        VERIFY(curr_op->obj_ver, ver_closed,
                               "check_operations: bad class_entry->ver_closed");
                        assert(curr_op->obj_ver == ver_closed);

                        if (ver_deleted > 0) {
                            VERIFY(atomic_load(&(class->tag)), H5P_MT_CLASS_INVALID_TAG,
                                   "check_operations: bad class->tag");
                            assert(atomic_load(&(class->tag)) == H5P_MT_CLASS_INVALID_TAG);

                            assert(ver_deleted == atomic_load(&(class->curr_version)));
                        }
                    }
                    else if (result == CLASS_DELETED) {
                        assert(curr_op->obj_ver >= ver_closed);
                    }
                    else {
                        assert(FALSE);
                    }

                } /* end else if ( op == DELETE ) */

            } /* end else created == TRUE */

        } /* end for ( int j = 0; j < op_count; j++ ) */

        if (op_log) {
            free(op_log);
            op_log = NULL;
        }

    } /* end for ( int i = 0; i < CLASS_TABLE_SIZE; i++ ) */

    /**
     * Iterate through every possible list and ensure all operations performed on
     * it were actually performed, and were performed in some trackable order.
     */
    for (int i = 0; i < LIST_TABLE_SIZE; i++) {
        list_entry = &list_table[i];
        list_sptr  = atomic_load(&(list_entry->list_sptr));
        list       = list_sptr.ptr;

        list_op_count = atomic_load(&(list_entry->op_count));

        if (list_op_count > 0) {
            op_log = create_operation_log(list_op_count);
            assert(op_log);
        }

        op_count = 0;

        for (uint64_t op_num = 0; op_num < TOTAL_OPS_PER_THREAD; op_num++) {
            for (uint64_t thread = 0; thread < num_threads; thread++) {
                thread_ops = &thread_params[thread];
                VERIFY(thread_ops->ops_performed, TOTAL_OPS_PER_THREAD, "check_operations");
                assert(thread_ops->ops_performed == TOTAL_OPS_PER_THREAD);

                op_info = &thread_ops->op_table[op_num];

                /* If the op hasn't been sorted yet and was done on a list */
                if (op_info->sorted == FALSE && op_info->test_id == list_entry->test_list_id) {
                    assert(op_count <= list_op_count);

                    /**
                     * This is so we only check these for the list if the list was ever
                     * actually created.
                     *
                     * NOTE: this is only for early debugging. In later debugging the if (list)
                     * will most likely get removed, because all classes and lists will be created.
                     */
                    if (list) {
                        /* Ensure basic fields match */
                        if (op_info->id != H5I_INVALID_HID) {
                            if (atomic_load(&(list->plist_id)) != op_info->id) {
                                TestErrPrintf("list%d id mismatch\n", i);
                                assert(FALSE);
                            }
                        }
                        if (atomic_load(&(list->plist_id)) != atomic_load(&(list_entry->id))) {
                            TestErrPrintf("list%d id mismatch\n", i);
                            assert(FALSE);
                        }
                        if (list->pclass_id != atomic_load(&(list_entry->parent_id))) {
                            TestErrPrintf("list%d parent id mismatch\n", i);
                            assert(FALSE);
                        }
                        parent = list->pclass_ptr;
                        if (0 != strcmp(parent->name, list_entry->parent_name) ||
                            0 != strcmp(parent->name, op_info->parent_name)) {
                            TestErrPrintf("list%d parent name mismatch\n", i);
                            assert(FALSE);
                        }

                    } /* end if ( list ) */

                    op_info->sorted  = TRUE;
                    op_log[op_count] = *op_info;
                    op_count++;

                } /* end if ( op_info->sorted == FALSE &&
                              op_info->test_id == list_entry->test_list_id ) */

            } /* end for ( uint64_t thread = 0; thread < num_threads; thread++ ) */

            /* If all ops on a list have been accounted for, no need to keep searching */
            if (op_count == list_op_count) {
                break;
            }
            else if (op_count > list_op_count) {
                assert(FALSE);
            }

        } /* end for ( uint64_t op_num = 0; op_num < TOTAL_OPS_PER_THREAD; op_num++ ) */

        /**
         * Must account for the first three and the fifth lists being created prior to
         * the tests which were not performed by any of the threads, thus wasn't
         * included in the thread_params, so set it up now.
         */
        if (i < 3 || i == 4) {
            op_log[list_op_count - 1].list        = list;
            op_log[list_op_count - 1].id          = atomic_load(&(list_entry->id));
            op_log[list_op_count - 1].test_id     = 100 + i;
            op_log[list_op_count - 1].parent_name = list_entry->parent_name;
            op_log[list_op_count - 1].op          = CREATE;
            op_log[list_op_count - 1].obj_ver     = 1;
            op_log[list_op_count - 1].sorted      = TRUE;

            op_count++;
        }

        /* Ensure the number of operations is correct */
        VERIFY(op_count, list_entry->op_count, "check_operations");
        assert(op_count == list_entry->op_count);

        /* Sort op_log in ascending version order and by operation */
        if (op_count > 1) {
            op_log = sort_op_log(op_log, op_count);
            CHECK_PTR(op_log, "sort_op_log");
            assert(op_log);
        }

        assert(op_info);

        if (list_entry->copy) {
            int k = 0;
            do {
                k++;
                og_list_entry = &list_table[i - k];

            } while (og_list_entry->copy);
        }

        created = FALSE;

        /* Iterate through each op in the list log */
        for (int j = 0; (uint64_t)(j) < op_count; j++) {
            curr_op = &op_log[j];

            op     = curr_op->op;
            result = curr_op->result;

            list = curr_op->list;

            //ver_copied  = atomic_load(&(list_entry->ver_copied));
            ver_deleted = atomic_load(&(list_entry->ver_deleted));

            /**
             * If the list wasn't created yet, all ops
             * (except CREATE/COPY) should've failed.
             */
            if (created == FALSE) {
                /* If creating the list */
                if (op == CREATE && !list_entry->copy) {
                    if (result == OP_SUCCESS) {
                        CHECK_PTR(list, "check_operations");
                        assert(list);
                        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG ||
                               atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);

                        created = TRUE;
                    }
                    else if (result == PARENT_DOESNT_EXIST || result == PARENT_DELETED) {
                        if (result == PARENT_DELETED) {
                            parent_entry = list_entry->parent_entry;
                            ver_deleted = atomic_load(&(parent_entry->ver_deleted));

                            assert(ver_deleted <= curr_op->obj_ver);
                        }
                    }
                    /* No other results should happen */
                    else {
                        assert(FALSE);
                    }

                } /* end if ( ! list_entry->copy && op == CREATE ) */
                /* If creating a copy */
                else if (op == COPY && list_entry->copy) {
                    if (result == OP_SUCCESS) {
                        CHECK_PTR(list, "check_operations");
                        assert(list);
                        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_TAG ||
                               atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);

                        created = TRUE;
                    }
                    else if (result == OG_DOESNT_EXIST || result == OG_DELETED) {
                        if (result == OG_DELETED) {
                            
                            ver_deleted = atomic_load(&(og_list_entry->ver_deleted));

                            assert(ver_deleted <= curr_op->obj_ver);
                        }
                    }
                    /* No other results should happen */
                    else {
                        assert(FALSE);
                    }

                }    /* end else if ( list_entry->copy && op == COPY ) */
                else /* All other ops should have failed */
                {
                    CHECK(result, OP_SUCCESS, "check_operations: OP_SUCCESS when it shouldn't be");
                    assert(result != OP_SUCCESS);
                }
            }    /* end if ( class_created == FALSE ) */
            else /* created == TRUE */
            {
                /* Trying to create an already created list should have failed */
                if (op == CREATE || op == COPY) {
                    assert(result == LIST_ALREADY_EXISTS || result == LIST_DELETED);

                    CHECK(curr_op->obj_ver, 0, "check_operations");
                    assert(curr_op->obj_ver > 0);
                }
                else if (op == SEARCH || op == SEARCH_VER || op == MOD_CREATE_PROP || op == MOD_MOD_PROP ||
                         op == MOD_DELETE_PROP) {
                    if (result == LIST_DELETED) {
                        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);
                        assert(curr_op->obj_ver == ver_deleted);
                        assert(ver_deleted == atomic_load(&(list->curr_version)));
                    }
                    else {
                        ret = verify_prop_in_list(list, curr_op->prop_name, curr_op->op_ver, op, result);
                        CHECK_I(ret, "verify_prop_in_list");
                        assert(ret == SUCCEED);
                    }
                }
                else if (op == CMP) {
                    if (result == LIST_DELETED) {
                        assert(curr_op->obj_ver >= ver_deleted);
                    }
                }
                else if (op == DELETE) {
                    if (result == OP_SUCCESS) {
                        VERIFY(curr_op->obj_ver, ver_deleted,
                               "check_operations: bad class_entry->ver_closed");
                        assert(curr_op->obj_ver == ver_deleted);

                        VERIFY(atomic_load(&(list->tag)), H5P_MT_LIST_INVALID_TAG,
                               "check_operations: bad list->tag");
                        assert(atomic_load(&(list->tag)) == H5P_MT_LIST_INVALID_TAG);
                    }
                    else if (result == LIST_DELETED) {
                        assert(curr_op->obj_ver == ver_deleted);
                    }
                    else {
                        assert(FALSE);
                    }

                } /* end else if ( op == DELETE ) */

            } /* end else created == TRUE */

        } /* end for ( int j = 0; (uint64_t)(j) < op_count; j++ ) */

        if (op_log) {
            free(op_log);
            op_log = NULL;
        }

    } /* end for ( int i = 0; i < LIST_TABLE_SIZE; i++ ) */

    return SUCCEED;

} /* end check_operations */

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

    /* Add tests */
    AddTest("test_h5p_mt_functions", st_test_1, NULL, reset_globals, NULL, 0, 0,
            "Single thread check of all H5P multithread functions");

    AddTest("test_h5p_mt_functions_mt", mt_test_1, NULL, reset_globals, NULL, 0, 0,
            "Simple multithread check of all H5P multithread functions");
#if 1
    AddTest("full_mt_h5p_test", mt_test_2, NULL, reset_globals, NULL, 0, 0,
            "Multithread test for H5P where thread collisions can occur");
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
