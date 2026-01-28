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
 * Purpose: This file contains declarations which are visible only within
 *          the H5P package.  Source files outside the H5P package should
 *          include H5Pprivate.h instead.
 */

#include <assert.h>
#include <stddef.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdatomic.h>

#ifndef H5Ppkg_mt_H
#define H5Ppkg_mt_H

/* Get package's private header */
#include "H5Pprivate.h"

/* Other private headers needed by this file */
#include "H5SLprivate.h" /* Skip lists				*/

/**************************/
/* Package Private Macros */
/**************************/

/**
 * Macro used for testing so the program will stop executing if hit.
 */
#define H5P_MT_ASSERT_FAIL TRUE

/****************************/
/* Package Private Typedefs */
/****************************/

/* Structures for Properties */

typedef struct H5P_mt_prop_t H5P_mt_prop_t; /* Forward declaration */

/****************************************************************************************
 *
 * Structure:   H5P_mt_prop_aptr_t
 *
 * Description:
 *
 * Struct H5P_mt_prop_aptr_t is a structure designed to contain a pointer to an instance
 * of H5P_mt_prop_t and a deleted flag in a single atomic structure. This is necessary,
 * as instances of H5P_mt_prop_t will typically appear in lock free singly linked lists.
 *
 * For correct operation, these lists require the next pointer and the deleted flag to
 * be accessed and modified in a single atomic operation.
 *
 * With padding, this structure is 128 bits, which allows true atomic operation on
 * many (most?) modern CPUs. However, it this becomes a problem, we can obtain the
 * same effect by stealing the low order bit of the pointer for a deleted bit -- which
 * works on all CPU / C compiler combinations I have tried.
 *
 * Fields:
 *
 * ptr (struct H5P_mt_prop_t):
 *      Pointer to an instance of H5P_mt_prop_t, or NULL.
 *
 * deleted (bool):
 *      Boolean flag.  If this instance of H5P_mt_prop_aptr_t appears as a field
 *      in an instance of H5P_mt_prop_t and this flag is TRUE, the instance of
 *      H5P_mt_prop_t is logically deleted, and not if the flag is FALSE.
 *
 * dummy_bool_1:
 * dummy_bool_2:
 * dummy_bool_3:
 *      The dummy_bool fields exist to pad H5P_mt_prop_aptr_t out to 128 bits, and allow
 *      preventino of insertion of garbage into an atomic instance of H5P_mt_prop_aptr_t,
 *      thus avoiding spurious failures of atomic_compare_exchange_strong(). They should
 *      always be set to FALSE.
 *
 ****************************************************************************************
 */
typedef struct H5P_mt_prop_aptr_t {
    H5P_mt_prop_t *ptr;

    bool deleted;

    bool dummy_bool_1;
    bool dummy_bool_2;
    bool dummy_bool_3;

} H5P_mt_prop_aptr_t;

/****************************************************************************************
 *
 * Structure: H5P_mt_value_t
 *
 * Description:
 *
 * Properties in a property list consist of a void pointer and a size.  To avoid race
 * conditions, the size and poitner must be set atomically.  This structure exists to
 * facilitate this.
 *
 * Fields:
 *
 * ptr (void *):
 *      Void pointer to the value, or NULL if the value is undefined.
 *
 * size (size_t):
 *      size_t containing the size of the buffer pointed to by the ptr field, or zero
 *      if ptr is NULL.
 *
 * Note: The above fields will usually have a total size of 128 bits. However, since
 * the size of size_t is not fixed across all 64 bit compilers, there is the potential
 * for occult failures in atomic_compare_exchange_strong() when garbage gets into
 * the un-used space in the structure. (recall that the sum of the sizes of the fields
 * of a structure need not equal the allocation size of the structure.)
 *
 * For now, it should be sufficient to assert that sizeof(size_t) = 8.
 * However, we will have to deal with the issue eventually. For example, I have read
 * that size_t is a 32 bit value on at least some compilers targeting Windows.
 *
 ****************************************************************************************
 */
typedef struct H5P_mt_prop_value_t {
    void  *ptr;
    size_t size;

} H5P_mt_prop_value_t;

/****************************************************************************************
 *
 * Structure: H5P_mt_prop_t
 *
 * Description:
 *
 * Struct H5P_mt_prop_t is a revised version of H5P_genprop_t designed for use in a
 * multi-thread safe version of H5P. The data structures supporting property lists
 * are lock free to the extent practical, and thus instances of H5P_mt_prop_t will
 * typically appear in lock free singly linked list.
 *
 * Further, to support versioning in property list classes,instances of H5P_mt_prop_t
 * in property list classes maintain reference counts of the number of property lists
 * that refer to them for default values, the create_version at which they were
 * inserted into the containing property list class or property list, and (if deleted)
 * the delete_version at which which the deletion took place.
 *
 * Fields:
 *
 * tag (uint32_t):
 *      Integer value set to H5P_MT_PROP_TAG when an instance of H5P_mt_prop_t is
 *      allocated from the heap, and to H5P_MT_PROP_INVALID_TAG just before it is
 *      released back to the heap. The field is used to validate pointers to instances of
 *      H5P_mt_prop_t. Two additional values are used for validation when a property is
 *      on the property free list. H5P_MT_PROP_VALID_ONFL_TAG when a property is on the
 *      property free list, but may still be valid for threads to access, and
 *      H5P_MT_PROP_FL_REALLOC_TAG when a property on the free list is no longer valid
 *      for accessing, but is a valid instance of a property structure to be reallocated
 *      for a new property.
 *
 *      NOTE: The current implementation of the multithread H5P package only uses the
 *      property free list to store property structures that have been deleted from a
 *      property list or property list class. All new properties are allocated from
 *      memory, until further testing is done.
 *
 *
 * next (_Atomic H5P_mt_prop_aptr_t):
 *      Atomic instance of H5P_mt_prop_aptr_t, which combines a pointer to the
 *      next element of the lock free singly linked list with a deleted flag.
 *      If there is no next element, or if the instance of H5P_mt_prop_t is
 *      not in a LFSLL, this field should be set to {NULL, FALSE}.
 *
 * sentinel (bool):
 *      Boolean flag. When set, this instance of H5P_mt_prop_t is a sentinel
 *      node in the lock free singly linked list -- and therefore does not
 *      represent a property.
 *
 * in_prop_class (bool):
 *      Boolean flag that is set to TRUE if this instance of H5P_mt_prop_t
 *      resides in a property list class, and FALSE otherwise. Note that
 *      the ref_count field is un-used if this field is FALSE.
 *
 * ref_count (_Atomic uint64_t):
 *      Atomic integer used to track the number of property list properties
 *      that point to this instance of H5P_mt_prop_t. This field must be
 *      zero if in_prop_class is FALSE.
 *
 *      Note that this ref_count is only increased when a new property list
 *      is created, and is decremented when the property list is discarded.
 *
 *      Thus this instance of H5P_mt_prop_t can be safely deleted if:
 *
 *      1) the ref count drops to zero, and
 *
 *      2) this property has been either deleted or superseded
 *      in the property list class.
 *
 *      NOTE: The current implementation only decrements a property's ref_count
 *      when a property list is closed. Further multithread testing is needed to
 *      ensure a property won't be deleted out from an entry in a list's lkup_tbl
 *      that points to that property.
 *
 * in_lkup_tbl (bool):
 *      Boolean flag that is set to TRUE if this instance of H5P_mt_prop_t
 *      is for a property that was inherited from the parent class and has a version
 *      in the list's lkup_tbl.
 *      This field aids when copying a list from another list, or when encoding a list,
 *      preventing any need from iterating the lkup_tbl multiple times.
 *
 *      NOTE: that if there are multiple versions of the property, the
 *      older versions will still have in_lkup_tbl set to TRUE, even though they don't
 *      have a pointer to them directly from the lkup_tbl.
 *
 *
 * Property Chksum, Name & Value:
 *
 * The lock free singly linked list used to store most instances of H5P_mt_prop_t
 * requires sentinels at the beginning and end of the list with values (conceptually)
 * of negative and positive infinity respectively. This is a bit awkward with strings,
 * so for this reason, the (name, creation_version) key is augmented with a 32 bit
 * checksum on the name, converting the key to a (chksum, name, create_version)
 * triplet.
 *
 * NOTE: that the check sum is a 32 bit unsigned value, which is stored in an int64_t.
 * Thus we can use LLONG_MIN and LLONG_MAX as our negative and positive infinity
 * respectively.
 *
 * The addition of the chksum changes the sorting order to ascending checksum, name, and
 * then decreasing create_version. This ordering, along with the delete_version
 * field, allows us to operate on specific versions of a property list classes and
 * property lists -- thus allowing concurrent operations without introducing
 * corruption.
 *
 * Fields:
 *
 * chksum (int64_t):
 *      int64_t containing a 32 bit checksum computed on the name field
 *      below, or LLONG_MIN or LLONG_MAX if either the head or tail
 *      sentinel in the lock free SLL respectively.
 *
 *      Since this field is constant for the life of the instance of
 *      H5P_mt_prop_t, and is set before the instance is visible to more
 *      than one thread, it need not be atomic.
 *
 * name (char *):
 *      Pointer to a dynamically allocated string containing the name of the
 *      property. This field is not atomic, as the string should be allocated,
 *      and initialized, and the name field set before the instance of
 *      H5P_mt_prop_t is visible to more than one thread. Since the name
 *      is constant for the life of the instance of H5P_mt_prop_t, this should
 *      be sufficient for thread safety.
 *
 * value (_Atomic H5P_mt_prop_value_t):
 *      Atomic structure containing the pointer to the buffer containing the
 *      value of the property, and its size.
 *
 * create_version (_Atomic uint64_t):
 *      Atomic integer which is set to the version of the containing
 *      property list class or property list in which this property was
 *      inserted.
 *
 * delete_version (_Atomic uint64_t):
 *      Atomic integer which is set to the version of the containing
 *      property list class or property list in which the property was
 *      deleted. If the property has not been deleted, this field is zero.
 *
 *
 * Property Callback Functions:
 *
 * NOTE: not all properties will have every or any callback functions.
 *
 * create:  Function to call when a property is created.
 *
 *      Signature:
 *
 *          herr_t
 *          H5P_prp_create_func_t (const char *name, size_t size, void *value)
 *
 *		    This callback should set up the initial value of the property by modifying
 *		    the provided value buffer. This is necessary when the property is a complex
 *		    object that cannot be deep copied by a single memcpy(). size describes the
 *		    size of value, and name is the name of the property being created.
 *
 *          value is a shallow copy of the initial property value provided to
 *          H5P__register_real(). If this callback returns a negative value, then the
 *          potentially modified value is not copied into the property and the creation
 *          routine returns an error.
 *
 *          The initialization done by this callback may consist of simply deep copying
 *          the initial value. This deep copy may be implemented via reference counting
 *          (as seen in H5P__facc_file_driver_create() and H5P__facc_vol_create()), or
 *          as a ’real’ copy with new memory allocation for each dynamically allocated
 *          field of the property value. The memory management method this callback uses
 *          to enable copy-by-value semantics must be cleaned up during the delete and
 *          free callbacks assigned to the same property.
 *
 *          The original dynamically allocated fields under value, if any, should not be
 *          freed or modified, since these fields are still in use by either the property
 *          list class or the original property list. An exception to this is that if
 *          reference counting is used to implement copy-by-value, then the underlying
 *          fields must be modified to update their reference count.
 *
 *          This callback is invoked in two places by the library: During the creation
 *          of a new property list in H5P__create_prop(), and when copying a property from
 *          one plist to another plist that does not already contain it in
 *          H5P__copy_prop_plist(). (If the target plist for a copy operation does
 *          already contain the property, the copy callback is used instead.)
 *
 *
 * set:     Function to call when a property value is set.
 *
 *      Signature:
 *
 *          herr_t
 *          H5P_prp_set_func_t(hid_t prop_id, const char *name, size_t size, void *value)
 *
 *          This callback should modify value as necessary for the set operation to
 *          follow copy-by-value semantics for the property. This callback is necessary
 *          when the value is a complex object with its own internal dynamic memory
 *          allocation. This callback may also perform a transformation on the property
 *          value, if the internal representation differs from the representation visible
 *          to the user.
 *
 *          prop_id is the ID of the property list being modified. name is the name of
 *          the property being modified. value is a shallow copy of the provided value
 *          to write. size is the size of the buffer value. If this callback returns a
 *          negative value, the potentially modified value is not copied into the
 *          property and the set routine returns an error.
 *
 *          If performing a deep copy, the set callback should either allocate new memory
 *          for the dynamically allocated fields of the property value, or ’fake’ copy
 *          them using reference counting - see H5P__facc_file_driver_set() and
 *          H5P__facc_vol_set() as examples. The memory management method this callback
 *          uses to enable copy-by-value semantics must be cleaned up during the delete
 *          and free callbacks assigned to the same property.
 *
 *          If no error occurs, the modified value buffer is copied to the target property
 *          after this callback finishes.
 *
 *          The original dynamically allocated fields under value, if any, should not be
 *          freed or modified, since these fields are still in use by the application.
 *          An exception to this is that if reference counting is used to implement
 *          copy-by-value, then the underlying fields must be modified to update their
 *          reference count.
 *
 *          The set callback is used to set the value of a property in a list by
 *          H5P__set_plist_cb(), and to set the value of a property in a class by
 *          H5P__set_pclass_cb().
 *
 *          If the set callback is not defined, the property read operation defaults to
 *          a simple memcpy() from the application buffer to the property value buffer.
 *
 *          NOTE: Due to the versioning system the multithread safe structures utilize,
 *          when a property is having its value 'set' the process is a new property is
 *          created that is a copy of the property to 'set' the value of with its
 *          create_version set to the containing list's or class's next_version. After
 *          the new property is created, the set callback function is called on that new
 *          version of the property.
 *
 *
 * get:     Function to call when a property value is retrieved.
 *
 *      Signature:
 *
 *          herr_t
 *          H5P_prp_get_func_t(hid_t prop_id, const char *name, size_t size, void *value)
 *
 *          This callback should modify value as necessary for the get operation
 *          to follow copy-by-value semantics for the property. This is necessary
 *          when the property value is a complex object with its own internal
 *          dynamic memory allocation. The get callback may also perform a
 *          transformation on the property value before providing it to the user,
 *          if the representation visible to the user differs from how it is
 *          stored in the library.
 *
 *          prop_id is the ID of the property list being queried. name is the name
 *          of the property being queried. value is a shallow copy of the property
 *          value that will eventually be returned to the application. size is the
 *          size of the buffer value. If this returns a negative value, then the
 *          user’s buffer is not modified and the get routine returns an error.
 *
 *          If performing a deep copy, the get callback should either allocate new
 *          memory for the dynamically allocated fields of the property value, or
 *          ’fake’ copy them using reference counting - see H5P__facc_file_driver_get()
 *          and H5P__facc_vol_get(). The memory management method this callback uses to
 *          enable copy-by-value semantics must be cleaned up during the delete and free
 *          callbacks assigned to the same property.
 *
 *          The original dynamically allocated fields under value, if any, should not
 *          be freed or modified, since these fields are still in use by the property
 *          itself. An exception to this is that if reference counting is used to
 *          implement copy-by-value, then the underlying fields must be modified to
 *          update their reference count.
 *
 *          If no error occurs, the modified value buffer is copied to the application
 *          buffer by H5P__get_cb().
 *
 *          If this callback is not defined, the read operation defaults to a simple
 *          memcpy() from the property’s value to the application buffer.
 *
 *
 * encode:  Function to call when a property is encoded.
 *
 *      Signature:
 *
 *          herr_t
 *          H5P_prp_encode_func_t(const void *value, void **buf, size_t *size)
 *
 *          This callback is used to encode the property value *value into the
 *          application-allocated buffer *buf. size describes the size of the
 *          destination buffer *buf. If the provided buffer is NULL, or if the
 *          provided size is zero, then the encode callback should modify size
 *          to return the necessary buffer size for the encoded value.
 *
 *          Unlike decode, the encode callback should not increment the provided
 *          value pointer after encoding.
 *
 *          NOTE: in the multithread property structure, its value in the field
 *          H5P_mt_prop_value_t value.ptr.
 *
 *
 * decode:  Function to call when a property is decoded.
 *
 *      Signature:
 *
 *          herr_t
 *          H5P_prp_decode_func_t(const void **buf, void *value)
 *
 *          This callback is used to decode the encoded property value in *buf to
 *          the library-allocated buffer value.
 *
 *          The decode callback must increment the pointer *buf by the size of the
 *          encoded value. This is the reason buf is a is provided as a void**.
 *          This incrementing is necessary for H5P__decode() to iterate through
 *          all properties in an encoded property list.
 *
 *          NOTE: in the multithread property structure, its value in the field
 *          H5P_mt_prop_value_t value.ptr.
 *
 *
 * del:   Function to call when a property is deleted.
 *
 *      Signature:
 *
 *          herr_t
 *          H5P_prp_delete_func_t(hid_t prop_id, const char *name, size_t size,
 *                                void *value)
 *
 *          This callback should clean up any callback-controlled resources under
 *          value that were allocated during create, set, or copy. It is invoked
 *          when a property is deleted from a property list or class, or when the
 *          value of a property is replaced by a set operation. The top-level value
 *          buffer itself should not be freed, as the library frees that buffer
 *          during generic property free operations.
 *
 *          prop_id is the ID of the property list the property is being deleted
 *          from. name is the name of the property being deleted. value is the value
 *          of the property which is being deleted. size is the size of value.
 *
 *          If this callback returns a negative value, then an error is returned,
 *          but the target property is still deleted.
 *
 *
 * copy:    Function to call when a property is copied.
 *
 *      Signature:
 *
 *          herr_t
 *          H5P_prp_copy_func_t(const char *name, size_t size, void *value)
 *
 *          This callback should modify the value as necessary for copy-by-value semantics
 *          to be upheld when copying this property between property lists. This is
 *          necessary when the property value is a complex object that is not fully
 *          copied by a single memcpy() call.
 *
 *          name is the name of the property being copied. value is a shallow copy of
 *          the original property value. size is the size in bytes of value. If this
 *          callback succeeds, then value is copied to the new property in the
 *          destination property list.
 *
 *          If this callback returns a negative value, the potentially modified value
 *          is not copied into the destination plist and the copy routine returns an
 *          error.
 *
 *          This callback may implement a deep copy by copying any allocated fields
 *          stored under value, or ’fake’ such copying by using reference-counted
 *          fields. The memory management method this callback uses to enable
 *          copy-by-value semantics must be cleaned up during the delete and free
 *          callbacks assigned to the same property.
 *
 *          Note that this callback is used when copying an entire property list, and
 *          when copying a property to another list that already contains a property
 *          of the same name, but not when copying a property to another list that
 *          does not contain a property of the same name. In this last case, the create
 *          callback is used instead.
 *
 *          The original dynamically allocated fields under value, if any, should not
 *          be freed or modified, since these fields are still in use by the original
 *          property. The exception to this is that if reference counting is used to
 *          implement copy-by-value, then the underlying fields must be modified to
 *          update their reference count.
 *
 *
 * cmp:     Function to call when a property is compared.
 *
 *      Signature:
 *
 *          int
 *          H5P_prp_compare_func_t(const void *value1, const void *value2, size_t size)
 *
 *          This callback should return a positive value if value1 >value2, a
 *          negative value if value2 >value1, or zero if value1 = value2. Neither
 *          input value should be modified.
 *
 *          This callback is only the final step of the property comparison operation
 *          H5P__cmp_prop(). Before this callback is used, the property’s names, sizes,
 *          and callbacks are compared. If any of these fields are nonequal, the
 *          comparison returns early and this callback is not used. If two properties
 *          are nonequal due to one not defining a callback which the other property
 *          does define, the property which defines the callback is considered greater.
 *          If two properties provide different implementations of the same callback,
 *          then the first property is considered smaller.
 *
 *          NOTE: In the multithread version of H5P the function H5P__mt_prop_cmp() is
 *          used instead of H5P__cmp_prop().
 *
 *          NOTE: Also, in the multithread version H5P__mt_prop_cmp() returns zero if the
 *          properties are the same or one if they are different. So far in testing there
 *          has not been a use for returning that one property is greater than the other
 *          property being compared, so this simplifies it.
 *
 *
 * close:   Function to call when a property is closed.
 *
 *      Signature:
 *
 *          herr_t
 *          H5P_prp_close_func_t(const char *name, size_t size, void *value)
 *
 *          This callback should clean up any callback-controlled resources under
 *          value that were allocated during create, set, or copy. This callback
 *          is invoked when a property list containing this property is destroyed.
 *
 *          name is the name of the property being closed. value is a buffer
 *          containing the value of the property being closed. size is the size
 *          of the buffer value.
 *
 *          The top-level value buffer itself should not be freed, as the library
 *          frees that buffer during generic property free operations.
 *
 *          If this callback returns a negative value, the property list close
 *          operation returns an error, but the property list is still closed.
 *
 *
 ****************************************************************************************
 */

