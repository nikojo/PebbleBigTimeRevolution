// Copyright (C) 2013-2014 by Tom Fukushima. All Rights Reserved.
// Copyright (c) 2013 Douwe Maan <http://www.douwemaan.com/>
// The above copyright notice shall be included in all copies or substantial portions of the program.

// Revolution watchface...
// Envisioned as a watchface by Jean-Noël Mattern
// Based on the display of the Freebox Revolution, which was designed by Philippe Starck.

#include <pebble.h>

#define GRECT_FULL_WINDOW GRect(0,0,144,168)

// Settings
#define USE_AMERICAN_DATE_FORMAT      true

// Magic numbers
#define SCREEN_WIDTH        144
#define SCREEN_HEIGHT       168

#define TIME_IMAGE_WIDTH    58
#define TIME_IMAGE_HEIGHT   70

#define DATE_IMAGE_WIDTH    18
#define DATE_IMAGE_HEIGHT   20

#define SECOND_IMAGE_WIDTH  18
#define SECOND_IMAGE_HEIGHT 20

#define DAY_IMAGE_WIDTH     20
#define DAY_IMAGE_HEIGHT    20

#define MARGIN              1
#define MARGIN_TIME_X       13
#define MARGIN_DATE_X       2
#define TIME_SLOT_SPACE     2
#define DATE_PART_SPACE     9
#define DATE_CONTAINER_HEIGHT SCREEN_HEIGHT - SCREEN_WIDTH
#define DATE_DAY_GAP        2

#define BATTERY_IMAGE_WIDTH 8
#define BATTERY_IMAGE_HEIGHT 16



// Images
#define NUMBER_OF_TIME_IMAGES 10
const int TIME_IMAGE_RESOURCE_IDS[NUMBER_OF_TIME_IMAGES] = {
  RESOURCE_ID_IMAGE_TIME_0, 
  RESOURCE_ID_IMAGE_TIME_1, RESOURCE_ID_IMAGE_TIME_2, RESOURCE_ID_IMAGE_TIME_3, 
  RESOURCE_ID_IMAGE_TIME_4, RESOURCE_ID_IMAGE_TIME_5, RESOURCE_ID_IMAGE_TIME_6, 
  RESOURCE_ID_IMAGE_TIME_7, RESOURCE_ID_IMAGE_TIME_8, RESOURCE_ID_IMAGE_TIME_9
};

#define NUMBER_OF_DATE_IMAGES 10
const int DATE_IMAGE_RESOURCE_IDS[NUMBER_OF_DATE_IMAGES] = {
  RESOURCE_ID_IMAGE_DATE_0, 
  RESOURCE_ID_IMAGE_DATE_1, RESOURCE_ID_IMAGE_DATE_2, RESOURCE_ID_IMAGE_DATE_3, 
  RESOURCE_ID_IMAGE_DATE_4, RESOURCE_ID_IMAGE_DATE_5, RESOURCE_ID_IMAGE_DATE_6, 
  RESOURCE_ID_IMAGE_DATE_7, RESOURCE_ID_IMAGE_DATE_8, RESOURCE_ID_IMAGE_DATE_9
};

#define NUMBER_OF_SECOND_IMAGES 10
const int SECOND_IMAGE_RESOURCE_IDS[NUMBER_OF_SECOND_IMAGES] = {
  RESOURCE_ID_IMAGE_DATE_0, 
  RESOURCE_ID_IMAGE_DATE_1, RESOURCE_ID_IMAGE_DATE_2, RESOURCE_ID_IMAGE_DATE_3, 
  RESOURCE_ID_IMAGE_DATE_4, RESOURCE_ID_IMAGE_DATE_5, RESOURCE_ID_IMAGE_DATE_6, 
  RESOURCE_ID_IMAGE_DATE_7, RESOURCE_ID_IMAGE_DATE_8, RESOURCE_ID_IMAGE_DATE_9
};

#define NUMBER_OF_DAY_IMAGES 7
const int DAY_IMAGE_RESOURCE_IDS[NUMBER_OF_DAY_IMAGES] = {
  RESOURCE_ID_IMAGE_DAY_0, RESOURCE_ID_IMAGE_DAY_1, RESOURCE_ID_IMAGE_DAY_2, 
  RESOURCE_ID_IMAGE_DAY_3, RESOURCE_ID_IMAGE_DAY_4, RESOURCE_ID_IMAGE_DAY_5, 
  RESOURCE_ID_IMAGE_DAY_6
};


