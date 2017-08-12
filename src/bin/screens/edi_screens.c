#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <Elementary.h>
#include <Ecore.h>

#include "edi_private.h"
#include "edi_screens.h"

static Evas_Object *_edi_screens_popup = NULL;

static void
_edi_screens_popup_cancel_cb(void *data, Evas_Object *obj EINA_UNUSED,
                             void *event_info EINA_UNUSED)
{
   evas_object_del((Evas_Object *) data);
}

static void
_edi_screens_message_confirm_cb(void *data, Evas_Object *obj,
                                void *event_info EINA_UNUSED)
{
   void ((*confirm_fn)(void *)) = evas_object_data_get(obj, "callback");

   confirm_fn(data);

   if (_edi_screens_popup)
     {
        evas_object_del(_edi_screens_popup);
        _edi_screens_popup = NULL;
     }
}

void edi_screens_message_confirm(Evas_Object *parent, const char *message, void ((*confirm_cb)(void *)), void *data)
{
   Evas_Object *popup, *table, *label, *button, *icon;

   _edi_screens_popup = popup = elm_popup_add(parent);
   elm_object_part_text_set(popup, "title,text", "Confirmation required");

   table = elm_table_add(popup);

   icon = elm_icon_add(table);
   elm_icon_standard_set(icon, "dialog-warning");
   evas_object_size_hint_min_set(icon, 48 * elm_config_scale_get(), 48 * elm_config_scale_get());
   evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(icon, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(icon);
   elm_table_pack(table, icon, 0, 0, 1, 1);

   label = elm_label_add(table);
   elm_object_text_set(label, message);
   evas_object_show(label);
   elm_table_pack(table, label, 1, 0, 1, 1);
   elm_object_content_set(popup, table);

   evas_object_show(table);

   button = elm_button_add(popup);
   elm_object_text_set(button, "Yes");
   elm_object_part_content_set(popup, "button1", button);
   evas_object_data_set(button, "callback", confirm_cb);
   evas_object_smart_callback_add(button, "clicked", _edi_screens_message_confirm_cb, data);

   button = elm_button_add(popup);
   elm_object_text_set(button, "No");
   elm_object_part_content_set(popup, "button2", button);
   evas_object_smart_callback_add(button, "clicked", _edi_screens_popup_cancel_cb, popup);

   evas_object_show(popup);
}