#define H5P_MT_PROP_TAG            0x1010 /* 4112 */
#define H5P_MT_PROP_VALID_ONFL_TAG 0X3030 /* 12336 */
#define H5P_MT_PROP_INVALID_TAG    0x2020 /* 8224 */
#define H5P_MT_PROP_FL_REALLOC_TAG 0x4040 /* 16448 */

typedef struct H5P_mt_prop_t {
    _Atomic uint32_t tag;

    _Atomic H5P_mt_prop_aptr_t next;

    bool sentinel;

    bool             in_prop_class;
    _Atomic uint64_t ref_count;
    bool             in_lkup_tbl;

    int64_t                     chksum;
    char                       *name;
    _Atomic H5P_mt_prop_value_t value;

    _Atomic uint64_t create_version;
    _Atomic uint64_t delete_version;

    /* Callback fields */
    bool                   callbacks_mt_safe;
    H5P_prp_create_func_t  create;
    H5P_prp_set_func_t     set;
    H5P_prp_get_func_t     get;
    H5P_prp_encode_func_t  encode;
    H5P_prp_decode_func_t  decode;
    H5P_prp_delete_func_t  del;
    H5P_prp_copy_func_t    copy;
    H5P_prp_compare_func_t cmp;
    H5P_prp_close_func_t   close;

} H5P_mt_prop_t;

/* Structures for Property List Classes  */

/****************************************************************************************
 *
 * Structure: H5P_mt_active_thread_count_t
 *
 * Description:
 *
 * Struct H5P_mt_active_thread_count_t is structure designed to contain a counter of the
 * number of threads currently active in the host structure and opening and closing flags
 * in a single atomic structure. The objectives are to prevent access to the containing
 * structure during setup, and to provide a mechanism for delaying the discard of the
 * containing structure until all threads currently active in the structure have exited.
 *
 * The possibility of access to a property list class or property list that is in the
 * process of being set stems from two points.
 *
 * First, some callbacks that must be called during setup require the ID of the host
 * property list. This requires that the property list be inserted into the index,
 * which in turn makes the incomplete property list accessible to other threads.
 *
 * Even in the absence of these callbacks, both property list classes and property lists
 * must be inserted into the index before they are completely set up. While it is
 * improbable, this makes them accessible to other threads via iterations on the host
 * indexes.
 *
 * To prevent this, the opening flag in the contained instance of
 * H5P_mt_active_thread_count_t is initialized to TRUE, and not set to FALSE until setup
 * completes. Any thread that wants to access the host property list much check this flag
 * on entry, and fail if it is TRUE.
 *
 * NOTE: When opening a list the call to H5I_register() is done in the function
 * H5P__mt_create_list(), thus the OPENING flag is set to FALSE at the end of that
 * function when the set up for the list has completed before returning.
 * However, for classes, the call to H5I_register() is not done in the function
 * H5P__mt_create_class() and must be called by the function that is calling
 * H5P__mt_create_class(). This is due to following the same procedure of the original
 * H5P package to make the multithread safe H5P functions easier to fit into the existing
 * code. This does mean that after calling H5P__mt_create_class(), a call to
 * H5I_register() must be done, and then the new class's id field must be set to the
 * returned ID from H5I_register(). Finally, the function that called
 * H5P__mt_create_class() must also atomically load the class's
 * H5P_mt_active_thread_count_t thrd structure field into a local copy and update the
 * opening flag to FALSE and then attempt to update the shared copy via a call to
 * atomic_compare_exchange_strong().
 *
 * In principle, it should be impossible for any thread to access a property list class
 * or property list that is in the process of being taken down. However, it seems prudent
 * to have a mechanism to detect the case where it does, and to manage it gracefully.
 * Note that in debug builds we should throw an assertion failure whenever a circumstance
 * that is forbidden occurs. One could argue that in production builds we should log the
 * issue and handle it gracefully – I am not sure I agree, but this is a discussion for
 * another time.
 *
 * In the typical case of a thread that reads or modifies the host data structure, it
 * must first do an atomic fetch on the associated instance of
 * H5P_mt_active_thread_count_t and fail if either the opening or closing flag is set. If
 * neither flag is set, it must increment the thread counter in the local copy, and
 * attempt to overwrite the shared copy with the local copy using a call to
 * atomic_compare_exchange_strong(). If this fails, it must repeat the procedure until
 * successful, or until the closing flag is set. When the thread is done with the host
 * data structure, it must again load the associated instance of
 * H5P_mt_active_thread_count_t, decrement the thread count in the local copy, and
 * attempt to overwrite the shared copy with the local copy with another call to
 * atomic_compare_exchange_strong() – repeating the procedure until successful. Note that
 * the flags are ignored in this case.
 *
 * Similarly, a thread that is about to discard the host structure must first do an
 * atomic fetch on the associated instance of H5P_mt_active_thread_count_t and fail if
 * either the opening or closing flag is set – preferably with an assertion failure. If
 * the closing flag is not set, it must set it in the local copy, and attempt to
 * overwrite the shared copy with the local copy using a call to
 * atomic_compare_exchange_strong(). If this fails, it must repeat the procedure until
 * successful. Once the closing flag is set, it must verify that no threads are active in
 * the host structure – either throwing an error or waiting until the thread count drops
 * to zero as appropriate.
 *
 * With padding, this structure is 128 bits, which allows true atomic operation on
 * many (most?) modern CPUs. However, it this becomes a problem, we can obtain the
 * same effect by stealing the low order bit of the pointer for a deleted bit -- which
 * works on all CPU / C compiler combinations I have tried.
 *
 * Fields:
 *
 * count (uint64_t):
 *      Number of threads currently active in the host structure.
 *
 * opening (bool):
 *      Boolean flag that is set to TRUE while the host property list class or property
 *      list is in the process of being setup. It must be set to FALSE once setup is
 *      complete.
 *
 * closing (bool):
 *      Boolean flag that is set to TRUE iff the host structure is about to be discarded.
 *
 ****************************************************************************************
 */
