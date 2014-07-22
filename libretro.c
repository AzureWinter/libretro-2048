#include "libretro.h"
#include "game.h"

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

static uint16_t *frame_buf;

static struct retro_log_callback logging;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

float frame_time = 0;

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}

void retro_init(void)
{
   game_calculate_pitch();

   frame_buf = calloc(SCREEN_HEIGHT, SCREEN_PITCH);

   game_init(frame_buf);

   char *savedir;
   environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &savedir);

   if (savedir)
   {
      char filename[1024] = {0};
      sprintf(filename, "%s/2048.srm", savedir);

      FILE *fp = fopen(filename, "rb");

      if (fp)
      {
         fread(game_data(), game_data_size(), 1, fp);
         fclose(fp);
      }
      else
         logging.log(RETRO_LOG_WARN, "[2048] unable to load game data: %s.", strerror(errno));
   }
   else
      logging.log(RETRO_LOG_WARN, "[2048] unable to load game data: save directory not set.");
}

void retro_deinit(void)
{
   char *savedir;
   environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &savedir);

   if (savedir)
   {
      char filename[1024] = {0};
      sprintf(filename, "%s/2048.srm", savedir);

      FILE *fp = fopen(filename, "wb");

      if (fp)
      {
         fwrite(game_save_data(), game_data_size(), 1, fp);
         fclose(fp);
      }
      else
         logging.log(RETRO_LOG_WARN, "[2048] unable to save game data: %s.", strerror(errno));
   }
   else
      logging.log(RETRO_LOG_WARN, "[2048] unable to save game data: save directory not set.");


   game_deinit();

   free(frame_buf);
   frame_buf = NULL;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "2048";
   info->library_version  = "v1.0";
   info->need_fullpath    = false;
   info->valid_extensions = NULL; // Anything is fine, we don't care.
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->timing.fps = 24.0;
   info->timing.sample_rate = 30000.0;

   info->geometry.base_width   = SCREEN_WIDTH;
   info->geometry.base_height  = SCREEN_HEIGHT;
   info->geometry.max_width    = SCREEN_WIDTH;
   info->geometry.max_height   = SCREEN_HEIGHT;
   info->geometry.aspect_ratio = 1;
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   bool no_rom = true;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);

   if (!cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
      logging.log = fallback_log;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_reset(void)
{
   game_reset();
}

static void frame_time_cb(retro_usec_t usec)
{
   frame_time = usec / 1000000.0;
}

void retro_run(void)
{
   key_state_t ks;

   input_poll_cb();

   ks.up = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
   ks.right = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
   ks.down = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
   ks.left = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
   ks.start = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
   ks.select = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);

   game_update(frame_time, &ks);
   game_render();

   video_cb(frame_buf, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_PITCH);
}

bool retro_load_game(const struct retro_game_info *info)
{
   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      logging.log(RETRO_LOG_INFO, "RGB565 is not supported.\n");
      return false;
   }

   struct retro_frame_time_callback frame_cb = { frame_time_cb, 1000000 / 60 };
   environ_cb(RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK, &frame_cb);

   (void)info;
   return true;
}

void retro_unload_game(void)
{
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return game_data_size();
}

bool retro_serialize(void *data_, size_t size)
{
   if (size < game_data_size())
      return false;

   memcpy(data_, game_data(), game_data_size());
   return true;
}

bool retro_unserialize(const void *data_, size_t size)
{
   if (size < game_data_size())
      return false;

   memcpy(game_data(), data_, game_data_size());
   return true;
}

void *retro_get_memory_data(unsigned id)
{
   if (id != RETRO_MEMORY_SAVE_RAM)
      return NULL;

   return game_data();
}

size_t retro_get_memory_size(unsigned id)
{
   if (id != RETRO_MEMORY_SAVE_RAM)
      return 0;

   return game_data_size();
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

