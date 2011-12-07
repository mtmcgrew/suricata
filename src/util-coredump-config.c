/* Copyright (C) 2007-2010 Open Information Security Foundation
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * \file
 *
 * \author Eileen Donlon <emdonlo@gmail.com>
 *
 * Coredump configuration
 */

#define _FILE_OFFSET_BITS 64
#include "util-coredump-config.h"
#include "conf.h"
#include <sys/prctl.h>
#include <sys/resource.h>

/**
 * \brief Configures the core dump size.
 *
 * \retval Returns 1 on success and 0 on failure.
 *
 */
int32_t CoredumpLoadConfig (void)
{
    /* get core dump configuration settings for suricata */
    char* dump_size_config = NULL;
    rlim_t max_dump = 0;
    uint32_t unlimited = 0;
    size_t rlim_size = sizeof(rlim_t);

    if (ConfGet ("coredump.max_dump", &dump_size_config) == 0) {
        SCLogInfo ("Core dump size not specified.");
        return 1;
    }
    if (strcasecmp (dump_size_config, "unlimited") == 0) {
        unlimited = 1;
    }
    else {
        /* disallow negative values */
        if (strchr (dump_size_config, '-') != NULL) {
            SCLogInfo ("Negative value for core dump size; ignored.");
            return 0;
        }
        /* the size of rlim_t is platform dependent */
        if (rlim_size > 8) {
            SCLogInfo ("Unexpected type for rlim_t");
            return 0;
        }
        errno = 0;
        if (rlim_size == 8) {
            max_dump = (rlim_t) strtoull (dump_size_config, NULL, 10);
        }
        else if (rlim_size == 4) {
            max_dump = (rlim_t) strtoul (dump_size_config, NULL, 10);
        }
        if ((errno == ERANGE) || (errno != 0 && max_dump == 0)) {
            SCLogInfo ("Illegal core dump size: %s.", dump_size_config);
            return 0;
        }
        SCLogInfo ("Max dump is %llu", (unsigned long long) max_dump);
    }

#if defined OS_WIN32
    /* todo: use the registry to get/set dump configuration */
    SCLogInfo("Configuring core dump is not yet supported on Windows.");
    return 0;

#elif !defined OS_FREEBSD && !defined __OpenBSD__
    /* Linux specific core dump configuration; set dumpable flag if needed */
    int dumpable = 0;
    dumpable = prctl (PR_GET_DUMPABLE, 0, 0, 0, 0);
    if (dumpable == -1) {
        SCLogInfo ("Can't get core dump configuration of process.");
    }
    else if (unlimited == 1 || max_dump > 0) {
        /* try to enable core dump for this process */
        if (prctl (PR_SET_DUMPABLE, 1, 0, 0, 0) == -1) {
            SCLogInfo ("Unable to make this process dumpable.");
            return 0;
        }
        else SCLogInfo ("Process is dumpable.");
    }
    /* don't clear dumpable flag since this will have other effects;
     * just set dump size to 0 below */
#endif /* Linux specific */

    struct rlimit lim;     /*existing limit*/
    struct rlimit new_lim; /*desired limit*/

    /* get the current core dump file configuration */
    if (getrlimit (RLIMIT_CORE, &lim) == -1) {
       SCLogInfo ("Can't read coredump limit for this process.");
        return 0;
    }

    if (unlimited) {
        /* we want no limit on coredump size */
        if (lim.rlim_max == RLIM_INFINITY && lim.rlim_cur == RLIM_INFINITY) {
            SCLogInfo ("Core dump size is unlimited.");
            return 1;
        }
        else {
            new_lim.rlim_max = RLIM_INFINITY;
            new_lim.rlim_cur = RLIM_INFINITY;
            if (setrlimit (RLIMIT_CORE, &new_lim) == 0) {
                SCLogInfo ("Core dump size set to unlimited.");
                return 1;
            }
            if (errno == EPERM) {
                /* couldn't raise the hard limit to unlimited;
                 * try increasing the soft limit to the hard limit instead */
                if (lim.rlim_cur < lim.rlim_max) {
                    new_lim.rlim_cur = lim.rlim_max;
                    if (setrlimit (RLIMIT_CORE, & new_lim) == 0) {
                        SCLogInfo ("Could not set core dump size to unlimited; core dump size set to the hard limit.");
                        return 0;
                    }
                    else {
                        SCLogInfo ("Failed to set core dump size to unlimited or to the hard limit.");
                        return 0;
                    }
                }
                SCLogInfo ("Could not set core dump size to unlimited; it's set to the hard limit.");
                return 0;
            }
        }
    }
    else {
        /* we want a non-infinite soft limit on coredump size */
        new_lim.rlim_cur = max_dump;

        /* check whether the hard limit needs to be adjusted */
        if (lim.rlim_max == RLIM_SAVED_MAX || lim.rlim_max == RLIM_INFINITY) {
            /* keep the current value (unknown or unlimited) for the hard limit */
            new_lim.rlim_max = lim.rlim_max;
        }
        else if (lim.rlim_max <  max_dump) {
            /* need to raise the hard coredump size limit */
            new_lim.rlim_max =  max_dump;
        }
        else {
            /* hard limit is ample */
            new_lim.rlim_max = lim.rlim_max;
        }
        if (setrlimit (RLIMIT_CORE, &new_lim) == 0) {
            SCLogInfo ("Core dump setting attempted is %llu", (unsigned long long) new_lim.rlim_cur);
            struct rlimit actual_lim;
            if (getrlimit (RLIMIT_CORE, &actual_lim) == 0) {
                if (actual_lim.rlim_cur == RLIM_INFINITY) {
                    SCLogInfo ("Core dump size set to unlimited.");
                }
                else if (actual_lim.rlim_cur == RLIM_SAVED_CUR) {
                    SCLogInfo ("Core dump size set to soft limit.");
                }
                else {
                    SCLogInfo ("Core dump size set to %llu", (unsigned long long) actual_lim.rlim_cur);
                }
            }
            return 1;
        }

        if (errno == EINVAL || errno == EPERM) {
            /* could't increase the hard limit, or the soft limit exceeded the hard
             * limit; try to raise the soft limit to the hard limit */
            if (lim.rlim_cur == RLIM_SAVED_CUR || (lim.rlim_cur < max_dump && lim.rlim_cur < lim.rlim_max)) {
                new_lim.rlim_max = lim.rlim_max;
                new_lim.rlim_cur = lim.rlim_max;
                if (setrlimit (RLIMIT_CORE, &new_lim) == 0)  {
                    SCLogInfo("Core dump size set to the hard limit.");
                    return 0;
                }
            }
        }
    }
    /* failed to set the coredump limit */
    SCLogInfo ("Could't set coredump size to %s.", dump_size_config);
    return 0;
}