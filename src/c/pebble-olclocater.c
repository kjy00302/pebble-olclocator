#include <pebble.h>
#include "localize.h"

const int REQ_GET_LOCATION = 0;

static Window *s_window;
static StatusBarLayer *s_status_layer;
static ActionBarLayer *s_actionbar_layer;

static GBitmap *s_bitmap_refresh;
static GBitmap *s_bitmap_auto_refresh;
static GBitmap *s_bitmap_manual_refresh;

static Layer *s_pluscode_layer;
static TextLayer *s_pre_shortcode_text_layer;
static TextLayer *s_shortcode_text_layer;
static TextLayer *s_accuracy_text_layer;

static TextLayer *s_address_text_layer;

static AppTimer *s_autorefresh_timer;
static int autorefresh = 0;

static void prv_send_appmessage(int n){
    DictionaryIterator *out_iter;
    AppMessageResult result = app_message_outbox_begin(&out_iter);
    if (result == APP_MSG_OK){
        dict_write_int(out_iter, MESSAGE_KEY_Request, &n, sizeof(int), true);
        result = app_message_outbox_send();
        if(result != APP_MSG_OK) {
            APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending the outbox: %d", (int)result);
        }
    }
    else{
        APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing the outbox: %d", (int)result);
    }
}

static void prv_req_get_location(){
    text_layer_set_text(s_pre_shortcode_text_layer, "????");
    text_layer_set_text(s_shortcode_text_layer, "????+??");
    text_layer_set_text(s_accuracy_text_layer, "±??m");
    if (connection_service_peek_pebble_app_connection()){
        text_layer_set_text(s_address_text_layer, _("Locating..."));
        prv_send_appmessage(REQ_GET_LOCATION);
    }
    else{
        text_layer_set_text(s_address_text_layer, _("Not connected"));
    }
}

static void prv_recv_appmessage(DictionaryIterator *iter, void *context){
    Tuple *pluscode_tuple = dict_find(iter, MESSAGE_KEY_PlusCode);
    if (pluscode_tuple){
        char *pluscode = pluscode_tuple->value->cstring;
        static char code_p1[5];
        static char code_p2[12];
        strncpy(code_p1, pluscode, 4);
        code_p1[4] = '\0';
        strncpy(code_p2, pluscode+4, 12);
        text_layer_set_text(s_pre_shortcode_text_layer, code_p1);
        text_layer_set_text(s_shortcode_text_layer, code_p2);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "PlusCode: %s", pluscode);
    }
    
    Tuple *rgeo_tuple = dict_find(iter, MESSAGE_KEY_RGeoResult);
    if (rgeo_tuple){
        char *rgeo = rgeo_tuple->value->cstring;
        if (rgeo[0] != '\0'){
            text_layer_set_text(s_address_text_layer, rgeo);
            APP_LOG(APP_LOG_LEVEL_DEBUG, "ReverseGeo: %s", rgeo);
        }
        else{
            text_layer_set_text(s_address_text_layer, "");
            APP_LOG(APP_LOG_LEVEL_DEBUG, "ReverseGeo None");
        }
    }
    
    Tuple *accuracy_tuple = dict_find(iter, MESSAGE_KEY_Accuracy);
    if (accuracy_tuple){
        int32_t accuracy = accuracy_tuple->value->int32;
        static char accuracy_string[16];
        if (accuracy >= 1000){
            snprintf(accuracy_string, sizeof(accuracy_string), "±%ld.%ldkm", accuracy/1000, (accuracy%1000)/100);
        }
        else{
            snprintf(accuracy_string, sizeof(accuracy_string), "±%ldm", accuracy);
        }
        text_layer_set_text(s_accuracy_text_layer, accuracy_string);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Accuracy: %s", accuracy_string);
    }
    
    Tuple *request_tuple = dict_find(iter, MESSAGE_KEY_Request);
    if (request_tuple){
        int32_t request = request_tuple->value->int32;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Req: %ld", request);
        if (request == 0){
            text_layer_set_text(s_address_text_layer, "");
        }
        else if (request == 2){
            text_layer_set_text(s_address_text_layer, _("Locating failed"));
        }
    }
}

