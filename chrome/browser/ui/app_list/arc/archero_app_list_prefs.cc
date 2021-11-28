#include <stddef.h>

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ash_features.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_scoped_pref_update.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"
#include "chrome/browser/ui/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/ui/app_list/arc/arc_pai_starter.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/compat_mode/arc_resize_lock_manager.h"
#include "components/arc/mojom/compatibility_mode.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/connection_holder.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/gfx/codec/png_codec.h"


#include "components/arc/arc_util.h"
#include "components/arc/session/archero_session.h"
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/arc/session/archero_session_manager.h"
#include "chrome/browser/ui/app_list/arc/archero_app_list_prefs.h"

constexpr char kActivity[] = "activity";
// constexpr char kFrameworkPackageName[] = "android";
constexpr char kResizeLockState[] = "resize_lock_state";
constexpr char kResizeLockNeedsConfirmation[] =
    "resize_lock_needs_confirmation";
constexpr char kIconResourceId[] = "icon_resource_id";
constexpr char kIconVersion[] = "icon_version";
constexpr char kInstallTime[] = "install_time";
constexpr char kIntentUri[] = "intent_uri";
constexpr char kLastBackupAndroidId[] = "last_backup_android_id";
constexpr char kLastBackupTime[] = "last_backup_time";
constexpr char kLastLaunchTime[] = "lastlaunchtime";
constexpr char kLaunchable[] = "launchable";
constexpr char kName[] = "name";
constexpr char kNotificationsEnabled[] = "notifications_enabled";
constexpr char kPackageName[] = "package_name";
constexpr char kPackageVersion[] = "package_version";
// constexpr char kPinIndex[] = "pin_index";
constexpr char kPermissionStates[] = "permission_states";
constexpr char kSticky[] = "sticky";
constexpr char kShortcut[] = "shortcut";
constexpr char kShouldSync[] = "should_sync";
constexpr char kSuspended[] = "suspended";
constexpr char kSystem[] = "system";
constexpr char kUninstalled[] = "uninstalled";
constexpr char kVPNProvider[] = "vpnprovider";
constexpr char kPermissionStateGranted[] = "granted";
constexpr char kPermissionStateManaged[] = "managed";

int current_icons_version = 1;
constexpr int default_app_icon_dip_sizes[] = {16, 32, 48, 64};

bool GetInt64FromPref(const base::DictionaryValue* dict,
                      const std::string& key,
                      int64_t* value) {
  DCHECK(dict);
  std::string value_str;
  if (!dict->GetStringWithoutPathExpansion(key, &value_str)) {
    VLOG(2) << "Can't find key in local pref dictionary. Invalid key: " << key
            << ".";
    return false;
  }

  if (!base::StringToInt64(value_str, value)) {
    VLOG(2) << "Can't change string to int64_t. Invalid string value: "
            << value_str << ".";
    return false;
  }

  return true;
}

bool AreAppStatesChanged(const ArcAppListPrefs::AppInfo& info1,
                         const ArcAppListPrefs::AppInfo& info2) {
  return info1.sticky != info2.sticky ||
         info1.notifications_enabled != info2.notifications_enabled ||
         info1.resize_lock_state != info2.resize_lock_state ||
         info1.resize_lock_needs_confirmation !=
             info2.resize_lock_needs_confirmation ||
         info1.ready != info2.ready || info1.suspended != info2.suspended ||
         info1.show_in_launcher != info2.show_in_launcher ||
         info1.launchable != info2.launchable;
}

bool InstallIconFromFileThread(const base::FilePath& icon_path,
                                const std::string& content_png) {
  LOG(INFO) << "==== InstallIconFromFileThread " << icon_path << " " << content_png.size();
  DCHECK(!content_png.empty());

  base::CreateDirectory(icon_path.DirName());

  int wrote =
      base::WriteFile(icon_path, reinterpret_cast<const char*>(&content_png[0]),
                      content_png.size());
  if (wrote != static_cast<int>(content_png.size())) {
    VLOG(2) << "Failed to write ARC icon file: " << icon_path.MaybeAsASCII()
            << ".";
    if (!base::DeleteFile(icon_path)) {
      VLOG(2) << "Couldn't delete broken icon file" << icon_path.MaybeAsASCII()
              << ".";
    }
    return false;
  }

  return true;
}

