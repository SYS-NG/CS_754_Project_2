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

            if (status.ok()) {
                if (response.success()) {
                    stbuf->st_mode = response.mode();
                    stbuf->st_nlink = response.nlink();
                    stbuf->st_size = response.size();
                    return 0; // Operation successful
                } else {
                    cerr << "gRPC NfsGetAttr failed: " << response.message() << endl;
                    return -response.errorcode(); // Map the errno from server to FUSE error code
                }
            } else {
                cerr << "gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;
                return -EIO; // Input/output error for failed communication
            }
        }

        static int nfs_release(const char *path, struct fuse_file_info *fi) {
            cout << "Releasing file: " << path << " with file handle: " << fi->fh << endl;

            // Create gRPC client context and request/response objects
            ClientContext context;
            NfsReleaseRequest request;
            NfsReleaseResponse response;

            request.set_filedescriptor(fi->fh);
            Status status = instance_->stub_->NfsRelease(&context, request, &response);

            if (status.ok()) {
                if (response.success()) {
                    cout << "File released successfully: " << path << endl;
                    return 0; // File released successfully
                } else {
                    cerr << "gRPC NfsRelease failed: " << response.message() << endl;
                    return -response.errorcode(); // Return the error code from server to FUSE as a negative value
                }
            } else {
                cerr << "gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;
                return -EIO; // Input/output error for failed communication
            }
        }

        static int nfs_open(const char *path, struct fuse_file_info *fi) {
            cout << "Opening file: " << path << endl;

            // Create gRPC client context and request/response objects
            ClientContext context;
            NfsOpenRequest request;
            NfsOpenResponse response;

            request.set_path(path);
            request.set_flags(fi->flags);
            Status status = instance_->stub_->NfsOpen(&context, request, &response);

            if (status.ok()) {
                if (response.success()) {
                    fi->fh = response.filedescriptor();
                    return 0; // File opened successfully
                } else {
                    cerr << "gRPC NfsOpen failed: " << response.message() << endl;
                    return -response.errorcode(); // Return the errorcode from server to FUSE as a negative value
                }
            } else {
                cerr << "gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;
                return -EIO; // Input/output error for failed communication
            }
        }

        

        // Write should return exactly the number of bytes requested except on error
        static int nfs_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
            cout << "Write to file: " << path << endl;
            cout << "Buffer content to write: " << string(buf, size) << endl; // Log the buffer content

            // Create gRPC client context and request/response objects
            ClientContext context;
            NfsWriteRequest request;
            NfsWriteResponse response;

            request.set_filedescriptor(fi->fh);
            request.set_content(buf);
            request.set_size(size);
            request.set_offset(offset);

            Status status = instance_->stub_->NfsWrite(&context, request, &response);

            int64_t len;

            if (status.ok() && response.success()) {
                len = response.bytes_written();
            } else {
                cerr << "gRPC NfsWrite failed: " << status.error_code() << " - " << status.error_message() << endl;
                return -ENOENT;
            }

            return len;
        }

        static int nfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
            cout << "Reading file: " << path << endl;
            
            // Create gRPC client context and request/response objects
            ClientContext context;
            NfsReadRequest request;
            NfsReadResponse response;

            request.set_filedescriptor(fi->fh);
            Status status = instance_->stub_->NfsRead(&context, request, &response);

            int64_t len;

            if (status.ok()) {
                if (response.success()) {
                    len = response.size();

                    if (offset < len) {
                        if (offset + size > len) {
                            size = len - offset;
                        }
                        cout << "Read " << len << " bytes from file: " << path << endl; // Log the length of content read
                        cout << "Content: " << response.content() << endl; // Log the content read
                        memcpy(buf, response.content().data() + offset, size);
                        return size; // Successfully read bytes
                    } else {
                        return 0; // Offset is beyond the file size, nothing to read
                    }
                } else {
                    cerr << "gRPC NfsRead failed: " << response.message() << endl;
                    return -response.errorcode(); // Return the errorcode from server to FUSE as a negative value
                }
            } else {
                cerr << "gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;
                return -EIO; // Input/output error for failed communication
            }
        }

        static int nfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
            cout << "Reading directory: " << path << endl;

            // Create gRPC client context and request/response objects
            ClientContext context;
            NfsReadDirRequest request;
            NfsReadDirResponse response;

            request.set_path(path);
            Status status = FuseGrpcClient::instance_->stub_->NfsReadDir(&context, request, &response);

            if (status.ok()) {
                if (response.success()) {
                    // Add files from the response
                    for (const auto& file : response.files()) {
                        cout << file.c_str() << endl;
                        filler(buf, file.c_str(), NULL, 0, FUSE_FILL_DIR_PLUS);
                    }
                    return 0; // Operation successful
                } else {
                    cerr << "gRPC NfsReadDir failed: " << response.message() << endl;
                    return -response.errorcode(); // Return the errorcode from server to FUSE as a negative value
                }
            } else {
                cerr << "gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;
                return -EIO; // Input/output error for failed communication
            }
        }

        static int nfs_unlink(const char *path) {
            cout << "Unlinking file: " << path << endl;

            // Create gRPC client context and request/response objects
            ClientContext context;
            NfsUnlinkRequest request;
            NfsUnlinkResponse response;

            request.set_path(path);
            Status status = instance_->stub_->NfsUnlink(&context, request, &response);

            if (status.ok()) {
                if (response.success()) {
                    cout << "File unlinked successfully: " << path << endl;
                    return 0; // File unlinked successfully
                } else {
                    cerr << "gRPC NfsUnlink failed: " << response.message() << endl;
                    return -response.errorcode(); // Return the error code from server to FUSE as a negative value
                }
            } else {
                cerr << "gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;
                return -EIO; // Input/output error for failed communication
            }
        }

        static int nfs_rmdir(const char *path) {
            cout << "Removing directory: " << path << endl;

            // Create gRPC client context and request/response objects
            ClientContext context;
            NfsRmdirRequest request;
            NfsRmdirResponse response;

            request.set_path(path);
            Status status = instance_->stub_->NfsRmdir(&context, request, &response);

            if (status.ok()) {
                if (response.success()) {
                    cout << "Directory removed successfully: " << path << endl;
                    return 0; // Directory removed successfully
                } else {
                    cerr << "gRPC NfsRmdir failed: " << response.message() << endl;
                    return -response.errorcode(); // Return the error code from server to FUSE as a negative value
                }
            } else {
                cerr << "gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;
                return -EIO; // Input/output error for failed communication
            }
        }

        static int nfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
            cout << "Creating file: " << path << " with mode: " << oct << mode << endl;

            if (mode == 0) {
                mode = 0666;
            }

            ClientContext context;
            NfsCreateRequest request;
            NfsCreateResponse response;

            request.set_path(path);
            request.set_mode(mode);
            Status status = instance_->stub_->NfsCreate(&context, request, &response);

            if (status.ok()) {
                if (response.success()) {
                    cout << "File created successfully: " << path << endl;
                    fi->fh = response.filehandle();
                    return 0;
                } else {
                    cerr << "gRPC NfsCreate failed: " << response.message() << endl;
                    return -response.errorcode();
                }
            } else {
                cerr << "gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;
                return -EIO;
            }
        }


        static int nfs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
            cout << "Updating timestamps for path: " << path << endl;

            // Create gRPC client context and request/response objects
            ClientContext context;
            NfsUtimensRequest request;
            NfsUtimensResponse response;

            request.set_path(path);
            request.set_atime(tv[0].tv_sec);  // Set access time from tv[0]
            request.set_mtime(tv[1].tv_sec);  // Set modification time from tv[1]

            Status status = instance_->stub_->NfsUtimens(&context, request, &response);

            if (status.ok()) {
                if (response.success()) {
                    cout << "Timestamps updated successfully for path: " << path << endl;
                    return 0; // Success
                } else {
                    cerr << "gRPC NfsUtimens failed: " << response.errorcode() << " - " << response.message() << endl;
                    return -response.errorcode(); // Return the error code from server
                }
            } else {
                cerr << "gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;
                return -EIO; // Communication failure, return I/O error
            }
        }

        static int nfs_mkdir(const char *path, mode_t mode) {
            if (mode == 0) {
                mode = 0755;
            }
            cout << "Creating directory: " << path << " with mode: " << mode << endl;

            // Create gRPC client context and request/response objects
            ClientContext context;
            NfsMkdirRequest request;
            NfsMkdirResponse response;

            request.set_path(path);
            request.set_mode(mode);
            Status status = instance_->stub_->NfsMkdir(&context, request, &response);

            if (status.ok()) {
                if (response.success()) {
                    cout << "Directory created successfully: " << path << endl;
                    return 0; // Success
                } else {
                    cerr << "gRPC NfsMkdir failed with error code: " << response.errorcode() << " - " << response.message() << endl;
                    return -response.errorcode(); // Return the error code from the server
                }
            } else {
                cerr << "gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;
                return -EIO; // Communication failure, return I/O error
            }
        }

        static int nfs_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
            cout << "Truncate called on file: " << path << " with size: " << size << endl;
            return 0; // Indicate success
        }

        void run_fuse_main(int argc, char** argv)
        {
            static struct fuse_operations nfs_oper = {
                .getattr = nfs_getattr,
                .mkdir   = nfs_mkdir,
                .unlink  = nfs_unlink,
                .rmdir   = nfs_rmdir,
                .truncate = nfs_truncate,
                .open    = nfs_open,
                .read    = nfs_read,
                .write   = nfs_write,
                .release = nfs_release,
                .readdir = nfs_readdir,
                .create  = nfs_create,
                .utimens = nfs_utimens,
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