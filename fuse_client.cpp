#define FUSE_USE_VERSION 31
// #include "/u7/x47zou/.local/include/fuse3/fuse.h"
#include <fuse3/fuse.h>
#include <cstring>
#include <iostream>
#include <errno.h>

static const char *file_path = "/testfile";
static const char *file_content = "Hello, FUSE!\n";

// 获取文件或目录的属性
static int nfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) { // 根目录
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (strcmp(path, file_path) == 0) { // 文件路径
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(file_content);
    } else {
        return -ENOENT;
    }
    return 0;
}

// 打开文件
static int nfs_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, file_path) != 0)
        return -ENOENT;
    return 0;
}

// 读取文件内容
static int nfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, file_path) != 0)
        return -ENOENT;

    size_t len = strlen(file_content);
    if (offset < len) {
        if (offset + size > len)
            size = len - offset;
        memcpy(buf, file_content + offset, size);
    } else {
        size = 0;
    }
    return size;
}

// 读取目录内容
static int nfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    
    if (strcmp(path, "/") != 0) {
        return -ENOENT;
    }

    // 添加默认目录项 '.' 和 '..'
    filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);

    // 添加文件 'testfile'
    filler(buf, "testfile", NULL, 0, FUSE_FILL_DIR_PLUS);

    return 0;
}

static struct fuse_operations nfs_oper = {
    .getattr = nfs_getattr,
    .open = nfs_open,
    .read = nfs_read,
    .readdir = nfs_readdir, // 添加 readdir 函数
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &nfs_oper, NULL);
}