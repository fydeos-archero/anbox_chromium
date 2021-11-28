#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARCHERO_APP_LIST_PREFS_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARCHERO_APP_LIST_PREFS_H_

#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/archero_app_list_prefs_factory.h"

#include "archero_bridge.h"

class ArcHeroAppListPrefs:
  public KeyedService,
  // public arc::mojom::AppHost,
  public archero::ArcHeroBridge::Observer {
// public:
  //  using ArcAppListPrefs::OnTaskCreated;
//    using ArcAppListPrefs::default_apps_;

public:
  using AppInfo = ArcAppListPrefs::AppInfo;
  using Observer = ArcAppListPrefs::Observer;
  using PackageInfo = ArcAppListPrefs::PackageInfo;

  static constexpr auto GetAppId = ArcAppListPrefs::GetAppId;

public:
  ArcHeroAppListPrefs(Profile* profile,
      arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
          app_connection_holder_for_testing);
  ~ArcHeroAppListPrefs();

  static ArcHeroAppListPrefs* Get(content::BrowserContext* context){
    return ArcHeroAppListPrefsFactory::GetInstance()->GetForBrowserContext(context);
  }

  static ArcHeroAppListPrefs* Create(Profile* profile) {
    return new ArcHeroAppListPrefs(profile, nullptr);
  }

  void AddObserver(Observer* observer) {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    observer_list_.RemoveObserver(observer);
  }

  bool HasObserver(Observer* observer) {
    return observer_list_.HasObserver(observer);
  }

  void OnTaskCreated(int32_t task_id,
                      const std::string& package_name,
                      const std::string& activity,
                      const absl::optional<std::string>& name,
                      const absl::optional<std::string>& intent,
                      int32_t session_id) {
    HandleTaskCreated(name, package_name, activity);
    for (auto& observer : observer_list_) {
      observer.OnTaskCreated(task_id, package_name, activity,
                            intent.value_or(std::string()), session_id);
    }
  }

  void OnTaskDestroyed(int32_t task_id) {
    for (auto& observer : observer_list_)
      observer.OnTaskDestroyed(task_id);
  }

  void HandleTaskCreated(const absl::optional<std::string>& name,
                                        const std::string& package_name,
                                        const std::string& activity) {

    LOG(INFO) << "=== ArcHeroAppListPrefs::HandleTaskCreated-0 " << tracked_apps_.size();

    // DCHECK(IsArcAndroidEnabledForProfile(profile_));
    const std::string app_id = GetAppId(package_name, activity);
    if (IsRegistered(app_id)) {
      LOG(INFO) << "=== ArcHeroAppListPrefs::HandleTaskCreated-1 " << package_name;
      // SetLastLaunchTimeInternal(app_id);
    } else {
      LOG(INFO) << "=== ArcHeroAppListPrefs::HandleTaskCreated-2 " << package_name;
      // Create runtime app entry that is valid for the current user session. This
      // entry is not shown in App Launcher and only required for shelf
      // integration.
      AddAppAndShortcut(name.value_or(std::string()), package_name, activity,
                        std::string() /* intent_uri */,
                        std::string() /* icon_resource_id */, false /* sticky */,
                        false /* notifications_enabled */, true /* app_ready */,
                        false /* suspended */, false /* shortcut */,
                        false /* launchable */);
    }
  }

  bool IsRegistered(const std::string& app_id) const;

  void InvalidateAppIcons(const std::string& app_id);

  void MaybeRemoveIconRequestRecord(const std::string& app_id) {
    request_icon_recorded_.erase(app_id);
  }

  void ScheduleAppFolderDeletion(const std::string& app_id) {
    // DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // file_task_runner_->PostTask(
    //     FROM_HERE,
    //     base::BindOnce(&DeleteAppFolderFromFileThread, GetAppPath(app_id)));
  }

  void SetNotificationsEnabled(const std::string& app_id,
                                              bool enabled) {
    // if (!IsRegistered(app_id)) {
    //   VLOG(2) << "Request to set notifications enabled flag for non-registered "
    //           << "app:" << app_id << ".";
    //   return;
    // }

    // std::unique_ptr<AppInfo> app_info = GetApp(app_id);
    // if (!app_info) {
    //   VLOG(2) << "Failed to get app info: " << app_id << ".";
    //   return;
    // }

    // // In case app is not ready, defer this request.
    // if (!ready_apps_.count(app_id)) {
    //   NotificationsEnabledDeferred(prefs_).Put(app_id, enabled);
    //   for (auto& observer : observer_list_)
    //     observer.OnNotificationsEnabledChanged(app_info->package_name, enabled);
    //   return;
    // }

    // auto* app_instance = ARC_GET_INSTANCE_FOR_METHOD(app_connection_holder(),
    //                                                 SetNotificationsEnabled);
    // if (!app_instance)
    //   return;

    // NotificationsEnabledDeferred(prefs_).Remove(app_id);
    // app_instance->SetNotificationsEnabled(app_info->package_name, enabled);
  }

  std::unique_ptr<AppInfo> GetApp(
      const std::string& app_id) const {
    // Information for default app is available before ARC enabled.
    // if ((!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_)) &&
    //     !default_apps_->HasApp(app_id)) {
    //   return nullptr;
    // }

    return GetAppFromPrefs(app_id);
  }

  std::unique_ptr<ArcHeroAppListPrefs::AppInfo> GetAppFromPrefs(
    const std::string& app_id) const;
  std::unique_ptr<ArcHeroAppListPrefs::PackageInfo> GetPackage(
    const std::string& package_name) const;

  std::vector<std::string> GetAppIds() const {
    // if (arc::ShouldArcAlwaysStart())
    //   return GetAppIdsNoArcEnabledCheck();

    // if (!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_)) {
    //   // Default ARC apps available before OptIn.
    //   std::vector<std::string> ids;
    //   for (const auto& default_app : default_apps_->GetActiveApps()) {
    //     // Default apps are iteratively added to prefs. That generates
    //     // |OnAppRegistered| event per app. Consumer may use this event to request
    //     // list of all apps. Although this practice is discouraged due the
    //     // performance reason, let be safe and in order to prevent listing of not
    //     // yet registered apps, filter out default apps based of tracked state.
    //     if (tracked_apps_.count(default_app.first))
    //       ids.push_back(default_app.first);
    //   }
    //   return ids;
    // }
    return GetAppIdsNoArcEnabledCheck();
  }

  std::vector<std::string> GetAppIdsNoArcEnabledCheck() const;

  void OnIconInstalled(const std::string& app_id,
                                      const ArcAppIconDescriptor& descriptor,
                                      bool install_succeed) {
    LOG(INFO) << "=== ArcAppListPrefs::OnIconInstalled install_succeed: " << install_succeed;

    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!install_succeed)
      return;

    LOG(INFO) << "=== ArcAppListPrefs::OnIconInstalled appId: " << app_id;

    for (auto& observer : observer_list_)
      observer.OnAppIconUpdated(app_id, descriptor);
  }

  base::FilePath GetIconPath(
      const std::string& app_id,
      const ArcAppIconDescriptor& descriptor) {
    // TODO(khmel): Add DCHECK(GetApp(app_id));
    active_icons_[app_id].insert(descriptor);
    return GetAppPath(app_id).AppendASCII(descriptor.GetName());
  }

  base::FilePath GetAppPath(const std::string& app_id) const {
    return base_path_.AppendASCII(app_id);
  }

  std::string GetAppIdByPackageName(
    const std::string& package_name) const;

  base::Time GetInstallTime(const std::string& app_id) const;


  void onAppAdded(archero::protobuf::bridge::Application *pItem) override;
  void onAppRemoved(archero::protobuf::bridge::Application *pItem) override;
  void OnTaskCreated(int task_id, const std::string &name, const std::string &package) override;
  void OnTaskRemoved(int task_id) override;

  void AddAppAndShortcut(const std::string& name,
                         const std::string& package_name,
                         const std::string& activity,
                         const std::string& intent_uri,
                         const std::string& icon_resource_id,
                         const bool sticky,
                         const bool notifications_enabled,
                         const bool app_ready,
                         const bool suspended,
                         const bool shortcut,
                         const bool launchable);

  void RemoveApp(const std::string& app_id);
  std::string GetActivityByPackageName(const std::string& package_name) const;

  void RequestIcon(const std::string& app_id,
                                  const ArcAppIconDescriptor& descriptor) {
    LOG(INFO) << "=== ArcHeroAppListPrefs RequestIcon " << app_id;

    std::unique_ptr<AppInfo> app_info = GetApp(app_id);
    if (!app_info) {

      VLOG(2) << "Failed to get app info: " << app_id << ".";
      return;
    }

    SendIconRequest(app_id, *app_info, descriptor);
  }

  void SendIconRequest(const std::string& app_id,
                                      const AppInfo& app_info,
                                      const ArcAppIconDescriptor& descriptor);

  void MaybeRequestIcon(const std::string& app_id,
                        const ArcAppIconDescriptor& descriptor);

  arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>* app_connection_holder();
  void OnIcon(const std::string& app_id,
              const ArcAppIconDescriptor& descriptor,
              arc::mojom::RawIconPngDataPtr icon){
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnIcon ";

    if (!icon || !icon->icon_png_data.has_value() ||
        icon->icon_png_data->empty()) {
      LOG(WARNING) << "Cannot fetch icon for " << app_id;
      return;
    }

    if (!IsRegistered(app_id)) {
      VLOG(2) << "Request to update icon for non-registered app: " << app_id;
      return;
    }

    // InstallIcon(app_id, descriptor, std::move(icon));
    const base::FilePath icon_path = GetIconPath(app_id, descriptor);
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnIcon " << icon_path;
  }

