//
// Created by danielsf97 on 2/4/20.
//

#include "fuse_w.h"

template<class T> fuse_wrapper::t_getattr fuse_wrapper::fuse<T>::getattr = nullptr;
template<class T> fuse_wrapper::t_readlink fuse_wrapper::fuse<T>::readlink = nullptr;
template<class T> fuse_wrapper::t_mknod fuse_wrapper::fuse<T>::mknod = nullptr;
template<class T> fuse_wrapper::t_mkdir fuse_wrapper::fuse<T>::mkdir = nullptr;
template<class T> fuse_wrapper::t_unlink fuse_wrapper::fuse<T>::unlink = nullptr;
template<class T> fuse_wrapper::t_rmdir fuse_wrapper::fuse<T>::rmdir = nullptr;
template<class T> fuse_wrapper::t_symlink fuse_wrapper::fuse<T>::symlink = nullptr;
template<class T> fuse_wrapper::t_rename fuse_wrapper::fuse<T>::rename = nullptr;
template<class T> fuse_wrapper::t_link fuse_wrapper::fuse<T>::link = nullptr;
template<class T> fuse_wrapper::t_chmod fuse_wrapper::fuse<T>::chmod = nullptr;
template<class T> fuse_wrapper::t_chown fuse_wrapper::fuse<T>::chown = nullptr;
template<class T> fuse_wrapper::t_truncate fuse_wrapper::fuse<T>::truncate = nullptr;
template<class T> fuse_wrapper::t_open fuse_wrapper::fuse<T>::open = nullptr;
template<class T> fuse_wrapper::t_read fuse_wrapper::fuse<T>::read = nullptr;
template<class T> fuse_wrapper::t_write fuse_wrapper::fuse<T>::write = nullptr;
template<class T> fuse_wrapper::t_statfs fuse_wrapper::fuse<T>::statfs = nullptr;
template<class T> fuse_wrapper::t_flush fuse_wrapper::fuse<T>::flush = nullptr;
template<class T> fuse_wrapper::t_release fuse_wrapper::fuse<T>::release = nullptr;
template<class T> fuse_wrapper::t_fsync fuse_wrapper::fuse<T>::fsync = nullptr;
template<class T> fuse_wrapper::t_setxattr fuse_wrapper::fuse<T>::setxattr = nullptr;
template<class T> fuse_wrapper::t_getxattr fuse_wrapper::fuse<T>::getxattr = nullptr;
template<class T> fuse_wrapper::t_listxattr fuse_wrapper::fuse<T>::listxattr = nullptr;
template<class T> fuse_wrapper::t_removexattr fuse_wrapper::fuse<T>::removexattr = nullptr;
template<class T> fuse_wrapper::t_opendir fuse_wrapper::fuse<T>::opendir = nullptr;
template<class T> fuse_wrapper::t_readdir fuse_wrapper::fuse<T>::readdir = nullptr;
template<class T> fuse_wrapper::t_releasedir fuse_wrapper::fuse<T>::releasedir = nullptr;
template<class T> fuse_wrapper::t_fsyncdir fuse_wrapper::fuse<T>::fsyncdir = nullptr;
template<class T> fuse_wrapper::t_init fuse_wrapper::fuse<T>::init = nullptr;
template<class T> fuse_wrapper::t_destroy fuse_wrapper::fuse<T>::destroy = nullptr;
template<class T> fuse_wrapper::t_access fuse_wrapper::fuse<T>::access = nullptr;
template<class T> fuse_wrapper::t_create fuse_wrapper::fuse<T>::create = nullptr;
template<class T> fuse_wrapper::t_lock fuse_wrapper::fuse<T>::lock = nullptr;
template<class T> fuse_wrapper::t_utimens fuse_wrapper::fuse<T>::utimens = nullptr;
template<class T> fuse_wrapper::t_bmap fuse_wrapper::fuse<T>::bmap = nullptr;
template<class T> fuse_wrapper::t_ioctl fuse_wrapper::fuse<T>::ioctl = nullptr;
template<class T> fuse_wrapper::t_poll fuse_wrapper::fuse<T>::poll = nullptr;
template<class T> fuse_wrapper::t_write_buf fuse_wrapper::fuse<T>::write_buf = nullptr;
template<class T> fuse_wrapper::t_read_buf fuse_wrapper::fuse<T>::read_buf = nullptr;
template<class T> fuse_wrapper::t_flock fuse_wrapper::fuse<T>::flock = nullptr;
template<class T> fuse_wrapper::t_fallocate fuse_wrapper::fuse<T>::fallocate = nullptr;

template<class T> struct fuse_operations fuse_wrapper::fuse<T>::operations_;