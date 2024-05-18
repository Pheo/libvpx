typedef struct insp_frame_data insp_frame_data;

struct insp_frame_data {
  int show_frame;
  // TODO: insp_mi_data *mi_grid;
  int mi_cols;
  int mi_rows;
};

void ifd_init(insp_frame_data *fd, int frame_width, int frame_height);
void ifd_clear(insp_frame_data *fd);
int ifd_inspect(insp_frame_data *fd, void *decoder);
