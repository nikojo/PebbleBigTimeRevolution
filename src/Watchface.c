// Copyright (C) 2013-2014 by Tom Fukushima. All Rights Reserved.
// Copyright (c) 2013 Douwe Maan <http://www.douwemaan.com/>
// The above copyright notice shall be included in all copies or substantial portions of the program.

// Revolution watchface...
// Envisioned as a watchface by Jean-Noël Mattern
// Based on the display of the Freebox Revolution, which was designed by Philippe Starck.

#include <pebble.h>

// Settings
#define USE_AMERICAN_DATE_FORMAT      true
#define TIME_SLOT_ANIMATION_DURATION  500

// Magic numbers
#define SCREEN_WIDTH        144
#define SCREEN_HEIGHT       168

#define TIME_IMAGE_WIDTH    58
#define TIME_IMAGE_HEIGHT   70

#define DATE_IMAGE_WIDTH    18
#define DATE_IMAGE_HEIGHT   20

#define SECOND_IMAGE_WIDTH  10
#define SECOND_IMAGE_HEIGHT 10

#define DAY_IMAGE_WIDTH     20
#define DAY_IMAGE_HEIGHT    20

#define MARGIN              1
#define MARGIN_TIME_X       13
#define MARGIN_DATE_X       2
#define TIME_SLOT_SPACE     2
#define DATE_PART_SPACE     10


// Images
#define NUMBER_OF_TIME_IMAGES 11
const int TIME_IMAGE_RESOURCE_IDS[NUMBER_OF_TIME_IMAGES] = {
  RESOURCE_ID_IMAGE_TIME_0, 
  RESOURCE_ID_IMAGE_TIME_1, RESOURCE_ID_IMAGE_TIME_2, RESOURCE_ID_IMAGE_TIME_3, 
  RESOURCE_ID_IMAGE_TIME_4, RESOURCE_ID_IMAGE_TIME_5, RESOURCE_ID_IMAGE_TIME_6, 
  RESOURCE_ID_IMAGE_TIME_7, RESOURCE_ID_IMAGE_TIME_8, RESOURCE_ID_IMAGE_TIME_9
};

#define NUMBER_OF_DATE_IMAGES 11
const int DATE_IMAGE_RESOURCE_IDS[NUMBER_OF_DATE_IMAGES] = {
  RESOURCE_ID_IMAGE_DATE_0, 
  RESOURCE_ID_IMAGE_DATE_1, RESOURCE_ID_IMAGE_DATE_2, RESOURCE_ID_IMAGE_DATE_3, 
  RESOURCE_ID_IMAGE_DATE_4, RESOURCE_ID_IMAGE_DATE_5, RESOURCE_ID_IMAGE_DATE_6, 
  RESOURCE_ID_IMAGE_DATE_7, RESOURCE_ID_IMAGE_DATE_8, RESOURCE_ID_IMAGE_DATE_9
};

#define NUMBER_OF_SECOND_IMAGES 10
const int SECOND_IMAGE_RESOURCE_IDS[NUMBER_OF_SECOND_IMAGES] = {
  RESOURCE_ID_IMAGE_SECOND_0, 
  RESOURCE_ID_IMAGE_SECOND_1, RESOURCE_ID_IMAGE_SECOND_2, RESOURCE_ID_IMAGE_SECOND_3, 
  RESOURCE_ID_IMAGE_SECOND_4, RESOURCE_ID_IMAGE_SECOND_5, RESOURCE_ID_IMAGE_SECOND_6, 
  RESOURCE_ID_IMAGE_SECOND_7, RESOURCE_ID_IMAGE_SECOND_8, RESOURCE_ID_IMAGE_SECOND_9
};

