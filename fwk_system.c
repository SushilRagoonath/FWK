#if (is(tcc) && is(linux)) || (is(gcc) && !is(mingw)) // || is(clang)
int __argc; char **__argv;
#if !is(ems)
__attribute__((constructor)) void init_argcv(int argc, char **argv) { __argc = argc; __argv = argv; }
#endif
#endif

const char *app_path() { // should return absolute path always
    static char buffer[1024] = {0};
    if( buffer[0] ) return buffer;
#if is(win32)
    unsigned length = GetModuleFileNameA(NULL, buffer, sizeof(buffer)); // @todo: use GetModuleFileNameW+wchar_t && convert to utf8 instead
    char *a = strrchr(buffer, '/');  if(!a) a = buffer + strlen(buffer);
    char *b = strrchr(buffer, '\\'); if(!b) b = buffer + strlen(buffer);
    char slash = (a < b ? *a : b < a ? *b : '/');
    snprintf(buffer, 1024, "%.*s%c", length - (int)(a < b ? b - a : a - b), buffer, slash), buffer;
    if( strendi(buffer, "art\\tools\\tcc-win\\") ) { // fix tcc -g -run case
        strcat(buffer, "..\\..\\..\\");
    }
#else // #elif is(linux)
    char path[21] = {0};
    sprintf(path, "/proc/%d/exe", getpid());
    readlink(path, buffer, sizeof(buffer));
#endif
    return buffer;
}

const char *app_temp() {
    static char buffer[256] = {0};
    if( !buffer[0] ) {
        snprintf(buffer, 256, "%s", ifdef(win32, getenv("TEMP"), P_tmpdir));
        for( int i = 0; buffer[i]; ++i ) if( buffer[i] == '\\' ) buffer[i] = '/';
        if(buffer[strlen(buffer)-1] != '/') strcat(buffer, "/");
    }
    return buffer;
}

#ifndef APP_NAME
#define APP_NAME __argv[0]
#endif

const char *app_name() {
    static char buffer[256] = {0};
    if( !buffer[0] ) {
        char s[256];
        strncpy(s, APP_NAME, 256);
        char *a = strrchr(s, '/');
        char *b = strrchr(s, '\\');
        strncpy(buffer, a > b ? a : b > a ? b : s, 256);
    }
    return buffer;
}

const char *app_cache() {
    static char buffer[256] = {0};
    if( !buffer[0] ) {

        #if is(osx)
        snprintf(buffer, 256, "~/Library/Caches/%s/", app_name()); // getenv("user.home")
        #elif is(win32) // APPDATA for roaming?
        snprintf(buffer, 256, "%s\\%s\\", getenv("LOCALAPPDATA"), app_name()); // getenv("LOCALAPPDATA")
        #else // linux
        snprintf(buffer, 256, "~/.cache/%s/", app_name()); // getenv("user.home")
        #endif

        mkdir(buffer, 0777);

        for( int i = 0; buffer[i]; ++i ) if( buffer[i] == '\\' ) buffer[i] = '/';
    }

    return buffer;
}

void app_reload() {
    // save_on_exit();
    fflush(0);
    chdir(app_path());
    execv(__argv[0], __argv);
    exit(0);
}

char* os_exec_output() {
    static local char os_exec__output[4096] = {0};
    return os_exec__output;
}
int (os_exec)( const char *cmd ) {
    int rc = -1;
    char *buf = os_exec_output(); buf[0] = 0; // memset(buf, 0, 4096);
    if(!strstr(cmd, ifdef(win32, ">NUL", ">/dev/null"))) puts(cmd); // log unless silent
    for( FILE *fp = popen( cmd, "r" ); fp; rc = pclose(fp), fp = 0) {
        while( fgets(buf, 4096 - 1, fp) ) {
            char *r = strrchr(buf, '\r'); if(r) *r = 0;
            char *n = strrchr(buf, '\n'); if(n) *n = 0;
        }
    }
    return rc;
}

