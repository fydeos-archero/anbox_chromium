// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/archero_app_list_prefs_factory.h"

#include "components/arc/arc_util.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/archero_app_list_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

bool ArcHeroAppListPrefsFactory::is_sync_test_ = false;

// static
ArcHeroAppListPrefs* ArcHeroAppListPrefsFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ArcHeroAppListPrefs*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ArcHeroAppListPrefsFactory* ArcHeroAppListPrefsFactory::GetInstance() {
  return base::Singleton<ArcHeroAppListPrefsFactory>::get();
}

// static
void ArcHeroAppListPrefsFactory::SetFactoryForSyncTest() {
  is_sync_test_ = true;
}

// static
bool ArcHeroAppListPrefsFactory::IsFactorySetForSyncTest() {
  return is_sync_test_;
}

void ArcHeroAppListPrefsFactory::RecreateServiceInstanceForTesting(
    content::BrowserContext* context) {
  Disassociate(context);
  BuildServiceInstanceFor(context);
}

ArcHeroAppListPrefsFactory::ArcHeroAppListPrefsFactory()
    : BrowserContextKeyedServiceFactory(
          "ArcHeroAppListPrefs",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

ArcHeroAppListPrefsFactory::~ArcHeroAppListPrefsFactory() {
}

KeyedService* ArcHeroAppListPrefsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  LOG(INFO) << "======== ArcHeroAppListPrefsFactory::BuildServiceInstanceFor";

  // if (!arc::IsArcAllowedForProfile(profile))
    // return nullptr;

  // auto* arc_service_manager = arc::ArcServiceManager::Get();
  // if (!arc_service_manager)
  //   return nullptr;  // ARC is not supported

  return ArcHeroAppListPrefs::Create(profile);
}

content::BrowserContext* ArcHeroAppListPrefsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // This matches the logic in ExtensionSyncServiceFactory, which uses the
  // orginal browser context.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}