// Main
Layer *root_layer;
Window *window;
Layer *date_container_layer;

static uint8_t battery_level;
static bool battery_plugged;
static GBitmap *icon_battery;
static GBitmap *icon_battery_charge;
static Layer *battery_layer;

static AppTimer *recheck_bluetooth_timer;

static InverterLayer *full_inverse_layer;
static Layer *inverter_layer;

#define EMPTY_SLOT -1

typedef struct Slot {
  int           number;
  BitmapLayer   *image_layer;
  GBitmap       *bitmap;
  int           state;
} Slot;

#define NUMBER_OF_TIME_SLOTS 4
Layer *time_layer;
Slot time_slots[NUMBER_OF_TIME_SLOTS];

// Date
typedef struct DateSlot {
  Slot slot;
  GRect frame;
} DateSlot;

#define NUMBER_OF_DATE_SLOTS 4
Layer *date_layer;
int date_layer_width;
DateSlot date_slots[NUMBER_OF_DATE_SLOTS];

// Seconds
#define NUMBER_OF_SECOND_SLOTS 2
Layer *seconds_layer;
Slot second_slots[NUMBER_OF_SECOND_SLOTS];

// Day
typedef struct ImageItem {
  BitmapLayer   *image_layer;
  GBitmap       *bitmap;
  GRect         frame;
  bool          loaded;
} ImageItem;
ImageItem day_item;
ImageItem slash_item;

// General
BitmapLayer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids);
void unload_digit_image_from_slot(Slot *slot);
void unload_image_item(ImageItem * item);
void unload_day();
void unload_slash();
void create_date_layer(struct tm *tick_time);

// Display
void display_time(struct tm *tick_time);
void display_date(struct tm *tick_time);
void display_seconds(struct tm *tick_time);
void display_day(struct tm *tick_time);
void display_slash();

// Time
void display_time_value(int value, int row_number);
void update_time_slot(Slot *time_slot, int digit_value);
GRect frame_for_time_slot(Slot *time_slot);

// Date
void display_date_value(int value, int part_number);
void update_date_slot(DateSlot *date_slot, int digit_value);

// Seconds
void update_second_slot(Slot *second_slot, int digit_value);

// Connection
void fail_mode();
void reset_fail_mode();

// handlers
static void handle_battery(BatteryChargeState charge_state);
static void handle_tick(struct tm *tick_time, TimeUnits units_changed);

// startup
void init();
void deinit();


// General
BitmapLayer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids) {
  if (digit_value < 0 || digit_value > 19) {
    return NULL;
  }

  if (slot->state != EMPTY_SLOT) {
    return NULL;
  }

  slot->state = digit_value;
  slot->image_layer = bitmap_layer_create(frame);
  slot->bitmap = gbitmap_create_with_resource(digit_resource_ids[digit_value]);
  bitmap_layer_set_bitmap(slot->image_layer, slot->bitmap);
  Layer * layer = bitmap_layer_get_layer(slot->image_layer);
  layer_set_clips(layer, true);
  layer_add_child(parent_layer, layer);

  return slot->image_layer;
}

void unload_digit_image_from_slot(Slot *slot) {
  if (slot->state == EMPTY_SLOT) {
    return;
  }

  layer_remove_from_parent(bitmap_layer_get_layer(slot->image_layer));
  gbitmap_destroy(slot->bitmap);
  bitmap_layer_destroy(slot->image_layer);

  slot->state = EMPTY_SLOT;
}

void unload_image_item(ImageItem *item) {
  if (item->loaded) {
    layer_remove_from_parent(bitmap_layer_get_layer(item->image_layer));
    gbitmap_destroy(item->bitmap);
    bitmap_layer_destroy(item->image_layer);
    item->loaded = false;
  }
}


void unload_day() {
  unload_image_item(&day_item);
}

void unload_slash() {
  unload_image_item(&slash_item);
}

void create_date_frames(int left_digit_count, int right_digit_count) {
  GRect base_frame = GRect(0, 0, DATE_IMAGE_WIDTH, DATE_IMAGE_HEIGHT);
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; ++i)
    date_slots[i].frame = base_frame;
  date_slots[1].frame.origin.x = date_slots[0].frame.origin.x + (left_digit_count > 1 ? DATE_IMAGE_WIDTH : 0);
  date_slots[2].frame.origin.x = date_slots[1].frame.origin.x + DATE_IMAGE_WIDTH + DATE_PART_SPACE;
  date_slots[3].frame.origin.x = date_slots[2].frame.origin.x + (right_digit_count > 1 ? DATE_IMAGE_WIDTH : 0);
}

