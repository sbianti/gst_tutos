#include <gst/gst.h>
#include <string.h>
#include <stdio.h>

#define CHUNK_SIZE 1024
#define SAMPLE_RATE 44100

#if GST_VERSION_MAJOR == 0
#define AUDIO_CAPS "audio/x-raw-int,channels=1,rate=%d,signed=(boolean)true," \
  "width=16,depth=16,endianness=BYTE_ORDER"
#define TEE_SRC_NAME "src%d"
#define NEW_SAMPLE_SIGNAL_NAME "new-buffer"
#define NEW_SAMPLE_ACTION_NAME "pull-buffer"
#define COMMAND "playbin2 uri=appsrc://"
#else
#define AUDIO_CAPS "audio/x-raw,channels=1,rate=%d,signed=(boolean)true," \
  "width=16,depth=16,endianness=BYTE_ORDER"
#define TEE_SRC_NAME "src_%u"
#define NEW_SAMPLE_SIGNAL_NAME "new-sample"
#define NEW_SAMPLE_ACTION_NAME "pull-sample"
#define COMMAND "playbin uri=appsrc://"
#endif

enum pipeline_component_index {
  APP_SRC,
  TEE,
  AUDIO_QUEUE,
  AUDIO_CONVERT_1,
  AUDIO_RESAMPLE,
  AUDIO_SINK,
  VIDEO_QUEUE,
  AUDIO_CONVERT_2,
  VISUAL,
  VIDEO_CONVERT,
  VIDEO_SINK,
  APP_QUEUE,
  APP_SINK,
  COMPONENT_NUMBER
};

struct pipeline_component_naming {
  char *type;
  char *name;
};

struct pipeline_component_naming component_naming[] = {
  {"appsrc", "source d'application roxante"},
  {"tee", "le thé vert"},
  {"queue", "queue d'oreille"},
  {"audioconvert", "convertisseur odio 1"},
  {"audioresample", "ré-échantilloneur audio"},
  {"autoaudiosink", "sink audio"},
  {"queue", "queue des yeux"},
  {"audioconvert", "convertisseur odio 2"},
  {"wavescope", "visuel"},
  {"autovideoconvert", "le convertisseur"},
  {"autovideosink", "sink vidéo"},
  {"queue", "queue de l'appli qui roxe"},
  {"appsink", "sink d'appli roxifiante"}
};

typedef struct _CustomData {
  GstElement *pipeline;
  GstElement *element[COMPONENT_NUMBER];
  guint64 num_samples;
  gfloat a, b, c, d;
  guint sourceid;
  GMainLoop *main_loop;
} CustomData;

#if GST_VERSION_MAJOR == 0
static gboolean push_data(CustomData *data) {
  GstBuffer *buffer;
  GstFlowReturn ret;
  int i;
  gint16 *raw;
  gint num_samples = CHUNK_SIZE / 2;
  gfloat freq;

  buffer = gst_buffer_new_and_alloc(CHUNK_SIZE);

  GST_BUFFER_TIMESTAMP(buffer) =
    gst_util_uint64_scale(data->num_samples, GST_SECOND, SAMPLE_RATE);
  GST_BUFFER_DURATION(buffer) =
    gst_util_uint64_scale(CHUNK_SIZE, GST_SECOND, SAMPLE_RATE);

  raw = (gint16 *)GST_BUFFER_DATA(buffer);
  data->c += data->d;
  data->d -= data->c / 1000;
  freq = 1100 + 1000 * data->d;
  for (i = 0; i < num_samples; i++) {
    data->a += data->b;
    data->b -= data->a / freq;
    raw[i] = (gint16)(500 * data->a);
  }
  data->num_samples += num_samples;

  g_signal_emit_by_name(data->element[APP_SRC], "push-buffer", buffer, &ret);

  gst_buffer_unref(buffer);

  if (ret != GST_FLOW_OK)
    return FALSE;

  return TRUE;
}

#else

