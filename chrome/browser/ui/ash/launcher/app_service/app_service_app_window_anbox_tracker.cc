// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_anbox_tracker.h"

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
// #include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_info.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget.h"

AppServiceAppWindowAnboxTracker::AppServiceAppWindowAnboxTracker(
    AppServiceAppWindowLauncherController* app_service_controller)
    : observed_profile_(app_service_controller->owner()->profile()),
      app_service_controller_(app_service_controller) {
  DCHECK(observed_profile_);
  DCHECK(app_service_controller_);

  auto const prefs = AnboxAppListPrefs::Get(observed_profile_);
  DCHECK(prefs);
  prefs->AddObserver(this);
}

AppServiceAppWindowAnboxTracker::~AppServiceAppWindowAnboxTracker() {
  auto const prefs = AnboxAppListPrefs::Get(observed_profile_);
  DCHECK(prefs);
  prefs->RemoveObserver(this);
}

void AppServiceAppWindowAnboxTracker::ActiveUserChanged(
    const std::string& user_email) {
  const std::string& primary_user_email = user_manager::UserManager::Get()
                                              ->GetPrimaryUser()
                                              ->GetAccountId()
                                              .GetUserEmail();
  if (user_email == primary_user_email) {
    // Make sure that we created items for all apps, not only which have a
    // window.
    for (const auto& info : task_id_to_arc_app_window_info_)
      AttachControllerToTask(info.first);

    // Update active status.
    OnTaskSetActive(active_task_id_);
  } else {
    // Some controllers might have no windows attached, for example background
    // task when foreground tasks is in full screen.
    for (const auto& it : app_shelf_group_to_controller_map_)
      app_service_controller_->owner()->CloseLauncherItem(
          it.second->shelf_id());
    app_shelf_group_to_controller_map_.clear();
  }
}

void AppServiceAppWindowAnboxTracker::OnWindowVisibilityChanged(
    aura::Window* window) {
  LOG(INFO) << "=== AppServiceAppWindowAnboxTracker::OnWindowVisibilityChanged";

  const int task_id = arc::GetWindowTaskId(window);
  if (task_id == arc::kNoTaskId || task_id == arc::kSystemWindowTaskId)
    return;

  // Attach window to multi-user manager now to let it manage visibility state
  // of the ARC window correctly.
  MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
      window,
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
}

void AppServiceAppWindowAnboxTracker::OnWindowDestroying(aura::Window* window) {
  LOG(INFO) << "=== AppServiceAppWindowAnboxTracker::OnWindowDestroying";

  app_service_controller_->UnregisterWindow(window);
}

void AppServiceAppWindowAnboxTracker::OnAppStatesChanged(
    const std::string& app_id,
    const AnboxAppListPrefs::AppInfo& app_info) {
  LOG(INFO) << "==== AppServiceAppWindowAnboxTracker::OnAppStatesChanged";

  if (!app_info.ready)
    OnAppRemoved(app_id);
}

void AppServiceAppWindowAnboxTracker::OnAppRemoved(const std::string& app_id) {
  LOG(INFO) << "=== AppServiceAppWindowAnboxTracker::OnAppRemoved";

  const std::vector<int> task_ids_to_remove = GetTaskIdsForApp(app_id);
  for (const auto task_id : task_ids_to_remove)
    OnTaskDestroyed(task_id);
  DCHECK(GetTaskIdsForApp(app_id).empty());
}

