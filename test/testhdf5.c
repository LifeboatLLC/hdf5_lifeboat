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
   FILE
   testhdf5.c - HDF5 testing framework main file.

   REMARKS
   General test wrapper for HDF5 base library test programs

   DESIGN
   Each test function should be implemented as function having no
   parameters and returning void (i.e. no return value).  They should be put
   into the list of AddTest() calls in main() below.  Functions which depend
   on other functionality should be placed below the AddTest() call for the
   base functionality testing.
   Each test module should include testhdf5.h and define a unique set of
   names for test files they create.

   BUGS/LIMITATIONS


 */

/* ANY new test needs to have a prototype in testhdf5.h */
#include "testhdf5.h"

int
main(int argc, char *argv[])
{
    /* Initialize testing framework */
    TestInit(argv[0], NULL, NULL);

    /* Tests are generally arranged from least to most complexity... */
    AddTest("config",  test_configure,  cleanup_configure,  "Configure definitions",  NULL, 0);
    AddTest("h5system",  test_h5_system,  cleanup_h5_system,  "H5system routines",  NULL, 0);
    AddTest("metadata", test_metadata, cleanup_metadata, "Encoding/decoding metadata", NULL, 0);
    AddTest("checksum",  test_checksum,  cleanup_checksum,  "Checksum algorithm",  NULL, 0);
    AddTest("skiplist",  test_skiplist,  NULL,  "Skip Lists",  NULL, 0);
    AddTest("refstr",  test_refstr,  NULL,  "Reference Counted Strings",  NULL, 0);
    AddTest("file", test_file, cleanup_file, "Low-Level File I/O", NULL, 0);
    AddTest("objects",  test_h5o,  cleanup_h5o,  "Generic Object Functions",  NULL, 0);
    AddTest("h5s",  test_h5s,  cleanup_h5s,  "Dataspaces",  NULL, 0);
    AddTest("coords",  test_coords,  cleanup_coords,  "Dataspace coordinates",  NULL, 0);
    AddTest("sohm",  test_sohm,  cleanup_sohm,  "Shared Object Header Messages",  NULL, 0);
    AddTest("attr",  test_attr,  cleanup_attr,  "Attributes",  NULL, 0);
    AddTest("select",  test_select,  cleanup_select,  "Selections",  NULL, 0);
    AddTest("time",  test_time,  cleanup_time,  "Time Datatypes",  NULL, 0);
    AddTest("ref_deprec",  test_reference_deprec,  cleanup_reference_deprec,  "Deprecated References",  NULL, 0);
    AddTest("ref",  test_reference,  cleanup_reference,  "References",  NULL, 0);
    AddTest("vltypes",  test_vltypes,  cleanup_vltypes,  "Variable-Length Datatypes",  NULL, 0);
    AddTest("vlstrings",  test_vlstrings,  cleanup_vlstrings,  "Variable-Length Strings",  NULL, 0);
    AddTest("iterate", test_iterate, cleanup_iterate, "Group & Attribute Iteration", NULL, 0);
    AddTest("array",  test_array,  cleanup_array,  "Array Datatypes",  NULL, 0);
    AddTest("genprop",  test_genprop,  cleanup_genprop,  "Generic Properties",  NULL, 0);
    AddTest("unicode",  test_unicode,  cleanup_unicode,  "UTF-8 Encoding",  NULL, 0);
    AddTest("id",  test_ids,  NULL,  "User-Created Identifiers",  NULL, 0);
    AddTest("misc",  test_misc,  cleanup_misc,  "Miscellaneous",  NULL, 0);

    /* Display testing information */
    TestInfo(argv[0]);

    /* Parse command line arguments */
    TestParseCmdLine(argc, argv);

    /* Perform requested testing */
    PerformTests();

    /* Display test summary, if requested */
    if (GetTestSummary())
        TestSummary();

    /* Clean up test files, if allowed */
    if (GetTestCleanup() && !HDgetenv(HDF5_NOCLEANUP))
        TestCleanup();

    /* Release test infrastructure */
    TestShutdown();

    /* Exit failure if errors encountered; else exit success. */
    /* No need to print anything since PerformTests() already does. */
    if (GetTestNumErrs() > 0)
        exit(EXIT_FAILURE);
    else
        exit(EXIT_SUCCESS);
} /* end main() */
