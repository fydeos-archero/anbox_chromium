// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/anbox_app_list_prefs_factory.h"

#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/anbox_app_list_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

bool AnboxAppListPrefsFactory::is_sync_test_ = false;

// static
AnboxAppListPrefs* AnboxAppListPrefsFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AnboxAppListPrefs*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AnboxAppListPrefsFactory* AnboxAppListPrefsFactory::GetInstance() {
  return base::Singleton<AnboxAppListPrefsFactory>::get();
}

// static
void AnboxAppListPrefsFactory::SetFactoryForSyncTest() {
  is_sync_test_ = true;
}

// static
bool AnboxAppListPrefsFactory::IsFactorySetForSyncTest() {
  return is_sync_test_;
}

void AnboxAppListPrefsFactory::RecreateServiceInstanceForTesting(
    content::BrowserContext* context) {
  Disassociate(context);
  BuildServiceInstanceFor(context);
}

AnboxAppListPrefsFactory::AnboxAppListPrefsFactory()
    : BrowserContextKeyedServiceFactory(
          "AnboxAppListPrefs",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

AnboxAppListPrefsFactory::~AnboxAppListPrefsFactory() {
}

KeyedService* AnboxAppListPrefsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  LOG(INFO) << "======== AnboxAppListPrefsFactory::BuildServiceInstanceFor";  

  // if (!arc::IsArcAllowedForProfile(profile))
    // return nullptr;

  // auto* arc_service_manager = arc::ArcServiceManager::Get();
  // if (!arc_service_manager)
  //   return nullptr;  // ARC is not supported

  return AnboxAppListPrefs::Create(profile);
}

content::BrowserContext* AnboxAppListPrefsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // This matches the logic in ExtensionSyncServiceFactory, which uses the
  // orginal browser context.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
