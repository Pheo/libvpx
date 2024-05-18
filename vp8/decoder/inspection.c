#include "../../vp8/decoder/inspection.h"
#include "../../vpx_ports/mem.h"

void ifd_init(insp_frame_data *fd, int frame_width, int frame_height) {
  // TODO: equivalent of int mi_cols = ALIGN_POWER_OF_TWO(frame_width, 3) >> MI_SIZE_LOG2;
  int mi_cols = frame_width;
  int mi_rows = frame_height;

  fd->mi_cols = mi_cols;
  fd->mi_rows = mi_rows;
}

void ifd_clear(insp_frame_data *fd) {
  // TODO
}

int ifd_inspect(insp_frame_data *fd, void *decoder) {
  // TODO
  return 1;
}
