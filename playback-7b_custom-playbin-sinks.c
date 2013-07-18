#include <string.h>
#include <stdio.h>
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

static gboolean filter_gaudi_features(GstPluginFeature *feature, gpointer gp) {
  GstElementFactory *factory;

  if (!GST_IS_ELEMENT_FACTORY(feature)) {
    gchar *name;

    g_object_get(feature, "name", &name, NULL);
    g_print("ce n'est pas une factory: <%s> \n", name);
    return FALSE;
  }

  factory = GST_ELEMENT_FACTORY(feature);
  if (!g_strrstr(gst_element_factory_get_klass(factory), "gaudieffects")) {
    g_print("klass: <%s>\n", gst_element_factory_get_klass(factory));
    return FALSE;
  }

  return TRUE;
}

static void print_current_values(CustomData *data)
{
  int i;
  gdouble val;

  g_print("\n");
}

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

  print_current_values(data);

  return TRUE;
}

int main(int argc, char *argv[])
{
  GstElement *pipeline, *bin, *gaudieffect, *convert, *sink;
  GstPad *pad, *ghost_pad;
  char *pipeline_str;
  GIOChannel *io_stdin = g_io_channel_unix_new(fileno(stdin));
  CustomData data;
  GstStateChangeReturn ret;
  GList *list, *walk;
  GstElementFactory *selected_factory = NULL;
  gboolean list_plugins = FALSE;
  gchar *plugin_name = NULL;
  GError *error = NULL;
  GOptionContext *context;
  GOptionEntry options[] = {
    { "list-plugins", 'l', 0, G_OPTION_ARG_NONE, &list_plugins,
      "list available plugins and exits", NULL },
    { "plugin", 'p', 0, G_OPTION_ARG_STRING, &plugin_name,
      "set the desired plugin", NULL },
    { NULL }
  };

  context = g_option_context_new("");
  g_option_context_add_main_entries(context, options, "");
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    g_print("option parsing failed: %s\n", error->message);
    return -1;
  }
  g_option_context_free(context);

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

  list = gst_registry_feature_filter(GET_PLUGIN_REGISTRY,
				     filter_gaudi_features, FALSE, NULL);

  if (plugin_name == NULL)
    plugin_name = "solarize";

  g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, &data);

  pipeline = gst_parse_launch(pipeline_str, NULL);

  g_print("Available gaudieffects plugins:\n");
  for (walk = list; walk != NULL; walk = g_list_next(walk)) {
    const gchar *name;
    GstElementFactory *factory;

    factory = GST_ELEMENT_FACTORY(walk->data);
    name = gst_element_factory_get_longname(factory);
    g_print("  %s\n", name);

    if (selected_factory == NULL || g_str_has_prefix(name, plugin_name)) {
      selected_factory = factory;
    }
  }

  if (!selected_factory) {
    g_print("No visualization plugins found!\n");
    return -1;
  }

  if (list_plugins == TRUE)
    return 0;

  g_print("Selected '%s'\n",
	  gst_element_factory_get_longname(selected_factory));

  gaudieffect = gst_element_factory_create(selected_factory, NULL);
  convert = gst_element_factory_make("videoconvert", "convert");
  sink = gst_element_factory_make("autovideosink", "video_sink");
  if (!gaudieffect || !convert || !sink) {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  data.gaudieffect = gaudieffect;

  bin = gst_bin_new("video_sink_bin");
  gst_bin_add_many(GST_BIN(bin), gaudieffect, convert, sink, NULL);
  gst_element_link_many(gaudieffect, convert, sink, NULL);
  pad = gst_element_get_static_pad(gaudieffect, "sink");
  ghost_pad = gst_ghost_pad_new("sink", pad);
  gst_pad_set_active(ghost_pad, TRUE);
  gst_element_add_pad(bin, ghost_pad);
  gst_object_unref(pad);

  g_object_set(GST_OBJECT(pipeline), "video-sink", bin, NULL);

  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  /* print_current_values(&data); */

  data.loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(data.loop);

  g_io_channel_unref(io_stdin);

  gst_element_set_state(pipeline, GST_STATE_NULL);

  gst_object_unref(pipeline);

  return 0;
}
