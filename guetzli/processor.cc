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

#include "guetzli/processor.h"

#include <algorithm>
#include <assert.h>
#include <set>
#include <string.h>
#include <time.h>
#include <vector>

#include "guetzli/debug_print.h"
#include "guetzli/fast_log.h"
#include "guetzli/jpeg_data_decoder.h"
#include "guetzli/jpeg_data_encoder.h"
#include "guetzli/jpeg_data_reader.h"
#include "guetzli/jpeg_data_writer.h"
#include "guetzli/output_image.h"
#include "guetzli/quantize.h"

namespace guetzli {

namespace {

static const size_t kBlockSize = 3 * kDCTBlockSize;

struct CoeffData {
  int idx;
  float block_err;
};
struct QuantData {
  int q[3][kDCTBlockSize];
  size_t jpg_size;
  bool dist_ok;
};
class Processor {
 public:
  bool ProcessJpegData(const Params& params, const JPEGData& jpg_in,
                       GuetzliOutput* out, ProcessStats* stats);

 private:
  void ComputeBlockZeroingOrder(
      const coeff_t block[kBlockSize], const coeff_t orig_block[kBlockSize],
      const int block_x, const int block_y, const int factor_x,
      const int factor_y, const uint8_t comp_mask, OutputImage* img,
      std::vector<CoeffData>* output_order);
  void OutputJpeg(const JPEGData& in, std::string* out);

  Params params_;
  GuetzliOutput* final_output_;
  ProcessStats* stats_;
};

bool CheckJpegSanity(const JPEGData& jpg) {
  const int kMaxComponent = 1 << 12;
  for (const JPEGComponent& comp : jpg.components) {
    const JPEGQuantTable& quant_table = jpg.quant[comp.quant_idx];
    for (int i = 0; i < comp.coeffs.size(); i++) {
      coeff_t coeff = comp.coeffs[i];
      int quant = quant_table.values[i % kDCTBlockSize];
      if (std::abs(static_cast<int64_t>(coeff) * quant) > kMaxComponent) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

int GuetzliStringOut(void* data, const uint8_t* buf, size_t count) {
  std::string* sink =
      reinterpret_cast<std::string*>(data);
  sink->append(reinterpret_cast<const char*>(buf), count);
  return count;
}

void Processor::OutputJpeg(const JPEGData& jpg,
                           std::string* out) {
  out->clear();
  JPEGOutput output(GuetzliStringOut, out);
  if (!WriteJpeg(jpg, params_.clear_metadata, output)) {
    assert(0);
  }
}

bool Processor::ProcessJpegData(const Params& params, const JPEGData& jpg_in,
                                GuetzliOutput* out, ProcessStats* stats) {
  params_ = params;
  final_output_ = out;
  stats_ = stats;
  
  if (jpg_in.components.size() != 3 || !HasYCbCrColorSpace(jpg_in)) {
    fprintf(stderr, "Only YUV color space input jpeg is supported\n");
    return false;
  }
  bool input_is_420;
  if (jpg_in.Is444()) {
    input_is_420 = false;
  } else if (jpg_in.Is420()) {
    input_is_420 = true;
  } else {
    fprintf(stderr, "Unsupported sampling factors:");
    for (size_t i = 0; i < jpg_in.components.size(); ++i) {
      fprintf(stderr, " %dx%d", jpg_in.components[i].h_samp_factor,
              jpg_in.components[i].v_samp_factor);
    }
    fprintf(stderr, "\n");
    return false;
  }
  int q_in[3][kDCTBlockSize];
  // Output the original image, in case we do not manage to create anything
  // with a good enough quality.
  std::string encoded_jpg;
  OutputJpeg(jpg_in, &encoded_jpg);
  final_output_->score = -1;
  GUETZLI_LOG(stats, "Original Out[%7zd]", encoded_jpg.size());
  final_output_->jpeg_data = encoded_jpg;
  final_output_->score = encoded_jpg.size();
  return true;
}

bool ProcessJpegData(const Params& params, const JPEGData& jpg_in,
                     GuetzliOutput* out, ProcessStats* stats) {
  Processor processor;
  return processor.ProcessJpegData(params, jpg_in, out, stats);
}

bool Process(const Params& params, ProcessStats* stats,
             const std::string& data,
             std::string* jpg_out) {
  JPEGData jpg;
  if (!ReadJpeg(data, JPEG_READ_ALL, &jpg)) {
    fprintf(stderr, "Can't read jpg data from input file\n");
    return false;
  }
  if (!CheckJpegSanity(jpg)) {
    fprintf(stderr, "Unsupported input JPEG (unexpectedly large coefficient "
            "values).\n");
    return false;
  }
  std::vector<uint8_t> rgb = DecodeJpegToRGB(jpg);
  if (rgb.empty()) {
    fprintf(stderr, "Unsupported input JPEG file (e.g. unsupported "
            "downsampling mode).\nPlease provide the input image as "
            "a PNG file.\n");
    return false;
  }
  GuetzliOutput out;
  ProcessStats dummy_stats;
  if (stats == nullptr) {
    stats = &dummy_stats;
  }
  bool ok = ProcessJpegData(params, jpg, &out, stats);
  *jpg_out = out.jpeg_data;
  return ok;
}

bool Process(const Params& params, ProcessStats* stats,
             const std::vector<uint8_t>& rgb, int w, int h,
             std::string* jpg_out) {
  JPEGData jpg;

  clock_t start, end;
  start = clock();
  if (!EncodeRGBToJpeg(rgb, w, h, &jpg)) {
    fprintf(stderr, "Could not create jpg data from rgb pixels\n");
    return false;
  }
  end = clock();
  printf("Took %f seconds to encode JPEG\n", ((double) (end - start)) / CLOCKS_PER_SEC);
  GuetzliOutput out;
  ProcessStats dummy_stats;
  if (stats == nullptr) {
    stats = &dummy_stats;
  }
  bool ok = ProcessJpegData(params, jpg, &out, stats);
  *jpg_out = out.jpeg_data;
  return ok;
}

}  // namespace guetzli
