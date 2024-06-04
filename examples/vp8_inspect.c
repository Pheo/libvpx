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

/****************************/
/* AV1 inspect.c signatures */
/****************************/
/*
int open_file(char *file);
int read_frame(void);
const char *get_aom_codec_build_config(void);
int get_bit_depth(void);
int get_bits_per_sample(void);
int get_image_format(void);
unsigned char *get_plane(int plane);
int get_plane_stride(int plane);
int get_plane_width(int plane);
int get_plane_height(int plane);
int get_frame_width(void);
int get_Frame_height(void);
int main(int argc, char **argv);
void quit(void);
void set_layers(LayerType v);
void set_compress(int v);
*/

/****************************/
/* vp9_inspect.c signatures */
/****************************/
/*
int open_file(char *file);
int read_frame();
const char *get_codec_build_config();
int get_bit_depth();
int get_bits_per_sample();
int get_image_format();
unsigned char *get_plane(int plane);
int get_plane_stride(int plane);
int get_plane_width(int plane);
int get_plane_height(int plane);
int get_frame_width();
int get_frame_height();
int main(int argc, char **argv);
void read_frames();
void quit();
void set_layers(LayerType v);
void set_compress(int v);
*/

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
const VpxVideoInfo *info = NULL; // note used by get_frame_width, get_frame_height

// NOTE: in worker.ts, native.FS.writeFile('/tmp/input.ivf', buffer, { encoding: 'binary' } writes to somewhere..
// before native._open_file() works on it!
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

void on_frame_decoded_dump(char *json) {
  // TODO: EM_ASM_({ Module.on_frame_decoded_json($0); }, json);
  printf("%s", json);
}

// NOTE: i think for vp8 & vp9, we should be able to skip ifd_init_cb
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

vpx_image_t *img = NULL; // TODO: so this is the big state?
EMSCRIPTEN_KEEPALIVE
int step_frame() {
  // TODO: shouldn't we make frame_size scoped in here???
  // NOTE: vp8 doesn't have show_existing_frame, but in case of VP9/AV1, we might want to not skip, and let client collapse
  if (!vpx_video_reader_read_frame(reader)) return EXIT_FAILURE; // end?
  frame = vpx_video_reader_get_frame(reader, &frame_size);

  if (vpx_codec_decode(&codec, frame, (unsigned int)frame_size, NULL, 0))
    die_codec(&codec, "Failed to decode frame."); // TODO: check if this die_codec aborts?

  vpx_codec_iter_t iter = NULL;

  // TODO: why does vp9_inspect.c use vpx_codec_control instead of vpx_codec_get_frame?
  // NOTE: vpx_codec_get_frame can return multiple frames, but not in vp8 right? i assume this is for superframes
  int frames_read = 0;
  while ((img = vpx_codec_get_frame(&codec, &iter)) != NULL) {
    // TODO: note that frame_data here is global..
    // TODO: implement inspect();, must have passed to frame_data?
    ++frames_read;
  }

  if (frames_read != 1) {
    fprintf(stderr, "expected exactly 1 frame? but found %d\n", frames_read);
    return EXIT_FAILURE;
  }

  frame_count += frames_read; // will only be +=1
  return EXIT_SUCCESS;
}

EMSCRIPTEN_KEEPALIVE
int get_bit_depth() { return img->bit_depth; }

EMSCRIPTEN_KEEPALIVE
int get_bits_per_sample() { return img->bps; }

EMSCRIPTEN_KEEPALIVE
int get_image_format() { return img->fmt; }

EMSCRIPTEN_KEEPALIVE
unsigned char *get_plane(int plane) { return img->planes[plane]; }

EMSCRIPTEN_KEEPALIVE
int get_plane_stride(int plane) { return img->stride[plane]; }

EMSCRIPTEN_KEEPALIVE
int get_plane_width(int plane) { return vpx_img_plane_width(img, plane); }

EMSCRIPTEN_KEEPALIVE
int get_plane_height(int plane) { return vpx_img_plane_height(img, plane); }

EMSCRIPTEN_KEEPALIVE
int get_frame_width() { return info->frame_width; }

EMSCRIPTEN_KEEPALIVE
int get_frame_height() { return info->frame_height; }

EMSCRIPTEN_KEEPALIVE
void quit() {
  if (vpx_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
  vpx_video_reader_close(reader);
}

// Main currently use for development. Keep in mind that inspector is stateful, and driven from aomanalyzer
EMSCRIPTEN_KEEPALIVE
int main(int argc, char **argv) {
  exec_name = argv[0];
  if (argc <= 1) {
    usage_exit();
  }

  // STEP 1: open .ivf, initializes frame_data
  open_file(argv[1]);

  printf("[\n");

  // STEP 2: read frame, should be only 1 per call in vp8?
  while (!step_frame()) {
    // STEP 3: inspect frame, prints out JSON
    // inspect() // TODO

    // STEP 4: scan img->planes, returns vpx_image
    // get_plane(42) TODO
  }

  printf("null\n");
  printf("]");

  quit(); // NOTE: This should be called externally (ie. EMSCRIPTEN_KEEPALIVE)
}

// void set_layers(LayerType v) { layers = v; } // TODO
// void set_compress(int v) { compress = v; }
