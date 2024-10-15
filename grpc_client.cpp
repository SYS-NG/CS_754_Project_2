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

            // Pings server
            ClientContext context;
            PingRequest request;
            PingResponse response;

            request.set_message("Ping");
            Status status = stub_->Ping(&context, request, &response);

            if (status.ok()) {
                cout << "Ping successful: " << response.message() << endl;
            } else {
                cerr << "Ping failed: " << status.error_code() << " - " << status.error_message() << endl;
            }
        }

        static int nfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
            cout << "Getting attributes for path: " << path << endl;
            memset(stbuf, 0, sizeof(struct stat));

            // Create gRPC client context and request/response objects
            ClientContext context;
            NfsGetAttrRequest request;
            NfsGetAttrResponse response;

            request.set_path(path);
            Status status = instance_->stub_->NfsGetAttr(&context, request, &response);

            if (status.ok() && response.success()) {
                stbuf->st_mode = response.mode();
                stbuf->st_nlink = response.nlink();
                stbuf->st_size = response.size();
            } else {
                cerr << "gRPC NfsGetAttr failed: " << status.error_code() << " - " << status.error_message() << endl;
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
    // Check if the first argument is provided
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <server_ip:port> [additional_arguments]" << endl;
        return 1;
    }

    string target_str = argv[1]; // Expecting server ip address:port

    // Create the gRPC client
    FuseGrpcClient client(grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));


    
    // Pass the rest of the arguments to run_fuse_main
    // Reduce the argument count by 1, and move the argument pointer to the next arg
    client.run_fuse_main(argc - 1, argv + 1);

    return 0;
}