typedef struct H5P_mt_active_thread_count_t {
    uint64_t count;
    bool     opening;
    bool     closing;

} H5P_mt_active_thread_count_t;

/****************************************************************************************
 *
 * Structure: H5P_mt_class_ref_counts_t
 *
 * Description:
 *
 * Property list classes (instances on H5P_mt_class_t in the new implementation) need to
 * maintain reference counts on the number of derived property list classes, the number
 * of derived property lists, and whether the property list class still exists in the
 * index.
 *
 * One can argue that these three ref counts should be combined into a single reference
 * count. For now, at least, I am inclined to retain this design feature for the
 * following reasons:
 *
 * First, maintaining these reference counts separately seems likely to have some
 * debugging benefits, in that it provides more information about the current derivatives
 * of the property list class than a single reference count.
 *
 * Second, given that we must replicate the behavior of the current implementation quite
 * closely in the single thread case, it seems to me that gratuitous design changes
 * should be avoided.
 *
 * This, however raises the issue of how to keep the different reference counts
 * synchronized, and in particular, how to avoid the case in which the combined
 * reference counts drop to zero, discard is initialized, and another thread comes
 * in and tries to increment one of the reference counts.
 *
 * This is solved by combining the various reference counts into a single atomic
 * structure, and not allowing any reference count to be incremented once all the
 * reference counts have dropped to zero.
 *
 * This structure is intended to fulfill this role. The individual fields are discussed
 * below. Observe that the size of the structure is less that 128 bits, which should
 * allow true atomic operation on most modern machines.
 *
 * With padding, this structure is 128 bits, which allows true atomic operation on
 * many (most?) modern CPUs. However, it this becomes a problem, we can obtain the
 * same effect by stealing the low order bit of the pointer for a deleted bit -- which
 * works on all CPU / C compiler combinations I have tried.
 *
 * Fields:
 *
 * pl (uint64_t):
 *      Number of property lists immediately derived from this property list class, and
 *      still extant.
 *
 * plc (uint32_t):
 *      Number of property list classes immediately derived from this property list
 *      class, and still extant.
 *
 * deleted (bool):
 *      Boolean flag indicating whether this property list class has been deleted from
 *      the index. This field is set to FALSE on creation, and set to TRUE when the
 *      reference count on the property list class in the index drops to zero.
 *
 * dummy_bool_1:
 * dummy_bool_2:
 * dummy_bool_3: The dummy_bool fields exist to pad H5P_mt_prop_aptr_t out to 128 bits
 *
 ****************************************************************************************
 */
typedef struct H5P_mt_class_ref_counts_t {
    uint64_t pl;
    uint32_t plc;
    bool     deleted;

    bool dummy_bool_1;
    bool dummy_bool_2;
    bool dummy_bool_3;

} H5P_mt_class_ref_counts_t;

/****************************************************************************************
 *
 * Structure: H5P_mt_class_sptr_t
 *
 * Description:
 *
 * The H5P_mt_class_sptr_t combines a pointer to H5P_mt_class_t with a serial number
 * in a 128 bit package. It is intended to allow instances of H5P_mt_class_t to be
 * linked together in a singly linked list – specifically in a free list.
 *
 * This combination of a pointer and a serial number is needed to prevent ABA
 * bugs.
 *
 * Fields:
 *
 * ptr (H5P_mt_class_t *):
 *      Pointer to an instance of H5P_mt_class_t.
 *
 * sn (uint64_t):
 *      Serial number that should be incremented by 1 each time a new value is assigned
 *      to ptr.
 *
 ****************************************************************************************
 */
typedef struct H5P_mt_class_t H5P_mt_class_t; /* Forward declaration */

typedef struct H5P_mt_class_sptr_t {
    H5P_mt_class_t *ptr;
    uint64_t        sn;

} H5P_mt_class_sptr_t;

