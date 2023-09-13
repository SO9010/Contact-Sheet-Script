/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2023 Samuel Oldham
 * Contact sheet plug-in (C) 2023 Samuel Oldham
 * e-mail: so9010sami@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * This plug-in generates a contact sheet from a selected directory
 */
// ADD ABITLIY TO CHANGE DPI 
// ADD ABILITY TO ADD EDGES
//fix gegl leaks

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <gexiv2/gexiv2.h>

#define PLUG_IN_PROC        "plug-in-contactsheet"
#define PLUG_IN_BINARY      "contactsheet"
#define PLUG_IN_ROLE        "gimp-contactsheet"

#define NAME_LEN            256

/* Variables to set in dialog box */
typedef struct
{
  gint            sheet_res;              /* Resolution of the sheet */
  gdouble         sheet_width;            /* Width of the sheet */
  gdouble         sheet_height;           /* Height of the sheet */
  GimpUnit        w_h_type;               /* Measure type of the width and height*/
  gdouble         gap_vert, gap_horiz;    /* Vertical and horizontal gaps between thumbnails */
  GimpUnit        vg_hg_type;             /* Measure type of the width and height*/
  gint            row, column;            /* Number of rows and columns */
  gboolean        rotate_images;          /* Rotate the thumbnails to horizontal */
  gchar           file_prefix[NAME_LEN];  /* Name of the file to be made */
  gboolean        flatten;                /* Flatten all layers */
  gchar           fontname[NAME_LEN];               /* Sheet font */
  gdouble         caption_size;           /* Caption size, initially pt */
  GimpUnit        cs_type;                /* Caption size measure type */

  /* Where the files are */
  gchar     file_dir_tree[NAME_LEN];/* Holds the current directory image, with out the image name*/
  
  /* List of boolean values for whether the user wants them to be displayed */
  gboolean        file_name;
  gboolean        aperture;
  gboolean        focal_length;
  gboolean        ISO;
  gboolean        exposure;

} SheetVals;

// Declare local functions
static void       query               (void);
static void       run                 (const gchar      *name,
                                       gint              nparams,
                                       const GimpParam  *param,
                                       gint             *nreturn_vals,
                                       GimpParam       **return_vals);

static gboolean   is_image_file       (GFile *file);

static gint32     add_image           (const gchar    *file,
                                       guint32        *image_ID_dst,
                                       guint32        *layer_ID,
                                       gint            dst_width, 
                                       gint            dst_height);

static gint32     add_caption         (const gchar    *file_dir,
                                       guint32 *image_ID_dst,
                                       guint32 *layer_ID,
                                       gint     dst_width);

static gint32     create_new_image    (guint           file_num,
                                       guint           width,
                                       guint           height,
                                       gint32         *layer_ID);

static gboolean   contact_sheet_dialog(gint32                image_ID);

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,
  NULL,
  query,
  run
};

static gchar *filename = "";               /* Holds the filename*/ 
/* Values when first invoked */
static SheetVals sheetvals =
{
  300,
  11.7,            /* Width of the sheet */
  8.3,            /* Height of the sheet */
  GIMP_UNIT_INCH,
  0.014, 0.014,           /* Vertical and horizontal gaps between thumbnails */
  GIMP_UNIT_INCH,
  5, 6,           /* Number of rows and columns */
  FALSE,           /* Rotate the thumbnails to horizontal */
  "Untitled",     /* Name of the file to be made */
  TRUE,           /* Flatten all layers */
  "Sans-serif",   /* Sheet font */
  6,              /* Caption size, initially pt */
  GIMP_UNIT_POINT,
  
  "~/Pictures",

  TRUE,
  TRUE,
  TRUE,
  TRUE,
  TRUE
};


static gint sheet_number = 0;

MAIN()

