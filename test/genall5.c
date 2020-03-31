/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.  *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Programmer:  John Mainzer
 *        9/23/15
 *
 *        This file contains a heavily edited and functionaly reduce
 *        version of the test code first written by Quincey in a file
 *        of the same name.
 */

#include "cache_common.h"
#include "genall5.h"

#define DSET_DIMS (1024 * 1024)
#define DSET_SMALL_DIMS (64 * 1024)
#define DSET_CHUNK_DIMS 1024
#define DSET_COMPACT_DIMS 4096


/*-------------------------------------------------------------------------
 * Function:    ns_grp_0
 *
 * Purpose:     Create an empty "new style" group at the specified location
 *              in the specified file.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */

bool
ns_grp_0(hid_t fid, const char *group_name)
{
    hid_t gid = -1;
    hid_t gcpl = -1;
    herr_t ret;

    gcpl = H5Pcreate(H5P_GROUP_CREATE);

    if (gcpl <= 0) {
        failure_mssg = "ns_grp_0: H5Pcreate() failed";
        return false;
    }

    ret = H5Pset_link_creation_order(gcpl, H5P_CRT_ORDER_TRACKED);

    if (ret < 0) {
        failure_mssg = "ns_grp_0: H5Pset_link_creation_order() failed";
        return false;
    }

    gid = H5Gcreate2(fid, group_name, H5P_DEFAULT, gcpl, H5P_DEFAULT);

    if (gid <= 0) {
        failure_mssg = "ns_grp_0: H5Gcreate2() failed";
        return false;
    }

    ret = H5Pclose(gcpl);

    if (ret < 0) {
        failure_mssg = "ns_grp_0: H5Pclose(gcpl) failed";
        return false;
    }

    ret = H5Gclose(gid);

    if (ret < 0) {
        failure_mssg = "ns_grp_0: H5Gclose(gid) failed";
        return false;
    }

    return true;
}


/*-------------------------------------------------------------------------
 * Function:    vrfy_ns_grp_0
 *
 * Purpose:     verify an empty "new style" group at the specified location
 *              in the specified file.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */

bool
vrfy_ns_grp_0(hid_t fid, const char *group_name)
{
    hid_t gid = -1;
    hid_t gcpl = -1;
    H5G_info_t grp_info;
    unsigned crt_order_flags = 0;
    herr_t ret;

    gid = H5Gopen2(fid, group_name, H5P_DEFAULT);

    if (gid <= 0) {
        failure_mssg = "vrfy_ns_grp_0: H5Gopen2() failed";
        return false;
    }

    gcpl = H5Gget_create_plist(gid);

    if (gcpl <= 0) {
        failure_mssg = "vrfy_ns_grp_0: H5Gget_create_plist() failed";
        return false;
    }

    ret = H5Pget_link_creation_order(gcpl, &crt_order_flags);

    if (ret < 0) {
        failure_mssg = "vrfy_ns_grp_0: H5Pget_link_creation_order() failed";
        return false;
    } else if ( H5P_CRT_ORDER_TRACKED != crt_order_flags) {
        failure_mssg =
                "vrfy_ns_grp_0: H5P_CRT_ORDER_TRACKED != crt_order_flags";
        return false;
    }

    ret = H5Pclose(gcpl);

    if (ret < 0) {
        failure_mssg = "vrfy_ns_grp_0: H5Pclose() failed";
        return false;
    }

    HDmemset(&grp_info, 0, sizeof(grp_info));
    ret = H5Gget_info(gid, &grp_info);

    if (ret < 0) {
        failure_mssg = "vrfy_ns_grp_0: H5Gget_info() failed";
        return false;
    } else if (H5G_STORAGE_TYPE_COMPACT != grp_info.storage_type) {
        failure_mssg =
                "vrfy_ns_grp_0: H5G_STORAGE_TYPE_COMPACT != grp_info.storage_type";
        return false;
    } else if (0 != grp_info.nlinks) {
        failure_mssg = "vrfy_ns_grp_0: 0 != grp_info.nlinks";
        return false;
    } else if (0 != grp_info.max_corder) {
        failure_mssg = "vrfy_ns_grp_0: 0 != grp_info.max_corder";
        return false;
    } else if ( FALSE != grp_info.mounted) {
        failure_mssg = "vrfy_ns_grp_0: FALSE != grp_info.mounted";
        return false;
    }

    ret = H5Gclose(gid);

    if (ret < 0) {
        failure_mssg = "vrfy_ns_grp_0: H5Gclose() failed";
        return false;
    }

    return true;
} /* vrfy_ns_grp_0() */


/*-------------------------------------------------------------------------
 * Function:    ns_grp_c
 *
 * Purpose:     Create a compact "new style" group, with 'nlinks'
 *              soft/hard/external links in it in the specified file.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */

bool
ns_grp_c(hid_t fid, const char *group_name, unsigned nlinks)
{
    hid_t gid = -1;
    hid_t gcpl = -1;
    unsigned max_compact;
    unsigned u;
    herr_t ret;

    gcpl = H5Pcreate(H5P_GROUP_CREATE);

    if (gcpl <= 0) {
        failure_mssg = "ns_grp_c: H5Pcreate(H5P_GROUP_CREATE) failed";
        return false;
    }

    ret = H5Pset_link_creation_order(gcpl, H5P_CRT_ORDER_TRACKED);

    if (ret < 0) {
        failure_mssg = "ns_grp_c:  H5Pset_link_creation_order() failed";
        return false;
    }

    gid = H5Gcreate2(fid, group_name, H5P_DEFAULT, gcpl, H5P_DEFAULT);

    if (gid <= 0) {
        failure_mssg = "ns_grp_c: H5Gcreate2() failed";
        return false;
    }

    max_compact = 0;
    ret = H5Pget_link_phase_change(gcpl, &max_compact, NULL);

    if (ret < 0) {
        failure_mssg = "ns_grp_c: H5Pget_link_phase_change() failed";
        return false;
    } else if (nlinks <= 0) {
        failure_mssg = "ns_grp_c: nlinks <= 0";
        return false;
    } else if (nlinks >= max_compact) {
        failure_mssg = "ns_grp_c: nlinks >= max_compact";
        return false;
    }

    for (u = 0; u < nlinks; u++) {
        char linkname[16];

        HDsprintf(linkname, "%u", u);

        if (0 == (u % 3)) {
            ret = H5Lcreate_soft(group_name, gid, linkname, H5P_DEFAULT,
                H5P_DEFAULT);

            if (ret < 0) {
                failure_mssg = "ns_grp_c: H5Lcreate_soft() failed";
                return false;
            }
        } else if (1 == (u % 3)) {
            ret = H5Lcreate_hard(fid, "/", gid, linkname, H5P_DEFAULT,
            H5P_DEFAULT);

            if (ret < 0) {
                failure_mssg = "ns_grp_c: H5Lcreate_hard() failed";
                return false;
            }
        } else {
            HDassert(2 == (u % 3));
            ret = H5Lcreate_external("external.h5", "/ext", gid, linkname,
            H5P_DEFAULT, H5P_DEFAULT);

            if (ret < 0) {
                failure_mssg = "ns_grp_c: H5Lcreate_external() failed";
                return false;
            }
        }
    }

    ret = H5Pclose(gcpl);

    if (ret < 0) {
        failure_mssg = "ns_grp_c: H5Pclose(gcpl) failed";
        return false;
    }

    ret = H5Gclose(gid);

    if (ret < 0) {
        failure_mssg = "ns_grp_c: H5Gclose(gid) failed";
        return false;
    }

    return true;

} /* ns_grp_c() */


/*-------------------------------------------------------------------------
 * Function:    vrfy_ns_grp_c
 *
 * Purpose:     Verify a compact "new style" group, with 'nlinks'
 *              soft/hard/external links in it in the specified file.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */

bool
vrfy_ns_grp_c(hid_t fid, const char *group_name, unsigned nlinks)
{
    hid_t gid = -1;
    hid_t gcpl = -1;
    H5G_info_t grp_info;
    unsigned crt_order_flags = 0;
    unsigned u;
    herr_t ret;

    gid = H5Gopen2(fid, group_name, H5P_DEFAULT);

    if (gid <= 0) {
        failure_mssg = "vrfy_ns_grp_c: H5Gopen2() failed";
        return false;
    }

    gcpl = H5Gget_create_plist(gid);

    if (gcpl <= 0) {
        failure_mssg = "vrfy_ns_grp_c: H5Gget_create_plist(gid) failed";
        return false;
    }

    ret = H5Pget_link_creation_order(gcpl, &crt_order_flags);

    if (ret < 0) {
        failure_mssg = "vrfy_ns_grp_c: H5Pget_link_creation_order() failed";
        return false;
    } else if ( H5P_CRT_ORDER_TRACKED != crt_order_flags) {
        failure_mssg =
            "vrfy_ns_grp_c: H5P_CRT_ORDER_TRACKED != crt_order_flags";
        return false;
    }

    ret = H5Pclose(gcpl);

    if (ret < 0) {
        failure_mssg = "vrfy_ns_grp_c: H5Pclose() failed";
        return false;
    }

    HDmemset(&grp_info, 0, sizeof(grp_info));
    ret = H5Gget_info(gid, &grp_info);

    if (ret < 0) {
        failure_mssg = "vrfy_ns_grp_c: H5Gget_info() failed";
        return false;
    } else if (H5G_STORAGE_TYPE_COMPACT != grp_info.storage_type) {
        failure_mssg =
            "vrfy_ns_grp_c: H5G_STORAGE_TYPE_COMPACT != grp_info.storage_type";
        return false;
    } else if (nlinks != grp_info.nlinks) {
        failure_mssg = "vrfy_ns_grp_c: nlinks != grp_info.nlinks";
        return false;
    } else if (nlinks != grp_info.max_corder) {
        failure_mssg = "vrfy_ns_grp_c: nlinks != grp_info.max_corder";
        return false;
    } else if ( FALSE != grp_info.mounted) {
        failure_mssg = "vrfy_ns_grp_c: FALSE != grp_info.mounted";
        return false;
    }

    
    for (u = 0; u < nlinks; u++) {
        H5L_info_t lnk_info;
        char linkname[16];
        htri_t link_exists;

        HDsprintf(linkname, "%u", u);
        link_exists = H5Lexists(gid, linkname, H5P_DEFAULT);

        if (link_exists < 0) {
            failure_mssg = "vrfy_ns_grp_c: H5Lexists() failed";
            return false;
        }

        HDmemset(&lnk_info, 0, sizeof(grp_info));
        ret = H5Lget_info(gid, linkname, &lnk_info, H5P_DEFAULT);

        if (ret < 0) {
            failure_mssg = "vrfy_ns_grp_c: H5Lget_info() failed";
            return false;
        } else if ( TRUE != lnk_info.corder_valid) {
            failure_mssg = "vrfy_ns_grp_c: TRUE != lnk_info.corder_valid";
            return false;
        } else if (u != lnk_info.corder) {
            failure_mssg = "vrfy_ns_grp_c: u != lnk_info.corder";
            return false;
        } else if (H5T_CSET_ASCII != lnk_info.cset) {
            failure_mssg = "vrfy_ns_grp_c: H5T_CSET_ASCII != lnk_info.cset";
            return false;
        }

        if (0 == (u % 3)) {
            char *slinkval;

            if (H5L_TYPE_SOFT != lnk_info.type) {
                failure_mssg = "vrfy_ns_grp_c: H5L_TYPE_SOFT != lnk_info.type";
                return false;
            }
            else if ((HDstrlen(group_name) + 1) != lnk_info.u.val_size) {
                failure_mssg = "vrfy_ns_grp_c: (HDstrlen(group_name) + 1) != lnk_info.u.val_size";
                return false;
            }

            slinkval = HDmalloc(lnk_info.u.val_size);

            if (!slinkval) {
                failure_mssg = "vrfy_ns_grp_c: HDmalloc of slinkval failed";
                return false;
            }

            ret = H5Lget_val(gid, linkname, slinkval, lnk_info.u.val_size,
            H5P_DEFAULT);
            if (ret < 0) {
                failure_mssg = "vrfy_ns_grp_c: H5Lget_val() failed";
                HDfree(slinkval);
                return false;
            } else if (0 != HDstrcmp(slinkval, group_name)) {
                failure_mssg = "vrfy_ns_grp_c: 0 != HDstrcmp(slinkval, group_name)";
                HDfree(slinkval);
                return false;
            }

            HDfree(slinkval);
        } else if (1 == (u % 3)) {
            H5O_info_t root_oinfo;

            if (H5L_TYPE_HARD != lnk_info.type) {
                failure_mssg = "vrfy_ns_grp_c: H5L_TYPE_HARD != lnk_info.type";
                return false;
            }

            HDmemset(&root_oinfo, 0, sizeof(root_oinfo));
            ret = H5Oget_info2(fid, &root_oinfo, H5O_INFO_BASIC);

            if (ret < 0) {
                failure_mssg = "vrfy_ns_grp_c: H5Oget_info() failed.";
                return false;
            } else if (root_oinfo.addr != lnk_info.u.address) {
                failure_mssg = "vrfy_ns_grp_c: root_oinfo.addr != lnk_info.u.address";
                return false;
            }
        } else {
            void *elinkval;
            const char *file = NULL;
            const char *path = NULL;

            HDassert(2 == (u % 3));

            if (H5L_TYPE_EXTERNAL != lnk_info.type) {
                failure_mssg =
                    "vrfy_ns_grp_c: H5L_TYPE_EXTERNAL != lnk_info.type";
                return false;
            }

            elinkval = HDmalloc(lnk_info.u.val_size);

            if (!elinkval) {
                failure_mssg = "vrfy_ns_grp_c: HDmalloc of elinkval failed.";
                return false;
            }

            ret = H5Lget_val(gid, linkname, elinkval, lnk_info.u.val_size,
            H5P_DEFAULT);
            if (ret < 0) {
                failure_mssg = "vrfy_ns_grp_c: H5Lget_val() failed.";
                return false;
            }

            ret = H5Lunpack_elink_val(elinkval, lnk_info.u.val_size, NULL,
                &file, &path);
            if (ret < 0) {
                failure_mssg = "vrfy_ns_grp_c: H5Lunpack_elink_val() failed.";
                HDfree(elinkval);
                return false;
            } else if (0 != HDstrcmp(file, "external.h5")) {
                failure_mssg =
                    "vrfy_ns_grp_c: 0 != HDstrcmp(file, \"external.h5\")";
                HDfree(elinkval);
                return false;
            } else if (0 != HDstrcmp(path, "/ext")) {
                failure_mssg = "vrfy_ns_grp_c: 0 != HDstrcmp(path, \"/ext\")";
                HDfree(elinkval);
                return false;
            }
            HDfree(elinkval);
        }
    }

    ret = H5Gclose(gid);

    if (ret < 0) {
        failure_mssg = "vrfy_ns_grp_c: H5Gclose() failed.";
        return false;
    }

    return true;
} /* vrfy_ns_grp_c() */


/*-------------------------------------------------------------------------
 * Function:    ns_grp_d
 *
 * Purpose:     Create a dense "new style" group, with 'nlinks'
 *              (soft/hard/external) links in it in the specified file.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */

bool
ns_grp_d(hid_t fid, const char *group_name, unsigned nlinks)
{
    hid_t gid = -1;
    hid_t gcpl = -1;
    unsigned max_compact;
    unsigned u;
    herr_t ret;

    gcpl = H5Pcreate(H5P_GROUP_CREATE);

    if (gcpl <= 0) {
        failure_mssg = "ns_grp_d: H5Pcreate() failed.";
        return false;
    }

    ret = H5Pset_link_creation_order(gcpl, H5P_CRT_ORDER_TRACKED);

    if (ret < 0) {
        failure_mssg = "ns_grp_d: H5Pset_link_creation_order() failed.";
        return false;
    }

    gid = H5Gcreate2(fid, group_name, H5P_DEFAULT, gcpl, H5P_DEFAULT);

    if (gid <= 0) {
        failure_mssg = "ns_grp_d: H5Gcreate2() failed.";
        return false;
    }

    max_compact = 0;
    ret = H5Pget_link_phase_change(gcpl, &max_compact, NULL);

    if (ret < 0) {
        failure_mssg = "ns_grp_d: H5Pget_link_phase_change() failed.";
        return false;
    }
    else if (nlinks <= max_compact) {
        failure_mssg = "ns_grp_d: nlinks <= max_compact";
        return false;
    }

    for (u = 0; u < nlinks; u++) {
        char linkname[16];

        HDsprintf(linkname, "%u", u);

        if (0 == (u % 3)) {
            ret = H5Lcreate_soft(group_name, gid, linkname,
            H5P_DEFAULT, H5P_DEFAULT);

            if (ret < 0) {
                failure_mssg = "ns_grp_d: H5Lcreate_soft() failed.";
                return false;
            }
        } else if (1 == (u % 3)) {
            ret = H5Lcreate_hard(fid, "/", gid, linkname,
            H5P_DEFAULT, H5P_DEFAULT);

            if (ret < 0) {
                failure_mssg = "ns_grp_d: H5Lcreate_hard() failed.";
                return false;
            }
        } else {
            HDassert(2 == (u % 3));

            ret = H5Lcreate_external("external.h5", "/ext", gid, linkname,
            H5P_DEFAULT, H5P_DEFAULT);

            if (ret < 0) {
                failure_mssg = "ns_grp_d: H5Lcreate_external() failed.";
                return false;
            }
        }
    }

    ret = H5Pclose(gcpl);

    if (ret < 0) {
        failure_mssg = "ns_grp_d: H5Pclose() failed.";
        return false;
    }

    ret = H5Gclose(gid);

    if (ret < 0) {
        failure_mssg = "ns_grp_d: H5Gclose() failed.";
        return false;
    }

    return true;
} /* ns_grp_d() */


/*-------------------------------------------------------------------------
 * Function:    vrfy_ns_grp_d
 *
 * Purpose:     Verify a dense "new style" group, with 'nlinks'
 *              soft/hard/external links in it in the specified file.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */


bool
vrfy_ns_grp_d(hid_t fid, const char *group_name, unsigned nlinks)
{
    hid_t gid = -1;
    hid_t gcpl = -1;
    H5G_info_t grp_info;
    unsigned crt_order_flags = 0;
    unsigned u;
    herr_t ret;

    gid = H5Gopen2(fid, group_name, H5P_DEFAULT);

    if (gid <= 0) {
        failure_mssg = "vrfy_ns_grp_d: H5Gopen2() failed.";
        return false;
    }

    gcpl = H5Gget_create_plist(gid);

    if (gcpl <= 0) {
        failure_mssg = "vrfy_ns_grp_d: H5Gget_create_plist() failed.";
        return false;
    }

    ret = H5Pget_link_creation_order(gcpl, &crt_order_flags);

    if (ret < 0) {
        failure_mssg = "vrfy_ns_grp_d: H5Pget_link_creation_order() failed.";
        return false;
    } else if (H5P_CRT_ORDER_TRACKED != crt_order_flags) {
        failure_mssg =
            "vrfy_ns_grp_d: H5P_CRT_ORDER_TRACKED != crt_order_flags";
        return false;
    }

    ret = H5Pclose(gcpl);

    if (ret < 0) {
        failure_mssg = "vrfy_ns_grp_d: H5Pclose() failed.";
        return false;
    }

    HDmemset(&grp_info, 0, sizeof(grp_info));
    ret = H5Gget_info(gid, &grp_info);

    if (ret < 0) {
        failure_mssg = "vrfy_ns_grp_d: H5Gget_info() failed.";
        return false;
    } else if (H5G_STORAGE_TYPE_DENSE != grp_info.storage_type) {
        failure_mssg = "vrfy_ns_grp_d: H5G_STORAGE_TYPE_DENSE != grp_info.storage_type";
        return false;
    } else if (nlinks != grp_info.nlinks) {
        failure_mssg = "vrfy_ns_grp_d: nlinks != grp_info.nlinks";
        return false;
    } else if (nlinks != grp_info.max_corder) {
        failure_mssg = "vrfy_ns_grp_d: nlinks != grp_info.max_corder";
        return false;
    } else if ( FALSE != grp_info.mounted) {
        failure_mssg = "vrfy_ns_grp_d: FALSE != grp_info.mounted";
        return false;
    }

    for (u = 0; u < nlinks; u++) {
        H5L_info_t lnk_info;
        char linkname[16];
        htri_t link_exists;

        HDsprintf(linkname, "%u", u);
        link_exists = H5Lexists(gid, linkname, H5P_DEFAULT);

        if (link_exists < 0) {
            failure_mssg = "vrfy_ns_grp_d: H5Lexists() failed.";
            return false;
        }

        HDmemset(&lnk_info, 0, sizeof(grp_info));
        ret = H5Lget_info(gid, linkname, &lnk_info, H5P_DEFAULT);

        if (ret < 0) {
            failure_mssg = "vrfy_ns_grp_d: H5Lget_info() failed.";
            return false;
        } else if (TRUE != lnk_info.corder_valid) {
            failure_mssg = "vrfy_ns_grp_d: TRUE != lnk_info.corder_valid";
            return false;
        } else if (u != lnk_info.corder) {
            failure_mssg = "vrfy_ns_grp_d: u != lnk_info.corder";
            return false;
        } else if (H5T_CSET_ASCII != lnk_info.cset) {
            failure_mssg = "vrfy_ns_grp_d: H5T_CSET_ASCII != lnk_info.cset";
            return false;
        }

        if (0 == (u % 3)) {
            char *slinkval;

            if (H5L_TYPE_SOFT != lnk_info.type) {
                failure_mssg = "vrfy_ns_grp_d: H5L_TYPE_SOFT != lnk_info.type";
                return false;
            }
            else if ((HDstrlen(group_name) + 1) != lnk_info.u.val_size) {
                failure_mssg = "vrfy_ns_grp_d: H5L_TYPE_SOFT != lnk_info.type";
                return false;
            }

            slinkval = HDmalloc(lnk_info.u.val_size);

            if (!slinkval) {
                failure_mssg = "vrfy_ns_grp_d: HDmalloc of slinkval failed";
                return false;
            }

            ret = H5Lget_val(gid, linkname, slinkval, lnk_info.u.val_size,
                H5P_DEFAULT);
            if (ret < 0) {
                failure_mssg = "vrfy_ns_grp_d: H5Lget_val() failed";
                HDfree(slinkval);
                return false;
            } else if (0 != HDstrcmp(slinkval, group_name)) {
                failure_mssg = "vrfy_ns_grp_d: 0 != HDstrcmp(slinkval, group_name)";
                HDfree(slinkval);
                return false;
            }
            HDfree(slinkval);
        } else if (1 == (u % 3)) {
            H5O_info_t root_oinfo;

            if (H5L_TYPE_HARD != lnk_info.type) {
                failure_mssg = "vrfy_ns_grp_d: H5L_TYPE_HARD != lnk_info.type";
                return false;
            }

            HDmemset(&root_oinfo, 0, sizeof(root_oinfo));
            ret = H5Oget_info2(fid, &root_oinfo, H5O_INFO_BASIC);
            if (ret < 0) {
                failure_mssg = "vrfy_ns_grp_d: H5Oget_info() failed.";
                return false;
            } else if (root_oinfo.addr != lnk_info.u.address) {
                failure_mssg = "vrfy_ns_grp_d: root_oinfo.addr != lnk_info.u.address";
                return false;
            }
        } else {
            void *elinkval;
            const char *file = NULL;
            const char *path = NULL;

            HDassert(2 == (u % 3));

            if (H5L_TYPE_EXTERNAL != lnk_info.type) {
                failure_mssg =
                    "vrfy_ns_grp_d: H5L_TYPE_EXTERNAL != lnk_info.type";
                return false;
            }

            elinkval = HDmalloc(lnk_info.u.val_size);

            if (!elinkval) {
                failure_mssg = "vrfy_ns_grp_d: HDmalloc of elinkval failed.";
                return false;
            }

            ret = H5Lget_val(gid, linkname, elinkval, lnk_info.u.val_size,
                H5P_DEFAULT);
            if (ret < 0) {
                failure_mssg = "vrfy_ns_grp_d: H5Lget_val failed.";
                return false;
            }

            ret = H5Lunpack_elink_val(elinkval, lnk_info.u.val_size, NULL,
                &file, &path);
            if (ret < 0) {
                failure_mssg = "vrfy_ns_grp_d: H5Lunpack_elink_val failed.";
                HDfree(elinkval);
                return false;
            } else if (0 != HDstrcmp(file, "external.h5")) {
                failure_mssg =
                    "vrfy_ns_grp_d: 0 != HDstrcmp(file, \"external.h5\").";
                HDfree(elinkval);
                return false;
            } else if (0 != HDstrcmp(path, "/ext")) {
                failure_mssg = "vrfy_ns_grp_d: 0 != HDstrcmp(path, \"/ext\")";
                HDfree(elinkval);
                return false;
            }
            HDfree(elinkval);
        }
    }

    ret = H5Gclose(gid);

    if (ret < 0) {
        failure_mssg = "vrfy_ns_grp_d: H5Gclose() failed.";
        return false;
    }

    return true;

} /* vrfy_ns_grp_d() */


/*-------------------------------------------------------------------------
 * Function:    os_grp_0
 *
 * Purpose:     Create an empty "old style" group.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */

bool
os_grp_0(hid_t fid, const char *group_name)
{
    hid_t gid = -1;
    hid_t fapl = -1;
    H5F_libver_t low, high;

    herr_t ret;

    /* get the file's file access property list */
    fapl = H5Fget_access_plist(fid);
    if ( fapl <= 0 ) {
        failure_mssg = "os_grp_0: H5Fget_access_plist() failed.";
        return false;
    }

    /* get low and high bounds from fapl */
    ret = H5Pget_libver_bounds(fapl, &low, &high);
    if ( ret < 0 ) {
        failure_mssg = "os_grp_0: H5Pget_libver_bounds() failed(1).";
        return false;
    }

    /* turn file format latest off */
    if(low >= H5F_LIBVER_V18) {
        ret = H5Fset_libver_bounds(fid, H5F_LIBVER_EARLIEST, high);
        if ( ret < 0 ) {
            failure_mssg = "os_grp_0: H5Fset_libver_bounds() failed(1).";
            return false;
        }
    }

    gid = H5Gcreate2(fid, group_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if ( gid <= 0 ) {
        failure_mssg = "os_grp_0: H5Gcreate2() failed.";
        return false;
    }

    ret = H5Gclose(gid);

    if ( ret < 0 ) {
        failure_mssg = "os_grp_0: H5Gclose() failed.";
        return false;
    }

    /* restore low and high bounds */
    if(low >= H5F_LIBVER_V18) {
        ret = H5Fset_libver_bounds(fid, low, high);
        if ( ret < 0 ) {
            failure_mssg = "os_grp_0: H5Fset_libver_bounds() failed(1).";
            return false;
        }
    }

    return true;
} /* os_grp_0() */


/*-------------------------------------------------------------------------
 * Function:    vrfy_os_grp_0
 *
 * Purpose:     Validate an empty "old style" group.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */

bool
vrfy_os_grp_0(hid_t fid, const char *group_name)
{
    hid_t gid = -1;
    hid_t gcpl = -1;
    H5G_info_t grp_info;
    unsigned crt_order_flags = 0;
    herr_t ret;

    gid = H5Gopen2(fid, group_name, H5P_DEFAULT);

    if (gid <= 0) {
        failure_mssg = "vrfy_os_grp_0: H5Gopen2() failed.";
        return false;
    }

    gcpl = H5Gget_create_plist(gid);

    if (gcpl <= 0) {
        failure_mssg = "vrfy_os_grp_0: H5Gget_create_plist() failed.";
        return false;
    }

    ret = H5Pget_link_creation_order(gcpl, &crt_order_flags);

    if (ret < 0) {
        failure_mssg = "vrfy_os_grp_0: H5Pget_link_creation_order() failed";
        return false;
    } else if (0 != crt_order_flags) {
        failure_mssg = "vrfy_os_grp_0: 0 != crt_order_flags";
        return false;
    }

    ret = H5Pclose(gcpl);

    if (ret < 0) {
        failure_mssg = "vrfy_os_grp_0: H5Pclose() failed.";
        return false;
    }

    HDmemset(&grp_info, 0, sizeof(grp_info));
    ret = H5Gget_info(gid, &grp_info);

    if (ret < 0) {
        failure_mssg = "vrfy_os_grp_0: H5Gget_info() failed.";
        return false;
    } else if (H5G_STORAGE_TYPE_SYMBOL_TABLE != grp_info.storage_type) {
        failure_mssg = "vrfy_os_grp_0: H5G_STORAGE_TYPE_SYMBOL_TABLE != grp_info.storage_type";
        return false;
    } else if (0 != grp_info.nlinks) {
        failure_mssg = "vrfy_os_grp_0: 0 != grp_info.nlinks";
        return false;
    } else if (0 != grp_info.max_corder) {
        failure_mssg = "vrfy_os_grp_0: 0 != grp_info.max_corder";
        return false;
    } else if ( FALSE != grp_info.mounted) {
        failure_mssg = "vrfy_os_grp_0: FALSE != grp_info.mounted";
        return false;
    }

    ret = H5Gclose(gid);

    if (ret < 0) {
        failure_mssg = "vrfy_os_grp_0: H5Gclose() failed.";
        return false;
    }

    return true;
} /* vrfy_os_grp_0() */


/*-------------------------------------------------------------------------
 * Function:    os_grp_n
 *
 * Purpose:     Create an "old style" group, with 'nlinks' soft/hard
 *              links in it.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */

bool
os_grp_n(hid_t fid, const char *group_name, int proc_num, unsigned nlinks)
{
    hid_t gid = -1;
    unsigned u;
    hid_t fapl = -1;
    H5F_libver_t low, high;
    herr_t ret;

    /* get the file's file access property list */
    fapl = H5Fget_access_plist(fid);
    if ( fapl <= 0 ) {
        failure_mssg = "os_grp_n: H5Fget_access_plist() failed.";
        return false;
    }

    /* get low and high bounds from fapl */
    ret = H5Pget_libver_bounds(fapl, &low, &high);
    if ( ret < 0 ) {
        failure_mssg = "os_grp_0: H5Pget_libver_bounds() failed(1).";
        return false;
    }

    /* turn file format latest off */
    if(low >= H5F_LIBVER_V18) {
        ret = H5Fset_libver_bounds(fid, H5F_LIBVER_EARLIEST, high);
        if ( ret < 0 ) {
            failure_mssg = "os_grp_0: H5Fset_libver_bounds() failed(1).";
            return false;
        }
    }

    gid = H5Gcreate2(fid, group_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if ( gid <= 0 ) {
        failure_mssg = "os_grp_n: H5Gcreate2() failed.";
        return false;
    }

    HDassert(nlinks > 0);

    for (u = 0; u < nlinks; u++) {
        char linkname[32];

        HDsprintf(linkname, "ln%d_%u", proc_num, u);

        if(0 == (u % 2)) {
            ret = H5Lcreate_soft(group_name, gid, linkname, H5P_DEFAULT, H5P_DEFAULT);
            if ( ret < 0 ) {
                failure_mssg = "os_grp_n: H5Lcreate_soft() failed.";
                return false;
            }
        } else {
            HDassert(1 == (u % 2));

            ret = H5Lcreate_hard(fid, "/", gid, linkname, H5P_DEFAULT, H5P_DEFAULT);
            if ( ret < 0 ) {
                failure_mssg = "os_grp_n: H5Lcreate_hard() failed.";
                return false;
            }
        }
    }

    ret = H5Gclose(gid);

    if ( ret < 0 ) {
        failure_mssg = "os_grp_n: H5Gclose() failed.";
        return false;
    }

    /* restore low and high bounds */
    if(low >= H5F_LIBVER_V18) {
        ret = H5Fset_libver_bounds(fid, low, high);
        if ( ret < 0 ) {
            failure_mssg = "os_grp_n: H5Fset_libver_bounds() failed(2).";
            return false;
        }
    }

    return true;
} /* os_grp_n() */


/*-------------------------------------------------------------------------
 * Function:    vrfy_os_grp_n
 *
 * Purpose:     Validate an "old style" group with 'nlinks' soft/hard
 *              links in it.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */
bool
vrfy_os_grp_n(hid_t fid, const char *group_name, int proc_num, unsigned nlinks)
{
    hid_t gid = -1;
    hid_t gcpl = -1;
    H5G_info_t grp_info;
    unsigned crt_order_flags = 0;
    unsigned u;
    herr_t ret;

    gid = H5Gopen2(fid, group_name, H5P_DEFAULT);

    if (gid <= 0) {
        failure_mssg = "vrfy_os_grp_n: H5Gopen2() failed";
        return false;
    }

    gcpl = H5Gget_create_plist(gid);

    if (gcpl <= 0) {
        failure_mssg = "vrfy_os_grp_n: H5Gget_create_plist() failed";
        return false;
    }

    ret = H5Pget_link_creation_order(gcpl, &crt_order_flags);

    if (ret < 0) {
        failure_mssg = "vrfy_os_grp_n: H5Pget_link_creation_order";
        return false;
    } else if (0 != crt_order_flags) {
        failure_mssg = "vrfy_os_grp_n: 0 != crt_order_flags";
        return false;
    }

    ret = H5Pclose(gcpl);

    if (ret < 0) {
        failure_mssg = "vrfy_os_grp_n: H5Pclose() failed";
        return false;
    }

    HDmemset(&grp_info, 0, sizeof(grp_info));

    ret = H5Gget_info(gid, &grp_info);

    if (ret < 0) {
        failure_mssg = "vrfy_os_grp_n: H5Gget_info() failed";
        return false;
    } else if (H5G_STORAGE_TYPE_SYMBOL_TABLE != grp_info.storage_type) {
        failure_mssg = "vrfy_os_grp_n: H5G_STORAGE_TYPE_SYMBOL_TABLE != grp_info.storage_type";
        return false;
    } else if (nlinks != grp_info.nlinks) {
        failure_mssg = "vrfy_os_grp_n: nlinks != grp_info.nlinks";
        return false;
    } else if (0 != grp_info.max_corder) {
        failure_mssg = "vrfy_os_grp_n: 0 != grp_info.max_corder";
        return false;
    } else if ( FALSE != grp_info.mounted) {
        failure_mssg = "vrfy_os_grp_n: FALSE != grp_info.mounted";
        return false;
    }

    
    for (u = 0; u < nlinks; u++) {
        H5L_info_t lnk_info;
        char linkname[32];
        htri_t link_exists;

        HDsprintf(linkname, "ln%d_%u", proc_num, u);
        link_exists = H5Lexists(gid, linkname, H5P_DEFAULT);

        if (link_exists < 0) {
            failure_mssg = "vrfy_os_grp_n: H5Lexists() failed";
            return false;
        }
        HDassert(link_exists >= 0);

        HDmemset(&lnk_info, 0, sizeof(grp_info));
        ret = H5Lget_info(gid, linkname, &lnk_info, H5P_DEFAULT);

        if (ret < 0) {
            failure_mssg = "vrfy_os_grp_n: H5Lget_info() failed";
            return false;
        }
        else if ( FALSE != lnk_info.corder_valid) {
            failure_mssg = "vrfy_os_grp_n: FALSE != lnk_info.corder_valid";
            return false;
        }
        else if (H5T_CSET_ASCII != lnk_info.cset) {
            failure_mssg = "vrfy_os_grp_n: H5T_CSET_ASCII != lnk_info.cset";
            return false;
        }

        if (0 == (u % 2)) {
            char *slinkval;

            if (H5L_TYPE_SOFT != lnk_info.type) {
                failure_mssg = "vrfy_os_grp_n: H5L_TYPE_SOFT != lnk_info.type";
                return false;
            } else if ((HDstrlen(group_name) + 1) != lnk_info.u.val_size) {
                failure_mssg = "vrfy_os_grp_n: (HDstrlen(group_name) + 1) != lnk_info.u.val_size";
                return false;
            }

            slinkval = HDmalloc(lnk_info.u.val_size);

            if (!slinkval) {
                failure_mssg = "vrfy_os_grp_n: HDmalloc of slinkval failed";
                return false;
            }

            ret = H5Lget_val(gid, linkname, slinkval, lnk_info.u.val_size,
                H5P_DEFAULT);

            if (ret < 0) {
                failure_mssg = "vrfy_os_grp_n: H5Lget_val() failed";
                HDfree(slinkval);
                return false;
            } else if (0 != HDstrcmp(slinkval, group_name)) {
                failure_mssg = "vrfy_os_grp_n: 0 != HDstrcmp(slinkval, group_name)";
                HDfree(slinkval);
                return false;
            }
            HDfree(slinkval);
        } else {
            H5O_info_t root_oinfo;

            HDassert(1 == (u % 2));

            if (H5L_TYPE_HARD != lnk_info.type) {
                failure_mssg = "vrfy_os_grp_n: H5L_TYPE_HARD != lnk_info.type";
                return false;
            }

            HDmemset(&root_oinfo, 0, sizeof(root_oinfo));
            ret = H5Oget_info2(fid, &root_oinfo, H5O_INFO_BASIC);

            if (ret < 0) {
                failure_mssg = "vrfy_os_grp_n: H5Oget_info() failed.";
                return false;
            } else if (root_oinfo.addr != lnk_info.u.address) {
                failure_mssg = "vrfy_os_grp_n: root_oinfo.addr != lnk_info.u.address";
                return false;
            }
        }
    }

    ret = H5Gclose(gid);

    if (ret < 0) {
        failure_mssg = "vrfy_os_grp_n: H5Gclose() failed.";
        return false;
    }

    return true;
} /* vrfy_os_grp_n() */


/*-------------------------------------------------------------------------
 * Function:    ds_ctg_i
 *
 * Purpose:     Create a contiguous dataset w/int datatype.  Write data
 *              to the data set or not as indicated by the write_data
 *              parameter.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */
bool
ds_ctg_i(hid_t fid, const char *dset_name, hbool_t write_data)
{
    int *wdata = NULL;
    unsigned u;
    hid_t dsid = -1;
    hid_t sid = -1;
    hsize_t dims[1] = { DSET_DIMS };
    herr_t ret;

    sid = H5Screate_simple(1, dims, NULL);

    if (sid <= 0) {
        failure_mssg = "ds_ctg_i: H5Screate_simple() failed";
        return false;
    }

    dsid = H5Dcreate2(fid, dset_name, H5T_NATIVE_INT, sid, H5P_DEFAULT,
    H5P_DEFAULT, H5P_DEFAULT);

    if (dsid <= 0) {
        failure_mssg = "ds_ctg_i: H5Dcreate2() failed";
        return false;
    }

    ret = H5Sclose(sid);

    if (ret < 0) {
        failure_mssg = "ds_ctg_i: H5Sclose() failed";
        return false;
    }

    if (write_data) {
        wdata = HDmalloc(sizeof(int) * DSET_DIMS);

        if (!wdata) {
            failure_mssg = "ds_ctg_i: HDmalloc of wdata failed.";
            return false;
        }

        for (u = 0; u < DSET_DIMS; u++)
            wdata[u] = (int) u;

        ret = H5Dwrite(dsid, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT,
            wdata);

        HDfree(wdata);

        if (ret < 0) {
            failure_mssg = "ds_ctg_i: H5Dwrite() failed.";
            return false;
        }
    }

    ret = H5Dclose(dsid);

    if (ret < 0) {
        failure_mssg = "ds_ctg_i: H5Dclose() failed";
        return false;
    }

    return true;
} /* ds_ctg_i */


/*-------------------------------------------------------------------------
 * Function:    vrfy_ds_ctg_i
 *
 * Purpose:     Validate a contiguous datasets w/int datatypes. Validate
 *              data if indicated via the write_data parameter.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */
bool
vrfy_ds_ctg_i(hid_t fid, const char *dset_name, hbool_t write_data)
{
    int *rdata = NULL;
    unsigned u;
    hid_t dsid = -1;
    hid_t sid = -1;
    hid_t tid = -1;
    hid_t dcpl = -1;
    H5D_space_status_t allocation;
    H5D_layout_t layout;
    int ndims;
    hsize_t dims[1], max_dims[1];
    htri_t type_equal;
    herr_t ret;

    dsid = H5Dopen2(fid, dset_name, H5P_DEFAULT);

    if (dsid <= 0) {
        failure_mssg = "vrfy_ds_ctg_i: H5Dopen2() failed.";
        return false;
    }

    sid = H5Dget_space(dsid);

    if (sid <= 0) {
        failure_mssg = "vrfy_ds_ctg_i: H5Dget_space() failed.";
        return false;
    }

    ndims = H5Sget_simple_extent_ndims(sid);

    if (1 != ndims) {
        failure_mssg = "vrfy_ds_ctg_i: 1 != ndims";
        return false;
    }

    ret = H5Sget_simple_extent_dims(sid, dims, max_dims);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_ctg_i: H5Sget_simple_extent_dims() failed";
        return false;
    } else if ( DSET_DIMS != dims[0]) {
        failure_mssg = "vrfy_ds_ctg_i: DSET_DIMS != dims[0]";
        return false;
    } else if ( DSET_DIMS != max_dims[0]) {
        failure_mssg = "vrfy_ds_ctg_i: DSET_DIMS != max_dims[0]";
        return false;
    }

    ret = H5Sclose(sid);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_ctg_i: H5Sclose() failed.";
        return false;
    }

    tid = H5Dget_type(dsid);

    if (tid <= 0) {
        failure_mssg = "vrfy_ds_ctg_i: H5Dget_type() failed.";
        return false;
    }

    type_equal = H5Tequal(tid, H5T_NATIVE_INT);

    if (1 != type_equal) {
        failure_mssg = "vrfy_ds_ctg_i: type not H5T_NATIVE_INT";
        return false;
    }

    ret = H5Tclose(tid);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_ctg_i: H5Tclose() failed.";
        return false;
    }

    ret = H5Dget_space_status(dsid, &allocation);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_ctg_i: H5Dget_space_status() failed.";
        return false;
    } else if (write_data && (allocation != H5D_SPACE_STATUS_ALLOCATED)) {
        failure_mssg = "vrfy_ds_ctg_i: "
            "write_data && allocation != H5D_SPACE_STATUS_ALLOCATED";
        return false;
    } else if (!write_data && (allocation != H5D_SPACE_STATUS_NOT_ALLOCATED)) {
        failure_mssg = "vrfy_ds_ctg_i: "
            "!write_data && allocation != H5D_SPACE_STATUS_NOT_ALLOCATED";
        return false;
    }

    dcpl = H5Dget_create_plist(dsid);

    if (dcpl <= 0) {
        failure_mssg = "vrfy_ds_ctg_i: H5Dget_create_plist() failed.";
        return false;
    }

    layout = H5Pget_layout(dcpl);

    if (H5D_CONTIGUOUS != layout) {
        failure_mssg = "vrfy_ds_ctg_i: H5D_CONTIGUOUS != layout";
        return false;
    }

    ret = H5Pclose(dcpl);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_ctg_i: H5Pclose() failed.";
        return false;
    }

    if (write_data) {
        rdata = HDmalloc(sizeof(int) * DSET_DIMS);

        if (!rdata) {
            failure_mssg = "vrfy_ds_ctg_i: HDmalloc of rdata failed.";
            return false;
        }

        ret = H5Dread(dsid, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT,
            rdata);
        if (ret < 0) {
            failure_mssg = "vrfy_ds_ctg_i: H5Dread() failed.";
            return false;
        }

        for (u = 0; u < DSET_DIMS; u++) {
            if ((int) u != rdata[u]) {
                failure_mssg = "vrfy_ds_ctg_i: u != rdata[u].";
                HDfree(rdata);
                return false;
            }
        }
        HDfree(rdata);
    }

    ret = H5Dclose(dsid);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_ctg_i: H5Dclose() failed";
        return false;
    }

    return true;

} /* vrfy_ds_ctg_i() */