void AppServiceAppWindowAnboxTracker::OnTaskCreated(
    int task_id,
    const std::string& package_name,
    const std::string& activity_name,
    const std::string& intent) {

  LOG(INFO) << "==== AppServiceAppWindowAnboxTracker::OnTaskCreated " << task_id;

  DCHECK(task_id_to_arc_app_window_info_.find(task_id) ==
         task_id_to_arc_app_window_info_.end());

  const std::string arc_app_id =
      ArcAppListPrefs::GetAppId(package_name, activity_name);
  const arc::ArcAppShelfId arc_app_shelf_id =
      arc::ArcAppShelfId::FromIntentAndAppId(intent, arc_app_id);
  task_id_to_arc_app_window_info_[task_id] = std::make_unique<ArcAppWindowInfo>(
      arc_app_shelf_id, intent, package_name);

  LOG(INFO) << "==== AppServiceAppWindowAnboxTracker::OnTaskCreated app_id " << arc_app_id;    

  CheckAndAttachControllers();

  // Some tasks can be started in background and might have no window until
  // pushed to the front. We need its representation on the shelf to give a user
  // control over it.
  AttachControllerToTask(task_id);

  aura::Window* const window =
      task_id_to_arc_app_window_info_[task_id]->window();
  if (!window)
    return;

  // If we found the window, update AppService InstanceRegistry to add the
  // window information.
  // Update |state|. The app must be started, and running state. If visible,
  // set it as |kVisible|, otherwise, clear the visible bit.
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(observed_profile_);
  apps::InstanceState state = proxy->InstanceRegistry().GetState(window);
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  app_service_controller_->app_service_instance_helper()->OnInstances(
      task_id_to_arc_app_window_info_[task_id]->app_shelf_id().app_id(), window,
      std::string(), state);
  arc_window_candidates_.erase(window);
}

void AppServiceAppWindowAnboxTracker::OnTaskDescriptionUpdated(
    int32_t task_id,
    const std::string& label,
    const std::vector<uint8_t>& icon_png_data) {
  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return;

  ArcAppWindowInfo* const info = it->second.get();
  DCHECK(info);
  info->SetDescription(label, icon_png_data);
  AppWindowBase* app_window =
      app_service_controller_->GetAppWindow(it->second->window());
  if (app_window)
    app_window->SetDescription(label, icon_png_data);
}

void AppServiceAppWindowAnboxTracker::OnTaskDestroyed(int task_id) {
  LOG(INFO) << "=== AppServiceAppWindowAnboxTracker::OnTaskDestroyed";

  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return;

  aura::Window* const window = it->second.get()->window();
  if (window) {
    // For ARC apps, window may be recreated in some cases, and OnTaskSetActive
    // could be called after the window is destroyed, so controller is not
    // closed on window destroying. Controller will be closed onTaskDestroyed
    // event which is generated when the actual task is destroyed. So when the
    // task is destroyed, delete the instance, otherwise, we might have an
    // instance though the window has been closed, and the task has been
    // destroyed.
    app_service_controller_->app_service_instance_helper()->OnInstances(
        it->second.get()->app_shelf_id().app_id(), window, std::string(),
        apps::InstanceState::kDestroyed);
    app_service_controller_->UnregisterWindow(window);
  }

  // Check if we may close controller now, at this point we can safely remove
  // controllers without window.
  const auto app_shelf_id = it->second->app_shelf_id();
  auto it_controller =
      app_shelf_group_to_controller_map_.find(app_shelf_id);
  if (it_controller != app_shelf_group_to_controller_map_.end()) {
    it_controller->second->RemoveTaskId(task_id);
    if (!it_controller->second->HasAnyTasks()) {
      app_service_controller_->owner()->CloseLauncherItem(
          it_controller->second->shelf_id());
      app_shelf_group_to_controller_map_.erase(app_shelf_id);
    }
  }
  task_id_to_arc_app_window_info_.erase(task_id);
}

