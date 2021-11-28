#include <algorithm>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/ash_features.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "chrome/browser/apps/app_service/webapk/webapk_manager.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/metrics/arc_metrics_service.h"
#include "components/arc/mojom/app_permissions.mojom.h"
#include "components/arc/mojom/compatibility_mode.mojom.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"

#include "chrome/browser/apps/app_service/publishers/archero_apps.h"
#include "chrome/browser/ash/arc/session/archero_session_manager.h"
#include "components/arc/session/archero_session.h"
// #include "chrome/browser/apps/app_service/app_service_proxy_chromeos.h"
// #include "ash/constants/ash_features.cc"
// #include "chrome/common/chrome_features.cc"

namespace apps {

bool ShouldShow(const ArcHeroAppListPrefs::AppInfo& app_info) {
  return app_info.show_in_launcher;
}

absl::optional<arc::UserInteractionType> GetUserInterationType(
    apps::mojom::LaunchSource launch_source) {
  auto user_interaction_type = arc::UserInteractionType::NOT_USER_INITIATED;
  switch (launch_source) {
    // kUnknown is not set anywhere, this case is not valid.
    case apps::mojom::LaunchSource::kUnknown:
      return absl::nullopt;
    case apps::mojom::LaunchSource::kFromChromeInternal:
      user_interaction_type = arc::UserInteractionType::NOT_USER_INITIATED;
      break;
    case apps::mojom::LaunchSource::kFromAppListGrid:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER;
      break;
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_CONTEXT_MENU;
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SEARCH;
      break;
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      user_interaction_type = arc::UserInteractionType::
          APP_STARTED_FROM_LAUNCHER_SEARCH_CONTEXT_MENU;
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP;
      break;
    case apps::mojom::LaunchSource::kFromParentalControls:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_SETTINGS;
      break;
    case apps::mojom::LaunchSource::kFromShelf:
      user_interaction_type = arc::UserInteractionType::APP_STARTED_FROM_SHELF;
      break;
    case apps::mojom::LaunchSource::kFromFileManager:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_FILE_MANAGER;
      break;
    case apps::mojom::LaunchSource::kFromLink:
      user_interaction_type = arc::UserInteractionType::APP_STARTED_FROM_LINK;
      break;
    case apps::mojom::LaunchSource::kFromOmnibox:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_OMNIBOX;
      break;
    case apps::mojom::LaunchSource::kFromSharesheet:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_SHARESHEET;
      break;
    case apps::mojom::LaunchSource::kFromFullRestore:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_FULL_RESTORE;
      break;
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
      user_interaction_type = arc::UserInteractionType::
          APP_STARTED_FROM_SMART_TEXT_SELECTION_CONTEXT_MENU;
      break;
    default:
      NOTREACHED();
      return absl::nullopt;
  }
  return user_interaction_type;
}

ArcHeroApps::ArcHeroApps(Profile* profile, apps::AppServiceProxyChromeOs* proxy):profile_(profile){
  // if (!arc::IsArcAllowedForProfile(profile_) ||
  //     (arc::ArcServiceManager::Get() == nullptr)) {
  //   return;
  // }

  if (!proxy) {
    proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  }
  mojo::Remote<apps::mojom::AppService>& app_service = proxy->AppService();
  if (!app_service.is_bound()) {
    return;
  }

  // Make some observee-observer connections.
  auto prefs = ArcHeroAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  prefs->AddObserver(this);
  proxy->SetArcIsRegistered();

  // auto* intent_helper_bridge =
  //     arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  // if (intent_helper_bridge) {
  //   if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
  //     intent_helper_bridge->SetAdaptiveIconDelegate(
  //         &arc_activity_adaptive_icon_impl_);
  //   }
  //   arc_intent_helper_observation_.Observe(intent_helper_bridge);
  // }

  // There is no MessageCenterController for unit tests, so observe when the
  // MessageCenterController is created in production code.
  // if (ash::ArcNotificationsHostInitializer::Get()) {
  //   notification_initializer_observation_.Observe(
  //       ash::ArcNotificationsHostInitializer::Get());
  // }

  // auto* instance_registry = &proxy->InstanceRegistry();
  // if (instance_registry) {
  //   instance_registry_observation_.Observe(instance_registry);
  // }

  // if (base::FeatureList::IsEnabled(ash::features::kWebApkGenerator)) {
  //   web_apk_manager_ = std::make_unique<apps::WebApkManager>(profile_);
  // }

  PublisherBase::Initialize(app_service, apps::mojom::AppType::kArcHero);

  LOG(INFO) << "=== ArcHeroApps::ArcHeroApps";

  // if (!proxy) {
      //   proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
      // }
      // mojo::Remote<apps::mojom::AppService>& app_service = proxy->AppService();
      // if (!app_service.is_bound()) {
      //   return;
      // }

      // LOG(INFO) << "======== ArcHeroApps::ArcHeroApps 3 " << ArcHeroAppListPrefs::Get(profile_);

      // // Make some observee-observer connections.
      // ArcHeroAppListPrefs* prefs = ArcHeroAppListPrefs::Get(profile_);
      // if (!prefs) {
      //   return;
      // }
      // prefs->AddObserver(this);
      // proxy->SetArcIsRegistered();

      // LOG(INFO) << "======== ArcHeroApps::ArcHeroApps 4";

      // auto* intent_helper_bridge =
      //     arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
      // if (intent_helper_bridge) {
      //   arc_intent_helper_observer_.Add(intent_helper_bridge);
      // }

      // app_service->RegisterPublisher(receiver_.BindNewPipeAndPassRemote(),
      //                               apps::mojom::AppType::kAnbox);
}

void ArcHeroApps::Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
               apps::mojom::ConnectOptionsPtr opts){
  std::vector<apps::mojom::AppPtr> apps;
  ArcHeroAppListPrefs* prefs = ArcHeroAppListPrefs::Get(profile_);
  if (prefs) {
    for (const auto& app_id : prefs->GetAppIds()) {
      std::unique_ptr<ArcHeroAppListPrefs::AppInfo> app_info =
          prefs->GetApp(app_id);
      if (app_info) {
        apps.push_back(Convert(prefs, app_id, *app_info));
      }
    }
  }
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps), apps::mojom::AppType::kArcHero,
                    true /* should_notify_initialized */);
  subscribers_.Add(std::move(subscriber));

  LOG(INFO) << "=== ArcHeroApps::Connect size: " << prefs->GetAppIds().size();
}

