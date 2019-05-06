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

#include "guetzli/jpeg_data_encoder.h"

#include <algorithm>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "guetzli/hwdct.h"
#include "guetzli/fdct.h"

namespace guetzli {

namespace {

static const int kIQuantBits = 16;
// Output of the DCT is upscaled by 16.
static const int kDCTBits = kIQuantBits + 4;
static const int kBias = 0x80 << (kDCTBits - 8);

void Quantize(coeff_t* v, int iquant) {
  *v = (*v * iquant + kBias) >> kDCTBits;
}

// Single pixel rgb to 16-bit yuv conversion.
// The returned yuv values are signed integers in the
// range [-128, 127] inclusive.
inline static void RGBToYUV16(const uint8_t* const rgb,
                              coeff_t *out) {
  enum { FRAC = 16, HALF = 1 << (FRAC - 1) };
  const int r = rgb[0];
  const int g = rgb[1];
  const int b = rgb[2];
  out[0] = (19595 * r  + 38469 * g +  7471 * b - (128 << 16) + HALF) >> FRAC;
  out[64] = (-11059 * r - 21709 * g + 32768 * b + HALF - 1) >> FRAC;
  out[128] = (32768 * r  - 27439 * g -  5329 * b + HALF - 1) >> FRAC;
}

}  // namespace

void AddApp0Data(JPEGData* jpg) {
  const unsigned char kApp0Data[] = {
      0xe0, 0x00, 0x10,              // APP0
      0x4a, 0x46, 0x49, 0x46, 0x00,  // 'JFIF'
      0x01, 0x01,                    // v1.01
      0x00, 0x00, 0x01, 0x00, 0x01,  // aspect ratio = 1:1
      0x00, 0x00                     // thumbnail width/height
  };
  jpg->app_data.push_back(
      std::string(reinterpret_cast<const char*>(kApp0Data),
                                 sizeof(kApp0Data)));
}

bool EncodeRGBToJpeg(const std::vector<uint8_t>& rgb, int w, int h,
                     const int* quant, JPEGData* jpg) {
  if (w < 0 || w >= 1 << 16 || h < 0 || h >= 1 << 16 ||
      rgb.size() != 3 * w * h) {
    return false;
  }
  InitJPEGDataForYUV444(w, h, jpg);
  AddApp0Data(jpg);

  int iquant[3 * kDCTBlockSize];
  int idx = 0;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < kDCTBlockSize; ++j) {
      int v = quant[idx];
      jpg->quant[i].values[j] = v;
      iquant[idx++] = ((1 << kIQuantBits) + 1) / v;
    }
  }

  int block_ix = 0;

#ifdef HLS
  int fdr = open("/dev/xillybus_read_32", O_RDONLY);
  if (fdr < 0) {
    fprintf(stderr, "Failed to open read bus: %s\n", strerror(errno));
    exit(1);
  }
  int fdw = open("/dev/xillybus_write_32", O_WRONLY);
  if (fdw < 0) {
    fprintf(stderr, "Failed to open write bus: %s\n", strerror(errno));
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    fprintf(stderr, "Failed to fork(): %s\n", strerror(errno));
    exit(1);
  }

  if (!pid) {
    // Child process does RGB->YUV and then writes to FIFO for DCT
    close(fdr);
#endif // HLS
    for (int block_y = 0; block_y < jpg->MCU_rows; ++block_y) {
      for (int block_x = 0; block_x < jpg->MCU_cols; ++block_x) {
        coeff_t block[3 * kDCTBlockSize];
        // RGB->YUV transform.
        for (int iy = 0; iy < 8; ++iy) {
          for (int ix = 0; ix < 8; ++ix) {
            int y = std::min(h - 1, 8 * block_y + iy);
            int x = std::min(w - 1, 8 * block_x + ix);
            int p = y * w + x;
            RGBToYUV16(&rgb[3 * p], &block[8 * iy + ix]);
          }
        }
#ifdef HLS
        // Send to FIFO for DCT
        FifoWriteBlock(block, fdw);
#else
        for (int i = 0; i < 3; ++i) {
          ComputeBlockDCT(&block[i * kDCTBlockSize]);
        }
#endif // HLS
#ifdef HLS
      }
    }
    // Sleep until we're terminated, or a minute at max
    close(fdw);
    exit(0);
  }
  else
  {
    // Parent process reads DCT coeffs from FIFO and then does quantization
    close(fdw);
    for (int block_y = 0; block_y < jpg->MCU_rows; ++block_y) {
      for (int block_x = 0; block_x < jpg->MCU_cols; ++block_x) {
        coeff_t block[3 * kDCTBlockSize];
        // Get DCT coeffs from FIFO
        FifoReadBlock(block, fdr);
#endif // HLS
        // Quantization
        for (int i = 0; i < 3 * 64; ++i) {
          Quantize(&block[i], iquant[i]);
        }
        // Copy the resulting coefficients to *jpg.
        for (int i = 0; i < 3; ++i) {
          memcpy(&jpg->components[i].coeffs[block_ix * kDCTBlockSize],
                &block[i * kDCTBlockSize], kDCTBlockSize * sizeof(block[0]));
        }
        ++block_ix;
      }
    }
#ifdef HLS
    close(fdr);
  }
#endif // HLS

  return true;
}

bool EncodeRGBToJpeg(const std::vector<uint8_t>& rgb, int w, int h,
                     JPEGData* jpg) {
  static const int quant[3 * kDCTBlockSize] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  };
  return EncodeRGBToJpeg(rgb, w, h, quant, jpg);
}

}  // namespace guetzli
