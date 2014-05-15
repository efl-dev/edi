#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libgen.h>

#include <Eina.h>
#include <Eio.h>

#include "edi_mainview.h"

#include "editor/edi_editor.h"

#include "edi_private.h"

static Evas_Object *nf, *tb, *_main_win;
static Evas_Object *_edi_mainview_choose_popup;
static const char *_edi_mainview_choose_path;

static Eina_List *_edi_mainview_items = NULL;

static void
dummy()
{}

EAPI void
edi_mainview_item_prev()
{
   Eina_List *item;
   Elm_Object_Item *current;
   Edi_Mainview_Item *it, *prev;

   current = elm_naviframe_top_item_get(nf);

   EINA_LIST_FOREACH(_edi_mainview_items, item, it)
     {
        if (it && it->view == current)
          {
             edi_mainview_item_select(prev);
             return;
          }

        prev = it;
     }
}

EAPI void
edi_mainview_item_next()
{
   Eina_List *item;
   Elm_Object_Item *current;
   Edi_Mainview_Item *it;
   Eina_Bool open_next = EINA_FALSE;

   current = elm_naviframe_top_item_get(nf);

   EINA_LIST_FOREACH(_edi_mainview_items, item, it)
     {
        if (it && open_next)
          {
             edi_mainview_item_select(it);
             return;
          }

        if (it && it->view == current)
          open_next = EINA_TRUE;
     }
}

EAPI void
edi_mainview_item_select(Edi_Mainview_Item *item)
{
   if (item->win)
     {
        elm_win_raise(item->win);
     }
   else
     {
        elm_naviframe_item_promote(item->view);
        elm_toolbar_item_selected_set(item->tab, EINA_TRUE);
     }
}

static void
_promote(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   elm_naviframe_item_promote(data);
}

static Edi_Mainview_Item *
_get_item_for_path(const char *path)
{
   Eina_List *item;
   Edi_Mainview_Item *it;

   EINA_LIST_FOREACH(_edi_mainview_items, item, it)
     {
        if (it && !strcmp(it->path, path))
          return it;
     }
   return NULL;
}

static Edi_Mainview_Item *
_edi_mainview_item_add(const char *path, Elm_Object_Item *tab, Evas_Object *view,
                       Evas_Object *win)
{
   Edi_Mainview_Item *item;

   item = malloc(sizeof(Edi_Mainview_Item));
   item->path = path;
   item->tab = tab;
   item->view = view;
   item->win = win;

   _edi_mainview_items = eina_list_append(_edi_mainview_items, item);

   return item;
}

static Evas_Object *
_edi_mainview_content_image_create(const char *path, Evas_Object *parent)
{
   Evas_Object *img, *scroll;

   scroll = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroll, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroll, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(scroll);
   img = elm_image_add(scroll);
   elm_image_file_set(img, path, NULL);
   elm_image_no_scale_set(img, EINA_TRUE);
   elm_object_content_set(scroll, img);
   evas_object_show(img);

   return scroll;
}

static Evas_Object *
_edi_mainview_content_create(const char *path, const char *type, Evas_Object *parent)
{
   if (!strcmp(type, "text"))
     {
        return edi_editor_add(parent, path);
     }
   else if (!strcmp(type, "image"))
     {
        return _edi_mainview_content_image_create(path, parent);
     }

   return NULL;
}

static void
_edi_mainview_item_tab_add(const char *path, const char *type)
{
   Evas_Object *content;
   Elm_Object_Item *it, *tab;
   Edi_Mainview_Item *item;

   content = _edi_mainview_content_create(path, type, nf);

   it = elm_naviframe_item_simple_push(nf, content);
   elm_naviframe_item_style_set(it, "overlap");
   tab = elm_toolbar_item_append(tb, NULL, basename(path), _promote, it);
   elm_toolbar_item_selected_set(tab, EINA_TRUE);

   item = _edi_mainview_item_add(path, tab, it, NULL);
   elm_object_item_data_set(it, item);
}

static void
_edi_mainview_win_exit(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Edi_Mainview_Item *it;

   evas_object_hide(obj);

   it = evas_object_data_get(obj, "edi_mainview_item");
   _edi_mainview_items = eina_list_remove(_edi_mainview_items, it);
   eina_stringshare_del(it->path);
   free(it);
}

static char *
_edi_mainview_win_title_get(const char *path)
{
   char *winname, *filename;

   filename = basename(path);
   winname = malloc((8 + strlen(filename)) * sizeof(char));
   snprintf(winname, 8 + strlen(filename), "Edi :: %s", filename);

   return winname;
}