class NotificationsEnabledDeferred {
 public:
  explicit NotificationsEnabledDeferred(PrefService* prefs) : prefs_(prefs) {}

  void Put(const std::string& app_id, bool enabled) {
    DictionaryPrefUpdate update(
        prefs_, arc::prefs::kArcSetNotificationsEnabledDeferred);
    base::DictionaryValue* const dict = update.Get();
    dict->SetKey(app_id, base::Value(enabled));
  }

  bool Get(const std::string& app_id) {
    const base::DictionaryValue* dict =
        prefs_->GetDictionary(arc::prefs::kArcSetNotificationsEnabledDeferred);
    return dict->FindBoolKey(app_id).value_or(false);
  }

  void Remove(const std::string& app_id) {
    DictionaryPrefUpdate update(
        prefs_, arc::prefs::kArcSetNotificationsEnabledDeferred);
    base::DictionaryValue* const dict = update.Get();
    dict->RemoveKey(app_id);
  }

 private:
  PrefService* const prefs_;
};

ArcHeroAppListPrefs::ArcHeroAppListPrefs(Profile* profile,
      arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
          app_connection_holder_for_testing):
          profile_(profile), prefs_(profile->GetPrefs()),
          file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})){

  auto archero_session_manager = arc::ArcHeroSessionManager::Get();
  if (archero_session_manager){
    archero_session_manager->GetArcHeroSession()->set_bridge_observer(this);
  }

  LOG(INFO) << "======== ArcHeroAppListPrefs::ArcHeroAppListPrefs ";

  // auto policy_bridge =
      arc::ArcPolicyBridge::GetForBrowserContext(profile_);

  const base::FilePath& base_path = profile->GetPath();
  base_path_ = base_path.AppendASCII(arc::prefs::kArcApps);

  // app_connection_holder()->SetHost(this);
}

ArcHeroAppListPrefs::~ArcHeroAppListPrefs() {
  // for (auto& observer : observer_list_)
  //   observer.OnArcAppListPrefsDestroyed();

  // arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // if (!arc_session_manager)
  //   return;
  // DCHECK(arc::ArcServiceManager::Get());
  // arc_session_manager->RemoveObserver(this);

  auto archero_session_manager = arc::ArcHeroSessionManager::Get();
  if (archero_session_manager){
    archero_session_manager->GetArcHeroSession()->set_bridge_observer(nullptr);
  }

  // app_connection_holder()->RemoveObserver(this);
}

