
#include <string.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <gexiv2/gexiv2.h>

#define PLUG_IN_PROC       "plug-in-contact-sheet"
#define PLUG_IN_BINARY     "contact-sheet"
#define PLUG_IN_ROLE       "gimp-contact-sheet"

#define NAME_LEN            256
#define SHEET_RES           300
// Define how the plugin works
// All of these will be in doubles as it will be in ppi
typedef struct
{
  gint            contact_sheet_width;
  gint            contact_sheet_height;
  gint            gap_vert, gap_horiz;
  GimpRGB         sheet_color;
  gint            row, column;
  gboolean        rotate_images;
  gchar           file_prefix[NAME_LEN];
  const gchar     file_dir[NAME_LEN];
  gboolean        flatten;
  gboolean        captions;
  const gchar    *fontname;
  gint            caption_size;
} SheetVals;

// Declare local functions
static void query (void);
static void run   (const gchar      *name,
                   gint              nparams,
                   const GimpParam  *param,
                   gint             *nreturn_vals,
                   GimpParam       **return_vals);

static gboolean  is_image_file      (const gchar   *filename);

static gint32    add_image          (const gchar    *file,
                                     guint32        *image_ID_dst,
                                     guint32        *layer_ID,
                                     gint            dst_width, 
                                     gint            dst_height);

static gint32 add_caption           (const gchar    *file_name,
                                     const gchar    *file_dir,
                                     guint32 *image_ID_dst,
                                     guint32 *layer_ID,
                                     gint     dst_width);

static gint32    create_new_image   (const gchar    *filename,
                                     guint           file_num,
                                     guint           width,
                                     guint           height,
                                     gint32         *layer_ID);

static gboolean  contact_sheet_dialog (gint32                image_ID);


GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,
  NULL,
  query,
  run
};

static SheetVals sheetvals =
{
  256, 256,              /* Width & height of sheet */
  4, 4,                   /* Gaps*/
  {1, 1, 1, 1},           /* Sheet colour */
  5, 4,                   /* Row, column */
  TRUE,                   /* Auto rotate*/
  "Untitled",
  "/tmp",
  TRUE,
  TRUE,
  "Sans-serif",
  16
};

MAIN()

