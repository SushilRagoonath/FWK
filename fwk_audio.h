// -----------------------------------------------------------------------------
// audio framework
// - rlyeh, public domain
//
// fixme: leaks, audio_delete
// fixme: sfxr (hack)
// @todo: audio_volume_fx, audio_volume_bgm, audio_volume_master instead?
// @todo: destroystream()    if( ss->type == WAV ) drwav_uninit(&ss->wav);
// @todo: destroystream()    if( ss->type == MOD ) jar_mod_unload(&ss->mod);
// @todo: destroystream()    if( ss->type == XM && ss->xm ) jar_xm_free_context(ss->xm);

typedef struct audio_handle* audio_t;

API audio_t audio_clip( const char *pathfile );
API audio_t audio_stream( const char *pathfile );
API     int audio_play( audio_t s, int flags );

API float   audio_volume_clip(float gain);   // set     fx volume if gain is in [0..1] range. return current     fx volume in any case
API float   audio_volume_stream(float gain); // set    bgm volume if gain is in [0..1] range. return current    bgm volume in any case
API float   audio_volume_master(float gain); // set master volume if gain is in [0..1] range. return current master volume in any case

enum {
    AUDIO_1CH = 0, // default
    AUDIO_2CH = 1,

    AUDIO_8 = 2,
    AUDIO_16 = 0, // default
    AUDIO_32 = 4,
    AUDIO_FLOAT = 8,

    AUDIO_22KHZ = 0, // default
    AUDIO_44KHZ = 16,
};

API int audio_queue( const void *samples, int num_samples, int flags );
