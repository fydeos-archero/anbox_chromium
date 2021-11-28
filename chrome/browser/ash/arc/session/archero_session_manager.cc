#include "chrome/browser/ash/arc/session/archero_session_manager.h"

#include "components/arc/session/archero_session.h"

namespace arc{

ArcHeroSessionManager* g_archero_session_manager = nullptr;

ArcHeroSessionManager::ArcHeroSessionManager():
  archero_session_(std::make_shared<arc::ArcHeroSession>()){

  g_archero_session_manager = this;
}

void ArcHeroSessionManager::Initialize(Profile* profile){
  archero_session_->Initialize(profile);
}

ArcHeroSessionManager* ArcHeroSessionManager::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_archero_session_manager;
}

}