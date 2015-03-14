#ifdef HAVE_CONFIG
# include "config.h"
#endif

#include "Elm_Code.h"

#include "elm_code_private.h"

static Elm_Code_Line *_elm_code_file_line_blank_create(Elm_Code_File *file, int line, void *data)
{
   Elm_Code_Line *ecl;

   ecl = calloc(1, sizeof(Elm_Code_Line));
   if (!ecl) return NULL;

   ecl->file = file;
   ecl->number = line;
   ecl->status = ELM_CODE_STATUS_TYPE_DEFAULT;
   ecl->data = data;

   return ecl;
}

static void _elm_code_file_line_insert_data(Elm_Code_File *file, const char *content, unsigned int length,
                                            unsigned int row, Eina_Bool mapped, void *data)
{
   Elm_Code_Line *line, *after;

   line = _elm_code_file_line_blank_create(file, row, data);
   if (!line) return;

   if (mapped)
     {
        line->content = content;
        line->length = length;
     }
   else
     {
        line->modified = malloc(sizeof(char)*(length+1));
        strncpy(line->modified, content, length);
        line->modified[length] = 0;
        line->length = length;
     }
   line->unicode_length = elm_code_text_unicode_strlen(content, length);

   if (row == 1)
     file->lines = eina_list_prepend(file->lines, line);
   else if (row == eina_list_count(file->lines) + 1)
     file->lines = eina_list_append(file->lines, line);
   else
     {
        after = eina_list_nth(file->lines, row - 2);
        file->lines = eina_list_append_relative(file->lines, line, after);
     }

   if (file->parent)
     {
        _elm_code_parse_line(file->parent, line);
        elm_code_callback_fire(file->parent, &ELM_CODE_EVENT_LINE_LOAD_DONE, line);
     }
}

EAPI Elm_Code_File *elm_code_file_new(Elm_Code *code)
{
   Elm_Code_File *ret;

   if (code->file)
     elm_code_file_free(code->file);

   ret = calloc(1, sizeof(Elm_Code_File));
   code->file = ret;
   ret->parent = code;

   return ret;
}

EAPI Elm_Code_File *elm_code_file_open(Elm_Code *code, const char *path)
{
   Elm_Code_File *ret;
   Eina_File *file;
   Eina_File_Line *line;
   Eina_Iterator *it;
   unsigned int lastindex;

   ret = elm_code_file_new(code);
   file = eina_file_open(path, EINA_FALSE);
   ret->file = file;
   lastindex = 1;

   ret->map = eina_file_map_all(file, EINA_FILE_POPULATE);
   it = eina_file_map_lines(file);
   EINA_ITERATOR_FOREACH(it, line)
     {
        Elm_Code_Line *ecl;

        /* Working around the issue that eina_file_map_lines does not trigger an item for empty lines */
        while (lastindex < line->index - 1)
          {
             ecl = _elm_code_file_line_blank_create(ret, ++lastindex, NULL);
             if (!ecl) continue;

             ret->lines = eina_list_append(ret->lines, ecl);
          }

        _elm_code_file_line_insert_data(ret, line->start, line->length, lastindex = line->index, EINA_TRUE, NULL);
     }
   eina_iterator_free(it);

   if (ret->parent)
     {
        _elm_code_parse_file(ret->parent, ret);
        elm_code_callback_fire(ret->parent, &ELM_CODE_EVENT_FILE_LOAD_DONE, ret);
     }
   return ret;
}

EAPI void elm_code_file_free(Elm_Code_File *file)
{
   Elm_Code_Line *l;

   EINA_LIST_FREE(file->lines, l)
     {
        elm_code_line_free(l);
     }

   if (file->file)
     {
        if (file->map)
          eina_file_map_free(file->file, file->map);

        eina_file_close(file->file);
     }
   free(file);
}

EAPI void elm_code_file_close(Elm_Code_File *file)
{
   eina_file_close(file->file);
}

EAPI const char *elm_code_file_filename_get(Elm_Code_File *file)
{
   return basename((char *)eina_file_filename_get(file->file));
}

EAPI const char *elm_code_file_path_get(Elm_Code_File *file)
{
   return eina_file_filename_get(file->file);
}

EAPI void elm_code_file_clear(Elm_Code_File *file)
{
   Elm_Code_Line *l;

   EINA_LIST_FREE(file->lines, l)
     {
        elm_code_line_free(l);
     }

   if (file->parent)
     elm_code_callback_fire(file->parent, &ELM_CODE_EVENT_FILE_LOAD_DONE, file);
}

EAPI unsigned int elm_code_file_lines_get(Elm_Code_File *file)
{
   return eina_list_count(file->lines);
}


EAPI void elm_code_file_line_append(Elm_Code_File *file, const char *line, int length, void *data)
{
   int row;

   row = elm_code_file_lines_get(file) + 1;
   _elm_code_file_line_insert_data(file, line, length, row, EINA_FALSE, data);
}

EAPI void elm_code_file_line_insert(Elm_Code_File *file, unsigned int row, const char *line, int length, void *data)
{
   Eina_List *item;
   Elm_Code_Line *line_item;
   unsigned int r;

   _elm_code_file_line_insert_data(file, line, length, row, EINA_FALSE, data);

   r = row;
   EINA_LIST_FOREACH(file->lines, item, line_item)
     {
        if (line_item->number < row)
          continue;

        line_item->number = r++;

        if (file->parent)
          elm_code_callback_fire(file->parent, &ELM_CODE_EVENT_LINE_LOAD_DONE, line_item);
     }
}

EAPI void elm_code_file_line_remove(Elm_Code_File *file, unsigned int row)
{
   Eina_List *item, *next;
   Elm_Code_Line *line_item, *tofree = NULL;
   unsigned int r;

   r = row;
   EINA_LIST_FOREACH_SAFE(file->lines, item, next, line_item)
     {
        if (line_item->number < row)
          continue;
        else if (line_item->number == row)
          {
             tofree = line_item;
             file->lines = eina_list_remove_list(file->lines, item);
             continue;
          }

        line_item->number = r++;

        if (file->parent)
          elm_code_callback_fire(file->parent, &ELM_CODE_EVENT_LINE_LOAD_DONE, line_item);
     }

   if (tofree)
     elm_code_line_free(tofree);
}

EAPI Elm_Code_Line *elm_code_file_line_get(Elm_Code_File *file, unsigned int number)
{
   return eina_list_nth(file->lines, number - 1);
}