#if is(osx)
#include <execinfo.h> // backtrace, backtrace_symbols
#include <dlfcn.h>    // dladdr, Dl_info
#elif is(gcc) && !is(ems) && !is(mingw)
#include <execinfo.h>  // backtrace, backtrace_symbols
#elif is(win32) // && !defined __TINYC__
#include <winsock2.h>  // windows.h alternative
#include <dbghelp.h>
#pragma comment(lib, "DbgHelp")
#pragma comment(lib, "Kernel32")
static int backtrace( void **addr, int maxtraces ) {
    static HANDLE process = 0;
    if( !process ) process = GetCurrentProcess();
    if( !process ) exit( puts( "error: no current process" ) );
    static int init = 0;
    if( !init ) init = SymSetOptions(SYMOPT_UNDNAME), SymInitialize( process, NULL, TRUE );
    if( !init ) exit( puts( "error: cannot initialize DbgHelp.lib" ) );

    typedef USHORT (WINAPI *pFN)(); // TINYC
    //typedef USHORT (WINAPI *pFN)(__in ULONG, __in ULONG, __out PVOID*, __out_opt PULONG); // _MSC_VER
    static pFN rtlCaptureStackBackTrace = 0;
    if( !rtlCaptureStackBackTrace ) {
        rtlCaptureStackBackTrace = (pFN)GetProcAddress(LoadLibraryA("kernel32.dll"), "RtlCaptureStackBackTrace");
    }
    if( !rtlCaptureStackBackTrace ) {
        return 0;
    }
    return rtlCaptureStackBackTrace(1, maxtraces, (PVOID *)addr, (DWORD *) 0);
}
static char **backtrace_symbols(void *const *array,int size) {
    HANDLE process = GetCurrentProcess();
    enum { MAXSYMBOLNAME = 512 - sizeof(IMAGEHLP_SYMBOL64) };
    char symbol64_buf     [ 512 ];
    char symbol64_bufblank[ 512 ] = {0};
    IMAGEHLP_SYMBOL64 *symbol64       = (IMAGEHLP_SYMBOL64*)symbol64_buf;
    IMAGEHLP_SYMBOL64 *symbol64_blank = (IMAGEHLP_SYMBOL64*)symbol64_bufblank;
    symbol64_blank->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    symbol64_blank->MaxNameLength = (MAXSYMBOLNAME-1) / 2; //wchar?

    int symlen = size * sizeof(char *);
    char **symbols = (char **)SYS_REALLOC(0, symlen);

    if( symbols ) {
        for( int i = 0; i < size; ++i ) {
            symbols[ i ] = NULL;
        }

        char begin[1024];
        for( int i = 0; i < size; ++i ) {
            char **symbuf, *buffer = begin;

            DWORD64 dw1 = 0, dw2 = 0;
            *symbol64 = *symbol64_blank;
            if( SymGetSymFromAddr64( process, (DWORD64)array[i], &dw1, symbol64 ) ) {
                IMAGEHLP_LINE64 l64 = {0};
                l64.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
                if( SymGetLineFromAddr64( process, (DWORD64)array[i], (DWORD*)&dw2, &l64 ) ) {
                    // int lenbase( const char *str );
                    int base = 0; // lenbase( l64.FileName );
                    buffer += sprintf(buffer,"%s (%s:%d)%c", symbol64->Name, &l64.FileName[ base ], l64.LineNumber, 0);
                } else {
                    buffer += sprintf(buffer,"%s (??)%c", symbol64->Name, 0);
                }
            } else  buffer += sprintf(buffer,"(??)%c", 0);

            size_t buflen = buffer - begin + 1;

            symbuf = (char **)SYS_REALLOC( symbols, symlen + buflen );
            if( symbuf ) {
                memcpy( (char *)symbuf + symlen, begin, buflen );
                symbuf[ i ] = (char *)(size_t)symlen;

                symbols = symbuf;
                symlen += buflen;
            } else break;
        }

        for( int i = 0; i < size; ++i ) {
            symbols[ i ] = (char *)symbols + (size_t)symbols[ i ];
        }
    }

    return symbols;
}
#else
static int backtrace(void **heap, int num) { return 0; }
static char **backtrace_symbols(void *const *sym,int num) { return 0; }
#endif

