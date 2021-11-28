// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/archero_apps_factory.h"

#include "base/logging.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/archero_apps.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/archero_app_list_prefs_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace apps {

// static
ArcHeroApps* ArcHeroAppsFactory::GetForProfile(Profile* profile) {
  LOG(INFO) << "======= ArcHeroAppsFactory::GetForProfile";

  return static_cast<ArcHeroApps*>(
      ArcHeroAppsFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
ArcHeroAppsFactory* ArcHeroAppsFactory::GetInstance() {
  return base::Singleton<ArcHeroAppsFactory>::get();
}

// static
void ArcHeroAppsFactory::ShutDownForTesting(content::BrowserContext* context) {
  auto* factory = GetInstance();
  factory->BrowserContextShutdown(context);
  factory->BrowserContextDestroyed(context);
}

ArcHeroAppsFactory::ArcHeroAppsFactory()
    : BrowserContextKeyedServiceFactory(
          "ArcHeroApps",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ArcHeroAppListPrefsFactory::GetInstance());
  DependsOn(arc::ArcIntentHelperBridge::GetFactory());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());

  LOG(INFO) << "============ ArcHeroAppsFactory::ArcHeroAppsFactory";
}

ArcHeroAppsFactory::~ArcHeroAppsFactory() = default;

KeyedService* ArcHeroAppsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  LOG(INFO) << "======= ArcHeroAppsFactory::BuildServiceInstanceFor " << context;

  return new ArcHeroApps(Profile::FromBrowserContext(context));
}

}  // namespace apps