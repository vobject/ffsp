#include <fuse.h>

fuse_context* fuse_get_context()
{
    static fuse_context dummy_ctx = {};
    return &dummy_ctx;
}
