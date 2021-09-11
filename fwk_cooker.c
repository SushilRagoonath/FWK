int COOKER_MAX_THREADS = ifdef(tcc, 1, 0); // [0 (auto) .. 16 max] tcc does not support threading

typedef struct fs {
    char *fname, status;
    uint64_t stamp;
    uint64_t bytes;
} fs;

struct cooker_args {
    const char **files;
    int numfiles;
    cooker_callback_t callback;
    char zipfile[16];
    int threadid;
};

static
int cooker__find_thread_number( const char *filename ) {
//    return hash_str(file_path(filename)) % COOKER_MAX_THREADS;
    return hash_str(filename) % COOKER_MAX_THREADS;
}

static
array(fs) cooker__fs_scan(struct cooker_args *args) {
    array(struct fs) fs = 0;

    // iterate all previously scanned files
    for( int i = 0; i < args->numfiles; ++i ) {
        const char *fname = args->files[i];
        if( file_directory(fname) ) continue; // skip folders
        if( fname[0] == '.' ) continue; // skip internal files, like .cook.zip

        int thread_id_target = cooker__find_thread_number(fname);
        if( thread_id_target != args->threadid ) continue; // skip files not meant for this thread

        // @todo: normalize path & rebase here (absolute to local)
        // [...]
        // fi.normalized = ; tolower->to_underscore([]();:+ )->remove_extra_underscores

        // make buffer writable
        char buffer[MAX_PATHFILE];
        snprintf(buffer, MAX_PATHFILE, "%s", fname);

        // get normalized current working directory (absolute)
        char cwd[MAX_PATHFILE] = {0};
        getcwd(cwd, sizeof(cwd));
        for(int i = 0; cwd[i]; ++i) if(cwd[i] == '\\') cwd[i] = '/';

        // normalize path
        for(int i = 0; buffer[i]; ++i) if(buffer[i] == '\\') buffer[i] = '/';

        // rebase from absolute to relative
        char *buf = buffer; int cwdlen = strlen(cwd);
        if( !strncmp(buf, cwd, cwdlen) ) buf += cwdlen;
        while(buf[0] == '/') ++buf;

        if( file_name(buf)[0] == '.' ) continue; // skip system files

        struct fs fi = {0};
        fi.fname = STRDUP(buf);
        fi.bytes = file_size(buf);
        fi.stamp = file_stamp_human(buf); // human-readable base10 timestamp

        array_push(fs, fi);
    }
    return fs;
}

static
fs *cooker__fs_locate(array(struct fs) fs, const char *file) {
    for(int i = 0; i < array_count(fs); ++i) {
        if( !strcmp(fs[i].fname,file)) {
            return &fs[i];
        }
    }
    return 0;
}

static local array(char*) added;
static local array(char*) changed;
static local array(char*) deleted;
static local array(char*) uncooked;

static
int cooker__fs_diff( zip* old, array(fs) now ) {
    array_free(added);
    array_free(changed);
    array_free(deleted);
    array_free(uncooked);

    // if not zipfile is present, all files are new and must be added
    if( !old ) {
        for( int i = 0; i < array_count(now); ++i ) {
            array_push(uncooked, STRDUP(now[i].fname));
        }
        return 1;
    }

    // compare for new & changed files
    for( int i = 0; i < array_count(now); ++i ) {
        int found = zip_find(old, now[i].fname);
        if( found < 0 ) {
            array_push(added, STRDUP(now[i].fname));
            array_push(uncooked, STRDUP(now[i].fname));
        } else {
            uint64_t oldsize = atoi64(zip_comment(old,found)); // zip_size(old, found); returns sizeof processed asset. return original size of unprocessed asset, which we store in comment section
            uint64_t oldstamp = atoi64(zip_modt(old, found)+20);
            if( oldsize != now[i].bytes || abs(oldstamp - now[i].stamp) > 1 ) { // @fixme: should use hash instead. hashof(tool) ^ hashof(args used) ^ hashof(rawsize) ^ hashof(rawdate)
                printf("%s:\t%llu vs %llu, %llu vs %llu\n", now[i].fname, (uint64_t)oldsize,(uint64_t)now[i].bytes, (uint64_t)oldstamp,(uint64_t)now[i].stamp);
                array_push(changed, STRDUP(now[i].fname));
                array_push(uncooked, STRDUP(now[i].fname));
            }
        }
    }
    // compare for deleted files
    for( int i = 0; i < zip_count(old); ++i ) {
        char *oldname = zip_name(old, i);
        int idx = zip_find(old, oldname); // find latest versioned file in zip
        unsigned oldsize = zip_size(old, idx);
        if (!oldsize) continue;
        fs *found = cooker__fs_locate(now, oldname);
        if( !found ) {
            array_push(deleted, STRDUP(oldname));
        }
    }
    return 1;
}

