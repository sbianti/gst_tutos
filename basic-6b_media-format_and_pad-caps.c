#include <gst/gst.h>
#include <stdio.h>

enum caps_type_t {SRC = 1, SINK = 2};
#define CAPS_TYPE(pointer) (*((enum caps_type_t *)pointer))

GHashTable *factory_names;

static gboolean print_field(GQuark field, const GValue *value, gpointer pfx)
{
  gchar *str = gst_value_serialize(value);

  g_print("%s  %15s: %s\n", (gchar *)pfx, g_quark_to_string(field), str);
  g_free(str);
  return TRUE;
}

static void print_caps(const GstCaps *caps, const gchar *pfx)
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
#define NUMPADTEMPLATES(f) gst_element_factory_get_num_pad_templates(f)
#define STATICPADTEMPLATES(f) gst_element_factory_get_static_pad_templates(f)
#endif

  g_print("Pad Templates for %s:\n", gst_element_factory_get_longname(factory));
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
  char *name;

  g_object_get(element, "name", &name, NULL);

#if GST_VERSION_MAJOR == 0
#define GET_CURRENT_CAPS(pad) gst_pad_get_negotiated_caps(pad)
#define QUERY_CAPS_REFFED(pad) gst_pad_get_caps_reffed(pad)
#else
#define GET_CURRENT_CAPS(pad) gst_pad_get_current_caps(pad)
#define QUERY_CAPS_REFFED(pad) gst_pad_query_caps(pad, NULL)
#endif

  pad = gst_element_get_static_pad(element, pad_name);
  if (!pad) {
    g_printerr("Could not retrieve pad '%s' for element %s\n", pad_name, name);
    return;
  }

  caps = GET_CURRENT_CAPS(pad);
  if (!caps) {
    g_print("BAD : Pad pas dispo tout de suite\n");
    caps = QUERY_CAPS_REFFED(pad);
  } else
    g_print("GOOD : Pad dispo tout de suite\n");

  g_print("Caps for the %s pad of %s:\n", pad_name, name);
  print_caps(caps, "      ");
  gst_caps_unref(caps);
  gst_object_unref(pad);
}

static void iterate_print_info_prv(gpointer factory, gpointer null_data)
{
  print_pad_templates_information((GstElementFactory *) factory);
}

static void iterate_print_info(gpointer factory_branch, gpointer null_data)
{
  g_slist_foreach((GSList *)factory_branch, iterate_print_info_prv, NULL);
}

static void fill_pipeline_with_branch_prv(gpointer elmt, gpointer pipeline)
{
  if (gst_bin_add(GST_BIN(pipeline), (GstElement *)elmt) == FALSE) {
    char *name;
    g_object_get(G_OBJECT(elmt), "name", &name, NULL);
    fprintf(stderr, "échec à l'ajout de l'élément %s dans le pipeline\n", name);
  }
}

static void fill_pipeline_with_branch(gpointer branch, gpointer pipeline)
{
  g_slist_foreach((GSList *)branch, fill_pipeline_with_branch_prv, pipeline);
}

static gboolean fill_pipeline(GstElement *pipeline, GSList *branch_list)
{
  g_slist_foreach(branch_list, fill_pipeline_with_branch, pipeline);

  if (pipeline == NULL)
    return FALSE;

  return TRUE;
}

static gboolean iterate_branch_link(GSList *branch_list)
{
  if (g_slist_length(branch_list) < 2)
    return TRUE;

  if (g_slist_length(branch_list) == 2) {
    GstElement *last_first_branch, *first_second_branch;

    last_first_branch =
      (GstElement *)(g_slist_last((GSList *)branch_list->data))->data;
    first_second_branch =
      (GstElement *)((GSList*)g_slist_last(branch_list)->data)->data;

    if (gst_element_link(last_first_branch, first_second_branch) == FALSE) {
      char *name[2];
      g_object_get(last_first_branch, "name", &name[0], NULL);
      g_object_get(first_second_branch, "name", &name[1], NULL);
      fprintf(stderr, "failed to link %s and %s\n", name[0], name[1]);
    }
  }

  return TRUE;
}

static void link_elements(gpointer branch, gpointer null_pointer)
{
  GSList *actual, *next;
  char *name[2];

  actual = branch;

  while ((next = g_slist_next(actual)) != NULL) {
    if (gst_element_link((GstElement *)(actual->data),
			 (GstElement *)(next->data)) == FALSE) {
      g_object_get((GstElement *)(actual->data), "name", &name[0], NULL);
      g_object_get((GstElement *)(next->data), "name", &name[1], NULL);
      fprintf(stderr,
	      "failed to link two filters (%s and %s)\n", name[0], name[1]);
    }

    actual = next;
  }
}

static gboolean iterate_element_link(GSList *branch)
{

  g_slist_foreach(branch, link_elements, NULL);

  return TRUE;
}

void create_element_branch_prv(gpointer factory, gpointer element_branch)
{
  GstElement *element;
  GSList **branch = element_branch;
  char *facto_name, *name;
  gint rc;

  g_object_get((GstElementFactory *)factory, "name", &facto_name, NULL);
  rc = GST_OBJECT_REFCOUNT(factory);
  name = g_strdup_printf("%s_%d", facto_name, rc);
  printf("creating <%s>\n", name);

  element = gst_element_factory_create((GstElementFactory *)factory, name);
  if (element == NULL) {
    fprintf(stderr, "ERROR: unable to create %s\n", name);
  }

  *branch = g_slist_prepend(*branch, element);
}

