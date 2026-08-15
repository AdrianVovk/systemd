/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "sd-varlink-idl.h"

#include "varlink-io.systemd.App.h"

static SD_VARLINK_DEFINE_METHOD(
                RegisterInstance,
                // - Moves the caller into a cgroup (i.e. starts a scope)
                // - appd reserves the right to augment these values with information obtained from other sources

                // app ID (required)

                // collection ID (nullable)

                // instance ID (nullable)
                //      - UUID
                //      - systemd will shorten it in the unit file name (then nobody will rely on trying to parse it out of the unit file name)
                //      - if unset, systemd will allocate a UUID for you

                // sandbox engine (nullable)
                //      - RDNS string
                //      - if set, app isn't allowed to call SetPermissions
                //      - if unset, the app isn't sandboxed

                // permissions - same as SetInstancePermissions

                SD_VARLINK_FIELD_COMMENT("The app's ID"),
                SD_VARLINK_DEFINE_INPUT(id, SD_VARLINK_STRING, 0)
                );

// Cgroup structure:
/apps/app-org.gnome.Calculator-<instance>/primary/<app controlled>
/apps/app-org.gnome.Calculator-<instance>/subapp-org.gnome.Whatever-<instance>/primary/<app controlled>
/apps/app-org.gnome.Calculator-<instance>/subapp-org.gnome.Whatever2-<instance>/primary/<app controlled>

// SetPermissionsError
// - current value of permissions doesn't match expected value, your view of the app's state is out of date

static SD_VARLINK_DEFINE_METHOD(
                SetInstancePermissions,

                // target
                //      connected socket fd => look up its pidfd
                //      or a pidfd => use it directly
                //      or null => look up the pidfd of the caller

                // oldpermissions: [string]object (nullable)
                //      - The old permissions value you got from a previous call to QueryInstance
                //      - appd uses this to enforce atomicity: if someone else overwrote the permissions between your Query and Set, the operation will fail

                // permissions: [string]object
                //      - Map of RDNS permission name to some arbitrary permission value
                //      - RDNS permission name is made up of the RDNS name of the service that enforces the permission, followed by an arbitrary name
                //      - Example: "org.dbus.talk-name": ["org.gnome.Shell", "org.gnome.Whatever"]

                // last write wins
                // we'd take a EXCLUSIVE flock on the cgroup

                );

static SD_VARLINK_DEFINE_METHOD(
                QueryInstance,

                // target
                //      connected socket fd => look up its pidfd
                //      or a pidfd => use it directly
                //      or null => look up the pidfd of the caller

                // Return current values for all the stuff that we pass in Register
                // Make sure we hold a SHARED lock while collecting the state to return in query
                );

SD_VARLINK_DEFINE_INTERFACE(
                io_systemd_App,
                "io.systemd.App",
                SD_VARLINK_INTERFACE_COMMENT("API for managing apps"),
                SD_VARLINK_SYMBOL_COMMENT("TODO"),
                &vl_method_Register,
                SD_VARLINK_SYMBOL_COMMENT("TODO"),
                &vl_method_Query);

// - systemd automatically registers any service w/ appd that matches the service naming pattern
// - we treat static permissions and dynamic permissions identically from appd's perspective
// - app registers itself, but also so can sandbox runtime. If sandbox registers, it blows a fuse and then everything inside of the cgroup is no longer allowed to change its own permissions
// - we don't care about sandbox engine or launcher or collection ID in the cgroup name, call it app-<appid>-<instanceid>.scope
// - we should also include app ID, collection ID, etc in cgroup xattrs because parsing cgroup name sucks big time
