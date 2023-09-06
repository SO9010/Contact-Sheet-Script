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

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <gexiv2/gexiv2.h>

#define PLUG_IN_PROC        "plug-in-contactsheet"
#define PLUG_IN_BINARY      "contactsheet"
#define PLUG_IN_ROLE        "gimp-contactsheet"

#define NAME_LEN            256
#define SHEET_RES           300

/* Variables to set in dialog box */
typedef struct
{
  gint            sheet_width;            /* Width of the sheet */
  gint            sheet_height;           /* Height of the sheet */
  gint            gap_vert, gap_horiz;    /* Vertical and horizontal gaps between thumbnails */
  GimpRGB         sheet_color;            /* Background sheet colour */
  gint            row, column;            /* Number of rows and columns */
  gboolean        rotate_images;          /* Rotate the thumbnails to horizontal */
  gchar           file_prefix[NAME_LEN];  /* Name of the file to be made */
  gboolean        flatten;                /* Flatten all layers */
  gboolean        captions;               /* Enable captions */
  const gchar    *fontname;               /* Sheet font */
  gint            caption_size;           /* Caption size, initially pt */
} SheetVals;

/* More variables to be set in dialog box*/
typedef struct
{
  const gchar     file_dir_tree[NAME_LEN];/* Holds the current directory image, with out the image name*/
  gchar          *filename;

  /* List of boolean values for wheather the user wants them to be displayed */
  gboolean        file_name;
  gboolean        aperture;
  gboolean        focal_length;
  gboolean        ISO;
  gboolean        exposure;

} ImageInfo;

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

