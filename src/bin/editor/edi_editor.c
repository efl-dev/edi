#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libgen.h>

#include <Eina.h>
#include <Elementary.h>

#include "edi_editor.h"

#include "mainview/edi_mainview.h"
#include "edi_config.h"

#include "edi_private.h"

typedef struct
{
   unsigned int line;
   unsigned int col;
} Edi_Location;

typedef struct
{
   Edi_Location start;
   Edi_Location end;
} Edi_Range;

#if HAVE_LIBCLANG
typedef struct
{
   enum CXCursorKind kind;
   char             *ret;
   char             *name;
   char             *param;
   Eina_Bool         is_param_cand;
} Suggest_Item;

static Evas_Object *_suggest_popup_bg, *_suggest_popup_genlist;
#endif

void
edi_editor_save(Edi_Editor *editor)
{
   if (!editor->modified)
     return;

   editor->save_time = time(NULL);
   edi_mainview_save();

   editor->modified = EINA_FALSE;
   ecore_timer_del(editor->save_timer);
   editor->save_timer = NULL;
}

static Eina_Bool
_edi_editor_autosave_cb(void *data)
{
   Edi_Editor *editor;

   editor = (Edi_Editor *)data;

   edi_editor_save(editor);
   return ECORE_CALLBACK_CANCEL;
}

static void
_changed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Edi_Editor *editor = data;

   editor->modified = EINA_TRUE;

   if (editor->save_timer)
     ecore_timer_reset(editor->save_timer);
   else if (_edi_config->autosave)
     editor->save_timer = ecore_timer_add(EDI_CONTENT_SAVE_TIMEOUT, _edi_editor_autosave_cb, editor);
}

#if HAVE_LIBCLANG
static char *
_edi_editor_current_word_get(Edi_Editor *editor, unsigned int row, unsigned int col)
{
   Elm_Code *code;
   Elm_Code_Line *line;
   char *ptr, *curword, *curtext;
   unsigned int curlen, wordlen;

   code = elm_code_widget_code_get(editor->entry);
   line = elm_code_file_line_get(code->file, row);

   curtext = (char *)elm_code_line_text_get(line, &curlen);
   ptr = curtext + col - 1;

   while (ptr != curtext &&
         ((*(ptr - 1) >= 'a' && *(ptr - 1) <= 'z') ||
          (*(ptr - 1) >= 'A' && *(ptr - 1) <= 'Z') ||
          (*(ptr - 1) >= '0' && *(ptr - 1) <= '9') ||
          *(ptr - 1) == '_'))
     ptr--;

   wordlen = col - (ptr - curtext) - 1;
   curword = malloc(sizeof(char) * (wordlen + 1));
   strncpy(curword, ptr, wordlen);
   curword[wordlen] = '\0';

   return curword;
}

static char *
_suggest_item_return_get(Suggest_Item *item)
{
   char *ret_str;
   int retlen;

   if (item->ret)
     {
        retlen = strlen(item->ret) + 6;
        ret_str = malloc(sizeof(char) * retlen);
        snprintf(ret_str, retlen, " %s<br>", item->ret);
     }
   else
     ret_str = strdup("");

   return ret_str;
}

static char *
_suggest_item_parameter_get(Suggest_Item *item)
{
   char *param_str;
   int paramlen;

   if (item->param)
     {
        paramlen = strlen(item->param) + 6;
        param_str = malloc(sizeof(char) * paramlen);
        snprintf(param_str, paramlen, "<br>%s", item->param);
     }
   else
     param_str = strdup("");

   return param_str;
}

static void
_suggest_item_free(Suggest_Item *item)
{
   if (item->ret) free(item->ret);
   if (item->name) free(item->name);
   if (item->param) free(item->param);
   free(item);
}

static Evas_Object *
_suggest_list_content_get(void *data, Evas_Object *obj, const char *part)
{
   Edi_Editor *editor;
   Edi_Mainview_Item *item;
   Suggest_Item *suggest_it = data;
   char *format, *display;
   const char *font;
   int font_size, displen;

   if (strcmp(part, "elm.swallow.content"))
     return NULL;

   item = edi_mainview_item_current_get();

   if (!item)
     return NULL;

   editor = (Edi_Editor *)evas_object_data_get(item->view, "editor");
   elm_code_widget_font_get(editor->entry, &font, &font_size);

   format = "<align=left><font=%s><font_size=%d> %s</font_size></font></align>";
   displen = strlen(suggest_it->name) + strlen(format) + strlen(font);
   display = malloc(sizeof(char) * displen);
   snprintf(display, displen, format, font, font_size, suggest_it->name);

   Evas_Object *label = elm_label_add(obj);
   elm_label_ellipsis_set(label, EINA_TRUE);
   elm_object_text_set(label, display);
   evas_object_color_set(label, 255, 255, 255, 255);
   evas_object_show(label);
   free(display);

   return label;
}

