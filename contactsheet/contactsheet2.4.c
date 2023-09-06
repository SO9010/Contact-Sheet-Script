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

static gint32     add_image           (guint32        *image_ID_dst,
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

      filename = g_dir_read_name (files);

      gint offset_x = sheetvals.gap_vert;
      gint offset_y = sheetvals.gap_horiz;

      gint number_x = 0;
      gint number_y = 0;

      while (filename != NULL) 
      {
        gchar* filed = g_build_filename(sheetvals.file_dir, filename, NULL);
        GFile *file = g_file_new_for_path(filed);

        if (is_image_file(file)) 
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