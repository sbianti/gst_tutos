#include <string.h>
#include <stdio.h>
#include <gst/gst.h>
#include <locale.h>

#if GST_VERSION_MAJOR == 0
#define PLAYBIN "playbin2"
#include <gst/interfaces/colorbalance.h>
#else
#define PLAYBIN "playbin"
#include <gst/video/colorbalance.h>
#endif

#define DEFAULT_URI "http://docs.gstreamer.com/media/sintel_trailer-480p.webm"
#define PERCENTAGE(val, min, max) (100 * (val - min) / (max - min))
#define PERCENT_TO_ABSVAL(percent, min, max) ((percent * (max - min)) / 100)
#define PERCENT_TO_VAL(pct, min, max) (PERCENT_TO_ABSVAL(pct, min, max) + min)

typedef struct _CustomData {
  GstElement *pipeline;
  GMainLoop *loop;
} CustomData;

enum setting_type_t {absolute, relative};

struct color_setting_t {
  enum setting_type_t type;
  gint val;
};

static void update_color_channel(const gchar *channel_name,
				 struct color_setting_t setting,
				 GstColorBalance *cb)
{
  gdouble step;
  gint value;
  GstColorBalanceChannel *channel = NULL;
  const GList *channels, *l;

  channels = gst_color_balance_list_channels(cb);
  for (l = channels; l != NULL; l = l->next) {
    GstColorBalanceChannel *tmp = (GstColorBalanceChannel *)l->data;

    if (g_strrstr(tmp->label, channel_name)) {
      channel = tmp;
      break;
    }
  }

  if (channel == NULL)
    return;

  switch (setting.type) {
  case absolute:
    value = PERCENT_TO_VAL(setting.val, channel->min_value, channel->max_value);
    break;
  case relative:
    value = gst_color_balance_get_value(cb, channel);
    value += PERCENT_TO_ABSVAL(setting.val, channel->min_value,
			       channel->max_value);
  }

  if (value > channel->max_value)
    value = channel->max_value;
  if (value < channel->min_value)
    value = channel->min_value;

  gst_color_balance_set_value(cb, channel, value);
}

static void print_current_values(GstElement *pipeline)
{
  const GList *channels, *l;

  channels = gst_color_balance_list_channels(GST_COLOR_BALANCE(pipeline));
  for (l = channels; l != NULL; l = l->next) {
    GstColorBalanceChannel *channel = (GstColorBalanceChannel *)l->data;
    gint value = gst_color_balance_get_value(GST_COLOR_BALANCE(pipeline),
					     channel);
    g_print("%s: %3d%% ", channel->label,
	    PERCENTAGE(value, channel->min_value, channel->max_value)
	    );
  }

  g_print("\n");
}

static struct color_setting_t parse_param(gchar *str)
{
  struct color_setting_t setting;

  setting.val = atoi(str);

  if (setting.val != 0)
    setting.type = index(str, '%') ? absolute : relative;
  else if (!g_ascii_isdigit(str[0]) && str[0] != '+' && str[0] != '-')
    setting.type = relative;
  else
    setting.type = absolute;

  return setting;
}

static gboolean handle_keyboard(GIOChannel *source, GIOCondition cond,
				CustomData *data)
{
  gchar *str = NULL;
  struct color_setting_t setting;

  if (g_io_channel_read_line(source, &str, NULL, NULL, NULL) !=
      G_IO_STATUS_NORMAL)
    return TRUE;

  /* …read_line puts EOL in str */
  if (strlen(str) < 3) {
    setting.type = relative;
    setting.val = g_ascii_isupper(str[0]) ? 10 : -10;
  } else {
    gchar *str_strip = g_strstrip(str+1);
    setting = parse_param(str_strip);
  }

  switch (g_ascii_tolower(str[0])) {
  case 'c':
    update_color_channel("CONTRAST", setting,
			 GST_COLOR_BALANCE(data->pipeline));
    break;
  case 'b':
    update_color_channel("BRIGHTNESS", setting,
			 GST_COLOR_BALANCE(data->pipeline));
    break;
  case 'h':
    update_color_channel("HUE", setting,
			 GST_COLOR_BALANCE(data->pipeline));
    break;
  case 's':
    update_color_channel("SATURATION", setting,
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

  setlocale(LC_ALL, "fr_FR.utf8");

  gst_init(&argc, &argv);

  memset(&data, 0, sizeof(data));

  g_print(
    "USAGE: Choose one of the following options, then press enter:\n"
    " 'C' to increase contrast of 10%, 'c' to decrease contrast of 10%\n"
    " 'B' to increase brightness of 10%, 'b' to decrease brightness of 10%\n"
    " 'H' to increase hue of 10%, 'h' to decrease hue of 10%\n"
    " 'S' to increase saturation of 10%, 's' to decrease saturation of 10%\n\n"
    " You can also use: « c -60 » which decrease the contrast of 60pts\n"
    " Or: « S 55% » which sets saturation to 60%"
    " 'Q' to quit\n\n");

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
