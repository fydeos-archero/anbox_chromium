#ifndef CHROME_BROWSER_CHROMEOS_ARC_SESSION_ANBOX_SESSION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_SESSION_ANBOX_SESSION_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/timer/timer.h"

#include "content/public/browser/browser_thread.h"

namespace arc{
class AnboxSession;

class AnboxSessionManager{
public:
  AnboxSessionManager();

public:
  void Initialize();  

  static AnboxSessionManager* Get();  

  std::shared_ptr<AnboxSession>& GetAnboxSession(){
    return anbox_session_;
  }
  
private:  
  std::shared_ptr<AnboxSession> anbox_session_;

private:
  DISALLOW_COPY_AND_ASSIGN(AnboxSessionManager);
};


}
#endif