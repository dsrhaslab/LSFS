//
// Created by danielsf97 on 2/4/20.
//

#include "fuse_wrapper.h"

template<class T> fuse_wrapper::t_getattr fuse_wrapper::fuse<T>::_getattr = nullptr;
template<class T> fuse_wrapper::t_readlink fuse_wrapper::fuse<T>::_readlink = nullptr;
template<class T> fuse_wrapper::t_mknod fuse_wrapper::fuse<T>::_mknod = nullptr;
template<class T> fuse_wrapper::t_mkdir fuse_wrapper::fuse<T>::_mkdir = nullptr;
template<class T> fuse_wrapper::t_unlink fuse_wrapper::fuse<T>::_unlink = nullptr;
template<class T> fuse_wrapper::t_rmdir fuse_wrapper::fuse<T>::_rmdir = nullptr;
template<class T> fuse_wrapper::t_symlink fuse_wrapper::fuse<T>::_symlink = nullptr;
template<class T> fuse_wrapper::t_rename fuse_wrapper::fuse<T>::_rename = nullptr;
template<class T> fuse_wrapper::t_link fuse_wrapper::fuse<T>::_link = nullptr;
template<class T> fuse_wrapper::t_chmod fuse_wrapper::fuse<T>::_chmod = nullptr;
template<class T> fuse_wrapper::t_chown fuse_wrapper::fuse<T>::_chown = nullptr;
template<class T> fuse_wrapper::t_truncate fuse_wrapper::fuse<T>::_truncate = nullptr;
template<class T> fuse_wrapper::t_open fuse_wrapper::fuse<T>::_open = nullptr;
template<class T> fuse_wrapper::t_read fuse_wrapper::fuse<T>::_read = nullptr;
template<class T> fuse_wrapper::t_write fuse_wrapper::fuse<T>::_write = nullptr;
template<class T> fuse_wrapper::t_statfs fuse_wrapper::fuse<T>::_statfs = nullptr;
template<class T> fuse_wrapper::t_flush fuse_wrapper::fuse<T>::_flush = nullptr;
template<class T> fuse_wrapper::t_release fuse_wrapper::fuse<T>::_release = nullptr;
template<class T> fuse_wrapper::t_fsync fuse_wrapper::fuse<T>::_fsync = nullptr;
template<class T> fuse_wrapper::t_setxattr fuse_wrapper::fuse<T>::_setxattr = nullptr;
template<class T> fuse_wrapper::t_getxattr fuse_wrapper::fuse<T>::_getxattr = nullptr;
template<class T> fuse_wrapper::t_listxattr fuse_wrapper::fuse<T>::_listxattr = nullptr;
template<class T> fuse_wrapper::t_removexattr fuse_wrapper::fuse<T>::_removexattr = nullptr;
template<class T> fuse_wrapper::t_opendir fuse_wrapper::fuse<T>::_opendir = nullptr;
template<class T> fuse_wrapper::t_readdir fuse_wrapper::fuse<T>::_readdir = nullptr;
template<class T> fuse_wrapper::t_releasedir fuse_wrapper::fuse<T>::_releasedir = nullptr;
template<class T> fuse_wrapper::t_fsyncdir fuse_wrapper::fuse<T>::_fsyncdir = nullptr;
template<class T> fuse_wrapper::t_init fuse_wrapper::fuse<T>::_init = nullptr;
template<class T> fuse_wrapper::t_destroy fuse_wrapper::fuse<T>::_destroy = nullptr;
template<class T> fuse_wrapper::t_access fuse_wrapper::fuse<T>::_access = nullptr;
template<class T> fuse_wrapper::t_create fuse_wrapper::fuse<T>::_create = nullptr;
template<class T> fuse_wrapper::t_lock fuse_wrapper::fuse<T>::_lock = nullptr;
template<class T> fuse_wrapper::t_utimens fuse_wrapper::fuse<T>::_utimens = nullptr;
template<class T> fuse_wrapper::t_bmap fuse_wrapper::fuse<T>::_bmap = nullptr;
template<class T> fuse_wrapper::t_ioctl fuse_wrapper::fuse<T>::_ioctl = nullptr;
template<class T> fuse_wrapper::t_poll fuse_wrapper::fuse<T>::_poll = nullptr;
template<class T> fuse_wrapper::t_write_buf fuse_wrapper::fuse<T>::_write_buf = nullptr;
template<class T> fuse_wrapper::t_read_buf fuse_wrapper::fuse<T>::_read_buf = nullptr;
template<class T> fuse_wrapper::t_flock fuse_wrapper::fuse<T>::_flock = nullptr;
template<class T> fuse_wrapper::t_fallocate fuse_wrapper::fuse<T>::_fallocate = nullptr;

template<class T> struct fuse_operations fuse_wrapper::fuse<T>::operations_;