void ArcHeroAppListPrefs::onAppAdded(archero::protobuf::bridge::Application *pItem){
  LOG(INFO) << "======== ArcHeroAppListPrefs::onAppAdded " <<
    pItem->name() << " " << pItem->package() << " " << pItem->launch_intent().component();

  auto app_id = GetAppId(pItem->package(), pItem->launch_intent().component());
  // auto icon_dec = ArcAppIconDescriptor{48, ui::ScaleFactor::SCALE_FACTOR_100P};
  LOG(INFO) << "======== ArcHeroAppListPrefs::onAppAdded app_id " << app_id << " " << pItem->icon().size();

  for (auto scale_factor : ui::GetSupportedScaleFactors()) {
    for (int dip_size : default_app_icon_dip_sizes) {
      auto icon_dec = ArcAppIconDescriptor(dip_size, scale_factor);

      InstallIconFromFileThread(
        GetIconPath(app_id, ArcAppIconDescriptor(dip_size, scale_factor)),
        pItem->icon()
      );

      OnIconInstalled(app_id, icon_dec, true);
    }
  }

  // InstallIconFromFileThread(
  //   GetIconPath(app_id, icon_dec),
  //   pItem->icon()
  // );

  // base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
  file_task_runner_->PostTask(FROM_HERE,
    base::BindOnce(
      &ArcHeroAppListPrefs::AddAppAndShortcut,
      weak_ptr_factory_.GetWeakPtr(),
      std::string(pItem->name()), std::string(pItem->package()), std::string(pItem->launch_intent().component()),
      std::string() /* intent_uri */, std::string() /* icon_resource_id */,
      false /*app->smticky*/, false/*app->notifications_enabled*/, true /* app_ready */,
      false /*app->suspended*/, false /* shortcut */, true /* launchable */
    )
  );

  // RequestIcon(app_id, ArcAppIconDescriptor{48, ui::ScaleFactor::SCALE_FACTOR_100P});



    // base::BindOnce([&](){
    //   AddAppAndShortcut(
    //     name, package, component,
    //     std::string() /* intent_uri */, std::string() /* icon_resource_id */,
    //     false /*app->sticky*/, false/*app->notifications_enabled*/, true /* app_ready */,
    //     false /*app->suspended*/, false /* shortcut */, true /* launchable */);

    // }));
      // base::BindOnce(&FileSelectHelper::ProcessSelectedFilesMacOnUIThread,
                    //  base::Unretained(this), files_out, temporary_files));

  // AddAppAndShortcut(
  //       pAppItem->name, pAppItem->package, pAppItem->launch_intent.component,
  //       std::string() /* intent_uri */, std::string() /* icon_resource_id */,
  //       false /*app->sticky*/, false/*app->notifications_enabled*/, true /* app_ready */,
  //       false /*app->suspended*/, false /* shortcut */, true /* launchable */);

  // AnboxAppInfo appInfo;
  // appInfo.name = pAppItem->name;
  // appInfo.package = pAppItem->package;
  // appInfo.launch_intent.action = pAppItem->launch_intent.action;
  // appInfo.launch_intent.component = pAppItem->launch_intent.component;
  // appInfo.launch_intent.categories = pAppItem->launch_intent.categories;
  // appInfo.launch_intent.flags = pAppItem->launch_intent.flags;
  // appInfo.launch_intent.package = pAppItem->launch_intent.package;
  // appInfo.launch_intent.type = pAppItem->launch_intent.type;
  // appInfo.launch_intent.uri = pAppItem->launch_intent.uri;

  // appInfo.icon2.resize(pAppItem->icon.size());
  // std::transform(pAppItem->icon.begin(), pAppItem->icon.end(), appInfo.icon2.begin(), [](char v) {
  //   return static_cast<uint8_t>(v);
  // });

  // InstallIcon(
  //   GetAppId(appInfo.package, appInfo.launch_intent.action),
  //   ArcAppIconDescriptor{20, ui::ScaleFactor::SCALE_FACTOR_100P},
  //   appInfo.icon2
  // );

  // auto app_id = GetAppId(appInfo.package, appInfo.launch_intent.action);
  // app_list_.insert({app_id, appInfo});

  // auto it = app_list_.find(app_id);
  // if (it != app_list_.end()){
    // InstallIcon(app_id, ArcAppIconDescriptor{20, ui::ScaleFactor::SCALE_FACTOR_100P}, it->second.icon2);
  //   InstallIcon(app_id, ArcAppIconDescriptor{48, ui::ScaleFactor::SCALE_FACTOR_100P}, it->second.icon2);
  //   InstallIcon(app_id, ArcAppIconDescriptor{64, ui::ScaleFactor::SCALE_FACTOR_100P}, it->second.icon2);

  //   InstallIcon(app_id, ArcAppIconDescriptor{20, ui::ScaleFactor::SCALE_FACTOR_200P}, it->second.icon2);
  //   InstallIcon(app_id, ArcAppIconDescriptor{48, ui::ScaleFactor::SCALE_FACTOR_200P}, it->second.icon2);
  //   InstallIcon(app_id, ArcAppIconDescriptor{64, ui::ScaleFactor::SCALE_FACTOR_200P}, it->second.icon2);
  // }
}