static void
_suggest_list_cb_selected(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Edi_Editor *editor;
   Edi_Mainview_Item *item;
   Suggest_Item *suggest_it;
   Evas_Object *label = data;
   char *format, *display, *ret_str, *param_str;
   const char *font;
   int font_size, displen;

   suggest_it = elm_object_item_data_get(event_info);

   item = edi_mainview_item_current_get();

   if (!item)
     return;

   editor = (Edi_Editor *)evas_object_data_get(item->view, "editor");
   elm_code_widget_font_get(editor->entry, &font, &font_size);

   ret_str = _suggest_item_return_get(suggest_it);
   param_str = _suggest_item_parameter_get(suggest_it);

   format = "<align=left><font=%s><font_size=%d>%s<b> %s</b>%s</font_size></font></align>";
   displen = strlen(ret_str) + strlen(param_str) + strlen(suggest_it->name)
             + strlen(format) + strlen(font);
   display = malloc(sizeof(char) * displen);
   snprintf(display, displen, format, font, font_size, ret_str, suggest_it->name,
            param_str);

   elm_object_text_set(label, display);
   free(display);
   free(ret_str);
   free(param_str);
}

static void
_suggest_list_update(char *word)
{
   Suggest_Item *suggest_it;
   Eina_List *list, *l;
   Elm_Genlist_Item_Class *ic;
   Elm_Object_Item *item;

   elm_genlist_clear(_suggest_popup_genlist);

   list = (Eina_List *)evas_object_data_get(_suggest_popup_genlist,
                                            "suggest_list");
   ic = elm_genlist_item_class_new();
   ic->item_style = "full";
   ic->func.content_get = _suggest_list_content_get;

   EINA_LIST_FOREACH(list, l, suggest_it)
     {
        if (eina_str_has_prefix(suggest_it->name, word))
          {
             elm_genlist_item_append(_suggest_popup_genlist,
                                     ic,
                                     suggest_it,
                                     NULL,
                                     ELM_GENLIST_ITEM_NONE,
                                     NULL,
                                     NULL);
          }
     }
   elm_genlist_item_class_free(ic);

   item = elm_genlist_first_item_get(_suggest_popup_genlist);
   if (item)
     {
        elm_genlist_item_selected_set(item, EINA_TRUE);
        elm_genlist_item_show(item, ELM_GENLIST_ITEM_SCROLLTO_TOP);
     }
   else
     evas_object_hide(_suggest_popup_bg);
}

static void
_suggest_list_set(Edi_Editor *editor)
{
   Elm_Code *code;
   CXCodeCompleteResults *res;
   struct CXUnsavedFile unsaved_file;
   char *curword;
   const char *path;
   unsigned int row, col;
   Eina_List *list = NULL;

   list = (Eina_List *)evas_object_data_get(_suggest_popup_genlist,
                                            "suggest_list");
   if (list)
     {
        Suggest_Item *suggest_it;

        EINA_LIST_FREE(list, suggest_it)
          _suggest_item_free(suggest_it);

        list = NULL;
        evas_object_data_del(_suggest_popup_genlist, "suggest_list");
     }

   elm_code_widget_cursor_position_get(editor->entry, &row, &col);

   code = elm_code_widget_code_get(editor->entry);
   path = elm_code_file_path_get(code->file);

   curword = _edi_editor_current_word_get(editor, row, col);

   unsaved_file.Filename = path;
   unsaved_file.Contents = elm_code_widget_text_between_positions_get(
                                                 editor->entry, 1, 1, col, row);
   unsaved_file.Length = strlen(unsaved_file.Contents);

   res = clang_codeCompleteAt(editor->as_unit, path, row, col - strlen(curword),
                              &unsaved_file, 1,
                              CXCodeComplete_IncludeMacros |
                              CXCodeComplete_IncludeCodePatterns);

   clang_sortCodeCompletionResults(res->Results, res->NumResults);

   for (unsigned int i = 0; i < res->NumResults; i++)
     {
        const CXCompletionString str = res->Results[i].CompletionString;
        Suggest_Item *suggest_it;
        Eina_Strbuf *buf = NULL;

        suggest_it = calloc(1, sizeof(Suggest_Item));
        suggest_it->kind = res->Results[i].CursorKind;
        if (suggest_it->kind == CXCursor_OverloadCandidate)
          suggest_it->is_param_cand = EINA_TRUE;

        for (unsigned int j = 0; j < clang_getNumCompletionChunks(str); j++)
          {
             enum CXCompletionChunkKind ch_kind;
             const CXString str_out = clang_getCompletionChunkText(str, j);

             ch_kind = clang_getCompletionChunkKind(str, j);

             switch (ch_kind)
               {
                case CXCompletionChunk_ResultType:
                   suggest_it->ret = strdup(clang_getCString(str_out));
                   break;
                case CXCompletionChunk_TypedText:
                case CXCompletionChunk_Text:
                   suggest_it->name = strdup(clang_getCString(str_out));
                   break;
                case CXCompletionChunk_LeftParen:
                case CXCompletionChunk_Placeholder:
                case CXCompletionChunk_Comma:
                case CXCompletionChunk_CurrentParameter:
                   if (!buf)
                     buf = eina_strbuf_new();
                   eina_strbuf_append(buf, clang_getCString(str_out));
                   break;
                case CXCompletionChunk_RightParen:
                   eina_strbuf_append(buf, clang_getCString(str_out));
                   suggest_it->param = eina_strbuf_string_steal(buf);
                   eina_strbuf_free(buf);
                   buf = NULL;
                   break;
                default:
                   break;
               }
          }
        list = eina_list_append(list, suggest_it);
     }

   clang_disposeCodeCompleteResults(res);

   evas_object_data_set(_suggest_popup_genlist, "suggest_list", list);
   _suggest_list_update(curword);
   free(curword);
}