void create_date_layer(struct tm *tick_time) {
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; ++i) {
    unload_digit_image_from_slot(&date_slots[i].slot);
  }
  unload_slash();

  if (date_layer != NULL) {
    layer_remove_from_parent(date_layer);
    layer_destroy(date_layer);
    date_layer = NULL;
  }
  int month_digit_count = tick_time->tm_mon > 8 ? 2 : 1;
  int day_digit_count = tick_time->tm_mday > 9 ? 2 : 1; 

  date_layer_width = DATE_IMAGE_WIDTH * month_digit_count + DATE_PART_SPACE + DATE_IMAGE_WIDTH * day_digit_count;
  GRect date_layer_rect = GRect(MARGIN, DATE_CONTAINER_HEIGHT - SECOND_IMAGE_HEIGHT - MARGIN,  
    date_layer_width, DATE_IMAGE_HEIGHT + MARGIN);
  date_layer = layer_create(date_layer_rect);  
  layer_set_clips(date_layer, true);
  layer_add_child(date_container_layer, date_layer);

#if USE_AMERICAN_DATE_FORMAT
  slash_item.frame = GRect(month_digit_count * DATE_IMAGE_WIDTH, 
    0, DATE_PART_SPACE, DATE_IMAGE_HEIGHT);
  create_date_frames(month_digit_count, day_digit_count);
#else
  slash_item.frame = GRect(day_digit_count * DATE_IMAGE_WIDTH, 
    0, DATE_PART_SPACE, DATE_IMAGE_HEIGHT);
  create_date_frames(day_digit_count, month_digit_count);
#endif
}


// Display
void display_time(struct tm *tick_time) {
  int hour = tick_time->tm_hour;

  if (!clock_is_24h_style()) {
    hour = hour % 12;
    if (hour == 0) {
      hour = 12;
    }
  }

  display_time_value(hour,              0);
  display_time_value(tick_time->tm_min, 1);
}

void display_date(struct tm *tick_time) {
  int day   = tick_time->tm_mday;
  int month = tick_time->tm_mon + 1;

#if USE_AMERICAN_DATE_FORMAT
  display_date_value(month, 0);
  display_date_value(day,   1);
#else
  display_date_value(day,   0);
  display_date_value(month, 1);
#endif
}

void display_seconds(struct tm *tick_time) {
  int seconds = tick_time->tm_sec;

  seconds = seconds % 100; // Maximum of two digits per row.

  for (int second_slot_number = 1; second_slot_number >= 0; second_slot_number--) {
    Slot *second_slot = &second_slots[second_slot_number];

    int ix = seconds % 10;
    update_second_slot(second_slot, ix);
    
    seconds = seconds / 10;
  }
}

void display_item(ImageItem * item, uint32_t resource_id, Layer *parent) {
  if (item->loaded) {
    layer_remove_from_parent(bitmap_layer_get_layer(item->image_layer));
    gbitmap_destroy(item->bitmap);
    bitmap_layer_destroy(item->image_layer);
  }

  item->image_layer = bitmap_layer_create(item->frame);
  item->bitmap = gbitmap_create_with_resource(resource_id);
  bitmap_layer_set_bitmap(item->image_layer, item->bitmap);
  Layer * layer = bitmap_layer_get_layer(item->image_layer);
  layer_set_clips(layer, true);
  layer_add_child(parent, layer);

  item->loaded = true;
}

void display_day(struct tm *tick_time) {
  int ix = tick_time->tm_wday;
  day_item.frame = GRect(
    date_layer_width + MARGIN + DATE_DAY_GAP, 
    DATE_CONTAINER_HEIGHT - DAY_IMAGE_HEIGHT - MARGIN, 
    DAY_IMAGE_WIDTH, 
    DAY_IMAGE_HEIGHT
  );
  display_item(&day_item, DAY_IMAGE_RESOURCE_IDS[ix], date_container_layer);
}

void display_slash() {
  display_item(&slash_item, RESOURCE_ID_IMAGE_SLASH, date_layer);
}

// Time
void display_time_value(int value, int row_number) {
  value = value % 100; // Maximum of two digits per row.

  for (int column_number = 1; column_number >= 0; column_number--) {

    int time_slot_number = (row_number * 2) + column_number;

    Slot *time_slot = &time_slots[time_slot_number];

    if (row_number == 0 && value == 0 && column_number == 0) { // ignore the leading 0 for hours
      unload_digit_image_from_slot(time_slot);
      return;
    }
	
    int ix = value % 10;       
    update_time_slot(time_slot, ix);

    value = value / 10;
  }
}

