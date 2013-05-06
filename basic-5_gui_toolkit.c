#include <string.h>

#include <gtk/gtk.h>
#include <gst/gst.h>
#if GST_VERSION_MAJOR == 0
#include <gst/interfaces/xoverlay.h>
#else
#include <gst/video/videooverlay.h>
#endif

#include <gdk/gdk.h>
#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#elif defined (GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#elif defined (GDK_WINDOWING_QUARTZ)
#include <gdk/gdkquartz.h>
#endif

#if GST_VERSION_MAJOR == 0
#define FMT &fmt
#else
#define FMT fmt
#endif

typedef struct _CustomData {
  GstElement *playbin;
  GtkWidget *slider;
  GtkWidget *streams_list;
  gulong slider_update_signal_id;
  GstState state;
  gint64 duration;
} CustomData;

/****************************************************************************/
/* This function is called when the GUI toolkit creates the physical window */
/* that will hold the video.						    */
/* At this point we can retrieve its handler (which has a different meaning */
/* depending on the windowing system) and pass it to GStreamer through	    */
/* the XOverlay interface.						    */
/****************************************************************************/
static void realize_cb(GtkWidget *widget, CustomData *data)
{
  GdkWindow *window = gtk_widget_get_window(widget);
  guintptr window_handle;

  if (!gdk_window_ensure_native(window))
    g_error("Couldn't create native window needed for Gst<?>Overlay!");

#if defined (GDK_WINDOWING_WIN32)
  window_handle = (guintptr)GDK_WINDOW_HWND(window);
#elif defined (GDK_WINDOWING_QUARTZ)
  window_handle = gdk_quartz_window_get_nsview(window);
#elif defined (GDK_WINDOWING_X11)
  window_handle = GDK_WINDOW_XID(window);
#endif

#if GST_VERSION_MAJOR == 0
  gst_x_overlay_set_window_handle(GST_X_OVERLAY(data->playbin), window_handle);
#else
  gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(data->playbin),
				      window_handle);
#endif
}

static void play_cb(GtkButton *button, CustomData *data)
{
  gst_element_set_state(data->playbin, GST_STATE_PLAYING);
}

static void pause_cb(GtkButton *button, CustomData *data)
{
  gst_element_set_state(data->playbin, GST_STATE_PAUSED);
}

static void stop_cb(GtkButton *button, CustomData *data)
{
  gst_element_set_state(data->playbin, GST_STATE_READY);
}

static void delete_event_cb(GtkWidget *widget, GdkEvent *event,
			    CustomData *data)
{
  stop_cb(NULL, data);
  gtk_main_quit();
}

static gboolean expose_cb(GtkWidget *widget, GdkEventExpose *event,
			  CustomData *data)
{
  if (data->state < GST_STATE_PAUSED) {
    GtkAllocation allocation;
    GdkWindow *window = gtk_widget_get_window(widget);
    cairo_t *cr;
 /***********************************************************************/
 /* Cairo is a 2D graphics library which we use here to clean the video */
 /* window.							        */
 /* It is used by GStreamer for other reasons, so it will always be     */
 /* available to us.						        */
 /***********************************************************************/
    gtk_widget_get_allocation(widget, &allocation);
    cr = gdk_cairo_create(window);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
    cairo_fill(cr);
    cairo_destroy(cr);
  }

  return FALSE;
}

static void slider_cb(GtkRange *range, CustomData *data)
{
  gdouble value = gtk_range_get_value(GTK_RANGE(data->slider));
  printf("seek to %.2fs\n", value);
  gst_element_seek_simple(data->playbin, GST_FORMAT_TIME,
			  GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
			  (gint64)(value * GST_SECOND));
}