static void
_suggest_list_selection_insert(Edi_Editor *editor, const char *selection)
{
   Elm_Code *code;
   Elm_Code_Line *line;
   char *word;
   unsigned int wordlen, col, row;

   elm_code_widget_cursor_position_get(editor->entry, &row, &col);

   code = elm_code_widget_code_get(editor->entry);
   line = elm_code_file_line_get(code->file, row);
   word = _edi_editor_current_word_get(editor, row, col);
   wordlen = strlen(word);
   free(word);

   elm_code_line_text_remove(line, col - wordlen - 1, wordlen);
   elm_code_line_text_insert(line, col - wordlen - 1, selection,
                             strlen(selection));
   elm_code_widget_cursor_position_set(editor->entry, row,
                                       col - wordlen + strlen(selection));
}

static void
_suggest_bg_cb_hide(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
                    Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Eina_List *list = NULL;

   list = (Eina_List *)evas_object_data_get(_suggest_popup_genlist,
                                            "suggest_list");
   if (list)
     {
        Suggest_Item *suggest_it;

        EINA_LIST_FREE(list, suggest_it)
          _suggest_item_free(suggest_it);

        list = NULL;
        evas_object_data_del(_suggest_popup_genlist, "suggest_list");
     }
   evas_object_key_ungrab(_suggest_popup_genlist, "Return", 0, 0);
   evas_object_key_ungrab(_suggest_popup_genlist, "Up", 0, 0);
   evas_object_key_ungrab(_suggest_popup_genlist, "Down", 0, 0);
}

static void
_suggest_list_cb_key_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
                          void *event_info)
{
   Edi_Editor *editor = (Edi_Editor *)data;
   Suggest_Item *suggest_it;
   Elm_Object_Item *it;
   Evas_Object *genlist = obj;
   Evas_Event_Key_Down *ev = event_info;

   if (!strcmp(ev->key, "Return"))
     {
        it = elm_genlist_selected_item_get(genlist);
        suggest_it = elm_object_item_data_get(it);

        _suggest_list_selection_insert(editor, suggest_it->name);
        evas_object_hide(_suggest_popup_bg);
     }
   else if (!strcmp(ev->key, "Up"))
     {
        it = elm_genlist_item_prev_get(elm_genlist_selected_item_get(genlist));
        if(!it) it = elm_genlist_last_item_get(genlist);

        elm_genlist_item_selected_set(it, EINA_TRUE);
        elm_genlist_item_show(it, ELM_GENLIST_ITEM_SCROLLTO_IN);
     }
   else if (!strcmp(ev->key, "Down"))
     {
        it = elm_genlist_item_next_get(elm_genlist_selected_item_get(genlist));
        if(!it) it = elm_genlist_first_item_get(genlist);

        elm_genlist_item_selected_set(it, EINA_TRUE);
        elm_genlist_item_show(it, ELM_GENLIST_ITEM_SCROLLTO_IN);
     }
}

static void
_suggest_list_cb_clicked_double(void *data, Evas_Object *obj EINA_UNUSED,
                                void *event_info)
{
   Elm_Object_Item *it = event_info;
   Suggest_Item *suggest_it;
   Edi_Editor *editor = (Edi_Editor *)data;

   suggest_it = elm_object_item_data_get(it);
   _suggest_list_selection_insert(editor, suggest_it->name);

   evas_object_hide(_suggest_popup_bg);
}

static void
_suggest_popup_show(Edi_Editor *editor)
{
   unsigned int col, row;
   Evas_Coord cx, cy, cw, ch;

   if (elm_genlist_items_count(_suggest_popup_genlist) <= 0)
     return;

   elm_code_widget_cursor_position_get(editor->entry, &row, &col);
   elm_code_widget_geometry_for_position_get(editor->entry, row, col,
                                             &cx, &cy, &cw, &ch);

   evas_object_move(_suggest_popup_bg, cx, cy);
   evas_object_show(_suggest_popup_bg);

   if (!evas_object_key_grab(_suggest_popup_genlist, "Return", 0, 0, EINA_TRUE))
     ERR("Failed to grab key - %s", "Return");
   if (!evas_object_key_grab(_suggest_popup_genlist, "Up", 0, 0, EINA_TRUE))
     ERR("Failed to grab key - %s", "Up");
   if (!evas_object_key_grab(_suggest_popup_genlist, "Down", 0, 0, EINA_TRUE))
     ERR("Failed to grab key - %s", "Down");
}

