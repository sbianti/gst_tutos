#include <gst/gst.h>

#if GST_VERSION_MAJOR == 0
#define PLAYBIN "playbin2"
#define GET_PLUGIN_REGISTRY gst_registry_get_default()
#else
#define PLAYBIN "playbin"
#define GET_PLUGIN_REGISTRY gst_registry_get()
#endif

#define DEFAULT_URI "http://radio.hbr1.com:19800/ambient.ogg"

typedef enum {
  GST_PLAY_FLAG_VIS = (1 << 3)
} GstPlayFlags;

/* Return TRUE if this is a Visualization element */
static gboolean filter_vis_features(GstPluginFeature *feature, gpointer data) {
  GstElementFactory *factory;

  if (!GST_IS_ELEMENT_FACTORY(feature))
    return FALSE;

  factory = GST_ELEMENT_FACTORY(feature);
  if (!g_strrstr(gst_element_factory_get_klass(factory), "Visualization"))
    return FALSE;

  return TRUE;
}

int main(int argc, char *argv[]) {
  GstElement *pipeline, *vis_plugin;
  GstBus *bus;
  GstMessage *msg;
  GList *list, *walk;
  GstElementFactory *selected_factory = NULL;
  guint flags;
  char *pipeline_str;
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
				     filter_vis_features, FALSE, NULL);

  if (plugin_name == NULL)
    plugin_name = "GOOM";

  g_print("Available visualization plugins:\n");
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

  /* Don't use the factory if it's still empty */
  /* e.g. no visualization plugins found */
  if (!selected_factory) {
    g_print("No visualization plugins found!\n");
    return -1;
  }

  if (list_plugins == TRUE)
    return 0;

  g_print("Selected '%s'\n",
	  gst_element_factory_get_longname(selected_factory));

  vis_plugin = gst_element_factory_create(selected_factory, NULL);
  if (!vis_plugin)
    return -1;

  pipeline = gst_parse_launch(pipeline_str, NULL);

  g_object_get(pipeline, "flags", &flags, NULL);
  flags |= GST_PLAY_FLAG_VIS;
  g_object_set(pipeline, "flags", flags, NULL);

  g_object_set(pipeline, "vis-plugin", vis_plugin, NULL);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  bus = gst_element_get_bus(pipeline);
  msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
				   GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  if (msg != NULL)
    gst_message_unref(msg);

  gst_plugin_feature_list_free(list);

  gst_object_unref(bus);

  gst_element_set_state(pipeline, GST_STATE_NULL);

  gst_object_unref(pipeline);

  return 0;
}