static void
query (void)
{
  static GimpParamDef args[] =
  {
    {
      GIMP_PDB_INT32,
      "run-mode",
      "Run mode"
    },
    {
      GIMP_PDB_IMAGE,
      "image",
      "Input image"
    },
    {
      GIMP_PDB_DRAWABLE,
      "drawable",
      "Input drawable"
    },
    {
      GIMP_PDB_INT32,
      "contact_sheet_height",
      "Contact sheet height"
    },
    {
      GIMP_PDB_INT32,
      "contact_sheet_width",
      "Contact sheet witdth"
    },
    {
      GIMP_PDB_INT32,
      "vertical_gap",
      "Vertical gap size"
    },
    {
      GIMP_PDB_INT32,
      "horizontal_gap",
      "Horizontal gap size"
    },
    { GIMP_PDB_COLOR,
    "film-color",
    "Color of the film" },
    {
      GIMP_PDB_STRING,
      "contact_file_prefix",
      "Contact file prefix"
    }
  };

  static const GimpParamDef return_vals[] =
  {
    { GIMP_PDB_IMAGE, "new-image", "Output image" }
  };

  gimp_install_procedure (
    PLUG_IN_PROC,
    "Contact sheet creator",
    "Creates a contact sheet made up of cells which contain the thumbnail and info about the image",
    "Samuel Oldham",
    "Copyright Samuel Oldham",
    "2023",
    "Contact sheet",
    NULL,
    GIMP_PLUGIN,
    G_N_ELEMENTS (args),
    G_N_ELEMENTS (return_vals),
    args, return_vals);

  gimp_plugin_menu_register (PLUG_IN_PROC,
                             "<Image>/File/Create");
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam  values[2];
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  gint32            image_ID;
  gint              cell_width;
  gint              cell_height;
  GDir             *files;
  gchar            *filename = NULL;

  gint              sheet_number = 0;

  GimpRunMode run_mode = param[0].data.d_int32;

  *nreturn_vals = 2;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type          = GIMP_PDB_IMAGE;
  values[1].data.d_int32  = -1;

  switch (run_mode)
  {
    case GIMP_RUN_INTERACTIVE:
    gimp_get_data (PLUG_IN_PROC, &sheetvals);

      if (! contact_sheet_dialog (param[1].data.d_int32)){
          return;
        }
      break;
    
    case GIMP_RUN_NONINTERACTIVE:
      if ((nparams != 3) || (param[10].data.d_int32 < 1))
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      else
        {
          sheetvals.contact_sheet_height  = 254;
          sheetvals.contact_sheet_width   = 254;
        }
    break;

    case GIMP_RUN_WITH_LAST_VALS:
    gimp_get_data (PLUG_IN_PROC, &sheetvals);
    break;

    default:
      break;
  }
  
  if (status == GIMP_PDB_SUCCESS)
    {
      gimp_progress_init ("Composing images");

      const gint tmp_row = sheetvals.row;
      const gint tmp_col = sheetvals.column;

      cell_height = (sheetvals.contact_sheet_width - (sheetvals.gap_horiz * (tmp_row + 1))) / tmp_row;
      cell_width = (sheetvals.contact_sheet_height - (sheetvals.gap_vert * (tmp_col + 1))) / tmp_col;
      
      // add to the background.
      gint32            image_ID_src, image_ID_dst, layer_ID_src, layer_ID_dst;
      image_ID_dst = create_new_image (sheetvals.file_prefix, sheet_number,
                                   (guint) sheetvals.contact_sheet_height, (guint) sheetvals.contact_sheet_width,
                                   &layer_ID_dst);

      // Itterate through directory

      files = g_dir_open (sheetvals.file_dir, 0, NULL);

      filename = g_dir_read_name (files);

      gchar* filed = g_strdup(sheetvals.file_dir);

      gint offset_x = sheetvals.gap_vert;
      gint offset_y = sheetvals.gap_horiz;

      gint number_x = 0;
      gint number_y = 0;

      while (filename != NULL) 
      {
        strcat(filed, "/");
        strcat(filed, filename);
        if (is_image_file(filed)) 
        {
          gint32 added_caption;
          gint32 added_image;
          
          if (sheetvals.captions)
          {
            added_caption = add_caption (filename,
                                         filed,
                                         &image_ID_dst,
                                         &layer_ID_dst,
                                         cell_width);
            
            added_image = add_image (filed,
                    &image_ID_dst,
                    &layer_ID_dst,
                    cell_width,
                    cell_height - gimp_drawable_height(added_caption));

            gimp_item_transform_translate (added_caption,
                              offset_x, 
                              offset_y + gimp_drawable_height(added_image));
          }
          else
          {
            gint32 added_image = add_image (filed,
                      &image_ID_dst,
                      &layer_ID_dst,
                      cell_width,
                      cell_height);
          }

          gimp_item_transform_translate (added_image,
                    offset_x,
                    offset_y);
          

          number_x++;
          offset_x += (cell_width + sheetvals.gap_vert);
          if (number_x == sheetvals.column){
            offset_x = sheetvals.gap_vert;
            offset_y += (cell_height + sheetvals.gap_horiz);
            number_x = 0;
            number_y++;
          }
          if (number_y == sheetvals.row)
          {
            if (sheetvals.flatten)
            {
              gimp_image_flatten (image_ID_dst);
            }
            
            gimp_image_undo_enable (image_ID_dst);

            gimp_display_new (image_ID_dst);
            sheet_number++;
   
            image_ID_dst = create_new_image (sheetvals.file_prefix, sheet_number,
                                   (guint) sheetvals.contact_sheet_height, (guint) sheetvals.contact_sheet_width,
                                   &layer_ID_dst);

            offset_x = sheetvals.gap_vert;
            offset_y = sheetvals.gap_horiz;
            number_x = 0;
            number_y = 0;
          }
        }
        filename = g_dir_read_name (files);
        filed = g_strdup(sheetvals.file_dir);
      }
      if (sheetvals.flatten)
      {
        gimp_image_flatten (image_ID_dst);
      }
      gimp_image_undo_enable (image_ID_dst);

      gimp_display_new (image_ID_dst);
    }

  values[0].data.d_status = status;
}


