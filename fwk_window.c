static void (*hooks[64])() = {0};
static void *userdatas[64] = {0};

bool window_hook(void (*func)(), void* user) {
    window_unhook( func );
    for( int i = 0; i < 64; ++i ) {
        if( !hooks[i] ) {
            hooks[i] = func;
            userdatas[i] = user;
            return true;
        }
    }
    return false;
}
void window_unhook(void (*func)()) {
    for( int i = 0; i < 64; ++i ) {
        if(hooks[i] == func) {
            hooks[i] = 0;
            userdatas[i] = 0;
        }
    }
}

static GLFWwindow *window;
static int w, h, xpos, ypos, paused;
static int fullscreen, xprev, yprev, wprev, hprev;
static double t, dt, fps;
static char title[128] = {0};
static char screenshot_file[512];
static char videorec_file[512];

void window_drop_callback(GLFWwindow* window, int count, const char** paths) {
    // @fixme: win: convert from utf8 to window16 before processing
    // @fixme: remove USERNAME for nonwin32
    // @fixme: wait until any active import (launch) is done

    char pathdir[512]; snprintf(pathdir, 512, "art/import/%llu_%s/", date_human(), ifdef(linux, getlogin(), getenv("USERNAME")));
    mkdir( pathdir, 0777 );

    int errors = 0;
    for( int i = 0; i < count; ++i ) {
        const char *src = paths[i];
        const char *dst = stringf("%s%s", pathdir, file_name(src));
        errors += file_copy(src, dst) ? 0 : 1;
    }

    if( errors ) PANIC("%d errors found during file dropping", errors);
    else  app_reload();
}

void window_create(float zoom, int flags) {
    fwk_init();
    t = glfwGetTime();

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    int area_width, area_height;
    glfwGetMonitorWorkarea(monitor, &xpos, &ypos, &area_width, &area_height);
    float width = area_width, height = area_height, ratio = width / (height + !!height);

    /* We need to explicitly ask for a 3.2 context on OS X */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // osx
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2); // osx, 2:#version150,3:330
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); //osx
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); //osx
    glfwWindowHint(GLFW_STENCIL_BITS, 8); //osx
glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    int keep_aspect_ratio = 1;
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); // makes it non-resizable
    if(flags & WINDOW_MSAA2) glfwWindowHint(GLFW_SAMPLES, 2); // x2 AA
    if(flags & WINDOW_MSAA4) glfwWindowHint(GLFW_SAMPLES, 4); // x4 AA
    if(flags & WINDOW_MSAA8) glfwWindowHint(GLFW_SAMPLES, 8); // x8 AA

    zoom = (zoom < 1 ? zoom * 100 : zoom > 100 ? 100 : zoom);
    if( zoom >= 100 ) {
        // full screen
        window = glfwCreateWindow(width = mode->width, height = mode->height, "", monitor, NULL);
    }
    else if( zoom >= 99 ) {
        // full screen windowed
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        window = glfwCreateWindow(width = mode->width, height = mode->height, "", monitor, NULL);
    } else {
        // windowed
        if( flags & WINDOW_SQUARE )   width = height = width > height ? height : width;
        if( flags & WINDOW_LANDSCAPE ) if( width < height ) height = width * ratio;
        if( flags & WINDOW_PORTRAIT )  if( width > height ) width = height * (1.f / ratio);

        window = glfwCreateWindow(width * zoom / 100, height * zoom / 100, "", NULL, NULL);
        glfwSetWindowPos(window, xpos = xpos + (area_width - width * zoom / 100) / 2, ypos = ypos + (area_height - height * zoom / 100) / 2);
    }
    wprev = w, hprev = h;
    xprev = xpos, yprev = ypos;

    glfwMakeContextCurrent(window);

    gladLoadGL(glfwGetProcAddress);