void ArcHeroAppListPrefs::onAppRemoved(archero::protobuf::bridge::Application *pItem){
  LOG(INFO) << "======== ArcHeroAppListPrefs::onAppRemoved " << pItem->package() << " " << pItem->launch_intent().component();

  // auto app_id = GetAppId(pItem->package(), pItem->launch_intent().component());
  auto app_id = GetAppIdByPackageName(pItem->package());
  if (app_id.length() > 0){
    file_task_runner_->PostTask(FROM_HERE,
      base::BindOnce(
        &ArcHeroAppListPrefs::RemoveApp,
        weak_ptr_factory_.GetWeakPtr(),
        std::string(app_id)
      )
    );
  }
}

void ArcHeroAppListPrefs::OnTaskCreated(int task_id, const std::string &name, const std::string &package){
  int sessionId = 0;
  OnTaskCreated(task_id, package, GetActivityByPackageName(package), name, base::nullopt, sessionId);
}

void ArcHeroAppListPrefs::OnTaskRemoved(int task_id){
  LOG(INFO) << "=== ArcHeroAppListPrefs::OnTaskRemoved " <<  task_id;
  OnTaskDestroyed(task_id);
}

void ArcHeroAppListPrefs::AddAppAndShortcut(const std::string& name,
                                        const std::string& package_name,
                                        const std::string& activity,
                                        const std::string& intent_uri,
                                        const std::string& icon_resource_id,
                                        const bool sticky,
                                        const bool notifications_enabled,
                                        const bool app_ready,
                                        const bool suspended,
                                        const bool shortcut,
                                        const bool launchable) {
  const std::string app_id = shortcut ? GetAppId(package_name, intent_uri)
                                      : GetAppId(package_name, activity);

  // Do not add Play Store in certain conditions.
  if (app_id == arc::kPlayStoreAppId) {
    // TODO(khmel): Use show_in_launcher flag to hide the Play Store app.
    // Display Play Store if we are in Demo Mode.
    // TODO(b/154290639): Remove check for |IsDemoModeOfflineEnrolled| when
    //                    fixed in Play Store.
    if (arc::IsRobotOrOfflineDemoAccountMode() &&
        !(ash::DemoSession::IsDeviceInDemoMode() &&
          chromeos::features::ShouldShowPlayStoreInDemoMode() &&
          !ash::DemoSession::IsDemoModeOfflineEnrolled())) {
      return;
    }
  }

  std::string updated_name = name;
  // Add "(beta)" string to Play Store. See crbug.com/644576 for details.
  if (app_id == arc::kPlayStoreAppId)
    updated_name = l10n_util::GetStringUTF8(IDS_ARC_PLAYSTORE_ICON_TITLE_BETA);

  base::Time last_launch_time;
  const bool was_tracked = tracked_apps_.count(app_id);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_old_info;
  if (was_tracked) {
    app_old_info = GetApp(app_id);
    DCHECK(app_old_info);
    DCHECK(launchable);
    last_launch_time = app_old_info->last_launch_time;
    if (updated_name != app_old_info->name) {
      for (auto& observer : observer_list_)
        observer.OnAppNameUpdated(app_id, updated_name);
    }
  }

  // Ensure to query the resize lock state from the prefs as we don't want the
  // default resize lock value (UNDEFINED) to override the existing value.
  // const auto resize_lock_state = GetResizeLockState(app_id);
  const auto resize_lock_state = arc::mojom::ArcResizeLockState::UNDEFINED;
  // const auto resize_lock_needs_confirmation =
  //     GetResizeLockNeedsConfirmation(app_id);
  const auto resize_lock_needs_confirmation = true;

  arc::ArcAppScopedPrefUpdate update(prefs_, app_id, arc::prefs::kArcApps);
  base::DictionaryValue* app_dict = update.Get();
  app_dict->SetString(kName, updated_name);
  app_dict->SetString(kPackageName, package_name);
  app_dict->SetString(kActivity, activity);
  app_dict->SetString(kIntentUri, intent_uri);
  app_dict->SetString(kIconResourceId, icon_resource_id);
  app_dict->SetBoolean(kSuspended, suspended);
  app_dict->SetBoolean(kSticky, sticky);
  app_dict->SetBoolean(kNotificationsEnabled, notifications_enabled);
  app_dict->SetInteger(kResizeLockState,
                       static_cast<int32_t>(resize_lock_state));
  app_dict->SetBoolean(kResizeLockNeedsConfirmation,
                       resize_lock_needs_confirmation);
  app_dict->SetBoolean(kShortcut, shortcut);
  app_dict->SetBoolean(kLaunchable, launchable);

  // Note the install time is the first time the Chrome OS sees the app, not the
  // actual install time in Android side.
  if (GetInstallTime(app_id).is_null()) {
    std::string install_time_str =
        base::NumberToString(base::Time::Now().ToInternalValue());
    app_dict->SetString(kInstallTime, install_time_str);
  }

  const bool was_disabled = ready_apps_.count(app_id) == 0;
  DCHECK(!(!was_disabled && !app_ready));
  if (was_disabled && app_ready)
    ready_apps_.insert(app_id);

  AppInfo app_info(
      updated_name, package_name, activity, intent_uri, icon_resource_id,
      last_launch_time, GetInstallTime(app_id), sticky, notifications_enabled,
      resize_lock_state, resize_lock_needs_confirmation, app_ready, suspended,
      launchable && arc::ShouldShowInLauncher(app_id), shortcut, launchable);

  if (was_tracked) {
    if (AreAppStatesChanged(*app_old_info, app_info)) {
      for (auto& observer : observer_list_)
        observer.OnAppStatesChanged(app_id, app_info);
    }
  } else {
    for (auto& observer : observer_list_)
      observer.OnAppRegistered(app_id, app_info);
    // default_apps_->SetAppHidden(app_id, false);
    tracked_apps_.insert(app_id);

    // Newly installed apps are subject to ARC++ resize lock. Set the state to
    // READY so the lock will be turned on next time they are launched.
    // SetResizeLockState(app_id, arc::mojom::ArcResizeLockState::READY);
  }

  // Send pending requests in case app becomes visible.
  if (!app_old_info || !app_old_info->ready) {
    for (const auto& descriptor : request_icon_recorded_[app_id])
      RequestIcon(app_id, descriptor);
  }

  if (app_ready) {
    const bool deferred_notifications_enabled =
        NotificationsEnabledDeferred(prefs_).Get(app_id);
    if (deferred_notifications_enabled)
      SetNotificationsEnabled(app_id, deferred_notifications_enabled);

    // Invalidate app icons in case it was already registered, becomes ready and
    // icon version is updated. This allows to use previous icons until new
    // icons are been prepared.
    const base::Value* existing_version = app_dict->FindKey(kIconVersion);
    if (was_tracked && (!existing_version ||
                        existing_version->GetInt() != current_icons_version)) {
      VLOG(1) << "Invalidate icons for " << app_id << " from "
              << (existing_version ? existing_version->GetInt() : -1) << " to "
              << current_icons_version;
      InvalidateAppIcons(app_id);
    }

    app_dict->SetKey(kIconVersion, base::Value(current_icons_version));

    if (arc::IsArcForceCacheAppIcon()) {
      // Request full set of app icons.
      VLOG(1) << "Requested full set of app icons " << app_id;
      for (auto scale_factor : ui::GetSupportedScaleFactors()) {
        for (int dip_size : default_app_icon_dip_sizes) {
          MaybeRequestIcon(app_id,
                           ArcAppIconDescriptor(dip_size, scale_factor));
        }
      }
    }
  }
}