/****************************************************************************************
 *
 * Structure: H5P_mt_class_t
 *
 * Description:
 *
 * Revised version of H5P_genclass_t designed for use in a multi-thread safe version
 * of the HDF5 property list module (H5P).
 *
 * At the conceptual level, a property list class is simply a template for constructing
 * a default version of a member of a class of property lists, with the default
 * properties, each with that property's default value.
 *
 * This simple concept is complicated by the requirement that modifications to property
 * list classes can not effect preexisting derived property lists or property list
 * classes.
 *
 * The single thread version of H5P addressed this problem by duplicating property list
 * classes with derived property lists and/or property list classes whenever they are
 * modified. The modification is applied to the duplicate, and the duplicate replaces
 * the base version in the index. This approach has a number of problems, not the
 * least being that it makes it possible for multiple versions of the property list
 * class to exist in the index, and thus be visible to the user.
 *
 * Instead, the multi-thread version of property list classes maintains back versions
 * of all properties tagged with the property list class version in which they were
 * created (and possibly deleted). All properties are ref counted with the number of
 * properties in derived property lists that refer to them for default values. Since
 * the ref count on a version of a property can only be incremented when that property
 * version appears in the current version of the property list class, this means that
 * back versions of properties may be safely discarded once their ref counts drop to
 * zero.
 *
 * Note that this no longer need be the case if we allow back version of property list
 * classes to be visible outside of H5P. Note also that this is a semantic change in
 * the H5P API from the single thread version, albeit an obscure one, and to my thinking,
 * very much in the right direction.
 *
 * More importantly, if all operations on a property list address a specific version,
 * and all modifications are effectively atomic, concurrent operations can occur without
 * the potential for data corruption as long as all modifications trigger an increment
 * of the property list class version number.
 *
 * Making modifications to a property list class effectively atomic is slightly tricky,
 * as to give an obvious example, inserting a new property and incrementing the
 * version number can't be made atomic without heroic measures. However, by
 * targeting every operation at a specific version, we can make changes in progress
 * effectively invisible since new or modified properties are represented by new
 * instances of H5P_mt_prop_t with creation property list class versions higher than
 * the current version, and thus don't become visible until the property list class
 * version is incremented to the point that they become visible.
 *
 * However, if multiple modifications to the property list class are in progress
 * simultaneously, there is a race condition between the issue of a new version
 * number to be used to tag a modification, and the increment of the property list
 * class version number when the modification completes.
 *
 * Conceptually, this can be handled by waiting to increment the version number until
 * the current version number is one less than the issued version number. Use of a
 * condition variable is the obvious solution here -- but we will sleep and try again
 * until we settle on a threading package.
 *
 * There is also a potential race condition if a delete and either a modify or an
 * insert on a single property is in progress at the same time. In this case, the
 * operations must proceed in target version issue order.
 *
 * A second fundamental difference between the single thread and the multi-thread
 * implementations of the property list class, is that properties are stored on a
 * lock free singly linked list (LFSLL) instead of a skip list. This LFSLL list is
 * sorted first by a hash (called chksum) on the property name, second by property name
 * (to allow for hash collisions), and finally by creation version in decreasing order.
 *
 * Given that property lists are typically short (less that 25 properties), and that
 * the LFSLL will be searched only on property insert, delete, or modification, the
 * LFSLL should be near optimal for this application. However, if the number of
 * properties (or back versions of same) balloon and cause performance issues, it
 * will be easy enough to replace the LFSLL with a lock free hash table.
 *
 * Finally, for code simplicity, properties inherited from the parent property list
 * class are copied into the LFSLL of properties in the derived property list class.
 * There is nothing magic about this, and we can revert to the old system if there
 * is some reason to do so.
 *
 * Note, however, that it is still necessary for property list classes to maintain
 * pointers to their parent property list classes due to the requirement that
 * all close functions in ancestor property list classes be called on close.
 *
 * With this outline of H5P_mt_class_t in hand, we now address individual fields.
 *
 *                                                          JRM -- 5/22/24
 *
 * Fields:
 *
 * tag (uint32_t):
 *      Integer value set to H5P_MT_CLASS_TAG when an instance of H5P_mt_class_t is
 *      allocated from the heap, and to H5P_MT_CLASS_INVALID_TAG just before it is
 *      released back to the heap. The field is used to validate pointers to instances
 *      of H5P_mt_class_t. Additionally, there is a third value to be set to,
 *      H5P_MT_CLASS_FL_REALLOC_TAG, when a instance of H5P_mt_class_t on the free list
 *      is guaranteed to no longer be accessed and is available to be realloced for a new
 *      allocation of an instance of H5P_mt_class_t, instead of allocating from the heap.
 *
 *      NOTE: Current implementation doesn't allocate new classes from the class free
 *      list, and all are allocated from the heap. More testing needs to be done on the
 *      multithread free lists to ensure there isn't an attempt to access them.
 *
 * parent_id (hid_t):
 *      ID assigned to the immediate parent property list class in the index. As the
 *      parent cannot be deleted until its ref_counts drop to zero, it must exist at
 *      least as long as this property list class.
 *
 *      This field is not atomic, as it is set before this property list class is
 *      inserted in the index, thus before it's visible to other threads, and doesn't
 *      change for the life of this instance.
 *
 * parent_ptr (H5P_mt_class_t *):
 *      Pointer to the instance of H5P_mt_class_t that represents the immediate parent
 *      property list class in the index. As the parent cannot be deleted until its
 *      ref_counts drop to zero, it must exist and this pointer must be valid at least
 *      as long as this property list class exists.
 *
 *      This field is not atomic, as it is set before this property list class is
 *      inserted in the index, thus before it's visible to other threads, and doesn't
 *      change for the life of this instance.
 *
 * perent_version (uint64_t):
 *      Version of the parent property list class from which this property list class
 *      is derived.
 *
 *      This field is not atomic, as it is set before this property list class is
 *      inserted in the index, thus before it's visible to other threads, and doesn't
 *      change for the life of this instance.
 *
 * name (char *):
 *      Pointer to a string containing the name of this property list class.
 *
 *      This field is not atomic, as it is set before this property list class is
 *      inserted in the index, thus before it's visible to other threads, and doesn't
 *      change for the life of this instance.
 *
 * id (_Atomic hid_t):
 *      Atomic instance of hid_t used to store the id assigned to this property list
 *      class in the index. This field is atomic, as it can't be set until after the
 *      instance of H5P_mt_class_t is registered, and thus visible to other threads.
 *      That said, once set, this field will not change for the life of the property
 *      list class.
 *
 *      NOTE: The original version of H5P, when a property list class is retrieved
 *      a new ID is assigned to it and inserted into the index via a call to
 *      H5I_register(), regardless of whether or not it already has an existing ID in the
 *      index. The multithread version has changed this to instead increment the
 *      reference count on that class's ID in the index, and return the already existing
 *      ID. This method is to ensure each property list class only ever has one ID.
 *
 * type (H5P_plist_type_t):
 *      Type of the property list class.
 *
 *      This field is not atomic, as it is set before this property list class is
 *      inserted in the index, thus before it's visible to other threads, and doesn't
 *      change for the life of this instance.
 *
 * curr_version (_Atomic uint64_t):
 *      Atomic uint64_t containing the current version number of the property list class.
 *      This version number is incremented each time a modification to the property list
 *      class is completed.
 *
 *      A uint64_t is used, as at present there is no provision for a roll over. Given
 *      the relative infrequency of modifications to propety list classes, 64 bits is
 *      probably sufficient for all reasonable cases. However, a roll over must never
 *      occur, and an error should be flagged if it does.
 *
 *      To allow an undefined deletion version, the curr_version must be no less than 1.
 *
 * next_version (_Atomic uint64_t):
 *      Atomic uint64_t containing the version number to be assigned to the next
 *      modification of the property list class. When no modifications to the property
 *      list class are in propgress next_version should be one greater than curr_version.
 *
 *      When a modification to a property list class begins, it does a fetch and
 *      increment on next_version, preforms its changes and tags them with the returned
 *      version, and finally increments the curr_version.
 *
 *      NOTE: To avoid exposure of partial modifications, increments to curr_version must
 *      be executed in next_version issue order. Thus, a thread that modifies the propety
 *      list class, must not increment curr_version until its value is one less than the
 *      version number it obtained when it started.
 *
 *      Further, if a modify or insert and a delete on the same property are active at
 *      the same time, they must be executed in issue_order.
 *
 * pl_head (H5P_mt_prop_t *):
 *      Atomic pointer to the head of the LFSLL containing the list of properties (i.e.
 *      instances of H5P_mt_prop_t) associated with the property list class. Other than
 *      during setup, this field will always point to the first node in the list whose
 *      value will be negative infinity.
 *
 *      Entries in this list are sorted first by a hash on the property name, second by
 *      property name (to allow for hash collisions), and finally by creation version in
 *      decreasing order. Other than during setup, the first and last entries in the list
 *      will be sentry nodes with hash values (conceptually) of negative and positive
 *      infinity respectively.
 *
 * nprops_added (_Atomic size_t):
 *      The number of properties added to the property list class after its creation.
 *
 *      NOTE: This field was added to fall in line with the original functionality of
 *      H5Pget_nprops() in H5P.c and H5P_get_nprops_pclass() in H5Pint.c. In the original
 *      version of H5P, new classes do not inherit the parent's properties and instead
 *      will loop up the parent tree to search their properties if needed. Because of
 *      this the value returned from the functions to get the number of properties from
 *      classes only included the properted added to that specific property list class,
 *      or if a default property of the parent was modified for this class. In this
 *      multithread version of H5P all valid properties of the parent are added to the
 *      derived class, thus the resulting output is higher then what is currently
 *      expected. This field remedies that issue, by only counting the added and not
 *      inherited properties for the property list class.
 *
 *      NOTE: Two things make a property invalid:
 *                  1) any property that has a create_version greater than the parent
 *                     class's version from which the new class is derived.
 *                  2) any property that has a delete_version less than or equal to the
 *                     parent class's version from which the new class is derived.
 *
 * log_pl_len (_Atomic size_t):
 *      Number of valid properties defined in the property list class at the current
 *      version.
 *
 *      NOTE: This value will not be correct for all versions of the property list class,
 *      and will be briefly incorrect even for the current version during property
 *      insertions and deletions. Thus when an exact value is required, the property list
 *      class must be scanned for the correct value for the desired version.
 *
 * phys_pl_len (_Atomic size_t):
 *      Number of instances of H5P_mt_prop_t in the property list class. This number
 *      includes sentinel nodes, and both current and superseded instances of
 *      H5P_mt_prop_t.
 *
 *      NOTE: This value will be briefly incorrect during property insertions, deletions,
 *      and modifications. Modification of a property cause this value to change, and a
 *      new instance of H5P_mt_prop_t is inserted with the desired changes and a new
 *      creation version.
 *
 * ref_count (_Atomic H5P_mt_class_ref_counts_t):
 *      Atomic instance of H5P_mt_class_ref_counts_t which combines:
 *
 *          1) The number of property lists immediately derived from this property list
 *             class, and still extant (ref_count.pl),
 *          2) The number of property list classes immediately derived from this property
 *             list class, and still extant (ref_count.plc),
 *          3) A boolean flag indicating whether this property list class has been
 *             deleted from the index (ref_count.deleted).
 *
 *      into a single atomic structure - thus ensuring sychronization between these three
 *      different values.
 *
 *      Once ref_count.pl and ref_count.plc have dropped to zero, and deleted is set to
 *      TRUE, the property list class may be discarded. Further, neither ref_count.pl or
 *      ref_count.plc may be incremented once this condition obtains.
 *
 *      Further, observe that once this condition holds, the reference counts on all
 *      versions of all properties in the property list class must be zero.
 *
 *
 *
 * The following fields are pointers to the callback functions associated with the
 * property along with pointers to data to be passed to these functions when called.
 * These are combined with a Boolean indicating whether all callbacks are thread safe.
 * If this flag is not set, all callbacks must be protected by the global mutex.
 *
 * The descriptions of the callbacks are all taken from Matt Larson’s “Census of H5P
 * Callbacks”, and are a major improvement on the existing documentation.  These
 * descriptions may have to be modified to reflect the re-implementation of H5P.
 *
 * Quoting from Matt’s document:
 *
 *     At the time of this document’s creation (HDF5 1.14.4.3), the library does
 *     not define any of these callbacks on any of its predefined property list
 *     classes.
 *
 *     If the test code for the property list class callbacks
 *     (test_genprop_class_callback in tgenprop.c) is indicative of the design
 *     intent, then these callbacks may be intended to let users associate
 *     reference-counted data with property list classes. Property list create,
 *     copy, and close operations would then reference shared data on the class
 *     object, and would not be threadsafe if the operations potentially modify
 *     that data.
 *
 * We need to determine if there are any other uses for these callbacks.
 *
 * callbacks_mt_safe:
 *      Boolean flag used to indicate whether all callbacks are
 *      multi-thread safe.  If this field is not set, the global mutex must
 *      be held when the callbacks are called.
 *
 *      NOTE: the boolean field callbacks_mt_safe does not currently exist in the
 *      H5P_mt_class_t structs. When multithread testing begins on the callbacks
 *      it will be added to track, but for now they are always considered not
 *      multithread safe.
 *
 * create_func: Function to call when a property list is created.
 *
 *              Signature:
 *
 *                  herr_t H5P_cls_create_func_t(hid_t prop_id, void *create_data)
 *
 *              This callback is invoked when a property list of the given class
 *              is created. prop_id is the identifier of the property list being
 *              created. create_data is a pointer to a buffer of application-defined
 *              data stored on the parent class of prop_id.
 *
 *              This callback may modify create_data, or perform application-defined
 *              initialization work on the list prop_id. If this callback allocates
 *              any resources under create_data, then those resources should be
 *              released by the corresponding property class close callback.
 *
 *              If this callback returns a negative value, then the new list is not
 *              returned to the user and the property list creation routine returns
 *              an error.
 *
 *              When this callback is invoked, it is invoked for every property list
 *              class in the class hierarchy of the list parent class, starting from
 *              the immediate parent class and proceeding until the root class.
 *
 *              If this callback modifies create_data, then it is not threadsafe due
 *              to modifying a resource which may be accessed by other threads
 *              performing plist operations concurrently.
 *
 *              create_data is not copied by the library; the buffer passed in by
 *              the application is used directly. If this buffer is dynamically
 *              allocated, releasing it is the responsibility of the application.
 *
 * create_data: Pointer to user data to pass along to create callback.
 *
 * copy_func:   Function to call when a property list is copied.
 *
 *              Signature:
 *
 *                  herr_t
 *                  H5P_cls_copy_func_t(hid_t new_prop_id, hid_t old_prop_id,
 *                                      void *copy_data)
 *
 *              This callback is invoked when copying a property list of the given
 *              class. new_prop_id is the identifier of the newly created property
 *              list copy. old_prop_id is the id of the list being copied. copy_data
 *              is a pointer to application-defined data on the class.
 *
 *              This callback may modify copy_data, or it may perform work on the new
 *              list or original list. If this callback allocates resources under
 *              copy_data, then those resources must be released by the corresponding
 *              property class close callback.
 *
 *              If this callback returns a negative value, the new list is not returned
 *              to the user, and the property list copy function returns an error value.
 *
 *              When this callback is invoked, it is invoked for every property list
 *              class in the class hierarchy of the list parent class, starting from
 *              the immediate parent class and proceeding until the root class.
 *
 *              If this callback modifies copy_data or old_prop_id, then it is not
 *              threadsafe due to modifying a resource which may be accessed by other
 *              threads performing plist operations concurrently.
 *
 *              copy_data is not copied by the library; the buffer passed in by the
 *              application is used directly. If this buffer is dynamically allocated,
 *              releasing it is the responsibility of the application.
 *
 * copy_data:   Pointer to user data to pass along to copy callback.
 *
 * close_func:  Function to call when a property list is closed.
 *
 *              Signature:
 *
 *                  herr_t H5P_cls_close_func_t(hid_t prop_id, void *close_data)
 *
 *              This callback is invoked when a property list of the given class
 *              is closed. prop_id is the ID of the property list being closed.
 *              close_data is a pointer to application-defined data on the property
 *              list class.
 *
 *              This callback should release any resources that were allocated
 *              under the class’s create or copy callbacks.
 *
 *              If this callback modifies close_data, then it is not threadsafe
 *              due to modifying a resource which may be accessed by other threads
 *              concurrently.
 *
 *              When this callback is invoked, it is invoked for every property
 *              list class in the class hierarchy of the list parent class,
 *              starting from the immediate parent class and proceeding until
 *              the root class.
 *
 *              close_data is not copied by the library; the buffer passed in by
 *              the application is used directly. If this buffer is dynamically
 *              allocated, releasing it is the responsibility of the application.
 *
 * close_data:  Pointer to user data to pass along to close callback.
 *
 *
 *
 * Free list and shutdown management fields:
 *
 * thrd (_Atomic H5P_mt_active_thread_count_t):
 *      This field is a structure used to verify that no threads are active in an
 *      instance of H5P_mt_class_t during setup or takedown of the structure prior to
 *      discard. See the header comment on H5P_mt_active_thread_count_t for a detailed
 *      discription of how its fields must be maintained when a thread wants to access
 *      the host instance of H5P_mt_class_t.
 *
 * fl_next (_Atomic_H5P_mt_class_sptr_t):
 *      This field is a structure used to contain a pointer to the next instance of
 *      H5P_mt_class_t in the free list. It is augmented with a serial number to avoid
 *      ABA bugs. This field is included to support a free list of instances of
 *      H5P_mt_class_t.
 *
 *
 *
 * Statistics Fields:
 *
 *
 * Insert statistics:
 *
 * H5P__insert_prop_setup__num_calls (_Atomic uint64_t):
 *      Tracks the number of H5P__insert_prop_setup() function calls. Can also be used in
 *      compination with H5P__mt_create_class() to track the number of times
 *      H5P__mt_insert_prop() and H5P__mt_create_prop were called.
 *
 * insert_max_nodes_visited (_Atomic uint64_t):
 *      Keeps track of the largest number of nodes visited in the LFSLL of the class
 *      during an insert.
 *
 * insert_avg_nodes_visited (_Atomic uint64_t):
 *      This field tracks the the average number of nodes visited in the LFSLL of the
 *      class across all inserts. Fields H5P__insert_prop_setup__num_calls and
 *      num_insert_node_visited are needed to calculate the insert_avg_nodes_visited.
 *
 * num_insert_nodes_visited (_Atomic uint64_t):
 *      The number of nodes in the LFSLL visited during the most recent insert call.
 *
 * num_insert_prop_cols (_Atomic uint64_t):
 *      The number of collisions with other threads that occur when attempting to
 *      atomically insert a property.
 *
 * num_insert_prop_success (_Atomic uint64_t):
 *      The number of successful property insertions. Will be equal to
 *      H5P__insert_prop_setup__num_calls unless an error occurs during the insert
 *      process
 *
 * num_insert_chksum_cols (_Atomic uint64_t):
 *      The number of inserts where the property being inserted is a different version
 *      of a property that already exists in the LFSLL of the class.
 *
 *
 * Delete Version statistics:
 *
 * H5P__set_delete_version__num_calls (_Atomic uint64_t):
 *      Tracks the number of H5P__set_delete_version() function calls.
 *
 * set_delete_max_nodes_visited (_Atomic uint64_t):
 *      Keeps track of the largest number of nodes visited in the LFSLL of the class
 *      during a set delete version call.
 *
 * set_delete_avg_nodes_visited (_Atomic uint64_t):
 *      This field tracks the the average number of nodes visited in the LFSLL of the
 *      class across all H5P__ set_delete_version() calls. Fields
 *      H5P__set_delete_version__num_calls and num_set_delete_nodes_visited are needed to
 *      calculate the set_delete_avg_nodes_visited.
 *
 * num_set_delete_nodes_visited (_Atomic uint64_t):
 *      The number of nodes visited during the most recent set_delete_version call.
 *
 * num_set_delete_prop_cols (_Atomic uint64_t):
 *      The number of collisions with other threads that occur when attempting to
 *      atomically set the delete_version of a property.
 *
 * num_set_delete_prop_success (_Atomic uint64_t):
 *      The number of properties with their delete_version set successfully. Will be
 *      equal to H5P__set_delete_version__num_calls unless an error occurs during
 *      H5P__set_delete_version().
 *
 * num_set_delete_chksum_cols (_Atomic uint64_t):
 *      The number of properties that have their delete_version set that have another
 *      version of the property in the LFSLL of the class.
 *
 *
 * Search statistics:
 *
 * H5P__search_prop__num_calls (_Atomic uint64_t):
 *      Tracks the number of H5P__mt_search_prop() function calls.
 *
 * search_max_nodes_visited (_Atomic uint64_t):
 *      Keeps track of the largest number of nodes visited in the LFSLL of the class
 *      during a search function call
 *
 * search_avg_nodes_visited (_Atomic uint64_t):
 *      This field tracks the the average number of nodes visited in the LFSLL of the
 *      class across all search function calls. Fields H5P__search_prop__num_calls and
 *      num_search_nodes_visited are needed to calculate the search_avg_nodes_visited.
 *
 * num_search_nodes_visited (_Atomic uint64_t):
 *      The number of nodes visited during the most recent search call.
 *
 * num_search_success (_Atomic uint64_t):
 *      The number of successful searches in the LFSLL for a property. Should be the same
 *      as H5P__search_prop__num_calls, unless an error occurs during a search.
 *
 * num_search_chksum_cols (_Atomic uint64_t):
 *      The number of searches where the property being searched for has another version
 *      of the property in the LFSLL of the class.
 *
 *
 * Version check statistics:
 *
 * num_wait_for_curr_version_to_inc (_Atomic uint64_t):
 *      The number of times a thread is trying to modify this class's LFSLL in some
 *      manner and has had to wait for the curr_version of the class to increment to be
 *      one less than the next_version that was recieved via an atomic_fetch_add() call.
 *
 *
 * Thread Count statistics:
 *
 * num_thrd_count_update_cols (_Atomic uint64_t):
 *      The number of times this class goes to update its thrd->count field and a
 *      collision with another thread occurs.
 *
 * num_thrd_count_update (_Atomic uint64_t):
 *      The number of times this class has its thrd->count field updated.
 *
 * num_thrd_closing_flag_set (_Atomic uint64_t):
 *      The number of times a thread tried to access this class when its thrd->closing
 *      is set to TRUE.
 *
 * num_thrd_opening_flag_set (_Atomic uint64_t):
 *      The number of times a thread tried to access this class when its thrd->opening
 *      is set to TRUE.
 *
 *
 * Reference Count statistics:
 *
 * num_ref_count_cols (_Atomic uint64_t):
 *      The number of times a thread tries to update the reference count (either pl or
 *      plc) for a class and a collision with another thread occurs.
 *
 * num_ref_count_update (_Atomic uint64_t):
 *      The number of times a thread updates the reference count (either pl or plc).
 *
 ****************************************************************************************
 */
