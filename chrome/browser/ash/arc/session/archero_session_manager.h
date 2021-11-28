#ifndef CHROME_BROWSER_CHROMEOS_ARC_SESSION_ARCHERO_SESSION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_SESSION_ARCHERO_SESSION_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/timer/timer.h"

#include "content/public/browser/browser_thread.h"

class Profile;

namespace arc{
class ArcHeroSession;

class ArcHeroSessionManager{
public:
  ArcHeroSessionManager();

public:
  void Initialize(Profile* profile);

  static ArcHeroSessionManager* Get();

  std::shared_ptr<ArcHeroSession>& GetArcHeroSession(){
    return archero_session_;
  }

private:
  std::shared_ptr<ArcHeroSession> archero_session_;

private:
  DISALLOW_COPY_AND_ASSIGN(ArcHeroSessionManager);
};


}
#endif