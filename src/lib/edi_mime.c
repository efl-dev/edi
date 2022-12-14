#ifdef HAVE_CONFIG
# include "config.h"
#endif

#include <Efreet_Mime.h>
#include <Ecore_File.h>
#include <Eina.h>

#include "Edi.h"
#include "edi_mime.h"

#include "edi_private.h"

EAPI const char *
edi_mime_type_get(const char *path)
{
   Eina_File *f;
   const char *mime;
   char *map;
   unsigned long long len;
   Eina_Bool likely_text = EINA_TRUE;

   if (!path) return NULL;

   f = eina_file_open(path, EINA_FALSE);
   if (!f) return efreet_mime_type_get(path);

   len = eina_file_size_get(f);
   if (!len)
     {
        eina_file_close(f);
        return "text/plain";
     }

   if (len > 2048) len = 2048;

   map = eina_file_map_new(f, EINA_FILE_POPULATE, 0, len);
   if (!map)
     {
        eina_file_close(f);
        return efreet_mime_type_get(path);
     }

   for (int i = 0; i < (int) len; i++)
     {
        if (map[i] == '\0')
          {
             likely_text = EINA_FALSE;
             break;
          }
     }

   eina_file_map_free(f, map);
   eina_file_close(f);

   mime = efreet_mime_type_get(path);
   if (!strcmp(mime, "application/x-shellscript")) return mime;
   if ((mime) && (strncmp(mime, "text/", 5)) && (likely_text))
     {
        return "text/plain";
     }

   // XXX: On efreet shutdown we can return NULL here.

   return mime;
}

