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
 * Purpose:	This file contains declarations which are visible only within
 *          the H5VL package.  Source files outside the H5VL package should
 *          include H5VLprivate.h instead.
 */

#if !(defined H5VL_FRIEND || defined H5VL_MODULE)
#error "Do not include this file outside the H5VL package!"
#endif

#ifndef H5VLpkg_H
#define H5VLpkg_H

/* Get package's private header */
#include "H5VLprivate.h" /* Generic Functions                    */

#ifdef H5_HAVE_MULTITHREAD
#include <stdatomic.h>
#endif

/* Other private headers needed by this file */

/**************************/
/* Package Private Macros */
/**************************/

/****************************/
/* Package Private Typedefs */
/****************************/

#ifdef H5_HAVE_MULTITHREAD

/****************************************************************************************
 *
 * struct H5VL_mt_t
 *
 * A single, global instance of H5VL_mt_t is used to collect all global variables that
 * required for the multi-thread version of H5VL.
 *
 * It is also used to maintain related statistics.
 *
 * Fields are discussed individually below.
 *
 *
 * H5VL_object_t free list:  Since pointers to instances of H5VL_object_t may persist
 * after the associated instance has been discarded, we must maintain a free list of
 * same to avoid accessing freed memory on the heap.
 *
 * vol_obj_fl_shead: Atomic instance of struct H5VL_mt_vol_obj_sptr_t, which contains
 *      a pointer (ptr) to the head of the id info free list, and a serial number (sn)
 *      which must be incremented each time a new value is assigned to vol_obj_fl_shead.
 *
 *      The objective here is to prevent ABA bugs.
 *
 *      Note that once initialized, the vol object free list will always contain at least
 *      one entry, and is logically empty if vol_obj_fl_shead.ptr == vol_obj_fl_stail.ptr
 *      != NULL.
 *
 * vol_obj_fl_stail: Atomic instance of struct H5VL_mt_vol_obj_sptr_t, which contains
 *      a pointer (ptr) to the tail of the vol object free list, and a serial number (sn)
 *      which must be incremented each time a new value is assigned to vol_obj_fl_stail.
 *
 *      The objective here is to prevent ABA bugs.
 *
 * vol_obj_fl_len: Atomic unsigned integer used to maintain a count of the number of
 *      entries in the vol object free list.  Note that due to the delay between free list
 *      insertions and deletions, and the update of this field, this count may be off
 *      for brief periods of time.
 *
 *      Recall that the free list must always contain at least one entry.  Thus, when
 *      correct, vol_obj_fl_len will be one greater than the number of entries on the
 *      free list.
 *
 * max_desired_vol_obj_fl_len: Unsigned integer field containing the desired maximum
 *      vol object free list length.  This is of necessity a soft limit as entries cannot
 *      be removed from the head of the free list unless they are re-allocable.
 *
 * vol_obj_next_sn: Atomic uint64_t containing the serial number for the next entry on the
 *      vol object free list. Whenever an entry is added to the vol object free list, that
 *      entry's serial_num field is set equal to vol_obj_next_sn and vol_obj_next_sn gets
 *      incremented.  The entry's serial_num field is compared to the vol_obj_max_realloc_sn
 *      field below, and is only reallocable if the serial_num field is lesser.
 *
 *      For this iteration the assumption is made that vol_obj_next_sn will not be
 *      incremented enough to overflow, but eventually this must be taken into account.
 *
 * vol_obj_max_realloc_sn: Atomic uint64_t containing one plus the maximum serial number that
 *      can be reallocated. When an entry's serial number is compared to this field, if
 *      it is less than this field, then it can be reallocated.
 *
 *      At present this field is not updated, which prevents H5VL_object_t reallocation.
 *      Address this, or choose another reallocation heuristic before the release
 *      version.
 *
 * vol_objs_active: Count of the number of instance of H5VL_object_t that have been
 *      allocated and not yet released to the free list.  This count is maintained
 *      so we can delay taking down the free list until all instances have been
 *      released to the free lists.
 *
 *      Note that due to the delay between updating the free list and updating this
 *      field, it value may be briefly incorrect when running multi-thread.  For
 *      now at least, this is a non-issue since we drop to a single thread during
 *      library shutdown.
 *
 *
 * Statistics:
 *
 * Object Free List Statistics:
 *
 * max_vol_obj_fl_len: Maximum number of entries that have resided on the vol object free
 *      list at any point during the current run.  In the multi-thread case, this number
 *      should be viewed as aproximate.
 *
 * num_vol_obj_structs_alloced_from_heap: Number of instances of H5VL_object_t
 *      allocated from the heap.
 *
 * num_vol_obj_structs_alloced_from_fl:  Number of times an instance of H5VL_object_t
 *      has been allocated from the vol object free list.
 *
 * num_vol_obj_structs_freed: Number of instances of H5VL_object_t that have been
 *      freed -- that is returned to the heap.
 *
 * num_vol_obj_structs_added_to_fl: Number of times an instance of H5VL_object_t
 *      has been added to the vol object free list.
 *
 * num_vol_obj_fl_head_update_cols: Number of vol object free list head update collisions.
 *
 * num_vol_obj_fl_tail_update_cols: Number of vol object free list tail update collisions.
 *
 * num_vol_obj_fl_append_cols: Number of collisions when appending an instance of
 *     H5VL_object_t to the vol object free list.
 *
 * num_vol_obj_fl_head_sn_is_zero: Number of times that the instance of H5VL_object_t
 *     at the head of the free list has serial number 0.  Note that thiw will trigger
 *     an assertion failure in the debug build.
 *
 * num_vol_obj_sn_assigned: Number of times that a serial number is assigned
 *     to an instance of H5VL_object_t.  This is done when an instance is added
 *     to the vol object free list.
 *
 * num_vol_obj_serial_num_resets:  Number of thimes that the serial number assinged
 *      to an instance of H5VL_object_t is reset.  This is done when an instance
 *      is removed from the free list just prior to being either reallocated to
 *      released to the heap.
 *
 * num_vol_obj_fl_alloc_req_denied_due_to_empty:  Number of times an alloc request
 *      for an instance of H5VL_object_t has had to be satisfied from the heap
 *      instead of the free list because the vol object free list is empty.  Recall
 *      that the vol object free list is logically empty when it contains only a single
 *      entry.
 *
 * num_vol_obj_fl_alloc_req_denied_due_to_no_reallocable_entries:  Number of times an
 *      alloc request for an instance of H5VL_object_t has had to be satisfied from
 *      the heap because the entry at the head of the free list has a serial number
 *      greater than or equal to object_max_realloc_sn.
 *
 * num_vol_obj_fl_frees_skipped_due_to_empty: Number of times that an instance of
 *      H5VL_object_t on the vol object free list is not released to the heap because
 *      the free list is empty.
 *
 * num_vol_obj_fl_frees_skipped_due_to_fl_too_small:  Number of times that an instance of
 *      H5VL_object_t on the vol object free list is not released to the heap because
 *      the free list length is less than max_desired_vol_obj_fl_len.
 *
 * num_object_fl_frees_skipped_due_to_no_reallocable_entries: Number of times that an
 *      instance of H5VL_object_t on the vol object free list is not released to the heap
 *      because the entry at the head of the free list has serial number greater than
 *      or equal to id_max_realloc_sn.
 *
 * H5VL__alloc_vol_obj__num_calls: Number of times that H5VL__alloc_vol_obj() is called
 *
 * H5VL__clear_vol_obj_free_list__num_calls: Number of times that
 *      H5VL__clear_vol_obj_free_list() is called.
 *
 * H5VL__discard_vol_obj__num_calls: Number of times that H5VL__discard_vol_obj() is
 *      called.
 *
 *
 ****************************************************************************************/