#define NUMBER_OF_DAY_IMAGES 7
const int DAY_IMAGE_RESOURCE_IDS[NUMBER_OF_DAY_IMAGES] = {
  RESOURCE_ID_IMAGE_DAY_0, RESOURCE_ID_IMAGE_DAY_1, RESOURCE_ID_IMAGE_DAY_2, 
  RESOURCE_ID_IMAGE_DAY_3, RESOURCE_ID_IMAGE_DAY_4, RESOURCE_ID_IMAGE_DAY_5, 
  RESOURCE_ID_IMAGE_DAY_6
};


// Main
Window *window;
Layer *date_container_layer;

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
#define NUMBER_OF_DATE_SLOTS 4
Layer *date_layer;
Slot date_slots[NUMBER_OF_DATE_SLOTS];

// Seconds
#define NUMBER_OF_SECOND_SLOTS 2
Layer *seconds_layer;
Slot second_slots[NUMBER_OF_SECOND_SLOTS];

// Day
typedef struct DayItem {
  BitmapLayer   *image_layer;
  GBitmap       *bitmap;
  Layer         *layer;
  bool          loaded;
} DayItem;
DayItem day_item;
GRect day_frame;

// General
BitmapLayer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids);
void unload_digit_image_from_slot(Slot *slot);

// Display
void display_time(struct tm *tick_time);
void display_date(struct tm *tick_time);
void display_seconds(struct tm *tick_time);
void display_day(struct tm *tick_time);

// Time
void display_time_value(int value, int row_number);
void update_time_slot(Slot *time_slot, int digit_value);
GRect frame_for_time_slot(Slot *time_slot);

// Date
void display_date_value(int value, int part_number);
void update_date_slot(Slot *date_slot, int digit_value);

// Seconds
void update_second_slot(Slot *second_slot, int digit_value);

// handlers
static void handle_tick(struct tm *tick_time, TimeUnits units_changed);

// startup
void init();
void deinit();


// General
BitmapLayer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids) {
  if (digit_value < 0 || digit_value > 10) {
    return NULL;
  }

  if (slot->state != EMPTY_SLOT) {
    return NULL;
  }

  slot->state = digit_value;
  slot->image_layer = bitmap_layer_create(frame);
  slot->bitmap = gbitmap_create_with_resource(digit_resource_ids[digit_value]);
  bitmap_layer_set_bitmap(slot->image_layer, slot->bitmap);
  layer_add_child(parent_layer, bitmap_layer_get_layer(slot->image_layer));

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

    update_second_slot(second_slot, seconds % 10);
    
    seconds = seconds / 10;
  }
}

void display_day(struct tm *tick_time) {
  if (day_item.loaded) {
    layer_remove_from_parent(bitmap_layer_get_layer(day_item.image_layer));
    gbitmap_destroy(day_item.bitmap);
    bitmap_layer_destroy(day_item.image_layer);
  }

  day_item.image_layer = bitmap_layer_create(day_frame);
  day_item.bitmap = gbitmap_create_with_resource(DAY_IMAGE_RESOURCE_IDS[tick_time->tm_wday]);
  bitmap_layer_set_bitmap(day_item.image_layer, day_item.bitmap);
  layer_add_child(day_item.layer, bitmap_layer_get_layer(day_item.image_layer));

  day_item.loaded = true;
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
	
    update_time_slot(time_slot, value % 10);

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

    int date_slot_number = (part_number * 2) + column_number;

    Slot *date_slot = &date_slots[date_slot_number];

#if USE_AMERICAN_DATE_FORMAT
    if (part_number == 0 && column_number == 0 && value == 0) {  // ignore the leading 0 for months
      unload_digit_image_from_slot(date_slot);
      return;
    }
#endif
    
    update_date_slot(date_slot, value % 10);

    value = value / 10;
  }
}

