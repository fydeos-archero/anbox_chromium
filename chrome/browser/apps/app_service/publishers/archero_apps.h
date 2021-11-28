#ifndef CHROME_BROWSER_APPS_APP_SERVICE_ARCHERO_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_ARCHERO_APPS_H_

#include "chrome/browser/apps/app_service/publishers/arc_apps.h"
#include "chrome/browser/ui/app_list/arc/archero_app_list_prefs.h"

namespace apps {

class ArcHeroApps:
  public KeyedService,
  public apps::PublisherBase,
  public ArcAppListPrefs::Observer{
public:
  // using ArcHeroAppListPrefs::Observer::OnAppRegistered;
  // using ArcHeroApps::LoadIcon;

public:
  // explicit ArcHeroApps(Profile* profile):ArcApps(nullptr), ArcHeroApps(profile, nullptr){
  explicit ArcHeroApps(Profile* profile):ArcHeroApps(profile, nullptr){
  }

  explicit ArcHeroApps(Profile* profile, apps::AppServiceProxyChromeOs* proxy);
  void Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              apps::mojom::WindowInfoPtr window_info) override;
  void OnAppIconUpdated(const std::string& app_id,
                        const ArcAppIconDescriptor& descriptor) override {
    LOG(INFO) << "=== ArcHeroApps::OnAppIconUpdated appId: " << app_id << " " << descriptor.GetName();

    SetIconEffect(app_id);
  }
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent,
                     int32_t session_id) override{

    const std::string app_id = ArcHeroAppListPrefs::GetAppId(package_name, activity);
    app_id_to_task_ids_[app_id].insert(task_id);
    task_id_to_app_id_[task_id] = app_id;
  }
  void OnTaskDestroyed(int32_t task_id) override {
    auto it = task_id_to_app_id_.find(task_id);
    if (it == task_id_to_app_id_.end()) {
      return;
    }

    const std::string app_id = it->second;
    task_id_to_app_id_.erase(it);
    DCHECK(base::Contains(app_id_to_task_ids_, app_id));
    app_id_to_task_ids_[app_id].erase(task_id);
    if (app_id_to_task_ids_[app_id].empty()) {
      app_id_to_task_ids_.erase(app_id);
    }
  }

  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;

  IconEffects GetIconEffects(const std::string& app_id,
                             const ArcAppListPrefs::AppInfo& app_info);

  void SetIconEffect(const std::string& app_id);

  void LoadIconFromVM(
          const std::string& app_id,
          apps::mojom::IconType icon_type,
          int32_t size_hint_in_dip,
          ui::ScaleFactor scale_factor,
          apps::IconEffects icon_effects,
          int fallback_icon_resource_id,
          apps::mojom::Publisher::LoadIconCallback callback){}

public:
  void OnAppRegistered(const std::string& app_id,
                                 const ArcAppListPrefs::AppInfo& app_info) override;
private:
  apps::mojom::AppPtr Convert(ArcHeroAppListPrefs* prefs,
                              const std::string& app_id,
                              const ArcHeroAppListPrefs::AppInfo& app_info,
                              bool update_icon = true);



private:
  using AppIdToTaskIds = std::map<std::string, std::set<int>>;
  using TaskIdToAppId = std::map<int, std::string>;

  Profile* const profile_;
  apps_util::IncrementingIconKeyFactory icon_key_factory_;
  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  AppIdToTaskIds app_id_to_task_ids_;
  TaskIdToAppId task_id_to_app_id_;

  // mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;
  base::WeakPtrFactory<ArcHeroApps> weak_ptr_factory_{this};
};

}

#endif