glDebugEnable();

    PRINTF("Monitor: %s\n", glfwGetMonitorName(monitor));
    PRINTF("GPU device: %s\n", glGetString(GL_RENDERER));
    PRINTF("GPU driver: %s\n", glGetString(GL_VERSION));

    glfwSwapInterval(1);

    /*if(keep_aspect_ratio)*/ glfwSetWindowAspectRatio(window, /*mode->*/width, /*mode->*/height);
    window_cursor(flags & WINDOW_NO_MOUSE ? false : true);

    glfwSetDropCallback(window, window_drop_callback);

    glfwPollEvents();

    fwk_pre_init_subsystems();

    // display a progress bar meanwhile cooker is working in the background
    // Sleep(500);
    if( WITH_COOKER && file_size("art/tools/ass2iqe") )
    while( cooker_progress() < 100 ) {
        for( int frames = 0; frames < 10 && window_swap(); frames += cooker_progress() >= 100 ) {
            window_title("Cooking assets %.2d%%", cooker_progress());

            glfwGetFramebufferSize(window, &w, &h);
            glNewFrame();

            static float previous = 0;
            float progress = (cooker_progress()+1) / 100.f; if(progress > 1) progress = 1;
            float speed = progress < 1 ? 0.2f : 0.5f;
            float smooth = previous = progress * speed + previous * (1-speed);

            // NDC coordinates (2d): bottom-left(-1,-1), center(0,0), top-right(+1,+1)
            float pixel = 2.f / window_height(), dist = smooth*2-1;
            ddraw_line(vec3(-1,-pixel*2,0), vec3(1,   -pixel*2,0));   // full line
            ddraw_line(vec3(-1,-pixel  ,0), vec3(dist,-pixel  ,0));   // progress line
            ddraw_line(vec3(-1, 0      ,0), vec3(dist, 0      ,0));   // progress line
            ddraw_line(vec3(-1,+pixel  ,0), vec3(dist,+pixel  ,0));   // progress line
            ddraw_line(vec3(-1,+pixel*2,0), vec3(1,   +pixel*2,0));   // full line

            ddraw_flush();

            do_once window_visible(1);
        }
        // set black screen
        glfwGetFramebufferSize(window, &w, &h);
        glNewFrame();
        window_swap();
        window_title("");
    }

    glfwShowWindow(window);

    fwk_post_init_subsystems();

    //    t = glfwGetTime();
}

static double boot_time = 0;

static
char* window_stats() {
    static double num_frames = 0, begin = FLT_MAX, prev_frame = 0;

    double now = time_ss();
    if( boot_time < 0 ) boot_time = now;

    if( begin > now ) {
        begin = now;
        num_frames = 0;
    }
    if( (now - begin) >= 0.25f ) {
        fps = num_frames * (1.f / (now - begin));
    }
    if( (now - begin) > 1 ) {
        begin = now + ((now - begin) - 1);
        num_frames = 0;
    }

    // @todo: print %used/%avail kib mem, %used/%avail objs as well
    static char buf[192];
    snprintf(buf, 192, "%s | boot %.2fs | %5.2ffps (%.2fms)", title, !boot_time ? now : boot_time, fps, (now - prev_frame) * 1000.f);

    prev_frame = now;
    ++num_frames;

    return buf + 3 * (buf[0] == ' ');
}