static void
_suggest_popup_key_down_cb(Edi_Editor *editor, const char *key, const char *string)
{
   Elm_Code *code;
   Elm_Code_Line *line;
   unsigned int col, row;
   char *word = NULL;

   if (!evas_object_visible_get(_suggest_popup_bg))
     return;

   elm_code_widget_cursor_position_get(editor->entry, &row, &col);

   code = elm_code_widget_code_get(editor->entry);
   line = elm_code_file_line_get(code->file, row);

   if (!strcmp(key, "Left"))
     {
        if (col - 1 <= 0)
          {
             evas_object_hide(_suggest_popup_bg);
             return;
          }

        word = _edi_editor_current_word_get(editor, row, col - 1);
        if (!strcmp(word, ""))
          evas_object_hide(_suggest_popup_bg);
        else
          {
             _suggest_list_update(word);
             _suggest_popup_show(editor);
          }
     }
   else if (!strcmp(key, "Right"))
     {
        if (line->length < col)
          {
             evas_object_hide(_suggest_popup_bg);
             return;
          }

        word = _edi_editor_current_word_get(editor, row, col + 1);
        if (!strcmp(word, ""))
          evas_object_hide(_suggest_popup_bg);
        else
          {
             _suggest_list_update(word);
             _suggest_popup_show(editor);
          }
     }
   else if (!strcmp(key, "BackSpace"))
     {
        if (col - 1 <= 0)
          {
             evas_object_hide(_suggest_popup_bg);
             return;
          }

        word = _edi_editor_current_word_get(editor, row, col - 1);
        if (!strcmp(word, ""))
          evas_object_hide(_suggest_popup_bg);
        else
          {
             _suggest_list_update(word);
             _suggest_popup_show(editor);
          }
     }
   else if (!strcmp(key, "Escape"))
     {
        evas_object_hide(_suggest_popup_bg);
     }
   else if (!strcmp(key, "Delete"))
     {
        //Do nothing
     }
   else if (string && strlen(string) == 1)
     {
        word = _edi_editor_current_word_get(editor, row, col);
        strncat(word, string, 1);
        _suggest_list_update(word);
        _suggest_popup_show(editor);
     }
   if (word)
     free(word);
}

