#include "chrome/browser/ui/ash/launcher/anbox_app_window.h"
#include "components/arc/session/anbox_session.h"
#include "chrome/browser/chromeos/arc/session/anbox_session_manager.h"

AnboxAppWindow::AnboxAppWindow(int task_id,
               const arc::ArcAppShelfId& app_shelf_id,
               views::Widget* widget,
               ArcAppWindowDelegate* owner,
               Profile* profile):ArcAppWindow(
                 task_id, app_shelf_id, widget, owner, profile
               ){
  LOG(INFO) << "=== AnboxAppWindow::AnboxAppWindow";               
}

AnboxAppWindow::~AnboxAppWindow(){

}

void AnboxAppWindow::Close(){    
  LOG(INFO) << "===== AnboxAppWindow::Close";  

  auto anboxSession = arc::AnboxSessionManager::Get()->GetAnboxSession();  

  anboxSession->Close(task_id());
}