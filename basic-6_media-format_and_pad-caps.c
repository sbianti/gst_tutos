#include <gst/gst.h>
#include <stdio.h>

static gboolean print_field(GQuark field, const GValue * value, gpointer pfx)
{
  gchar *str = gst_value_serialize(value);

  g_print("%s  %15s: %s\n", (gchar *) pfx, g_quark_to_string(field), str);
  g_free(str);
  return TRUE;
}

static void print_caps(const GstCaps * caps, const gchar * pfx)
{
  guint i;

  g_return_if_fail(caps != NULL);

  if (gst_caps_is_any(caps)) {
    g_print("%sANY\n", pfx);
    return;
  }
  if (gst_caps_is_empty(caps)) {
    g_print("%sEMPTY\n", pfx);
    return;
  }

  for (i = 0; i < gst_caps_get_size(caps); i++) {
    GstStructure *structure = gst_caps_get_structure(caps, i);

    g_print("%s%s\n", pfx, gst_structure_get_name(structure));
    gst_structure_foreach(structure, print_field, (gpointer) pfx);
  }
}

static void print_pad_templates_information(GstElementFactory *factory)
{
  const GList *pads;
  GstStaticPadTemplate *padtemplate;

#if GST_VERSION_MAJOR == 0
#define NUMPADTEMPLATES(facto) facto->numpadtemplates
#define STATICPADTEMPLATES(facto) facto->staticpadtemplates
#else
#define NUMPADTEMPLATES(facto) gst_element_factory_get_num_pad_templates(facto)
#define STATICPADTEMPLATES(facto) gst_element_factory_get_static_pad_templates(facto)
#endif

  g_print("Pad Templates for %s:\n",
	  gst_element_factory_get_longname(factory));

  if (NUMPADTEMPLATES(factory) == 0) {
    g_print("  none\n");
    return;
  }

  pads = STATICPADTEMPLATES(factory);
  while (pads) {
    padtemplate = (GstStaticPadTemplate *)(pads->data);
    pads = g_list_next(pads);

    if (padtemplate->direction == GST_PAD_SRC)
      g_print("  SRC template: '%s'\n", padtemplate->name_template);
    else if (padtemplate->direction == GST_PAD_SINK)
      g_print("  SINK template: '%s'\n", padtemplate->name_template);
    else
      g_print("  UNKNOWN!!! template: '%s'\n", padtemplate->name_template);

    if (padtemplate->presence == GST_PAD_ALWAYS)
      g_print("    Availability: Always\n");
    else if (padtemplate->presence == GST_PAD_SOMETIMES)
      g_print("    Availability: Sometimes\n");
    else if (padtemplate->presence == GST_PAD_REQUEST) {
      g_print("    Availability: On request\n");
    } else
      g_print("    Availability: UNKNOWN!!!\n");

    if (padtemplate->static_caps.string) {
      g_print("    Capabilities:\n");
      print_caps(gst_static_caps_get(&padtemplate->static_caps), "      ");
    }

    g_print("\n");
  }
}

static void print_pad_capabilities(GstElement *element, gchar *pad_name)
{
  GstPad *pad = NULL;
  GstCaps *caps = NULL;

#if GST_VERSION_MAJOR == 0
#define GET_CURRENT_CAPS(pad) gst_pad_get_negotiated_caps(pad)
#define QUERY_CAPS_REFFED(pad) gst_pad_get_caps_reffed(pad)
#else
#define GET_CURRENT_CAPS(pad) gst_pad_get_current_caps(pad)
#define QUERY_CAPS_REFFED(pad) gst_pad_query_caps(pad, NULL)
#endif

  pad = gst_element_get_static_pad(element, pad_name);
  if (!pad) {
    g_printerr("Could not retrieve pad '%s'\n", pad_name);
    return;
  }

  /* Retrieve negotiated caps (or acceptable caps if negotiation is not finished yet) */
  caps = GET_CURRENT_CAPS(pad);
  if (!caps) {
    g_print("BAD : Pad pas dispo tout de suite\n");
    caps = QUERY_CAPS_REFFED(pad);
  } else
    g_print("GOOD : Pad dispo tout de suite\n");

  g_print("Caps for the %s pad:\n", pad_name);
  print_caps(caps, "      ");
  gst_caps_unref(caps);
  gst_object_unref(pad);
}

int main(int argc, char *argv[])
{
  GstElement *pipeline, *source, *sink;
  GstElementFactory *source_factory, *sink_factory;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  gboolean terminate = FALSE;

  gst_init(&argc, &argv);

  source_factory = gst_element_factory_find("audiotestsrc");
  sink_factory = gst_element_factory_find("autoaudiosink");
  if (!source_factory || !sink_factory) {
    g_printerr("Not all element factories could be created. <%p>;<%p>\n",
	       source_factory, sink_factory);
    return -1;
  }

  print_pad_templates_information(source_factory);
  print_pad_templates_information(sink_factory);

  source = gst_element_factory_create(source_factory, "source");
  sink = gst_element_factory_create(sink_factory, "sink");

  pipeline = gst_pipeline_new("test-pipeline");

  if (!pipeline || !source || !sink) {
    g_printerr("Not all elements could be created. <%p>;<%p>;<%p>\n",
	       pipeline, source, sink);
    return -1;
  }

  gst_bin_add_many(GST_BIN(pipeline), source, sink, NULL);
  if (gst_element_link(source, sink) != TRUE) {
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  g_print("In NULL state:\n");
  print_pad_capabilities(sink, "sink");
  /* Pas de pad source et sink pour la source car ils sont créés à la volée: */
  /* print_pad_capabilities(source, "sink"); */

  {
    gdouble freq;
    g_object_get(source, "freq", &freq, NULL);
    printf("fréquence :%dHz\n", (int)freq);
  }

  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state"
	       " (check the bus for error messages).\n");
  }

  bus = gst_element_get_bus(pipeline);
  do {
    msg = gst_bus_timed_pop_filtered(bus,
				     GST_CLOCK_TIME_NONE,
				     GST_MESSAGE_ERROR | GST_MESSAGE_EOS |
				     GST_MESSAGE_STATE_CHANGED);

    if (msg != NULL) {
      GError *err;
      gchar *debug_info;

      switch (GST_MESSAGE_TYPE(msg)) {
      case GST_MESSAGE_ERROR:
	gst_message_parse_error(msg, &err, &debug_info);
	g_printerr("Error received from element %s: %s\n",
		   GST_OBJECT_NAME(msg->src), err->message);
	g_printerr("Debugging information: %s\n",
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

	  print_pad_capabilities(sink, "sink");
	}
	break;
      default:
	g_printerr("Unexpected message received.\n");
	break;
      }
      gst_message_unref(msg);
    }
  } while(!terminate);

  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  gst_object_unref(source_factory);
  gst_object_unref(sink_factory);
  return 0;
}