static gboolean push_data(CustomData *data) {
  GstBuffer *buffer;
  GstFlowReturn ret;
  int i;
  gint16 *raw;
  gint num_samples = CHUNK_SIZE / 2; /* samples of 16 bits */
  gfloat freq;
  GstMemory *mem;
  GstMapInfo info;

  buffer = gst_buffer_new_and_alloc(CHUNK_SIZE);

  GST_BUFFER_PTS(buffer) =
    gst_util_uint64_scale(data->num_samples, GST_SECOND, SAMPLE_RATE);
  GST_BUFFER_DURATION(buffer) =
    gst_util_uint64_scale(CHUNK_SIZE, GST_SECOND, SAMPLE_RATE);

  mem = gst_buffer_peek_memory(buffer, 0);
  if (gst_memory_map(mem, &info, GST_MAP_WRITE) == FALSE) {
    fprintf(stderr, "\nErreur à gst_memory_map\n");
    return FALSE;
  }

  printf("\rVoici un buffer de %d octets ; %d échantillons envoyés",
	 info.size, data->num_samples + num_samples);

  /* Generate some psychodelic waveforms */
  raw = (gint16 *)info.data;
  data->c += data->d;
  data->d -= data->c / 1000;
  freq = 1100 + 1000 * data->d;
  for (i = 0; i < num_samples; i++) {
    data->a += data->b;
    data->b -= data->a / freq;
    raw[i] = (gint16)(100 * data->a);
  }

  gst_memory_unmap(mem, &info);

  data->num_samples += num_samples;

  g_signal_emit_by_name(data->element[APP_SRC], "push-buffer", buffer, &ret);

  gst_buffer_unref(buffer);

  if (ret != GST_FLOW_OK) {
    fprintf(stderr, "\nErr !!\n");
    return FALSE;
  }

  return TRUE;
}

#endif

static void start_feed(GstElement *source, guint size, CustomData *data) {
  if (data->sourceid == 0) {
    g_print("Start feeding\n");
    data->sourceid = g_idle_add((GSourceFunc) push_data, data);
  }
}

static void stop_feed(GstElement *source, CustomData *data) {
  if (data->sourceid != 0) {
    g_print("Stop feeding\n");
    g_source_remove(data->sourceid);
    data->sourceid = 0;
  }
}

static void error_cb(GstBus *bus, GstMessage *msg, CustomData *data) {
  GError *err;
  gchar *debug_info;

  gst_message_parse_error(msg, &err, &debug_info);
  g_printerr("Error received from element %s: %s\n",
	     GST_OBJECT_NAME(msg->src), err->message);
  g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
  g_clear_error(&err);
  g_free(debug_info);

  g_main_loop_quit(data->main_loop);
}

static void source_setup(GstElement *pipeline, GstElement *source,
			 CustomData *data) {
  gchar *audio_caps_text;
  GstCaps *audio_caps;

  g_print("Source has been created. Configuring.\n");
  data->element[APP_SRC] = source;

  audio_caps_text = g_strdup_printf(AUDIO_CAPS, SAMPLE_RATE);
  audio_caps = gst_caps_from_string(audio_caps_text);
  g_object_set(source, "caps", audio_caps, NULL);
  g_signal_connect(source, "need-data", G_CALLBACK(start_feed), data);
  g_signal_connect(source, "enough-data", G_CALLBACK(stop_feed), data);
  gst_caps_unref(audio_caps);
  g_free(audio_caps_text);
}

int main(int argc, char *argv[]) {
  CustomData data;
  GstBus *bus;
  gchar *command;

  memset(&data, 0, sizeof(data));
  data.b = 1;
  data.d = 1;

  gst_init(&argc, &argv);

  data.pipeline = gst_parse_launch(COMMAND, NULL);
  g_signal_connect(data.pipeline, "source-setup", G_CALLBACK(source_setup),
		   &data);

  bus = gst_element_get_bus(data.pipeline);
  gst_bus_add_signal_watch(bus);
  g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)error_cb,
		   &data);
  gst_object_unref(bus);

  gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

  data.main_loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(data.main_loop);

  gst_element_set_state(data.pipeline, GST_STATE_NULL);
  gst_object_unref(data.pipeline);
  return 0;
}
