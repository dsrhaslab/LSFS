#ifndef fuse_pt_header_util_h_
#define fuse_pt_header_util_h_

#include <boost/regex.hpp>
#include "lsfs/fuse_common/macros.h"

static boost::regex string_capacity_size_regex("(\\d+)([bBkKmM])$");
static boost::regex temp_extensions("(\\.swx$|\\.swp$|\\.inf$|/\\.|~$)");
static boost::regex parent_dir_pattern("(.*)/[^/]+$");
static boost::regex child_name_pattern("/([^/]+)$");

bool is_temp_file(const std::string& path);
std::unique_ptr<std::string> get_parent_dir(const std::string& path);
std::unique_ptr<std::string> get_child_name(const std::string& path);
bool operator <(const timespec& lhs, const timespec& rhs);
std::string merge_metadata(std::string& bytes, std::string& new_bytes);
size_t convert_string_size_to_num_bytes(const std::string &format_size);


#endif /* fuse_pt_header_util_h_ */
