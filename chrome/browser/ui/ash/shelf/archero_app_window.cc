#include "chrome/browser/ui/ash/shelf/archero_app_window.h"
#include "components/arc/session/archero_session.h"
#include "chrome/browser/ash/arc/session/archero_session_manager.h"

ArcHeroAppWindow::ArcHeroAppWindow(int task_id,
               const arc::ArcAppShelfId& app_shelf_id,
               views::Widget* widget,
               ArcAppWindowDelegate* owner,
               Profile* profile):ArcAppWindow(
                 task_id, app_shelf_id, widget, owner, profile
               ){
  LOG(INFO) << "=== ArcHeroAppWindow::ArcHeroAppWindow";
}

ArcHeroAppWindow::~ArcHeroAppWindow(){

}

void ArcHeroAppWindow::Close(){
  LOG(INFO) << "===== ArcHeroAppWindow::Close";

  auto archeroSession = arc::ArcHeroSessionManager::Get()->GetArcHeroSession();

  archeroSession->Close(task_id());
}