static volatile int cooker__progress[16] = {0};

int cooker_progress() {
    int count = 0, sum = 0;
    for( int i = 0; i < countof(cooker__progress); ++i ) {
        if( cooker__progress[i] >= 0 ) {
            sum += cooker__progress[i];
            ++count;
        }
    }
    return sum / (count+!count);
}

static
int cooker_sync( void *userptr ) {
    struct cooker_args *args = userptr;
    ASSERT( args && args->callback );

    volatile int *progress = &cooker__progress[args->threadid];
    *progress = 0;

    array(struct fs) now = cooker__fs_scan(args);
    //printf("Scanned: %d items found\n", array_count(now));

    if( file_size(args->zipfile) == 0 ) unlink(args->zipfile);

    // populate added/deleted/changed arrays by examining current disk vs last cache
    zip *z = zip_open(args->zipfile, "r+b");
    cooker__fs_diff(z, now);
    if( z ) zip_close(z);

    fflush(0);

    z = zip_open(args->zipfile, "a+b");
    if( !z ) {
        unlink(args->zipfile);
        z = zip_open(args->zipfile, "a+b"); // try again
        if(!z) PANIC("cannot open file for updating: %s", args->zipfile);
    }

char COOKER_TMPFILE[MAX_PATHFILE+1]; snprintf(COOKER_TMPFILE, MAX_PATHFILE, "%s", tmpnam(0)); // ".temp%d", args->threadid);

    FILE *out = fopen(COOKER_TMPFILE, "a+b");
    if( !out ) PANIC("cannot open %s file for writing", COOKER_TMPFILE);
    fseek(out, 0L, SEEK_SET);

unlink(COOKER_TMPFILE); // @fixme:

    // deleted files
    for( int i = 0, end = array_count(deleted); i < end; ++i ) {
        printf("Deleting %03d%% %s\n", (i+1) == end ? 100 : (i * 100) / end, deleted[i]);
        FILE* out = fopen(COOKER_TMPFILE, "wb"); fclose(out);
        FILE* in = fopen(COOKER_TMPFILE, "rb");
        char *comment = "0";
        zip_append_file(z, deleted[i], comment, in, 0);
        fclose(in);
    }
    // added or changed files
    for( int i = 0, end = array_count(uncooked); i < end; ++i ) {
        *progress = ((i+1) == end ? 90 : (i * 90) / end); // (i+i>0) * 100.f / end;

        char *fname = uncooked[i];

        FILE *in = fopen(fname, "rb");
        if( !in ) PANIC("cannot open file for reading: %s", fname);
        fseek(in, 0L, SEEK_END);
        size_t inlen = ftell(in);
        fseek(in, 0L, SEEK_SET);

        fseek(out, 0L, SEEK_SET);

        char *ext = strrchr(fname, '.'); ext = ext ? ext : ""; // .jpg
        char header[16]; fread(header, 1, 16, in); fseek(in, 0L, SEEK_SET);

        const char *info = stringf("Cooking %03d%% %s\n", *progress, uncooked[i]);
        int compression = (errno = 0, args->callback(fname, ext, header, in, out, info, args->threadid));
        int failed = errno != 0;
        if( failed ) PRINTF("importing failed: %s", fname);
        else if( compression >= 0 ) {
            fseek(out, 0L, SEEK_SET);
            char *comment = stringf("%d",(int)inlen);
            if( !zip_append_file(z, fname, comment, out, compression) ) {
                PANIC("failed to add processed file into %s: %s", args->zipfile, fname);
            }
        }

        fclose(in);
#if 0
        fclose(out);
#else
        size_t resize = 0;
        {
            int fd = fileno(out);
            if( fd != -1 ) {
                int ok = 0 == ftruncate( fd, (off_t)resize );
                fflush(out);
                fseek(out, 0L, SEEK_END );
            }
        }
#endif
    }
    zip_close(z);

    fclose(out);
//  fflush(0);

    *progress = 100;
    return 1;
}

