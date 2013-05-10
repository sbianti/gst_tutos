#include <gst/gst.h>
#include <stdio.h>

enum pipeline_component_index {
  AUDIO_SRC,
  TEE,
  AUDIO_QUEUE,
  AUDIO_CONVERT,
  AUDIO_RESAMPLE,
  AUDIO_SINK,
  VIDEO_QUEUE,
  VISUAL,
  VIDEO_CONVERT,
  VIDEO_SINK,
  COMPONENT_NUMBER
};

struct pipeline_component_naming {
  char *type;
  char *name;
};

struct pipeline_component_naming component_naming[] = {
  {"audiotestsrc", "source audio"},
  {"tee", "le thé vert"},
  {"queue", "queue d'oreille"},
  {"audioconvert", "convertisseur"},
  {"audioresample", "ré-échantilloneur audio"},
  {"autoaudiosink", "sink audio"},
  {"queue", "queue des yeux"},
  {"wavescope", "visuel"},
  {"autovideoconvert", "le convertisseur"},
  {"autovideosink", "sink vidéo"}
};

gboolean make_pipe_components(GstElement **component)
{
  int i;

  for (i=0; i<COMPONENT_NUMBER; i++) {
    component[i] = gst_element_factory_make(component_naming[i].type,
					    component_naming[i].name);
    if (component[i] == NULL) {
      printf("%s%s\n", "failed to create ", component_naming[i].name);
      return FALSE;
    }
  }

  return TRUE;
}

int main(int argc, char *argv[]) {
  GstElement *pipeline;
  GstElement *pipeline_component[COMPONENT_NUMBER];
  GstBus *bus;
  GstMessage *msg;
  GstPadTemplate *tee_src_pad_template;
  GstPad *tee_audio_pad, *tee_video_pad;
  GstPad *queue_audio_pad, *queue_video_pad;
  gboolean terminate = FALSE;
  gint frequence;

  if (argc < 2) {
    g_printerr("No frequence arg: will be played at 215Hz\n");
    frequence = 215;
  } else
    frequence = atoi(argv[1]);

  gst_init(&argc, &argv);

  if (make_pipe_components(pipeline_component) == FALSE) {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  pipeline = gst_pipeline_new("test-pipeline");
  if (pipeline == NULL) {
    fprintf(stderr, "Pipeline pas créé.\n");
    return -1;
  }

  g_object_set(pipeline_component[AUDIO_SRC], "freq", (float)frequence, NULL);
  g_object_set(pipeline_component[VISUAL], "shader", 0, "style", 1, NULL);

  gst_bin_add_many(GST_BIN(pipeline),
		   pipeline_component[AUDIO_SRC],
		   pipeline_component[TEE],
		   pipeline_component[AUDIO_QUEUE],
		   pipeline_component[AUDIO_CONVERT],
		   pipeline_component[AUDIO_RESAMPLE],
		   pipeline_component[AUDIO_SINK],
		   pipeline_component[VIDEO_QUEUE],
		   pipeline_component[VISUAL],
		   pipeline_component[VIDEO_CONVERT],
		   pipeline_component[VIDEO_SINK],
		   NULL);

  if (gst_element_link(pipeline_component[AUDIO_SRC],
		       pipeline_component[TEE]) != TRUE ||
      gst_element_link_many(pipeline_component[AUDIO_QUEUE],
			    pipeline_component[AUDIO_CONVERT],
			    pipeline_component[AUDIO_RESAMPLE],
			    pipeline_component[AUDIO_SINK], NULL) != TRUE ||
      gst_element_link_many(pipeline_component[VIDEO_QUEUE],
      			    pipeline_component[VISUAL],
      			    pipeline_component[VIDEO_CONVERT],
      			    pipeline_component[VIDEO_SINK], NULL) != TRUE) {
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(pipeline);
    return -1;
  }

#if GST_VERSION_MAJOR == 0
#define TEE_SRC_NAME "src%d"
#else
#define TEE_SRC_NAME "src_%u"
#endif

  tee_src_pad_template = gst_element_class_get_pad_template
    (GST_ELEMENT_GET_CLASS(pipeline_component[TEE]), TEE_SRC_NAME);

  tee_audio_pad = gst_element_request_pad(pipeline_component[TEE],
					  tee_src_pad_template, NULL, NULL);
  g_print("Obtained request pad %s for audio branch.\n",
	  gst_pad_get_name(tee_audio_pad));

  queue_audio_pad = gst_element_get_static_pad(pipeline_component[AUDIO_QUEUE],
					       "sink");

  tee_video_pad = gst_element_request_pad(pipeline_component[TEE],
					  tee_src_pad_template, NULL, NULL);
  g_print("Obtained request pad %s for video branch.\n",
	  gst_pad_get_name(tee_video_pad));

  queue_video_pad = gst_element_get_static_pad(pipeline_component[VIDEO_QUEUE],
					       "sink");

  if (gst_pad_link(tee_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK ||
      gst_pad_link(tee_video_pad, queue_video_pad) != GST_PAD_LINK_OK) {
    g_printerr("Tee could not be linked.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  gst_object_unref(queue_audio_pad);
  gst_object_unref(queue_video_pad);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  bus = gst_element_get_bus(pipeline);
  do {
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
				     GST_MESSAGE_ERROR | GST_MESSAGE_EOS |
				     GST_MESSAGE_STATE_CHANGED);
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
	terminate = TRUE;
	break;
      case GST_MESSAGE_EOS:
	g_print("End-Of-Stream reached.\n");
	terminate = TRUE;
	break;
      case GST_MESSAGE_STATE_CHANGED:
	if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
	  GstState old_state, new_state, pending_state;
	  gst_message_parse_state_changed(msg, &old_state, &new_state,
					  &pending_state);
	  g_print("\nPipeline state changed from %s to %s:\n",
		  gst_element_state_get_name(old_state),
		  gst_element_state_get_name(new_state));
	}
	break;
      default:
	fprintf(stderr, "Unexpected message received.\n");
	break;
      }
      gst_message_unref(msg);
    }
  } while (!terminate);

  gst_element_release_request_pad(pipeline_component[TEE], tee_audio_pad);
  gst_element_release_request_pad(pipeline_component[TEE], tee_video_pad);
  gst_object_unref(tee_audio_pad);
  gst_object_unref(tee_video_pad);

  if (msg != NULL)
    gst_message_unref(msg);
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);

  gst_object_unref(pipeline);
  return 0;
}
