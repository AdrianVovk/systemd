/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <errno.h>
#include <unistd.h>

#include "blockdev-util.h"
#include "device-util.h"
#include "dropin.h"
#include "generator.h"
#include "initrd-util.h"
#include "parse-util.h"
#include "path-util.h"
#include "proc-cmdline.h"
#include "special.h"
#include "volatile-util.h"

static const char *arg_dest = NULL;
static enum {
        INSTALLER_NO,
        INSTALLER_YES,
        INSTALLER_AUTO,
} arg_enabled = INSTALLER_NO;
static VolatileMode arg_volatile = VOLATILE_YES;

static int parse_cmdline(const char *key, const char *value, void *data) {
        int r;

        assert(key);

        if (proc_cmdline_key_streq(key, "systemd.installer")) {
                if (!value)
                        arg_enabled = INSTALLER_YES;
                else if (streq(value, "auto"))
                        arg_enabled = INSTALLER_AUTO;
                else {
                        r = parse_boolean(value);
                        if (r < 0)
                                log_warning_errno(r, "Failed to parse systemd.installer value \"%s\", ignoring: %m", value);
                        arg_enabled = r ? INSTALLER_YES : INSTALLER_NO;
                }
        } else if (proc_cmdline_key_streq(key, "systemd.installer_volatile")) {
                if (!value)
                        arg_volatile = VOLATILE_YES;
                else if (streq(value, "overlay"))
                        arg_volatile = VOLATILE_OVERLAY;
                else {
                        r = parse_boolean(value);
                        if (r < 0)
                                log_warning_errno(r, "Failed to parse systemd.installer_volatile value \"%s\", ignoring: %m", value);
                        else
                                arg_volatile = r ? VOLATILE_YES : VOLATILE_NO;
                }
        }

        return 0;
}

static int reconfigure_repart(void) {
        return write_drop_in_format(arg_dest, "systemd-repart.service", 50, "installer",
                                    "# Automatically generated by systemd-installer-generator\n\n"
                                    "[Service]\n"
                                    "ExecStart=\n"
                                    "ExecStart=" ROOTBINDIR "/systemd-repart --dry-run=no --installer=%s\n",
                                    arg_enabled == INSTALLER_AUTO ? "auto" : "yes");
}

static int enable_volatile_root(void) {
        int r;

        r = generator_add_symlink(arg_dest, SPECIAL_INITRD_ROOT_FS_TARGET, "requires",
                                  SYSTEM_DATA_UNIT_DIR "/" SPECIAL_VOLATILE_ROOT_SERVICE);
        if (r < 0)
                return r;

        if (arg_enabled == INSTALLER_AUTO) {
                r = write_drop_in(arg_dest, SPECIAL_VOLATILE_ROOT_SERVICE, 50, "installer",
                                  "# Automatically generated by systemd-installer-generator\n\n"
                                  "[Service]\n"
                                  "Environment=SYSTEMD_INSTALLER_DISABLE_IF_REMOVABLE=1\n");
                if (r < 0)
                        return r;
        }

        return 0;
}

static int root_is_removable(void) {
        dev_t devno;
        _cleanup_(sd_device_unrefp) sd_device *dev = NULL;
        int r;

        r = blockdev_get_root(LOG_ERR, &devno);
        if (r < 0)
                return r;
        if (r == 0)
                return log_error_errno(SYNTHETIC_ERRNO(EINVAL), "Root file system not backed by a (single) whole block device.");

        r = block_get_whole_disk(devno, &devno);
        if (r < 0)
                return log_error_errno(r, "Failed to get disk from root block device: %m");

        r = sd_device_new_from_devnum(&dev, 'b', devno);
        if (r < 0)
                return log_error_errno(r, "Failed to open root device: %m");

        r = device_is_removable(dev);
        if (r < 0)
                return log_error_errno(r, "Failed to check if root device is removable: %m");
        return r;
}


static int symlink_unit(const char *unit, const char *target) {
        _cleanup_free_ char *p = NULL;

        p = path_join(arg_dest, unit);
        if (!p)
                return log_oom();

        if (symlink(target, p) < 0)
                return log_error_errno(errno, "Failed to link unit %s -> %s: %m", unit, target);

        return 0;
}

static int run(const char *dest, const char *dest_early, const char *dest_late) {
        int r;

        assert_se(arg_dest = dest_early);

        r = proc_cmdline_parse(parse_cmdline, NULL, 0);
        if (r < 0)
                log_warning_errno(r, "Failed to parse kernel command line, ignoring: %m");

        if (arg_enabled == INSTALLER_NO)
                return 0;

        r = reconfigure_repart();
        if (r < 0)
                return log_error_errno(r, "Failed to reconfigure systemd-repart: %m");

        if (in_initrd()) {
                if (arg_volatile != VOLATILE_NO) {
                        r = enable_volatile_root();
                        if (r < 0)
                                return log_error_errno(r, "Failed to enable volatile root: %m");
                }

                log_debug("In initrd; skipping link default.target -> installer.target");
                return 0;
        }

        if (arg_enabled == INSTALLER_AUTO) {
                r = root_is_removable();
                if (r <= 0)
                        return r;
        }

        return symlink_unit(SPECIAL_DEFAULT_TARGET, SYSTEM_DATA_UNIT_DIR "/installer.target");
}

DEFINE_MAIN_GENERATOR_FUNCTION(run);