#if 0
public:
  void OnAppAddedDeprecated(AppInfoPtr app) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnAppAddedDeprecated";
  }

  void OnAppListRefreshed(std::vector<AppInfoPtr> apps) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnAppListRefreshed";
  }

  void OnPackageAdded(ArcPackageInfoPtr arcPackageInfo) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnPackageAdded";
  }


  void OnPackageAppListRefreshed(const std::string& package_name, std::vector<AppInfoPtr> apps) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnPackageAppListRefreshed";
  }

  void OnPackageListRefreshed(std::vector<ArcPackageInfoPtr> packages) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnPackageListRefreshed";
  }

  void OnPackageModified(ArcPackageInfoPtr arcPackageInfo) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnPackageModified";
  }

  void OnPackageRemoved(const std::string& package_name) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnPackageRemoved";
  }

  void OnTaskCreated(int32_t task_id, const std::string& package_name, const std::string& activity, const absl::optional<std::string>& name, const absl::optional<std::string>& intent, int32_t session_id) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnTaskCreated";
  }

  void OnTaskDescriptionUpdated(int32_t task_id, const std::string& label, const std::vector<uint8_t>& icon_png_data) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnTaskDescriptionUpdated";
  }

  void OnTaskDescriptionChanged(int32_t task_id, const std::string& label, RawIconPngDataPtr icon, uint32_t primary_color, uint32_t status_bar_color) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnTaskDescriptionChanged";
  }

  void OnTaskDestroyed(int32_t task_id) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnTaskDestroyed";
  }

  void OnTaskSetActive(int32_t task_id) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnTaskSetActive";
  }

  void OnNotificationsEnabledChanged(const std::string& package_name, bool enabled) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnNotificationsEnabledChanged";
  }

  void OnInstallShortcut(ShortcutInfoPtr shortcut) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnInstallShortcut";
  }

  void OnInstallationStarted(const absl::optional<std::string>& package_name) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnInstallationStarted";
  }

  void OnInstallationFinished(InstallationResultPtr result) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnInstallationFinished";
  }

  void OnUninstallShortcut(const std::string& package_name, const std::string& intent_uri) override {
    LOG(INFO) << "=== ArcHeroAppListPrefs::OnUninstallShortcut";
  }
#endif

private:
  Profile* const profile_;

  // Owned by the BrowserContext.
  PrefService* const prefs_;

  base::FilePath base_path_;

  // std::unique_ptr<ArcDefaultAppList> default_apps_;
  base::ObserverList<Observer> observer_list_;
  std::unordered_set<std::string> ready_apps_;
  std::unordered_set<std::string> tracked_apps_;
  std::map<std::string, std::set<ArcAppIconDescriptor>> active_icons_;
  std::map<std::string, std::set<ArcAppIconDescriptor>> request_icon_recorded_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  base::WeakPtrFactory<ArcHeroAppListPrefs> weak_ptr_factory_{this};
};

#endif