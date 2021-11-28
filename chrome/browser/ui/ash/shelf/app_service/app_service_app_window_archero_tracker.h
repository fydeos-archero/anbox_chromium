#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_ARCHERO_TRACKER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_ARCHERO_TRACKER_H_

#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_arc_tracker.h"
#include "chrome/browser/ui/app_list/arc/archero_app_list_prefs.h"

class AppServiceAppWindowShelfController;
class AppServiceAppWindowShelfItemController;
class ArcAppWindowInfo;
class Profile;

class AppServiceAppWindowArcHeroTracker:
  public ArcHeroAppListPrefs::Observer{
public:
  explicit AppServiceAppWindowArcHeroTracker(AppServiceAppWindowShelfController* app_service_controller);
  ~AppServiceAppWindowArcHeroTracker() override;

  void OnTaskCreated(int32_t task_id,
                               const std::string& package_name,
                               const std::string& activity,
                               const std::string& intent,
                               int32_t session_id) override;
  void OnTaskDestroyed(int32_t task_id) override;
  void OnTaskSetActive(int32_t task_id) override;

  int GetTaskIdSharingLogicalWindow(int task_id);

  void CheckAndAttachControllers() {
    for (auto* window : arc_window_candidates_)
      AttachControllerToWindow(window);
  }

  void AttachControllerToWindow(aura::Window* window);
  void AttachControllerToTask(int task_id);
  void ActiveUserChanged(const std::string& user_email);

  void AddCandidateWindow(aura::Window* window) {
    arc_window_candidates_.insert(window);
  }

  void HandleWindowVisibilityChanged(aura::Window* window);

  void RemoveCandidateWindow(
      aura::Window* window) {
    arc_window_candidates_.erase(window);
  }

  void HandleWindowDestroying(aura::Window* window);

  ash::ShelfID GetShelfId(int task_id) const;
  void OnItemDelegateDiscarded(
    const ash::ShelfID& shelf_id,
    ash::ShelfItemDelegate* delegate);

  int active_task_id() const { return active_task_id_; }


private:
  using TaskIdToArcAppWindowInfo =
      std::map<int, std::unique_ptr<ArcAppWindowInfo>>;

  // Maps shelf group id to controller. Shelf group id is optional parameter for
  // the Android task. If it is not set, app id is used instead.
  using ShelfGroupToAppControllerMap =
      std::map<arc::ArcAppShelfId, AppServiceAppWindowShelfItemController*>;

  Profile* const observed_profile_;
  int active_task_id_ = arc::kNoTaskId;
  AppServiceAppWindowShelfController* const app_service_controller_;
  std::set<aura::Window*> arc_window_candidates_;

  TaskIdToArcAppWindowInfo task_id_to_arc_app_window_info_;
  ShelfGroupToAppControllerMap app_shelf_group_to_controller_map_;

  base::WeakPtrFactory<AppServiceAppWindowArcHeroTracker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_ARC_TRACKER_H_