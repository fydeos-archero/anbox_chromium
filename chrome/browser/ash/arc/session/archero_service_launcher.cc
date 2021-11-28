#include "chrome/browser/ash/arc/session/archero_service_launcher.h"

#include <utility>

#include "ash/public/cpp/default_scale_factor_retriever.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/session/archero_session.h"
#include "chrome/browser/ash/arc/session/archero_session_manager.h"
#include "chrome/browser/apps/app_service/publishers/archero_apps_factory.h"
#include "components/arc/archero_service_manager.h"

// ChromeBrowserMainPartsChromeos owns.
archero::ArcHeroServiceLauncher* g_archero_service_launcher = nullptr;

namespace archero{
ArcHeroServiceLauncher::ArcHeroServiceLauncher(chromeos::SchedulerConfigurationManagerBase* scheduler_configuration_manager):
  scheduler_configuration_manager_(scheduler_configuration_manager),
  archero_session_manager_(std::make_unique<arc::ArcHeroSessionManager>()),
  archero_service_manager_(std::make_unique<arc::ArcHeroServiceManager>()) {

  g_archero_service_launcher = this;
}

ArcHeroServiceLauncher::~ArcHeroServiceLauncher(){
  // new ArcHeroServiceLauncher(scheduler_configuration_manager_);
}

// static
ArcHeroServiceLauncher* ArcHeroServiceLauncher::Get() {
  return g_archero_service_launcher;
}

void ArcHeroServiceLauncher::OnPrimaryUserProfilePrepared(Profile* profile){
  // scheduler_configuration_manager_;
  LOG(INFO) << "===== ArcHeroServiceLauncher::OnPrimaryUserProfilePrepared";

  apps::ArcHeroAppsFactory::GetForProfile(profile);
  // arc::ArcInstanceThrottle::GetForBrowserContext(profile);

  archero_session_manager_->Initialize(profile);
}

}