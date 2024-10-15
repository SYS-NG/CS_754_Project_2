#define FUSE_USE_VERSION 31

#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include <fuse3/fuse.h>
#include "grpc_service.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using namespace grpc_service;
using namespace std;

class FuseGrpcClient {
    private:
        unique_ptr<GrpcService::Stub> stub_;
        static FuseGrpcClient* instance_;

    public:
        FuseGrpcClient(shared_ptr<Channel> channel) {
            stub_     = GrpcService::NewStub(channel);
            instance_ = this;
        }

        static int nfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
            cout << "Getting attributes for path: " << path << endl;
            memset(stbuf, 0, sizeof(struct stat));
            if (strcmp(path, "/") == 0) { // root directory
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
            } else if (strcmp(path, "/testfile") == 0) { // file path
                stbuf->st_mode = S_IFREG | 0444;
                stbuf->st_nlink = 1;
                stbuf->st_size = strlen("Testing Testing!\n");
            } else {
                return -ENOENT;
            }
            return 0;
        }

        static int nfs_open(const char *path, struct fuse_file_info *fi) {
            cout << "Opening file: " << path << endl;
            if (strcmp(path, "/testfile") != 0)
                return -ENOENT;
            return 0;
        }

        static int nfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
            cout << "Reading file: " << path << endl;
            const char *file_content = "Testing Testing!\n";
            size_t len = strlen(file_content);
            if (strcmp(path, "/testfile") != 0)
                return -ENOENT;

            if (offset < len) {
                if (offset + size > len)
                    size = len - offset;
                memcpy(buf, file_content + offset, size);
            } else {
                size = 0;
            }
            return size;
        }

        static int nfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
            cout << "Reading directory: " << path << endl;
            if (strcmp(path, "/") != 0) {
                return -ENOENT;
            }

            // Add default directory entries '.' and '..'
            filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
            filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
            // Add file 'testfile'
            filler(buf, "testfile", NULL, 0, FUSE_FILL_DIR_PLUS);

            return 0;
        }

        void run_fuse_main(int argc, char** argv)
        {
            static struct fuse_operations nfs_oper = {
                .getattr = nfs_getattr,
                .open    = nfs_open,
                .read    = nfs_read,
                .readdir = nfs_readdir,
            };

            fuse_main(argc, argv, &nfs_oper, NULL);
        }
};

FuseGrpcClient* FuseGrpcClient::instance_ = nullptr;

int main(int argc, char** argv) {
    string target_str = "0.0.0.0:50051";
    if (argc > 1) {
        target_str = argv[1];
    }

    FuseGrpcClient client(grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
    client.run_fuse_main(argc, argv);

    return 0;
}