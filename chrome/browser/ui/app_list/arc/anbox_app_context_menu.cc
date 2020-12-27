#include "chrome/browser/ui/app_list/arc/anbox_app_context_menu.h"

AnboxAppContextMenu::AnboxAppContextMenu(app_list::AppContextMenuDelegate* delegate,
                    Profile* profile,
                    const std::string& app_id,
                    AppListControllerDelegate* controller):
                    ArcAppContextMenu(delegate, profile, app_id, controller){
  LOG(INFO) << "== AnboxAppContextMenu::AnboxAppContextMenu";
}

AnboxAppContextMenu::~AnboxAppContextMenu(){}

void AnboxAppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  // if (command_id == ash::LAUNCH_NEW) {
  //   delegate()->ExecuteLaunchCommand(event_flags);
  // } else if (command_id == ash::UNINSTALL) {
  //   apps::AppServiceProxy* proxy =
  //       apps::AppServiceProxyFactory::GetForProfile(profile());
  //   DCHECK(proxy);
  //   proxy->Uninstall(app_id(),
  //                    controller() ? controller()->GetAppListWindow() : nullptr);
  // } else if (command_id == ash::SHOW_APP_INFO) {
  //   ShowPackageInfo();
  // } else if (command_id >= ash::LAUNCH_APP_SHORTCUT_FIRST &&
  //            command_id <= ash::LAUNCH_APP_SHORTCUT_LAST) {
  //   DCHECK(app_shortcuts_menu_builder_);
  //   app_shortcuts_menu_builder_->ExecuteCommand(command_id);
  // } else {
  //   app_list::AppContextMenu::ExecuteCommand(command_id, event_flags);
  // }

  LOG(INFO) << "==== AnboxAppContextMenu::ExecuteCommand " << command_id;
}