// Get image, set layer ID returns the image_ID

static gboolean
is_image_file(const gchar   *filename) // fix me!!
{
  GError        *error;
  const gchar   *content_type;
  gboolean       is_image;

  error = NULL;
  content_type = g_content_type_guess(filename, NULL, 0, &error);

  if (error != NULL) {
    g_error_free(error);
    return FALSE;
  }

  is_image = g_content_type_is_a(content_type, "image/*");
  g_free((gpointer)content_type);

  return is_image;
}

// Loads and adds an image as a layer, scaled proportionaltly to what is needed, it will also handle rotating the image and moving it so it can then be moved into the correct position later
static gint32
add_image (const gchar    *file,
           guint32 *image_ID_dst,
           guint32 *layer_ID,
           gint     dst_width, 
           gint     dst_height)
{
  gboolean rotated = FALSE;
  *layer_ID = gimp_file_load_layer (GIMP_RUN_NONINTERACTIVE, *image_ID_dst, file);
  gimp_image_insert_layer (*image_ID_dst,
                          *layer_ID,
                          0,
                          -1);

  if (sheetvals.rotate_images){
    if (gimp_drawable_width (*layer_ID) < gimp_drawable_height (*layer_ID)){
      gimp_item_transform_rotate (*layer_ID,
                                  -1.5708,
                                  FALSE,
                                  0,
                                  0);
      rotated = TRUE;
    }
  }

  gdouble asepct_ratio = (gdouble)dst_width / gimp_drawable_width (*layer_ID);

  if ((gimp_drawable_height (*layer_ID) * asepct_ratio) > dst_height){
    asepct_ratio = (gdouble)dst_height / gimp_drawable_height (*layer_ID);

    gimp_layer_scale(*layer_ID,
                gimp_drawable_width (*layer_ID) * asepct_ratio,
                gimp_drawable_height (*layer_ID) * asepct_ratio,
                FALSE);
  }
  else
  {

    gimp_layer_scale(*layer_ID,
                dst_width,
                gimp_drawable_height (*layer_ID) * asepct_ratio,
                FALSE);

  }

  if (rotated){
      gimp_item_transform_translate (*layer_ID,
                    0,
                    gimp_drawable_height (*layer_ID));
  }
  gimp_item_transform_translate (*layer_ID,
              (dst_width - gimp_drawable_width (*layer_ID)) / 2,
              0);
  return *layer_ID;
}

/*
Samuel Oldham: GimpMetadata is a subclass of GExiv2Metadata.
Samuel Oldham: so yes, use GExiv2 API on the GimpMetadata given by the plug-in.
https://gnome.pages.gitlab.gnome.org/gexiv2/docs/
gexiv2 Reference Manual: gexiv2 Reference Manual
gexiv2 Reference Manual for GExiv2 0.14.2. The latest version of this documentation can be found online at https://gnome.pages.gitlab.gnome.org/gexiv2/docs . GExiv2 image metadata handling library
Samuel Oldham: this is true both in the 2.0 and 3.0 API. :-)*/

