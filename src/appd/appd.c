/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "alloc-util.h"
#include "cgroup-util.h"
#include "log.h"
#include "main-func.h"
#include "unit-name.h"
#include "varlink-util.h"
#include "varlink-io.systemd.AppInstance.h"

// USING DBUS IS _NOT ALLOWED_

// target
//      connected socket fd => look up its pidfd
//      or a pidfd => use it directly
//      or null => look up the pidfd of the caller
// (We don't allow a sandboxed caller to set a target other than itself?)

// Cgroup structure:
// /apps/app-org.gnome.Calculator-<instance>/primary/<app controlled>
// /apps/app-org.gnome.Calculator-<instance>/subapp-org.gnome.Whatever-<instance>/primary/<app controlled>
// /apps/app-org.gnome.Calculator-<instance>/subapp-org.gnome.Whatever2-<instance>/primary/<app controlled>
//
// /app.slice/app-<launcher>-<appid>-<instance>.scope/<app controlled>
// /app.slice/app[-<launcher>]-<appid>@<instance>.service/<app controlled>

// Algorithm to transform a cgroup name to a stable FD to the appd cgroup:
// - Get the cgroup path from the pidref
// - Open that cgroup
//

static int ensure_cgroup(const PidRef *instance, const char *instance_app_id, char *ret_cgroup) {
        int r;

        //unit_name_to_type
        //unit_name_to_app_id

        _cleanup_free_ char *cgroup = NULL;
        r = cg_pidref_get_path(instance, &cgroup);

        _cleanup_free_ char *unit = NULL;
        r = cg_pidref_get_user_unit (instance, &unit);
        if (r >= 0) {
                _cleanup_free_ char *unit_app_id = NULL;
                // Check if the unit matches naming spec compliance
        }

        // Find the .service or .scope unit that the app is currently in (cg_pidref_get_user_unit)
        //
        // If unit exists
        //      Check unit for naming spec compliance (make sure to check aliases!)
        //      If following
        //              Check if app ID matches the one provided here at call site
        //              If matches
        //                      return success
        //              Else if unit is sandboxed
        //                      return ConflictingIdentity error
        //
        // do
        //      Pick a random number for disambiguation
        //      Create a scope for this process
        // while Scope creation failed b/c of duplicate
}

typedef struct MethodRegisterParams {
        const char *id;
} MethodRegisterParams;

static int vl_method_register(
                sd_varlink *link,
                sd_json_variant *parameters,
                sd_varlink_method_flags_t flags,
                void *userdata) {

        static const sd_json_dispatch_field dispatch_table[] = {
                { "id", SD_JSON_VARIANT_STRING, sd_json_dispatch_const_string, offsetof (MethodRegisterParams, id), 0 },
                {}
        };
        MethodRegisterParams p = {};
        int r;

        assert(link);

        r = sd_varlink_dispatch(link, parameters, dispatch_table, &p);
        if (r != 0)
                return r;

        log_warning ("Not implemented! register(id='%s')", p.id);

        return sd_varlink_reply (link, NULL);
}

static int vl_method_query(
                sd_varlink *link,
                sd_json_variant *parameters,
                sd_varlink_method_flags_t flags,
                void *userdata) {

        assert(link);

        return sd_varlink_reply (link, NULL);
}

static int run(int argc, char *argv[]) {
        _cleanup_(sd_varlink_server_unrefp) sd_varlink_server *varlink_server = NULL;
        int r;

        log_setup ();

        if (argc != 1)
                return log_error_errno(SYNTHETIC_ERRNO(EINVAL), "This program takes no arguments.");

        r = varlink_server_new(
                        &varlink_server,
                        SD_VARLINK_SERVER_HANDLE_SIGINT|
                        SD_VARLINK_SERVER_HANDLE_SIGTERM,
                        /* userdata= */ NULL);
        if (r < 0)
                return log_error_errno(r, "Failed to allocate Varlink server: %m");

        r = sd_varlink_server_add_interface(varlink_server, &vl_interface_io_systemd_AppInstance);
        if (r < 0)
                return log_error_errno(r, "Failed to add Varlink interface: %m");

        r = sd_varlink_server_bind_method_many(
                        varlink_server,
                        "io.systemd.AppInstance.Register", vl_method_register_instance,
                        "io.systemd.AppInstance.Query", vl_method_query_instance,
                        "io.systemd.AppInstance.SetPermissions", vl_method_set_instance_permissions);
        if (r < 0)
                return log_error_errno(r, "Failed to bind Varlink methods: %m");

        r = sd_varlink_server_loop_auto(varlink_server);
        if (r < 0)
                return log_error_errno(r, "Failed to run Varlink event loop: %m");

        return 0;
}

DEFINE_MAIN_FUNCTION (run)

// Make sure we hold a SHARED lock while collecting the state to return in query

// SetPermissionsError
// - current value of permissions doesn't match expected value, your view of the app's state is out of date
// - caller is the target and also sandboxed, can't change own permissions

// permissions: [string]object
//      - Map of RDNS permission name to some arbitrary permission value
//      - RDNS permission name is made up of the RDNS name of the service that enforces the permission, followed by an arbitrary name
//      - Example: "org.dbus.talk-name": ["org.gnome.Shell", "org.gnome.Whatever"]

// - systemd automatically registers any service w/ appd that matches the service naming pattern
// - we treat static permissions and dynamic permissions identically from appd's perspective
// - app registers itself, but also so can sandbox runtime. If sandbox registers, it blows a fuse and then everything inside of the cgroup is no longer allowed to change its own permissions
// - we don't care about sandbox engine or launcher or collection ID in the cgroup name, call it app-<appid>-<instanceid>.scope
// - we should also include app ID, collection ID, etc in cgroup xattrs because parsing cgroup name sucks big time
//      - We actually _can't_ parse the cgroup name reliably! .service units might name themselves with an alias!


// todo: entitlements
