#ifndef fuse_pt_header_util_h_
#define fuse_pt_header_util_h_

/* -------------------------------------------------------------------------- */

#include "fuse/fuse_common/fuse35.h"
#include <regex>

/* -------------------------------------------------------------------------- */

static std::regex temp_extensions("(\\.swx$|\\.swp$|\\.inf$|/\\.|~$)");
static std::regex parent_dir_pattern("(.+)/[^/]+$");
static std::regex child_name_pattern("/([^/]+)$");

bool is_temp_file(std::string path);
std::unique_ptr<std::string> get_parent_dir(std::string path);
std::unique_ptr<std::string> get_child_name(std::string path);
bool operator <(const timespec& lhs, const timespec& rhs);

/* -------------------------------------------------------------------------- */
#endif /* fuse_pt_header_util_h_ */
