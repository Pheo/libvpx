// VP8 Bitstream Analyzer
// Compatible with AOMAnalyzer
// ==============

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../vpx/vpx_decoder.h"

#include "../tools_common.h"
#include "../video_common.h"; // VpxVideoInfo
#include "../video_reader.h"
#include "vpx_config.h"

#include "../vp8/decoder/inspection.h"
#include "../vpx_mem/vpx_mem.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

// Max JSON buffer size // TODO: identify if there are any cases where this is insufficient?
const int MAX_BUFFER = 1024 * 1024 * 32;

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s ivf_file\n", exec_name);
  exit(EXIT_FAILURE);
}

/******************/
/*  buffer puts   */
/******************/

// Apparently snprintf is too slow?
int put_str(char *buffer, const char *str) {
  int i;
  for (i = 0; str[i] != '\0'; i++) {
    buffer[i] = str[i];
  }
  return i;
}

/*********************/
/*       setup       */
/*********************/

// TODO: These are set globally (esp frame-by-frame state), have these clearly explained
insp_frame_data frame_data;
int frame_count = 0;
int decoded_frame_count = 0;
vpx_codec_ctx_t codec;
VpxVideoReader *reader = NULL;
const VpxInterface *decoder = NULL;
const VpxVideoInfo *info = NULL;

EMSCRIPTEN_KEEPALIVE
int open_file(char *file) {
  if (file == NULL) {
    // TODO: default behaviour
    file = "/tmp/input.ivf";
  }

  reader = vpx_video_reader_open(file);
  if (!reader) die("Failed to open %s for reading.", file);

  // Identification of format (VP8 vs VP9)? Could we make it unified?
  info = vpx_video_reader_get_info(reader);

  decoder = get_vpx_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) die ("Unknown input codec.");

  fprintf(stderr, "Using %s\n",
      vpx_codec_iface_name(decoder->codec_interface()));
  if (vpx_codec_dec_init(&codec, decoder->codec_interface(), NULL, 0))
    die_codec(&codec, "Failed to initialize decoder.");

  // Initialize ifd (note: so its global?)
  ifd_init(&frame_data, info->frame_width, info->frame_height);
  // TODO: ifd_init_cb() equivalent of vpx_codec_control(&codec, VP8_SET_INSPECTION_CALLBACK?, &ii)
  return EXIT_SUCCESS;
};

/*********************/
/*    inspection     */
/*********************/
// TODO: declare at top?
int inspect();
void on_frame_decoded_dump(char *json);

const unsigned char *frame;
const unsigned char *end_frame;
size_t frame_size = 0;
int have_frame = 0;

// TODO: make single read_frame()?
EMSCRIPTEN_KEEPALIVE
void read_frames() {
  // TODO: skip .show_existing frames as they are no-op?
  // NOTE: actually, maybe we shouldn't skip, and send back to analyzer. We can hide/collapse client side with a flag
  while (vpx_video_reader_read_frame(reader)) {
    vpx_codec_iter_t iter = NULL;
    vpx_image_t *img = NULL;

    frame = vpx_video_reader_get_frame(reader, &frame_size);
    if (vpx_codec_decode(&codec, frame, (unsigned int)frame_size, NULL, 0))
      die_codec(&codec, "Failed to decode frame.");

    while ((img = vpx_codec_get_frame(&codec, &iter)) != NULL) {
      // vpx_img_write(img, outfile);
      // TODO: note that frame_data here is global..
      inspect();
      ++frame_count;
    }
  }
}

void on_frame_decoded_dump(char *json) {
  // TODO: EM_ASM_({ Module.on_frame_decoded_json($0); }, json);
  printf("%s", json);
}

// TODO: pass frame_data + decoder as args instead of pulling from global
int inspect() {
  // fetch frame data (global)
  ifd_inspect(&frame_data, &decoder);

  // JSON string
  char *buffer = vpx_malloc(MAX_BUFFER);
  char *buf = buffer;
  buf += put_str(buf, "{\n");
  buf += snprintf(buf, MAX_BUFFER, " \"mi_cols\": %d,\n", frame_data.mi_cols);
  buf += snprintf(buf, MAX_BUFFER, " \"mi_rows\": %d,\n", frame_data.mi_rows);

  decoded_frame_count++; // TODO: actually decode...
  buf += put_str(buf, "},\n");
  *(buf++) = 0; // null string terminator
  on_frame_decoded_dump(buffer);
  vpx_free(buffer);
  return 1;
}

EMSCRIPTEN_KEEPALIVE
void quit() {
  if (vpx_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
  vpx_video_reader_close(reader);
}

EMSCRIPTEN_KEEPALIVE
int main(int argc, char **argv) {
  exec_name = argv[0];
  if (argc <= 1) {
    usage_exit();
  }

  open_file(argv[1]);
  printf("[\n");
  read_frames();

  printf("null\n");
  printf("]");

  quit(); // NOTE: This should be called externally (ie. EMSCRIPTEN_KEEPALIVE)
}