static gint32     add_caption         (guint32 *image_ID_dst,
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

/* Values when first invoked */
static SheetVals sheetvals =
{
  256,            /* Width of the sheet */
  256,            /* Height of the sheet */
  4, 4,           /* Vertical and horizontal gaps between thumbnails */
  {1, 1, 1, 1},   /* Background sheet colour */
  5, 4,           /* Number of rows and columns */
  TRUE,           /* Rotate the thumbnails to horizontal */
  "Untitled",     /* Name of the file to be made */
  TRUE,           /* Flatten all layers */
  TRUE,           /* Enable captions */
  "Sans-serif",   /* Sheet font */
  16              /* Caption size, initially pt */
};

static ImageInfo imageinfo =
{
  "/tmp",
  NULL,

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
  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32,    "run-mode",     "The run mode { RUN-INTERACTIVE (0), RUN-NONINTERACTIVE (1) }" },
    { GIMP_PDB_IMAGE,    "image",        "Input image (unused)" },
    { GIMP_PDB_DRAWABLE, "drawable",     "Input drawable" },
    { GIMP_PDB_INT32,    "sheet-height", "Contact sheet height" },
    { GIMP_PDB_INT32,    "sheet-width",  "Contact sheet Width" },
    { GIMP_PDB_INT32,    "vertical-gap", "Hertical gaps between images" },
    { GIMP_PDB_INT32,    "horizontal-gap","Horizontal gaps between images" },
    { GIMP_PDB_COLOR,    "sheet-color",  "Background colour of the sheet" },
    { GIMP_PDB_INT32,    "row",          "Number of rows" },
    { GIMP_PDB_INT32,    "column",       "Number of columns" },
    { GIMP_PDB_INT32,    "rotate-images","Rotate to horizontal { FALSE (0), TRUE (1) }" },
    { GIMP_PDB_STRING,   "file_prefix",  "Prefix of the file name" },
    { GIMP_PDB_INT32,    "flatten",      "Flatten to one layer { FALSE (0), TRUE (1) }" },
    { GIMP_PDB_INT32,    "enable-captions","Enable captions { FALSE (0), TRUE (1) }" },
    { GIMP_PDB_STRING,   "fontname",     "Font name for the whole sheet" },
    { GIMP_PDB_INT32,    "caption-size", "Size of the captions" },

    { GIMP_PDB_STRING,   "file-dir",     "Directory tree for the file" },
    { GIMP_PDB_INT32,    "file-name",    "Show file name { FALSE (0), TRUE (1) }" },
    { GIMP_PDB_INT32,    "aperture",     "show aperture { FALSE (0), TRUE (1) }" },
    { GIMP_PDB_INT32,    "focal-length", "Show focal length { FALSE (0), TRUE (1) }" },
    { GIMP_PDB_INT32,    "ISO",          "Show ISO speed { FALSE (0), TRUE (1) }" },
    { GIMP_PDB_INT32,    "exposre",      "Show expsoure time { FALSE (0), TRUE (1) }" },
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
    
  INIT_I18N ();

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
      /* Try and get data*/
      gimp_get_data (PLUG_IN_PROC, &sheetvals);

      /*  First acquire information with a dialog  */
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

      imageinfo.filename = g_dir_read_name (files);

      gint offset_x = sheetvals.gap_vert;
      gint offset_y = sheetvals.gap_horiz;

      gint number_x = 0;
      gint number_y = 0;

      while (filename != NULL) 
      {
        gchar* filed = g_build_filename(imageinfo.file_dir_tree, imageinfo.filename, NULL);
        GFile *file = g_file_new_for_path(filed);

        if (is_image_file(file)) 
        {
          gint32 added_caption;
          gint32 added_image;
          
          if (sheetvals.captions)
          {
            added_caption = add_caption (imageinfo.filename,
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

static gint32
add_caption (const gchar    *file_dir,
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
  
  gexiv2_metadata_open_path (metadata,
                           file_dir,
                           NULL);
  
  f_number = gexiv2_metadata_try_get_fnumber (metadata, NULL);
  focal_length = gexiv2_metadata_try_get_focal_length (metadata, NULL);
  iso_speed = gexiv2_metadata_try_get_iso_speed (metadata, NULL);
  
  gexiv2_metadata_try_get_exposure_time (metadata,
                                       &exposure_time_nom,
                                       &exposure_time_dom,
                                       NULL);

  char captionBuffer[256];  // Adjust the buffer size as needed
  if (imageinfo.file_name) {
    snprintf(captionBuffer, sizeof(captionBuffer), "%s - ", imageinfo.filename);
  }
  if (f_number >= 0 && imageinfo.appeture) {
      snprintf(captionBuffer + strlen(captionBuffer), sizeof(captionBuffer) - strlen(captionBuffer),
                "f/%.2g, ", f_number);
  }
  if (focal_length > 1 && imageinfo.focal_length) {
      snprintf(captionBuffer + strlen(captionBuffer), sizeof(captionBuffer) - strlen(captionBuffer),
                "%.2gmm, ", focal_length);
  }
  if (iso_speed > 1 && imageinfo.ISO) {
      snprintf(captionBuffer + strlen(captionBuffer), sizeof(captionBuffer) - strlen(captionBuffer),
                "%d, ", iso_speed);
  }
  if (exposure_time_nom > 0 && exposure_time_dom > 0 && imageinfo.exposure) {
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
create_new_image (guint           file_num,
                  guint           width,
                  guint           height,
                  gint32         *layer_ID)
{
  gint32            image_ID;
  gimp_context_push ();
  gimp_context_set_background (&sheetvals.sheet_color);
  image_ID = gimp_image_new (width, height, GIMP_RGB);


  gimp_image_set_filename (image_ID, g_strdup_printf("%s_%d", sheetvals.file_prefix, (gchar)file_num));

  gimp_image_undo_disable (image_ID);

  *layer_ID = gimp_layer_new(image_ID, "Background", width, height,
                             GIMP_RGB,
                             100,
                             gimp_image_get_default_new_layer_mode (image_ID));

  gimp_drawable_fill(*layer_ID, GIMP_BACKGROUND_FILL);

  gimp_image_insert_layer (image_ID, *layer_ID, -1, 0);
  
  return image_ID;
}

