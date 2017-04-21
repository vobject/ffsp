#ifndef FUSE_FFSP_LOG_HPP
#define FUSE_FFSP_LOG_HPP

#include "fuse.h"

#include <ostream>

#ifdef _WIN32
std::ostream& operator<<(std::ostream& os, const struct FUSE_STAT& stat);
#else
std::ostream& operator<<(std::ostream& os, const struct stat& stat);
#endif
std::ostream& operator<<(std::ostream& os, const fuse_file_info& fi);
std::ostream& operator<<(std::ostream& os, const struct statvfs& sfs);
std::ostream& operator<<(std::ostream& os, const fuse_conn_info& conn);
std::ostream& operator<<(std::ostream& os, const struct timespec& tv);

#endif // FUSE_FFSP_LOG_HPP