apps::mojom::AppPtr ArcHeroApps::Convert(ArcHeroAppListPrefs* prefs,
                              const std::string& app_id,
                              const ArcHeroAppListPrefs::AppInfo& app_info,
                              bool update_icon){
  apps::mojom::AppPtr app = PublisherBase::MakeApp(
    apps::mojom::AppType::kArcHero, app_id,
    app_info.suspended ? apps::mojom::Readiness::kDisabledByPolicy
                        : apps::mojom::Readiness::kReady,
    app_info.name, apps::mojom::InstallSource::kUser);

  app->publisher_id = app_info.package_name;

  // auto paused = paused_apps_.IsPaused(app_id)
  //                   ? apps::mojom::OptionalBool::kTrue
  //                   : apps::mojom::OptionalBool::kFalse;
  auto paused = apps::mojom::OptionalBool::kFalse;

  // if (update_icon) {
  //   app->icon_key =
  //       icon_key_factory_.MakeIconKey(GetIconEffects(app_id, app_info));
  // }

  app->last_launch_time = app_info.last_launch_time;
  app->install_time = app_info.install_time;

  auto show = apps::mojom::OptionalBool::kTrue;
  // All published ARC apps are launchable. All launchable apps should be
  // permitted to be shown on the shelf, and have their pins on the shelf
  // persisted.
  app->show_in_shelf = apps::mojom::OptionalBool::kTrue;
  app->show_in_launcher = show;
  app->show_in_search = show;
  app->show_in_management = show;

  // app->has_badge = app_notifications_.HasNotification(app_id)
  //                     ? apps::mojom::OptionalBool::kTrue
  //                     : apps::mojom::OptionalBool::kFalse;
  app->has_badge = apps::mojom::OptionalBool::kFalse;
  app->paused = paused;
  // app->resize_locked = IsResizeLocked(prefs, app_id);
  app->resize_locked = apps::mojom::OptionalBool::kFalse;

  std::unique_ptr<ArcHeroAppListPrefs::PackageInfo> package =
      prefs->GetPackage(app_info.package_name);
  if (package) {
    // UpdateAppPermissions(package->permissions, &app->permissions);
  }

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper_bridge &&
      app_info.package_name !=
          arc::ArcIntentHelperBridge::kArcIntentHelperPackageName) {
    // UpdateAppIntentFilters(app_info.package_name, intent_helper_bridge,
    //                       &app->intent_filters);
  }

  return app;
}