static void
_edi_mainview_item_win_add(const char *path, const char *type)
{
   Evas_Object *win, *content;
   Edi_Mainview_Item *item;

   win = elm_win_util_standard_add("mainview", _edi_mainview_win_title_get(path));
   if (!win) return;

   elm_win_focus_highlight_enabled_set(win, EINA_TRUE);
   evas_object_smart_callback_add(win, "delete,request", _edi_mainview_win_exit, NULL);
   item = _edi_mainview_item_add(path, NULL, NULL, win);
   evas_object_data_set(win, "edi_mainview_item", item);

   content = _edi_mainview_content_create(path, type, win);
   elm_win_resize_object_add(win, content);

   evas_object_resize(win, 380, 260);
   evas_object_show(win);
}

static void
_edi_mainview_choose_type_tab_cb(void *data,
                             Evas_Object *obj EINA_UNUSED,
                             void *event_info EINA_UNUSED)
{
   evas_object_del(_edi_mainview_choose_popup);
   edi_mainview_open_path(_edi_mainview_choose_path, (const char *) data);

}

static void
_edi_mainview_choose_type_win_cb(void *data,
                             Evas_Object *obj EINA_UNUSED,
                             void *event_info EINA_UNUSED)
{
   evas_object_del(_edi_mainview_choose_popup);
   edi_mainview_open_window_path(_edi_mainview_choose_path, (const char *) data);

}

static void
_edi_mainview_choose_type_close_cb(void *data EINA_UNUSED,
                                   Evas_Object *obj EINA_UNUSED,
                                   void *event_info EINA_UNUSED)
{
   evas_object_del(_edi_mainview_choose_popup);
}

static void
_edi_mainview_choose_type(Evas_Object *parent, const char *path, void *cb)
{
   Evas_Object *popup, *cancel, *icon;

   popup = elm_popup_add(_main_win);
   _edi_mainview_choose_popup = popup;
   _edi_mainview_choose_path = path;

   // popup title
   elm_object_part_text_set(popup, "title,text",
                            "Unrecognied file type");

   icon = elm_icon_add(popup);
   elm_icon_standard_set(icon, "txt");
   elm_popup_item_append(popup, "text", icon, cb, "text");
   icon = elm_icon_add(popup);
   elm_icon_standard_set(icon, "image");
   elm_popup_item_append(popup, "image", icon, cb, "image");


   cancel = elm_button_add(popup);
   elm_object_text_set(cancel, "cancel");
   elm_object_part_content_set(popup, "button1", cancel);
   evas_object_smart_callback_add(cancel, "clicked",
                                       _edi_mainview_choose_type_close_cb, NULL);

   // popup show should be called after adding all the contents and the buttons
   // of popup to set the focus into popup's contents correctly.
   evas_object_show(popup);
}

static void
_edi_mainview_tab_stat_done(void *data, Eio_File *handler EINA_UNUSED, const Eina_Stat *stat)
{
   const char *path, *mime;

   path = data;
   if (S_ISREG(stat->mode))
     {
        mime = efreet_mime_type_get(path);
        if (!strcasecmp(mime, "text/plain") || !strcasecmp(mime, "application/x-shellscript"))
          _edi_mainview_item_tab_add(path, "text");
        else if (!strcasecmp(mime, "text/x-chdr") || !strcasecmp(mime, "text/x-csrc"))
          _edi_mainview_item_tab_add(path, "text"); // TODO make a code view
        else if (!strncasecmp(mime, "image/", 6))
          _edi_mainview_item_tab_add(path, "image");
        else
          _edi_mainview_choose_type(nf, path, _edi_mainview_choose_type_tab_cb);
     }

   eina_stringshare_del(path);
}

static void
_edi_mainview_win_stat_done(void *data, Eio_File *handler EINA_UNUSED, const Eina_Stat *stat)
{
   const char *path, *mime;

   path = data;
   if (S_ISREG(stat->mode))
     {
        mime = efreet_mime_type_get(path);
        if (!strcasecmp(mime, "text/plain") || !strcasecmp(mime, "application/x-shellscript"))
          _edi_mainview_item_win_add(path, "text");
        else if (!strcasecmp(mime, "text/x-chdr") || !strcasecmp(mime, "text/x-csrc"))
          _edi_mainview_item_win_add(path, "text"); // TODO make a code view
        else if (!strncasecmp(mime, "image/", 6))
          _edi_mainview_item_win_add(path, "image");
        else
          _edi_mainview_choose_type(nf, path, _edi_mainview_choose_type_win_cb);
     }

   eina_stringshare_del(path);
}

EAPI void
edi_mainview_open_path(const char *path, const char *type)
{
   Edi_Mainview_Item *it;

   path = eina_stringshare_add(path);
   it = _get_item_for_path(path);
   if (it)
     {
        edi_mainview_item_select(it);
        eina_stringshare_del(path);
        return;
     }

   if (type == NULL)
     {
        eio_file_direct_stat(path, _edi_mainview_tab_stat_done, dummy,
                             eina_stringshare_add(path));
     }
   else
     {
        _edi_mainview_item_tab_add(path, type);
     }
}