void trace_cb( int traces, int (*yield)(const char *)) {
    enum { skip = 1 }; /* exclude 1 trace from stack (this function) */
    enum { maxtraces = 128 };

    int inc = 1;
    if( traces < 0 ) traces = -traces, inc = -1;
    if( traces == 0 ) return;
    if( traces > maxtraces ) traces = maxtraces;

    void *stack[ maxtraces ];
    traces = backtrace( stack, traces );
    char **symbols = backtrace_symbols( stack, traces );

    char demangled[1024] = "??", buf[1024];
    int L = 0, B = inc>0 ? skip - 1 : traces, E = inc>0 ? traces : skip - 1;
    for( int i = B; ( i += inc ) != E; ) {
#if is(linux)
        char *address = strstr( symbols[i], "[" ) + 1; address[strlen(address) - 1] = '\0';
        char *binary = symbols[i]; strstr( symbols[i], "(" )[0] = '\0';
        char command[1024]; sprintf(command, "addr2line -e %s %s", binary, address);
        for( FILE *fp = popen( command, "r" ); fp ; pclose(fp), fp = 0 ) {
            fgets(demangled, sizeof(demangled), fp);
            int len = strlen(demangled); while( len > 0 && demangled[len-1] < 32 ) demangled[--len] = 0;
        }
        symbols[i] = demangled;
#elif is(osx)
        /*struct*/ Dl_info info;
        if( dladdr(stack[i], &info) && info.dli_sname ) {
            char *dmgbuf = info.dli_sname[0] != '_' ? NULL :
                 ifdef(cpp, __cxa_demangle(info.dli_sname, NULL, 0, NULL), info.dli_sname);
            strcpy( demangled, dmgbuf ? dmgbuf : info.dli_sname );
            symbols[i] = demangled;
            FREE( dmgbuf );
        }
#endif
        sprintf(buf, "%03d: %#016p %s", ++L, stack[i], symbols[i]);
        //sprintf(buf, "%03d: %s", ++L, symbols[i]);
        if( yield(buf) < 0 ) break;
    }

    SYS_REALLOC( symbols, 0 );
}

static local char *trace_strbuf[128] = {0};
static local int trace_counter = 0, trace_len = 0;
int trace_(const char *line) {
    int len = strlen(line);
    trace_len += len + 1;
    trace_strbuf[trace_counter] = (char*)SYS_REALLOC(trace_strbuf[trace_counter], (len * 1.5));
    strcpy(trace_strbuf[trace_counter], line );
    trace_counter = (trace_counter +1) % 128;
    return 1;
}
char *callstack( int traces ) {
#if is(linux)
    return ""; // @fixme: not really working as expected
#else
    //if( trace_ ) trace_str_ = SYS_REALLOC(trace_str_, trace_lenbuf_ = 0);
    trace_counter = trace_len = 0;
    trace_cb( traces, trace_ );
    static local char *buf = 0; // @fixme: 1 leak per invoking thread
    SYS_REALLOC(buf, 0);
    buf = (char*)SYS_REALLOC( 0, trace_len + 1 ); buf[0] = 0;
    for( int i = 0; i < trace_counter; ++i ) {
        strcat(buf, trace_strbuf[i] ); // <-- optimize
        strcat(buf, "\n");
    }
    return buf ? buf : ""; // @fixme: should return NULL if no callstack is retrieved?
#endif
}
int callstackf( FILE *fp, int traces ) {
    char *buf = callstack(traces);
    fputs(buf, fp);
    return 0;
}

// -----------------------------------------------------------------------------
// endian

#if is(msc)
#include <stdlib.h>
#define swap16 _byteswap_ushort
#define swap32 _byteswap_ulong
#define swap64 _byteswap_uint64
#elif is(gcc)
#define swap16 __builtin_bswap16
#define swap32 __builtin_bswap32
#define swap64 __builtin_bswap64
#else
uint16_t swap16( uint16_t x ) { return (x << 8) | (x >> 8); }
uint32_t swap32( uint32_t x ) { x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); return (x << 16) | (x >> 16); }
uint64_t swap64( uint64_t x ) { x = ((x <<  8) & 0xff00ff00ff00ff00ULL) | ((x >>  8) & 0x00ff00ff00ff00ffULL); x = ((x << 16) & 0xffff0000ffff0000ULL) | ((x >> 16) & 0x0000ffff0000ffffULL); return (x << 32) | (x >> 32); }
#endif

float    swap32f(float n)  { union { float  t; uint32_t i; } conv; conv.t = n; conv.i = swap32(conv.i); return conv.t; }
double   swap64f(double n) { union { double t; uint64_t i; } conv; conv.t = n; conv.i = swap64(conv.i); return conv.t; }

#define is_big()    ((*(uint16_t *)"\0\1") == 1)
#define is_little() ((*(uint16_t *)"\0\1") != 1)

uint16_t  lil16(uint16_t n) { return is_big()    ? swap16(n) : n; }
uint32_t  lil32(uint32_t n) { return is_big()    ? swap32(n) : n; }
uint64_t  lil64(uint64_t n) { return is_big()    ? swap64(n) : n; }
uint16_t  big16(uint16_t n) { return is_little() ? swap16(n) : n; }
uint32_t  big32(uint32_t n) { return is_little() ? swap32(n) : n; }
uint64_t  big64(uint64_t n) { return is_little() ? swap64(n) : n; }