void update_time_slot(Slot *time_slot, int digit_value) {
  if (time_slot->state == digit_value) {
    return;
  }

  GRect frame = frame_for_time_slot(time_slot);

  unload_digit_image_from_slot(time_slot);
  load_digit_image_into_slot(time_slot, digit_value, time_layer, frame, TIME_IMAGE_RESOURCE_IDS);

}

GRect frame_for_time_slot(Slot *time_slot) {
  int x = MARGIN_TIME_X + (time_slot->number % 2) * (TIME_IMAGE_WIDTH + TIME_SLOT_SPACE);
  int y = MARGIN + (time_slot->number / 2) * (TIME_IMAGE_HEIGHT + TIME_SLOT_SPACE);

  return GRect(x, y, TIME_IMAGE_WIDTH, TIME_IMAGE_HEIGHT);
}


// Date
void display_date_value(int value, int part_number) {
  value = value % 100; // Maximum of two digits per row.

  for (int column_number = 1; column_number >= 0; column_number--) {

    DateSlot *date_slot = &date_slots[column_number + part_number*2];

    if (column_number == 0 && value == 0) {  // ignore the leading 0
      unload_digit_image_from_slot(&date_slot->slot);
    } else {
      int ix = value % 10;
      update_date_slot(date_slot, ix);
    }

    value = value / 10;
  }
}

void update_date_slot(DateSlot *date_slot, int digit_value) {
  if (date_slot->slot.state == digit_value) {
    return;
  }

  unload_digit_image_from_slot(&date_slot->slot);
  load_digit_image_into_slot(&date_slot->slot, digit_value, date_layer, 
    date_slot->frame, DATE_IMAGE_RESOURCE_IDS);
}

// Seconds
void update_second_slot(Slot *second_slot, int digit_value) {
  if (second_slot->state == digit_value) {
    return;
  }

  GRect frame = GRect(
    second_slot->number * (SECOND_IMAGE_WIDTH + MARGIN), 
    0, 
    SECOND_IMAGE_WIDTH, 
    SECOND_IMAGE_HEIGHT
  );

  unload_digit_image_from_slot(second_slot);
  load_digit_image_into_slot(second_slot, digit_value, seconds_layer, frame, SECOND_IMAGE_RESOURCE_IDS);
}

void fail_mode() {
  vibes_long_pulse();
  layer_add_child(root_layer, inverter_layer);
}

void reset_fail_mode() {
  layer_remove_from_parent(inverter_layer);
}


int main() {
  init();
  app_event_loop();
  deinit();
}

static void handle_battery(BatteryChargeState charge) {

  battery_level = charge.charge_percent;
  battery_plugged = charge.is_plugged;
  layer_mark_dirty(battery_layer);
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  if ((units_changed & MINUTE_UNIT) == MINUTE_UNIT) {
    display_time(tick_time);
  }

  if ((units_changed & DAY_UNIT) == DAY_UNIT) {
    create_date_layer(tick_time);
    display_day(tick_time);
    display_date(tick_time);
    display_slash();
  }


  display_seconds(tick_time);
}

/*
 * Battery icon callback handler
 */
void battery_layer_update_callback(Layer *layer, GContext *ctx) {

  graphics_context_set_compositing_mode(ctx, GCompOpAssign);

  if (!battery_plugged) {
    graphics_draw_bitmap_in_rect(ctx, icon_battery, GRect(0, 0, BATTERY_IMAGE_WIDTH, BATTERY_IMAGE_HEIGHT));
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_fill_color(ctx, GColorWhite);
    int height = (uint8_t)((battery_level / 100.0) * 11.0);
    graphics_fill_rect(ctx, GRect(2, 14 - height, 4, height), 0, GCornerNone);
  } else {
    graphics_draw_bitmap_in_rect(ctx, icon_battery_charge, GRect(0, 0, BATTERY_IMAGE_WIDTH, BATTERY_IMAGE_HEIGHT));
  }
}

void recheck_bluetooth(void *data) {
    app_timer_cancel(recheck_bluetooth_timer);
    if (!bluetooth_connection_service_peek()) {
      fail_mode();
    }
}

void bluetooth_connection_handler(bool connected) {
  if (connected) {
    reset_fail_mode();
  } else {
    recheck_bluetooth_timer = app_timer_register(3000, recheck_bluetooth, NULL);
  }
}