#define H5P_MT_CLASS_TAG            0x1011 /* 4113 */
#define H5P_MT_CLASS_INVALID_TAG    0x2021 /* 8225 */
#define H5P_MT_CLASS_FL_REALLOC_TAG 0x3031 /* 12337 */

typedef struct H5P_mt_class_t {
    _Atomic uint32_t tag;

    /* fields related to the parent class */
    hid_t           parent_id;
    H5P_mt_class_t *parent_ptr;
    uint64_t        parent_version;

    /* Fields related to this class */
    char            *name;
    _Atomic hid_t    id;
    H5P_plist_type_t type;
    _Atomic uint64_t curr_version;
    _Atomic uint64_t next_version;

    /* List of properties, and related fields */
    H5P_mt_prop_t *pl_head;
    _Atomic size_t nprops_added;
    _Atomic size_t log_pl_len;
    _Atomic size_t phys_pl_len;

    /* reference counts */
    _Atomic H5P_mt_class_ref_counts_t ref_count;

    /* Callback function pointers and info */
    H5P_cls_create_func_t create_func;
    void                 *create_data;
    H5P_cls_copy_func_t   copy_func;
    void                 *copy_data;
    H5P_cls_close_func_t  close_func;
    void                 *close_data;

    /* Shutdown and free list management fields */
    _Atomic H5P_mt_active_thread_count_t thrd;
    _Atomic H5P_mt_class_sptr_t          fl_next;

    /* Stats */

    /* H5P_mt_class_t insert stats */
    _Atomic uint64_t H5P__insert_prop_class__num_calls;
    _Atomic uint64_t insert_max_nodes_visited;
    _Atomic uint64_t insert_avg_nodes_visited;
    _Atomic uint64_t num_insert_nodes_visited;
    _Atomic uint64_t num_insert_prop__cols;
    _Atomic uint64_t num_insert_prop__success;
    _Atomic uint64_t num_insert_prop__chksum_cols;

    /* H5P_mt_class_t set delete version stats */
    _Atomic uint64_t H5P__delete_prop__class__num_calls;
    _Atomic uint64_t set_delete__max_nodes_visited;
    _Atomic uint64_t set_delete__avg_nodes_visited;
    _Atomic uint64_t num_set_delete__nodes_visited;
    _Atomic uint64_t num_set_delete__cols;
    _Atomic uint64_t num_set_delete__success;
    _Atomic uint64_t num_set_delete_chksum_cols;

    /* H5P_mt_class_t search stats */
    _Atomic uint64_t H5P__search_prop__class__num_calls;
    _Atomic uint64_t search_class__max_nodes_visited;
    _Atomic uint64_t search_class__avg_nodes_visited;
    _Atomic uint64_t num_search_class__nodes_visited;
    _Atomic uint64_t num_search_class__success;
    _Atomic uint64_t num_search_chksum_cols;

    /* Version check stats */
    _Atomic uint64_t num_wait_for_curr_version_to_inc;

    /* H5P_mt_active_thread_count_t stats */
    _Atomic uint64_t num_thrd_update_cols;
    _Atomic uint64_t num_thrd_count_update;
    _Atomic uint64_t num_thrd_closing_flag_set;
    _Atomic uint64_t num_thrd_opening_flag_set;

    /* H5P_mt_class_ref_counts_t stats */
    _Atomic uint64_t num_ref_count_cols;
    _Atomic uint64_t num_ref_count_update;
    _Atomic uint64_t num_ref_count_inc_while_deleted;
    _Atomic uint64_t num_ref_count_marked_deleted;
    _Atomic uint64_t num_ref_count_unmarked_deleted;

    /* Property ref_count stats */
    _Atomic uint64_t num_prop_ref_count_update;

} H5P_mt_class_t;

/* Structures for Property Lists */

/****************************************************************************************
 *
 * Structure: H5P_mt_list_sptr_t
 *
 * Description:
 *
 * The H5P_mt_list_sptr_t combines a pointer to H5P_mt_list_t with a serial number
 * in a 128 bit package. It is intended to allow instances of H5P_mt_list_t to be
 * linked together in a singly linked list – specifically in a free list.
 *
 * This combination of a pointer and a serial number is needed to prevent ABA
 * bugs.
 *
 * Fields:
 *
 * ptr (H5P_mt_list_t *):
 *      Pointer to an instance of H5P_mt_list_t.
 *
 * sn (uint64_t):
 *      Serial number that should be incremented by 1 each time a new value is assigned
 *      to ptr.
 *
 ****************************************************************************************
 */
typedef struct H5P_mt_list_t H5P_mt_list_t; /* Forward declaration */

typedef struct H5P_mt_list_sptr_t {
    H5P_mt_list_t *ptr;
    uint64_t       sn;

} H5P_mt_list_sptr_t;

/****************************************************************************************
 *
 * Structure: H5P_mt_list_prop_ref_t
 *
 * Description:
 *
 * H5P_mt_list_prop_ref_t is a structure designed to contain a pointer to an instance of
 * H5P_mt_prop_t and a version number in a single atomic structure. This is necessary, as
 * when an entry in the H5P_mt_list_table_entry_t is updated, we need to update both the
 * pointer to the instance of H5P_mt_prop_t and the version number in a single atomic
 * operation.
 *
 * This structure is 128 bits, which allows true atomic operation on many (most?) modern
 * CPUs.
 *
 * The structure is used in two contexts:
 *
 * First to point to the instance of H5P_mt_prop_t in the property list class from
 * which the host property list was derived. In this case, version number should be
 * the initial version of the host property list class.
 *
 * Second, if the default value of the property has been overwritten, to point to an
 * instance of H5P_mt_prop_t in the host property list LFSLL of modified or added
 * properties. In this case, the ver field must match the create_version field of the
 * instance of H5P_mt_prop_t pointed to by ptr.
 *
 * Fields:
 *
 * ptr (H5P_mt_prop_t *):
 *      Pointer to an instance of H5P_mt_prop_t.
 *
 * ver (uint64_t):
 *      Version number of the host property list class at which this pointer was set.
 *
 ****************************************************************************************
 */
typedef struct H5P_mt_list_prop_ref_t {
    H5P_mt_prop_t *ptr;
    uint64_t       ver;

} H5P_mt_list_prop_ref_t;

/****************************************************************************************
 *
 * Structure: H5P_mt_list_table_entry_t
 *
 * Description:
 *
 * An array of instances of H5P_mt_list_table_entry_t is used to create a look up table
 * for properties inherited from the parent property list class.
 *
 * Fields:
 *
 * chksum (int64_t):
 *      int64_t containing a 32-bit checksum computed on the name field below.
 *
 * name (char *):
 *      Pointer to a dynamically allocated string containing the name of the property.
 *
 *      This field is not atomic, as it is set before this instance of H5P_mt_prop_t is
 *      visible to other threads, and doesn't change for the life of this instance.
 *
 * base (_Atomic H5P_mt_list_prop_ref_t):
 *      Atomic structure of H5P_mt_list_prop_ref_t with it's ptr field pointing to the
 *      instance of H5P_mt_prop_t in the parent property list class, and the ver field
 *      should contain the initial version number of the property list.
 *
 *      NOTE: If the instance of H5P_mt_prop_t in the parent property list class has a
 *      create callback, we must create a copy the property into the new property list,
 *      and not use the property in the parent property list class as the initial value
 *      of the property. In this case, base.ptr is set to NULL, and base.ver set to 0,
 *      and the copy is inserted into the lock free slignly linked list, and curr.ptr is
 *      set to point to the copy until such time as the value of the property is
 *      modified. In this case, curr.ver is initialized to the initial version of the
 *      property list.
 *
 * base_delete_version (_Atomic uint64_t):
 *      Property lists derived from a property list class must not modify properties in
 *      the parent property list class. Thus they must maintain their own create and
 *      delete version. The create version is simply the initial version of the property
 *      list, and is stored in the base.ver field. However, if the property is deleted
 *      from the property list, we must have a delete version to indicate the property
 *      list version at which this took place.
 *
 *      The atomic uint64_t base_delete_version exists to serve this purpose. If the base
 *      version of the property has not been deleted, this field will be 0. Once set to
 *      a non-zero value, it will never change for the life of the property list.
 *
 * curr (_Atomic H5P_mt_list_prop_ref_t):
 *      Atomic instance of H5P_mt_list_prop_ref_t whose ptr and ver fields must be
 *      initialized to NULL and 0 respectively.
 *
 *      If the value of the inherited property is modified, a new instance of
 *      H5P_mt_prop_t is allocated, copying the tag, sentinel, chksum, name, and
 *      callback fields from the most recent version of the property pointed to by
 *      either base.ptr or curr.ptr.
 *
 *      The in_prop_class, and ref_count fields are set to zero, and not used in
 *      property lists. The create_version is set to the version of the property list in
 *      which the modified version is set, and the delete_version is set to 0. The value
 *      field is set to point to the new value of the property, and the new instance of
 *      H5P_mt_prop_t is inserted into the LFSLL of new / modified properties associated
 *      with the host property list.
 *
 *      Next, curr.ptr is set to point to the new instance, and curr.ver is set to its
 *      create_version. Recall that both of these fields are set in a single atomic
 *      operation.
 *
 *      Finally, the version of the host property list is incremented to make these
 *      chagnes visible.
 *
 * first_ver_of_curr (_Atomic uint64_t first_ver_of_curr):
 *      Atomic uint64_t to store the version of the list a default property was modified
 *      at. Due to curr being updated to point to and store the version number of the
 *      most current version of the property the entry in the lkup_tbl is associated
 *      with, this field is used to prevent the LFSLL of a list to be iterated for no
 *      reason. If the version number of the list we are searching for a property in is
 *      less than the field first_ver_of_curr, we know that at this version the base is
 *      valid version of the property and thus no need to iterated the LFSLL.
 *
 ****************************************************************************************
 */
typedef struct H5P_mt_list_table_entry_t {
    int64_t                        chksum;
    char                          *name;
    _Atomic H5P_mt_list_prop_ref_t base;
    _Atomic uint64_t               base_delete_version;
    _Atomic H5P_mt_list_prop_ref_t curr;
    _Atomic uint64_t               first_ver_of_curr;

} H5P_mt_list_table_entry_t;