void ArcHeroAppListPrefs::RemoveApp(const std::string& app_id) {
  // Delete cached icon if there is any.
  std::unique_ptr<ArcHeroAppListPrefs::AppInfo> app_info = GetApp(app_id);
  if (app_info && !app_info->icon_resource_id.empty())
    arc::RemoveCachedIcon(app_info->icon_resource_id);

  MaybeRemoveIconRequestRecord(app_id);

  // From now, app is not available.
  ready_apps_.erase(app_id);
  active_icons_.erase(app_id);

  // In case default app, mark it as hidden.
  // default_apps_->SetAppHidden(app_id, true);

  // Remove asyncronously local data on file system.
  ScheduleAppFolderDeletion(app_id);

  // Remove from prefs.
  DictionaryPrefUpdate update(prefs_, arc::prefs::kArcApps);
  base::DictionaryValue* apps = update.Get();
  const bool removed = apps->Remove(app_id, nullptr);
  DCHECK(removed);

  // |tracked_apps_| contains apps that are reported externally as available.
  // However, in case ARC++ appears as disbled on next start and had some apps
  // left in prefs from the previous session, app clean up is performed on very
  // early stage. Don't report |OnAppRemoved| in this case once the app was not
  // reported as available for the current session.
  if (!tracked_apps_.count(app_id))
    return;

  for (auto& observer : observer_list_)
    observer.OnAppRemoved(app_id);
  tracked_apps_.erase(app_id);
}

