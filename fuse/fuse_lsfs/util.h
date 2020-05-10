#ifndef fuse_pt_header_util_h_
#define fuse_pt_header_util_h_

/* -------------------------------------------------------------------------- */

#include "fuse/fuse_common/fuse35.h"
//#include <regex>
#include <boost/regex.hpp>

/* -------------------------------------------------------------------------- */

static boost::regex temp_extensions("(\\.swx$|\\.swp$|\\.inf$|/\\.|~$)");
static boost::regex parent_dir_pattern("(.*)/[^/]+$");
static boost::regex child_name_pattern("/([^/]+)$");
//static std::regex parent_dir_pattern("(.*)/[^/]+$");
//static std::regex child_name_pattern("/([^/]+)$");


bool is_temp_file(const std::string& path);
std::unique_ptr<std::string> get_parent_dir(const std::string& path);
std::unique_ptr<std::string> get_child_name(const std::string& path);
bool operator <(const timespec& lhs, const timespec& rhs);
std::string merge_metadata(std::string& bytes, std::string& new_bytes);


/* -------------------------------------------------------------------------- */
#endif /* fuse_pt_header_util_h_ */
