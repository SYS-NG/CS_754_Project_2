#include <iostream>
#include <memory>
#include <string>   
#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"

// For getting the server IP
#include <ifaddrs.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using grpc::Status;
using grpc::ServerContext;
using grpc::ServerBuilder;
using grpc::Server;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::InsecureServerCredentials;
using namespace std;

#define MB_CHUNK 1024 * 1024
#define ONE_HUNDRED_MB_CHUNK 1024 * 1024 * 100
class grpcServices final : public grpc_service::GrpcService::Service {
    private:
        std::string directory_path_; // Where All the files will get mounted

    public: 
        grpcServices(const std::string& directory_path) : directory_path_(directory_path) {}

        Status Ping(
            ServerContext*                   context,
            const grpc_service::PingRequest* request,
            grpc_service::PingResponse*      response
        ) override {
            response->set_message("Ack");
            return Status::OK;
        }
        
        Status SendAndWaitAck(
            ServerContext*                 context,
            const grpc_service::DataChunk* data,
            grpc_service::PingResponse*    response
        ) override {
            try {
                int64_t received_size = data->data().size();
                response->set_message("Ack");
                return Status::OK;
            } catch (const std::exception& e) {
                cerr << "Exception in SendAndWaitAck: " << e.what() << endl;
                return Status::CANCELLED;
            }
        }

        Status NfsGetAttr(
            ServerContext* context,
            const grpc_service::NfsGetAttrRequest* request,
            grpc_service::NfsGetAttrResponse* response
        ) override {
            const std::string path = request->path();
            struct stat st;
            if (stat((directory_path_ + path).c_str(), &st) != 0) {
                response->set_success(false);
                response->set_message("File not found");
                return Status::OK;
            }
            response->set_success(true);
            response->set_size(st.st_size);
            response->set_mode(st.st_mode);
            response->set_nlink(st.st_nlink);
            return Status::OK;
        }

        Status ServerStreamData(
            ServerContext* context,
            const grpc_service::DataRequest* request,
            ServerWriter<grpc_service::DataChunk>* writer
        ) override {
            int64_t remaining_size = request->size();
            grpc_service::DataChunk chunk;
            int64_t normal_chunk_size = MB_CHUNK;
            string data(normal_chunk_size, 'a');  // 1MB chunk
            int64_t chunk_size = min(remaining_size, normal_chunk_size);
            
            while (remaining_size > 0) {
                chunk_size = min(remaining_size, normal_chunk_size);
                chunk.set_data(data.substr(0, chunk_size));
                writer->Write(chunk);
                remaining_size -= chunk_size;
            }
            return Status::OK;
        }

        Status ClientStreamData(
            ServerContext* context,
            ServerReader<grpc_service::DataChunk>* reader,
            grpc_service::TransferStatus* status
        ) override {
            grpc_service::DataChunk chunk;
            int64_t total_bytes = 0;
            while (reader->Read(&chunk)) {
                total_bytes += chunk.data().size();
            }
            status->set_success(true);
            status->set_bytes_received(total_bytes);
            return Status::OK;
        }
};

std::string getServerIP() {
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];
    std::string ip_address;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        if (s != 0) {
            printf("getnameinfo() failed: %s\n", gai_strerror(s));
            continue;
        }

        // Skip loopback interface
        if (strcmp(ifa->ifa_name, "lo") != 0) {
            ip_address = host;
					break;
        }
    }

    freeifaddrs(ifaddr);
    return ip_address;
}

void RunServer(string remote_storage_dir_path) {

    // Check that the remote storage directory exists, if not create it
    struct stat st;
    if (stat(remote_storage_dir_path.c_str(), &st) != 0) {
        // Directory does not exist, create it
        if (mkdir(remote_storage_dir_path.c_str(), 0777) != 0) {
            perror("mkdir");
            return;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        // Path exists but is not a directory
        cerr << "Error: " << remote_storage_dir_path << " is not a directory." << endl;
        return;
    }

    cout << "Running Storage at: " << remote_storage_dir_path << endl;

    // Create GRPC Server
    string server_address = getServerIP() + ":50051";
    grpcServices service(remote_storage_dir_path);
    ServerBuilder builder;
    builder.AddListeningPort(server_address, InsecureServerCredentials());
    builder.RegisterService(&service);
    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Server listening on " << server_address << endl;
    server->Wait();
}

int main(int argc, char** argv) {
    string remote_storage_dir_path = "./remoteStore";
    if (argc > 1) {
        remote_storage_dir_path = argv[1];
    }
    RunServer(remote_storage_dir_path);
    return 0;
}