static gint32
add_caption (const gchar    *file_name,
            const gchar    *file_dir,
            guint32 *image_ID_dst,
            guint32 *layer_ID,
            gint     dst_width)
{  
  // Put this into a glist?
  GExiv2Metadata *metadata;
  const gchar *caption = "";
  gdouble f_number;
  gdouble focal_length;
  gint    iso_speed;
  gint exposure_time_nom, exposure_time_dom;

  metadata = gexiv2_metadata_new();
  
  f_number = gexiv2_metadata_try_get_fnumber (metadata, NULL);
  focal_length = gexiv2_metadata_try_get_focal_length (metadata, NULL);
  iso_speed = gexiv2_metadata_try_get_iso_speed (metadata, NULL);
  
  gexiv2_metadata_try_get_exposure_time (metadata,
                                       exposure_time_nom,
                                       exposure_time_dom,
                                       NULL);

  gexiv2_metadata_open_path (metadata,
                           file_dir,
                           NULL);
  
  *layer_ID = gimp_text_layer_new (*image_ID_dst,
                     g_strdup_printf("%c, appeture: f/%.2g, focal length: %.2gmm, ISO: %d, exposr: %d/%d of a sec.", file_name, f_number, focal_length, iso_speed, exposure_time_nom, exposure_time_dom),
                     sheetvals.fontname,
                     sheetvals.caption_size,
                     GIMP_UNIT_PIXEL);
 
  gimp_image_insert_layer (*image_ID_dst,
                          *layer_ID,
                          0,
                          -1);
  
  gimp_text_layer_resize (*layer_ID,
                          dst_width,
                          gimp_drawable_height(*layer_ID)
                         );

  gimp_text_layer_set_justification (*layer_ID,
                                   GIMP_TEXT_JUSTIFY_CENTER);

  return *layer_ID;
}

// Create an image, set layer_ID, drawable and rgn, returns the image_ID REWRTIE!!
static gint32
create_new_image (const gchar    *filename,
                  guint           file_num,
                  guint           width,
                  guint           height,
                  gint32         *layer_ID)
{
  gint32            image_ID;
  gimp_context_push ();
  gimp_context_set_background (&sheetvals.sheet_color);
  image_ID = gimp_image_new (width, height, GIMP_RGB);


  gimp_image_set_filename (image_ID, g_strdup_printf("%s_%d", filename, (gchar)file_num));

  gimp_image_undo_disable (image_ID);


  *layer_ID = gimp_layer_new(image_ID, "Background", width, height,
                             GIMP_RGB,
                             100,
                             gimp_image_get_default_new_layer_mode (image_ID));

  gimp_drawable_fill(*layer_ID, GIMP_BACKGROUND_FILL);

  gimp_image_insert_layer (image_ID, *layer_ID, -1, 0);
  
  return image_ID;
}


// GUI, cahnge the way this is done in order to have it do it in real time, so then you can reuse the widghets