static void create_ui(CustomData *data)
{
  GtkWidget *main_window;
  GtkWidget *video_window;
  GtkWidget *main_box;
  GtkWidget *main_hbox;
  GtkWidget *controls;
  GtkWidget *play_button, *pause_button, *stop_button;

  main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(G_OBJECT(main_window), "delete-event",
		   G_CALLBACK(delete_event_cb), data);

  video_window = gtk_drawing_area_new();
  gtk_widget_set_double_buffered(video_window, FALSE);
  g_signal_connect(video_window, "realize", G_CALLBACK(realize_cb), data);
#if GTK_MAJOR_VERSION == 2
  g_signal_connect(video_window, "expose_event", G_CALLBACK(expose_cb), data);
#else
  g_signal_connect(video_window, "draw", G_CALLBACK(expose_cb), data);
#endif

  play_button = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
  g_signal_connect(G_OBJECT(play_button), "clicked", G_CALLBACK(play_cb), data);

  pause_button = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PAUSE);
  g_signal_connect(G_OBJECT(pause_button), "clicked",
		   G_CALLBACK(pause_cb), data);

  stop_button = gtk_button_new_from_stock(GTK_STOCK_MEDIA_STOP);
  g_signal_connect(G_OBJECT(stop_button), "clicked", G_CALLBACK(stop_cb), data);

  data->slider = gtk_hscale_new_with_range(0, 100, 1);
  gtk_scale_set_draw_value(GTK_SCALE(data->slider), 0);
  data->slider_update_signal_id =
    g_signal_connect(G_OBJECT(data->slider), "value-changed",
		     G_CALLBACK(slider_cb), data);

  data->streams_list = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(data->streams_list), FALSE);

  controls = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(controls), play_button, FALSE, FALSE, 2);
  gtk_box_pack_start(GTK_BOX(controls), pause_button, FALSE, FALSE, 2);
  gtk_box_pack_start(GTK_BOX(controls), stop_button, FALSE, FALSE, 2);
  gtk_box_pack_start(GTK_BOX(controls), data->slider, TRUE, TRUE, 2);

  main_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(main_hbox), video_window, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(main_hbox), data->streams_list, FALSE, FALSE, 2);

  main_box = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(main_box), main_hbox, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(main_box), controls, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(main_window), main_box);
  gtk_window_set_default_size(GTK_WINDOW(main_window), 640, 480);

  gtk_widget_show_all(main_window);
}

static inline void refresh_slider(CustomData *data, GstFormat fmt)
{
  gint64 pos = -1;

  if (gst_element_query_position(data->playbin, FMT, &pos)) {
    g_signal_handler_block(data->slider, data->slider_update_signal_id);
    gtk_range_set_value(GTK_RANGE(data->slider), (gdouble)pos / GST_SECOND);
    g_signal_handler_unblock(data->slider, data->slider_update_signal_id);
  }
}

static gboolean refresh_ui(CustomData *data)
{
  GstFormat fmt = GST_FORMAT_TIME;

  if (data->state < GST_STATE_PLAYING)
    return TRUE;

  if (!GST_CLOCK_TIME_IS_VALID(data->duration)) {
    if (!gst_element_query_duration(data->playbin, FMT, &data->duration)) {
      g_printerr("Could not query current duration.\n");
      return TRUE;
    } else {
      gtk_range_set_range(GTK_RANGE(data->slider), 0,
			  (gdouble)data->duration / GST_SECOND);
    }
  }

  refresh_slider(data, FMT);

  return TRUE;
}

static void tags_cb(GstElement *playbin, gint stream, CustomData *data)
{
  GstMessage* message =
    gst_message_new_application(GST_OBJECT(playbin),
				gst_structure_new("tags-changed", NULL));

  gst_element_post_message(playbin, message);
}

static void error_cb(GstBus *bus, GstMessage *msg, CustomData *data)
{
  GError *err;
  gchar *debug_info;

  gst_message_parse_error(msg, &err, &debug_info);
  g_printerr("Error received from element %s: %s\n",
	     GST_OBJECT_NAME(msg->src), err->message);
  g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
  g_clear_error(&err);
  g_free(debug_info);

  gst_element_set_state(data->playbin, GST_STATE_READY);
}

static void eos_cb(GstBus *bus, GstMessage *msg, CustomData *data)
{
  g_print("End-Of-Stream reached.\n");
  gst_element_set_state(data->playbin, GST_STATE_READY);
}

static void state_changed_cb(GstBus *bus, GstMessage *msg, CustomData *data)
{
  GstState old_state, new_state, pending_state;

  gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

  if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->playbin)) {
    data->state = new_state;
    g_print("State set to %s\n", gst_element_state_get_name(new_state));
    if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
      refresh_slider(data, GST_FORMAT_TIME);
    }
  }
}

