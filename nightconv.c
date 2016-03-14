#include <glib.h>
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>

static int bus_call(GstBus *bus, GstMessage *msg, void *data);
static void connect_pad(GstElement *element, GstPad *pad, void *data);
static void configure_10band_equalizer(GstElement *equalizer,
    const double *bands);
static GstElement *gst_element_or_die(const char *elemname,
    const char *localname);

static const double pitch_factor = 1.0; /* Sounds like crap when turned on. */
static const double rate_factor = 1.3;
static const double equalizer_bands[] = {
  1.0, 3.0, 3.0, 1.0, 0.0, 0.0, -1.0, -2.0, -2.0, -2.0, 0.0,
};

int
main(int argc, char **argv)
{
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  const char *infile = NULL, *outfile = NULL;

  gst_init(&argc, &argv);

  if (argc < 2) {
    g_printerr("Usage: nightcore <infile> [outfile]\n");
    exit(1);
  } else if (argc > 2) {
    outfile = argv[2];
  }

  infile = argv[1];

  GstElement *pipeline = gst_pipeline_new("mainpipe"),
             *source = gst_element_or_die("filesrc", "source"),
             *decode = gst_element_or_die("decodebin", "decode"),
             *convert1 = gst_element_or_die("audioconvert", "convert1"),
             *resample1 = gst_element_or_die("audioresample", "resample1"),
             *pitch = gst_element_or_die("pitch", "pitch"),
             *equalizer = gst_element_or_die("equalizer-10bands", "equalizer"),
             *convert2 = gst_element_or_die("audioconvert", "convert2"),
             *resample2 = gst_element_or_die("audioresample", "resample2");

  g_assert_nonnull(pipeline); /* Should never fail... */

  g_object_set(G_OBJECT(source), "location", infile, NULL);

  g_object_set(G_OBJECT(pitch), "rate", rate_factor, "pitch", pitch_factor, NULL);

  gst_bin_add_many(GST_BIN(pipeline), source, decode, convert1, resample1,
      pitch, equalizer, convert2, resample2, NULL);

  configure_10band_equalizer(equalizer, equalizer_bands);

  gst_element_link(source, decode);
  gst_element_link_many(convert1, resample1, pitch, equalizer, convert2,
      resample2, NULL);
  /*
   * decodebin doesn't know how many streams are in the file until it has
   * been read, so we have to set up the connection dynamically.
   */
  g_signal_connect(decode, "pad-added", G_CALLBACK(connect_pad), convert1);

  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  unsigned watch_id = gst_bus_add_watch(bus, bus_call, loop);
  gst_object_unref(bus);

  if (outfile) {
    GstElement *mp3enc = gst_element_or_die("lamemp3enc", "mp3enc"),
               *filesink = gst_element_or_die("filesink", "filesink");

    g_object_set(G_OBJECT(filesink), "location", outfile, NULL);

    gst_bin_add_many(GST_BIN(pipeline), mp3enc, filesink, NULL);
    gst_element_link_many(resample2, mp3enc, filesink, NULL);
  } else {
    GstElement *autosink = gst_element_or_die("autoaudiosink", "autosink");

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

/*
 * Exit program on EOS event, or on error.
 */
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

/*
 * When a pad appears on element, connect it to data.
 */
static void
connect_pad(GstElement *element, GstPad *pad, void *data)
{
  GstPad *sinkpad;
  GstElement *decode = data;

  sinkpad = gst_element_get_static_pad(decode, "sink");

  gst_pad_link(pad, sinkpad);

  gst_object_unref(sinkpad);
}

/*
 * Create a new elemname element, or die trying.
 */
static GstElement *
gst_element_or_die(const char *elemname, const char *localname)
{
  GstElement *elem = gst_element_factory_make(elemname, localname);
  if (elem)
    return elem;

  g_printerr("error: Failed to create pipeline element '%s'.\n"
             "       Do you have all the gstreamer plugins installed?\n",
             elemname);
  exit(1);
}

static void
configure_10band_equalizer(GstElement *equalizer, const double *bands)
{
  for (int i = 0; i < 10; i++ ) {
    char buffer[8];
    snprintf(buffer, 8, "band%d", i);
    g_object_set(G_OBJECT(equalizer), buffer, bands[i], NULL);
  }
}

