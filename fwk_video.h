// -----------------------------------------------------------------------------
// video decoder (mpeg)
// - rlyeh, public domain
//
// [ref] https://github.com/phoboslab/pl_mpeg/blob/master/pl_mpeg_player.c
// [use] ffmpeg -i infile.mp4 -c:v mpeg1video -c:a mp2 -format mpeg outfile.mpg

typedef struct video_t video_t;

API video_t*   video( const char *filename, int flags );
API texture_t*  video_decode( video_t *v ); // decodes next frame, returns associated texture(s)
API int         video_has_finished(video_t *v);
API double      video_duration(video_t *v);
API int         video_seek(video_t *v, double seek_to);
API double      video_position(video_t *v);
API void       video_destroy( video_t *v );