static
int cooker_async( void *userptr ) {
    while(!window_handle()) sleep_ms(100); // wait for window handle to be created

    int ret = cooker_sync(userptr);
    thread_exit( ret );
    return ret;
}

bool cooker( const char *masks, cooker_callback_t callback, int flags ) {
    static struct cooker_args args[16] = {0};
    const char **files = file_list(masks);

    int numfiles = 0; while(files[numfiles]) ++numfiles;
    args[0].files = files;
    args[0].callback = callback;
    args[0].numfiles = numfiles;
    snprintf(args[0].zipfile, 16, ".cook[%d].zip", args[0].threadid = 0);

    for( int i = 0; i < countof(cooker__progress); ++i ) cooker__progress[i] = -1;

    if( COOKER_MAX_THREADS == 1 || !(flags & COOKER_ASYNC) ) {
        return !!cooker_sync( &args[0] );
    } else {
        if( !COOKER_MAX_THREADS ) COOKER_MAX_THREADS = cpu_cores();
        // @todo: sort files by threaid: (hash(path_excluding_basefile)%numthreads)
        for( int i = 0; i < COOKER_MAX_THREADS; ++i ) {
            args[i] = args[0];
            snprintf(args[i].zipfile, 16, ".cook[%d].zip", args[i].threadid = i);
            thread_ptr_t thd = thread_init( cooker_async, &args[i], "cooker_async()", 0/*STACK_SIZE*/ );
        }
        return true;
    }
}

// -----------------------------------------------------------------------------
// data pipeline

#define cookme(...) cookme(threadid, stringf(__VA_ARGS__))

static int (cookme)(int threadid, const char *cmd) {

//  dear linux/osx/bsd users:
//  tools going wrong for any reason? cant compile them maybe?
//  small hack to autodetect wine & use win32 pipeline tools instead
#if !is(win32)
    if( 0 == os_exec("wine --version 2>/dev/null") ) {
        char buf[MAX_PATHFILE] = "wine ", *ptr = buf + 5;
        while(*cmd && *cmd != 32) *ptr++ = *cmd++;
        if(*cmd == 32) ptr += sprintf(ptr, ".exe");
        while(*cmd) *ptr++ = *cmd++;
        cmd = stringf("%s", buf);
    } else {
        cmd = stringf("LD_LIBRARY_PATH=art/tools %s", cmd);
    }
#endif

    int rc = os_exec(cmd);

    if(0) { // <-- uncomment to create batch file for (debug) forensic purposes
        char cook_bat[16]; snprintf(cook_bat, 16, ".cook[%d].bat", threadid);
        for( FILE* fp = fopen(cook_bat, "a+b"); fp; fclose(fp), fp = 0) {
            fputs(cmd, fp);
            fputs("\r\n", fp);
            fprintf(fp, "REM Returned: %d [%#x]\r\n", rc, rc);
            fputs("\r\n", fp);
        }
    }

    return rc;
}

unsigned determinate_color_from_text(const char *text);

