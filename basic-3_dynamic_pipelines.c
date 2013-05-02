#include <gst/gst.h>
#include <stdlib.h>
#include <stdio.h>

/* http://docs.gstreamer.com/display/GstSDK/Basic+tutorial+3%3A+Dynamic+pipelines */

typedef struct _CustomData {
  GstElement *pipeline;
  GstElement *source;
  GstElement *convert;
  GstElement *sink;
} CustomData;

static void pad_added_handler(GstElement *src, GstPad *pad, CustomData *data);

int main(int argc, char *argv[]) {
  CustomData data;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  gboolean terminate = FALSE;
  gchar *file_name;
  gchar *cmd;

  if (argc < 2) {
    fprintf(stderr, "File to play missing\n");
    exit(-1);
  }

  file_name = argv[1];

  gst_init(&argc, &argv);

  data.source = gst_element_factory_make("uridecodebin", "la source");
  data.convert = gst_element_factory_make("audioconvert", "le convertisseur");
  data.sink = gst_element_factory_make("autoaudiosink", "le sinkronizeur");

  data.pipeline = gst_pipeline_new("test-pipeline");

  if (!data.pipeline || !data.source || !data.convert || !data.sink) {
    fprintf(stderr, "Not all elements could be created.\n");
    return -1;
  }

  gst_bin_add_many(GST_BIN(data.pipeline),
		   data.source, data.convert, data.sink, NULL);
  if (!gst_element_link(data.convert, data.sink)) {
    fprintf(stderr, "Elements could not be linked.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  cmd = g_strdup_printf("%s%s", "file://", file_name);
  g_object_set(data.source, "uri", cmd, NULL);

  g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler),
		   &data);

  // Ces 2 éléments ne génèrent pas "pad-added":
  /* g_signal_connect(data.convert, "pad-added", G_CALLBACK(pad_added_handler), */
  /* 		    &data); */
  /* g_signal_connect(data.sink, "pad-added", G_CALLBACK(pad_added_handler), */
  /* 		    &data); */

  ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    fprintf(stderr, "Unable to set the pipeline to the playing state.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  bus = gst_element_get_bus(data.pipeline);
  do {
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
				     GST_MESSAGE_STATE_CHANGED |
				     GST_MESSAGE_ERROR |
				     GST_MESSAGE_EOS);

    if (msg != NULL) {
      GError *err;
      gchar *debug_info;
      GstState old_state, new_state, pending_state;

      switch (GST_MESSAGE_TYPE(msg)) {
      case GST_MESSAGE_ERROR:
	gst_message_parse_error(msg, &err, &debug_info);
	fprintf(stderr, "Error received from element %s: %s\n",
		GST_OBJECT_NAME(msg->src), err->message);
	fprintf(stderr, "Debugging information: %s\n",
		debug_info ? debug_info : "none");
	g_clear_error(&err);
	g_free(debug_info);
	terminate = TRUE;
	break;
      case GST_MESSAGE_EOS:
	printf("End-Of-Stream reached.\n");
	terminate = TRUE;
	break;
      case GST_MESSAGE_STATE_CHANGED:
	/* We are only interested in state-changed messages from the pipeline */
	if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data.pipeline)) {
	  gst_message_parse_state_changed(msg, &old_state, &new_state,
					  &pending_state);
	  printf("Pipeline state changed from %s to %s:\n",
		 gst_element_state_get_name(old_state),
		 gst_element_state_get_name(new_state));
	} else {
	  if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data.source))
	    puts("C'est la source");
	  else if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data.convert))
	    puts("C'est le convert");
	  else if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data.sink))
	    puts("C'est le SINK");
	  else
	    printf("C'est vraiment un autre élément : %p\n", msg);

	  gst_message_parse_state_changed(msg, &old_state, &new_state,
					  &pending_state);
	  printf("state : «%s» ⇒ «%s»\n",
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

  gst_object_unref(bus);
  gst_element_set_state(data.pipeline, GST_STATE_NULL);
  gst_object_unref(data.pipeline);
  return 0;
}

static void pad_added_handler(GstElement *src,
			      GstPad *new_pad,
			      CustomData *data) {
  GstPad *sink_pad = gst_element_get_static_pad(data->convert, "sink");
  GstPadLinkReturn ret;
  GstCaps *new_pad_caps = NULL;
  GstStructure *new_pad_struct = NULL;
  const gchar *new_pad_type = NULL;

  printf("Received new pad '%s' from '%s':\n",
	 GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

  if (gst_pad_is_linked(sink_pad)) {
    printf("  We are already linked. Ignoring.\n");
    goto exit;
  }

#if GST_VERSION_MAJOR == 0
  new_pad_caps = gst_pad_get_caps(new_pad);
#else
  new_pad_caps = gst_pad_query_caps(new_pad, NULL);
#endif
  new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
  new_pad_type = gst_structure_get_name(new_pad_struct);
  if (!g_str_has_prefix(new_pad_type, "audio/x-raw")) {
    printf("  It has type '%s' which is not raw audio. Ignoring.\n",
	   new_pad_type);
    goto exit;
  }

  ret = gst_pad_link(new_pad, sink_pad);
  if (GST_PAD_LINK_FAILED(ret)) {
    printf("  Type is '%s' but link failed.\n", new_pad_type);
  } else {
    printf("  Link succeeded(type '%s').\n", new_pad_type);
  }

 exit:
  if (new_pad_caps != NULL)
    gst_caps_unref(new_pad_caps);

  gst_object_unref(sink_pad);
}