static void
query (void)
{
  static const GimpParamDef args[] =
  {
 
    { GIMP_PDB_INT32,    "run-mode",     "The run mode { RUN-INTERACTIVE (0), RUN-NONINTERACTIVE (1) }" },
    { GIMP_PDB_IMAGE,    "image",        "Input image (unused)" },
    { GIMP_PDB_DRAWABLE, "drawable",     "Input drawable" },

    { GIMP_PDB_FLOAT,    "sheet-res",     "Resolution of the sheet" },
    { GIMP_PDB_FLOAT,    "sheet-width",   "Contact sheet Width" },
    { GIMP_PDB_FLOAT,    "sheet-height",  "Contact sheet height" },
    { GIMP_PDB_INT32,    "vg-hg-type",    "Data type for width and height, {PIXEL (0), INCH (1), MM (2), POINT (3)}" },
    { GIMP_PDB_FLOAT,    "gap-vert",      "Vertical gaps between images" },
    { GIMP_PDB_FLOAT,    "gap-horiz",     "Horizontal gaps between images" },
    { GIMP_PDB_INT32,    "vg-hg-type",    "Data type for gaps, {PIXEL (0), INCH (1), MM (2), POINT (3)}" },
    { GIMP_PDB_INT32,    "row",           "Number of rows" },
    { GIMP_PDB_INT32,    "column",        "Number of columns" },
    { GIMP_PDB_INT32,    "rotate-images", "Rotate to horizontal { FALSE (0), TRUE (1) }" },
    { GIMP_PDB_INT32,    "flatten",       "Flatten to one layer { FALSE (0), TRUE (1) }" },
    { GIMP_PDB_STRING,   "fontname",      "Font name for the whole sheet" },
    { GIMP_PDB_FLOAT,    "caption-size",  "Size of the captions" },
    { GIMP_PDB_INT32,    "cs-type",       "Data type for font, {PIXEL (0), INCH (1), MM (2), POINT (3)}" },

    { GIMP_PDB_STRING,   "file-dir-tree", "File directory to the folder containing the files" },

    { GIMP_PDB_INT32,    "file-name",     "Show file name { FALSE (0), TRUE (1) }" },
    { GIMP_PDB_INT32,    "aperture",      "show aperture { FALSE (0), TRUE (1) }" },
    { GIMP_PDB_INT32,    "focal-length",  "Show focal length { FALSE (0), TRUE (1) }" },
    { GIMP_PDB_INT32,    "ISO",           "Show ISO speed { FALSE (0), TRUE (1) }" },
    { GIMP_PDB_INT32,    "exposure",      "Show exposure time { FALSE (0), TRUE (1) }" },
  };  
  
  static const GimpParamDef return_vals[] =
  {
    { GIMP_PDB_IMAGE, "new-image", "Output image" }
  };

  gimp_install_procedure (
    PLUG_IN_PROC,
    "Contact sheet creator",
    "Creates a contact sheet made up of cells which contain the thumbnail and information about the image",
    "Samuel Oldham",
    "Samuel Oldham",
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
  
  GimpRunMode run_mode = param[0].data.d_int32;

  *nreturn_vals = 1;
  *return_vals  = values;
  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type          = GIMP_PDB_IMAGE;

  switch (run_mode)
  {
    case GIMP_RUN_INTERACTIVE:
      /* Try and get data*/
      gimp_get_data (PLUG_IN_PROC, &sheetvals);

      /*  First acquire information with a dialog  */
      if (! contact_sheet_dialog (param[1].data.d_int32))
      {
        return;
      }
    break;
    
    case GIMP_RUN_NONINTERACTIVE:
      status = GIMP_PDB_CALLING_ERROR;
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

      const gdouble sheet_width = gimp_units_to_pixels (sheetvals.sheet_width, sheetvals.w_h_type, sheetvals.sheet_res);
      const gdouble sheet_height = gimp_units_to_pixels (sheetvals.sheet_height, sheetvals.w_h_type, sheetvals.sheet_res);
      
      const gdouble gap_vert = gimp_units_to_pixels (sheetvals.gap_vert, sheetvals.vg_hg_type, sheetvals.sheet_res);
      const gdouble gap_horiz = gimp_units_to_pixels (sheetvals.gap_horiz, sheetvals.vg_hg_type, sheetvals.sheet_res);
      const gint tmp_row = sheetvals.row;
      const gint tmp_col = sheetvals.column;

      cell_width = (sheet_width - (gap_vert * (tmp_col + 1))) / tmp_col;
      cell_height = (sheet_height - (gap_horiz* (tmp_row + 1))) / tmp_row;

      // add to the background.
      gint32            image_ID_src, image_ID_dst, layer_ID_src, layer_ID_dst;
      image_ID_dst = create_new_image (sheet_number,
                                   (guint) sheet_width, (guint) sheet_height,
                                   &layer_ID_dst);

      // Itterate through directory

      files = g_dir_open (sheetvals.file_dir_tree, 0, NULL);

      filename = g_dir_read_name (files);

      gint offset_x = gap_vert;
      gint offset_y = gap_horiz;

      gint number_x = 0;
      gint number_y = 0;

      while (filename != NULL) 
      {
        gchar* filed = g_build_filename(sheetvals.file_dir_tree, filename, NULL);
        GFile *file = g_file_new_for_path(filed);

        if (is_image_file(file)) 
        {
          gint32 added_caption;
          gint32 added_image;
          
          if (sheetvals.file_name || sheetvals.aperture || sheetvals.focal_length || sheetvals.ISO || sheetvals.exposure)
          {
            added_caption = add_caption (filed,
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
            added_image = add_image (filed,
                      &image_ID_dst,
                      &layer_ID_dst,
                      cell_width,
                      cell_height);
          }

          gimp_item_transform_translate (added_image,
                                         offset_x,
                                         offset_y);
          
          number_x++;
          offset_x += (cell_width + gap_vert);
          if (number_x == sheetvals.column){
            offset_x = gap_vert;
            offset_y += (cell_height + gap_horiz);
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

            if(sheetvals.file_dir_tree[0] != '~' && number_x > 0){
              gimp_display_new (image_ID_dst);
              gimp_image_delete(image_ID_dst);
            }
            else{
              g_message("No Images here!\n");
            }
            sheet_number++;
   
            image_ID_dst = create_new_image (sheet_number,
                                   (guint) sheet_width, (guint) sheet_height,
                                   &layer_ID_dst);

            offset_x = gap_vert;
            offset_y = gap_horiz;
            number_x = 0;
            number_y = 0;
          }
        }
        filename = g_dir_read_name (files);
        filed = g_strdup(sheetvals.file_dir_tree);
        g_object_unref(file);
      }
      if (sheetvals.flatten)
      {
        gimp_image_flatten (image_ID_dst);
      }
      gimp_image_undo_enable (image_ID_dst);

      if(sheetvals.file_dir_tree[0] != '~' && number_x > 0){
        gimp_display_new (image_ID_dst);
        gimp_image_delete(image_ID_dst);
      }
      else{
        g_message("No Images here!\n");
      }

      if (run_mode == GIMP_RUN_INTERACTIVE){
        gimp_set_data (PLUG_IN_PROC, &sheetvals, sizeof (SheetVals));
      }
    }
    
  values[0].data.d_status = status;
}

static gboolean
is_image_file(GFile *file)
{
  GFileInfo *file_info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, NULL, NULL);

  if (file_info != NULL)
  {
      const gchar *content_type = g_file_info_get_content_type(file_info);

      // Check if the content type is an image type
      if (g_content_type_is_a(content_type, "image/*"))
      {
          // Process the image file
          return TRUE;
      }

      g_object_unref(file_info);
  }
  return FALSE;
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

  gdouble aspect_ratio = (gdouble)dst_width / gimp_drawable_width (*layer_ID);

  if ((gimp_drawable_height (*layer_ID) * aspect_ratio) > dst_height){
    aspect_ratio = (gdouble)dst_height / gimp_drawable_height (*layer_ID);

    gimp_layer_scale(*layer_ID,
                gimp_drawable_width (*layer_ID) * aspect_ratio,
                gimp_drawable_height (*layer_ID) * aspect_ratio,
                FALSE);
  }
  else
  {

    gimp_layer_scale(*layer_ID,
                dst_width,
                gimp_drawable_height (*layer_ID) * aspect_ratio,
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

static gint32
add_caption (const gchar    *file_dir,
             guint32 *image_ID_dst,
             guint32 *layer_ID,
             gint     dst_width)
{  
  GExiv2Metadata *metadata;
  const gchar *caption = "";
  gdouble f_number;
  gdouble focal_length;
  gint    iso_speed;
  gint exposure_time_nom, exposure_time_dom;

  const gdouble caption_size = gimp_units_to_pixels (sheetvals.caption_size, sheetvals.cs_type, sheetvals.sheet_res);

  metadata = gexiv2_metadata_new();
  
  gexiv2_metadata_open_path (metadata,
                           file_dir,
                           NULL);
  
  f_number = gexiv2_metadata_try_get_fnumber (metadata, NULL);
  focal_length = gexiv2_metadata_try_get_focal_length (metadata, NULL);
  iso_speed = gexiv2_metadata_try_get_iso_speed (metadata, NULL);
  
  
  char captionBuffer[256];  // Adjust the buffer size as needed
  if (sheetvals.file_name) {
    snprintf(captionBuffer, sizeof(captionBuffer), "%s - ", filename);
  }
  if (f_number != -1 && sheetvals.aperture) {
      snprintf(captionBuffer + strlen(captionBuffer), sizeof(captionBuffer) - strlen(captionBuffer),
                "f/%.2g, ", f_number);
  }
  if (focal_length != -1 && sheetvals.focal_length) {
      snprintf(captionBuffer + strlen(captionBuffer), sizeof(captionBuffer) - strlen(captionBuffer),
                "%.2gmm, ", focal_length);
  }
  if (iso_speed != 0 && sheetvals.ISO) {
      snprintf(captionBuffer + strlen(captionBuffer), sizeof(captionBuffer) - strlen(captionBuffer),
                "%dISO, ", iso_speed);
  }
  if (gexiv2_metadata_try_get_exposure_time (metadata, &exposure_time_nom, &exposure_time_dom, NULL)  && sheetvals.exposure ) {
      snprintf(captionBuffer + strlen(captionBuffer), sizeof(captionBuffer) - strlen(captionBuffer),
                "%d/%ds, ", exposure_time_nom, exposure_time_dom);
  }

  // Remove the trailing comma and space
  size_t captionLength = strlen(captionBuffer);
  if (captionLength >= 2) {
      captionBuffer[captionLength - 2] = '\0';
  }

  // Set the caption
  caption = g_strdup(captionBuffer);


  *layer_ID = gimp_text_layer_new (*image_ID_dst,
                     caption,
                     sheetvals.fontname,
                     caption_size,
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
  g_free (caption);
  g_object_unref (metadata);
  return *layer_ID;
}

// Create an image, set layer_ID, drawable and rgn, returns the image_ID REWRTIE!!

static gint32
create_new_image (guint           file_num,
                  guint           width,
                  guint           height,
                  gint32         *layer_ID)
{
  gint32            image_ID;
  gimp_context_push ();
  gimp_context_set_background (&(GimpRGB){1.0, 1.0, 1.0, 1.0});
  image_ID = gimp_image_new (width, height, GIMP_RGB);


  gimp_image_set_filename (image_ID, g_strdup_printf("%s_%d", sheetvals.file_prefix, (gchar)file_num));

  gimp_image_undo_disable (image_ID);

  *layer_ID = gimp_layer_new(image_ID, "Background", width, height,
                             GIMP_RGB,
                             100,
                             gimp_image_get_default_new_layer_mode (image_ID));

  gimp_drawable_fill(*layer_ID, GIMP_FILL_BACKGROUND);

  gimp_image_insert_layer (image_ID, *layer_ID, -1, 0);
  
  return image_ID;
}

//GUI, cahnge the way this is done in order to have it do it in real time, so then you can reuse the widghets, also make it so it isd in multiple functions

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
  GtkWidget       *sheet_res;
  GtkWidget       *file_entry;
  GtkWidget       *prefix;
  GtkWidget       *grid;
  GtkWidget       *caption_check_box;
  GtkWidget       *check_box;
  GtkWidget       *font_button;
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
  if(sheetvals.file_dir_tree[0] != '~'){
    gtk_file_chooser_set_current_folder(file_entry, sheetvals.file_dir_tree);
  }
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

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (width), sheetvals.w_h_type);

  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (width), 0, sheetvals.sheet_res, TRUE);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (width), 1, sheetvals.sheet_res, TRUE);

  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (width), 0, sheetvals.sheet_width);
  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (width), 1, sheetvals.sheet_height);

  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (width), "Width",
                                0, 1, 0.0);
  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (width), "Height",
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

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (gap), sheetvals.vg_hg_type);

  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (gap), 0, sheetvals.sheet_res, TRUE);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (gap), 1, sheetvals.sheet_res, TRUE);

  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (gap), 0, sheetvals.gap_vert);
  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (gap), 1, sheetvals.gap_horiz);

  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (gap), "Vertical",
                                0, 1, 0.0);
  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (gap), "Horizontal",
                                0, 2, 0.0);
  label = gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (gap), "Gaps :",
        1, 0, 0.0);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  // Rows and columns
  row_column = gimp_size_entry_new (2,                        /*  number_of_fields  */
                               unit,                          /*  unit              */
                               "%a",                          /*  unit_format       */
                               TRUE,                          /*  menu_show_pixels  */
                               FALSE,                         /*  menu_show_percent */
                               FALSE,                         /*  show_refval       */
                               8,                             /*  spinbutton_usize  */
                               GIMP_SIZE_ENTRY_UPDATE_SIZE);  /*  update_policy     */

  gtk_box_pack_start (GTK_BOX (hbox), row_column, FALSE, FALSE, 0);
  gtk_widget_show (row_column);

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (row_column), GIMP_UNIT_PIXEL);
  gimp_size_entry_show_unit_menu (GIMP_SIZE_ENTRY (row_column), FALSE);

  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (row_column), 0, sheetvals.column);
  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (row_column), 1, sheetvals.row);

  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (row_column), "Columns",
                                0, 1, 0.0);
  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (row_column), "Rows",
                                0, 2, 0.0);

  // Auto rotate
  check_box = gtk_check_button_new_with_mnemonic("Rotate to fit");
  gtk_widget_show(check_box);
  gtk_toggle_button_set_active(check_box, sheetvals.rotate_images);
  gtk_box_pack_start (GTK_BOX (hbox), check_box, FALSE, FALSE, 0);

  g_signal_connect (check_box, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &sheetvals.rotate_images);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  check_box = gtk_check_button_new_with_mnemonic("File name");
  gtk_widget_show(check_box);
  gtk_toggle_button_set_active(check_box, sheetvals.file_name);
  gtk_box_pack_start (GTK_BOX (hbox), check_box, FALSE, FALSE, 0);
  g_signal_connect (check_box, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &sheetvals.file_name);

  check_box = gtk_check_button_new_with_mnemonic("aperture");
  gtk_widget_show(check_box);
  gtk_toggle_button_set_active(check_box, sheetvals.aperture);
  gtk_box_pack_start (GTK_BOX (hbox), check_box, FALSE, FALSE, 0);
  g_signal_connect (check_box, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &sheetvals.aperture);

  check_box = gtk_check_button_new_with_mnemonic("Focal Length");
  gtk_widget_show(check_box);
  gtk_toggle_button_set_active(check_box, sheetvals.focal_length);
  gtk_box_pack_start (GTK_BOX (hbox), check_box, FALSE, FALSE, 0);
  g_signal_connect (check_box, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &sheetvals.focal_length);

  check_box = gtk_check_button_new_with_mnemonic("ISO");
  gtk_widget_show(check_box);
  gtk_toggle_button_set_active(check_box, sheetvals.ISO);
  gtk_box_pack_start (GTK_BOX (hbox), check_box, FALSE, FALSE, 0);
  g_signal_connect (check_box, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &sheetvals.ISO);

  check_box = gtk_check_button_new_with_mnemonic("Exposure");
  gtk_widget_show(check_box);
  gtk_toggle_button_set_active(check_box, sheetvals.exposure);
  gtk_box_pack_start (GTK_BOX (hbox), check_box, FALSE, FALSE, 0);
  g_signal_connect (check_box, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &sheetvals.exposure);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

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

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (caption_text_size), sheetvals.cs_type);

  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (caption_text_size), 0, sheetvals.sheet_res, TRUE);

  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (caption_text_size), 0, sheetvals.caption_size);

  label = gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (caption_text_size), "Text size :",
        1, 0, 0.0);

  /*  Caption text size entry  */
  sheet_res = gimp_size_entry_new (1,                            /*  number_of_fields  */
                               unit,                         /*  unit              */
                               "%a",                         /*  unit_format       */
                               TRUE,                         /*  menu_show_pixels  */
                               FALSE,                         /*  menu_show_percent */
                               FALSE,                        /*  show_refval       */
                               8,            /*  spinbutton_usize  */
                               GIMP_SIZE_ENTRY_UPDATE_SIZE); /*  update_policy     */

  gtk_box_pack_start (GTK_BOX (hbox), sheet_res, FALSE, FALSE, 0);
  gtk_widget_show (sheet_res);

  gimp_size_entry_show_unit_menu (GIMP_SIZE_ENTRY (sheet_res), FALSE);
  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (sheet_res), GIMP_UNIT_PIXEL);

  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (sheet_res), 0, sheetvals.sheet_res);

  label = gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (sheet_res), "Sheet Resolution :",
        1, 0, 0.0);


  // Flatten image toggle
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  check_box = gtk_check_button_new_with_mnemonic("Flatten");
  gtk_widget_show(check_box);

  gtk_box_pack_start (GTK_BOX (hbox), check_box, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(check_box, sheetvals.flatten);
  g_signal_connect (check_box, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &sheetvals.flatten);

  //File name prefix entry option
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  label = gtk_label_new("Prefix: ");
  prefix = gtk_entry_new();
  gtk_entry_set_text(prefix, sheetvals.file_prefix);

  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), prefix, FALSE, FALSE, 0);
  gtk_widget_show (label);
  gtk_widget_show (prefix);

  // run
  run = (gimp_dialog_run (GIMP_DIALOG (dlg)) == GTK_RESPONSE_OK);
  if (run)
    {
      
      if (gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(file_entry)) != NULL)
      {
        strcpy(sheetvals.file_dir_tree, gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(file_entry)));
      }

      sheetvals.sheet_res = 
        gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (sheet_res), 0);

      sheetvals.w_h_type = 
        gimp_size_entry_get_unit(GIMP_SIZE_ENTRY (width)); 
      sheetvals.sheet_width =
        gimp_size_entry_get_value (GIMP_SIZE_ENTRY (width), 0);
      sheetvals.sheet_height =
        gimp_size_entry_get_value (GIMP_SIZE_ENTRY (width), 1);

      sheetvals.vg_hg_type = 
        gimp_size_entry_get_unit(GIMP_SIZE_ENTRY (gap)); 
      sheetvals.gap_vert =
        gimp_size_entry_get_value (GIMP_SIZE_ENTRY (gap), 0);
      sheetvals.gap_horiz =
        gimp_size_entry_get_value (GIMP_SIZE_ENTRY (gap), 1);

      sheetvals.column =
        gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (row_column), 0);
      sheetvals.row =
        gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (row_column), 1);

      sheetvals.cs_type = 
        gimp_size_entry_get_unit(GIMP_SIZE_ENTRY (caption_text_size)); 
      sheetvals.caption_size = 
        gimp_size_entry_get_value (GIMP_SIZE_ENTRY (caption_text_size), 0);

      strcpy(sheetvals.file_prefix, gtk_entry_get_text(prefix));
    }

  gtk_widget_destroy (dlg);
 return run;
}
