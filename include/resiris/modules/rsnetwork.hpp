#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "resiris/module.hpp"

namespace resiris {

// State behind RSNetwork.html_server_handler(). The CLI binds to 127.0.0.1;
// a firmware host can replace the socket handling without changing the API.
struct HtmlServerState : FrameAwareState {
    ~HtmlServerState() override;

    int port = 0;
    int listen_fd = -1;
    int client_fd = -1;
    bool created = false;
    bool started = false;

    std::string page_content;
    std::string client_script;

    // The one request the server is allowed to keep. `client_fd` stays open
    // until the next process() call sends the buffered response.
    bool has_request = false;
    bool request_read = false;
    std::string request_value;
    std::string pending_response;
};

class RsNetworkModule : public Module {
public:
    std::string module_name() const override { return "RSNetwork"; }

    std::vector<std::string> function_names() const override;

    std::map<std::string, std::string> variables() const override {
        return {
            {"ROUTEMODE_CRE", "string"},
            {"ROUTEMODE_SR", "string"},
            {"ROUTEMODE_MB", "string"},
        };
    }

    Value call_function(const std::string& fn, const std::vector<Value>& args) override;
    Value call_object_method(const std::string& fn, const std::shared_ptr<FrameAwareState>& handle,
                             const std::vector<Value>& args) override;
    Value get_constant(const std::string& name) const override;
};

} // namespace resiris