/*-------------------------------------------------------------------------
 * Function:    ds_chk_i
 *
 * Purpose:     Create a chunked dataset w/int datatype.  Write data
 *              to the data set or not as indicated by the write_data
 *              parameter.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */
bool
ds_chk_i(hid_t fid, const char *dset_name, hbool_t write_data)
{
    int *wdata = NULL;
    unsigned u;
    hid_t dsid = -1;
    hid_t dcpl = -1;
    hid_t sid = -1;
    hsize_t dims[1] = { DSET_DIMS };
    hsize_t chunk_dims[1] = { DSET_CHUNK_DIMS };
    herr_t ret;

    sid = H5Screate_simple(1, dims, NULL);

    if (sid <= 0) {
        failure_mssg = "ds_chk_i: H5Screate_simple() failed.";
        return false;
    }

    dcpl = H5Pcreate(H5P_DATASET_CREATE);

    if (dcpl <= 0) {
        failure_mssg = "ds_chk_i: H5Pcreate() failed.";
        return false;
    }

    ret = H5Pset_chunk(dcpl, 1, chunk_dims);

    if (ret < 0) {
        failure_mssg = "ds_chk_i: H5Pset_chunk() failed.";
        return false;
    }

    dsid = H5Dcreate2(fid, dset_name, H5T_NATIVE_INT, sid, H5P_DEFAULT, dcpl,
        H5P_DEFAULT);

    if (dsid <= 0) {
        failure_mssg = "ds_chk_i: H5Dcreate2() failed";
        return false;
    }

    ret = H5Pclose(dcpl);

    if (ret < 0) {
        failure_mssg = "ds_chk_i: H5Pclose() failed.";
        return false;
    }

    ret = H5Sclose(sid);

    if (ret < 0) {
        failure_mssg = "ds_chk_i: H5Sclose() failed.";
        return false;
    }

    if (write_data) {
        wdata = HDmalloc(sizeof(int) * DSET_DIMS);

        if (!wdata) {
            failure_mssg = "ds_chk_i: HDmalloc of wdata failed.";
            return false;
        }

        for (u = 0; u < DSET_DIMS; u++)
            wdata[u] = (int) u;

        ret = H5Dwrite(dsid, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT,
            wdata);
        HDfree(wdata);
        if (ret < 0) {
            failure_mssg = "ds_chk_i: H5Dwrite() failed.";
            return false;
        }
    }

    ret = H5Dclose(dsid);

    if (ret < 0) {
        failure_mssg = "ds_chk_i: H5Dclose() failed.";
        return false;
    }

    return true;
} /* ds_chk_i */


