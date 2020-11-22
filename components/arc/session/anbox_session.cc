#include "components/arc/session/anbox_session.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"

#include "anbox/application/database.h"
#include "anbox/bridge/android_api_stub.h"
#include "anbox/bridge/platform_api_skeleton.h"
#include "anbox/network/published_socket_connector.h"
#include "anbox/network/local_socket_messenger.h"
#include "anbox/bridge/platform_message_processor.h"
#include "anbox/rpc/channel.h"
#include "anbox/rpc/connection_creator.h"
// #include "anbox/rpc/template_message_processor.h"
#include "anbox/wm/single_window_manager.h"
#include "anbox/runtime.h"
#include "anbox/logger.h"

#include "anbox/platform/null/platform.h"
#include "anbox/graphics/rect.h"

#include "anbox_rpc.pb.h"
#include "anbox_bridge.pb.h"

// #ifdef USE_PROTOBUF_CALLBACK_HEADER
#include <google/protobuf/stubs/callback.h>
// #endif

#include "fydeos/anbox_rpc_chrome.h"


namespace arc{

class PlatformMessageProcessor2: 
  public anbox::rpc::MessageProcessor {
 public:
  PlatformMessageProcessor2(const std::shared_ptr<anbox::network::MessageSender> &sender,    
    const std::shared_ptr<anbox::rpc::PendingCallCache> &pending_calls,
    const std::shared_ptr<AnboxSession> &anbox_session
    ):
    anbox::rpc::MessageProcessor(sender, pending_calls),
    anbox_session_(anbox_session){        
  }      

  ~PlatformMessageProcessor2(){    
  }

  void dispatch(anbox::rpc::Invocation const &invocation) override {
    LOG(INFO) << "===== PlatformMessageProcessor2::dispatch";    

    // invoke(this, this, &PlatformMessageProcessor2::remove_task, invocation);
  }

  void process_event_sequence(const std::string &raw_events) override {
    LOG(INFO) << "===== PlatformMessageProcessor2::process_event_sequence";

    anbox::protobuf::bridge::EventSequence seq;
    auto hasEventSeq = seq.ParseFromString(raw_events);    

    LOG(INFO) << "===== PlatformMessageProcessor2::process_event_sequence hasEventSeq " << hasEventSeq;

    if (hasEventSeq && seq.has_application_list_update()){
      auto event = seq.application_list_update();

      for (int n = 0; n < event.removed_applications_size(); n++) {
        anbox::application::Database::Item item;

        const auto app = event.removed_applications(n);
        item.package = app.package();

        if (item.package.empty())
          continue;
        
        // app_db_->remove(item);

        for (auto& observer: anbox_session_->GetObserver()){
          observer.onAppRemoved(&item);
        }
        

        LOG(INFO) << "===== PlatformMessageProcessor2::process_event_sequence remove: " << app.name() << " " << app.package();
      }

      for (int n = 0; n < event.applications_size(); n++) {
        anbox::application::Database::Item item;

        const auto app = event.applications(n);
        item.name = app.name();
        item.package = app.package();

        const auto li = app.launch_intent();
        item.launch_intent.action = li.action();
        item.launch_intent.uri = li.uri();
        item.launch_intent.type = li.uri();
        item.launch_intent.package = li.package();
        item.launch_intent.component = li.component();

        for (int m = 0; m < li.categories_size(); m++)
          item.launch_intent.categories.push_back(li.categories(m));

        item.icon = std::vector<char>(app.icon().begin(), app.icon().end());

        if (item.package.empty())
          continue;

        // app_db_->store_or_update(item);
                
        for (auto& observer: anbox_session_->GetObserver()){
          observer.onAppAdded(&item);
        }

        LOG(INFO) << "===== PlatformMessageProcessor2::process_event_sequence new: " << app.name() << " " << app.package();
      }

      return;
    }        
    
    anbox::protobuf::rpc::Result seq2;
    auto hasResult = seq2.ParseFromString(raw_events);

    LOG(INFO) << "===== PlatformMessageProcessor2::process_event_sequence hasResult " << hasResult;

    if (hasResult && seq2.has_id()){      
      
      LOG(INFO) << "===== PlatformMessageProcessor2::process_event_sequence result " << seq2.id() << " " << seq2.response();      
            
      auto it = anbox_session_->GetLaunchedApps().find(seq2.response());
      if (it != anbox_session_->GetLaunchedApps().end()){

        for (auto &observer: anbox_session_->GetObserver()){
          observer.OnTaskCreated(seq2.id(), it->second);

          // OnTaskSetActive->
        }     

        anbox_session_->GetLaunchedApps().erase(it);
      }            
    }
  }

  // void remove_task(anbox::protobuf::bridge::RemoveTask const *request,
  //                               anbox::protobuf::rpc::Void *response,
  //                               google::protobuf::Closure *done) {
  // }

private:  
  std::shared_ptr<AnboxSession> anbox_session_;    
};

AnboxSession::AnboxSession(): rt_(anbox::Runtime::create()){}
AnboxSession::~AnboxSession(){  
}

void AnboxSession::Initialize(){  
  anbox::Log().Init(anbox::Logger::Severity::kDebug);    

  // mkdir("/run/chrome/anbox/", S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  // mkdir("/run/chrome/anbox/sockets", S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);      
  
  publish_socket_ = std::make_shared<anbox::network::PublishedSocketConnector>(
    // std::string("/run/chrome/anbox/sockets/anbox_bridge2"), rt_,
    std::string(ANBOX_RPC_CHROME_NAME), rt_,
    std::make_shared<anbox::rpc::ConnectionCreator>(rt_, [&](const std::shared_ptr<anbox::network::MessageSender> &sender) {

      LOG(INFO) << "====== AnboxSession::Initialize connected";          

      auto pendding_call = std::make_shared<anbox::rpc::PendingCallCache>();

      channel_ = std::make_shared<anbox::rpc::Channel>(pendding_call, sender);                    
            
      return std::make_shared<PlatformMessageProcessor2>(
        sender,
        pendding_call,
        shared_from_this()
      );
    })
  );

  rt_->start();  
}

bool AnboxSession::LaunchApp(std::string &name, std::string &package, std::string &component){
  LOG(INFO) << "====== AnboxSession::LaunchApp " << package << " " << component;

  {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    launch_wait_handle_.expect_result();
  }

  auto c = std::make_shared<anbox::protobuf::rpc::Void>();    
  anbox::protobuf::bridge::LaunchApplication message;  
  message.set_stack(::anbox::protobuf::bridge::LaunchApplication_Stack_FREEFORM);

  auto launch_intent = message.mutable_intent();
  launch_intent->set_component(component);
  launch_intent->set_package(package);        
        
  launched_apps_.insert({name, AppInfo{name, package, component}});  

  channel_->call_method(
    "launch_application", &message, c.get(),
    google::protobuf::NewCallback(this, &AnboxSession::callback)
  );    

  launch_wait_handle_.wait_for_pending(std::chrono::milliseconds{30 * 1000});
  if (!launch_wait_handle_.has_result()){
    auto it = launched_apps_.find(name);
    if (it != launched_apps_.end()){
      launched_apps_.erase(it);
    }
    LOG(INFO) <<  "====== AnboxSession::LaunchApp failed " << package << " " << component;
  }

  // for (auto &observer: observer_list_){
  //   observer.OnTaskCreated(next_task_id.fetch_add(1), package, component);
  // }     

  // channel_->call_method()  

  // auto c = std::make_shared<anbox::Request<anbox::protobuf::rpc::Void>>();
  // protobuf::bridge::LaunchApplication message;

  // constexpr const char *default_appmgr_package{"org.anbox.appmgr"};
  // constexpr const char *default_appmgr_component{"org.anbox.appmgr.AppViewActivity"};

  // android::Intent launch_intent;
  // launch_intent.package = default_appmgr_package;
  // launch_intent.component = default_appmgr_component;
  // // As this will only be executed in single window mode we don't have
  // // to specify and launch bounds.
  // android_api_stub->launch(
  //   launch_intent, 
  //   graphics::Rect::Invalid, 
  //   // graphics::Rect(600, 500),
  //   // wm::Stack::Id::Default 
  //   wm::Stack::Id::Freeform
  // );

  return true;
}

}