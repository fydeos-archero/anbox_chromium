#ifndef CHROME_BROWSER_CHROMEOS_ARC_SESSION_ARCHERO_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_SESSION_ARCHERO_SERVICE_LAUNCHER_H_

#include <memory>

#include "ash/public/cpp/default_scale_factor_retriever.h"
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

#include "chrome/browser/ash/arc/session/archero_session_manager.h"

class Profile;

namespace chromeos {
class SchedulerConfigurationManagerBase;
}

namespace arc{
class ArcHeroSessionManager;
class ArcHeroServiceManager;
}

namespace archero{

class ArcHeroServiceLauncher {
public:
  explicit ArcHeroServiceLauncher(chromeos::SchedulerConfigurationManagerBase*
                                  scheduler_configuration_manager);
  ~ArcHeroServiceLauncher();

public:
// Returns a global instance.
  static ArcHeroServiceLauncher* Get();

  void OnPrimaryUserProfilePrepared(Profile* profile);

private:
  [[maybe_unused]] chromeos::SchedulerConfigurationManagerBase* const
      scheduler_configuration_manager_;

  std::unique_ptr<arc::ArcHeroSessionManager> archero_session_manager_;
  std::unique_ptr<arc::ArcHeroServiceManager> archero_service_manager_;

private:
  DISALLOW_COPY_AND_ASSIGN(ArcHeroServiceLauncher);
};

}
#endif