/*-------------------------------------------------------------------------
 * Function:    vrfy_ds_chk_i
 *
 * Purpose:     Validate a chunked datasets w/int datatypes. Validate
 *              data if indicated via the write_data parameter.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */
bool
vrfy_ds_chk_i(hid_t fid, const char *dset_name, hbool_t write_data)
{
    int *rdata = NULL;
    unsigned u;
    hid_t dsid = -1;
    hid_t sid = -1;
    hid_t tid = -1;
    hid_t dcpl = -1;
    H5D_space_status_t allocation;
    H5D_layout_t layout;
    int ndims;
    hsize_t dims[1], max_dims[1], chunk_dims[1];
    htri_t type_equal;
    herr_t ret;

    dsid = H5Dopen2(fid, dset_name, H5P_DEFAULT);

    if (dsid <= 0) {
        failure_mssg = "vrfy_ds_chk_i: H5Dopen2() failed.";
        return false;
    }

    sid = H5Dget_space(dsid);

    if (sid <= 0) {
        failure_mssg = "vrfy_ds_chk_i: H5Dget_space() failed.";
        return false;
    }

    ndims = H5Sget_simple_extent_ndims(sid);

    if (1 != ndims) {
        failure_mssg = "vrfy_ds_chk_i: 1 != ndims";
        return false;
    }

    ret = H5Sget_simple_extent_dims(sid, dims, max_dims);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_chk_i: H5Sget_simple_extent_dims() failed";
        return false;
    } else if ( DSET_DIMS != dims[0]) {
        failure_mssg = "vrfy_ds_chk_i: DSET_DIMS != dims[0]";
        return false;
    } else if ( DSET_DIMS != max_dims[0]) {
        failure_mssg = "vrfy_ds_chk_i: DSET_DIMS != max_dims[0]";
        return false;
    }

    ret = H5Sclose(sid);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_chk_i: H5Sclose() failed.";
        return false;
    }

    tid = H5Dget_type(dsid);

    if (tid <= 0) {
        failure_mssg = "vrfy_ds_chk_i: H5Dget_type() failed.";
        return false;
    }

    type_equal = H5Tequal(tid, H5T_NATIVE_INT);

    if (1 != type_equal) {
        failure_mssg = "vrfy_ds_chk_i: tid != H5T_NATIVE_INT";
        return false;
    }

    ret = H5Tclose(tid);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_chk_i: H5Tclose() failed.";
        return false;
    }

    ret = H5Dget_space_status(dsid, &allocation);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_chk_i: H5Dget_space_status() failed.";
        return false;
    } else if (write_data && (allocation != H5D_SPACE_STATUS_ALLOCATED)) {
        failure_mssg = "vrfy_ds_chk_i: write_data && allocation != H5D_SPACE_STATUS_ALLOCATED";
        return false;
    } else if (!write_data && (allocation != H5D_SPACE_STATUS_NOT_ALLOCATED)) {
        failure_mssg = "vrfy_ds_chk_i: !write_data && allocation != H5D_SPACE_STATUS_NOT_ALLOCATED";
        return false;
    }

    dcpl = H5Dget_create_plist(dsid);

    if (dcpl <= 0) {
        failure_mssg = "vrfy_ds_chk_i: H5Dget_create_plist() failed.";
        return false;
    }

    layout = H5Pget_layout(dcpl);

    if (H5D_CHUNKED != layout) {
        failure_mssg = "vrfy_ds_chk_i: H5D_CHUNKED != layout";
        return false;
    }

    ret = H5Pget_chunk(dcpl, 1, chunk_dims);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_chk_i: H5Pget_chunk";
        return false;
    } else if ( DSET_CHUNK_DIMS != chunk_dims[0]) {
        failure_mssg = "vrfy_ds_chk_i: ";
        return false;
    }

    ret = H5Pclose(dcpl);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_chk_i: H5Pclose() failed.";
        return false;
    }

    if (write_data) {
        rdata = HDmalloc(sizeof(int) * DSET_DIMS);

        if (!rdata) {
            failure_mssg = "vrfy_ds_chk_i: HDmalloc of rdata failed.";
            return false;
        }

        ret = H5Dread(dsid, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT,
            rdata);
        if (ret < 0) {
            failure_mssg = "vrfy_ds_chk_i: H5Dread() failed.";
            return false;
        }

        for (u = 0; u < DSET_DIMS; u++) {
            if ((int) u != rdata[u]) {
                failure_mssg = "vrfy_ds_chk_i: u != rdata[u]";
                HDfree(rdata);
                return false;
            }
        }
        HDfree(rdata);
    }

    ret = H5Dclose(dsid);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_chk_i: H5Dclose() failed.";
        return false;
    }

    return true;
} /* vrfy_ds_chk_i() */


/*-------------------------------------------------------------------------
 * Function:    ds_cpt_i
 *
 * Purpose:     Create a compact dataset w/int datatype.  Write data
 *              to the data set or not as indicated by the write_data
 *              parameter.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */
bool
ds_cpt_i(hid_t fid, const char *dset_name, hbool_t write_data)
{
    int *wdata = NULL;
    unsigned u;
    hid_t dsid = -1;
    hid_t dcpl = -1;
    hid_t sid = -1;
    hsize_t dims[1] = { DSET_COMPACT_DIMS };
    herr_t ret;

    sid = H5Screate_simple(1, dims, NULL);

    if (sid <= 0) {
        failure_mssg = "ds_cpt_i: H5Screate_simple() failed.";
        return false;
    }

    dcpl = H5Pcreate(H5P_DATASET_CREATE);

    if (dcpl <= 0) {
        failure_mssg = "ds_cpt_i: H5Pcreate() failed.";
        return false;
    }

    ret = H5Pset_layout(dcpl, H5D_COMPACT);

    if (ret < 0) {
        failure_mssg = "ds_cpt_i: H5Pset_layout() failed.";
        return false;
    }

    dsid = H5Dcreate2(fid, dset_name, H5T_NATIVE_INT, sid, H5P_DEFAULT, dcpl,
        H5P_DEFAULT);

    if (dsid <= 0) {
        failure_mssg = "ds_cpt_i: H5Dcreate2() failed.";
        return false;
    }

    ret = H5Pclose(dcpl);

    if (ret < 0) {
        failure_mssg = "ds_cpt_i: H5Pclose() failed.";
        return false;
    }

    ret = H5Sclose(sid);

    if (ret < 0) {
        failure_mssg = "ds_cpt_i: H5Sclose() failed.";
        return false;
    }

    if (write_data) {
        wdata = HDmalloc(sizeof(int) * DSET_COMPACT_DIMS);

        if (!wdata) {
            failure_mssg = "ds_cpt_i: HDmalloc of wdata failed.";
            return false;
        }

        for (u = 0; u < DSET_COMPACT_DIMS; u++)
            wdata[u] = (int) u;

        ret = H5Dwrite(dsid, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT,
            wdata);
        HDfree(wdata);

        if (ret < 0) {
            failure_mssg = "ds_cpt_i: H5Dwrite() failed.";
            return false;
        }
    }

    ret = H5Dclose(dsid);

    if (ret < 0) {
        failure_mssg = "ds_cpt_i: H5Dclose() failed.";
        return false;
    }

    return true;

} /* ds_cpt_i() */


/*-------------------------------------------------------------------------
 * Function:    vrfy_ds_cpt_i
 *
 * Purpose:     Validate a compact datasets w/int datatypes. Validate
 *              data if indicated via the write_data parameter.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */
bool
vrfy_ds_cpt_i(hid_t fid, const char *dset_name, hbool_t write_data)
{
    int *rdata = NULL;
    unsigned u;
    hid_t dsid = -1;
    hid_t sid = -1;
    hid_t tid = -1;
    hid_t dcpl = -1;
    H5D_space_status_t allocation;
    H5D_layout_t layout;
    int ndims;
    hsize_t dims[1], max_dims[1];
    htri_t type_equal;
    herr_t ret;

    dsid = H5Dopen2(fid, dset_name, H5P_DEFAULT);

    if (dsid <= 0) {
        failure_mssg = "vrfy_ds_cpt_i: H5Dopen2() failed.";
        return false;
    }

    sid = H5Dget_space(dsid);

    if (sid <= 0) {
        failure_mssg = "vrfy_ds_cpt_i: H5Dget_space() failed.";
        return false;
    }

    ndims = H5Sget_simple_extent_ndims(sid);

    if (1 != ndims) {
        failure_mssg = "vrfy_ds_cpt_i: 1 != ndims";
        return false;
    }

    ret = H5Sget_simple_extent_dims(sid, dims, max_dims);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_cpt_i: H5Sget_simple_extent_dims() failed";
        return false;
    } else if ( DSET_COMPACT_DIMS != dims[0]) {
        failure_mssg = "vrfy_ds_cpt_i: DSET_COMPACT_DIMS != dims[0]";
        return false;
    } else if ( DSET_COMPACT_DIMS != max_dims[0]) {
        failure_mssg = "vrfy_ds_cpt_i: DSET_COMPACT_DIMS != max_dims[0]";
        return false;
    }

    ret = H5Sclose(sid);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_cpt_i: H5Sclose() failed.";
        return false;
    }

    tid = H5Dget_type(dsid);

    if (tid <= 0) {
        failure_mssg = "vrfy_ds_cpt_i: H5Dget_type() failed.";
        return false;
    }

    type_equal = H5Tequal(tid, H5T_NATIVE_INT);

    if (1 != type_equal) {
        failure_mssg = "vrfy_ds_cpt_i: type != H5T_NATIVE_INT";
        return false;
    }

    ret = H5Tclose(tid);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_cpt_i: H5Tclose() failed.";
        return false;
    }

    ret = H5Dget_space_status(dsid, &allocation);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_cpt_i: H5Dget_space_status() failed.";
        return false;
    } else if (H5D_SPACE_STATUS_ALLOCATED != allocation) {
        failure_mssg =
                "vrfy_ds_cpt_i: H5D_SPACE_STATUS_ALLOCATED != allocation";
        return false;
    }

    dcpl = H5Dget_create_plist(dsid);

    if (dcpl <= 0) {
        failure_mssg = "vrfy_ds_cpt_i: H5Dget_create_plist() failed.";
        return false;
    }

    layout = H5Pget_layout(dcpl);

    if (H5D_COMPACT != layout) {
        failure_mssg = "vrfy_ds_cpt_i: H5D_COMPACT != layout";
        return false;
    }

    ret = H5Pclose(dcpl);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_cpt_i: H5Pclose() failed.";
        return false;
    }

    if (write_data) {
        rdata = HDmalloc(sizeof(int) * DSET_COMPACT_DIMS);

        if (!rdata) {
            failure_mssg = "vrfy_ds_cpt_i: HDmalloc of rdata failed.";
            return false;
        }

        ret = H5Dread(dsid, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT,
            rdata);
        if (ret < 0) {
            failure_mssg = "vrfy_ds_cpt_i: H5Dread() failed.";
            return false;
        }

        for (u = 0; u < DSET_COMPACT_DIMS; u++) {
            if ((int) u != rdata[u]) {
                failure_mssg = "vrfy_ds_cpt_i: (int)u != rdata[u]";
                HDfree(rdata);
                return false;
            }
        }
        HDfree(rdata);
    }

    ret = H5Dclose(dsid);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_cpt_i: H5Dclose() failed.";
        return false;
    }

    return true;
} /* vrfy_ds_cpt_i() */


/*-------------------------------------------------------------------------
 * Function:    ds_ctg_v
 *
 * Purpose:     Create a contiguous dataset w/variable-length datatype.
 *              Write data to the data set or not as indicated by the
 *              write_data parameter.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */
bool
ds_ctg_v(hid_t fid, const char *dset_name, hbool_t write_data)
{
    hid_t dsid = -1;
    hid_t sid = -1;
    hid_t tid = -1;
    hsize_t dims[1] = { DSET_SMALL_DIMS };
    herr_t ret;
    hvl_t *wdata = NULL;
    unsigned u;

    sid = H5Screate_simple(1, dims, NULL);

    if (sid <= 0) {
        failure_mssg = "ds_ctg_v: H5Screate_simple";
        return false;
    }

    tid = H5Tvlen_create(H5T_NATIVE_INT);

    if (tid <= 0) {
        failure_mssg = "ds_ctg_v: H5Tvlen_create() failed.";
        return false;
    }

    dsid = H5Dcreate2(fid, dset_name, tid, sid, H5P_DEFAULT,
    H5P_DEFAULT, H5P_DEFAULT);

    if (dsid <= 0) {
        failure_mssg = "ds_ctg_v: H5Dcreate2() failed.";
        return false;
    }

    if (write_data) {
        wdata = HDmalloc(sizeof(hvl_t) * DSET_SMALL_DIMS);

        if (!wdata) {
            failure_mssg = "ds_ctg_v: HDmalloc of wdata failed.";
            return false;
        }

        for (u = 0; u < DSET_SMALL_DIMS; u++) {
            int *tdata;
            unsigned len;
            unsigned v;

            len = (u % 10) + 1;
            tdata = HDmalloc(sizeof(int) * len);

            if (!tdata) {
                failure_mssg = "ds_ctg_v: HDmalloc of tdata failed.";
                while (u > 0)
                    free(wdata[u--].p);
                HDfree(wdata);
                return false;
            }

            for (v = 0; v < len; v++)
                tdata[v] = (int) (u + v);

            wdata[u].len = len;
            wdata[u].p = tdata;
        }

        ret = H5Dwrite(dsid, tid, H5S_ALL, H5S_ALL, H5P_DEFAULT, wdata);

        if (ret < 0) {
            failure_mssg = "ds_ctg_v: H5Dwrite() failed.";
            for (u = 0; u < DSET_SMALL_DIMS; u++)
                free(wdata[u].p);
            HDfree(wdata);
            return false;
        }

        ret = H5Treclaim(tid, sid, H5P_DEFAULT, wdata);

        HDfree(wdata);

        if (ret < 0) {
            failure_mssg = "ds_ctg_v: H5Treclaim() failed.";
            return false;
        }
    }

    ret = H5Sclose(sid);

    if (ret < 0) {
        failure_mssg = "ds_ctg_v: H5Sclose() failed.";
        return false;
    }

    ret = H5Tclose(tid);

    if (tid < 0) {
        failure_mssg = "ds_ctg_v: H5Tclose() failed.";
        return false;
    }

    ret = H5Dclose(dsid);

    if (ret < 0) {
        failure_mssg = "ds_ctg_v: H5Dclose() failed.";
        return false;
    }

    return true;
} /* ds_ctg_v() */


/*-------------------------------------------------------------------------
 * Function:    vrfy_ds_ctg_v
 *
 * Purpose:     Validate a contiguous datasets w/variable-length datatypes.
 *              Validate data if indicated via the write_data parameter.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 * Return:      true on success, false on failure.
 *
 * Programmer:  John Mainzer
 *              9/14/15
 *
 *-------------------------------------------------------------------------
 */
bool
vrfy_ds_ctg_v(hid_t fid, const char *dset_name, hbool_t write_data)
{
    hid_t dsid = -1;
    hid_t sid = -1;
    hid_t tid = -1;
    hid_t tmp_tid = -1;
    hid_t dcpl = -1;
    H5D_space_status_t allocation;
    H5D_layout_t layout;
    int ndims;
    hsize_t dims[1], max_dims[1];
    htri_t type_equal;
    hvl_t *rdata = NULL;
    unsigned u;
    herr_t ret;

    dsid = H5Dopen2(fid, dset_name, H5P_DEFAULT);

    if (dsid <= 0) {
        failure_mssg = "vrfy_ds_ctg_v: H5Dopen2() failed.";
        return false;
    }

    sid = H5Dget_space(dsid);

    if (sid <= 0) {
        failure_mssg = "vrfy_ds_ctg_v: H5Dget_space() failed";
        return false;
    }

    ndims = H5Sget_simple_extent_ndims(sid);

    if (1 != ndims) {
        failure_mssg = "vrfy_ds_ctg_v: 1 != ndims";
        return false;
    }

    ret = H5Sget_simple_extent_dims(sid, dims, max_dims);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_ctg_v: H5Sget_simple_extent_dims() failed.";
        return false;
    } else if ( DSET_SMALL_DIMS != dims[0]) {
        failure_mssg = "vrfy_ds_ctg_v: DSET_SMALL_DIMS != dims[0]";
        return false;
    } else if ( DSET_SMALL_DIMS != max_dims[0]) {
        failure_mssg = "vrfy_ds_ctg_v: DSET_SMALL_DIMS != max_dims[0]";
        return false;
    }

    tid = H5Dget_type(dsid);

    if (tid <= 0) {
        failure_mssg = "vrfy_ds_ctg_v: H5Dget_type() failed.";
        return false;
    }

    tmp_tid = H5Tvlen_create(H5T_NATIVE_INT);

    if (tmp_tid <= 0) {
        failure_mssg = "vrfy_ds_ctg_v: H5Tvlen_create() failed.";
        return false;
    }

    type_equal = H5Tequal(tid, tmp_tid);

    if (1 != type_equal) {
        failure_mssg = "vrfy_ds_ctg_v: type != vlen H5T_NATIVE_INT";
        return false;
    }

    ret = H5Tclose(tmp_tid);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_ctg_v: H5Tclose() failed.";
        return false;
    }

    ret = H5Dget_space_status(dsid, &allocation);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_ctg_v: H5Dget_space_status() failed";
        return false;
    } else if (write_data && (allocation != H5D_SPACE_STATUS_ALLOCATED)) {
        failure_mssg =
                "vrfy_ds_ctg_v: write_data && allocation != H5D_SPACE_STATUS_ALLOCATED";
        return false;
    } else if (!write_data
            && (allocation != H5D_SPACE_STATUS_NOT_ALLOCATED)) {
        failure_mssg =
                "vrfy_ds_ctg_v: !write_data && allocation != H5D_SPACE_STATUS_NOT_ALLOCATED";
        return false;
    }

    dcpl = H5Dget_create_plist(dsid);

    if (dcpl <= 0) {
        failure_mssg = "vrfy_ds_ctg_v: H5Dget_create_plist() failed.";
        return false;
    }

    layout = H5Pget_layout(dcpl);

    if (H5D_CONTIGUOUS != layout) {
        failure_mssg = "vrfy_ds_ctg_v: H5D_CONTIGUOUS != layout";
        return false;
    }

    ret = H5Pclose(dcpl);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_ctg_v: H5Pclose() failed.";
        return false;
    }

    if (write_data) {
        rdata = HDmalloc(sizeof(hvl_t) * DSET_SMALL_DIMS);

        if (!rdata) {
            failure_mssg = "vrfy_ds_ctg_v: HDmalloc of rdata failed.";
            return false;
        }

        ret = H5Dread(dsid, tid, H5S_ALL, H5S_ALL, H5P_DEFAULT, rdata);

        if (ret < 0) {
            failure_mssg = "vrfy_ds_ctg_v: H5Dread() failed.";
            return false;
        }

        for (u = 0; u < DSET_SMALL_DIMS; u++) {
            unsigned len;
            unsigned v;

            len = (unsigned) rdata[u].len;
            for (v = 0; v < len; v++) {
                int *tdata = (int *) rdata[u].p;

                if (!tdata) {
                    failure_mssg = "vrfy_ds_ctg_v: !tdata";
                    return false;
                } else if ((int) (u + v) != tdata[v]) {
                    failure_mssg = "vrfy_ds_ctg_v: (int)(u + v) != tdata[v]";
                    return false;
                }
            }
        }

        ret = H5Treclaim(tid, sid, H5P_DEFAULT, rdata);

        HDfree(rdata);

        if (ret < 0) {
            failure_mssg = "vrfy_ds_ctg_v: H5Treclaim() failed.";
            return false;
        }
    }

    ret = H5Sclose(sid);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_ctg_v: H5Sclose() failed.";
        return false;
    }

    ret = H5Tclose(tid);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_ctg_v: H5Tclose() failed.";
        return false;
    }

    ret = H5Dclose(dsid);

    if (ret < 0) {
        failure_mssg = "vrfy_ds_ctg_v: H5Dclose() failed.";
        return false;
    }

    return true;
} /* vrfy_ds_ctg_v() */