void update_date_slot(Slot *date_slot, int digit_value) {
  if (date_slot->state == digit_value) {
    return;
  }

  int x = date_slot->number * DATE_IMAGE_WIDTH;
  if (date_slot->number >= 2) {
    x += DATE_PART_SPACE; // extra pixels of space between the day and month
  }
  GRect frame =  GRect(x, 0, DATE_IMAGE_WIDTH, DATE_IMAGE_HEIGHT);

  unload_digit_image_from_slot(date_slot);
  load_digit_image_into_slot(date_slot, digit_value, date_layer, frame, DATE_IMAGE_RESOURCE_IDS);
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

int main() {
  init();
  app_event_loop();
  deinit();
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  display_seconds(tick_time);

  if ((units_changed & MINUTE_UNIT) == MINUTE_UNIT) {
    display_time(tick_time);
  }

  if ((units_changed & DAY_UNIT) == DAY_UNIT) {
    display_day(tick_time);
    display_date(tick_time);
  }
}

void init() {

  window = window_create();
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

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
    Slot *date_slot = &date_slots[i];
    date_slot->number = i;
    date_slot->state  = EMPTY_SLOT;
    date_slot->image_layer = NULL;
    date_slot->bitmap  = NULL;
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
  Layer *root_layer = window_get_root_layer(window);

  // Time
  time_layer = layer_create(GRect(0, 0, SCREEN_WIDTH, SCREEN_WIDTH));
  layer_set_clips(time_layer, true);
  layer_add_child(root_layer, time_layer);

  // Date container
  int date_container_height = SCREEN_HEIGHT - SCREEN_WIDTH;

  date_container_layer = layer_create(GRect(0, SCREEN_WIDTH, SCREEN_WIDTH, date_container_height));
  layer_add_child(root_layer, date_container_layer);

  // Day
  day_frame = GRect(
    MARGIN, 
    date_container_height - DAY_IMAGE_HEIGHT - MARGIN, 
    DAY_IMAGE_WIDTH, 
    DAY_IMAGE_HEIGHT
  );

  // Day slot
  day_item.loaded = false;
  day_item.layer = date_container_layer;

  // Date
  GRect date_layer_frame = GRectZero;
  date_layer_frame.size.w   = DATE_IMAGE_WIDTH + DATE_IMAGE_WIDTH + DATE_PART_SPACE + DATE_IMAGE_WIDTH + DATE_IMAGE_WIDTH;
  date_layer_frame.size.h   = DATE_IMAGE_HEIGHT;
  date_layer_frame.origin.x = (SCREEN_WIDTH - date_layer_frame.size.w) / 2;
  date_layer_frame.origin.y = date_container_height - DATE_IMAGE_HEIGHT - MARGIN;

  date_layer = layer_create(date_layer_frame);
  layer_add_child(date_container_layer, date_layer);

  // Seconds
  GRect seconds_layer_frame = GRect(
    SCREEN_WIDTH - SECOND_IMAGE_WIDTH - MARGIN - SECOND_IMAGE_WIDTH - MARGIN, 
    date_container_height - SECOND_IMAGE_HEIGHT - MARGIN, 
    SECOND_IMAGE_WIDTH + MARGIN + SECOND_IMAGE_WIDTH, 
    SECOND_IMAGE_HEIGHT
  );
  seconds_layer = layer_create(seconds_layer_frame);
  layer_add_child(date_container_layer, seconds_layer);


  // Display
  struct tm *tick_time;
  time_t current_time = time(NULL);
  tick_time = localtime(&current_time);

  display_time(tick_time);
  display_day(tick_time);
  display_date(tick_time);
  display_seconds(tick_time);

  tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
}

void deinit() {
  for (int i = 0; i < NUMBER_OF_TIME_SLOTS; i++) {
    unload_digit_image_from_slot(&time_slots[i]);
  }
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; i++) {
    unload_digit_image_from_slot(&date_slots[i]);
  }
  for (int i = 0; i < NUMBER_OF_SECOND_SLOTS; i++) {
    unload_digit_image_from_slot(&second_slots[i]);
  }

  if (day_item.loaded) {
      layer_remove_from_parent(bitmap_layer_get_layer(day_item.image_layer));
      gbitmap_destroy(day_item.bitmap);
      bitmap_layer_destroy(day_item.image_layer);
  }
}