float     lil32f(float n)  { return is_big()    ? swap32f(n) : n; }
double    lil64f(double n) { return is_big()    ? swap64f(n) : n; }
float     big32f(float n)  { return is_little() ? swap32f(n) : n; }
double    big64f(double n) { return is_little() ? swap64f(n) : n; }

uint16_t* lil16p(void *p, int sz)  { if(is_big()   ) { uint16_t *n = (uint16_t *)p; for(int i = 0; i < sz; ++i) n[i] = swap16(n[i]); } return p; }
uint16_t* big16p(void *p, int sz)  { if(is_little()) { uint16_t *n = (uint16_t *)p; for(int i = 0; i < sz; ++i) n[i] = swap16(n[i]); } return p; }
uint32_t* lil32p(void *p, int sz)  { if(is_big()   ) { uint32_t *n = (uint32_t *)p; for(int i = 0; i < sz; ++i) n[i] = swap32(n[i]); } return p; }
uint32_t* big32p(void *p, int sz)  { if(is_little()) { uint32_t *n = (uint32_t *)p; for(int i = 0; i < sz; ++i) n[i] = swap32(n[i]); } return p; }
uint64_t* lil64p(void *p, int sz)  { if(is_big()   ) { uint64_t *n = (uint64_t *)p; for(int i = 0; i < sz; ++i) n[i] = swap64(n[i]); } return p; }
uint64_t* big64p(void *p, int sz)  { if(is_little()) { uint64_t *n = (uint64_t *)p; for(int i = 0; i < sz; ++i) n[i] = swap64(n[i]); } return p; }

float   * lil32pf(void *p, int sz) { if(is_big()   ) { float    *n = (float    *)p; for(int i = 0; i < sz; ++i) n[i] = swap32f(n[i]); } return p; }
float   * big32pf(void *p, int sz) { if(is_little()) { float    *n = (float    *)p; for(int i = 0; i < sz; ++i) n[i] = swap32f(n[i]); } return p; }
double  * lil64pf(void *p, int sz) { if(is_big()   ) { double   *n = (double   *)p; for(int i = 0; i < sz; ++i) n[i] = swap64f(n[i]); } return p; }
double  * big64pf(void *p, int sz) { if(is_little()) { double   *n = (double   *)p; for(int i = 0; i < sz; ++i) n[i] = swap64f(n[i]); } return p; }

// -----------------------------------------------------------------------------
// cpu

int cpu_cores(void) {
#if is(win32)
    DWORD_PTR pm, sm;
    if( GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm) ) if( pm ) {
        int count = 0;
        while( pm ) {
            ++count;
            pm &= pm - 1;
        }
        return count;
    }
    { SYSTEM_INFO si; GetSystemInfo(&si); return (int)si.dwNumberOfProcessors; }
#else // unix
    int count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? count : 1;
#endif
#if 0
#elif is(linux)
    cpu_set_t prevmask, testmask;
    CPU_ZERO(&prevmask);
    CPU_ZERO(&testmask);
    sched_getaffinity(0, sizeof(prevmask), &prevmask);     //Get current mask
    sched_setaffinity(0, sizeof(testmask), &testmask);     //Set zero mask
    sched_getaffinity(0, sizeof(testmask), &testmask);     //Get mask for all CPUs
    sched_setaffinity(0, sizeof(prevmask), &prevmask);     //Reset current mask
    int num = CPU_COUNT(&testmask);
    return (num > 1 ? num : 1);
#elif is(cpp)
    return (int)std::thread::hardware_concurrency();
#elif defined(_OPENMP)
    // omp
    int cores = 0;
    #pragma omp parallel
    {
        #pragma omp atomic
        ++cores;
    }
    return cores;
#endif
}

// ----------------------------------------------------------------------------
// time