std::string ArcHeroAppListPrefs::GetActivityByPackageName(
    const std::string& package_name) const {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  if (!apps)
    return std::string();

  for (const auto& it : apps->DictItems()) {
    const base::Value& value = it.second;
    const base::Value* installed_package_name =
        value.FindKeyOfType(kPackageName, base::Value::Type::STRING);
    if (!installed_package_name ||
        installed_package_name->GetString() != package_name)
      continue;

    const base::Value* activity_name =
        value.FindKeyOfType(kActivity, base::Value::Type::STRING);
    return activity_name ? activity_name->GetString() : std::string();
  }
  return std::string();
}

std::unique_ptr<ArcHeroAppListPrefs::AppInfo> ArcHeroAppListPrefs::GetAppFromPrefs(
  const std::string& app_id) const {
  const base::DictionaryValue* app = nullptr;
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  LOG(INFO) << "=== ArcHeroAppListPrefs::GetAppFromPrefs " << app_id << " " << apps->GetDictionaryWithoutPathExpansion(app_id, &app);
  if (!apps || !apps->GetDictionaryWithoutPathExpansion(app_id, &app))
    return nullptr;

  LOG(INFO) << "=== ArcHeroAppListPrefs::GetAppFromPrefs succeed " << app_id;

  std::string name;
  std::string package_name;
  std::string activity;
  std::string intent_uri;
  std::string icon_resource_id;
  bool notifications_enabled =
      app->FindBoolKey(kNotificationsEnabled).value_or(true);
  auto resize_lock_state = static_cast<arc::mojom::ArcResizeLockState>(
      app->FindIntKey(kResizeLockState)
          .value_or(
              static_cast<int32_t>(arc::mojom::ArcResizeLockState::UNDEFINED)));
  const bool shortcut = app->FindBoolKey(kShortcut).value_or(false);
  const bool launchable = app->FindBoolKey(kLaunchable).value_or(true);

  app->GetString(kName, &name);
  app->GetString(kPackageName, &package_name);
  app->GetString(kActivity, &activity);
  app->GetString(kIntentUri, &intent_uri);
  app->GetString(kIconResourceId, &icon_resource_id);

  DCHECK(!name.empty());
  DCHECK(!shortcut || activity.empty());
  DCHECK(!shortcut || !intent_uri.empty());

  int64_t last_launch_time_internal = 0;
  base::Time last_launch_time;
  if (GetInt64FromPref(app, kLastLaunchTime, &last_launch_time_internal)) {
    last_launch_time = base::Time::FromInternalValue(last_launch_time_internal);
  }

  const bool deferred = NotificationsEnabledDeferred(prefs_).Get(app_id);
  if (deferred)
    notifications_enabled = deferred;

  return std::make_unique<AppInfo>(
      name, package_name, activity, intent_uri, icon_resource_id,
      last_launch_time, GetInstallTime(app_id),
      app->FindBoolKey(kSticky).value_or(false), notifications_enabled,
      resize_lock_state,
      app->FindBoolKey(kResizeLockNeedsConfirmation).value_or(true),
      ready_apps_.count(app_id) > 0 /* ready */,
      app->FindBoolKey(kSuspended).value_or(false),
      launchable && arc::ShouldShowInLauncher(app_id), shortcut, launchable);
}