/****************************************************************************************
 *
 * Structure: H5P_mt_list_t
 *
 * Description:
 *
 * Revised version of H5P_genlist_t designed for use in a multi-thread safe version of
 * the HDF5 property list module (H5P).
 *
 * At the conceptual level, a property list class is simply a list of properties -- i.e.
 * name value pairs.
 *
 * When a property list is created, it incorporates a list of properties with default
 * values from its parent property list class at the version at which the creation of the
 * property list was started. At this time, the number of properties in the property list
 * class is determined and stored in the nprops_inherited field.
 *
 * This done, an array of H5P_mt_list_table_entry_t of length nprops_inherited is
 * allocated, with base.ptr field pointing to the associated instance of H5P_mt_prop_t
 * in the parent property list class's lock free singly linked list (LFSLL) of
 * properties, and the base.ver field set to the initial version of the property list.
 *
 * NOTE: This lkup_tbl does not need to be sorted, as its entries are set in the order
 * of the parent's LFSLL, thus already sorted by chksum and then name.
 *
 * NOTE: During the case of creating a copy of an existing property list, the current
 * implementation has the size of the lkup_tbl for the copy being allocated to be the
 * same size of the original list. This is to simplify the code and perform fewer
 * iterations, but may lead to some of the entries in the copy to basically be blank, if
 * they were deleted at the version of the original list, this instance is copied from.
 *
 * If the value of any inherited property is modified, a new instance of H5P_mt_prop_t
 * is created with the modified value and new creation version, and is inserted into the
 * property list's LFSLL of properties. The curr.ptr field of the appropriate entry
 * in the lookup table is set to point to it, and the curr.ver field is set to the new
 * version of the property. Finally, the property list's curr_version field is
 * incremented to make this modification visible.
 *
 * If a new property is added to the property list, it is simply inserted into the
 * property list's LFSLL of instances of H5P_mt_prop_t, and the nprops_added field is
 * incremented. On searches, the lookup table is searched first, with the LFSLL being
 * searched only if this first search fails, and nprops_added is positive.
 *
 * As with the multi-thread version of the property list classes, property lists
 * maintain back versions of all properties tagged with the property list version
 * in which they were created (and possibly deleted). Unlike property list classes,
 * the properties are not reference counted.
 *
 * All operations on a property list must address a specific version, which must be no
 * greater than the current version at the start of the operation. As shall be seen,
 * this allows us to make all modifications effectively atomic, which in turn allows
 * concurrent operations to occur without the potential for data corruption.
 *
 * As with property list classes, making modifications to a property list effectively
 * atomic is slightly tricky, but can be handled in much the same way. Since every
 * operation on a property list is targeted at a specific version, we can make changes in
 * progress effectively invisible since new or modified properties are represented by
 * new instances of H5P_mt_prop_t with creation property list versions higher
 * than the current version, and thus don't become visible until the property list
 * version is incremented to the point that they become visible..
 *
 * However, as with property list classes, if multiple modifications to the property list
 * are in progress simultaneously, there is a race condition between the issue of a new
 * version number to be used to tag a modification, and the increment of the property list
 * version number when the modification completes.
 *
 * As with property list classes, this can be usually be handled by waiting to increment
 * the version number until the current version number is one less than the issued
 * version number. However, if a modify or insert and a delete on the same property are
 * active at the same time, they must be executed in issue order.
 *
 * Also, as per property list classes, modified / new properties are stored on a LFSLL
 * instead of a skip list. This LFSLL list is sorted first by the chksum (hash on the
 * property name), second by property name (to allow for hash collisions), and finally by
 * creation version in decreasing order.
 *
 * Since only new / modified properties are stored on this list, it should be shorter
 * than the similar list in property list classes. More importantly, the latest
 * version of each inherited property is pointed to by the appropriate entry in the
 * lookup table. Added properties still require a linear search through the LFSLL.
 * If this proves to be a performance issue, we can either keep added entries in a
 * different list, or allow the lookup table to be extended when new entries are added.
 *
 * Property lists are stored in the index, and should only be accessed via their IDs.
 * Within HDF5, the reference count on the property list ID should be incremented before
 * its pointer is looked up in the index, and should not be decremented until the code
 * in question is done with the property list. If this rule is followed religiously, it
 * should be impossible for a property list to be deleted out from under a thread, or
 * for any thread to access a property list after its reference count drops to zero and
 * it is removed from the index and discarded.
 *
 * However, since any failure of this mechanism will be hard to diagnose, an instance
 * of H5P_mt_active_thread_count_t is included in H5P_mt_list_t, and must be maintained.
 * The protocol for doing this is discussed in the header comment for
 * H5P_mt_active_thread_count_t. In the context of H5P_mt_list_t, a positive thread
 * count on discard is an error and should trigger an assertion failure.
 *
 * Similarly, H5P_mt_list_t contains an instance of H5P_mt_list_sptr_t to support a
 * free list that shouldn’t be necessary, but which is probably prudent for much the
 * same reason.
 *
 * Fields:
 *
 * tag (uint32_t):
 *      Integer value set to H5P_MT_LIST_TAG when an instance of H5P_mt_list_t is
 *      allocated from the heap, and set to H5P_MT_LIST_INVALID_TAG just before it is
 *      released back to the heap. The field is used to validate pointers to instances
 *      of H5P_mt_list_t. Additionally, there is a third value to be set to,
 *      H5P_MT_LIST_FL_REALLOC_TAG, when a instance of H5P_mt_list_t on the free list
 *      is guaranteed to no longer be accessed and is available to be realloced for a new
 *      allocation of an instance of H5P_mt_list_t, instead of allocating from the heap.
 *
 *      NOTE: Current implementation doesn't allocate new list from the list free
 *      list, and all are allocated from the heap. More testing needs to be done on the
 *      multithread free lists to ensure there isn't an attempt to access them.
 *
 * pclass_id (hid_t):
 *      ID of the instance of H5P_mt_class_t from which the property list was derived.
 *
 *      This field is not atomic, as it is set before this property list is inserted in
 *      the index, thus before it's visible to other threads, and doesn't change for the
 *      life of this instance.
 *
 * pclass_ptr (H5P_mt_class_t *):
 *      Pointer to the instance of H5P_mt_class_t from which the property list was
 *      derived.
 *
 *      This field is not atomic, as it is set before this property list is inserted in
 *      the index, thus before it's visible to other threads, and doesn't change for the
 *      life of this instance.
 *
 * pclass_version (uint64_t):
 *      Version of the parent property list class from which this property list was
 *      derived.
 *
 * plist_id (_Atomic hid_t):
 *      ID assigned to this property list. This field must be atomic, because the
 *      instance of H5P_mt_list_t becomes visible to other threads before this field can
 *      be set. That said, once it is set, it should not change for the life of the
 *      property list.
 *
 * curr_version (_Atomic uint64_t):
 *      Atomic uint64_t containing the current version of the propety list. This version
 *      number is incremented each time a modification to the property list is completed.
 *
 *      A uint64_t is used, as at present there is no provision for a roll over. Given
 *      the relative infrequency of modifications to property lists, 64-bits is probably
 *      sufficient for all reasonable cases. However, a roll over must never occur, and
 *      an error should be flagged if it does.
 *
 *      To allow an undefined deletion version, the curr_version must be no less than 1.
 *
 * next_version (_Atomic uint64_t):
 *      Atomic uint64_t containing the version number to be assigned to the next
 *      modification of the property list.
 *
 *      When no modificaitons to the property list are in progress, next_version should
 *      be one greater than curr_version.
 *
 *      When a modification to a property list begins, it does a fetch and increment on
 *      next_version, preforms its changes and tags them with the returned version, and
 *      finally increments the curr_version.
 *
 *      NOTE: To avoid exposure of partial modifications, increments to curr_version must
 *      be executed in next_version issue order. Thus, a thread that modifies the propery
 *      list, must not increment curr_version until its value is one less than the
 *      version number it obtained when it started.
 *
 * lkup_tbl (H5P_mt_list_table_entry_t *):
 *      Pointer to an array of H5P_mt_list_table_entry_t that permits fast lookup of
 *      properties inherited from the parent property list class.
 *
 *      See the header comment for H5P_mt_list_table_entry_t for further details.
 *
 * nprops_inherited (size_t):
 *      The number of properties inherited from the parent property list class, and also
 *      the number of entries in the lookup table (lkup_tbl) above. This field is not
 *      atomic, as it is set before this property list is inserted in the index, thus
 *      before it's visible to other threads, and doesn't change for the life of this
 *      instance.
 *
 *      NOTE: Any or all of these properties may be deleted in an arbitrary version of
 *      the property list.
 *
 *      NOTE: If any non-modified inherited properties are deleted from the property list
 *      this field is NOT decremented.
 *
 * nprops_added (_Atomic size_t):
 *      The number of properties added to the property list after its creation.
 *
 *      NOTE: These properties do not appear in the lkup_tbl, and thus if a search for
 *      a property fails in lkup_tbl and nprops_added is positive, the LFSLL pointed to
 *      by pl_head (below) must also be searched.
 *
 * nprops (_Atomic size_t):
 *      Number of properties defined in the current version of the property list.
 *
 *      NOTE: This value may be briefly incorrect during property additions or deletions
 *      -- if an accurate value is required, the LFSLL pointed to by pl_head must be
 *      scanned for the target property list version.
 *
 * TODO: currently nprops_peeked isn't being used, but is set up for the likelihood
 *       it will be needed.
 *
 * nprops_peeked (_Atomic int32_t):
 *      Number of properties that have had the function H5P_peek() called on them, and
 *      still have an active pointer, pointing to that property. While an active pointer
 *      is pointing to a property in the list, it cannot be closed
 *      (maybe just not be cleared), so this field needs to be checked before closing.
 *
 * pl_head (_Atomic H5P_mt_prop_t *):
 *      Atomic pointer to the head of the LFSLL containing the list of modified or
 *      inserted properties (i.e. instances of H5P_mt_prop_t) associated with the
 *      property list. Other than during setup, this field will always point to the first
 *      node in the list whose value will be negative infinity (conceptually).
 *
 *      Entries in this list are sorted first by a hash on the property name, second by
 *      property name (to allow for collisions), and finally by creation version in
 *      decreasing order. Other than during setup, the first and last entries in the list
 *      will be sentinel entries with hash values (conceptually) of negative and positive
 *      infinity respectively.
 *
 * log_pl_len (_Atomic size_t):
 *      Number of valid properties in the LFSLL of the property list at the current
 *      version.
 *
 *      NOTE: This value will not be correct for all versions of the property list, and
 *      will be briefly incorrect even for the current version during property
 *      insertions and deletions. Thus when an exact value is required, the property list
 *      must be scanned for the correct value for the desired version.
 *
 * phys_pl_len (_Atomic size_t):
 *      Number of instances of H5P_mt_prop_t in the property list. This number includes
 *      sentinel nodes, and both current and superseded instanced of H5P_mt_prop_t.
 *
 *      NOTE: This value will be briefly incorrect during property insertions, deletions,
 *      and modifications. Modifications of a property cause this value to change, and a
 *      new instance of H5P_mt_prop_t is inserted with the desired changed and a new
 *      creation version.
 *
 * class_init (_Atomic bool):
 *      True iff the class initialization callback finished successfully.
 *
 *
 * Free list and shutdown management fields:
 *
 * thrd (_Atomic H5P_mt_active_thread_count_t):
 *      This field is a structure used to verify that no threads are active in an
 *      instance of H5P_mt_list_t during setup or takedown of the structure prior to
 *      discard. See the header comment on H5P_mt_active_thread_count_t for a detailed
 *      discription of how its fields must be maintained when a thread wants to access
 *      the host instance of H5P_mt_list_t.
 *
 * fl_next (_Atomic H5P_mt_list_sptr_t):
 *      This field is a structure used to contain a pointer to the next instance of
 *      H5P_mt_list_t in the free list. It is augmented with a serial number to avoid
 *      ABA bugs. This field is included to support a free list of instances of
 *      H5P_mt_list_t.
 *
 *
 * Statistics Fields:
 *
 *
 * Insert statistics:
 *
 * H5P__insert_prop_setup__num_calls (_Atomic uint64_t):
 *      Tracks the number of H5P__insert_prop_setup() function calls. Also is the number
 *      of times H5P__mt_insert_prop() and H5P__mt_create_prop were called for the list.
 *
 * insert_max_nodes_visited (_Atomic uint64_t):
 *      Keeps track of the largest number of nodes visited in the LFSLL of the list
 *      during an insert.
 *
 * insert_avg_nodes_visited (_Atomic uint64_t):
 *      This field tracks the the average number of nodes visited in the LFSLL of the
 *      list across all inserts. Fields H5P__insert_prop_setup__num_calls and
 *      num_insert_node_visited are needed to calculate the insert_avg_nodes_visited.
 *
 * num_insert_nodes_visited (_Atomic uint64_t):
 *      The number of nodes in the LFSLL visited during the most recent insert call.
 *
 * num_insert_prop_cols (_Atomic uint64_t):
 *      The number of collisions with other threads that occur when attempting to
 *      atomically insert a property.
 *
 * num_insert_prop_success (_Atomic uint64_t):
 *      The number of successful property insertions. Will be equal to
 *      H5P__insert_prop_setup__num_calls unless an error occurs during the insert
 *      process
 *
 * num_insert_chksum_cols (_Atomic uint64_t):
 *      The number of inserts where the property being inserted is a different version
 *      of a property that already exists in the LFSLL of the list.
 *
 * num_insert_update_entry_success (_Atomic uint64_t):
 *      The number of successful updates to the curr field of an entry in the lkup_tbl.
 *
 * num_insert_update_entry_cols (_Atomic uint64_t):
 *      The number of collisions with other threads when trying to update the curr field
 *      of an entry in the lkup_tbl.
 *
 *
 * Delete Version statistics:
 *
 * H5P__set_delete_version__num_calls (_Atomic uint64_t):
 *      Tracks the number of H5P__set_delete_version() function calls.
 *
 * set_delete_max_nodes_visited (_Atomic uint64_t):
 *      Keeps track of the largest number of nodes visited in the LFSLL of the list
 *      during a set delete version call.
 *
 * set_delete_avg_nodes_visited (_Atomic uint64_t):
 *      This field tracks the the average number of nodes visited in the LFSLL of the
 *      list across all H5P__ set_delete_version() calls. Fields
 *      H5P__set_delete_version__num_calls and num_set_delete_nodes_visited are needed to
 *      calculate the set_delete_avg_nodes_visited.
 *
 * num_set_delete_nodes_visited (_Atomic uint64_t):
 *      The number of nodes visited during the most recent set_delete_version call.
 *
 * num_set_delete_prop_cols (_Atomic uint64_t):
 *      The number of collisions with other threads that occur when attempting to
 *      atomically set the delete_version of a property.
 *
 * num_set_delete_prop_success (_Atomic uint64_t):
 *      The number of properties in the LFSLL with their delete_version set successfully.
 *      Will be equal to H5P__set_delete_version__num_calls plus
 *      num_set_entry_base_delete_version unless an error occurs during
 *      H5P__set_delete_version().
 *
 * num_set_delete_chksum_cols (_Atomic uint64_t):
 *      The number of properties that have their delete_version set that have another
 *      version of the property in the LFSLL of the list.
 *
 * num_set_entry_base_delete_version (_Atomic uint64_t):
 *      The number of times the property to set the delete_version of was in the lkup_tbl
 *      and the base was the most recent version, so the entry's base_delete_version was
 *      set.
 *
 * num_set_delete_on_curr_entry (_Atomic uint64_t):
 *      The number of times the property to set the delete_version of is the property
 *      that a lkup_tbl entry's curr.ptr points to.
 *
 * num_set_delete_older_ver_than_curr (_Atomic uint64_t):
 *      The number of times a property to set the delete_version of is an older version
 *      of a property that a lkup_tbl's entry curr.ptr points to.
 *
 *
 * Search statistics:
 *
 * H5P__search_prop__num_calls (_Atomic uint64_t):
 *      Tracks the number of H5P__mt_search_prop() function calls.
 *
 * search_max_nodes_visited (_Atomic uint64_t):
 *      Keeps track of the largest number of nodes visited in the LFSLL of the list
 *      during a search function call
 *
 * search_avg_nodes_visited (_Atomic uint64_t):
 *      This field tracks the the average number of nodes visited in the LFSLL of the
 *      list across all search function calls. Fields H5P__search_prop__num_calls and
 *      num_search_nodes_visited are needed to calculate the search_avg_nodes_visited.
 *
 * num_search_nodes_visited (_Atomic uint64_t):
 *      The number of nodes visited during the most recent search call.
 *
 * num_search_success (_Atomic uint64_t):
 *      The number of successful searches in the LFSLL for a property. Should be the same
 *      as H5P__search_prop__num_calls plus num_search_tbl_found_base plus
 *      num_search_tbl_found_curr, unless an error occurs during a search.
 *
 * num_search_chksum_cols (_Atomic uint64_t):
 *      The number of searches where the property being searched for has another version
 *      of the property in the LFSLL of the list.
 *
 * num_search_tbl_found_base (_Atomic uint64_t):
 *      The number of searches in a list where the target property is being pointed to by
 *      an entry in the lkup_tbl's base.ptr.
 *
 * num_search_tbl_found_curr (_Atomic uint64_t):
 *      The number of searches in a list where the target property is being pointed to by
 *      an entry in the lkup_tbl's curr.ptr.
 *
 * num_search_tbl_found_older_than_curr (_Atomic uint64_t):
 *      The number of searches in a list where the target property is an older version of
 *      the property that an entry in the lkup_tbl's curr.ptr is pointing to.
 *
 *
 * Version check statistics:
 *
 * num_wait_for_curr_version_to_inc (_Atomic uint64_t):
 *      The number of times a thread is trying to modify this list's LFSLL in some
 *      manner and has had to wait for the curr_version of the list to increment to be
 *      one less than the next_version that was recieved via an atomic_fetch_add() call.
 *
 *
 * Thread Count statistics:
 *
 * num_thrd_count_update_cols (_Atomic uint64_t):
 *      The number of times this list goes to update its thrd->count field and a
 *      collision with another thread occurs.
 *
 * num_thrd_count_update (_Atomic uint64_t):
 *      The number of times this list has its thrd->count field updated.
 *
 * num_thrd_closing_flag_set (_Atomic uint64_t):
 *      The number of times a thread tried to access this list when its thrd->closing
 *      is set to TRUE.
 *
 * num_thrd_opening_flag_set (_Atomic uint64_t):
 *      The number of times a thread tried to access this list when its thrd->opening
 *      is set to TRUE.
 *
 ****************************************************************************************
 */
