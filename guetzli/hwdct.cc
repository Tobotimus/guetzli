/*
 * Copyright 2016 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Integer implementation of the Discrete Cosine Transform (DCT)
//
// Note! DCT output is kept scaled by 16, to retain maximum 16bit precision

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "guetzli/hwdct.h"


namespace guetzli {

void FifoWriteBlock(coeff_t *buf, int fd) {
    // static int numWrites = 0;
    int donebytes, rc;

    donebytes = 0;
    while (donebytes < sizeof(coeff_t) * 3 * kDCTBlockSize) {
        rc = write(fd, buf + donebytes, sizeof(coeff_t) * (3 * kDCTBlockSize) - donebytes);

        if ((rc < 0) && (errno == EINTR))
            continue;


        if (rc < 0) {
            fprintf(stderr, "write() to xillybus failed: %s\n", strerror(errno));
            exit(1);
        }

        donebytes += rc;
    }
}

void FifoReadBlock(coeff_t *buf, int fd) {
    // static int numReads = 0;
    int donebytes, rc;

    donebytes = 0;
    while (donebytes < sizeof(coeff_t) * 3 * kDCTBlockSize) {
        rc = read(fd, buf + donebytes, sizeof(coeff_t) * (3 * kDCTBlockSize) - donebytes);

        if ((rc < 0) && (errno == EINTR))
            continue;

        if (rc < 0) {
            fprintf(stderr, "read() from xillybus failed: %s\n", strerror(errno));
            exit(1);
        }

        if (rc == 0) {
            fprintf(stderr, "Reached read EOF!? Should never happen.\n");
            continue;
        }

        donebytes += rc;
    }
}

}  // namespace guetzli
