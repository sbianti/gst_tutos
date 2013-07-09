#include <string.h>
#include <stdio.h>
#include <gst/gst.h>

#if GST_VERSION_MAJOR == 0
#define PLAYBIN "playbin2"
#include <gst/interfaces/colorbalance.h>
#else
#define PLAYBIN "playbin"
#include <gst/video/colorbalance.h>
#endif

#define DEFAULT_URI "http://docs.gstreamer.com/media/sintel_trailer-480p.webm"

typedef struct _CustomData {
  GstElement *pipeline;
  GMainLoop *loop;
} CustomData;

/* Process a color balance command */
static void update_color_channel(const gchar *channel_name,
				 gboolean increase,
				 GstColorBalance *cb) {
  gdouble step;
  gint value;
  GstColorBalanceChannel *channel = NULL;
  const GList *channels, *l;

  /* Retrieve the list of channels and locate the requested one */
  channels = gst_color_balance_list_channels(cb);
  for (l = channels; l != NULL; l = l->next) {
    GstColorBalanceChannel *tmp = (GstColorBalanceChannel *)l->data;

    if (g_strrstr(tmp->label, channel_name)) {
      channel = tmp;
      break;
    }
  }
  if (!channel)
    return;

  /* Change the channel's value */
  step = 0.1 * (channel->max_value - channel->min_value);
  value = gst_color_balance_get_value(cb, channel);
  if (increase) {
    value = (gint)(value + step);
    if (value > channel->max_value)
      value = channel->max_value;
  } else {
    value = (gint)(value - step);
    if (value < channel->min_value)
      value = channel->min_value;
  }
  gst_color_balance_set_value(cb, channel, value);
}

#define PERCENTAGE(val, min, max) (100 * (val - min) / (max - min))

static void print_current_values(GstElement *pipeline) {
  const GList *channels, *l;

  channels = gst_color_balance_list_channels(GST_COLOR_BALANCE(pipeline));
  for (l = channels; l != NULL; l = l->next) {
    GstColorBalanceChannel *channel = (GstColorBalanceChannel *)l->data;
    gint value = gst_color_balance_get_value(GST_COLOR_BALANCE(pipeline),
					     channel);
    g_print("%s: %3d%% ",
	    channel->label,
	    PERCENTAGE(value, channel->min_value, channel->max_value)
	    );
  }
  g_print("\n");
}

static gboolean handle_keyboard(GIOChannel *source, GIOCondition cond,
				CustomData *data) {
  gchar *str = NULL;

  if (g_io_channel_read_line(source, &str, NULL, NULL, NULL) !=
      G_IO_STATUS_NORMAL)
    return TRUE;

  switch (g_ascii_tolower(str[0])) {
  case 'c':
    update_color_channel("CONTRAST", g_ascii_isupper(str[0]),
			 GST_COLOR_BALANCE(data->pipeline));
    break;
  case 'b':
    update_color_channel("BRIGHTNESS", g_ascii_isupper(str[0]),
			 GST_COLOR_BALANCE(data->pipeline));
    break;
  case 'h':
    update_color_channel("HUE", g_ascii_isupper(str[0]),
			 GST_COLOR_BALANCE(data->pipeline));
    break;
  case 's':
    update_color_channel("SATURATION", g_ascii_isupper(str[0]),
			 GST_COLOR_BALANCE(data->pipeline));
    break;
  case 'q':
    g_main_loop_quit(data->loop);
    break;
  default:
    break;
  }

  g_free(str);

  print_current_values(data->pipeline);

  return TRUE;
}

int main(int argc, char *argv[]) {
  CustomData data;
  GstStateChangeReturn ret;
  GIOChannel *io_stdin;
  char *pipeline_str;

  gst_init(&argc, &argv);

  memset(&data, 0, sizeof(data));

  g_print(
    "USAGE: Choose one of the following options, then press enter:\n"
    " 'C' to increase contrast, 'c' to decrease contrast\n"
    " 'B' to increase brightness, 'b' to decrease brightness\n"
    " 'H' to increase hue, 'h' to decrease hue\n"
    " 'S' to increase saturation, 's' to decrease saturation\n"
    " 'Q' to quit\n");

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

  data.pipeline = gst_parse_launch(pipeline_str, NULL);

#ifdef _WIN32
  io_stdin = g_io_channel_win32_new_fd(fileno(stdin));
#else
  io_stdin = g_io_channel_unix_new(fileno(stdin));
#endif
  g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, &data);

  ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }
  print_current_values(data.pipeline);

  data.loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(data.loop);

  g_main_loop_unref(data.loop);
  g_io_channel_unref(io_stdin);
  gst_element_set_state(data.pipeline, GST_STATE_NULL);
  gst_object_unref(data.pipeline);
  return 0;
}