// void LoadIconFromFileWithFallback(
//     apps::mojom::IconType icon_type,
//     int size_hint_in_dip,
//     const base::FilePath& path,
//     IconEffects icon_effects,
//     apps::mojom::Publisher::LoadIconCallback callback,
//     base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)>
//         fallback)

void ArcHeroApps::LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback){

  // if (app_id.length() > 0){
  //   std::move(callback).Run(apps::mojom::IconValue::New());
  //   return;
  // }

  if (app_id.length() > 0){
    auto scale_factor = apps_util::GetPrimaryDisplayUIScaleFactor();

    const base::FilePath icon_path = profile_->GetPath();
    auto icon_full_path = icon_path.AppendASCII(/*arc::prefs::kArcApps*/ "arc.apps")
            .AppendASCII(app_id)
            .AppendASCII(
              ArcAppIconDescriptor{48, ui::ScaleFactor::SCALE_FACTOR_100P}.GetName()
            );

    LOG(INFO) << "=== ArcHeroApps::LoadIcon " << icon_full_path;

    LoadIconFromFileWithFallback(
        icon_type, size_hint_in_dip, icon_full_path,
        apps::IconEffects::kNone, std::move(callback),
        base::BindOnce(&ArcHeroApps::LoadIconFromVM, weak_ptr_factory_.GetWeakPtr(), app_id,
                         apps::mojom::IconType::kUncompressed, size_hint_in_dip,
                         scale_factor, apps::IconEffects::kNone, 0)
    );


    // LoadIconFromFileWithFallback(apps::mojom::IconType::kUncompressed, size_hint_in_dip,
    //   // registry_->GetIconPath(app_id, scale_factor),
    //   p1,
    //   // GetIconPath(app_id, ArcAppIconDescriptor{20, ui::ScaleFactor::SCALE_FACTOR_100P}),
    //   static_cast<IconEffects>(icon_key->icon_effects), std::move(callback),
    //   base::BindOnce(&ArcHeroApps::LoadIconFromVM,
    //                      weak_ptr_factory_.GetWeakPtr())
    // );
    return;
  }

  if (!icon_key) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }
  // IconEffects icon_effects = static_cast<IconEffects>(icon_key->icon_effects);

  // Treat the Play Store as a special case, loading an icon defined by a
  // resource instead of asking the Android VM (or the cache of previous
  // responses from the Android VM). Presumably this is for bootstrapping:
  // the Play Store icon (the UI for enabling and installing Android apps)
  // should be showable even before the user has installed their first
  // Android app and before bringing up an Android VM for the first time.
  if (app_id == arc::kPlayStoreAppId) {
    // LoadPlayStoreIcon(icon_compression, size_hint_in_dip, icon_effects,
                      // std::move(callback));
  } else {
    auto arc_prefs = ArcHeroAppListPrefs::Get(profile_);
    DCHECK(arc_prefs);

    // If the app has been removed, immediately terminate the icon request since
    // it can't possibly succeed.
    std::unique_ptr<ArcHeroAppListPrefs::AppInfo> app_info =
        arc_prefs->GetApp(app_id);
    if (!app_info) {
      std::move(callback).Run(apps::mojom::IconValue::New());
      return;
    }

    // arc_icon_once_loader_.LoadIcon(
    //     app_id, size_hint_in_dip, icon_compression,
    //     base::BindOnce(&OnArcAppIconCompletelyLoaded, icon_compression,
    //                    size_hint_in_dip, icon_effects, std::move(callback)));
  }
}

