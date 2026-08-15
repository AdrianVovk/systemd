# SPDX-License-Identifier: LGPL-2.1-or-later

executables += [
        libexec_template + {
                'name' : 'systemd-appd',
                'conditions' : [
                        'ENABLE_APPD'
                ],
                'sources' : files('appd.c'),
        },
]
