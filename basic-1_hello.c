#include <gst/gst.h>
#include <stdlib.h>

/* http://docs.gstreamer.com/pages/viewpage.action?pageId=327735 */

int main(int argc, char *argv[]) {
  GstElement *pipeline;
  GstBus *bus;
  GstMessage *msg;
  char *file_name;
  char *pipeline_cmd;
  GstStateChangeReturn returned_change_state;

  if (argc < 2) {
    g_printerr("File to play missing\n");
    exit (-1);
  }

  file_name = argv[1];

  gst_init (&argc, &argv);

  pipeline_cmd = g_strdup_printf("%s%s%c",
				 "playbin uri=\"file://", file_name, '"');
  pipeline = gst_parse_launch(pipeline_cmd, NULL);
   
  returned_change_state = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  switch (returned_change_state) {
  case GST_STATE_CHANGE_FAILURE:
    puts("GST_STATE_CHANGE_FAILURE, au revoir !");
    gst_object_unref (pipeline);
    return -1;
  case GST_STATE_CHANGE_SUCCESS:
    puts("GST_STATE_CHANGE_SUCCESS");
    break;
  case GST_STATE_CHANGE_ASYNC:
    puts("GST_STATE_CHANGE_ASYNC");
    break;
  case GST_STATE_CHANGE_NO_PREROLL:
    puts("GST_STATE_CHANGE_NO_PREROLL");
  }
   
  bus = gst_element_get_bus(pipeline);
  msg = gst_bus_timed_pop_filtered(bus,
				   GST_CLOCK_TIME_NONE,
				   GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  if (msg != NULL)
    gst_message_unref(msg);
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);

  return 0;
}