void ArcHeroApps::Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              apps::mojom::WindowInfoPtr window_info){
  LOG(INFO) << "=== ArcHeroApps::Launch";

  auto user_interaction_type = GetUserInterationType(launch_source);
  if (!user_interaction_type.has_value()) {
    return;
  }

  auto archero_app_list_prefs = ArcHeroAppListPrefs::Get(profile_);
  if (archero_app_list_prefs){
    auto app_info = archero_app_list_prefs->GetApp(app_id);

    if (!app_info){
      LOG(INFO) << "====== AnboxApps::Launch not found app info";
      return;
    }

    LOG(INFO) << "====== AnboxApps::Launch " << app_info->package_name << " " << app_info->name;

    auto archero_session_manager = arc::ArcHeroSessionManager::Get();
    if (archero_session_manager){
      archero_session_manager->GetArcHeroSession()->LaunchApp(app_info->name, app_info->package_name, app_info->activity);
    }
  }

  // arc::LaunchApp(profile_, app_id, event_flags, user_interaction_type.value(),
  //                display_id);
}

void ArcHeroApps::OnAppRegistered(const std::string& app_id,
                                 const ArcAppListPrefs::AppInfo& app_info){
  LOG(INFO) << "====== ArcHeroApps::OnAppRegistered " << app_id << " " <<
    app_info.package_name << " " << app_info.activity;

  auto prefs = ArcHeroAppListPrefs::Get(profile_);
  if (prefs) {
    Publish(Convert(prefs, app_id, app_info), subscribers_);
  }
}

void ArcHeroApps::SetIconEffect(const std::string& app_id) {
  auto prefs = ArcHeroAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }

  LOG(INFO) << "=== ArcHeroApps::SetIconEffect-0 " << app_id;

  // std::unique_ptr<ArcHeroAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  // if (!app_info) {
  //   return;
  // }

  // LOG(INFO) << "=== ArcHeroApps::SetIconEffect-1 " << app_id;

  // apps::mojom::AppPtr app = apps::mojom::App::New();
  // app->app_type = apps::mojom::AppType::kArcHero;
  // app->app_id = app_id;
  // app->icon_key =
  //     icon_key_factory_.MakeIconKey(GetIconEffects(app_id, *app_info));
  // Publish(std::move(app), subscribers_);

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kArcHero;
  app->app_id = app_id;
  app->icon_key =
      icon_key_factory_.MakeIconKey(IconEffects::kNone);
  Publish(std::move(app), subscribers_);
}

IconEffects ArcHeroApps::GetIconEffects(const std::string& app_id,
                                    const ArcHeroAppListPrefs::AppInfo& app_info) {
  // IconEffects icon_effects = IconEffects::kNone;
  // if (app_info.suspended) {
  //   icon_effects =
  //       static_cast<IconEffects>(icon_effects | IconEffects::kBlocked);
  // }
  // if (paused_apps_.IsPaused(app_id)) {
  //   icon_effects =
  //       static_cast<IconEffects>(icon_effects | IconEffects::kPaused);
  // }
  // return icon_effects;

  return IconEffects::kNone;
}

void ArcHeroApps::GetMenuModel(const std::string& app_id,
                           apps::mojom::MenuType menu_type,
                           int64_t display_id,
                           GetMenuModelCallback callback) {
  auto prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }
  const std::unique_ptr<ArcHeroAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  // Add Open item if the app is not opened and not suspended.
  if (!base::Contains(app_id_to_task_ids_, app_id) && !app_info->suspended) {
    AddCommandItem((menu_type == apps::mojom::MenuType::kAppList)
                       ? ash::LAUNCH_NEW
                       : ash::MENU_OPEN_NEW,
                   IDS_APP_CONTEXT_MENU_ACTIVATE_ARC, &menu_items);
  }

  if (app_info->shortcut) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_REMOVE_SHORTCUT, &menu_items);
  } else if (app_info->ready && !app_info->sticky) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, &menu_items);
  }

  // App Info item.
  if (app_info->ready && ShouldShow(*app_info)) {
    AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                   &menu_items);
  }

  if (menu_type == apps::mojom::MenuType::kShelf &&
      base::Contains(app_id_to_task_ids_, app_id)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, &menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
  // BuildMenuForShortcut(app_info->package_name, std::move(menu_items),
  //                      std::move(callback));
}

}