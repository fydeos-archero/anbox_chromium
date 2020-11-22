#include "chrome/browser/chromeos/arc/session/anbox_service_launcher.h"

#include <utility>

#include "ash/public/cpp/default_scale_factor_retriever.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/session/anbox_session.h"
#include "chrome/browser/chromeos/arc/session/anbox_session_manager.h"
#include "chrome/browser/apps/app_service/anbox_apps_factory.h"
#include "components/arc/anbox_service_manager.h"
#include "chrome/browser/chromeos/arc/instance_throttle/arc_instance_throttle.h"

// ChromeBrowserMainPartsChromeos owns.
anbox::AnboxServiceLauncher* g_anbox_service_launcher = nullptr;

namespace anbox{
AnboxServiceLauncher::AnboxServiceLauncher(chromeos::SchedulerConfigurationManagerBase* scheduler_configuration_manager):
  scheduler_configuration_manager_(scheduler_configuration_manager),
  anbox_session_manager_(std::make_unique<arc::AnboxSessionManager>()),
  anbox_service_manager_(std::make_unique<arc::AnboxServiceManager>()) {      

  g_anbox_service_launcher = this;
}

AnboxServiceLauncher::~AnboxServiceLauncher(){
  // new AnboxServiceLauncher(scheduler_configuration_manager_);
}

// static
AnboxServiceLauncher* AnboxServiceLauncher::Get() {  
  return g_anbox_service_launcher;
}

void AnboxServiceLauncher::OnPrimaryUserProfilePrepared(Profile* profile){  
  // scheduler_configuration_manager_;
  LOG(INFO) << "===== AnboxServiceLauncher::OnPrimaryUserProfilePrepared";

  apps::AnboxAppsFactory::GetForProfile(profile);  
  // arc::ArcInstanceThrottle::GetForBrowserContext(profile);
  anbox_session_manager_->Initialize();   
}

}