int window_needs_flush = 1;
void window_flush() {
    // flush systems that are batched and need to be rendered before frame swapping. order matters.
    if( window_needs_flush ) {
        window_needs_flush = 0;

        fwk_pre_swap_subsystems();
    }
}
int window_swap() {
    int ready = !glfwWindowShouldClose(window);
    if( ready ) {
        static int first = 1;

        window_flush();

        glfwPollEvents();

        // input_update(); // already hooked!

        double now = paused ? t : glfwGetTime();
        dt = now - t;
        t = now;

#if 0
        static unsigned frames = 0;
        static double t = 0;
        t += window_delta();
        if( t >= 1.0 ) {
            fps = frames / t;
            glfwSetWindowTitle(window, stringf("%s | %5.2f fps (%.2fms)", title, fps, (t*1000.0) / fps));
            frames = 0;
            t = 0;
        }
        ++frames;
#else
        char *st = window_stats();
        static double t = 0;
        t += window_delta();
        if( t >= 0.25 ) {
            glfwSetWindowTitle(window, st);
            t = 0;
        }
#endif

        // save screenshot if queued
        if( screenshot_file[0] ) {
            int n = 3;
            void *rgb = screenshot(n);
            stbi_flip_vertically_on_write(true);
            if(!stbi_write_png(screenshot_file, w, h, n, rgb, n * w) ) {
                PANIC("!could not write screenshot file `%s`\n", screenshot_file);
            }
            screenshot_file[0] = 0;
        }
        if( videorec_file[0] ) {
            for( FILE *fp = fopen(videorec_file, "a+b"); fp; fclose(fp), fp = 0) {
                void* rgb = screenshot(-3); // 3 RGB, -3 BGR
                jo_write_mpeg(fp, rgb, window_width(), window_height(), 24);  // 24fps
            }
            // videorec_file[0] = 0;
        }

        if( !first ) {
            glfwSwapBuffers(window);
        }

        glfwGetFramebufferSize(window, &w, &h); //glfwGetWindowSize(window, &w, &h);
        glNewFrame();
        window_needs_flush = 1;

        // @todo: deprecate me, this is only useful for apps that plan to use ddraw without any camera setup
        // ddraw_flush();

        // run user-defined hooks
        for(int i = 0; i < 64; ++i) {
            if( hooks[i] ) hooks[i]( userdatas[i] );
        }

        first = 0;
    }
    return ready;
}
int window_width() {
    return w;
}
int window_height() {
    return h;
}
double window_aspect() {
    return (double)w / (h + !!h);
}
double window_time() {
    return t;
}
double window_delta() {
    return dt;
}
double window_fps() {
    return fps;
}
void (window_title)(const char *title_) {
    snprintf(title, 128, "%s", title_);
    if( !title[0] ) glfwSetWindowTitle(window, title);
}
void window_icon(const char *file_icon) {
    void *data = vfs_read(file_icon); data = data ? data : file_read(file_icon);
    if( data ) {
        unsigned len = vfs_size(file_icon); len = len ? len : file_size(file_icon);
        if( len ) {
            image_t img = image_from_mem(data, len, IMAGE_RGBA);
            if( img.w && img.h && img.pixels ) {
                GLFWimage images[1];
                images[0].width = img.w;
                images[0].height = img.h;
                images[0].pixels = img.pixels;
                glfwSetWindowIcon(window, 1, images);
                return;
            }
        }
    }
#ifdef _WIN32
    HANDLE hIcon = LoadImageA(0, file_icon, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
    if( hIcon ) {
        HWND hWnd = glfwGetWin32Window(window);
        SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(hWnd, WM_SETICON, ICON_BIG,   (LPARAM)hIcon);
        SendMessage(GetWindow(hWnd, GW_OWNER), WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(GetWindow(hWnd, GW_OWNER), WM_SETICON, ICON_BIG,   (LPARAM)hIcon);
        return;
    }
#endif
}
void* window_handle() {
    return window;
}
static
GLFWmonitor *window_find_monitor(int wx, int wy) {
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();

    // find best monitor given current window coordinates. @todo: select by ocuppied window area inside each monitor instead.
    int num_monitors = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&num_monitors);
    for( int i = 0; i < num_monitors; ++i) {
        int mx = 0, my = 0, mw = 0, mh = 0;
        glfwGetMonitorWorkarea(monitors[i], &mx, &my, &mw, &mh);
        monitor = wx >= mx && wx <= (mx+mw) && wy >= my && wy <= (my+mh) ? monitors[i] : monitor;
    }

    return monitor;
}
void window_fullscreen(int enabled) {
    fullscreen = !!enabled;
    if( fullscreen ) {
        int wx = 0, wy = 0; glfwGetWindowPos(window, &wx, &wy);
        GLFWmonitor *monitor = window_find_monitor(wx, wy);

        wprev = w, hprev = h, xprev = wx, yprev = wy; // save window context for further restoring

        int width, height;
        glfwGetMonitorWorkarea(monitor, NULL, NULL, &width, &height);
        glfwSetWindowMonitor(window, monitor, 0, 0, width, height, GLFW_DONT_CARE);
    } else {
        glfwSetWindowMonitor(window, NULL, xpos, ypos, wprev, hprev, GLFW_DONT_CARE);
        glfwSetWindowPos(window, xprev, yprev);
    }
}
int window_has_fullscreen() {
    return fullscreen;
}
void window_pause(int enabled) {
    paused = enabled;
}
int window_has_pause() {
    return paused;
}
void window_focus() {
    glfwFocusWindow(window);
}
int window_has_focus() {
    return !!glfwGetWindowAttrib(window, GLFW_FOCUSED);
}
void window_cursor(int visible) {
    glfwSetInputMode(window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}
int window_has_cursor() {
    return glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL;
}
void window_visible(int visible) {
    if(!window) return;
    //if(window) (visible ? glfwRestoreWindow : glfwIconifyWindow)(window);
    (visible ? glfwShowWindow : glfwHideWindow)(window);
    // call glfwpollevents in linux to flush visiblity changes that would happen in next frame otherwise
    #if is(linux) || is(osx)
    glfwPollEvents();
    #endif
}
int window_has_visible() {
    return glfwGetWindowAttrib(window, GLFW_VISIBLE);
}

void window_screenshot(const char* filename_png) {
    snprintf(screenshot_file, 512, "%s", filename_png ? filename_png : "");
}

void window_videorec(const char* filename_mpg) {
    snprintf(videorec_file, 512, "%s", filename_mpg ? filename_mpg : "");
}
int window_has_videorec() {
    return !!videorec_file[0];
}