#define H5P_MT_LIST_TAG            0x1012 /* 4114 */
#define H5P_MT_LIST_INVALID_TAG    0x2022 /* 8226 */
#define H5P_MT_LIST_FL_REALLOC_TAG 0x3032 /* 12338 */

typedef struct H5P_mt_list_t {
    _Atomic uint32_t tag;

    /* Fields related to the parent class*/
    hid_t           pclass_id;
    H5P_mt_class_t *pclass_ptr;
    uint64_t        pclass_version;

    /* Fields related to this class */
    _Atomic hid_t    plist_id;
    _Atomic uint64_t curr_version;
    _Atomic uint64_t next_version;

    H5P_mt_list_table_entry_t *lkup_tbl;

    /* Fields related to number of properties */
    size_t         nprops_inherited;
    _Atomic size_t nprops_added;
    _Atomic size_t nprops;
    //_Atomic int32_t             nprops_peeked; /* Not added yet, but ready to be if needed */

    /* List of properties in the LFSLL, and related fields */
    H5P_mt_prop_t *pl_head;
    _Atomic size_t log_pl_len;
    _Atomic size_t phys_pl_len;

    _Atomic bool class_init;

    /* Shutdown and free list management fields */
    _Atomic H5P_mt_active_thread_count_t thrd;
    _Atomic H5P_mt_list_sptr_t           fl_next;

    /* stats */

    /* H5P_mt_list_t insert stats */
    _Atomic uint64_t H5P__insert_prop_list__num_calls;
    _Atomic uint64_t insert_max_nodes_visited;
    _Atomic uint64_t insert_avg_nodes_visited;
    _Atomic uint64_t num_insert_nodes_visited;
    _Atomic uint64_t num_insert_prop_cols;
    _Atomic uint64_t num_insert_prop_success;
    _Atomic uint64_t num_insert_update_entry;
    _Atomic uint64_t num_insert_update_entry_cols;
    _Atomic uint64_t num_insert_prop__chksum_cols;

    /* H5P_mt_list_t set delete version stats */
    _Atomic uint64_t H5P__delete_prop__list__num_calls;
    _Atomic uint64_t num_deletes_from_lfsll;
    _Atomic uint64_t set_delete__max_nodes_visited;
    _Atomic uint64_t set_delete__avg_nodes_visited;
    _Atomic uint64_t num_set_delete__nodes_visited;
    _Atomic uint64_t num_set_delete__cols;
    _Atomic uint64_t num_set_delete__success;
    _Atomic uint64_t num_set_delete_chksum_cols;
    _Atomic uint64_t num_set_delete__base_delete_version;
    _Atomic uint64_t num_set_delete__curr_entry;
    _Atomic uint64_t num_set_delete__older_curr;

    /* H5P_mt_list_t search stats */
    _Atomic uint64_t H5P__search_prop__list__num_calls;
    _Atomic uint64_t search_list__max_nodes_visited;
    _Atomic uint64_t search_list__avg_nodes_visited;
    _Atomic uint64_t num_search_list__nodes_visited;
    _Atomic uint64_t num_search_list__success;
    _Atomic uint64_t num_search_chksum_cols;
    _Atomic uint64_t num_search_list__found_base;
    _Atomic uint64_t num_search_list__found_curr;
    _Atomic uint64_t num_target_prop_found_but_deleted;

    /* Version check stats */
    _Atomic uint64_t num_wait_for_curr_version_to_inc;

    /* H5P_mt_active_thread_count_t stats */
    _Atomic uint64_t num_thrd_update_cols;
    _Atomic uint64_t num_thrd_count_update;
    _Atomic uint64_t num_thrd_closing_flag_set;
    _Atomic uint64_t num_thrd_opening_flag_set;

    /* init_lkup_tbl_stats */
    _Atomic uint64_t num_inherited_with_create_cb;
    _Atomic uint64_t num_lkup_tbl_copy_entries_blank;

} H5P_mt_list_t;

/****************************************************************************************
 *
 * Structure: H5P_mt_t
 *
 * Description:
 *
 *      A single, global instance of H5P_mt_t is used to handle the free lists for the
 *      properties (H5P_mt_prop_t struct), lists (H5P_mt_list_t struct), and classes
 *      (H5P_mt_class_t structs), and collects statistical variables related to the free
 *      lists as well as statistical variables used for comparing different classes
 *      and lists.
 *
 * Fields:
 *
 * active_threads (_Atomic uint32_t):
 *      Atomic integer used to track the number of threads currently active in H5P.
 *
 * prop_fl_head (_Atomic H5P_mt_prop_aptr_t):
 *      Atomic instance of struct H5P_mt_prop_aptr_t,
 *
 *
 *
 *
 * Property Free List statistics:
 *
 * prop_fl_head_update (_Atomic uint64_t):
 *      Number of times the head of the property free list was updated.
 *
 * prop_fl_head_update_cols (_Atomic uint64_t):
 *      Number of times a collision occured with another thread when trying to update the
 *      head of the property free list.
 *
 * prop_fl_tail_update (_Atomic uint64_t):
 *      Number of times the tail of the property free list was updated.
 *
 * prop_fl_tail_update_cols (_Atomic uint64_t):
 *      Number of times a collision occured with another thread when trying to update the
 *      tail of the property free list.
 *
 * prop_fl_next_update (_Atomic uint64_t):
 *      Number of times a the next.ptr of a property on the property free list gets
 *      updated.
 *
 * prop_fl_next_update_cols (_Atomic uint64_t):
 *      Number of times a collision occured with another thread when trying to update the
 *      next.ptr of a property on the property free list.
 *
 *
 * List Free List statistics:
 *      TODO: list free list stats
 *
 * Class Free List statistics:
 *      TODO: class free list stats
 *
 *
 * Clear Function Statistics:
 *
 *
 *
 ****************************************************************************************
 */
