#include "chrome/browser/chromeos/arc/session/anbox_session_manager.h"

#include "components/arc/session/anbox_session.h"


//#include <boost/thread/thread.hpp>

namespace arc{

AnboxSessionManager* g_anbox_session_manager = nullptr;

AnboxSessionManager::AnboxSessionManager():  
  anbox_session_(std::make_shared<arc::AnboxSession>()){
  // app_db_(std::make_shared<anbox::application::Database>()) {
  // android_api_stub_ = std::make_shared<anbox::bridge::AndroidApiStub>();  

  LOG(INFO) << "==== AnboxSessionManager::AnboxSessionManager " << anbox_session_.use_count();

  g_anbox_session_manager = this;
}

void AnboxSessionManager::Initialize(){
  // auto platform = std::make_shared<anbox::platform::NullPlatform>();    

  anbox_session_->Initialize();
  
  // base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},  
  //   base::BindOnce(&AnboxSessionManager::callback2, base::Unretained(this))
  // );    

  // base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
  //   base::BindOnce(&AnboxSessionManager::callback3, base::Unretained(this))       
  // );

  // LOG(INFO) << "====== AnboxSessionManager::init rt_ start";

  // auto app_db = std::make_shared<anbox::application::Database>();  

  // auto display_frame = anbox::graphics::Rect::Invalid;
  // auto window_manager = std::make_shared<anbox::wm::SingleWindowManager>(platform, display_frame, app_db);  

  // auto pending_calls = std::make_shared<anbox::rpc::PendingCallCache>();
  // // auto socket = std::make_shared<boost::asio::local::stream_protocol::socket>(rt_->service());  

  // auto const messenger = std::make_shared<anbox::network::LocalSocketMessenger>(std::string("/run/chrome/anbox/sockets/anbox_bridge"), rt_);

  // auto rpc_channel = std::make_shared<anbox::rpc::Channel>(pending_calls, messenger);  

  // android_api_stub_->set_rpc_channel(rpc_channel);  

  // constexpr const char *default_appmgr_package{"org.anbox.appmgr"};
  // constexpr const char *default_appmgr_component{"org.anbox.appmgr.AppViewActivity"};

  // anbox::android::Intent launch_intent;
  // launch_intent.package = default_appmgr_package;
  // launch_intent.component = default_appmgr_component;
  // // As this will only be executed in single window mode we don't have
  // // to specify and launch bounds.
  // android_api_stub_->launch(
  //   launch_intent, 
  //   anbox::graphics::Rect::Invalid, 
  //   // graphics::Rect(600, 500),
  //   // wm::Stack::Id::Default 
  //   anbox::wm::Stack::Id::Freeform
  // );                

  
  
  // auto const processor = message_processor_factory_(messenger);

  // auto bridge_connector = std::make_shared<anbox::network::PublishedSocketConnector>(
  //   "/dev/anbox_bridge", rt_, std::make_shared<anbox::rpc::ConnectionCreator>(rt_, [&](const std::shared_ptr<anbox::network::MessageSender> &sender) {
  //         auto pending_calls = std::make_shared<anbox::rpc::PendingCallCache>();
  //         // auto rpc_channel =
  //         //     std::make_shared<rpc::Channel>(pending_calls, sender);
  //         // // This is safe as long as we only support a single client. If we
  //         // // support
  //         // // more than one one day we need proper dispatching to the right
  //         // // one.
  //         // android_api_stub->set_rpc_channel(rpc_channel);

          
  //         // auto server = std::make_shared<anbox::bridge::PlatformApiSkeleton>(
  //         //     pending_calls, platform, window_manager, app_db);
  //         // server->register_boot_finished_handler([&]() {
  //         //   DEBUG("Android successfully booted");
  //         //   android_api_stub->ready().set(true);
  //         //   appmgr_start_timer.expires_from_now(default_appmgr_startup_delay);
  //         //   appmgr_start_timer.async_wait([&](const boost::system::error_code &err) {
  //         //     if (err != boost::system::errc::success)
  //         //       return;
  //         //     launch_appmgr_if_needed(android_api_stub);
  //         //   });
  //         // });

  //         return std::make_shared<PlatformMessageProcessor2>(sender, pending_calls);
  //         // return std::make_shared<anbox::bridge::PlatformMessageProcessor>(
  //         //     sender, server, pending_calls);
  //       }));

  // rt_->start();
}

AnboxSessionManager* AnboxSessionManager::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_anbox_session_manager;
}

}