double time_ss() {
    return glfwGetTime();
}
double time_ms() {
    return glfwGetTime() * 1000.0;
}
uint64_t time_us() {
    return (uint64_t)(glfwGetTime() * 1000000.0); // @fixme: use a high resolution timer instead, or time_gpu below
}
#if 0
uint64_t time_gpu() {
    GLint64 t = 123456789;
    glGetInteger64v(GL_TIMESTAMP, &t);
    return (uint64_t)t;
}
#endif
uint64_t date_human() {
    time_t mtime = time(0);
    struct tm *ti = localtime(&mtime);
    return atoi64(stringf("%04d%02d%02d%02d%02d%02d",ti->tm_year+1900,ti->tm_mon+1,ti->tm_mday,ti->tm_hour,ti->tm_min,ti->tm_sec));
}
uint64_t sleep_us(uint64_t us) { // @fixme: use a high resolution sleeper instead
    return sleep_ms( us / 1000.0 );
}
double sleep_ms(double ms) {
    double now = time_ms();
    if( ms <= 0 ) {
#if is(win32)
        Sleep(0); // yield
#else
        usleep(0);
#endif
    } else {
#if is(win32)
        Sleep(ms);
#else
        usleep(ms * 1000);
#endif
    }
    return time_ms() - now;
}
double sleep_ss(double ss) {
    return sleep_ms( ss * 1000 ) / 1000.0;
}


// ----------------------------------------------------------------------------
// argc/v

int argc() { return __argc; }
char* argv(int arg) { return __argv[arg]; }

// ----------------------------------------------------------------------------
// options

const char *option(const char *commalist, const char *defaults) {
    while( commalist[0] ) {
        const char *begin = commalist;
        while(*commalist != ',' && *commalist != '\0') ++commalist;
        const char *end = commalist;

        char token[128];
        snprintf(token, 128, "%.*s", (int)(end - begin), begin);

        for( int i = 1; i < argc(); ++i ) {
            char *arg = argv(i);

            if( !strcmpi( arg, token ) ) {
                if( (i+1) < argc() ) {
                    return argv(i+1);
                }
            }
        }

        commalist = end + 1;
    }
    return defaults;
}

int optioni(const char *commalist, int defaults) {
    const char *rc = option(commalist, 0);
    return rc ? atoi(rc) : defaults;
}
float optionf(const char *commalist, float defaults) {
    const char *rc = option(commalist, 0);
    return rc ? atof(rc) : defaults;
}

// ----------------------------------------------------------------------------
// tty

void tty_color(unsigned color) {
    #if is(win32)
    do_once {
        DWORD mode = 0; SetConsoleMode(GetStdHandle(-11), (GetConsoleMode(GetStdHandle(-11), &mode), mode|4));
    }
    #endif
    if( color ) {
        // if( color == RED ) breakpoint("break on RED"); // debug
        unsigned r = (color >> 16) & 255;
        unsigned g = (color >>  8) & 255;
        unsigned b = (color >>  0) & 255;
        // 24-bit console ESC[ … 38;2;<r>;<g>;<b> … m Select RGB foreground color
        // 256-color console ESC[38;5;<fgcode>m
        // 0x00-0x07:  standard colors (as in ESC [ 30..37 m)
        // 0x08-0x0F:  high intensity colors (as in ESC [ 90..97 m)
        // 0x10-0xE7:  6*6*6=216 colors: 16 + 36*r + 6*g + b (0≤r,g,b≤5)
        // 0xE8-0xFF:  grayscale from black to white in 24 steps
        r /= 51, g /= 51, b /= 51; // [0..5]
        printf("\033[38;5;%dm", r*36+g*6+b+16); // "\033[0;3%sm", color_code);
    } else {
        printf("%s", "\x1B[39;49m"); // reset
    }
}
void tty_puts(unsigned color, const char *text) {
    tty_color(color); puts(text);
}
void tty_init() {
    tty_color(0);
}
int tty_cols() {
#if is(win32)
    CONSOLE_SCREEN_BUFFER_INFO c;
    if( GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &c) ) {
        int w = c.srWindow.Right-c.srWindow.Left-c.dwCursorPosition.X;
        return w > 2 ? w - 1 : w; // w-1 to allow window resizing to a larger dimension (already printed text would break otherwise)
    }
#endif
#ifdef TIOCGWINSZ
    struct winsize ws;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
    return ws.ws_col - 1;
#endif
#ifdef TIOCGSIZE
    struct ttysize ts;
    ioctl(STDIN_FILENO, TIOCGSIZE, &ts);
    return ts.ts_cols - 1;
#endif
    return 80;
}

// -----------------------------------------------------------------------------
// debugger

