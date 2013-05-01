#include <stdio.h>
#include <gst/gst.h>
#include <glib.h>

/* http://docs.gstreamer.com/display/GstSDK/Basic+tutorial+3%3A+Dynamic+pipelines */

#define MAX_PATTERN_VALUE 20
#define DEFAULT_BG_COLOUR 0xff000000
#define DEFAULT_FG_COLOUR 0xffffffff

int main(int argc, char *argv[]) {
  GstElement *pipeline, *source, *sink, *vertigo_filter, *videoconvert;

  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;

  int pattern = 0;
  gint64 *timestamp_offset;
  gboolean is_live = FALSE;
  gboolean peer_alloc = FALSE;
  gboolean vertigotv = FALSE;
  /* typedef enum { */
  /*   GST_VIDEO_TEST_SRC_BT601, */
  /*   GST_VIDEO_TEST_SRC_BT709 */
  /* } GstVideoTestSrcColorSpec*/
  //  GstVideoTestSrcColorSpec colorspec;
  gint k0 = 0;
  gint kt = 0;
  gint kt2 = 0;
  gint kx = 0;
  gint kx2 = 0;
  gint kxt = 0;
  gint kxy = 0;
  gint ky = 0;
  gint ky2 = 0;
  gint kyt = 0;
  gint xoffset = 0;
  gint yoffset = 0;
  gint h_speed = 0;
  guint background_color = DEFAULT_BG_COLOUR;
  guint foreground_color = DEFAULT_FG_COLOUR;

  GError *error = NULL;
  GOptionContext *context;

  GOptionEntry options[] = {
    { "vertigotv", '\0', 0, G_OPTION_ARG_NONE, &vertigotv,
      "Adds this filter to the pipeline", NULL },
    { "pattern", '\0', 0, G_OPTION_ARG_INT, &pattern,
      "The type of test pattern to generate", NULL },
    { "timestamp-offset", '\0', 0, G_OPTION_ARG_INT, &timestamp_offset,
      "An offset added to timestamps set on buffers (in ns)", NULL },
    { "is-live", '\0', 0, G_OPTION_ARG_NONE, &is_live,
      "Whether to act as a live source", NULL },
    { "peer-alloc", '\0', 0, G_OPTION_ARG_NONE, &peer_alloc,
      "Ask the peer to allocate an output buffer", NULL },
    /* { "colorspec", '\0', 0, G_OPTION_ARG_INT, &colorspec "Generate video in the given color specification (Deprecated: use a caps filter with video/x-raw-yuv,color-matrix=\"sdtv\" or \"hdtv\" instead)", NULL }, */
    { "k0", '\0', 0, G_OPTION_ARG_INT, &k0,
      "Zoneplate zero order phase, for generating plain fields"
      " or phase offsets", NULL },
    { "kt", '\0', 0, G_OPTION_ARG_INT, &kt,
      "Zoneplate 1st order t phase, for generating phase rotation"
      " as a function of time", NULL },
    { "kt2", '\0', 0, G_OPTION_ARG_INT, &kt2,
      "Zoneplate 2nd order t phase, t*t/256 cycles per picture", NULL },
    { "kx", '\0', 0, G_OPTION_ARG_INT, &kx,
      "Zoneplate 1st order x phase, for generating "
      "constant horizontal frequencies", NULL },
    { "kx2", '\0', 0, G_OPTION_ARG_INT, &kx2,
      "Zoneplate 2nd order x phase, normalised to "
      "kx2/256 cycles per horizontal pixel at width/2 from origin", NULL },
    { "kxt", '\0', 0, G_OPTION_ARG_INT, &kxt,
      "Zoneplate x*t product phase, normalised to "
      "kxy/256 cycles per vertical pixel at width/2 from origin", NULL },
    { "kxy", '\0', 0, G_OPTION_ARG_INT, &kxy,
      "Zoneplate x*y product phase", NULL },
    { "ky", '\0', 0, G_OPTION_ARG_INT, &ky,
      "Zoneplate 1st order y phase, for generating "
      "contant vertical frequencies", NULL },
    { "ky2", '\0', 0, G_OPTION_ARG_INT, &ky2,
      "Zoneplate 2nd order y phase, normalised to "
      "ky2/256 cycles per vertical pixel at height/2 from origin", NULL },
    { "kyt", '\0', 0, G_OPTION_ARG_INT, &kyt,
      "Zoneplate y*t product phase", NULL },
    { "xoffset", '\0', 0, G_OPTION_ARG_INT, &xoffset,
      "Zoneplate 2nd order products x offset", NULL },
    { "yoffset", '\0', 0, G_OPTION_ARG_INT, &yoffset,
      "Zoneplate 2nd order products y offset", NULL },
    { "background-color", '\0', 0, G_OPTION_ARG_INT, &background_color,
      "Color to use for background color of some patterns. "
      "Default is black (0xff000000)", NULL },
    { "foreground-color", '\0', 0, G_OPTION_ARG_INT, &foreground_color,
      "Color to use for solid-color pattern and foreground color"
      " of other patterns. Default is white (0xffffffff)", NULL },
    { "horizontal-speed", '\0', 0, G_OPTION_ARG_INT, &h_speed,
      "Scroll image number of pixels per frame "
      "(positive is scroll to the left).\nDefault value: 0", NULL },
    { NULL }
  };

  context = g_option_context_new("test autovideosink");

  g_option_context_add_main_entries(context, options, "");
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    g_print("option parsing failed: %s\n", error->message);
    return -1;
  }
  g_option_context_free(context);

  if (pattern > MAX_PATTERN_VALUE || pattern < 0) {
    fprintf(stderr,
	    "Attention! %s out of [0..%d] range\nConsidering 0 value\n",
	    argv[1], MAX_PATTERN_VALUE, MAX_PATTERN_VALUE);
    pattern = 0;
  }

  gst_init(&argc, &argv);

  source = gst_element_factory_make("videotestsrc", "la source");
  sink = gst_element_factory_make("autovideosink", "la sinkronizatiòn");

  pipeline = gst_pipeline_new("pipe-ligne de test");

  if (!pipeline || !source || !sink) {
    fprintf(stderr, "Not all elements could be created: %p, %p, %p.\n",
	    pipeline, source, sink);
    return -1;
  }

  gst_bin_add_many(GST_BIN(pipeline), sink, source, NULL);

  if (vertigotv) {
    vertigo_filter = gst_element_factory_make("vertigotv",
					      "le vertige putain");
    if (vertigo_filter == NULL) {
      fprintf(stderr, "vertigo_filter est NULL, bye bye\n");
      return -1;
    }

    videoconvert = gst_element_factory_make("videoconvert", "el convertor");
    if (videoconvert == NULL) {
      fprintf(stderr, "videoconvert est NULL, bye bye\n");
      return -1;
    }

    gst_bin_add_many(GST_BIN(pipeline),
		     vertigo_filter, videoconvert, NULL);

    if (gst_element_link(source, vertigo_filter) != TRUE) {
      fprintf(stderr, "linked source → vertigo failed !\n");
      gst_object_unref(pipeline);
      return -1;
    }

    if (gst_element_link(vertigo_filter, videoconvert) != TRUE) {
      fprintf(stderr, "link vertigo → videoconvert failed !\n");
      gst_object_unref(pipeline);
      return -1;
    }

    if (gst_element_link(videoconvert, sink) != TRUE) {
      fprintf(stderr, "link videoconvert → sink failed !\n");
      gst_object_unref(pipeline);
      return -1;
    }

    g_object_set(vertigo_filter, "speed", 30.0, NULL);
    g_object_set(vertigo_filter, "zoom-speed", 1.1, NULL);
  } else if (gst_element_link(source, sink) != TRUE) {
    fprintf(stderr, "source → sink could not be linked.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  g_object_set(source, "pattern", pattern, NULL);
  if (k0 != 0)
    g_object_set(source, "k0", k0, NULL);
  if (kt != 0)
    g_object_set(source, "kt", kt, NULL);
  if (kt2 != 0)
    g_object_set(source, "kt2", kt2, NULL);
  if (kx != 0)
    g_object_set(source, "kx", kx, NULL);
  if (kx2 != 0)
    g_object_set(source, "kx2", kx2, NULL);
  if (kxt != 0)
    g_object_set(source, "kxt", kxt, NULL);
  if (kxy != 0)
    g_object_set(source, "kxy", kxy, NULL);
  if (ky != 0)
    g_object_set(source, "ky", ky, NULL);
  if (ky2 != 0)
    g_object_set(source, "ky2", ky2, NULL);
  if (kyt != 0)
    g_object_set(source, "kyt", kyt, NULL);
  if (xoffset != 0)
    g_object_set(source, "xoffset", xoffset, NULL);
  if (yoffset != 0)
    g_object_set(source, "yoffset", yoffset, NULL);
  if (h_speed)
    g_object_set(source, "horizontal_speed", h_speed, NULL);
  if (background_color != DEFAULT_BG_COLOUR)
    g_object_set(source, "background_color", background_color, NULL);
  if (foreground_color != DEFAULT_FG_COLOUR)
    g_object_set(source, "foreground_color", foreground_color, NULL);

  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    fprintf(stderr, "Unable to set the pipeline to the playing state.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  bus = gst_element_get_bus(pipeline);
  msg = gst_bus_timed_pop_filtered(bus,
				   GST_CLOCK_TIME_NONE,
				   GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  if (msg != NULL) {
    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error(msg, &err, &debug_info);
      fprintf(stderr, "Erreur reçu de l'élément «%s»: %s\n",
	      GST_OBJECT_NAME(msg->src), err->message);
      fprintf(stderr, "Debugging info: %s\n",
	      debug_info ? debug_info : "none");
      g_clear_error(&err);
      g_free(debug_info);
      break;
    case GST_MESSAGE_EOS:
      g_print("End-Of-Stream reached.\n");
      break;
    default:
      fprintf(stderr, "Unexpected message received.\n");
      break;
    }
    gst_message_unref(msg);
  }

  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  return 0;
}