static void
_suggest_popup_setup(Edi_Editor *editor)
{
   //Popup bg
   Evas_Object *bg = elm_bubble_add(editor->entry);
   _suggest_popup_bg = bg;
   evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(bg, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(bg, EVAS_CALLBACK_HIDE,
                                  _suggest_bg_cb_hide, NULL);
   evas_object_resize(bg, 400 * elm_config_scale_get(), 300 * elm_config_scale_get());

   //Box
   Evas_Object *box = elm_box_add(bg);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(bg, box);
   evas_object_show(box);

   //Genlist
   Evas_Object *genlist = elm_genlist_add(box);
   _suggest_popup_genlist = genlist;
   evas_object_size_hint_weight_set(genlist, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(genlist, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_focus_allow_set(genlist, EINA_FALSE);
   elm_genlist_homogeneous_set(genlist, EINA_TRUE);
   evas_object_event_callback_add(genlist, EVAS_CALLBACK_KEY_DOWN,
                                  _suggest_list_cb_key_down, editor);
   evas_object_smart_callback_add(genlist, "clicked,double",
                                  _suggest_list_cb_clicked_double, editor);
   evas_object_show(genlist);
   elm_box_pack_end(box, genlist);

   //Label
   Evas_Object *label = elm_label_add(box);
   elm_label_line_wrap_set(label, ELM_WRAP_MIXED);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_color_set(label, 255, 255, 255, 255);
   evas_object_show(label);
   elm_box_pack_end(box, label);

   evas_object_smart_callback_add(genlist, "selected",
                                  _suggest_list_cb_selected, label);
}

static void
_clang_autosuggest_setup(Edi_Editor *editor)
{
   Elm_Code *code;
   const char *path;
   char **clang_argv;
   const char *args;
   unsigned int clang_argc;

   code = elm_code_widget_code_get(editor->entry);
   path = elm_code_file_path_get(code->file);

   //Initialize Clang
   args = "-I/usr/inclue/ " EFL_CFLAGS " " CLANG_INCLUDES " -Wall -Wextra";
   clang_argv = eina_str_split_full(args, " ", 0, &clang_argc);

   editor->as_idx = clang_createIndex(0, 0);

   editor->as_unit = clang_parseTranslationUnit(editor->as_idx, path,
                                  (const char *const *)clang_argv,
                                  (int)clang_argc, NULL, 0,
                                  clang_defaultEditingTranslationUnitOptions());

   _suggest_popup_setup(editor);
}

static void
_clang_autosuggest_dispose(Edi_Editor *editor)
{
   clang_disposeTranslationUnit(editor->as_unit);
   clang_disposeIndex(editor->as_idx);
}
#endif

static void
_smart_cb_key_down(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
                   Evas_Object *obj EINA_UNUSED, void *event)
{
   Edi_Mainview_Item *item;
   Edi_Editor *editor;
   Eina_Bool ctrl, alt, shift;
   Evas_Event_Key_Down *ev = event;

   ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   alt = evas_key_modifier_is_set(ev->modifiers, "Alt");
   shift = evas_key_modifier_is_set(ev->modifiers, "Shift");

   item = edi_mainview_item_current_get();

   if (!item)
     return;

   editor = (Edi_Editor *)evas_object_data_get(item->view, "editor");

   if ((!alt) && (ctrl) && (!shift))
     {
        if (!strcmp(ev->key, "Prior"))
          {
             edi_mainview_item_prev();
          }
        else if (!strcmp(ev->key, "Next"))
          {
             edi_mainview_item_next();
          }
        else if (!strcmp(ev->key, "s"))
          {
             edi_editor_save(editor);
          }
        else if (!strcmp(ev->key, "f"))
          {
             edi_mainview_search();
          }
        else if (!strcmp(ev->key, "g"))
          {
             edi_mainview_goto_popup_show();
          }
#if HAVE_LIBCLANG
        else if (!strcmp(ev->key, "space"))
          {
             _suggest_list_set(editor);
             _suggest_popup_show(editor);
          }
#endif
     }
#if HAVE_LIBCLANG
   if ((!alt) && (!ctrl))
     _suggest_popup_key_down_cb(editor, ev->key, ev->string);
#endif
}

static void
_edit_cursor_moved(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Elm_Code_Widget *widget;
   char buf[30];
   unsigned int line;
   unsigned int col;

   widget = (Elm_Code_Widget *)obj;
   elm_code_widget_cursor_position_get(widget, &line, &col);

   snprintf(buf, sizeof(buf), "Line:%d, Column:%d", line, col);
   elm_object_text_set((Evas_Object *)data, buf);
}

static void
_edi_editor_statusbar_add(Evas_Object *panel, Edi_Editor *editor, Edi_Mainview_Item *item)
{
   Evas_Object *position, *mime, *lines;
   Elm_Code *code;

   elm_box_horizontal_set(panel, EINA_TRUE);

   mime = elm_label_add(panel);
   if (item->mimetype)
     elm_object_text_set(mime, item->mimetype);
   else
     elm_object_text_set(mime, item->editortype);
   evas_object_size_hint_align_set(mime, 0.0, 0.5);
   evas_object_size_hint_weight_set(mime, 0.1, 0.0);
   elm_box_pack_end(panel, mime);
   evas_object_show(mime);
   elm_object_disabled_set(mime, EINA_TRUE);

   lines = elm_label_add(panel);
   code = elm_code_widget_code_get(editor->entry);
   if (elm_code_file_line_ending_get(code->file) == ELM_CODE_FILE_LINE_ENDING_WINDOWS)
     elm_object_text_set(lines, "WIN");
   else
     elm_object_text_set(lines, "UNIX");
   evas_object_size_hint_align_set(lines, 0.0, 0.5);
   evas_object_size_hint_weight_set(lines, EVAS_HINT_EXPAND, 0.0);
   elm_box_pack_end(panel, lines);
   evas_object_show(lines);
   elm_object_disabled_set(lines, EINA_TRUE);

   position = elm_label_add(panel);
   evas_object_size_hint_align_set(position, 1.0, 0.5);
   evas_object_size_hint_weight_set(position, EVAS_HINT_EXPAND, 0.0);
   elm_box_pack_end(panel, position);
   evas_object_show(position);
   elm_object_disabled_set(position, EINA_TRUE);

   _edit_cursor_moved(position, editor->entry, NULL);
   evas_object_smart_callback_add(editor->entry, "cursor,changed", _edit_cursor_moved, position);
}

#if HAVE_LIBCLANG
static void
_edi_range_color_set(Edi_Editor *editor, Edi_Range range, Elm_Code_Token_Type type)
{
   Elm_Code *code;
   Elm_Code_Line *line, *extra_line;
   unsigned int number;

   ecore_thread_main_loop_begin();

   code = elm_code_widget_code_get(editor->entry);
   line = elm_code_file_line_get(code->file, range.start.line);

   elm_code_line_token_add(line, range.start.col - 1, range.end.col - 2,
                           range.end.line - range.start.line + 1, type);

   elm_code_widget_line_refresh(editor->entry, line);
   for (number = line->number + 1; number <= range.end.line; number++)
     {
        extra_line = elm_code_file_line_get(code->file, number);
        elm_code_widget_line_refresh(editor->entry, extra_line);
     }

   ecore_thread_main_loop_end();
}

static void
_edi_line_status_set(Edi_Editor *editor, unsigned int number, Elm_Code_Status_Type status,
                     const char *text)
{
   Elm_Code *code;
   Elm_Code_Line *line;

   ecore_thread_main_loop_begin();

   code = elm_code_widget_code_get(editor->entry);
   line = elm_code_file_line_get(code->file, number);
   if (!line)
     {
        if (text)
          ERR("Status on invalid line %d (\"%s\")", number, text);

        ecore_thread_main_loop_end();
        return;
     }

   elm_code_line_status_set(line, status);
   if (text)
     elm_code_line_status_text_set(line, text);

   elm_code_widget_line_refresh(editor->entry, line);

   ecore_thread_main_loop_end();
}

static void
_clang_load_highlighting(const char *path, Edi_Editor *editor)
{
        CXFile cfile = clang_getFile(editor->tx_unit, path);

        CXSourceRange range = clang_getRange(
              clang_getLocationForOffset(editor->tx_unit, cfile, 0),
              clang_getLocationForOffset(editor->tx_unit, cfile, eina_file_size_get(eina_file_open(path, EINA_FALSE))));

        clang_tokenize(editor->tx_unit, range, &editor->tokens, &editor->token_count);
        editor->cursors = (CXCursor *) malloc(editor->token_count * sizeof(CXCursor));
        clang_annotateTokens(editor->tx_unit, editor->tokens, editor->token_count, editor->cursors);
}

static void
_clang_show_highlighting(Edi_Editor *editor)
{
   unsigned int i = 0;

   for (i = 0 ; i < editor->token_count ; i++)
     {
        Edi_Range range;
        Elm_Code_Token_Type type = ELM_CODE_TOKEN_TYPE_DEFAULT;

        CXSourceRange tkrange = clang_getTokenExtent(editor->tx_unit, editor->tokens[i]);
        clang_getSpellingLocation(clang_getRangeStart(tkrange), NULL,
              &range.start.line, &range.start.col, NULL);
        clang_getSpellingLocation(clang_getRangeEnd(tkrange), NULL,
              &range.end.line, &range.end.col, NULL);
        /* FIXME: Should probably do something fancier, this is only a limited
         * number of types. */
        switch (clang_getTokenKind(editor->tokens[i]))
          {
             case CXToken_Punctuation:
                if (i < editor->token_count - 1 && range.start.col == 1 &&
                    (clang_getTokenKind(editor->tokens[i + 1]) == CXToken_Identifier && (editor->cursors[i + 1].kind == CXCursor_MacroDefinition ||
                    editor->cursors[i + 1].kind == CXCursor_InclusionDirective || editor->cursors[i + 1].kind == CXCursor_PreprocessingDirective)))
                  type = ELM_CODE_TOKEN_TYPE_PREPROCESSOR;
                else
                  type = ELM_CODE_TOKEN_TYPE_BRACE;
                break;
             case CXToken_Identifier:
                if (editor->cursors[i].kind < CXCursor_FirstRef)
                  {
                      type = ELM_CODE_TOKEN_TYPE_CLASS;
                      break;
                  }
                switch (editor->cursors[i].kind)
                  {
                   case CXCursor_DeclRefExpr:
                      /* Handle different ref kinds */
                      type = ELM_CODE_TOKEN_TYPE_FUNCTION;
                      break;
                   case CXCursor_MacroDefinition:
                   case CXCursor_InclusionDirective:
                   case CXCursor_PreprocessingDirective:
                      type = ELM_CODE_TOKEN_TYPE_PREPROCESSOR;
                      break;
                   case CXCursor_TypeRef:
                      type = ELM_CODE_TOKEN_TYPE_TYPE;
                      break;
                   case CXCursor_MacroExpansion:
                      type = ELM_CODE_TOKEN_TYPE_PREPROCESSOR;//_MACRO_EXPANSION;
                      break;
                   default:
                      break;
                  }
                break;
             case CXToken_Keyword:
                switch (editor->cursors[i].kind)
                  {
                   case CXCursor_PreprocessingDirective:
                      type = ELM_CODE_TOKEN_TYPE_PREPROCESSOR;
                      break;
                   case CXCursor_CaseStmt:
                   case CXCursor_DefaultStmt:
                   case CXCursor_IfStmt:
                   case CXCursor_SwitchStmt:
                   case CXCursor_WhileStmt:
                   case CXCursor_DoStmt:
                   case CXCursor_ForStmt:
                   case CXCursor_GotoStmt:
                   case CXCursor_IndirectGotoStmt:
                   case CXCursor_ContinueStmt:
                   case CXCursor_BreakStmt:
                   case CXCursor_ReturnStmt:
                   case CXCursor_AsmStmt:
                   case CXCursor_ObjCAtTryStmt:
                   case CXCursor_ObjCAtCatchStmt:
                   case CXCursor_ObjCAtFinallyStmt:
                   case CXCursor_ObjCAtThrowStmt:
                   case CXCursor_ObjCAtSynchronizedStmt:
                   case CXCursor_ObjCAutoreleasePoolStmt:
                   case CXCursor_ObjCForCollectionStmt:
                   case CXCursor_CXXCatchStmt:
                   case CXCursor_CXXTryStmt:
                   case CXCursor_CXXForRangeStmt:
                   case CXCursor_SEHTryStmt:
                   case CXCursor_SEHExceptStmt:
                   case CXCursor_SEHFinallyStmt:
//                      type = ELM_CODE_TOKEN_TYPE_KEYWORD_STMT;
                      break;
                   default:
                      type = ELM_CODE_TOKEN_TYPE_KEYWORD;
                      break;
                  }
                break;
             case CXToken_Literal:
                if (editor->cursors[i].kind == CXCursor_StringLiteral || editor->cursors[i].kind == CXCursor_CharacterLiteral)
                  type = ELM_CODE_TOKEN_TYPE_STRING;
                else
                  type = ELM_CODE_TOKEN_TYPE_NUMBER;
                break;
             case CXToken_Comment:
                type = ELM_CODE_TOKEN_TYPE_COMMENT;
                break;
          }

        if (editor->highlight_cancel)
          break;
        _edi_range_color_set(editor, range, type);
     }
}

static void
_clang_free_highlighting(Edi_Editor *editor)
{
   free(editor->cursors);
   clang_disposeTokens(editor->tx_unit, editor->tokens, editor->token_count);
}

static void
_clang_load_errors(const char *path EINA_UNUSED, Edi_Editor *editor)
{
   Elm_Code *code;
   const char *filename;
   unsigned n = clang_getNumDiagnostics(editor->tx_unit);
   unsigned i = 0;

   ecore_thread_main_loop_begin();
   code = elm_code_widget_code_get(editor->entry);
   filename = elm_code_file_path_get(code->file);
   ecore_thread_main_loop_end();

   for(i = 0, n = clang_getNumDiagnostics(editor->tx_unit); i != n; ++i)
     {
        CXDiagnostic diag = clang_getDiagnostic(editor->tx_unit, i);
        CXFile file;
        unsigned int line;
        CXString path;

        // the parameter after line would be a caret position but we're just highlighting for now
        clang_getSpellingLocation(clang_getDiagnosticLocation(diag), &file, &line, NULL, NULL);

        path = clang_getFileName(file);
        if (!clang_getCString(path) || strcmp(filename, clang_getCString(path)))
          continue;

        /* FIXME: Also handle ranges and fix suggestions. */
        Elm_Code_Status_Type status = ELM_CODE_STATUS_TYPE_DEFAULT;

        switch (clang_getDiagnosticSeverity(diag))
          {
           case CXDiagnostic_Ignored:
              status = ELM_CODE_STATUS_TYPE_IGNORED;
              break;
           case CXDiagnostic_Note:
              status = ELM_CODE_STATUS_TYPE_NOTE;
              break;
           case CXDiagnostic_Warning:
              status = ELM_CODE_STATUS_TYPE_WARNING;
              break;
           case CXDiagnostic_Error:
              status = ELM_CODE_STATUS_TYPE_ERROR;
              break;
           case CXDiagnostic_Fatal:
              status = ELM_CODE_STATUS_TYPE_FATAL;
              break;
          }
        CXString str = clang_getDiagnosticSpelling(diag);
        if (status != ELM_CODE_STATUS_TYPE_DEFAULT)
          _edi_line_status_set(editor, line, status, clang_getCString(str));
        clang_disposeString(str);

        clang_disposeDiagnostic(diag);
        if (editor->highlight_cancel)
          break;
     }
}

static void
_edi_clang_setup(void *data, Ecore_Thread *thread EINA_UNUSED)
{
   Edi_Editor *editor;
   Elm_Code *code;
   const char *path, *args;
   char **clang_argv;
   unsigned int clang_argc;

   ecore_thread_main_loop_begin();

   editor = (Edi_Editor *)data;
   code = elm_code_widget_code_get(editor->entry);
   path = elm_code_file_path_get(code->file);

   ecore_thread_main_loop_end();

   /* Clang */
   /* FIXME: index should probably be global. */
   args = "-I/usr/inclue/ " EFL_CFLAGS " " CLANG_INCLUDES " -Wall -Wextra";
   clang_argv = eina_str_split_full(args, " ", 0, &clang_argc);

   editor->idx = clang_createIndex(0, 0);

   /* FIXME: Possibly activate more options? */
   editor->tx_unit = clang_parseTranslationUnit(editor->idx, path, (const char *const *)clang_argv, (int)clang_argc, NULL, 0,
     clang_defaultEditingTranslationUnitOptions() | CXTranslationUnit_DetailedPreprocessingRecord);

   _clang_load_errors(path, editor);
   _clang_load_highlighting(path, editor);
   _clang_show_highlighting(editor);
}

static void
_edi_clang_dispose(void *data, Ecore_Thread *thread EINA_UNUSED)
{
   Edi_Editor *editor = (Edi_Editor *)data;

   _clang_free_highlighting(editor);
   clang_disposeTranslationUnit(editor->tx_unit);
   clang_disposeIndex(editor->idx);

   editor->highlight_thread = NULL;
   editor->highlight_cancel = EINA_FALSE;
}
#endif

static void
_unfocused_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Edi_Editor *editor;

   editor = (Edi_Editor *)data;

   if (_edi_config->autosave)
     edi_editor_save(editor);
}

static void
_mouse_up_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
             Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Elm_Code_Widget *widget;
   Evas_Object *popup;
   Evas_Event_Mouse_Up *event;
   Eina_Bool ctrl;
   unsigned int row;
   int col;
   const char *word;

   widget = (Elm_Code_Widget *)data;
   event = (Evas_Event_Mouse_Up *)event_info;

#if HAVE_LIBCLANG
   if (_suggest_popup_bg)
     evas_object_hide(_suggest_popup_bg);
#endif

   ctrl = evas_key_modifier_is_set(event->modifiers, "Control");
   if (event->button != 3 || !ctrl)
     return;

   elm_code_widget_position_at_coordinates_get(widget, event->canvas.x, event->canvas.y, &row, &col);
   elm_code_widget_selection_select_word(widget, row, col);
   word = elm_code_widget_selection_text_get(widget);
   if (!word || !strlen(word))
     return;

   popup = elm_popup_add(widget);
   elm_popup_timeout_set(popup,1.5);

   elm_object_style_set(popup, "transparent");
   elm_object_part_text_set(popup, "title,text", word);
   elm_object_text_set(popup, "No help available for this term");

   evas_object_show(popup);
}

static void
_edi_editor_parse_line_cb(Elm_Code_Line *line EINA_UNUSED, void *data)
{
   Edi_Editor *editor = (Edi_Editor *)data;

   // We have caused a reset in the file parser, if it is active
   if (!editor->highlight_thread)
     return;

   editor->highlight_cancel = EINA_TRUE;
}

static void
_edi_editor_parse_file_cb(Elm_Code_File *file EINA_UNUSED, void *data)
{
   Edi_Editor *editor;

   editor = (Edi_Editor *)data;
   if (editor->highlight_thread)
     return;

#if HAVE_LIBCLANG
   editor->highlight_cancel = EINA_FALSE;
   editor->highlight_thread = ecore_thread_run(_edi_clang_setup, _edi_clang_dispose, NULL, editor);
#endif
}

static Eina_Bool
_edi_editor_config_changed(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Elm_Code_Widget *widget;
   Elm_Code *code;

   widget = (Elm_Code_Widget *) data;
   code = elm_code_widget_code_get(widget);

   code->config.trim_whitespace = _edi_config->trim_whitespace;

   elm_obj_code_widget_font_set(widget, _edi_project_config->font.name, _edi_project_config->font.size);
   elm_obj_code_widget_show_whitespace_set(widget, _edi_project_config->gui.show_whitespace);
   elm_obj_code_widget_tab_inserts_spaces_set(widget, _edi_project_config->gui.tab_inserts_spaces);
   elm_obj_code_widget_line_width_marker_set(widget, _edi_project_config->gui.width_marker);
   elm_obj_code_widget_tabstop_set(widget, _edi_project_config->gui.tabstop);

   return ECORE_CALLBACK_RENEW;
}

static void
_editor_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *o EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ecore_Event_Handler *ev_handler = data;

   ecore_event_handler_del(ev_handler);
#if HAVE_LIBCLANG
   Evas_Object *view = o;
   Edi_Editor *editor = (Edi_Editor *)evas_object_data_get(view, "editor");
   _clang_autosuggest_dispose(editor);
#endif
}