#include <stdio.h>
void hexdumpf( FILE *fp, const void *ptr, unsigned len, int width ) {
    unsigned char *data = (unsigned char*)ptr;
    for( unsigned jt = 0; jt < len; jt += width ) {
        fprintf( fp, "; %05d ", jt );
        for( unsigned it = jt, next = it + width; it < len && it < next; ++it ) {
            fprintf( fp, "%02x %s", (unsigned char)data[it], &" \n\0...\n"[ (1+it) < len ? 2 * !!((1+it) % width) : 3 ] );
        }
        fprintf( fp, "; %05d ", jt );
        for( unsigned it = jt, next = it + width; it < len && it < next; ++it ) {
            fprintf( fp, " %c %s", (signed char)data[it] >= 32 ? (signed char)data[it] : (signed char)'.', &" \n\0...\n"[ (1+it) < len ? 2 * !!((1+it) % width) : 3 ] );
        }
    }
}
void hexdump( const void *ptr, unsigned len ) {
    hexdumpf( stdout, ptr, len, 16 );
}

#if 0 // is(msc) only
static void debugbreak(void) {
    do { \
        __try { DebugBreak(); } \
        __except (GetExceptionCode() == EXCEPTION_BREAKPOINT ? \
            EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {} \
    } while(0);
}
#endif

#if is(win32)
static void debugbreak(void) { if(IsDebuggerPresent()) DebugBreak(); }
#else // is(unix)
static int is_debugger_present = -1;
static void _sigtrap_handler(int signum) {
    is_debugger_present = 0;
    signal(SIGTRAP, SIG_DFL);
}
static void debugbreak(void) { // break if debugger present
    // __builtin_trap(); //
    //raise(SIGABRT); // SIGTRAP);
    //__asm__ volatile("int $0x03");
    if( is_debugger_present < 0 ) {
        is_debugger_present = 1;
        signal(SIGTRAP, _sigtrap_handler);
        raise(SIGTRAP);
    }
}
#endif

void (alert)(const char *message) { // @todo: move to app_, besides die()
#if is(win32)
    MessageBoxA(0, message, 0,0);
#elif is(linux)
    for(FILE *fp = fopen("/tmp/fwk.warning","wb");fp;fp=0)
    fputs(message,fp), fclose(fp), system("xmessage -center -file /tmp/fwk.warning");
#elif is(osx)
    system(stringf("osascript -e 'display alert \"Alert\" message \"%s\"'", message));
#endif
}

void breakpoint(const char *reason) {
    window_visible(false);
    char *fulltext = reason[0] == '!' ? stringf("%s\n%s", reason+1, callstack(+48)) : reason;
    PRINTF("%s", fulltext);

    (alert)(fulltext);
    debugbreak();

    window_visible(true);
}

bool has_debugger() {
#if is(win32)
    return IsDebuggerPresent(); // SetLastError(123); OutputDebugStringA("\1"); enabled = GetLastError() != 123;
#else
    return false;
#endif
}

void die(const char *message) {
   fprintf(stderr, "%s\n", message);
   fflush(stderr);
   alert(message);
   exit(-1);
}

// ----------------------------------------------------------------------------
// logger

unsigned determinate_color_from_text(const char *text) {
    /**/ if( strstri(text, "fail") || strstri(text, "error") ) return RED;
    else if( strstri(text, "warn") || strstri(text, "not found") ) return YELLOW;
    return 0;
}

int (PRINTF)(const char *text, const char *stack, const char *file, int line, const char *function) {
    uint32_t color = /*errno ? RED :*/ determinate_color_from_text(text); // errno = 0;
    #if is(msc)
    char *slash = strrchr(file, '\\'); if(slash) file = slash + 1;
    #endif
    char *location = stringf("|%s|%s:%d", /*errno?strerror(errno):*/function, file, line);
    int cols = tty_cols() + 1 - (int)strlen(location);
    double ss = time_ss();

static thread_mutex_t lock, *init = 0; if(!init) thread_mutex_init(init = &lock);
thread_mutex_lock( &lock );

    tty_color(color);
    printf("\r%*.s%s", cols, "", location);
    printf("\r%07.3fs|%s%s", ss, text, stack);
    tty_color(0);

thread_mutex_unlock( &lock );
    return 1;
}

// ----------------------------------------------------------------------------
// panic

static void *panic_oom_reserve; // for out-of-memory recovery
int (PANIC)(const char *error, const char *file, int line) {
    panic_oom_reserve = SYS_REALLOC(panic_oom_reserve, 0);

    tty_color(RED);

    error += error[0] == '!';
    fprintf(stderr, "Error: %s (%s:%d) (errno:%s)\n", error, file, line, strerror(errno));
    fprintf(stderr, "%s", callstack(+16)); // no \n
    fflush(0); // fflush(stderr);

    tty_color(0);

    breakpoint(error);
    exit(-line);
    return 1;
}
