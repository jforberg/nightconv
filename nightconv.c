#include <glib.h>
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>

static int bus_call(GstBus *bus, GstMessage *msg, void *data);
static void on_pad_added(GstElement *element, GstPad *pad, void *data);

static const double equalizer_bands[] = {
  1.0, 3.0, 3.0, 1.0, 0.0, 0.0, -1.0, -2.0, -2.0, -2.0, 0.0,
};
static char *infile, *outfile;

int
main(int argc, char **argv)
{
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);

  gst_init(&argc, &argv);

  if (argc < 2) {
    g_printerr("Usage: nightcore <infile> [outfile]\n");
    exit(1);
  } else if (argc > 2) {
    outfile = argv[2];
  }

  infile = argv[1];

  GstElement *pipeline = gst_pipeline_new("mainpipe"),
             *source = gst_element_factory_make("filesrc", "source"),
             *decode = gst_element_factory_make("decodebin", "decode"),
             *convert1 = gst_element_factory_make("audioconvert", "convert1"),
             *resample1 = gst_element_factory_make("audioresample", "resample1"),
             *pitch = gst_element_factory_make("pitch", "pitch"),
             *equalizer = gst_element_factory_make("equalizer-10bands", "equalizer"),
             *convert2 = gst_element_factory_make("audioconvert", "convert2"),
             *resample2 = gst_element_factory_make("audioresample", "resample2");

  if (!pipeline || !source || !decode || !convert1 || !resample1 || !pitch ||
      !equalizer || !convert2 || !resample2) {
    g_printerr("Failed to create pipeline\n");
    exit(1);
  }

  g_object_set(G_OBJECT(source), "location", infile, NULL);

  g_object_set(G_OBJECT(pitch), "rate", 1.3, "pitch", 1.0, NULL);

  for (int i = 0; i < 10; i++ ) {
    char buffer[256];
    snprintf(buffer, 256, "band%d", i);
    g_object_set(G_OBJECT(equalizer), buffer, equalizer_bands[i], NULL);
  }

  gst_bin_add_many(GST_BIN(pipeline), source, decode, convert1, resample1,
                   pitch, equalizer, convert2, resample2, NULL);

  gst_element_link(source, decode);
  gst_element_link_many(convert1, resample1, pitch, equalizer, convert2,
                        resample2, NULL);
  g_signal_connect(decode, "pad-added", G_CALLBACK(on_pad_added), convert1);

  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  unsigned watch_id = gst_bus_add_watch(bus, bus_call, loop);
  gst_object_unref(bus);

  if (outfile) {
    GstElement *mp3enc = gst_element_factory_make("lamemp3enc", "mp3enc"),
               *filesink = gst_element_factory_make("filesink", "filesink");

    if (!mp3enc || !filesink) {
      g_printerr("Failed to create pipeline\n");
      exit(1);
    }

    g_object_set(G_OBJECT(filesink), "location", outfile, NULL);

    gst_bin_add_many(GST_BIN(pipeline), mp3enc, filesink, NULL);
    gst_element_link_many(resample2, mp3enc, filesink, NULL);
  } else {
    GstElement *autosink = gst_element_factory_make("autoaudiosink", "autosink");

    if (!autosink) {
      g_printerr("Failed to create pipeline\n");
      exit(1);
    }

    gst_bin_add(GST_BIN(pipeline), autosink);
    gst_element_link(resample2, autosink);
  }

  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  g_main_loop_run(loop);

  gst_element_set_state(pipeline, GST_STATE_NULL);

  gst_object_unref(pipeline);
  g_source_remove(watch_id);
  g_main_loop_unref(loop);
}

static int
bus_call(GstBus *bus, GstMessage *msg, void *data)
{
  GMainLoop *loop = data;

  switch (GST_MESSAGE_TYPE(msg)) {
  case GST_MESSAGE_EOS:
    g_main_loop_quit(loop);
    break;

  case GST_MESSAGE_ERROR:
  {
    char *debug;
    GError *error;

    gst_message_parse_error(msg, &error, &debug);
    g_free(debug);

    g_printerr("Error: %s\n", error->message);
    g_error_free(error);

    g_main_loop_quit(loop);
    break;
  }
  default:
    ;
  }

  return TRUE;
}

static void
on_pad_added(GstElement *element, GstPad *pad, void *data)
{
  GstPad *sinkpad;
  GstElement *decode = data;

  sinkpad = gst_element_get_static_pad(decode, "sink");

  gst_pad_link(pad, sinkpad);

  gst_object_unref(sinkpad);
}