static void prv_autorefresh_timer_callback(){
    if (autorefresh){
        APP_LOG(APP_LOG_LEVEL_DEBUG, "AutoRefresh!");
        prv_req_get_location();
        s_autorefresh_timer = app_timer_register(10000, prv_autorefresh_timer_callback, NULL);
    }
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(s_text_layer, "Select");
  prv_req_get_location();
}
/*
static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(s_text_layer, "Up");
  prv_send_appmessage(1);
}
*/
static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(s_text_layer, "Down");
  if (autorefresh){
    action_bar_layer_set_icon_animated(s_actionbar_layer, BUTTON_ID_DOWN, s_bitmap_manual_refresh, false);
    app_timer_cancel(s_autorefresh_timer);
    autorefresh = 0;
  }
  else {
    action_bar_layer_set_icon_animated(s_actionbar_layer, BUTTON_ID_DOWN, s_bitmap_auto_refresh, false);
    s_autorefresh_timer = app_timer_register(10000, prv_autorefresh_timer_callback, NULL);
    autorefresh = 1;
  }
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  //window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  GRect pluscode_bounds = GRect(PBL_IF_RECT_ELSE(0,33),STATUS_BAR_LAYER_HEIGHT,bounds.size.w,68);
  s_pluscode_layer = layer_create(pluscode_bounds);

  s_status_layer = status_bar_layer_create();
  layer_add_child(window_layer, status_bar_layer_get_layer(s_status_layer));
  
  s_bitmap_refresh = gbitmap_create_with_resource(RESOURCE_ID_ICON_REFRESH);
  s_bitmap_auto_refresh = gbitmap_create_with_resource(RESOURCE_ID_ICON_AUTO_REFRESH);
  s_bitmap_manual_refresh = gbitmap_create_with_resource(RESOURCE_ID_ICON_MANUAL_REFRESH);
  
  s_actionbar_layer = action_bar_layer_create();
  action_bar_layer_add_to_window(s_actionbar_layer, window);
  action_bar_layer_set_click_config_provider(s_actionbar_layer, prv_click_config_provider);
  action_bar_layer_set_icon_animated(s_actionbar_layer, BUTTON_ID_SELECT, s_bitmap_refresh, false);
  action_bar_layer_set_icon_animated(s_actionbar_layer, BUTTON_ID_DOWN, s_bitmap_manual_refresh, false);

  s_pre_shortcode_text_layer = text_layer_create(GRect(0,0, 50, 34));
  text_layer_set_font(s_pre_shortcode_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text(s_pre_shortcode_text_layer, "????");
  layer_add_child(s_pluscode_layer, text_layer_get_layer(s_pre_shortcode_text_layer));
  
  s_shortcode_text_layer = text_layer_create(GRect(0,34, 100, 34));
  text_layer_set_font(s_shortcode_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text(s_shortcode_text_layer, "????+??");
  layer_add_child(s_pluscode_layer, text_layer_get_layer(s_shortcode_text_layer));

  s_accuracy_text_layer = text_layer_create(GRect(50, 10, bounds.size.w-50-ACTION_BAR_WIDTH-pluscode_bounds.origin.x, 24));
  text_layer_set_font(s_accuracy_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_accuracy_text_layer, GTextAlignmentRight);
  text_layer_set_text(s_accuracy_text_layer, "±??m");
  layer_add_child(s_pluscode_layer, text_layer_get_layer(s_accuracy_text_layer));
  
  layer_add_child(window_layer, s_pluscode_layer);
  
  s_address_text_layer = text_layer_create(GRect(0, 68+STATUS_BAR_LAYER_HEIGHT, bounds.size.w-ACTION_BAR_WIDTH, bounds.size.h-85));
  if (connection_service_peek_pebble_app_connection()){
      text_layer_set_text(s_address_text_layer, _("Locating..."));
  }
  else{
      text_layer_set_text(s_address_text_layer, _("Not connected"));
  }
  layer_add_child(window_layer, text_layer_get_layer(s_address_text_layer));
  text_layer_enable_screen_text_flow_and_paging(s_address_text_layer, 5);
  
}

static void prv_window_unload(Window *window) {
  status_bar_layer_destroy(s_status_layer);
  action_bar_layer_destroy(s_actionbar_layer);
  
  gbitmap_destroy(s_bitmap_refresh);
  gbitmap_destroy(s_bitmap_auto_refresh);
  gbitmap_destroy(s_bitmap_manual_refresh);
  
  text_layer_destroy(s_pre_shortcode_text_layer);
  text_layer_destroy(s_shortcode_text_layer);
  text_layer_destroy(s_accuracy_text_layer);
  layer_destroy(s_pluscode_layer);
  
  text_layer_destroy(s_address_text_layer);
}

static void prv_init(void) {
  app_message_open(128, 16);
  app_message_register_inbox_received(prv_recv_appmessage);
  s_window = window_create();
  //window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  locale_init();
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}
