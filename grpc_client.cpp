#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using namespace grpc_service;
using namespace std;

class GrpcClient {
    private:
        unique_ptr<GrpcService::Stub> stub_;

    public:
        GrpcClient(shared_ptr<Channel> channel): stub_(GrpcService::NewStub(channel)) {}
        
};

int main(int argc, char** argv) {
    string target_str = "0.0.0.0:50051";
    if (argc > 1) {
        target_str = argv[1];
    }

    GrpcClient client(grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));

    return 0;
}