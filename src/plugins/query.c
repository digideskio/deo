/*
 * Copyright (c) 2015 Red Hat, Inc.
 * Author: Nathaniel McCallum <npmccallum@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../main.h"

#include <openssl/err.h>

#include <error.h>
#include <unistd.h>

static int
query(int argc, char *argv[])
{
    PETERA_ERR err = PETERA_ERR_NONE;
    AUTO_STACK(X509, anchors);
    AUTO(PETERA_MSG, rep);
    AUTO(FILE, fp);

    anchors = sk_X509_new_null();
    if (anchors == NULL)
        error(EXIT_FAILURE, ENOMEM, "Unable to make anchors");

    optind = 2;
    for (int c; (c = getopt(argc, argv, "a:")) != -1; ) {
        AUTO(FILE, file);

        switch (c) {
        case 'a':
            file = fopen(optarg, "r");
            if (file == NULL)
                error(EXIT_FAILURE, errno, "Error opening anchor file");

            if (!petera_load(file, anchors))
                error(EXIT_FAILURE, 0, "Error parsing anchor file");

            break;

        default:
            error(EXIT_FAILURE, 0, "Invalid option");
        }
    }

    if (argc - optind != 1) {
        fprintf(stderr,
                "Usage: %s query [-a <anchor(s)> ...] <target>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    rep = petera_request(anchors, &(ASN1_UTF8STRING) {
        .data = (uint8_t *) argv[optind],
        .length = strlen(argv[optind])
    }, &(PETERA_MSG) {
        .type = PETERA_MSG_TYPE_CRT_REQ,
        .value.crt_req = &(ASN1_NULL) {0}
    });

    if (rep == NULL) {
        ERR_print_errors_fp(stderr);
        return EXIT_FAILURE;
    }

    switch (rep->type) {
    case PETERA_MSG_TYPE_ERR:
        err = ASN1_ENUMERATED_get(rep->value.err);
        error(EXIT_FAILURE, 0, "Server error: %s", petera_err_string(err));

    case PETERA_MSG_TYPE_CRT_REP:
        if (!petera_validate(anchors, rep->value.crt_rep))
            error(EXIT_FAILURE, 0, "Validation failed: %s", argv[optind]);

        for (int i = 0; i < sk_X509_num(rep->value.crt_rep); i++)
            PEM_write_X509(stdout, sk_X509_value(rep->value.crt_rep, i));

        return 0;

    default:
        error(EXIT_FAILURE, 0, "Invalid response");
    }

    return EXIT_FAILURE;
}

petera_plugin petera = {
    query, "Fetches and verifies a server's encryption certificate chain"
};