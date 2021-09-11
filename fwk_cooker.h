// -----------------------------------------------------------------------------
// asset pipeline framework
// - rlyeh, public domain.
//
// all cooked assets are stored inside .cook.zip file at root folder, which acts as an asset database.
// during game boot, the database gets rebuilt as follows: (note: step 0 is an optional optimization)
// 0. for N given cores, split list of infiles into N zipfiles. then, parallelize cookers.
// 1. compare local disk files against file in zip database. for each mismatch do:
// 2. - invalidate its entry in database, if local file was removed from disk.
// 3. - write its *cooked* contents into database, if local file was created or modified from disk.
// 4. mount any existing zipfile(s) after cooking.
//
// notes: meta-datas from every raw asset are stored into comment field, inside .cook.zip archive.
// @todo: fix leaks
// @todo: symlink exact files
// @todo: idle threads should steal jobs from busy threads (maybe use jobs/coroutines for this?)
// @todo: ... and should compress them in the original cook[N] bucket

enum {
    COOKER_ASYNC = 1,
};

// user defined callback for asset cooking:
// must read infile, process data, and write it to outfile
// must set errno on exit if errors are found
// must return compression level if archive needs to be cooked, else return <0
typedef int (*cooker_callback_t)(char *filename, const char *ext, const char header[16], FILE *in, FILE *out, const char *info, int threadid);

API int  cooker_progress(); // [0..100]
API bool cooker( const char *masks, cooker_callback_t cb, int flags );