std::unique_ptr<ArcHeroAppListPrefs::PackageInfo> ArcHeroAppListPrefs::GetPackage(
    const std::string& package_name) const {
  // if (!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_))
  //   return nullptr;

  const base::DictionaryValue* package = nullptr;
  const base::DictionaryValue* packages =
      prefs_->GetDictionary(arc::prefs::kArcPackages);
  if (!packages ||
      !packages->GetDictionaryWithoutPathExpansion(package_name, &package))
    return nullptr;

  if (package->FindBoolKey(kUninstalled).value_or(false))
    return nullptr;

  int64_t last_backup_android_id = 0;
  int64_t last_backup_time = 0;
  base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
      permissions;

  GetInt64FromPref(package, kLastBackupAndroidId, &last_backup_android_id);
  GetInt64FromPref(package, kLastBackupTime, &last_backup_time);
  const base::Value* permission_val = package->FindKey(kPermissionStates);
  if (permission_val) {
    const base::DictionaryValue* permission_dict = nullptr;
    permission_val->GetAsDictionary(&permission_dict);
    DCHECK(permission_dict);

    for (base::DictionaryValue::Iterator iter(*permission_dict);
         !iter.IsAtEnd(); iter.Advance()) {
      int64_t permission_type = -1;
      base::StringToInt64(iter.key(), &permission_type);
      DCHECK_NE(-1, permission_type);

      const base::Value& permission_state = iter.value();

      const base::DictionaryValue* permission_state_dict;
      if (permission_state.GetAsDictionary(&permission_state_dict)) {
        bool granted =
            permission_state_dict->FindBoolKey(kPermissionStateGranted)
                .value_or(false);
        bool managed =
            permission_state_dict->FindBoolKey(kPermissionStateManaged)
                .value_or(false);
        arc::mojom::AppPermission permission =
            static_cast<arc::mojom::AppPermission>(permission_type);
        permissions.emplace(permission,
                            arc::mojom::PermissionState::New(granted, managed));
      } else {
        LOG(ERROR) << "Permission state was not a dictionary.";
      }
    }
  }

  return std::make_unique<PackageInfo>(
      package_name, package->FindIntKey(kPackageVersion).value_or(0),
      last_backup_android_id, last_backup_time,
      package->FindBoolKey(kShouldSync).value_or(false),
      package->FindBoolKey(kSystem).value_or(false),
      package->FindBoolKey(kVPNProvider).value_or(false),
      std::move(permissions));
}

std::string ArcHeroAppListPrefs::GetAppIdByPackageName(
  const std::string& package_name) const {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  if (!apps)
    return std::string();

  for (const auto& it : apps->DictItems()) {
    const base::Value& value = it.second;
    const base::Value* installed_package_name =
        value.FindKeyOfType(kPackageName, base::Value::Type::STRING);
    if (!installed_package_name ||
        installed_package_name->GetString() != package_name)
      continue;

    const base::Value* activity_name =
        value.FindKeyOfType(kActivity, base::Value::Type::STRING);
    return activity_name ? GetAppId(package_name, activity_name->GetString())
                        : std::string();
  }
  return std::string();
}

base::Time ArcHeroAppListPrefs::GetInstallTime(const std::string& app_id) const {
  const base::DictionaryValue* app = nullptr;
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  if (!apps || !apps->GetDictionaryWithoutPathExpansion(app_id, &app))
    return base::Time();

  std::string install_time_str;
  if (!app->GetString(kInstallTime, &install_time_str))
    return base::Time();

  int64_t install_time_i64;
  if (!base::StringToInt64(install_time_str, &install_time_i64))
    return base::Time();
  return base::Time::FromInternalValue(install_time_i64);
}

bool ArcHeroAppListPrefs::IsRegistered(const std::string& app_id) const {
  // if ((!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_)) &&
  //     !default_apps_->HasApp(app_id))
  //   return false;

  const base::DictionaryValue* app = nullptr;
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  return apps && apps->GetDictionaryWithoutPathExpansion(app_id, &app);
}

