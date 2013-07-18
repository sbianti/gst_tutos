#include <string.h>
#include <stdio.h>
#include <gst/gst.h>

#if GST_VERSION_MAJOR == 0
#define PLAYBIN "playbin2"
#else
#define PLAYBIN "playbin"
#endif

#define DEFAULT_URI "http://docs.gstreamer.com/media/sintel_trailer-480p.webm"
#define BAND_NB 3

typedef struct _CustomData {
  GstElement *equalizer;
  GMainLoop *loop;
} CustomData;

struct band_st {
  gchar *name;
  gchar *caracteristic;
};

static struct band_st band[BAND_NB] = {{"band0", "20Hz-100Hz"},
				       {"band1", "101Hz-1100Hz"},
				       {"band2", "1101Hz-11kHz"}};

static void print_current_values(CustomData *data)
{
  int i;
  gdouble val;

  g_print("\t Intervalle par bande [-24db +12db]\n");
  for (i=0; i < BAND_NB; i++) {
    g_object_get(G_OBJECT(data->equalizer), band[i].name, &val, NULL);
    g_print("%s [%s]: %3ddB   ", band[i].name, band[i].caracteristic, (int)val);
  }

  g_print("\n");
}

static gboolean handle_keyboard(GIOChannel *src, GIOCondition cond,
				CustomData *data)
{
  gchar *str = NULL;

  if (g_io_channel_read_line(src, &str, NULL, NULL, NULL) != G_IO_STATUS_NORMAL)
    return TRUE;

  switch (str[0]) {
  case '0':
  case '1':
  case '2':
    g_object_set(G_OBJECT(data->equalizer), band[str[0] - '0'].name,
		 (gdouble) atoi(str+1), NULL);
    break;
  case 'q':
    g_main_loop_quit(data->loop);
  }

  g_free(str);

  print_current_values(data);

  return TRUE;
}

int main(int argc, char *argv[])
{
  GstElement *pipeline, *bin, *equalizer, *convert, *sink;
  GstPad *pad, *ghost_pad;
  char *pipeline_str;
  GIOChannel *io_stdin = g_io_channel_unix_new(fileno(stdin));
  CustomData data;
  GstStateChangeReturn ret;

  memset(&data, 0, sizeof(data));

  if (argc > 1) {
    if (g_str_has_prefix(argv[1], "http://") ||
	g_str_has_prefix(argv[1], "ftp://"))
      pipeline_str = g_strdup_printf("%s uri=\"%s\"", PLAYBIN, argv[1]);
    else if (argv[1][0] == '~')
      pipeline_str = g_strdup_printf("%s uri=\"file://%s%s\"", PLAYBIN,
				     g_get_home_dir(), argv[1]+1);
    else if (g_file_test(argv[1], G_FILE_TEST_IS_REGULAR))
      pipeline_str = g_strdup_printf("playbin uri=\"file://%s\"", argv[1]);
    else
      pipeline_str = g_strdup_printf("%s uri=%s", PLAYBIN, DEFAULT_URI);
  } else
    pipeline_str = g_strdup_printf("%s uri=%s", PLAYBIN, DEFAULT_URI);

  gst_init(&argc, &argv);

  g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, &data);

  pipeline = gst_parse_launch(pipeline_str, NULL);

  equalizer = gst_element_factory_make("equalizer-3bands", "equalizer");
  convert = gst_element_factory_make("audioconvert", "convert");
  sink = gst_element_factory_make("autoaudiosink", "audio_sink");
  if (!equalizer || !convert || !sink) {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  data.equalizer = equalizer;

  bin = gst_bin_new("audio_sink_bin");
  gst_bin_add_many(GST_BIN(bin), equalizer, convert, sink, NULL);
  gst_element_link_many(equalizer, convert, sink, NULL);
  pad = gst_element_get_static_pad(equalizer, "sink");
  ghost_pad = gst_ghost_pad_new("sink", pad);
  gst_pad_set_active(ghost_pad, TRUE);
  gst_element_add_pad(bin, ghost_pad);
  gst_object_unref(pad);

  g_object_set(GST_OBJECT(pipeline), "audio-sink", bin, NULL);

  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  print_current_values(&data);

  data.loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(data.loop);

  g_io_channel_unref(io_stdin);

  gst_element_set_state(pipeline, GST_STATE_NULL);

  gst_object_unref(pipeline);

  return 0;
}
