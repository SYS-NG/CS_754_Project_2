#define FUSE_USE_VERSION 31

#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include <fuse3/fuse.h>
#include "grpc_service.grpc.pb.h"
#include <thread>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using namespace grpc_service;
using namespace std;

#define RUN_SYNC false

class FuseGrpcClient {
    private:
        shared_ptr<Channel> channel_;
        unique_ptr<GrpcService::Stub> stub_;
        static FuseGrpcClient* instance_;
        string target_;

    public:
        FuseGrpcClient(shared_ptr<Channel> channel, const string& target) {
            channel_ = channel;
            stub_     = GrpcService::NewStub(channel_);
            instance_ = this;
            target_ = target;

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

            int max_retries = 3;  // Set the maximum number of retries
            int retry_count = 0;  // Initialize retry count
            int backoff_time = 1; // Initial backoff time in seconds

            while (retry_count < max_retries) {
                // Create gRPC client context and request/response objects
                ClientContext context;
                NfsGetAttrRequest request;
                NfsGetAttrResponse response;

                // Set timeout for the request
                auto deadline = chrono::system_clock::now() + chrono::seconds(1);
                context.set_deadline(deadline);

                // Prepare the request
                request.set_path(path);

                // Make the gRPC call
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
                    cerr << "nfs_getattr gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;

                    if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ||
                        status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                        retry_count++;
                        cout << "Retrying " << retry_count << "/" << max_retries << " after " << backoff_time << " second(s)..." << endl;

                        // Wait for a backoff period before retrying
                        this_thread::sleep_for(chrono::seconds(backoff_time));

                        // Increase backoff time for the next retry
                        backoff_time *= 2;

                        // Reconnect if UNAVAILABLE
                        if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                            cout << "Re-establishing gRPC connection..." << endl;
                            instance_->channel_ = grpc::CreateChannel(instance_->target_, grpc::InsecureChannelCredentials());
                            instance_->stub_ = GrpcService::NewStub(instance_->channel_);
                        }
                    } else {
                        // Other errors, don't retry
                        return -EIO;
                    }
                }
            }

            cerr << "Failed to get attributes after " << max_retries << " retries." << endl;
            return -EIO; // Input/output error for failed retries
        }

        static int nfs_release(const char *path, struct fuse_file_info *fi) {
            cout << "Releasing file: " << path << endl;

            int max_retries = 3;  // Set the maximum number of retries
            int retry_count = 0;  // Initialize retry count
            int backoff_time = 1; // Initial backoff time in seconds

            while (retry_count < max_retries) {
                // Create gRPC client context and request/response objects
                ClientContext context;
                NfsReleaseRequest request;
                NfsReleaseResponse response;

                // Set timeout for the request
                auto deadline = chrono::system_clock::now() + chrono::seconds(1);
                context.set_deadline(deadline);

                // Prepare the request
                request.set_path(path);
                request.set_flags(fi->flags);

                // Make the gRPC call
                Status status;
                if (RUN_SYNC) {
                    status = instance_->stub_->NfsRelease(&context, request, &response);
                } else {
                    status = instance_->stub_->NfsReleaseAsync(&context, request, &response);
                }

                if (status.ok()) {
                    if (response.success()) {
                        cout << "File released successfully: " << path << endl;
                        return 0; // Operation successful
                    } else {
                        cerr << "gRPC NfsRelease failed: " << response.message() << endl;
                        return -response.errorcode(); // Map the errno from server to FUSE error code
                    }
                } else {
                    cerr << "nfs_release gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;

                    // Retry on timeout or transient error
                    if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ||
                        status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                        retry_count++;
                        cout << "Retrying " << retry_count << "/" << max_retries << " after " << backoff_time << " second(s)..." << endl;

                        // Wait for a backoff period before retrying
                        this_thread::sleep_for(chrono::seconds(backoff_time));

                        // Increase backoff time for the next retry
                        backoff_time *= 2;

                        // Reconnect if UNAVAILABLE
                        if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                            cout << "Re-establishing gRPC connection..." << endl;
                            instance_->channel_ = grpc::CreateChannel(instance_->target_, grpc::InsecureChannelCredentials());
                            instance_->stub_ = GrpcService::NewStub(instance_->channel_);
                        }
                    } else {
                        // Other errors, don't retry
                        return -EIO;
                    }
                }
            }

            cerr << "Failed to release file after " << max_retries << " retries." << endl;
            return -EIO; // Input/output error for failed retries
        }

        static int nfs_open(const char *path, struct fuse_file_info *fi) {
            cout << "Opening file: " << path << endl;

            int max_retries = 3;  // Set the maximum number of retries
            int retry_count = 0;  // Initialize retry count
            int backoff_time = 1; // Initial backoff time in seconds

            while (retry_count < max_retries) {
                // Create gRPC client context and request/response objects
                ClientContext context;
                NfsOpenRequest request;
                NfsOpenResponse response;

                // Set timeout for the request (e.g., 1 second)
                auto deadline = chrono::system_clock::now() + chrono::seconds(1);
                context.set_deadline(deadline);

                // Prepare the request
                request.set_path(path);
                request.set_flags(fi->flags);

                // Make the gRPC call
                Status status = instance_->stub_->NfsOpen(&context, request, &response);

                if (status.ok()) {
                    if (response.success()) {
                        fi->fh = -1;
                        return 0; // File opened successfully
                    } else {
                        cerr << "gRPC NfsOpen failed: " << response.message() << endl;
                        return -response.errorcode(); // Return the error code from server to FUSE as a negative value
                    }
                } else {
                    cerr << "nfs_open gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;

                    // Retry on timeout or transient error
                    if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ||
                        status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                        retry_count++;
                        cout << "Retrying " << retry_count << "/" << max_retries << " after " << backoff_time << " second(s)..." << endl;

                        // Wait for a backoff period before retrying
                        this_thread::sleep_for(chrono::seconds(backoff_time));

                        // Increase backoff time for the next retry
                        backoff_time *= 2;

                        // Reconnect if UNAVAILABLE
                        if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                            cout << "Re-establishing gRPC connection..." << endl;
                            instance_->channel_ = grpc::CreateChannel(instance_->target_, grpc::InsecureChannelCredentials());
                            instance_->stub_ = GrpcService::NewStub(instance_->channel_);
                        }
                    } else {
                        // Other errors, don't retry
                        return -EIO;
                    }
                }
            }

            cerr << "Failed to open file after " << max_retries << " retries." << endl;
            return -EIO; // Input/output error for failed retries
        }
    
        // Write should return exactly the number of bytes requested except on error
        static int nfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
            cout << "Write to file: " << path << endl;
            cout << "Buffer content to write: " << string(buf, size) << endl; // Log the buffer content

            int max_retries = 3;  // Set the maximum number of retries
            int retry_count = 0;  // Initialize retry count
            int backoff_time = 1; // Initial backoff time in seconds


            while (retry_count < max_retries) {
                // Create gRPC client context and request/response objects
                ClientContext context;
                NfsWriteRequest request;
                NfsWriteResponse response;

                // Set timeout for the request (e.g., 1 second)
                auto deadline = chrono::system_clock::now() + chrono::seconds(1);
                context.set_deadline(deadline);

                // Prepare the request
                request.set_path(path);
                request.set_content(buf);
                request.set_size(size);
                request.set_offset(offset);

                // Make the gRPC call
                Status status;
                if (RUN_SYNC) {
                    status = instance_->stub_->NfsWrite(&context, request, &response);
                } else {
                    status = instance_->stub_->NfsWriteAsync(&context, request, &response);
                }

                if (status.ok()) {
                    if (response.success()) {
                        int64_t len = response.bytes_written();
                        return len; // Operation successful, return bytes written
                    } else {
                        cerr << "gRPC NfsWrite failed: " << response.message() << endl;
                        return -response.errorcode(); // Map the errno from server to FUSE error code
                    }
                } else {
                    cerr << "nfs_write gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;

                    // Retry on timeout or transient error
                    if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ||
                        status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                        retry_count++;
                        cout << "Retrying " << retry_count << "/" << max_retries << " after " << backoff_time << " second(s)..." << endl;

                        // Wait for a backoff period before retrying
                        this_thread::sleep_for(chrono::seconds(backoff_time));

                        // Increase backoff time for the next retry
                        backoff_time *= 2;

                        // Reconnect if UNAVAILABLE
                        if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                            cout << "Re-establishing gRPC connection..." << endl;
                            instance_->channel_ = grpc::CreateChannel(instance_->target_, grpc::InsecureChannelCredentials());
                            instance_->stub_ = GrpcService::NewStub(instance_->channel_);
                        }
                    } else {
                        // Other errors, don't retry
                        return -EIO;
                    }
                }
            }

            cerr << "Failed to write to file after " << max_retries << " retries." << endl;
            return -EIO; // Input/output error for failed retries
        }

        static int nfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
            cout << "Reading file: " << path << endl;

            int max_retries = 3;  // Set the maximum number of retries
            int retry_count = 0;  // Initialize retry count
            int backoff_time = 1; // Initial backoff time in seconds


            while (retry_count < max_retries) {
                // Create gRPC client context and request/response objects
                ClientContext context;
                NfsReadRequest request;
                NfsReadResponse response;

                // Set timeout for the request (e.g., 1 second)
                auto deadline = chrono::system_clock::now() + chrono::seconds(1);
                context.set_deadline(deadline);

                // Prepare the request
                request.set_path(path);
                request.set_offset(offset);
                request.set_flags(fi->flags);
                request.set_size(size);

                // Make the gRPC call
                Status status = instance_->stub_->NfsRead(&context, request, &response);

                if (status.ok()) {
                    if (response.success()) {
                        int64_t len = response.size();

                        if (len <= size) {
                            size = len;
                            cout << "Read " << len << " bytes from file: " << path << endl; // Log the length of content read
                            cout << "Content: " << response.content() << endl; // Log the content read
                            memcpy(buf, response.content().data(), size);
                            return size; // Successfully read bytes
                        } else {
                            cerr << "Error: Read size (" << len << ") exceeds buffer size (" << size << ")." << endl;
                            return -EFBIG; // Return an error indicating that the file is too large
                        }
                    } else {
                        cerr << "gRPC NfsRead failed: " << response.message() << endl;
                        return -response.errorcode(); // Return the error code from server to FUSE as a negative value
                    }
                } else {
                    cerr << "nfs_read gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;

                    // Retry on timeout or transient error
                    if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ||
                        status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                        retry_count++;
                        cout << "Retrying " << retry_count << "/" << max_retries << " after " << backoff_time << " second(s)..." << endl;

                        // Wait for a backoff period before retrying
                        this_thread::sleep_for(chrono::seconds(backoff_time));

                        // Increase backoff time for the next retry
                        backoff_time *= 2;

                        // Reconnect if UNAVAILABLE
                        if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                            cout << "Re-establishing gRPC connection..." << endl;
                            instance_->channel_ = grpc::CreateChannel(instance_->target_, grpc::InsecureChannelCredentials());
                            instance_->stub_ = GrpcService::NewStub(instance_->channel_);
                        }
                    } else {
                        // Other errors, don't retry
                        return -EIO;
                    }
                }
            }

            cerr << "Failed to read after " << max_retries << " retries." << endl;
            return -EIO; // Input/output error for failed retries
        }

        static int nfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
            cout << "Reading directory: " << path << endl;

            int max_retries = 3;  // Set the maximum number of retries
            int retry_count = 0;  // Initialize retry count
            int backoff_time = 1; // Initial backoff time in seconds

            while (retry_count < max_retries) {
                // Create gRPC client context and request/response objects
                ClientContext context;
                NfsReadDirRequest request;
                NfsReadDirResponse response;

                // Set timeout for the request (e.g., 1 second)
                auto deadline = chrono::system_clock::now() + chrono::seconds(1);
                context.set_deadline(deadline);

                // Prepare the request
                request.set_path(path);

                // Make the gRPC call
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
                        return -response.errorcode(); // Return the error code from server to FUSE as a negative value
                    }
                } else {
                    cerr << "nfs_readdir gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;

                    // Retry on timeout or transient error
                    if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ||
                        status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                        retry_count++;
                        cout << "Retrying " << retry_count << "/" << max_retries << " after " << backoff_time << " second(s)..." << endl;

                        // Wait for a backoff period before retrying
                        this_thread::sleep_for(chrono::seconds(backoff_time));

                        // Increase backoff time for the next retry
                        backoff_time *= 2;

                        // Reconnect if UNAVAILABLE
                        if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                            cout << "Re-establishing gRPC connection..." << endl;
                            instance_->channel_ = grpc::CreateChannel(instance_->target_, grpc::InsecureChannelCredentials());
                            instance_->stub_ = GrpcService::NewStub(instance_->channel_);
                        }
                    } else {
                        // Other errors, don't retry
                        return -EIO;
                    }
                }
            }

            cerr << "Failed to read directory after " << max_retries << " retries." << endl;
            return -EIO; // Input/output error for failed retries
        }

        static int nfs_unlink(const char *path) {
            cout << "Unlinking file: " << path << endl;

            int max_retries = 3;  // Set the maximum number of retries
            int retry_count = 0;  // Initialize retry count
            int backoff_time = 1; // Initial backoff time in seconds

            while (retry_count < max_retries) {
                // Create gRPC client context and request/response objects
                ClientContext context;
                NfsUnlinkRequest request;
                NfsUnlinkResponse response;

                // Set timeout for the request (e.g., 1 second)
                auto deadline = chrono::system_clock::now() + chrono::seconds(1);
                context.set_deadline(deadline);

                // Prepare the request
                request.set_path(path);

                // Make the gRPC call
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
                    cerr << "nfs_unlink gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;

                    // Retry on timeout or transient error
                    if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ||
                        status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                        retry_count++;
                        cout << "Retrying " << retry_count << "/" << max_retries << " after " << backoff_time << " second(s)..." << endl;

                        // Wait for a backoff period before retrying
                        this_thread::sleep_for(chrono::seconds(backoff_time));

                        // Increase backoff time for the next retry
                        backoff_time *= 2;

                        // Reconnect if UNAVAILABLE
                        if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                            cout << "Re-establishing gRPC connection..." << endl;
                            instance_->channel_ = grpc::CreateChannel(instance_->target_, grpc::InsecureChannelCredentials());
                            instance_->stub_ = GrpcService::NewStub(instance_->channel_);
                        }
                    } else {
                        // Other errors, don't retry
                        return -EIO;
                    }
                }
            }

            cerr << "Failed to unlink file after " << max_retries << " retries." << endl;
            return -EIO; // Input/output error for failed retries
        }

        static int nfs_rmdir(const char *path) {
            cout << "Removing directory: " << path << endl;

            int max_retries = 3;  // Set the maximum number of retries
            int retry_count = 0;  // Initialize retry count
            int backoff_time = 1; // Initial backoff time in seconds

            while (retry_count < max_retries) {
                // Create gRPC client context and request/response objects
                ClientContext context;
                NfsRmdirRequest request;
                NfsRmdirResponse response;

                // Set timeout for the request (e.g., 1 second)
                auto deadline = chrono::system_clock::now() + chrono::seconds(1);
                context.set_deadline(deadline);

                // Prepare the request
                request.set_path(path);

                // Make the gRPC call
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
                    cerr << "nfs_rmdir gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;

                    // Retry on timeout or transient error
                    if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ||
                        status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                        retry_count++;
                        cout << "Retrying " << retry_count << "/" << max_retries << " after " << backoff_time << " second(s)..." << endl;

                        // Wait for a backoff period before retrying
                        this_thread::sleep_for(chrono::seconds(backoff_time));

                        // Increase backoff time for the next retry
                        backoff_time *= 2;

                        // Reconnect if UNAVAILABLE
                        if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                            cout << "Re-establishing gRPC connection..." << endl;
                            instance_->channel_ = grpc::CreateChannel(instance_->target_, grpc::InsecureChannelCredentials());
                            instance_->stub_ = GrpcService::NewStub(instance_->channel_);
                        }
                    } else {
                        // Other errors, don't retry
                        return -EIO;
                    }
                }
            }

            cerr << "Failed to remove directory after " << max_retries << " retries." << endl;
            return -EIO; // Input/output error for failed retries
        }

        static int nfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
            cout << "Creating file: " << path << " with mode: " << oct << mode << endl;

            if (mode == 0) {
                mode = 0666;
            }

            int max_retries = 3;  // Set the maximum number of retries
            int retry_count = 0;  // Initialize retry count
            int backoff_time = 1; // Initial backoff time in seconds

            while (retry_count < max_retries) {
                // Create gRPC client context and request/response objects
                ClientContext context;
                NfsCreateRequest request;
                NfsCreateResponse response;

                // Set timeout for the request (e.g., 1 second)
                auto deadline = chrono::system_clock::now() + chrono::seconds(1);
                context.set_deadline(deadline);

                // Prepare the request
                request.set_path(path);
                request.set_mode(mode);

                // Make the gRPC call
                Status status = instance_->stub_->NfsCreate(&context, request, &response);

                if (status.ok()) {
                    if (response.success()) {
                        cout << "File created successfully: " << path << endl;
                        fi->fh = -1;
                        return 0; // File created successfully
                    } else {
                        cerr << "gRPC NfsCreate failed: " << response.message() << endl;
                        return -response.errorcode(); // Return the error code from server to FUSE as a negative value
                    }
                } else {
                    cerr << "nfs_create gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;

                    // Retry on timeout or transient error
                    if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ||
                        status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                        retry_count++;
                        cout << "Retrying " << retry_count << "/" << max_retries << " after " << backoff_time << " second(s)..." << endl;

                        // Wait for a backoff period before retrying
                        this_thread::sleep_for(chrono::seconds(backoff_time));

                        // Increase backoff time for the next retry
                        backoff_time *= 2;

                        // Reconnect if UNAVAILABLE
                        if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                            cout << "Re-establishing gRPC connection..." << endl;
                            instance_->channel_ = grpc::CreateChannel(instance_->target_, grpc::InsecureChannelCredentials());
                            instance_->stub_ = GrpcService::NewStub(instance_->channel_);
                        }
                    } else {
                        // Other errors, don't retry
                        return -EIO;
                    }
                }
            }

            cerr << "Failed to create file after " << max_retries << " retries." << endl;
            return -EIO; // Input/output error for failed retries
        }

        static int nfs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
            cout << "Updating timestamps for path: " << path << endl;

            int max_retries = 3;  // Set the maximum number of retries
            int retry_count = 0;  // Initialize retry count
            int backoff_time = 1; // Initial backoff time in seconds

            while (retry_count < max_retries) {
                // Create gRPC client context and request/response objects
                ClientContext context;
                NfsUtimensRequest request;
                NfsUtimensResponse response;

                // Set timeout for the request (e.g., 1 second)
                auto deadline = chrono::system_clock::now() + chrono::seconds(1);
                context.set_deadline(deadline);

                // Prepare the request
                request.set_path(path);
                request.set_atime(tv[0].tv_sec);  // Set access time from tv[0]
                request.set_mtime(tv[1].tv_sec);  // Set modification time from tv[1]

                // Make the gRPC call
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
                    cerr << "nfs_utimens gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;

                    // Retry on timeout or transient error
                    if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ||
                        status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                        retry_count++;
                        cout << "Retrying " << retry_count << "/" << max_retries << " after " << backoff_time << " second(s)..." << endl;

                        // Wait for a backoff period before retrying
                        this_thread::sleep_for(chrono::seconds(backoff_time));

                        // Increase backoff time for the next retry
                        backoff_time *= 2;

                        // Reconnect if UNAVAILABLE
                        if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                            cout << "Re-establishing gRPC connection..." << endl;
                            instance_->channel_ = grpc::CreateChannel(instance_->target_, grpc::InsecureChannelCredentials());
                            instance_->stub_ = GrpcService::NewStub(instance_->channel_);
                        }
                    } else {
                        // Other errors, don't retry
                        return -EIO;
                    }
                }
            }

            cerr << "Failed to update timestamps after " << max_retries << " retries." << endl;
            return -EIO; // Input/output error for failed retries
        }

        static int nfs_mkdir(const char *path, mode_t mode) {
            if (mode == 0) {
                mode = 0755;
            }
            cout << "Creating directory: " << path << " with mode: " << mode << endl;

            int max_retries = 3;  // Set the maximum number of retries
            int retry_count = 0;  // Initialize retry count
            int backoff_time = 1; // Initial backoff time in seconds

            while (retry_count < max_retries) {
                // Create gRPC client context and request/response objects
                ClientContext context;
                NfsMkdirRequest request;
                NfsMkdirResponse response;

                // Set timeout for the request (e.g., 1 second)
                auto deadline = chrono::system_clock::now() + chrono::seconds(1);
                context.set_deadline(deadline);

                // Prepare the request
                request.set_path(path);
                request.set_mode(mode);

                // Make the gRPC call
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
                    cerr << "nfs_mkdir gRPC communication failed: " << status.error_code() << " - " << status.error_message() << endl;

                    // Retry on timeout or transient error
                    if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ||
                        status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                        retry_count++;
                        cout << "Retrying " << retry_count << "/" << max_retries << " after " << backoff_time << " second(s)..." << endl;

                        // Wait for a backoff period before retrying
                        this_thread::sleep_for(chrono::seconds(backoff_time));

                        // Increase backoff time for the next retry
                        backoff_time *= 2;

                        // Reconnect if UNAVAILABLE
                        if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                            cout << "Re-establishing gRPC connection..." << endl;
                            instance_->channel_ = grpc::CreateChannel(instance_->target_, grpc::InsecureChannelCredentials());
                            instance_->stub_ = GrpcService::NewStub(instance_->channel_);
                        }
                    } else {
                        // Other errors, don't retry
                        return -EIO;
                    }
                }
            }

            cerr << "Failed to create directory after " << max_retries << " retries." << endl;
            return -EIO; // Input/output error for failed retries
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
    FuseGrpcClient client(grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()), target_str);
    
    // Pass the rest of the arguments to run_fuse_main
    // Reduce the argument count by 1, and move the argument pointer to the next arg
    client.run_fuse_main(argc - 1, argv + 1);

    return 0;
}