void ArcHeroAppListPrefs::InvalidateAppIcons(const std::string& app_id) {
  // Ignore Play Store app since we provide its icon in Chrome resources.
  if (app_id == arc::kPlayStoreAppId)
    return;

  // Clean up previous icon records. They may refer to outdated icons.
  MaybeRemoveIconRequestRecord(app_id);

  // Clear icon cache that contains outdated icons.
  ScheduleAppFolderDeletion(app_id);

  // Re-request active icons.
  for (const auto& descriptor : active_icons_[app_id])
    MaybeRequestIcon(app_id, descriptor);
}

std::vector<std::string> ArcHeroAppListPrefs::GetAppIdsNoArcEnabledCheck() const {
  std::vector<std::string> ids;
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  DCHECK(apps);

  // crx_file::id_util is de-facto utility for id generation.
  for (base::DictionaryValue::Iterator app_id(*apps); !app_id.IsAtEnd();
      app_id.Advance()) {
    if (!crx_file::id_util::IdIsValid(app_id.key()))
      continue;

    ids.push_back(app_id.key());
  }

  return ids;
}

void ArcHeroAppListPrefs::SendIconRequest(const std::string& app_id,
                                      const AppInfo& app_info,
                                      const ArcAppIconDescriptor& descriptor) {
  auto callback =
      base::BindOnce(&ArcHeroAppListPrefs::OnIcon, weak_ptr_factory_.GetWeakPtr(),
                     app_id, descriptor);
  if (app_info.icon_resource_id.empty()) {
    auto* app_instance =
        ARC_GET_INSTANCE_FOR_METHOD(app_connection_holder(), GetAppIcon);
    if (!app_instance)
      return;  // Error is logged in macro.

    LOG(INFO) << "=== ArcHeroAppListPrefs::SendIconRequest-0 " << app_id;
    app_instance->GetAppIcon(app_info.package_name, app_info.activity,
                             descriptor.GetSizeInPixels(), std::move(callback));
  } else {
    auto* app_instance = ARC_GET_INSTANCE_FOR_METHOD(app_connection_holder(),
                                                     GetAppShortcutIcon);
    if (!app_instance)
      return;  // Error is logged in macro.

    LOG(INFO) << "=== ArcHeroAppListPrefs::SendIconRequest-1 " << app_id;
    app_instance->GetAppShortcutIcon(app_info.icon_resource_id,
                                     descriptor.GetSizeInPixels(),
                                     std::move(callback));
  }
}

void ArcHeroAppListPrefs::MaybeRequestIcon(const std::string& app_id,
                        const ArcAppIconDescriptor& descriptor){
    LOG(INFO) << "=== ArcHeroAppListPrefs MaybeRequestIcon " << app_id;

    // ArcSessionManager can be terminated during test tear down, before callback
    // into this function.
    // TODO(victorhsieh): figure out the best way/place to handle this situation.
    if (arc::ArcSessionManager::Get() == nullptr)
      return;

    if (!IsRegistered(app_id)) {
      VLOG(2) << "Request to load icon for non-registered app: " << app_id << ".";
      return;
    }

    // In case app is not ready, recorded request will be send to ARC when app
    // becomes ready.
    // This record will prevent ArcAppIcon from resending request to ARC for app
    // icon when icon file decode failure is suffered in case app sends bad icon.
    request_icon_recorded_[app_id].insert(descriptor);

    if (!ready_apps_.count(app_id))
      return;

    if (!app_connection_holder()->IsConnected()) {
      // AppInstance should be ready since we have app_id in ready_apps_. This
      // can happen in browser_tests.
      return;
    }

    std::unique_ptr<AppInfo> app_info = GetApp(app_id);
    if (!app_info) {
      VLOG(2) << "Failed to get app info: " << app_id << ".";
      return;
    }

    SendIconRequest(app_id, *app_info, descriptor);
  }

  arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
    ArcHeroAppListPrefs::app_connection_holder() {
    // Some tests set their own holder. If it's set, return the holder.
    // if (app_connection_holder_for_testing_)
      // return app_connection_holder_for_testing_;
    auto* arc_service_manager = arc::ArcServiceManager::Get();
    // The null check is for unit tests. On production, |arc_service_manager| is
    // always non-null.
    if (!arc_service_manager)
      return nullptr;
    return arc_service_manager->arc_bridge_service()->app();
  }