void AppServiceAppWindowAnboxTracker::OnTaskSetActive(int32_t task_id) {
  LOG(INFO) << "====== AppServiceAppWindowAnboxTracker::OnTaskSetActive-0 " << task_id;
  
  if (observed_profile_ != app_service_controller_->owner()->profile()) {
    active_task_id_ = task_id;
    return;
  }

  if (task_id == active_task_id_)
    return;

  LOG(INFO) << "====== AppServiceAppWindowAnboxTracker::OnTaskSetActive-1 " << task_id;

  auto it = task_id_to_arc_app_window_info_.find(active_task_id_);
  if (it != task_id_to_arc_app_window_info_.end()) {
    ArcAppWindowInfo* const previous_arc_app_window_info = it->second.get();
    DCHECK(previous_arc_app_window_info);
    app_service_controller_->owner()->SetItemStatus(
        previous_arc_app_window_info->shelf_id(), ash::STATUS_RUNNING);
    AppWindowBase* previous_app_window = app_service_controller_->GetAppWindow(
        previous_arc_app_window_info->window());
    if (previous_app_window) {
      previous_app_window->SetFullscreenMode(
          previous_app_window->widget() &&
                  previous_app_window->widget()->IsFullscreen()
              ? ArcAppWindow::FullScreenMode::kActive
              : ArcAppWindow::FullScreenMode::kNonActive);
    }
    if (previous_arc_app_window_info->window()) {
      apps::InstanceState state =
          app_service_controller_->app_service_instance_helper()
              ->CalculateActivatedState(previous_arc_app_window_info->window(),
                                        false /* active */);
      app_service_controller_->app_service_instance_helper()->OnInstances(
          previous_arc_app_window_info->app_shelf_id().app_id(),
          previous_arc_app_window_info->window(), std::string(), state);
    }
  }

  active_task_id_ = task_id;
  it = task_id_to_arc_app_window_info_.find(active_task_id_);
  if (it == task_id_to_arc_app_window_info_.end())
    return;
  ArcAppWindowInfo* const current_arc_app_window_info = it->second.get();
  if (!current_arc_app_window_info || !current_arc_app_window_info->window())
    return;
  aura::Window* const window = current_arc_app_window_info->window();
  views::Widget* const widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  if (widget && widget->IsActive()) {
    auto* controller = app_service_controller_->ControllerForWindow(window);
    if (controller)
      controller->SetActiveWindow(window);
  }

  LOG(INFO) << "====== AppServiceAppWindowAnboxTracker::OnTaskSetActive-2 " << task_id;

  app_service_controller_->owner()->SetItemStatus(
      current_arc_app_window_info->shelf_id(), ash::STATUS_RUNNING);

  apps::InstanceState state =
      app_service_controller_->app_service_instance_helper()
          ->CalculateActivatedState(window, true /* active */);
  app_service_controller_->app_service_instance_helper()->OnInstances(
      current_arc_app_window_info->app_shelf_id().app_id(), window,
      std::string(), state);
}

void AppServiceAppWindowAnboxTracker::AttachControllerToWindow(
    aura::Window* window) {
  LOG(INFO) << "==== AppServiceAppWindowAnboxTracker::AttachControllerToWindow " << window;    
  const int task_id = arc::GetWindowTaskId(window);
  if (task_id == arc::kNoTaskId)
    return;

  LOG(INFO) << "==== AppServiceAppWindowAnboxTracker::AttachControllerToWindow-1 " << task_id;

  // System windows are also arc apps.
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::ANBOX_APP));

  if (task_id == arc::kSystemWindowTaskId)
    return;

  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return;

  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);

  ArcAppWindowInfo* const info = it->second.get();
  DCHECK(info);

  // Check if we have set the AppWindowBase for this task.
  if (app_service_controller_->GetAppWindow(window))
    return;

  LOG(INFO) << "==== AppServiceAppWindowAnboxTracker::AttachControllerToWindow-3 " << task_id;  

  views::Widget* const widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  info->set_window(window);
  const ash::ShelfID shelf_id = info->shelf_id();
  AttachControllerToTask(task_id);
  app_service_controller_->AddWindowToShelf(window, shelf_id);
  AppWindowBase* app_window = app_service_controller_->GetAppWindow(window);
  if (app_window)
    app_window->SetDescription(info->title(), info->icon_data_png());

  window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
  window->SetProperty(ash::kArcPackageNameKey,
                      new std::string(info->package_name()));
  window->SetProperty(ash::kAppIDKey, new std::string(shelf_id.app_id));  
}

void AppServiceAppWindowAnboxTracker::AddCandidateWindow(aura::Window* window) {
  LOG(INFO) << "=== AppServiceAppWindowAnboxTracker::AddCandidateWindow";
  arc_window_candidates_.insert(window);
}

void AppServiceAppWindowAnboxTracker::RemoveCandidateWindow(
    aura::Window* window) {
  arc_window_candidates_.erase(window);
}