void init() {

  window = window_create();
  window_stack_push(window, true /* Animated */);

  // Time slots
  for (int i = 0; i < NUMBER_OF_TIME_SLOTS; i++) {
    Slot *time_slot = &time_slots[i];
    time_slot->number  = i;
    time_slot->state   = EMPTY_SLOT;
    time_slot->image_layer = NULL;
    time_slot->bitmap  = NULL;
  }

  // Date slots
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; i++) {
    DateSlot *date_slot = &date_slots[i];
    date_slot->slot.number = i;
    date_slot->slot.state  = EMPTY_SLOT;
    date_slot->slot.image_layer = NULL;
    date_slot->slot.bitmap  = NULL;
    date_slot->frame = GRectZero;
  }

  // Second slots
  for (int i = 0; i < NUMBER_OF_SECOND_SLOTS; i++) {
    Slot *second_slot = &second_slots[i];
    second_slot->number = i;
    second_slot->state  = EMPTY_SLOT;
    second_slot->image_layer = NULL;
    second_slot->bitmap  = NULL;
  }

  // Root layer
  root_layer = window_get_root_layer(window);

  // Time
  time_layer = layer_create(GRect(0, 0, SCREEN_WIDTH, SCREEN_WIDTH));
  layer_set_clips(time_layer, true);
  layer_add_child(root_layer, time_layer);

  // Date container
  date_container_layer = layer_create(GRect(0, SCREEN_WIDTH, SCREEN_WIDTH, DATE_CONTAINER_HEIGHT));
  layer_set_clips(date_container_layer, true);
  layer_add_child(root_layer, date_container_layer);

  struct tm *tick_time;
  time_t current_time = time(NULL);
  tick_time = localtime(&current_time);

  // Slash
  slash_item.loaded = false;

  // Day slot
  day_item.loaded = false;

  // Date
  date_layer = NULL;
  create_date_layer(tick_time);

  // Seconds
  GRect seconds_layer_frame = GRect(
    SCREEN_WIDTH - SECOND_IMAGE_WIDTH - MARGIN - SECOND_IMAGE_WIDTH - MARGIN, 
    DATE_CONTAINER_HEIGHT - SECOND_IMAGE_HEIGHT - MARGIN, 
    SECOND_IMAGE_WIDTH + MARGIN + SECOND_IMAGE_WIDTH, 
    SECOND_IMAGE_HEIGHT
  );
  seconds_layer = layer_create(seconds_layer_frame);
  layer_set_clips(seconds_layer, true);
  layer_add_child(date_container_layer, seconds_layer);

  // Battery status setup
  icon_battery = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY);
  icon_battery_charge = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING);

  battery_layer = layer_create(GRect(SCREEN_WIDTH-BATTERY_IMAGE_WIDTH,4,BATTERY_IMAGE_WIDTH,BATTERY_IMAGE_HEIGHT));
  BatteryChargeState initial = battery_state_service_peek();
  battery_level = initial.charge_percent;
  battery_plugged = initial.is_plugged;
  layer_set_update_proc(battery_layer, &battery_layer_update_callback);
  layer_add_child(root_layer, battery_layer);

  // Inverter
  full_inverse_layer = inverter_layer_create(GRECT_FULL_WINDOW);
  inverter_layer = inverter_layer_get_layer(full_inverse_layer);

  // Display
  window_set_background_color(window, GColorBlack);

  display_time(tick_time);
  display_day(tick_time);
  display_date(tick_time);
  display_slash();
  display_seconds(tick_time);

  tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
  battery_state_service_subscribe(&handle_battery);
  handle_battery(battery_state_service_peek());
  bluetooth_connection_service_subscribe(&bluetooth_connection_handler);
  bluetooth_connection_handler(bluetooth_connection_service_peek());
}

void deinit() {
  for (int i = 0; i < NUMBER_OF_TIME_SLOTS; i++) {
    unload_digit_image_from_slot(&time_slots[i]);
  }
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; i++) {
    unload_digit_image_from_slot(&date_slots[i].slot);
  }
  for (int i = 0; i < NUMBER_OF_SECOND_SLOTS; i++) {
    unload_digit_image_from_slot(&second_slots[i]);
  }

  gbitmap_destroy(icon_battery);
  gbitmap_destroy(icon_battery_charge);
  layer_destroy(battery_layer);

  unload_day();
  unload_slash();
  layer_destroy(date_container_layer);
  layer_destroy(time_layer);
  inverter_layer_destroy(full_inverse_layer);
  window_destroy(window);
}
