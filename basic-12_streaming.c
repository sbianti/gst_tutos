#include <gst/gst.h>
#include <string.h>
#include <locale.h>

typedef struct _CustomData {
  gboolean is_live;
  GstElement *pipeline;
  GMainLoop *loop;
} CustomData;

static void cb_message(GstBus *bus, GstMessage *msg, CustomData *data) {
  static int buffering_nb = 0;

  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
      GError *err;
      gchar *debug;

      gst_message_parse_error(msg, &err, &debug);
      g_print("Error: %s\n", err->message);
      g_error_free(err);
      g_free(debug);

      gst_element_set_state(data->pipeline, GST_STATE_READY);
      g_main_loop_quit(data->loop);

      g_print("On a bufferisé %d fois\n", buffering_nb);
      break;
    }
    case GST_MESSAGE_EOS:
      gst_element_set_state(data->pipeline, GST_STATE_READY);
      g_main_loop_quit(data->loop);
      break;
    case GST_MESSAGE_BUFFERING: {
      gint percent = 0;

      buffering_nb++;
      if (data->is_live) break;

      gst_message_parse_buffering(msg, &percent);
      g_print("Buffering(%3d%%)\r", percent);

      if (percent < 100)
        gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
      else
        gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
      break;
    }
    case GST_MESSAGE_CLOCK_LOST:
      gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
      gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
      break;
    default:
      break;
    }
}

int main(int argc, char *argv[]) {
  GstElement *pipeline;
  GstBus *bus;
  GstStateChangeReturn ret;
  GMainLoop *main_loop;
  CustomData data;
  char *uri;

#define DEFAULT_URI "http://docs.gstreamer.com/media/sintel_trailer-480p.webm"

  setlocale(LC_ALL, "fr_FR.utf8");

  if (argc > 1) {
    if (g_str_has_prefix(argv[1], "http://") ||
	g_str_has_prefix(argv[1], "ftp://"))
      uri = g_strdup_printf("playbin uri=%s", argv[1]);
    else if (argv[1][0] == '~')
      uri = g_strdup_printf("playbin uri=\"file://%s%s\"",
			    g_get_home_dir(), argv[1]+1);
    else
      if (g_file_test(argv[1], G_FILE_TEST_IS_REGULAR))
	uri = g_strdup_printf("playbin uri=\"file://%s\"", argv[1]);
      else
	uri = NULL;
  } else
    uri = NULL;

  gst_init(&argc, &argv);

  memset(&data, 0, sizeof(data));

  if (uri == NULL)
    uri = g_strdup_printf("playbin uri=%s", DEFAULT_URI);

  pipeline = gst_parse_launch(uri, NULL);
  bus = gst_element_get_bus(pipeline);

  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(pipeline);
    return -1;
  } else if (ret == GST_STATE_CHANGE_NO_PREROLL) {
    data.is_live = TRUE;
  }

  main_loop = g_main_loop_new(NULL, FALSE);
  data.loop = main_loop;
  data.pipeline = pipeline;

  gst_bus_add_signal_watch(bus);
  g_signal_connect(bus, "message", G_CALLBACK(cb_message), &data);

  g_main_loop_run(main_loop);

  g_main_loop_unref(main_loop);
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  return 0;
}
