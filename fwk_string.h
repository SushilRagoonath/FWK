// string framework
// - rlyeh, public domain

#ifndef STRING_H
#define STRING_H

// string: temporary api (stack)
API char*   stringf(const char *fmt, ...);
#define     stringf(...) (printf || printf(__VA_ARGS__), stringf(__VA_ARGS__))  // vs2015 check trick

#if 1
// string: allocated api (heap)
API char*   stringf_cat(char *x, const char *buf);
#define     stringf_cat(s,fmt,...)  stringf_cat((s), stringf(fmt, __VA_ARGS__)) // stringfcat ?
#define     stringf_del(s)         ((REALLOC((s), 0)), (s)=0) // stringfdel ?
#endif

#if defined _MSC_VER || (defined __TINYC__ && defined _WIN32)
#define strtok_r strtok_s
#endif

#define each_substring(str, delims, keyname) \
    ( int len_ = strlen(str) + 1; len_; len_ = 0 ) \
    for( char buf_[1024], *ptr_ = len_ < 1024 ? buf_ : REALLOC(0, len_), *lit_ = (char*)(str), *_bak = (snprintf(ptr_, len_, "%s", lit_), ptr_); _bak; _bak = 0, (ptr_ == buf_ ? 0 : REALLOC(ptr_, 0)) ) \
    for( char *next_token = 0, *keyname = strtok_r(_bak, delims, &next_token); keyname; keyname = strtok_r(NULL, delims, &next_token) )

// utils

API int          strmatch(const char *s, const char *wildcard);

API int          strcmp_qsort(const void *a, const void *b);
API int          strcmpi_qsort(const void *a, const void *b);

API bool         strbegi(const char *src, const char *sub);  // returns true if both strings match at beginning. case insensitive
API bool         strendi(const char *src, const char *sub);  // returns true if both strings match at end. case insensitive
API const char * strstri(const char *src, const char *sub);  // returns find first substring in string. case insensitive.
#define          strcmpi  ifdef(msc, _stricmp, strcasecmp)

API char *       strrepl(char **copy, const char *target, const char *replace); // replace any 'target' as 'repl' in 'copy'. returns 'copy'
API char *       strswap(char *copy, const char *target, const char *replace);  // replaced only if repl is shorter than target. no allocations.
API char *       strcut(char *copy, const char *target);                        // remove any 'target' in 'copy'. returns 'copy'

API char *       str16to8(const wchar_t *str); // convert from wchar16(win) to utf8/ascii

API const char * strlerp(unsigned numpairs, const char **pairs, const char *str); // using key-value pairs, null-terminated

#ifndef __APPLE__ // BSD provides these
API size_t       strlcat(char *dst, const char *src, size_t dstcap); // concat 2 strings safely. always NUL terminates. may truncate.
API size_t       strlcpy(char *dst, const char *src, size_t dstcap); // copy 2 strings safely. always NUL terminates. truncates if retval>=dstcap
#endif

API array(char*) strsplit(const char *string, const char *delimiters);
API char*        strjoin(array(char*) list, const char *separator);

#endif // STRING_H
