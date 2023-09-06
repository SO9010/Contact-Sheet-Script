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


// this is updating for gimp 2.99
#include "config.h"

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

typedef struct _ContactSheet      ContactSheet;
typedef struct _ContactSheetClass ContactSheetClass;

struct _ContactSheet
{
  GimpPlugIn parent_instance;
};

struct _ContactSheetClass
{
  GimpPlugInClass parent_class;
};


#define CONTACTSHEET_TYPE  (contactsheet_get_type ())
#define CONTACTSHEET (obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), CONTACTSHEET_TYPE, ContactSheet))

GType                   contactsheet_get_type         (void) G_GNUC_CONST;

static GList          * contactsheet_query_procedures (GimpPlugIn           *plug_in);
static GimpProcedure  * contactsheet_create_procedure (GimpPlugIn           *plug_in,
                                                       const gchar          *name);

static GimpValueArray * contactsheet_run              (GimpProcedure        *procedure,
                                                       GimpRunMode           run_mode,
                                                       GimpImage            *image,
                                                       gint                  n_drawables,
                                                       GimpDrawable        **drawables,
                                                       const GimpValueArray *args,
                                                       gpointer              run_data);

static gboolean   is_image_file                       (GFile *file);

static gint32     add_image                           (const gchar    *file,
                                                       guint32        *image_ID_dst,
                                                       guint32        *layer_ID,
                                                       gint            dst_width, 
                                                       gint            dst_height);

static gint32     add_caption                         (const gchar    *file_name,
                                                       const gchar    *file_dir_tree,
                                                       guint32 *image_ID_dst,
                                                       guint32 *layer_ID,
                                                       gint     dst_width);

static gint32     create_new_image                    (const gchar    *filename,
                                                       guint           file_num,
                                                       guint           width,
                                                       guint           height,
                                                       gint32         *layer_ID);

static gboolean   contact_sheet_dialog                (gint32                image_ID);

G_DEFINE_TYPE (ContactSheet, contactsheet, GIMP_TYPE_PLUG_IN)

GIMP_MAIN (CONTACTSHEET_TYPE)
DEFINE_STD_SET_I18N


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

static void
contactsheet_class_init (ContactSheetClass *klass)
{
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

  plug_in_class->query_procedures = contactsheet_query_procedures;
  plug_in_class->create_procedure = contactsheet_create_procedure;
  plug_in_class->set_i18n         = STD_SET_I18N;
}

static void
contactsheet_init (ContactSheet *contactsheet)
{
}

static GList *
contactsheet_query_procedures (GimpPlugIn *plug_in)
{
  return g_list_append (NULL, g_strdup (PLUG_IN_PROC));
}

static GimpProcedure *
contactsheet_create_procedure (GimpPlugIn  *plug_in,
                          const gchar *name)
{
  GimpProcedure *procedure = NULL;

  if (! strcmp (name, PLUG_IN_PROC))
    {
      procedure = gimp_image_procedure_new (plug_in, name,
                                            GIMP_PDB_PROC_TYPE_PLUGIN,
                                            contactsheet_run, NULL, NULL);

      gimp_procedure_set_image_types (procedure, "RGB*, GRAY*");
      gimp_procedure_set_sensitivity_mask (procedure,
                                           GIMP_PROCEDURE_SENSITIVE_DRAWABLE);

      gimp_procedure_set_menu_label (procedure, _("_Sparkle..."));
      gimp_procedure_add_menu_path (procedure,
                                    "<Image>/Filters/Light and Shadow/Light");

      gimp_procedure_set_documentation (procedure,
                                        _("Turn bright spots into "
                                          "starry sparkles"),
                                        "Uses a percentage based luminoisty "
                                        "threhsold to find candidate pixels "
                                        "for adding some sparkles (spikes).",
                                        name);
    }
}