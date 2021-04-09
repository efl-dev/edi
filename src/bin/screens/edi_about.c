#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <Elementary.h>
#include <Ecore.h>

#include "edi_private.h"

static void
_edi_about_exit(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   evas_object_del(data);
}

static void
_edi_about_url_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   edi_open_url((const char *)data);
}

Evas_Object *
edi_about_show(Evas_Object *mainwin)
{
   Evas_Object *win, *vbox, *box, *table, *bg;
   Evas_Object *text, *title, *authors, *buttonbox, *button, *space;
   const char *title_text;
   int alpha, r, g, b;
   char buf[PATH_MAX];

   win = elm_win_add(mainwin, "about", ELM_WIN_BASIC);
   if (!win) return NULL;

   title_text = eina_slstr_printf(_("About Edi %s"), PACKAGE_VERSION);
   elm_win_title_set(win, title_text);
   evas_object_smart_callback_add(win, "delete,request", _edi_about_exit, win);

   table = elm_table_add(win);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_win_resize_object_add(win, table);
   evas_object_show(table);

   bg = elm_bg_add(win);
   evas_object_color_set(bg, 26, 26, 26, 255);
   evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(bg, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(table, bg, 0, 0, 1, 1);
   evas_object_show(bg);

   snprintf(buf, sizeof(buf), "%s/images/about.png", elm_app_data_dir_get());
   bg = elm_image_add(win);
   elm_image_fill_outside_set(bg, EINA_TRUE);
   elm_image_file_set(bg, buf, NULL);
   evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(bg, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(table, bg, 0, 0, 1, 1);

   evas_object_color_get(bg, &r, &g, &b, &alpha);
   evas_color_argb_unpremul(alpha, &r, &g, &b);
   alpha = 64;

   evas_color_argb_premul(alpha, &r, &g, &b);
   evas_object_color_set(bg, r, g, b, alpha);
   evas_object_show(bg);

   vbox = elm_box_add(win);
   elm_box_padding_set(vbox, 25, 0);
   elm_box_horizontal_set(vbox, EINA_TRUE);
   evas_object_size_hint_weight_set(vbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(vbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(table, vbox, 0, 0, 1, 1);
   evas_object_show(vbox);

   elm_box_pack_end(vbox, elm_box_add(vbox));
   box = elm_box_add(win);
   elm_box_padding_set(box, 10, 0);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(vbox, box);
   evas_object_show(box);
   elm_box_pack_end(vbox, elm_box_add(vbox));

   text = elm_label_add(box);
   elm_object_text_set(text, _("<br>EDI is an IDE designed to get people into coding for Unix.<br>" \
                             "It's based on the <b>EFL</b> and written completely natively<br>" \
                             "to provide a <i>responsive</i> and <i>beautiful</i> UI.<br>"));
   evas_object_size_hint_weight_set(text, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(text, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, text);
   evas_object_show(text);

   title = elm_entry_add(box);
   elm_object_text_set(title, _("<hilight>EDI's lovely authors</hilight>"));
   elm_entry_editable_set(title, EINA_FALSE);
   elm_object_focus_allow_set(title, EINA_FALSE);
   evas_object_size_hint_weight_set(title, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(title, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, title);
   evas_object_show(title);

   /* Authors screen */
   authors = elm_entry_add(box);
   elm_entry_line_wrap_set(authors, EINA_FALSE);
   elm_entry_text_style_user_push(authors, "DEFAULT='font=Mono')");
   elm_entry_file_set(authors, PACKAGE_DOC_DIR "/AUTHORS", ELM_TEXT_FORMAT_PLAIN_UTF8);
   elm_entry_editable_set(authors, EINA_FALSE);
   elm_object_focus_allow_set(authors, EINA_FALSE);
   evas_object_size_hint_weight_set(authors, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(authors, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, authors);
   evas_object_show(authors);

   buttonbox = elm_box_add(box);
   elm_box_horizontal_set(buttonbox, EINA_TRUE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, buttonbox);
   evas_object_show(buttonbox);

   space = elm_box_add(box);
   evas_object_size_hint_min_set(space, 0, 14 * elm_config_scale_get());
   elm_box_pack_end(box, space);
   evas_object_show(space);

   button = elm_button_add(box);
   elm_object_text_set(button, _("Visit Website"));
   evas_object_smart_callback_add(button, "clicked", _edi_about_url_cb,
                                  "https://www.enlightenment.org/about-edi.md");
   elm_box_pack_end(buttonbox, button);
   evas_object_show(button);

   space = elm_box_add(box);
   evas_object_size_hint_min_set(space, 20 * elm_config_scale_get(), 0);
   elm_box_pack_end(buttonbox, space);
   evas_object_show(space);

   button = elm_button_add(box);
   elm_object_text_set(button, _("Report Issues"));
   evas_object_smart_callback_add(button, "clicked", _edi_about_url_cb,
                                  "https://phab.enlightenment.org");
   elm_box_pack_end(buttonbox, button);
   evas_object_show(button);

   space = elm_box_add(box);
   evas_object_size_hint_min_set(space, 20 * elm_config_scale_get(), 0);
   elm_box_pack_end(buttonbox, space);
   evas_object_show(space);

   space = elm_box_add(box);
   evas_object_size_hint_min_set(space, 20 * elm_config_scale_get(), 0);
   elm_box_pack_end(buttonbox, space);
   evas_object_show(space);

   evas_object_resize(win, 520 * elm_config_scale_get(), 360 * elm_config_scale_get());
   evas_object_show(win);

   return win;
}