#define H5VL__MAX_DESIRED_OBJECT_FL_LEN 4ULL

typedef struct H5VL_mt_t {

    _Atomic H5VL_mt_vol_obj_sptr_t vol_obj_fl_shead;
    _Atomic H5VL_mt_vol_obj_sptr_t vol_obj_fl_stail;
    _Atomic uint64_t               vol_obj_fl_len;
    _Atomic uint64_t               max_desired_vol_obj_fl_len;
    _Atomic uint64_t               vol_obj_next_sn;
    _Atomic uint64_t               vol_obj_max_realloc_sn;
    _Atomic int64_t                vol_objs_active;

    /* Statistics: */

    /* object free list stats */
    _Atomic uint64_t max_vol_obj_fl_len;
    _Atomic uint64_t num_vol_obj_structs_alloced_from_heap;
    _Atomic uint64_t num_vol_obj_structs_alloced_from_fl;
    _Atomic uint64_t num_vol_obj_structs_freed;
    _Atomic uint64_t num_vol_obj_structs_added_to_fl;
    _Atomic uint64_t num_vol_obj_fl_tail_update_cols;
    _Atomic uint64_t num_vol_obj_fl_head_update_cols;
    _Atomic uint64_t num_vol_obj_fl_append_cols;
    _Atomic uint64_t num_vol_obj_fl_alloc_req_denied_due_to_empty;
    _Atomic uint64_t num_vol_obj_fl_head_sn_is_zero;
    _Atomic uint64_t num_vol_obj_sn_assigned;
    _Atomic uint64_t num_vol_obj_serial_num_resets;
    _Atomic uint64_t num_vol_obj_fl_alloc_req_denied_due_to_no_reallocable_entries;
    _Atomic uint64_t num_vol_obj_fl_frees_skipped_due_to_empty;
    _Atomic uint64_t num_vol_obj_fl_frees_skipped_due_to_fl_too_small;
    _Atomic uint64_t num_vol_obj_fl_frees_skipped_due_to_no_reallocable_entries;
    _Atomic uint64_t H5VL__alloc_vol_obj__num_calls;
    _Atomic uint64_t H5VL__clear_vol_obj_free_list__num_calls;
    _Atomic uint64_t H5VL__discard_vol_obj__num_calls;

} H5VL_mt_t;

