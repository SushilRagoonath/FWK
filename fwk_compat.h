#if defined __TINYC__ && defined _WIN32
#define MAPVK_VSC_TO_VK 1
#define MAPVK_VK_TO_VSC 0
#define IPV6_V6ONLY 0
#define _WIN32_WINNT_VISTA 0
#define _WIN32_WINNT_WINXP 0
#define _WIN32_WINNT_WIN7 0
#endif
#ifdef __TINYC__
#define STBI_NO_SIMD
// no uint128_t (3rd_https.h)
// no interlock (3rd_thread.h)
float fabsf    (float f)            { return (float)abs(f); }
float sqrtf    (float f)            { return (float)sqrt(f); }
float sinf     (float f)            { return (float)sin(f); }
float asinf    (float f)            { return (float)asin(f); }
float cosf     (float f)            { return (float)cos(f); }
float acosf    (float f)            { return (float)acos(f); }
float tanf     (float f)            { return (float)tan(f); }
float atan2f   (float f1, float f2) { return (float)atan2(f1, f2); }
float powf     (float f1, float f2) { return (float)pow(f1, f2); }
float floorf   (float f)            { return (float)floor(f); }
float logf     (float f)            { return (float)log(f); }
float ceilf    (float f)            { return (float)ceil(f); }
float copysignf(float f, float g)   { return (float)copysign(f,g); }
char* strtok_s(
   char* str,
   const char* delimiters,
   char** context
);
#endif
#if defined __TINYC__ && defined _WIN32
errno_t strerror_s(
   char *buffer,
   size_t sizeInBytes,
   int errnum
);
typedef int socklen_t;
#define restrict
const char *inet_ntop(int af, const void *restrict src,
                     char *restrict dst, socklen_t size);
int inet_pton(int af, const char *restrict src, void *restrict dst);
errno_t fopen_s(
   FILE** pFile,
   const char *filename,
   const char *mode
);
int execv(const char *path, char *const argv[]);
#endif
