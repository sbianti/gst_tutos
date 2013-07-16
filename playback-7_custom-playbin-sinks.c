#include <gst/gst.h>

#if GST_VERSION_MAJOR == 0
#define PLAYBIN "playbin2"
#else
#define PLAYBIN "playbin"
#endif

#define DEFAULT_URI "http://docs.gstreamer.com/media/sintel_trailer-480p.webm"

int main(int argc, char *argv[]) {
  GstElement *pipeline, *bin, *equalizer, *convert, *sink;
  GstPad *pad, *ghost_pad;
  GstBus *bus;
  GstMessage *msg;
  char *pipeline_str;

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

  pipeline = gst_parse_launch(pipeline_str, NULL);

  equalizer = gst_element_factory_make("equalizer-3bands", "equalizer");
  convert = gst_element_factory_make("audioconvert", "convert");
  sink = gst_element_factory_make("autoaudiosink", "audio_sink");
  if (!equalizer || !convert || !sink) {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  bin = gst_bin_new("audio_sink_bin");
  gst_bin_add_many(GST_BIN(bin), equalizer, convert, sink, NULL);
  gst_element_link_many(equalizer, convert, sink, NULL);
  pad = gst_element_get_static_pad(equalizer, "sink");
  ghost_pad = gst_ghost_pad_new("sink", pad);
  gst_pad_set_active(ghost_pad, TRUE);
  gst_element_add_pad(bin, ghost_pad);
  gst_object_unref(pad);

  g_object_set(G_OBJECT(equalizer), "band1", (gdouble)-24.0, NULL);
  g_object_set(G_OBJECT(equalizer), "band2", (gdouble)-24.0, NULL);

  g_object_set(GST_OBJECT(pipeline), "audio-sink", bin, NULL);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  bus = gst_element_get_bus(pipeline);
  msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
				   GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  if (msg != NULL)
    gst_message_unref(msg);

  gst_object_unref(bus);

  gst_element_set_state(pipeline, GST_STATE_NULL);

  gst_object_unref(pipeline);

  return 0;
}