Evas_Object *
edi_editor_add(Evas_Object *parent, Edi_Mainview_Item *item)
{
   Evas_Object *vbox, *box, *searchbar, *statusbar;
   Evas_Modifier_Mask ctrl, shift, alt;
   Ecore_Event_Handler *ev_handler;
   Evas *e;

   Elm_Code *code;
   Elm_Code_Widget *widget;
   Edi_Editor *editor;

   vbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(vbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(vbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(vbox);

   searchbar = elm_box_add(vbox);
   evas_object_size_hint_weight_set(searchbar, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(searchbar, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(vbox, searchbar);

   box = elm_box_add(vbox);
   elm_box_horizontal_set(box, EINA_TRUE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(vbox, box);
   evas_object_show(box);

   statusbar = elm_box_add(vbox);
   evas_object_size_hint_weight_set(statusbar, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(statusbar, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(vbox, statusbar);
   evas_object_show(statusbar);

   code = elm_code_create();
   widget = elm_code_widget_add(vbox, code);
   elm_code_widget_editable_set(widget, EINA_TRUE);
   elm_code_widget_line_numbers_set(widget, EINA_TRUE);
   _edi_editor_config_changed(widget, 0, NULL);

   editor = calloc(1, sizeof(*editor));
   editor->entry = widget;
   editor->show_highlight = !strcmp(item->editortype, "code");
   evas_object_event_callback_add(widget, EVAS_CALLBACK_KEY_DOWN,
                                  _smart_cb_key_down, editor);
   evas_object_smart_callback_add(widget, "changed,user", _changed_cb, editor);
   evas_object_event_callback_add(widget, EVAS_CALLBACK_MOUSE_UP, _mouse_up_cb, widget);
   evas_object_smart_callback_add(widget, "unfocused", _unfocused_cb, editor);

   elm_code_parser_standard_add(code, ELM_CODE_PARSER_STANDARD_TODO);
   if (editor->show_highlight)
     elm_code_parser_add(code, _edi_editor_parse_line_cb,
		               _edi_editor_parse_file_cb, editor);
   elm_code_file_open(code, item->path);

   evas_object_size_hint_weight_set(widget, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(widget, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(widget);
   elm_box_pack_end(box, widget);

   edi_editor_search_add(searchbar, editor);
   _edi_editor_statusbar_add(statusbar, editor, item);

   e = evas_object_evas_get(widget);
   ctrl = evas_key_modifier_mask_get(e, "Control");
   alt = evas_key_modifier_mask_get(e, "Alt");
   shift = evas_key_modifier_mask_get(e, "Shift");

   (void)!evas_object_key_grab(widget, "Prior", ctrl, shift | alt, 1);
   (void)!evas_object_key_grab(widget, "Next", ctrl, shift | alt, 1);
   (void)!evas_object_key_grab(widget, "s", ctrl, shift | alt, 1);
   (void)!evas_object_key_grab(widget, "f", ctrl, shift | alt, 1);
   (void)!evas_object_key_grab(widget, "g", ctrl, shift | alt, 1);
   (void)!evas_object_key_grab(widget, "space", ctrl, shift | alt, 1);

   evas_object_data_set(vbox, "editor", editor);
   ev_handler = ecore_event_handler_add(EDI_EVENT_CONFIG_CHANGED, _edi_editor_config_changed, widget);
   evas_object_event_callback_add(vbox, EVAS_CALLBACK_DEL, _editor_del_cb, ev_handler);

#if HAVE_LIBCLANG
   _clang_autosuggest_setup(editor);
#endif
   return vbox;
}
