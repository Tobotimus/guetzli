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

#ifndef GUETZLI_OUTPUT_IMAGE_H_
#define GUETZLI_OUTPUT_IMAGE_H_

#include <stdint.h>
#include <vector>

#include "guetzli/jpeg_data.h"

namespace guetzli {

class OutputImageComponent {
 public:
  OutputImageComponent(int w, int h);

  void Reset(int factor_x, int factor_y);

  int width() const { return width_; }
  int height() const { return height_; }
  int factor_x() const { return factor_x_; }
  int factor_y() const { return factor_y_; }
  int width_in_blocks() const { return width_in_blocks_; }
  int height_in_blocks() const { return height_in_blocks_; }
  const coeff_t* coeffs() const { return &coeffs_[0]; }
  const int* quant() const { return &quant_[0]; }
  bool IsAllZero() const;

  // Fills in block[] with the 8x8 coefficient block with block coordinates
  // (block_x, block_y).
  // NOTE: If the component is 2x2 subsampled, this corresponds to the 16x16
  // pixel area with upper-left corner (16 * block_x, 16 * block_y).
  void GetCoeffBlock(int block_x, int block_y,
                     coeff_t block[kDCTBlockSize]) const;

  // Fills in out[] array with the 8-bit pixel view of this component cropped
  // to the specified window. The window's upper-left corner, (xmin, ymin) must
  // be within the image, but the window may extend past the image. In that
  // case the edge pixels are duplicated.
  void ToPixels(int xmin, int ymin, int xsize, int ysize,
                uint8_t* out, int stride) const;

  // Fills in out[] array with the floating-point precision pixel view of the
  // component.
  // REQUIRES: factor_x() == 1 and factor_y() == 1.
  void ToFloatPixels(float* out, int stride) const;

  // Sets the 8x8 coefficient block with block coordinates (block_x, block_y)
  // to block[].
  // NOTE: If the component is 2x2 subsampled, this corresponds to the 16x16
  // pixel area with upper-left corner (16 * block_x, 16 * block_y).
  // REQUIRES: block[k] % quant()[k] == 0 for each coefficient index k.
  void SetCoeffBlock(int block_x, int block_y,
                     const coeff_t block[kDCTBlockSize]);

  // Requires that comp is not downsampled.
  void CopyFromJpegComponent(const JPEGComponent& comp,
                             int factor_x, int factor_y,
                             const int* quant);

 private:
  void UpdatePixelsForBlock(int block_x, int block_y,
                            const uint8_t idct[kDCTBlockSize]);

  const int width_;
  const int height_;
  int factor_x_;
  int factor_y_;
  int width_in_blocks_;
  int height_in_blocks_;
  int num_blocks_;
  std::vector<coeff_t> coeffs_;
  std::vector<uint16_t> pixels_;
  // default is all 1s.
  int quant_[kDCTBlockSize];
};

class OutputImage {
 public:
  OutputImage(int w, int h);

  int width() const { return width_; }
  int height() const { return height_; }

  OutputImageComponent& component(int c) { return components_[c]; }
  const OutputImageComponent& component(int c) const { return components_[c]; }

  // Requires that jpg is in YUV444 format.
  void CopyFromJpegData(const JPEGData& jpg);

  void SaveToJpegData(JPEGData* jpg) const;

  std::vector<uint8_t> ToSRGB() const;

  std::vector<uint8_t> ToSRGB(int xmin, int ymin, int xsize, int ysize) const;

  void ToLinearRGB(std::vector<std::vector<float> >* rgb) const;

  void ToLinearRGB(int xmin, int ymin, int xsize, int ysize,
                   std::vector<std::vector<float> >* rgb) const;

  std::string FrameTypeStr() const;

 private:
  const int width_;
  const int height_;
  std::vector<OutputImageComponent> components_;
};

}  // namespace guetzli

#endif  // GUETZLI_OUTPUT_IMAGE_H_
