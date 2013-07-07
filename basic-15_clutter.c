#include <clutter-gst/clutter-gst.h>

#if GST_VERSION_MAJOR == 0
#define PLAYBIN "playbin2"
#else
#define PLAYBIN "playbin"
#endif

#define DEFAULT_URI "http://docs.gstreamer.com/media/sintel_trailer-480p.webm"

/* Setup the video texture once its size is known */
void size_change(ClutterActor *texture,
		 gint width, gint height,
		 gpointer user_data) {
  ClutterActor *stage;
  gfloat new_x, new_y, new_width, new_height;
  gfloat stage_width, stage_height;
  ClutterAnimation *animation = NULL;

  stage = clutter_actor_get_stage(texture);
  if (stage == NULL)
    return;

  clutter_actor_get_size(stage, &stage_width, &stage_height);

  /* Center video on window and calculate new size preserving aspect ratio */
  new_height = (height * stage_width) / width;

  if (new_height <= stage_height) {
    new_width = stage_width;
    new_x = 0;
    new_y = (stage_height - new_height) / 2;
  } else {
    new_width  = (width * stage_height) / height;
    new_height = stage_height;
    new_x = (stage_width - new_width) / 2;
    new_y = 0;
  }

  clutter_actor_set_position(texture, new_x, new_y);
  clutter_actor_set_size(texture, new_width, new_height);
  clutter_actor_set_rotation(texture, CLUTTER_Y_AXIS, 0.0, stage_width/2, 0, 0);
  /* Animate it */
  animation = clutter_actor_animate(texture, CLUTTER_LINEAR, 10000,
				    "rotation-angle-y", 360.0, NULL);
  clutter_animation_set_loop(animation, TRUE);
}

int main(int argc, char *argv[]) {
  GstElement *pipeline, *sink;
  ClutterTimeline *timeline;
  ClutterActor *stage, *texture;
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

  if (clutter_gst_init(&argc, &argv) != CLUTTER_INIT_SUCCESS) {
    g_error("Failed to initialize clutter\n");
    return -1;
  }

  stage = clutter_stage_get_default();

  timeline = clutter_timeline_new(1000);
  g_object_set(timeline, "loop", TRUE, NULL);

  /* Create new texture and disable slicing so the video is properly mapped onto it */
  texture = CLUTTER_ACTOR(g_object_new(CLUTTER_TYPE_TEXTURE, "disable-slicing",
				       TRUE, NULL));
  g_signal_connect(texture, "size-change", G_CALLBACK(size_change), NULL);

  pipeline = gst_parse_launch(pipeline_str, NULL);

  sink = gst_element_factory_make("autocluttersink", NULL);
  if (sink == NULL) {
    /* Revert to the older cluttersink */
    sink = gst_element_factory_make("cluttersink", NULL);
  }
  if (sink == NULL) {
    g_printerr("Unable to find a Clutter sink.\n");
    return -1;
  }

  /* Link GStreamer with Clutter by passing the Clutter texture to the Clutter sink*/
  g_object_set(sink, "texture", texture, NULL);

  g_object_set(pipeline, "video-sink", sink, NULL);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  clutter_timeline_start(timeline);

  /* Add texture to the stage, and show it */
  clutter_group_add(CLUTTER_GROUP(stage), texture);
  clutter_actor_show_all(stage);

  clutter_main();

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  return 0;
}