static void analyze_streams(CustomData *data)
{
  gint i;
  GstTagList *tags;
  gchar *str, *total_str;
  guint rate;
  gint n_video, n_audio, n_text;
  GtkTextBuffer *text;

  text = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->streams_list));
  gtk_text_buffer_set_text(text, "", -1);

  g_object_get(data->playbin, "n-video", &n_video, NULL);
  g_object_get(data->playbin, "n-audio", &n_audio, NULL);
  g_object_get(data->playbin, "n-text", &n_text, NULL);

  for (i = 0; i < n_video; i++) {
    tags = NULL;

    g_signal_emit_by_name(data->playbin, "get-video-tags", i, &tags);
    if (tags) {
      total_str = g_strdup_printf("video stream %d:\n", i);
      gtk_text_buffer_insert_at_cursor(text, total_str, -1);
      g_free(total_str);
      gst_tag_list_get_string(tags, GST_TAG_VIDEO_CODEC, &str);
      total_str = g_strdup_printf("  codec: %s\n", str ? str : "unknown");
      gtk_text_buffer_insert_at_cursor(text, total_str, -1);
      g_free(total_str);
      g_free(str);
      gst_tag_list_free(tags);
    }
  }

  for (i = 0; i < n_audio; i++) {
    tags = NULL;

    g_signal_emit_by_name(data->playbin, "get-audio-tags", i, &tags);
    if (tags) {
      total_str = g_strdup_printf("\naudio stream %d:\n", i);
      gtk_text_buffer_insert_at_cursor(text, total_str, -1);
      g_free(total_str);
      if (gst_tag_list_get_string(tags, GST_TAG_AUDIO_CODEC, &str)) {
        total_str = g_strdup_printf("  codec: %s\n", str);
        gtk_text_buffer_insert_at_cursor(text, total_str, -1);
        g_free(total_str);
        g_free(str);
      }
      if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &str)) {
        total_str = g_strdup_printf("  language: %s\n", str);
        gtk_text_buffer_insert_at_cursor(text, total_str, -1);
        g_free(total_str);
        g_free(str);
      }
      if (gst_tag_list_get_uint(tags, GST_TAG_BITRATE, &rate)) {
        total_str = g_strdup_printf("  bitrate: %d\n", rate);
        gtk_text_buffer_insert_at_cursor(text, total_str, -1);
        g_free(total_str);
      }
      gst_tag_list_free(tags);
    }
  }

  for (i = 0; i < n_text; i++) {
    tags = NULL;

    g_signal_emit_by_name(data->playbin, "get-text-tags", i, &tags);
    if (tags) {
      total_str = g_strdup_printf("\nsubtitle stream %d:\n", i);
      gtk_text_buffer_insert_at_cursor(text, total_str, -1);
      g_free(total_str);
      if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &str)) {
        total_str = g_strdup_printf("  language: %s\n", str);
        gtk_text_buffer_insert_at_cursor(text, total_str, -1);
        g_free(total_str);
        g_free(str);
      }
      gst_tag_list_free(tags);
    }
  }
}

static void application_cb(GstBus *bus, GstMessage *msg, CustomData *data)
{
#if GST_VERSION_MAJOR == 0
#define MSG_STR msg->structure
#else
#define MSG_STR gst_message_get_structure(msg)
#endif
  if (g_strcmp0(gst_structure_get_name(MSG_STR), "tags-changed") == 0) {
    analyze_streams(data);
  }
}

int main(int argc, char *argv[])
{
  CustomData data;
  GstStateChangeReturn ret;
  GstBus *bus;
  char *uri;

  if (argc < 2) {
    g_printerr("File to play missing\n");
    return -1;
  }

  uri = g_strdup_printf("%s%s", "file://", argv[1]);

  gtk_init(&argc, &argv);

  gst_init(&argc, &argv);

  memset(&data, 0, sizeof (data));
  data.duration = GST_CLOCK_TIME_NONE;

  data.playbin = gst_element_factory_make("playbin", "playbin");

  if (!data.playbin) {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  g_object_set(data.playbin, "uri", uri, NULL);

  g_signal_connect(G_OBJECT(data.playbin), "video-tags-changed",
		   (GCallback) tags_cb, &data);
  g_signal_connect(G_OBJECT(data.playbin), "audio-tags-changed",
		   (GCallback) tags_cb, &data);
  g_signal_connect(G_OBJECT(data.playbin), "text-tags-changed",
		   (GCallback) tags_cb, &data);

  create_ui(&data);

  bus = gst_element_get_bus(data.playbin);
  gst_bus_add_signal_watch(bus);
  g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)error_cb, &data);
  g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)eos_cb, &data);
  g_signal_connect(G_OBJECT(bus), "message::state-changed",
		   (GCallback)state_changed_cb, &data);
  g_signal_connect(G_OBJECT(bus), "message::application",
		   (GCallback)application_cb, &data);
  gst_object_unref(bus);

  ret = gst_element_set_state(data.playbin, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(data.playbin);
    return -1;
  }

  g_timeout_add_seconds(1, (GSourceFunc)refresh_ui, &data);

  gtk_main();

  gst_element_set_state(data.playbin, GST_STATE_NULL);
  gst_object_unref(data.playbin);
  return 0;
}