void create_element_branch(gpointer factory_branch, gpointer element_list)
{
  GSList *element_branch = NULL;
  GSList **list = element_list;

  g_slist_foreach(factory_branch, create_element_branch_prv, &element_branch);

  element_branch = g_slist_reverse(element_branch);
  *list = g_slist_prepend(*list, element_branch);
}

GSList *element_branch_list_new(GSList *factory_branch_list)
{
  GSList *element_list = NULL;

  g_slist_foreach(factory_branch_list, create_element_branch, &element_list);

  element_list = g_slist_reverse(element_list);

  return element_list;
}

GSList *factory_branch_list_new(int argc, char *argv[])
{
  GSList *branch_list = NULL;
  GSList *branch = NULL;
  GstElementFactory *factory;
  char *name;
  int i;

  for (i=1; i<argc; i++) {

    if (g_strcmp0(argv[i], "BRANCH") != 0) {
      factory = (GstElementFactory *)g_hash_table_lookup(factory_names,
							 argv[i]);
      if (factory == NULL){
	factory = gst_element_factory_find(argv[i]);
	g_hash_table_insert(factory_names, argv[i], factory);

	if (factory == NULL) {
	  fprintf(stderr, "échec à la création de l'usine <%s>\n", argv[i]);
	  return NULL;
	}
      }

      branch = g_slist_prepend(branch, factory);
    }

    if (g_strcmp0(argv[i], "BRANCH") == 0 || g_strcmp0(argv[i], "tee") == 0) {
      branch = g_slist_reverse(branch);
      branch_list = g_slist_prepend(branch_list, branch);
      branch = NULL;
    }
  }

  branch = g_slist_reverse(branch);
  branch_list = g_slist_prepend(branch_list, branch);
  branch_list = g_slist_reverse(branch_list);

  return branch_list;
}

void print_element_caps(gpointer element, gpointer caps_type)
{
  if (SINK & CAPS_TYPE(caps_type))
    print_pad_capabilities(element, "sink");
  if (SRC & CAPS_TYPE(caps_type))
    print_pad_capabilities(element, "src");
}

void print_branch_pad_caps(gpointer branch, gpointer caps_type)
{
  g_slist_foreach((GSList *)branch, print_element_caps, caps_type);
}

int main(int argc, char *argv[])
{
  GstElement *pipeline;
  GSList *element_branch_list, *factory_branch_list;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  gboolean terminate = FALSE;
  gint tee_idx;
  gint extra_branch;

  if (argc < 3) {
    g_printerr("./bidule source filter-1 filter-2… filter-N sink\n");
    g_printerr("ex: ./bidule audiotestsrc autoaudiosink\n");
    return(-1);
  }

  /* ATTENTION: Ce modèle ne permet pas aux branches de se rejoindre */

  gst_init(&argc, &argv);

  element_branch_list = NULL;
  factory_branch_list = NULL;

  factory_names = g_hash_table_new(g_str_hash, g_str_equal);

  factory_branch_list = factory_branch_list_new(argc, argv);
  if (factory_branch_list == NULL) {
    fprintf(stderr, "Échec à la création de la liste des factory-branches\n");
  }

  g_slist_foreach(factory_branch_list, iterate_print_info, NULL);

  element_branch_list = element_branch_list_new(factory_branch_list);
  if (element_branch_list == NULL) {
    fprintf(stderr, "Échec à la création de la liste des element-branches\n");
  }

  pipeline = gst_pipeline_new("test-pipeline");

  if (!pipeline) {
    g_printerr("Not all elements could be created. <%p>\n", pipeline);
    return -1;
  }

  /* gst_bin_add_many (GST_BIN (pipeline), source, sink, NULL); */
  if (fill_pipeline(pipeline, element_branch_list) == FALSE) {
    fprintf(stderr, "Échec à l'ajout des élément dans le pipeline\n");
    return -1;
  }

  iterate_element_link(element_branch_list);
  iterate_branch_link(element_branch_list);
  /* if (get_tee_index(filter, element_nb, &tee_idx) == FALSE) { */
  /*   if (iterate_element_link(source, filter, element_nb, sink) == FALSE) { */
  /*     g_printerr("Elements could not be linked.\n"); */
  /*     gst_object_unref(pipeline); */
  /*     return -1; */
  /*   } */
  /* } else { */
  /*   if (iterate_element_link(source, filter, tee_idx+1, NULL) == FALSE || */
  /* 	iterate_element_link(filter[tee_idx+1], */
  /* 			     filter+tee_idx+2, element_nb-tee_idx-2, */
  /* 			     sink) == FALSE || */
  /* 	gst_element_link(filter[tee_idx], filter[tee_idx+1]) == FALSE) { */
  /* 	    fprintf(stderr, "Elements (en 3 fois) could not be linked.\n"); */
  /* 	    gst_object_unref(pipeline); */
  /* 	    return -1; */
  /*   } */
  /* } */

  g_print("In NULL state:\n");
  /* print_pad_capabilities(sink, "sink"); */
  /* Pas de pad source et sink pour la source car ils sont créés à la volée: */
  /* print_pad_capabilities(source, "sink"); */

  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state (check the bus for error messages).\n");
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
      GstElement *sink_elmnt;

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
	  GSList *sub_list;
	  enum caps_type_t ct = SINK | SRC;
	  gst_message_parse_state_changed(msg, &old_state, &new_state,
					   &pending_state);
	  g_print("\nPipeline state changed from %s to %s:\n",
		   gst_element_state_get_name(old_state),
		   gst_element_state_get_name(new_state));
	  g_slist_foreach(element_branch_list, print_branch_pad_caps, &ct);
	}
	break;
      default:
	g_printerr("Unexpected message received.\n");
	break;
      }
      gst_message_unref(msg);
    }
  } while (!terminate);

  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  return 0;
}