static
int fwk_cook(char *filename, const char *ext, const char header[16], FILE *in, FILE *out, const char *info, int threadid) {
    // reserve i/o buffer (2 MiB)
    enum { BUFSIZE = 2 * 1024 * 1024 };
    static local char *buffer = 0; if(!buffer) buffer = REALLOC(0, BUFSIZE);

    // exclude extension-less files
    if( !ext[0] ) goto bypass;
    // exclude vc c/c++ .obj files
    if( !strcmp(ext, ".obj") && !memcmp(header, "\x64\x86", 2)) goto bypass;

    // exclude anything which is not supported
    ext = stringf("%s.", ext); // ".c" -> ".c."
    int is_supported = !!strstr(
        ".image.jpg.jpeg.png.tga.bmp.psd.hdr.pic.pnm.ico"
        ".model.iqm.gltf.gltf2.fbx.obj.dae.blend.md3.md5.ms3d.smd.x.3ds.bvh.dxf.lwo"
        ".audio.wav.mod.xm.flac.ogg.mp1.mp3.mid"
        ".font.ttf"
        ".text.json.xml.csv.ini.cfg.doc.txt.md"
        ".shader.glsl.vs.fs"
        ".script.lua.tl"
        ".video.mp4.ogv.avi.mkv.wmv.mpg.mpeg" ".", ext);
    if( !is_supported ) goto bypass;

    int must_process_model = !!strstr(".model.gltf.gltf2.fbx.obj.dae.blend.md3.md5.ms3d.smd.x.3ds.bvh.dxf.lwo" ".", ext); // note: no .iqm here
    int must_process_audio = !!strstr(".audio.mid" ".", ext);
    int must_process_video = !!strstr(".video.mp4.ogv.avi.mkv.wmv.mpg.mpeg" ".", ext);
    int must_process_text  = !!strstr(".text.xml" ".", ext);
    int must_process = must_process_model || must_process_audio || must_process_video || must_process_text;

    if( !must_process ) {
        // read -> write
        for( int n; !!(n = fread( buffer, 1, BUFSIZE, in )); ){
            bool ok = fwrite( buffer, 1, n, out ) == n;
            if( !ok ) goto failed;
        }
    } else {
        // read -> process -> write
        puts(info);
        float dt = -time_ss();

        tty_color(GREEN);

        if( must_process_text ) {
            const char *infile, *outfile;
            char tempfile[16]; snprintf(tempfile, 16, ".temp%d.xml", threadid);

            const char *symbols[] = {
                "{{EXE}}",     ifdef(osx, ".osx", ifdef(win32,".exe","")),
                "{{INPUT}}",    infile = filename,
                "{{OUTPUT}}",   outfile = tempfile,
                "{{OPTIONS}}",  "" };
            int rc = cookme(strlerp(4, symbols, "art/tools/xml2json{{EXE}}  '{{INPUT}}' > {{OUTPUT}}"));
            tty_color(rc || !file_size(outfile) ? RED : determinate_color_from_text(os_exec_output()));
            printf("%s\nReturned: %d (%#x)\n", os_exec_output(), rc, rc);

            bool ok = !!file_size(outfile);
            if( ok ) for( FILE *in_ = fopen(outfile, "rb"); in_; fclose(in_), in_ = 0) {
                for( int n; !!(n = fread( buffer, 1, BUFSIZE, in_ )); ){
                    ok = fwrite( buffer, 1, n, out ) == n;
                    if( !ok ) break;
                }
            }

            unlink(tempfile);
            if( !ok ) goto failed;
        }
        if( must_process_video ) {
            const char *infile, *outfile;
            char temp_mpg[16]; snprintf(temp_mpg, 16, ".temp%d.mpg", threadid);

            const char *symbols[] = {
                "{{EXE}}",     ifdef(osx, ".osx", ifdef(win32,".exe","")),
                "{{INPUT}}",    infile = filename,
                "{{OUTPUT}}",   outfile = temp_mpg,
                "{{OPTIONS}}",  "-qscale:v 4 -y -c:v mpeg1video -c:a mp2 -ac 1 -b:a 128k -ar 44100 -format mpeg" };
            int rc = cookme(strlerp(4, symbols, "art/tools/ffmpeg{{EXE}} -y -i '{{INPUT}}' {{OPTIONS}} {{OUTPUT}}"));
            tty_color(rc || !file_size(outfile) ? RED : determinate_color_from_text(os_exec_output()));
            printf("%s\nReturned: %d (%#x)\n", os_exec_output(), rc, rc);

            bool ok = !!file_size(outfile);
            if( ok ) for( FILE *in_ = fopen(outfile, "rb"); in_; fclose(in_), in_ = 0) {
                for( int n; !!(n = fread( buffer, 1, BUFSIZE, in_ )); ){
                    ok = fwrite( buffer, 1, n, out ) == n;
                    if( !ok ) break;
                }
            }

            unlink(temp_mpg);
            if( !ok ) goto failed;
        }
        if( must_process_audio ) {
            const char *infile, *outfile;
            char temp_wav1[16]; snprintf(temp_wav1, 16, ".temp%d.1.wav", threadid);
            char temp_wav2[16]; snprintf(temp_wav2, 16, ".temp%d.2.wav", threadid);

            {
                const char *symbols[] = {
                    "{{EXE}}",          ifdef(osx, ".osx", ifdef(win32,".exe","")),
                    "{{INPUT}}",         infile = filename,
                    "{{OUTPUT}}",        outfile = temp_wav1,
                    "{{SOUNDBANK_SF2}}", "art/tools/AweROMGM.sf2" };
                int rc = cookme(strlerp(4, symbols, "art/tools/mid2wav{{EXE}}  '{{INPUT}}' {{OUTPUT}} {{SOUNDBANK_SF2}}"));
                tty_color(rc || !file_size(outfile) ? RED : determinate_color_from_text(os_exec_output()));
                printf("%s\nReturned: %d (%#x)\n", os_exec_output(), rc, rc);
            }

            {
                const char *symbols[] = {
                    "{{EXE}}",     ifdef(osx, ".osx", ifdef(win32,".exe","")),
                    "{{INPUT}}",    infile = outfile,
                    "{{OUTPUT}}",   outfile = temp_wav2 };
                int rc = cookme(strlerp(3, symbols, "art/tools/ffmpeg{{EXE}} -y -i '{{INPUT}}' -f wav -acodec adpcm_ms {{OUTPUT}}"));
                tty_color(rc || !file_size(outfile) ? RED : determinate_color_from_text(os_exec_output()));
                printf("%s\nReturned: %d (%#x)\n", os_exec_output(), rc, rc);
            }

            bool ok = !!file_size(outfile);
            if( ok ) for( FILE *in_ = fopen(outfile, "rb"); in_; fclose(in_), in_ = 0) {
                for( int n; !!(n = fread( buffer, 1, BUFSIZE, in_ )); ){
                    ok = fwrite( buffer, 1, n, out ) == n;
                    if( !ok ) break;
                }
            }

            unlink(temp_wav1); unlink(temp_wav2);
            if( !ok ) goto failed;
        }
        if( must_process_model ) {
            const char *infile, *outfile;
            char temp_iqe[16]; snprintf(temp_iqe, 16, ".temp%d.iqe", threadid);
            char temp_iqm[16]; snprintf(temp_iqm, 16, ".temp%d.iqm", threadid);

            {
                const char *symbols[] = {
                    "{{EXE}}",     ifdef(osx, ".osx", ifdef(win32,".exe","")),
                    "{{INPUT}}",    infile = filename,
                    "{{OUTPUT}}",   outfile = temp_iqe,
                    "{{OPTIONS}}",  !strcmp(ext,".dae" ".") ? "-U" : "" }; // flip UVs for .daes
                int rc = cookme(strlerp(4, symbols, "art/tools/ass2iqe{{EXE}}  {{OPTIONS}} -o {{OUTPUT}} '{{INPUT}}'"));
                tty_color(rc || !file_size(outfile) ? RED : determinate_color_from_text(os_exec_output()));
                printf("%s\nReturned: %d (%#x)\n", os_exec_output(), rc, rc);
            }

            {
                const char *symbols[] = {
                    "{{EXE}}",     ifdef(osx, ".osx", ifdef(win32,".exe","")),
                    "{{INPUT}}",    infile = temp_iqe,
                    "{{OUTPUT}}",   outfile = temp_iqm };
                int rc = cookme(strlerp(3, symbols, "art/tools/iqe2iqm{{EXE}}  {{OUTPUT}} '{{INPUT}}'"));
                tty_color(rc || !file_size(outfile) ? RED : determinate_color_from_text(os_exec_output()));
                printf("%s\nReturned: %d (%#x)\n", os_exec_output(), rc, rc);
            }

            bool ok = !!file_size(outfile);
            if( ok ) for( FILE *in_ = fopen(outfile, "rb"); in_; fclose(in_), in_ = 0) {
                for( int n; !!(n = fread( buffer, 1, BUFSIZE, in_ )); ){
                    ok = fwrite( buffer, 1, n, out ) == n;
                    if( !ok ) break;
                }
            }

            unlink(temp_iqe), unlink(temp_iqm);
            if( !ok ) goto failed;
        }

        dt += time_ss(); printf("%.2fs\n\n", dt);
        tty_color(0);
    }

    // return compression level
    // exclude non-compressible files (jpg,mp3,...) -> lvl 0
    // exclude also files that compress a little bit, but we better leave them raw inside zip for streaming purposes (like wavs) -> lvl 0
    // exclude also infiles whose outfiles are one of the above (mid->wav)
    int level = WITH_COMPRESSOR;
    return errno = 0, strstr(".jpg.jpeg.png.flac.ogg.mp1.mp3.mpg.mpeg.wav.mid" ".", ext) ? 0 : level;

    bypass: return errno = 0, -1;
    failed: return errno = -1;
}