EAPI void
edi_mainview_open_window_path(const char *path, const char *type)
{
   Edi_Mainview_Item *it;

   it = _get_item_for_path(path);
   if (it)
     {
        edi_mainview_item_select(it);
        elm_naviframe_item_pop(nf);
        elm_object_item_del(elm_toolbar_selected_item_get(tb));
        _edi_mainview_items = eina_list_remove(_edi_mainview_items, it);
        free(it);
     }

   if (type == NULL)
     {
        eio_file_direct_stat(path, _edi_mainview_win_stat_done, dummy,
                             eina_stringshare_add(path));
     }
   else
     {
        _edi_mainview_item_win_add(path, type);
     }
}

EAPI void
edi_mainview_save()
{
   Evas_Object *txt;
   Elm_Object_Item *it;

   it = elm_naviframe_top_item_get(nf);
   txt = elm_object_item_content_get(it);
   if (txt)
     elm_entry_file_save(txt);
}

EAPI void
edi_mainview_new_window()
{
   Edi_Mainview_Item *item;

   item = elm_object_item_data_get(elm_naviframe_top_item_get(nf));
   if (!item)
     return;

   edi_mainview_open_window_path(item->path, NULL);
}

EAPI void
edi_mainview_close()
{
   Edi_Mainview_Item *it;

   it = elm_object_item_data_get(elm_naviframe_top_item_get(nf));
   if (!it)
     return;

   elm_naviframe_item_pop(nf);
   elm_object_item_del(elm_toolbar_selected_item_get(tb));
   _edi_mainview_items = eina_list_remove(_edi_mainview_items, it);
   eina_stringshare_del(it->path);
   free(it);
}

EAPI void
edi_mainview_cut()
{
   Evas_Object *txt;
   Elm_Object_Item *it;

   it = elm_naviframe_top_item_get(nf);
   txt = elm_object_item_content_get(it);
   if (txt)
     elm_entry_selection_cut(txt);
}

EAPI void
edi_mainview_copy()
{
   Evas_Object *txt;
   Elm_Object_Item *it;

   it = elm_naviframe_top_item_get(nf);
   txt = elm_object_item_content_get(it);
   if (txt)
     elm_entry_selection_copy(txt);
}

EAPI void
edi_mainview_paste()
{
   Evas_Object *txt;
   Elm_Object_Item *it;

   it = elm_naviframe_top_item_get(nf);
   txt = elm_object_item_content_get(it);
   if (txt)
     elm_entry_selection_paste(txt);
}

EAPI void
edi_mainview_search()
{
   Evas_Object *txt;
   Elm_Object_Item *it;

   it = elm_naviframe_top_item_get(nf);
   txt = elm_object_item_content_get(it);
   if (txt)
     edi_editor_search(txt);
}

EAPI void
edi_mainview_goto(int line)
{
   Evas_Object *txt;
   Elm_Object_Item *it;
   Evas_Object *tb;
   Evas_Textblock_Cursor *mcur;
   Evas_Coord x, y, w, h;

   it = elm_naviframe_top_item_get(nf);
   txt = elm_object_item_content_get(it);
   if (!txt || line <= 0)
     return;

   tb = elm_entry_textblock_get(txt);
   mcur = evas_object_textblock_cursor_get(tb);

   evas_textblock_cursor_line_set(mcur, line-1);
   elm_entry_cursor_geometry_get(txt, &x, &y, &w, &h);
   elm_scroller_region_show(txt, x, y, w, h);
   elm_entry_calc_force(txt);

   elm_object_focus_set(txt, EINA_TRUE);
}

EAPI void
edi_mainview_add(Evas_Object *parent, Evas_Object *win)
{
   Evas_Object *box, *txt;
   Elm_Object_Item *it;

   _main_win = win;

   box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);
   elm_box_pack_end(parent, box);   
   
   tb = elm_toolbar_add(parent);
   evas_object_size_hint_weight_set(tb, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_toolbar_homogeneous_set(tb, EINA_FALSE);
   elm_toolbar_shrink_mode_set(tb, ELM_TOOLBAR_SHRINK_SCROLL);
   elm_toolbar_select_mode_set(tb, ELM_OBJECT_SELECT_MODE_ALWAYS);
   elm_box_pack_end(box, tb);
   evas_object_show(tb);

   nf = elm_naviframe_add(parent);
   evas_object_size_hint_weight_set(nf, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(nf, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, nf);
   evas_object_show(nf);

   txt = elm_label_add(parent);
   elm_object_text_set(txt, "Welcome - tap a file to edit");
   evas_object_size_hint_weight_set(txt, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(txt, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(txt);

   it = elm_naviframe_item_simple_push(nf, txt);
   elm_naviframe_item_style_set(it, "overlap");
   elm_toolbar_item_append(tb, NULL, "Welcome", _promote, it);
}
