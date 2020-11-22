// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/anbox_apps_factory.h"

#include "base/logging.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/anbox_apps.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/anbox_app_list_prefs_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace apps {

// static
AnboxApps* AnboxAppsFactory::GetForProfile(Profile* profile) {
  LOG(INFO) << "======= AnboxAppsFactory::GetForProfile";

  return static_cast<AnboxApps*>(
      AnboxAppsFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
AnboxAppsFactory* AnboxAppsFactory::GetInstance() {
  return base::Singleton<AnboxAppsFactory>::get();
}

// static
void AnboxAppsFactory::ShutDownForTesting(content::BrowserContext* context) {
  auto* factory = GetInstance();
  factory->BrowserContextShutdown(context);
  factory->BrowserContextDestroyed(context);
}

AnboxAppsFactory::AnboxAppsFactory()
    : BrowserContextKeyedServiceFactory(
          "AnboxApps",
          BrowserContextDependencyManager::GetInstance()) {  
  DependsOn(AnboxAppListPrefsFactory::GetInstance());
  DependsOn(arc::ArcIntentHelperBridge::GetFactory());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());

  LOG(INFO) << "============ AnboxAppsFactory::AnboxAppsFactory";
}

AnboxAppsFactory::~AnboxAppsFactory() = default;

KeyedService* AnboxAppsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  LOG(INFO) << "======= AnboxAppsFactory::BuildServiceInstanceFor " << context;  

  return new AnboxApps(Profile::FromBrowserContext(context));
}

}  // namespace apps