/* Create or, if `validate` is true, validate objects in file `fid` under
 * group `full_path`.  `proc_num` tells the processor number the test runs
 * on.  The set of objects to create/validate is chosen by `selector`.
 *
 * The valid selectors are consecutive and they start at 0.
 *
 * If the selected objects cannot be created/validated, `*okp` will be set
 * to `false`, indicating that the selected objects could not be
 * created/validated, and `failure_mmsg` set to a description of the error
 * that occurred.  If the objects can be created/validated, then `*okp`
 * will be set to `true`.
 *
 * The program may also fail an assert()ion if the selected objects cannot
 * be created/validated.
 * 
 * Return `true` if the selector was valid, `false` if it was not.
 */
static bool
create_or_validate_selection(hid_t fid, const char *full_path, int proc_num,
    int selector, bool skip_varlen, bool validate, bool *okp)
{
    const bool wr = !validate;
    bool ok;

    switch (selector) {
    case 0: /* Add & verify an empty "new style" group */
        ok = wr ? ns_grp_0(fid, full_path) : vrfy_ns_grp_0(fid, full_path);
        break;
    case 1: /* Add & verify a compact "new style" group (3 link messages) */
        ok = wr ? ns_grp_c(fid, full_path, 3)
                : vrfy_ns_grp_c(fid, full_path, 3);
        break;
    case 2:
        /* Add & verify a dense "new style" group (w/300 links,
         * in v2 B-tree & fractal heap)
         */
        ok = wr ? ns_grp_d(fid, full_path, 300)
                : vrfy_ns_grp_d(fid, full_path, 300);
        break;
    case 3: /* Add & verify an empty "old style" group to file */
        ok = wr ? os_grp_0(fid, full_path) : vrfy_os_grp_0(fid, full_path);
        break;
    case 4:
        /* Add & verify an "old style" group (w/300 links, in
         * v1 B-tree & local heap) to file
         */
        ok = wr ? os_grp_n(fid, full_path, proc_num, 300)
                : vrfy_os_grp_n(fid, full_path, proc_num, 300);
        break;
    case 5:
        /* Add & verify a contiguous dataset w/integer datatype (but no data)
         * to file
         */
        ok = wr ? ds_ctg_i(fid, full_path, FALSE)
                : vrfy_ds_ctg_i(fid, full_path, FALSE);
        break;
    case 6:
        /* Add & verify a contiguous dataset w/integer datatype (with data)
         * to file
         */
        ok = wr ? ds_ctg_i(fid, full_path, TRUE)
                : vrfy_ds_ctg_i(fid, full_path, TRUE);
        break;
    case 7:
        /* Add & verify a chunked dataset w/integer datatype (but no data)
         * to file
         */
        ok = wr ? ds_chk_i(fid, full_path, FALSE)
                : vrfy_ds_chk_i(fid, full_path, FALSE);
        break;
    case 8:
        /* Add & verify a chunked dataset w/integer datatype (and data)
         * to file
         */
        ok = wr ? ds_chk_i(fid, full_path, TRUE)
                : vrfy_ds_chk_i(fid, full_path, TRUE);
        break;
    case 9:
        /* Add & verify a compact dataset w/integer datatype (but no data)
         * to file
         */
        ok = wr ? ds_cpt_i(fid, full_path, FALSE)
                : vrfy_ds_cpt_i(fid, full_path, FALSE);
        break;
    case 10:
        /* Add & verify a compact dataset w/integer datatype (and data)
         * to file
         */
        ok = wr ? ds_cpt_i(fid, full_path, TRUE)
                : vrfy_ds_cpt_i(fid, full_path, TRUE);
        break;
    case 11:
        if (skip_varlen)
            return false;
        /* Add & verify a contiguous dataset w/variable-length datatype
         * (but no data) to file
         */
        ok = wr ? ds_ctg_v(fid, full_path, FALSE)
                : vrfy_ds_ctg_v(fid, full_path, FALSE);
        break;
    case 12:
        if (skip_varlen)
            return false;
        /* Add & verify a contiguous dataset w/variable-length datatype
         * (and data) to file
         */
        ok = wr ? ds_ctg_v(fid, full_path, TRUE)
                : vrfy_ds_ctg_v(fid, full_path, TRUE);
        break;
    default:
        return false;
    }
    *okp = ok;
    return true;
}

/* Create and validate objects or, if `only_validate` is true, only
 * validate objects in file `fid` under group `base_path`. `proc_num`
 * tells the processor number the test runs on.  If `skip_varlen` is
 * true, do NOT perform tests that use variable-length data.
 *
 * Return true if all tests pass, false if any test fails.
 */
static bool
tend_zoo(hid_t fid, const char *base_path, int proc_num, bool skip_varlen,
    bool only_validate)
{
    const bool create = !only_validate;
    char full_path[1024];
    int i, nwritten;
    char *leafp;
    bool ok = true;

    nwritten = snprintf(full_path, sizeof(full_path), "%s/*", base_path);
    if (nwritten < 0 || (size_t)nwritten >= sizeof(full_path)) {
        failure_mssg = "create_zoo: snprintf failed";
        return false;
    }

    if ((leafp = strrchr(full_path, '*')) == NULL) {
        failure_mssg = "create_zoo: strrchr failed";
        return false;
    }

    for (i = 0; ok; i++) {
        assert('A' + i <= 'Z');
        *leafp = (char)('A' + i);
        if (create &&
            !create_or_validate_selection(fid, full_path, proc_num, i,
                skip_varlen, false, &ok))
            break;
        zoo_create_hook(fid);
        if (!create_or_validate_selection(fid, full_path, proc_num, i,
                skip_varlen, true, &ok))
            break;
    }
    if (!ok)
        fprintf(stderr, "%s: %s\n", __func__, failure_mssg);
    return ok;
}

/*-------------------------------------------------------------------------
 * Function:    create_zoo
 *
 * Purpose:     Given the path to a group, construct a variety of HDF5
 *              data sets, groups, and other objects selected so as to
 *              include instances of all on disk data structures used
 *              in the HDF5 library.
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 *              This function was initially created to assist in testing
 *              the cache image feature of the metadata cache.  Thus, it
 *              only concerns itself with the version 2 superblock, and
 *              on disk structures that can occur with this version of
 *              the superblock.
 *
 *              Note the associated validate_zoo() function.
 *-------------------------------------------------------------------------
 */

bool
create_zoo(hid_t fid, const char *base_path, int proc_num, bool skip_varlen)
{
    return tend_zoo(fid, base_path, proc_num, skip_varlen, false);
}

/*-------------------------------------------------------------------------
 * Function:    validate_zoo
 *
 * Purpose:     Given the path to a group in which a "zoo" has been
 *              constructed, validate the objects in the "zoo".
 *
 *              If an error is detected, return false, and set
 *              failure_mssg to point to an appropriate error message.
 *
 *              This function was initially created to assist in testing
 *              the cache image feature of the metadata cache.  Thus, it
 *              only concerns itself with the version 2 superblock, and
 *              on disk structures that can occur with this version of
 *              the superblock.
 *
 *              Note the associated create_zoo() function.
 *-------------------------------------------------------------------------
 */

bool
validate_zoo(hid_t fid, const char *base_path, int proc_num, bool skip_varlen)
{
    return tend_zoo(fid, base_path, proc_num, skip_varlen, true);
}
