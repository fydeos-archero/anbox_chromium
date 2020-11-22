#ifndef CHROME_BROWSER_CHROMEOS_ARC_SESSION_ANBOX_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_SESSION_ANBOX_SERVICE_LAUNCHER_H_

#include <memory>

#include "ash/public/cpp/default_scale_factor_retriever.h"
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

#include "chrome/browser/chromeos/arc/session/anbox_session_manager.h"

class Profile;

namespace chromeos {
class SchedulerConfigurationManagerBase;
}

namespace arc{
class AnboxSessionManager; 
class AnboxServiceManager;
}

namespace anbox{

class AnboxServiceLauncher {
public:
  explicit AnboxServiceLauncher(chromeos::SchedulerConfigurationManagerBase*
                                  scheduler_configuration_manager);
  ~AnboxServiceLauncher();

public:
// Returns a global instance.
  static AnboxServiceLauncher* Get();

  void OnPrimaryUserProfilePrepared(Profile* profile);  

private:
  [[maybe_unused]] chromeos::SchedulerConfigurationManagerBase* const
      scheduler_configuration_manager_;

  std::unique_ptr<arc::AnboxSessionManager> anbox_session_manager_;      
  std::unique_ptr<arc::AnboxServiceManager> anbox_service_manager_;

private:
  DISALLOW_COPY_AND_ASSIGN(AnboxServiceLauncher);
};

}
#endif