#if defined(_WIN32) || defined(__MINGW32__)

#include "alsa2sdlBridge.hpp"
#include "symbolResolver.hpp"
#include "../audio/lindbergh/alsa2sdl/alsa2sdl.h"

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

namespace Alsa2SdlBridge
{
    void initBridges()
    {
        MAP("snd_pcm_open", shared_snd_pcm_open);
        MAP("snd_pcm_close", shared_snd_pcm_close);

        MAP("snd_pcm_hw_params_malloc", shared_snd_pcm_hw_params_malloc);
        MAP("snd_pcm_hw_params_free", shared_snd_pcm_hw_params_free);
        MAP("snd_pcm_hw_params_any", shared_snd_pcm_hw_params_any);
        MAP("snd_pcm_hw_params_set_access", shared_snd_pcm_hw_params_set_access);
        MAP("snd_pcm_hw_params_set_format", shared_snd_pcm_hw_params_set_format);
        MAP("snd_pcm_hw_params_set_channels", shared_snd_pcm_hw_params_set_channels);
        MAP("snd_pcm_hw_params_set_rate_near", shared_snd_pcm_hw_params_set_rate_near);
        MAP("snd_pcm_hw_params_set_periods", shared_snd_pcm_hw_params_set_periods);
        MAP("snd_pcm_hw_params_set_buffer_size", shared_snd_pcm_hw_params_set_buffer_size);
        MAP("snd_pcm_hw_params_set_buffer_size_near", shared_snd_pcm_hw_params_set_buffer_size_near);
        MAP("snd_pcm_hw_params_set_period_size_near", shared_snd_pcm_hw_params_set_period_size_near);
        MAP("snd_pcm_hw_params", shared_snd_pcm_hw_params);

        MAP("snd_pcm_sw_params_malloc", shared_snd_pcm_sw_params_malloc);
        MAP("snd_pcm_sw_params_free", shared_snd_pcm_sw_params_free);
        MAP("snd_pcm_sw_params_current", shared_snd_pcm_sw_params_current);
        MAP("snd_pcm_sw_params_set_avail_min", shared_snd_pcm_sw_params_set_avail_min);
        MAP("snd_pcm_sw_params_set_start_threshold", shared_snd_pcm_sw_params_set_start_threshold);
        MAP("snd_pcm_sw_params_set_xfer_align", shared_snd_pcm_sw_params_set_xfer_align);
        MAP("snd_pcm_sw_params", shared_snd_pcm_sw_params);

        MAP("snd_pcm_prepare", shared_snd_pcm_prepare);
        MAP("snd_pcm_start", shared_snd_pcm_start);
        MAP("snd_pcm_drop", shared_snd_pcm_drop);
        MAP("snd_pcm_resume", shared_snd_pcm_resume);
        MAP("snd_pcm_drain", shared_snd_pcm_drain);
        MAP("snd_pcm_delay", shared_snd_pcm_delay);
        MAP("snd_pcm_writei", shared_snd_pcm_writei);
        MAP("snd_pcm_avail_update", shared_snd_pcm_avail_update);
        MAP("snd_pcm_state", shared_snd_pcm_state);
        MAP("snd_pcm_format_width", shared_snd_pcm_format_width);
        MAP("snd_strerror", shared_snd_strerror);

        // Namco's sound driver (Dead Heat's nsAdrv): a mixer without elements, the card's info empty
        MAP("snd_pcm_hw_params_set_channels_max", shared_snd_pcm_hw_params_set_channels_max);
        MAP("snd_pcm_hw_params_set_buffer_time_near", shared_snd_pcm_hw_params_set_buffer_time_near);
        MAP("snd_pcm_hw_params_set_period_time_near", shared_snd_pcm_hw_params_set_period_time_near);
        MAP("snd_pcm_hw_params_get_buffer_size", shared_snd_pcm_hw_params_get_buffer_size);
        MAP("snd_pcm_hw_params_get_period_size", shared_snd_pcm_hw_params_get_period_size);
        MAP("snd_pcm_sw_params_set_xrun_mode", shared_snd_pcm_sw_params_set_xrun_mode);
        MAP("snd_pcm_wait", shared_snd_pcm_wait);
        MAP("snd_ctl_open", shared_snd_ctl_open);
        MAP("snd_ctl_close", shared_snd_ctl_close);
        MAP("snd_ctl_card_info_sizeof", shared_snd_ctl_card_info_sizeof);
        MAP("snd_ctl_card_info", shared_snd_ctl_card_info);
        MAP("snd_mixer_open", shared_snd_mixer_open);
        MAP("snd_mixer_selem_get_name", shared_snd_mixer_selem_get_name);
        MAP("snd_mixer_selem_get_playback_volume_range", shared_snd_mixer_selem_get_volume_range);
        MAP("snd_mixer_selem_get_capture_volume_range", shared_snd_mixer_selem_get_volume_range);
        MAP("snd_mixer_first_elem", shared_snd_mixer_none);
        MAP("snd_mixer_elem_next", shared_snd_mixer_none);
        for (const char *name : {"snd_mixer_attach", "snd_mixer_detach", "snd_mixer_load", "snd_mixer_close", "snd_mixer_free",
                                 "snd_mixer_selem_register", "snd_mixer_selem_is_active", "snd_mixer_selem_has_capture_switch",
                                 "snd_mixer_selem_has_capture_volume", "snd_mixer_selem_has_capture_volume_joined",
                                 "snd_mixer_selem_has_common_volume", "snd_mixer_selem_has_playback_switch",
                                 "snd_mixer_selem_has_playback_volume", "snd_mixer_selem_has_playback_volume_joined",
                                 "snd_mixer_selem_set_capture_switch_all", "snd_mixer_selem_set_capture_volume",
                                 "snd_mixer_selem_set_capture_volume_all", "snd_mixer_selem_set_playback_switch_all",
                                 "snd_mixer_selem_set_playback_volume", "snd_mixer_selem_set_playback_volume_all"})
            MAP(name, shared_snd_mixer_ok);
    }
} // namespace Alsa2SdlBridge

#endif