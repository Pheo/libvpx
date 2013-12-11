/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_DECODE_TEST_DRIVER_H_
#define TEST_DECODE_TEST_DRIVER_H_
#include <cstring>
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "./vpx_config.h"
#include "vpx/vpx_decoder.h"

namespace libvpx_test {

class CodecFactory;
class CompressedVideoSource;

// Provides an object to handle decoding output
class DxDataIterator {
 public:
  explicit DxDataIterator(vpx_codec_ctx_t *decoder)
      : decoder_(decoder), iter_(NULL) {}

  const vpx_image_t *Next() {
    return vpx_codec_get_frame(decoder_, &iter_);
  }

 private:
  vpx_codec_ctx_t  *decoder_;
  vpx_codec_iter_t  iter_;
};

// Provides a simplified interface to manage one video decoding.
// Similar to Encoder class, the exact services should be added
// as more tests are added.
class Decoder {
 public:
  Decoder(vpx_codec_dec_cfg_t cfg, unsigned long deadline)
      : cfg_(cfg), deadline_(deadline), init_done_(false) {
    memset(&decoder_, 0, sizeof(decoder_));
  }

  virtual ~Decoder() {
    vpx_codec_destroy(&decoder_);
  }

  vpx_codec_err_t DecodeFrame(const uint8_t *cxdata, int size);

  DxDataIterator GetDxData() {
    return DxDataIterator(&decoder_);
  }

  void set_deadline(unsigned long deadline) {
    deadline_ = deadline;
  }

  void Control(int ctrl_id, int arg) {
    InitOnce();
    const vpx_codec_err_t res = vpx_codec_control_(&decoder_, ctrl_id, arg);
    ASSERT_EQ(VPX_CODEC_OK, res) << DecodeError();
  }

  void Control(int ctrl_id, const void *arg) {
    InitOnce();
    const vpx_codec_err_t res = vpx_codec_control_(&decoder_, ctrl_id, arg);
    ASSERT_EQ(VPX_CODEC_OK, res) << DecodeError();
  }

  const char* DecodeError() {
    const char *detail = vpx_codec_error_detail(&decoder_);
    return detail ? detail : vpx_codec_error(&decoder_);
  }

  // Passes the external frame buffer information to libvpx.
  vpx_codec_err_t SetExternalFrameBuffers(
      vpx_codec_frame_buffer_t *fb_list, int fb_count,
      vpx_realloc_frame_buffer_cb_fn_t cb, void *user_priv) {
    InitOnce();
    return vpx_codec_set_frame_buffers(&decoder_,
                                       fb_list, fb_count,
                                       cb, user_priv);
  }

 protected:
  virtual const vpx_codec_iface_t* CodecInterface() const = 0;

  void InitOnce() {
    if (!init_done_) {
      const vpx_codec_err_t res = vpx_codec_dec_init(&decoder_,
                                                     CodecInterface(),
                                                     &cfg_, 0);
      ASSERT_EQ(VPX_CODEC_OK, res) << DecodeError();
      init_done_ = true;
    }
  }

  vpx_codec_ctx_t     decoder_;
  vpx_codec_dec_cfg_t cfg_;
  unsigned int        deadline_;
  bool                init_done_;
};

// Common test functionality for all Decoder tests.
class DecoderTest {
 public:
  // Main decoding loop
  virtual void RunLoop(CompressedVideoSource *video);

  // Hook to be called before decompressing every frame.
  virtual void PreDecodeFrameHook(const CompressedVideoSource& video,
                                  Decoder *decoder) {}

  // Hook to be called on every decompressed frame.
  virtual void DecompressedFrameHook(const vpx_image_t& img,
                                     const unsigned int frame_number) {}

 protected:
  explicit DecoderTest(const CodecFactory *codec) : codec_(codec) {}

  virtual ~DecoderTest() {}

  const CodecFactory *codec_;
};

}  // namespace libvpx_test

#endif  // TEST_DECODE_TEST_DRIVER_H_
