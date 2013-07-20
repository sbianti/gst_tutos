#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <gst/gst.h>

#if GST_VERSION_MAJOR == 0
#define PLAYBIN "playbin2"
#define GET_PLUGIN_REGISTRY gst_registry_get_default()
#else
#define PLAYBIN "playbin"
#define GET_PLUGIN_REGISTRY gst_registry_get()
#endif

#define DEFAULT_URI "http://docs.gstreamer.com/media/sintel_trailer-480p.webm"

typedef struct _CustomData {
  GstElement *gaudieffect;
  GMainLoop *loop;
} CustomData;

static gboolean handle_keyboard(GIOChannel *src, GIOCondition cond,
				CustomData *data)
{
  gchar *str = NULL;

  if (g_io_channel_read_line(src, &str, NULL, NULL, NULL) != G_IO_STATUS_NORMAL)
    return TRUE;

  switch (str[0]) {
  case 'q':
    g_main_loop_quit(data->loop);
  }

  g_free(str);

  return TRUE;
}

gboolean to_bool(gchar *val)
{
  if (g_strcmp0(g_ascii_strup(val, -1), "TRUE"))
    return TRUE;
  else
    return FALSE;
}

gint prv_g_object_set(GstElement *effect_element, gchar *prop)
{
  gchar **split_str = g_strsplit(prop, ",", 3);
  
  if (split_str == NULL) {
    g_error("bad property syntax\n");
    return -1;
  }

  else if (g_str_has_suffix(split_str[1], "int"))
    g_object_set(effect_element, split_str[0], atoi(split_str[2]), NULL);
  else if (g_str_has_prefix(split_str[1], "bool"))
    g_object_set(effect_element, split_str[0], to_bool(split_str[2]), NULL);

  g_strfreev(split_str);
}

void set_props(GstElement *effect_element, gchar *props_str)
{
  gchar **props = g_strsplit(props_str, ";", 0);
  gint i=0;

  if (props == NULL)
    return;

  while (props[i] != NULL) {
    g_print("prop %d : «%s»\n", i, props[i]);
    prv_g_object_set(effect_element, props[i]);
    i++;
  }

  g_strfreev(props);
}

gint list_gaudieffects_features()
{
  GList *list, *walk;

  g_print("Available gaudieffects features :\n");
  list = gst_registry_get_feature_list_by_plugin(GET_PLUGIN_REGISTRY, "gaudieffects");
  for (walk = list; walk != NULL; walk = g_list_next(walk))
    g_print("feature: <%s>\n",
	    gst_plugin_feature_get_name((GstPluginFeature *)walk->data));

  gst_plugin_feature_list_free(list);

  return 0;
}

int main(int argc, char *argv[])
{
  GstElement *pipeline, *bin, *effect_element, *convert, *sink;
  GstPad *pad, *ghost_pad;
  char *pipeline_str;
  GIOChannel *io_stdin = g_io_channel_unix_new(fileno(stdin));
  CustomData data;
  GstStateChangeReturn ret;
  gboolean list_effects = FALSE;
  gchar *effect_name = NULL;
  GError *error = NULL;
  GstPlugin *gaudiplugin;
  gchar *props_str = NULL;
  GOptionContext *context;
  GOptionEntry options[] = {
    { "list-effects", 'l', 0, G_OPTION_ARG_NONE, &list_effects,
      "list available effects and exits", NULL },
    { "effect", 'e', 0, G_OPTION_ARG_STRING, &effect_name,
      "set the desired effect", NULL },
    { "props", 'p', 0, G_OPTION_ARG_STRING, &props_str,
      "for property setting (-p \"silent,bool,true;adjustement,uint,150\")",
      NULL },
    { NULL }
  };

  setlocale(LC_ALL, "fr_FR.utf8");

  gst_init(&argc, &argv);

  gaudiplugin = gst_registry_find_plugin(GET_PLUGIN_REGISTRY, "gaudieffects");
  if (gaudiplugin == NULL) {
    g_print("Pas de plugin “gaudieffects” trouvé !! :(\n");
    return -1;
  }

  context = g_option_context_new("");
  g_option_context_add_main_entries(context, options, "");
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    g_print("option parsing failed: %s\n", error->message);
    return -1;
  }
  g_option_context_free(context);

  if (list_effects == TRUE)
    return  list_gaudieffects_features();

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

  g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, &data);

  pipeline = gst_parse_launch(pipeline_str, NULL);

  if (gst_plugin_is_loaded(gaudiplugin) == FALSE)
    gst_plugin_load(gaudiplugin);

  if (effect_name == NULL)
    effect_name = "solarize";

  effect_element = gst_element_factory_make(effect_name, effect_name);
  convert = gst_element_factory_make("videoconvert", "convert");
  sink = gst_element_factory_make("autovideosink", "video_sink");
  if (!effect_element || !convert || !sink) {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  bin = gst_bin_new("video_sink_bin");
  gst_bin_add_many(GST_BIN(bin), effect_element, convert, sink, NULL);
  gst_element_link_many(effect_element, convert, sink, NULL);
  pad = gst_element_get_static_pad(effect_element, "sink");
  ghost_pad = gst_ghost_pad_new("sink", pad);
  gst_pad_set_active(ghost_pad, TRUE);
  gst_element_add_pad(bin, ghost_pad);
  gst_object_unref(pad);

  g_object_set(GST_OBJECT(pipeline), "video-sink", bin, NULL);

  if (props_str != NULL)
    set_props(effect_element, props_str);

  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  data.loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(data.loop);

  g_io_channel_unref(io_stdin);

  gst_element_set_state(pipeline, GST_STATE_NULL);

  gst_object_unref(pipeline);

  return 0;
}
