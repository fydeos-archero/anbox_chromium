#include "chrome/browser/chromeos/arc/session/anbox_session_manager.h"

#include "components/arc/session/anbox_session.h"

namespace arc{

AnboxSessionManager* g_anbox_session_manager = nullptr;

AnboxSessionManager::AnboxSessionManager():  
  anbox_session_(std::make_shared<arc::AnboxSession>()){    

  g_anbox_session_manager = this;
}

void AnboxSessionManager::Initialize(Profile* profile){
  anbox_session_->Initialize(profile);    
}

AnboxSessionManager* AnboxSessionManager::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_anbox_session_manager;
}

}