typedef struct H5P_mt_t {
    _Atomic H5P_mt_prop_aptr_t prop_fl_head;
    _Atomic H5P_mt_prop_aptr_t prop_fl_tail;
    _Atomic uint64_t           prop_fl_len;
    _Atomic uint64_t           prop_max_desired_fl_len;

    _Atomic H5P_mt_class_sptr_t class_fl_head;
    _Atomic H5P_mt_class_sptr_t class_fl_tail;
    _Atomic uint64_t            class_fl_len;
    _Atomic uint64_t            class_max_desired_fl_len;

    _Atomic H5P_mt_list_sptr_t list_fl_head;
    _Atomic H5P_mt_list_sptr_t list_fl_tail;
    _Atomic uint64_t           list_fl_len;
    _Atomic uint64_t           list_max_desired_fl_len;

    /* stats */

    /* Property free list stats */
    _Atomic uint64_t prop_fl_head_update;
    _Atomic uint64_t prop_fl_head_update_cols;
    _Atomic uint64_t prop_fl_tail_update;
    _Atomic uint64_t prop_fl_tail_update_cols;
    _Atomic uint64_t prop_fl_next_update;
    _Atomic uint64_t prop_fl_next_update_cols;
    _Atomic uint64_t num_props_added_to_fl;
    _Atomic uint64_t prop_fl_head_freed_due_to_max_len;
    _Atomic uint64_t prop_fl_head_free_skipped_due_to_empty;
    _Atomic uint64_t prop_fl_head_free_skipped_no_reallocable;

    /* Class free list stats */
    _Atomic uint64_t class_fl_head_update;
    _Atomic uint64_t class_fl_head_update_cols;
    _Atomic uint64_t class_fl_tail_update;
    _Atomic uint64_t class_fl_tail_update_cols;
    _Atomic uint64_t class_fl_next_update;
    _Atomic uint64_t class_fl_next_update_cols;
    _Atomic uint64_t num_class_added_to_fl;
    _Atomic uint64_t class_fl_head_freed_due_to_max_len;
    _Atomic uint64_t class_fl_head_free_skipped_due_to_empty;
    _Atomic uint64_t class_fl_head_free_skipped_no_reallocable;

    /* List free list stats */
    _Atomic uint64_t list_fl_head_update;
    _Atomic uint64_t list_fl_head_update_cols;
    _Atomic uint64_t list_fl_tail_update;
    _Atomic uint64_t list_fl_tail_update_cols;
    _Atomic uint64_t list_fl_next_update;
    _Atomic uint64_t list_fl_next_update_cols;
    _Atomic uint64_t num_list_added_to_fl;
    _Atomic uint64_t list_fl_head_freed_due_to_max_len;
    _Atomic uint64_t list_fl_head_free_skipped_due_to_empty;
    _Atomic uint64_t list_fl_head_free_skipped_no_reallocable;

    /* stats for creating or copying classes */
    _Atomic uint64_t H5P__mt_create_class__num_calls;
    _Atomic uint64_t H5P__mt_copy_class__num_calls;
    _Atomic uint64_t num_class_structs_allocated_from_heap;
    _Atomic uint64_t num_class_structs_allocated_from_fl;

    /* stats for creating or copying lists */
    _Atomic uint64_t H5P__mt_create_list__num_calls;
    _Atomic uint64_t num_list_structs_allocated_from_heap;
    _Atomic uint64_t num_list_structs_allocated_from_fl;
    _Atomic uint64_t H5P__init_lkup_tbl__num_calls;
    _Atomic uint64_t H5P__init_lkup_tbl_copy__num_calls;

    /* stats for creating props */
    _Atomic uint64_t H5P__mt_create_prop__num_calls;
    _Atomic uint64_t num_prop_structs_allocated_from_heap;
    _Atomic uint64_t num_prop_structs_allocated_from_fl;

    /* stats for property inserts */
    _Atomic uint64_t num_props_inserted_classes;
    _Atomic uint64_t num_props_inserted_lists;
    _Atomic uint64_t H5P__mt_ins_or_mod_prop__lfsll_ins__num_calls;

    /* stats for number of deletes */
    _Atomic uint64_t num_props_deleted_classes;
    _Atomic uint64_t num_props_deleted_classes_prop_not_found;
    _Atomic uint64_t num_props_deleted_classes_already_deleted;
    _Atomic uint64_t num_props_deleted_lists;
    _Atomic uint64_t num_props_deleted_lists_prop_not_found;
    _Atomic uint64_t num_props_deleted_lists_already_deleted;

    /* stats for searches */
    _Atomic uint64_t num_searches_classes;
    _Atomic uint64_t num_searches_classes_prop_not_found;
    _Atomic uint64_t num_searches_while_an_op_occurs_class;
    _Atomic uint64_t num_searches_lists;
    _Atomic uint64_t num_searches_lists_prop_not_found;
    _Atomic uint64_t num_searches_while_an_op_occurs_list;

    /* Property chksum cols stats */
    _Atomic uint64_t num_chksum_cols;

    /* H5P__mt_enforce_serialization stats */
    _Atomic uint64_t H5P__mt_enforce_serialization__num_calls;

    /* stats for marking classes deleted or unmarking classes as deleted */
    _Atomic uint64_t close_class_but_pl_not_zero;
    _Atomic uint64_t close_class_but_plc_not_zero;
    _Atomic uint64_t class_un_marked_as_deleted;

    /* stats for the clear functions */
    _Atomic uint64_t num_classes_freed;
    _Atomic uint64_t num_lists_freed;
    _Atomic uint64_t num_props_freed;

    /* H5P_mt_class_t and H5P_mt_list_t comparison stats */
    _Atomic uint64_t max_derived_classes;
    _Atomic uint64_t max_derived_lists;
    _Atomic uint64_t max_class_num_phys_props;
    _Atomic uint64_t max_list_num_phys_props;
    _Atomic uint64_t max_class_version_number;
    _Atomic uint64_t max_list_version_number;

} H5P_mt_t;

/*****************************/
/* Package Private Variables */
/*****************************/

/**
 * This structure contains globals for the class, list, and property free lists,
 * and additionally contains stats for the multithread structures and functions.
 */
H5_DLLVAR H5P_mt_t H5P_mt_g;

/**
 * This is for the test file to have a callback to get the exact
 * version of an object an operation is being performed on.
 */
typedef void (*H5P_version_cb_t)(uint64_t version);

typedef struct H5P_mt_cb_t {
    H5P_version_cb_t ver_cb;
    uint64_t         version;

} H5P_mt_cb_t;

extern _Thread_local H5P_mt_cb_t H5P_mt_cb;

// void H5P__version_tls_set(version_cb_t cb, void *ctx);
// void H5P__version_tls_clear(void);

// void H5P__version_tls_store(H5P_mt_prop_t *prop, uint64_t version);

/******************************/
/* Package Private Prototypes */
/******************************/

herr_t          H5P_mt_init_free_lists(void);
H5P_mt_class_t *H5P__mt_create_class(H5P_mt_class_t *parent, const char *name, H5P_plist_type_t type,
                                     uint64_t src_version, H5P_cls_create_func_t create_func,
                                     void *create_data, H5P_cls_copy_func_t copy_func, void *copy_data,
                                     H5P_cls_close_func_t close_func, void *close_data);
H5P_mt_class_t *H5P__mt_copy_class(H5P_mt_class_t *class);
H5P_mt_class_t *H5P__mt_alloc_class(void);
H5P_mt_list_t  *H5P__mt_create_list(H5P_mt_class_t *parent, H5P_mt_list_t *old_list, bool copy,
                                    uint64_t src_version, bool app_ref);
H5P_mt_list_t  *H5P__mt_alloc_list(void);
herr_t          H5P__init_lkup_tbl(H5P_mt_class_t *parent, uint64_t version, H5P_mt_list_t *list);
herr_t          H5P__init_lkup_tbl_copy(H5P_mt_list_t *old_list, uint64_t version, H5P_mt_list_t *new_list);
H5P_mt_prop_t  *H5P__create_sentinels(bool in_prop_class);
H5P_mt_prop_t  *H5P__mt_create_prop(const char *name, const void *value_ptr, size_t value_size,
                                    bool in_prop_class, uint64_t create_version,
                                    H5P_prp_create_func_t prp_create, H5P_prp_set_func_t prp_set,
                                    H5P_prp_get_func_t prp_get, H5P_prp_encode_func_t prp_encode,
                                    H5P_prp_decode_func_t prp_decode, H5P_prp_delete_func_t prp_del,
                                    H5P_prp_copy_func_t prp_copy, H5P_prp_compare_func_t prp_cmp,
                                    H5P_prp_close_func_t prp_close);

H5P_mt_prop_t *H5P__mt_alloc_prop(void);

herr_t H5P__mt_copy_lfsll(void *param, H5P_mt_prop_t *old_prop, uint64_t version);
herr_t H5P__mt_ins_or_mod_prop__class(H5P_mt_class_t *class, const char *name, void *value, size_t size,
                                      bool is_new, H5P_prp_create_func_t prp_create,
                                      H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get,
                                      H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode,
                                      H5P_prp_delete_func_t prp_del, H5P_prp_copy_func_t prp_copy,
                                      H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close);
herr_t H5P__mt_ins_or_mod_prop__list(H5P_mt_list_t *list, const char *name, void *value, size_t size,
                                     bool create, bool copy, bool is_new, H5P_prp_create_func_t prp_create,
                                     H5P_prp_set_func_t prp_set, H5P_prp_get_func_t prp_get,
                                     H5P_prp_encode_func_t prp_encode, H5P_prp_decode_func_t prp_decode,
                                     H5P_prp_delete_func_t prp_del, H5P_prp_copy_func_t prp_copy,
                                     H5P_prp_compare_func_t prp_cmp, H5P_prp_close_func_t prp_close);
herr_t H5P__mt_ins_or_mod_prop__lfsll_ins(H5P_mt_prop_t *pl_head, H5P_mt_prop_t *new_prop,
                                          uint32_t *deletes_ptr, uint32_t *nodes_visited_ptr,
                                          uint32_t *thrd_cols_ptr, bool *chksum_cols_ptr);
herr_t H5P__mt_delete_prop__class(H5P_mt_class_t *class, const char *name);
herr_t H5P__mt_delete_prop__list(H5P_mt_list_t *list, const char *name);
H5P_mt_prop_t             *H5P__mt_search__class(H5P_mt_class_t *class, const char *name, uint64_t version);
H5P_mt_prop_t             *H5P__mt_search__list(H5P_mt_list_t *list, const char *name, uint64_t version);
H5P_mt_list_table_entry_t *H5P__mt_search_lkup_tbl(H5P_mt_list_table_entry_t *lkup_tbl, size_t left_entry,
                                                   size_t right_entry, int64_t chksum, const char *name);
H5P_mt_prop_t             *H5P__mt_search_lfsll(H5P_mt_prop_t *pl_head, int64_t chksum, const char *name,
                                                uint64_t version, uint64_t *visited, bool *chksum_cols);
H5P_mt_prop_t             *H5P__mt_entry_find_version(H5P_mt_list_table_entry_t *entry, uint64_t version,
                                                      bool *base_flag);
herr_t                     H5P__find_mod_point(H5P_mt_prop_t *pl_head, H5P_mt_prop_t **first_ptr_ptr,
                                               H5P_mt_prop_t **second_ptr_ptr, uint32_t *deletes_ptr, uint32_t *nodes_visited_ptr,
                                               uint32_t *thrd_cols_ptr, int64_t chksum, const char *name, uint64_t version);
H5P_mt_prop_t             *H5P__get_next_valid_prop(H5P_mt_prop_t *prop, uint64_t version, uint64_t *visited);
H5P_mt_prop_t             *H5P__find_valid_version(H5P_mt_prop_t *prop, uint64_t version, uint64_t *visited);
int32_t                    H5P__is_valid(H5P_mt_prop_t *prop, uint64_t version);
#if 0
int32_t
    H5P__mt_compare_prop(H5P_mt_prop_t *prop1, H5P_mt_prop_t *prop2);
#endif
int32_t H5P__mt_prop_cmp(H5P_mt_prop_t *prop1, H5P_mt_prop_t *prop2);
int32_t H5P__mt_cmp_class(H5P_mt_class_t *class1, uint64_t version1, H5P_mt_class_t *class2,
                          uint64_t version2);

int32_t H5P__mt_cmp_list(H5P_mt_list_t *list1, uint64_t version1, H5P_mt_list_t *list2, uint64_t version2);
int32_t H5P__mt_is_derived__class(H5P_mt_class_t *parent, uint64_t version1, H5P_mt_class_t *derived,
                                  uint64_t version2);
int32_t H5P__mt_is_derived__list(H5P_mt_class_t *parent, uint64_t version1, H5P_mt_list_t *derived,
                                 uint64_t version2);
H5P_mt_prop_t  *H5P__mt_next_prop_to_cmp(H5P_mt_prop_t *prop, uint64_t version);
uint64_t        H5P__mt_get_version(void *param);
herr_t          H5P__mt_close_prop(H5P_mt_prop_t *prop);
H5P_mt_prop_t  *H5P__clear_mt_prop(H5P_mt_prop_t *prop);
herr_t          H5P__mt_close_class(H5P_mt_class_t *class);
H5P_mt_class_t *H5P__clear_mt_class(H5P_mt_class_t *class);
herr_t          H5P__mt_close_list(H5P_mt_list_t *list);
H5P_mt_list_t  *H5P__clear_mt_list(H5P_mt_list_t *list);
uint64_t        H5P__mt_enforce_serialization(void *param, uint64_t curr_version, uint64_t next_version);
herr_t          H5P__inc_thrd_count(void *param);
herr_t          H5P__dec_thrd_count(void *param);
herr_t          H5P__inc_ref_count(H5P_mt_class_t *parent, bool plc);
herr_t          H5P__dec_ref_count(H5P_mt_class_t *parent, bool plc);
#if 0 /** NOTE: Didn't actually need this, just put what this does in H5P_get() */
herr_t 
    H5P__mt_get_value(H5P_mt_list_t *list, const char *name, void *value_ptr);
#endif
herr_t   H5P__mt_encode(H5P_mt_list_t *list, uint64_t version, void *buf, size_t *nalloc);
herr_t   H5P__mt_encode_prop(H5P_mt_prop_t *prop, bool encode, size_t *encode_size, uint8_t **p);
uint64_t H5P__calc_avg_visited(uint64_t avg_visited, uint64_t num_calls, uint64_t visited);

/* Stats functions */
herr_t H5P__init_stats_global(void);
herr_t H5P__reset_stats_global(void);
herr_t H5P__init_stats_class(H5P_mt_class_t *class);
herr_t H5P__reset_stats_class(H5P_mt_class_t *class);
herr_t H5P__init_stats_list(H5P_mt_list_t *list);
herr_t H5P__reset_stats_list(H5P_mt_list_t *list);
herr_t H5P__dump_stats_global(FILE *file_ptr);
herr_t H5P__dump_stats_class(FILE *file_ptr, H5P_mt_class_t *class);
herr_t H5P__dump_stats_list(FILE *file_ptr, H5P_mt_list_t *list);

/* shutdown function for free lists */
herr_t H5P__mt_term_free_lists(void);

#endif /* H5Ppkg_mt_H */
