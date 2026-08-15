/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "log.h"
#include "main-func.h"
#include "varlink-util.h"
#include "varlink-io.systemd.App.h"

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

        r = sd_varlink_server_add_interface(varlink_server, &vl_interface_io_systemd_App);
        if (r < 0)
                return log_error_errno(r, "Failed to add Varlink interface: %m");

        r = sd_varlink_server_bind_method_many(
                        varlink_server,
                        "io.systemd.App.Register", vl_method_register,
                        "io.systemd.App.Query", vl_method_query);
        if (r < 0)
                return log_error_errno(r, "Failed to bind Varlink methods: %m");

        r = sd_varlink_server_loop_auto(varlink_server);
        if (r < 0)
                return log_error_errno(r, "Failed to run Varlink event loop: %m");

        return 0;
}

DEFINE_MAIN_FUNCTION (run)
