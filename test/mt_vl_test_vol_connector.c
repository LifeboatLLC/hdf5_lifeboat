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

/* Purpose:     A simple virtual object layer (VOL) connector with almost no
 *              functionality that is used for testing basic plugin handling
 *              (registration, etc.).
 */

/* For HDF5 plugin functionality */
#include "H5PLextern.h"

/* This connector's header */
#include "mt_vl_test_vol_connector.h"

#include <pthread.h>
#include <stdatomic.h>

#ifdef H5_HAVE_MULTITHREAD 

void *mt_vl_test_file_open(const char *name, unsigned flags, hid_t fapl_id,
                          hid_t dxpl_id, void **req);

herr_t mt_vl_test_file_close(void *file, hid_t dxpl_id, void **req);

herr_t mt_vl_test_file_specific(void *obj, H5VL_file_specific_args_t *args,
                               hid_t dxpl_id, void **req);

herr_t mt_vl_test_introspect_opt_query(void *obj, H5VL_subclass_t subcls,
                                      int opt_type, uint64_t *flags);

/* The VOL class struct */
static const H5VL_class_t mt_vl_test_vol_g = {
    H5VL_VERSION,                  /* VOL class struct version */
    MT_VL_TEST_VOL_CONNECTOR_VALUE, /* value            */
    MT_VL_TEST_VOL_CONNECTOR_NAME,  /* name             */
    0,                             /* connector version */
    H5VL_CAP_FLAG_FILE_BASIC | H5VL_CAP_FLAG_THREADSAFE, /* capability flags */
    NULL,                                                /* initialize       */
    NULL,                                                /* terminate */
    {
        /* info_cls */
        (size_t)0, /* size             */
        NULL,      /* copy             */
        NULL,      /* compare          */
        NULL,      /* free             */
        NULL,      /* to_str           */
        NULL,      /* from_str         */
    },
    {
        /* wrap_cls */
        NULL, /* get_object       */
        NULL, /* get_wrap_ctx     */
        NULL, /* wrap_object      */
        NULL, /* unwrap_object    */
        NULL, /* free_wrap_ctx    */
    },
    {
        /* attribute_cls */
        NULL, /* create           */
        NULL, /* open             */
        NULL, /* read             */
        NULL, /* write            */
        NULL, /* get              */
        NULL, /* specific         */
        NULL, /* optional         */
        NULL  /* close            */
    },
    {
        /* dataset_cls */
        NULL, /* create           */
        NULL, /* open             */
        NULL, /* read             */
        NULL, /* write            */
        NULL, /* get              */
        NULL, /* specific         */
        NULL, /* optional         */
        NULL  /* close            */
    },
    {
        /* datatype_cls */
        NULL, /* commit           */
        NULL, /* open             */
        NULL, /* get_size         */
        NULL, /* specific         */
        NULL, /* optional         */
        NULL  /* close            */
    },
    {
        /* file_cls */
        NULL,                    /* create           */
        mt_vl_test_file_open,     /* open             */
        NULL,                    /* get              */
        mt_vl_test_file_specific, /* specific         */
        NULL, /* optional         */
        mt_vl_test_file_close     /* close            */
    },
    {
        /* group_cls */
        NULL, /* create           */
        NULL,                   /* open             */
        NULL,                   /* get              */
        NULL,                   /* specific         */
        NULL,                   /* optional         */
        NULL                    /* close            */
    },
    {
        /* link_cls */
        NULL, /* create           */
        NULL, /* copy             */
        NULL, /* move             */
        NULL, /* get              */
        NULL, /* specific         */
        NULL  /* optional         */
    },
    {
        /* object_cls */
        NULL, /* open             */
        NULL, /* copy             */
        NULL, /* get              */
        NULL, /* specific         */
        NULL  /* optional         */
    },
    {
        /* introspect_cls */
        NULL,                           /* get_conn_cls     */
        NULL,                           /* get_cap_flags    */
        mt_vl_test_introspect_opt_query, /* opt_query        */
    },
    {
        /* request_cls */
        NULL, /* wait             */
        NULL, /* notify           */
        NULL, /* cancel           */
        NULL, /* specific         */
        NULL, /* optional         */
        NULL  /* free             */
    },
    {
        /* blob_cls */
        NULL, /* put              */
        NULL, /* get              */
        NULL, /* specific         */
        NULL  /* optional         */
    },
    {
        /* token_cls */
        NULL, /* cmp              */
        NULL, /* to_str           */
        NULL  /* from_str         */
    },
    NULL /* optional         */
};

/*--------------------------------------------------------------------------
 * Function: mt_vl_test_file_open
 *
 * Purpose: Always return success to pretend to open a file
 *
 * Return: (void*)1
 *-------------------------------------------------------------------------
 */
void *mt_vl_test_file_open(const char *name, unsigned flags, hid_t fapl_id,
                          hid_t dxpl_id, void **req) {
  /* Silence warnings */
  (void)name;
  (void)flags;
  (void)fapl_id;
  (void)dxpl_id;
  (void)req;

  return (void *)1;
} /* end mt_vl_test_file_open() */

/*--------------------------------------------------------------------------
 * Function: mt_vl_test_file_close
 *
 * Purpose: Always return success to 'close' a fake file object
 *
 * Return: 0 on success, -1 on failure
 *-------------------------------------------------------------------------
 */
herr_t mt_vl_test_file_close(void *file, hid_t dxpl_id, void **req) {
  /* Silence warnings */
  (void)file;
  (void)dxpl_id;
  (void)req;

  return 0;
} /* end mt_vl_test_file_close() */

/*--------------------------------------------------------------------------
 * Function: mt_vl_test_file_specific
 *
 * Purpose: Implement H5Fis_accessible() to always indicate file is accessible,
 *          to facilitate testing file open failure.
 * 
 * Return: 0 on success, -1 on failure
 *-------------------------------------------------------------------------
 */
herr_t mt_vl_test_file_specific(void *obj, H5VL_file_specific_args_t *args,
                               hid_t dxpl_id, void **req) {
  herr_t ret_value = 0;

  /* Silence warnings */
  (void)obj;
  (void)dxpl_id;
  (void)req;

  switch (args->op_type) {
  case H5VL_FILE_IS_ACCESSIBLE: {
    *args->args.is_accessible.accessible = true;
    break;
  }

  default:
    ret_value = -1;
    break;
  }

  return ret_value;
} /* end mt_vl_test_file_specific() */

/* Return 'success' to indicate that this VOL implements expected operations. */
herr_t mt_vl_test_introspect_opt_query(void *obj, H5VL_subclass_t subcls,
                                      int opt_type, uint64_t *flags) {
  herr_t ret_value = 0;

  /* Silence warnings */
  (void)obj;
  (void)subcls;
  (void)opt_type;
  (void)flags;

  return ret_value;
} /* end mt_vl_test_introspect_opt_query() */

/* These two functions are necessary to load this plugin using
 * the HDF5 library.
 */

H5PL_type_t H5PLget_plugin_type(void) { return H5PL_TYPE_VOL; }
const void *H5PLget_plugin_info(void) { return &mt_vl_test_vol_g; }

#endif /* H5_HAVE_MULTITHREAD */