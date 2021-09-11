// dll utils
// - rlyeh, public domain

#ifdef _MSC_VER
#define IMPORT __declspec(dllimport)
#define EXPORT __declspec(dllexport)
#else
#define IMPORT
#define EXPORT
#endif

// msvc users may need to -DAPI=IMPORT/EXPORT as needed when loading/building FWK as DLL.
#ifndef API
#define API
#endif

API void* dll(const char *filename, const char *symbol);
