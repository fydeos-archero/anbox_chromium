#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ANBOX_APP_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ANBOX_APP_CONTEXT_MENU_H_

#include "chrome/browser/ui/app_list/arc/arc_app_context_menu.h"

class AnboxAppContextMenu:
  public ArcAppContextMenu{
public:
  AnboxAppContextMenu(app_list::AppContextMenuDelegate* delegate,
                    Profile* profile,
                    const std::string& app_id,
                    AppListControllerDelegate* controller);
  ~AnboxAppContextMenu() override;

  void ExecuteCommand(int command_id, int event_flags) override;
};

#endif