static gboolean
contact_sheet_dialog (gint32 image_ID)
{
  GtkWidget       *dlg;
  GtkWidget       *main_vbox;
  GtkWidget       *vbox;
  GtkWidget       *hbox;
  GtkWidget       *table;
  GtkWidget       *button;
  GtkWidget       *label;
  GtkWidget       *caption_text_size;
  GtkWidget       *file_entry;
  GtkWidget       *prefix;
  GtkWidget       *grid;
  GtkWidget       *caption_check_box;
  GtkWidget       *check_box;
  gboolean         run;
  GimpUnit         unit;
  GtkWidget       *width;
  GtkWidget       *gap;
  GtkWidget       *row_column;
	GtkWidget       *spinbutton;
	GtkObject       *spinbutton_adj;
  gdouble          xres, yres;

  gimp_ui_init (PLUG_IN_BINARY, TRUE);

  dlg = gimp_dialog_new ("Contact Sheet", PLUG_IN_ROLE,
                         NULL, 0,
                         gimp_standard_help_func, PLUG_IN_PROC,

                         "_Cancel", GTK_RESPONSE_CANCEL,
                         "_OK",     GTK_RESPONSE_OK,

                         NULL); 

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dlg),
                                            GTK_RESPONSE_OK,
                                            GTK_RESPONSE_CANCEL,
                                            -1);

  gimp_window_set_transient (GTK_WINDOW (dlg));

  main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))),
                      main_vbox, TRUE, TRUE, 0);
  gtk_widget_show (main_vbox);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_box_pack_start (GTK_BOX (main_vbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  // File entry change to dir entry
  file_entry = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  gtk_widget_set_size_request(file_entry, 600, 200);

  gtk_box_pack_start (GTK_BOX (vbox), file_entry, FALSE, FALSE, 0);
  gtk_widget_show (file_entry);

  /*  The sheet size entries  */
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  width = gimp_size_entry_new (2,                            /*  number_of_fields  */
                               unit,                         /*  unit              */
                               "%a",                         /*  unit_format       */
                               TRUE,                         /*  menu_show_pixels  */
                               FALSE,                        /*  menu_show_percent */
                               FALSE,                        /*  show_refval       */
                               8,                            /*  spinbutton_usize  */
                               GIMP_SIZE_ENTRY_UPDATE_SIZE); /*  update_policy     */

  gtk_box_pack_start (GTK_BOX (hbox), width, FALSE, FALSE, 0);
  gtk_widget_show (width);

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (width), GIMP_UNIT_INCH);

  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (width), 0, SHEET_RES, TRUE);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (width), 1, SHEET_RES, TRUE);

  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (width), 0, 8.3);
  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (width), 1, 11.7);

  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (width), "Height",
                                0, 1, 0.0);
  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (width), "Width",
                                0, 2, 0.0);

  label = gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (width), "Size:",
        1, 0, 0.0);

   /*  Gaps  */
  gap = gimp_size_entry_new (2,                            /*  number_of_fields  */
                               unit,                         /*  unit              */
                               "%a",                         /*  unit_format       */
                               TRUE,                         /*  menu_show_pixels  */
                               FALSE,                         /*  menu_show_percent */
                               FALSE,                        /*  show_refval       */
                               8,            /*  spinbutton_usize  */
                               GIMP_SIZE_ENTRY_UPDATE_SIZE); /*  update_policy     */

  gtk_box_pack_start (GTK_BOX (hbox), gap, FALSE, FALSE, 0);
  gtk_widget_show (gap);

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (gap), GIMP_UNIT_INCH);

  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (gap), 0, SHEET_RES, TRUE);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (gap), 1, SHEET_RES, TRUE);

  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (gap), 0, 0.014);
  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (gap), 1, 0.014);

  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (gap), "Vertical",
                                0, 1, 0.0);
  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (gap), "Horizontal",
                                0, 2, 0.0);
  label = gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (gap), "Gaps :",
        1, 0, 0.0);
        
  /* sheet color */
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  label = gtk_label_new("Sheet color:");
  button = gimp_color_button_new ("Select Sheet Color",
                                  50, 20,
                                  &sheetvals.sheet_color,
                                  GIMP_COLOR_AREA_FLAT);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (label);
  gtk_widget_show (button);

  gimp_color_button_set_color (GIMP_COLOR_BUTTON (button), &(GimpRGB){1.0, 1.0, 1.0, 1.0});
  sheetvals.sheet_color = (GimpRGB){1.0, 1.0, 1.0, 1.0};

  g_signal_connect (button, "color-changed",
                    G_CALLBACK (gimp_color_button_get_color),
                    &sheetvals.sheet_color);

  // Rows and columns
  row_column = gimp_size_entry_new (2,                            /*  number_of_fields  */
                               unit,                         /*  unit              */
                               "%a",                         /*  unit_format       */
                               TRUE,                         /*  menu_show_pixels  */
                               FALSE,                         /*  menu_show_percent */
                               FALSE,                        /*  show_refval       */
                               8,            /*  spinbutton_usize  */
                               GIMP_SIZE_ENTRY_UPDATE_SIZE); /*  update_policy     */

  gtk_box_pack_start (GTK_BOX (hbox), row_column, FALSE, FALSE, 0);
  gtk_widget_show (row_column);

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (row_column), GIMP_UNIT_PIXEL);
  gimp_size_entry_show_unit_menu (GIMP_SIZE_ENTRY (row_column), FALSE);

  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (row_column), 0, 6);
  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (row_column), 1, 5);

  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (row_column), "Columns",
                                0, 1, 0.0);
  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (row_column), "Rows",
                                0, 2, 0.0);

  // Auto rotate
  check_box = gtk_check_button_new_with_mnemonic("Rotate to fit");
  gtk_widget_show(check_box);

  gtk_box_pack_start (GTK_BOX (hbox), check_box, FALSE, FALSE, 0);

  g_signal_connect (check_box, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &sheetvals.rotate_images);

  // Caption options toggle
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  caption_check_box = gtk_check_button_new_with_mnemonic("Captions");
  gtk_widget_show(caption_check_box);
  
  gtk_toggle_button_set_active(caption_check_box, TRUE);
  
  g_signal_connect (caption_check_box, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &sheetvals.captions);

  gtk_box_pack_start (GTK_BOX (hbox), caption_check_box, FALSE, FALSE, 0);

  /*  Caption text size entry  */
  caption_text_size = gimp_size_entry_new (1,                            /*  number_of_fields  */
                               unit,                         /*  unit              */
                               "%a",                         /*  unit_format       */
                               TRUE,                         /*  menu_show_pixels  */
                               FALSE,                         /*  menu_show_percent */
                               FALSE,                        /*  show_refval       */
                               8,            /*  spinbutton_usize  */
                               GIMP_SIZE_ENTRY_UPDATE_SIZE); /*  update_policy     */

  gtk_box_pack_start (GTK_BOX (hbox), caption_text_size, FALSE, FALSE, 0);
  gtk_widget_show (caption_text_size);

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (caption_text_size), GIMP_UNIT_POINT);

  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (caption_text_size), 0, SHEET_RES, TRUE);

  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (caption_text_size), 0, 6);

  label = gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (caption_text_size), "Text size :",
        1, 0, 0.0);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  check_box = gtk_check_button_new_with_mnemonic("Title");
  gtk_widget_show(check_box);
  gtk_widget_set_sensitive(check_box, FALSE);
  
  gtk_box_pack_start (GTK_BOX (hbox), check_box, FALSE, FALSE, 0);

  // Flatten image toggle
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  check_box = gtk_check_button_new_with_mnemonic("Flatten");
  gtk_widget_show(check_box);

  gtk_box_pack_start (GTK_BOX (hbox), check_box, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(check_box, TRUE);
  g_signal_connect (check_box, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &sheetvals.flatten);

  //File name prefix entry option
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  label = gtk_label_new("Prefix: ");
  prefix = gtk_entry_new();
  gtk_entry_set_text(prefix, "Untitled");

  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), prefix, FALSE, FALSE, 0);
  gtk_widget_show (label);
  gtk_widget_show (prefix);

  // run
  run = (gimp_dialog_run (GIMP_DIALOG (dlg)) == GTK_RESPONSE_OK);
  if (run)
    {
      strcpy(sheetvals.file_dir, gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(file_entry)));


      sheetvals.contact_sheet_width =
        gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (width), 0);
      sheetvals.contact_sheet_height =
        gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (width), 1);

      sheetvals.gap_vert =
        gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (gap), 0);
      sheetvals.gap_horiz =
        gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (gap), 1);

      sheetvals.column =
        gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (row_column), 0);
      sheetvals.row =
        gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (row_column), 1);

      sheetvals.caption_size =
        gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (caption_text_size), 0);

      strcpy(sheetvals.file_prefix, gtk_entry_get_text(prefix));
    }

  gtk_widget_destroy (dlg);
 return run;
}