#endif /* H5_HAVE_MULTITHREAD */

/*****************************/
/* Package Private Variables */
/*****************************/

#ifdef H5_HAVE_MULTITHREAD

/* This structure contains global fields specific to the multi-thread H5VL build. */
H5_DLLVAR H5VL_mt_t H5VL_mt_g;

#endif /* H5_HAVE_MULTITHREAD */

/******************************/
/* Package Private Prototypes */
/******************************/
H5_DLL herr_t  H5VL__set_def_conn(void);
H5_DLL hid_t   H5VL__register_connector(const void *cls, hbool_t app_ref, hid_t vipl_id);
H5_DLL hid_t   H5VL__register_connector_by_class(const H5VL_class_t *cls, hbool_t app_ref, hid_t vipl_id);
H5_DLL hid_t   H5VL__register_connector_by_name(const char *name, hbool_t app_ref, hid_t vipl_id);
H5_DLL hid_t   H5VL__register_connector_by_value(H5VL_class_value_t value, hbool_t app_ref, hid_t vipl_id);
H5_DLL htri_t  H5VL__is_connector_registered_by_name(const char *name);
H5_DLL htri_t  H5VL__is_connector_registered_by_value(H5VL_class_value_t value);
H5_DLL hid_t   H5VL__get_connector_id(hid_t obj_id, hbool_t is_api);
H5_DLL hid_t   H5VL__get_connector_id_by_name(const char *name, hbool_t is_api);
H5_DLL hid_t   H5VL__get_connector_id_by_value(H5VL_class_value_t value, hbool_t is_api);
H5_DLL hid_t   H5VL__peek_connector_id_by_name(const char *name);
H5_DLL hid_t   H5VL__peek_connector_id_by_value(H5VL_class_value_t value);
H5_DLL herr_t  H5VL__connector_str_to_info(const char *str, hid_t connector_id, void **info);
H5_DLL ssize_t H5VL__get_connector_name(hid_t id, char *name /*out*/, size_t size);
H5_DLL void    H5VL__is_default_conn(hid_t fapl_id, hid_t connector_id, hbool_t *is_default);
H5_DLL herr_t  H5VL__register_opt_operation(H5VL_subclass_t subcls, const char *op_name, int *op_val);
H5_DLL size_t  H5VL__num_opt_operation(void);
H5_DLL herr_t  H5VL__find_opt_operation(H5VL_subclass_t subcls, const char *op_name, int *op_val);
H5_DLL herr_t  H5VL__unregister_opt_operation(H5VL_subclass_t subcls, const char *op_name);
H5_DLL herr_t  H5VL__term_opt_operation(void);
H5_DLL void    H5VL__init_opt_operation_table(void);

/* Testing functions */
#ifdef H5VL_TESTING
H5_DLL herr_t H5VL__reparse_def_vol_conn_variable_test(void);
#endif /* H5VL_TESTING */

#endif /* H5VLpkg_H */