void AppServiceAppWindowAnboxTracker::OnItemDelegateDiscarded(
    const ash::ShelfID& shelf_id,
    ash::ShelfItemDelegate* delegate) {
  arc::ArcAppShelfId app_shelf_id =
      arc::ArcAppShelfId::FromString(shelf_id.app_id);
  auto it = app_shelf_group_to_controller_map_.find(app_shelf_id);
  if (it != app_shelf_group_to_controller_map_.end() &&
      static_cast<ash::ShelfItemDelegate*>(it->second) == delegate) {
    app_shelf_group_to_controller_map_.erase(it);
  }
}

ash::ShelfID AppServiceAppWindowAnboxTracker::GetShelfId(int task_id) const {
  LOG(INFO) << "=== AppServiceAppWindowAnboxTracker::GetShelfId " << task_id;
  if (observed_profile_ != app_service_controller_->owner()->profile())
    return ash::ShelfID();

  const auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return ash::ShelfID();

  return it->second->shelf_id();
}

void AppServiceAppWindowAnboxTracker::CheckAndAttachControllers() {
  for (auto* window : arc_window_candidates_)
    AttachControllerToWindow(window);
}

void AppServiceAppWindowAnboxTracker::AttachControllerToTask(int task_id) {  
  LOG(INFO) << "==== AppServiceAppWindowAnboxTracker::AttachControllerToTask-0 " << task_id;    

  ArcAppWindowInfo* const app_window_info =
      task_id_to_arc_app_window_info_[task_id].get();  
  const arc::ArcAppShelfId& app_shelf_id = app_window_info->app_shelf_id();
  if (base::Contains(app_shelf_group_to_controller_map_, app_shelf_id)) {
    LOG(INFO) << "==== AppServiceAppWindowAnboxTracker::AttachControllerToTask-0.1 " << task_id;    
    app_shelf_group_to_controller_map_[app_shelf_id]->AddTaskId(task_id);
    return;
  }

  LOG(INFO) << "==== AppServiceAppWindowAnboxTracker::AttachControllerToTask-1 " << task_id;

  const ash::ShelfID shelf_id(app_shelf_id.ToString());
  std::unique_ptr<AppServiceAppWindowLauncherItemController> controller =
      std::make_unique<AppServiceAppWindowLauncherItemController>(
          shelf_id, app_service_controller_);
  AppServiceAppWindowLauncherItemController* item_controller = controller.get();

  if (!app_service_controller_->owner()->GetItem(shelf_id)) {
    LOG(INFO) << "==== AppServiceAppWindowAnboxTracker::AttachControllerToTask-2 " << task_id;

    app_service_controller_->owner()->CreateAppLauncherItem(
        std::move(controller), ash::STATUS_RUNNING);
  } else {
    LOG(INFO) << "==== AppServiceAppWindowAnboxTracker::AttachControllerToTask-3 " << task_id;

    app_service_controller_->owner()->shelf_model()->SetShelfItemDelegate(
        shelf_id, std::move(controller));
    app_service_controller_->owner()->SetItemStatus(shelf_id,
                                                    ash::STATUS_RUNNING);
  }

  LOG(INFO) << "==== AppServiceAppWindowAnboxTracker::AttachControllerToTask-4 " << task_id;

  item_controller->AddTaskId(task_id);
  app_shelf_group_to_controller_map_[app_shelf_id] = item_controller;
}

// void AppServiceAppWindowAnboxTracker::OnArcOptInManagementCheckStarted() {
//   // In case of retry this time is updated and we measure only successful run.
//   opt_in_management_check_start_time_ = base::Time::Now();
// }

// void AppServiceAppWindowAnboxTracker::OnArcSessionStopped(
//     arc::ArcStopReason stop_reason) {
//   opt_in_management_check_start_time_ = base::Time();
// }

std::vector<int> AppServiceAppWindowAnboxTracker::GetTaskIdsForApp(
    const std::string& app_id) const {
  std::vector<int> task_ids;
  for (const auto& it : task_id_to_arc_app_window_info_) {
    const ArcAppWindowInfo* app_window_info = it.second.get();
    if (app_window_info->app_shelf_id().app_id() == app_id)
      task_ids.push_back(it.first);
  }

  return task_ids;
}
