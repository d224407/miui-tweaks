
#include "common.h"

static const char *CATEGORY_CONF_KEY(const char *cat) {
  static const struct { const char *cat, *key; } map[] = {
    {"ads","DISABLE_ADS"},{"tracking","DISABLE_TRACKING"},{"analytics","DISABLE_ANALYTICS"},
    {"reporting","DISABLE_REPORTING"},{"background","DISABLE_BACKGROUND"},{"update","DISABLE_UPDATE"},
    {"location","DISABLE_LOCATION"},{"geofence","DISABLE_GEOFENCE"},{"nearby","DISABLE_NEARBY"},
    {"cast","DISABLE_CAST"},{"discovery","DISABLE_DISCOVERY"},{"sync","DISABLE_SYNC"},
    {"cloud","DISABLE_CLOUD"},{"auth","DISABLE_AUTH"},{"wallet","DISABLE_WALLET"},
    {"payment","DISABLE_PAYMENT"},{"wear","DISABLE_WEAR"},{"fitness","DISABLE_FITNESS"},
    {NULL,NULL}
  };
  for (int i = 0; map[i].cat; i++) if (strcmp(map[i].cat, cat) == 0) return map[i].key;
  return NULL; 
}

static int gms_should_disable(const char *conf, const char *cat) {
  const char *key = CATEGORY_CONF_KEY(cat);
  if (!key) return 0;
  return mt_is_on(conf, key);
}

static void pm_toggle(const char *svc, int disable) {
  char *argv[] = { (char *)"/system/bin/pm", disable ? (char *)"disable" : (char *)"enable", (char *)svc, NULL };
  mt_run(argv, 1);
}


typedef struct { const char *svc, *cat; } gms_entry;

/* Order matters: entries are applied top to bottom, so a later line for the
   same service overrides an earlier one (the trailing "core" block relies on it). */
static const gms_entry GMS_LIST[] = {
  {"com.google.android.gms/com.google.android.gms.ads.identifier.service.AdvertisingIdNotificationService", "ads"},
  {"com.google.android.gms/com.google.android.gms.ads.identifier.service.AdvertisingIdService", "ads"},
  {"com.google.android.gms/com.google.android.gms.nearby.mediums.nearfieldcommunication.NfcAdvertisingService", "ads"},
  {"com.google.android.gms/com.google.android.gms.analytics.AnalyticsService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.analytics.AnalyticsTaskService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.analytics.internal.PlayLogReportingService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.stats.eastworld.EastworldService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.stats.service.DropBoxEntryAddedService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.stats.PlatformStatsCollectorService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.common.stats.GmsCoreStatsService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.common.stats.StatsUploadService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.backup.stats.BackupStatsService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.checkin.CheckinApiService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.checkin.CheckinService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.tron.CollectionService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.common.config.PhenotypeCheckinService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.feedback.FeedbackAsyncService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.feedback.LegacyBugReportService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.feedback.OfflineReportSendTaskService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.googlehelp.metrics.ReportBatchedMetricsGcmTaskService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.analytics.internal.PlayLogReportingService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.locationsharingreporter.service.reporting.periodic.PeriodicReporterMonitoringService", "reporting"},
  {"com.google.android.gms/com.google.android.location.reporting.service.ReportingAndroidService", "location"},
  {"com.google.android.gms/com.google.android.location.reporting.service.ReportingSyncService", "location"},
  {"com.google.android.gms/com.google.android.gms.common.stats.net.NetworkReportService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.presencemanager.service.PresenceManagerPresenceReportService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.usagereporting.service.UsageReportingIntentService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastMediaRoute2ProviderService", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastMediaRoute2ProviderService_Isolated", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastMediaRoute2ProviderService_Persistent", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastMediaRouteProviderService", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastMediaRouteProviderService_Isolated", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastMediaRouteProviderService_Persistent", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastRemoteDisplayProviderService", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastRemoteDisplayProviderService_Isolated", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastRemoteDisplayProviderService_Persistent", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.service.CastPersistentService_Persistent", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.service.CastSocketMultiplexerLifeCycleService", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.service.CastSocketMultiplexerLifeCycleService_Isolated", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.service.CastSocketMultiplexerLifeCycleService_Persistent", "cast"},
  {"com.google.android.gms/com.google.android.gms.chimera.CastPersistentBoundBrokerService", "cast"},
  {"com.google.android.gms/com.google.android.gms.nearby.messages.debug.DebugPokeService", "nearby"},
  {"com.google.android.gms/com.google.android.gms.clearcut.debug.ClearcutDebugDumpService", "nearby"},
  {"com.google.firebase.components.ComponentDiscoveryService", "discovery"},
  {"com.google.android.gms/com.google.android.gms.nearby.discovery.service.DiscoveryService", "discovery"},
  {"com.google.android.gms/com.google.mlkit.common.internal.MlKitComponentDiscoveryService", "discovery"},
  {"com.google.android.gms/com.google.android.gms.geotimezone.GeoTimeZoneService", "location"},
  {"com.google.android.gms/com.google.android.gms.location.geocode.GeocodeService", "location"},
  {"com.google.android.gms/com.google.android.gms.chimera.GmsIntentOperationService_AuthAccountIsolated", "auth"},
  {"com.google.android.gms/com.google.android.gms.chimera.GmsIntentOperationService_AuthAccountIsolate", "auth"},
  {"com.google.android.gms/com.google.android.gms.chimera.PersistentApiService_AuthAccountIsolated", "auth"},
  {"com.google.android.gms/com.google.android.gms.chimera.PersistentIntentOperationService_AuthAccountIsolated", "auth"},
  {"com.google.android.gms/com.google.android.gms.auth.folsom.service.FolsomPublicKeyUpdateService", "update"},
  {"com.google.android.gms/com.google.android.gms.fonts.update.UpdateSchedulerService", "update"},
  {"com.google.android.gms/com.google.android.gms.icing.proxy.IcingInternalCorporaUpdateService", "update"},
  {"com.google.android.gms/com.google.android.gms.instantapps.routing.DomainFilterUpdateService", "update"},
  {"com.google.android.gms/com.google.android.gms.mobiledataplan.service.PeriodicUpdaterService", "update"},
  {"com.google.android.gms/com.google.android.gms.phenotype.service.sync.PackageUpdateTaskService", "update"},
  {"com.google.android.gms/com.google.android.gms.update.SystemUpdateGcmTaskService", "update"},
  {"com.google.android.gms/com.google.android.gms.update.SystemUpdateService", "update"},
  {"com.google.android.gms/com.google.android.gms.update.UpdateFromSdCardService", "update"},
  {"com.google.android.gms/com.google.android.gms.backup.wear.BackupSettingsListenerService", "wear"},
  {"com.google.android.gms/com.google.android.gms.dck.service.DckWearableListenerService", "wear"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.wearable.WearableSyncAccountService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.wearable.WearableSyncConfigService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.wearable.WearableSyncConnectionService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.wearable.WearableSyncMessageService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.wearables.WearableSyncService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.wearable.service.WearableControlService", "wear"},
  {"com.google.android.gms/com.google.android.gms.wearable.service.WearableService", "wear"},
  {"com.google.android.gms/com.google.android.gms.nearby.fastpair.service.WearableDataListenerService", "nearby"},
  {"com.google.android.gms/com.google.android.location.wearable.WearableLocationService", "location"},
  {"com.google.android.gms/com.google.android.location.fused.wearable.GmsWearableListenerService", "location"},
  {"com.google.android.gms/com.google.android.gms.mdm.services.MdmPhoneWearableListenerService", "update"},
  {"com.google.android.gms/com.google.android.gms.tapandpay.wear.WearProxyService", "wear"},
  {"com.google.android.gms/com.google.android.gms.auth.trustagent.GoogleTrustAgent", "wear"},
  {"com.google.android.gms/com.google.android.gms.trustagent.api.bridge.TrustAgentBridgeService", "wear"},
  {"com.google.android.gms/com.google.android.gms.trustagent.api.state.TrustAgentState", "wear"},
  {"com.google.android.gms/com.google.android.gms.enpromo.PromoInternalPersistentService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.enpromo.PromoInternalService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.thunderbird.EmergencyLocationService", "location"},
  {"com.google.android.gms/com.google.android.gms.thunderbird.EmergencyPersistentService", "location"},
  {"com.google.android.gms/com.google.android.gms.kids.chimera.KidsServiceProxy", "wear"},
  {"com.google.android.gms/com.google.android.gms.personalsafety.service.PersonalSafetyService", "wear"},
  {"com.google.android.gms/com.google.android.gms.fitness.cache.DataUpdateListenerCacheService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.ble.FitBleBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.config.FitConfigBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.goals.FitGoalsBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.history.FitHistoryBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.internal.FitInternalBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.proxy.FitProxyBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.recording.FitRecordingBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.sensors.FitSensorsBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.sessions.FitSessionsBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.sensors.sample.CollectSensorService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.sync.FitnessSyncAdapterService", "sync"},
  {"com.google.android.gms/com.google.android.gms.fitness.sync.SyncGcmTaskService", "sync"},
  {"com.google.android.gms/com.google.android.gms.nearby.bootstrap.service.NearbyBootstrapService", "nearby"},
  {"com.google.android.gms/com.google.android.gms.nearby.connection.service.NearbyConnectionsAndroidService", "nearby"},
  {"com.google.android.gms/com.google.location.nearby.direct.service.NearbyDirectService", "nearby"},
  {"com.google.android.gms/com.google.android.gms.nearby.messages.service.NearbyMessagesService", "nearby"},
  {"com.google.android.gms/com.google.android.gms.analytics.internal.PlayLogReportingService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.romanesco.ContactsLoggerUploadService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.magictether.logging.DailyMetricsLoggerService", "background"},
  {"com.google.android.gms/com.google.android.gms.checkin.EventLogService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.backup.component.FullBackupJobLoggerService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.security.safebrowsing.SafeBrowsingUpdateTaskService", "auth"},
  {"com.google.android.gms/com.google.android.gms.security.verifier.ApkUploadService", "auth"},
  {"com.google.android.gms/com.google.android.gms.security.verifier.InternalApkUploadService", "auth"},
  {"com.google.android.gms/com.google.android.gms.security.snet.SnetIdleTaskService", "auth"},
  {"com.google.android.gms/com.google.android.gms.security.snet.SnetNormalTaskService", "auth"},
  {"com.google.android.gms/com.google.android.gms.security.snet.SnetService", "auth"},
  {"com.google.android.gms/com.google.android.gms.tapandpay.security.StorageKeyCacheService", "auth"},
  {"com.google.android.gms/com.google.android.gms.droidguard.DroidGuardGcmTaskService", "auth"},
  {"com.google.android.gms/com.google.android.gms.pay.security.storagekey.service.StorageKeyCacheService", "auth"},
  {"com.google.android.gms/com.google.android.gms.tapandpay.gcmtask.TapAndPayGcmTaskService", "payment"},
  {"com.google.android.gms/com.google.android.gms.tapandpay.globalactions.QuickAccessWalletService", "wallet"},
  {"com.google.android.gms/com.google.android.gms.tapandpay.globalactions.WalletQuickAccessWalletService", "wallet"},
  {"com.google.android.gms/com.google.android.gms.pay.gcmtask.PayGcmTaskService", "payment"},
  {"com.google.android.gms/com.google.android.gms.pay.hce.service.PayHceService", "payment"},
  {"com.google.android.gms/com.google.android.gms.pay.notifications.PayNotificationService", "payment"},
  {"com.google.android.gms/com.google.android.gms.wallet.service.PaymentService", "payment"},
  {"com.google.android.gms/com.google.android.gms.wallet.service.WalletGcmTaskService", "wallet"},
  {"com.google.android.gms/com.google.android.gms.fitness.cache.DataUpdateListenerCacheService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.sensors.sample.CollectSensorService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.sync.SyncGcmTaskService", "sync"},
  {"com.google.android.gms/com.google.android.gms.games.chimera.GamesSignInIntentServiceProxy", "wear"},
  {"com.google.android.gms/com.google.android.gms.games.chimera.GamesSyncServiceNotificationProxy", "wear"},
  {"com.google.android.gms/com.google.android.gms.games.chimera.GamesUploadServiceProxy", "wear"},
  {"com.google.android.gms/com.google.android.gms.gp.gameservice.GameService", "wear"},
  {"com.google.android.gms/com.google.android.gms.gp.gameservice.GameSessionService", "wear"},
  {"com.google.android.gms/com.google.android.gms.chimera.GmsApiServiceNoInstantApps", "sync"},
  {"com.google.android.gms/com.google.android.gms.chimera.PersistentApiServiceNoInstantApps", "sync"},
  {"com.google.android.gms/com.google.android.gms.instantapps.service.InstantAppsService", "sync"},
  {"com.google.android.gms/com.google.android.gms.chimera.UiApiServiceNoInstantApps", "sync"},
  {"com.android.vending/com.google.android.fisnky.instantapps.InstantAppsLoggingService", "background"},
  {"com.google.android.gms/.instantapps.service.InstantAppsService", "background"},
  {"com.google.android.gms/com.google.android.finsky.instantapps.InstantAppsLoggingService", "background"},
  {"com.google.android.apps.work.clouddpc/.base.policy.services.ReportingPartialCollectionJobService", "background"},
  {"com.google.android.apps.work.clouddpc/.base.policy.services.StatusReportJobService", "background"},
  {"com.google.android.apps.work.clouddpc/.vanilla.bugreport.jobs.RemoteBugReportJobService", "background"},
  {"com.google.android.gms/.ads.AdRequestBrokerService", "ads"},
  {"com.google.android.gms/.ads.cache.CacheBrokerService", "ads"},
  {"com.google.android.gms/.ads.config.FlagsReceiver", "ads"},
  {"com.google.android.gms/.ads.identifier.service.AdvertisingIdNotificationService", "ads"},
  {"com.google.android.gms/.ads.identifier.service.AdvertisingIdService", "ads"},
  {"com.google.android.gms/.ads.jams.NegotiationService", "ads"},
  {"com.google.android.gms/.ads.measurement.GmpConversionTrackingBrokerService", "tracking"},
  {"com.google.android.gms/.ads.social.GcmSchedulerWakeupService", "ads"},
  {"com.google.android.gms/.adsidentity.service.AdServicesExtDataStorageService", "ads"},
  {"com.google.android.gms/.analytics.AnalyticsReceiver", "analytics"},
  {"com.google.android.gms/.analytics.AnalyticsService", "analytics"},
  {"com.google.android.gms/.analytics.AnalyticsTaskService", "analytics"},
  {"com.google.android.gms/.analytics.internal.PlayLogReportingService", "analytics"},
  {"com.google.android.gms/.analytics.service.AnalyticsService", "analytics"},
  {"com.google.android.gms/.auth.trustagent.ActiveUnlockTrustAgent", "auth"},
  {"com.google.android.gms/.auth.trustagent.GoogleTrustAgent", "wear"},
  {"com.google.android.gms/.backup.component.FullBackupJobLoggerService", "analytics"},
  {"com.google.android.gms/.backup.stats.BackupStatsService", "analytics"},
  {"com.google.android.gms/.checkin.EventLogService", "reporting"},
  {"com.google.android.gms/.chimera.GmsIntentOperationService", "auth"},
  {"com.google.android.gms/.chimera.container.logger.ExternalDebugLoggerService", "analytics"},
  {"com.google.android.gms/.common.appdoctor.LocalAppDoctorReceiver", "analytics"},
  {"com.google.android.gms/.common.stats.GmsCoreStatsService", "analytics"},
  {"com.google.android.gms/.common.stats.StatsUploadService", "analytics"},
  {"com.google.android.gms/.feedback.LegacyBugReportService", "reporting"},
  {"com.google.android.gms/.feedback.OfflineReportSendTaskService", "reporting"},
  {"com.google.android.gms/.googlehelp.metrics.ReportBatchedMetricsGcmTaskService", "reporting"},
  {"com.google.android.gms/.location.reporting.service.GcmBroadcastReceiver", "location"},
  {"com.google.android.gms/.magictether.logging.DailyMetricsLoggerService", "background"},
  {"com.google.android.gms/.measurement.AppMeasurementJobService", "analytics"},
  {"com.google.android.gms/.measurement.AppMeasurementReceiver", "analytics"},
  {"com.google.android.gms/.measurement.AppMeasurementService", "analytics"},
  {"com.google.android.gms/.measurement.PackageMeasurementReceiver", "analytics"},
  {"com.google.android.gms/.romanesco.ContactsLoggerUploadService", "analytics"},
  {"com.google.android.gms/.stats.PlatformStatsCollectorService", "analytics"},
  {"com.google.android.gms/.stats.eastworld.EastworldService", "analytics"},
  {"com.google.android.gms/.stats.service.DropBoxEntryAddedReceiver", "analytics"},
  {"com.google.android.gms/.stats.service.DropBoxEntryAddedService", "analytics"},
  {"com.google.android.gms/.tron.AlarmReceiver", "analytics"},
  {"com.google.android.gms/.tron.CollectionService", "analytics"},
  {"com.google.android.gms/.udc.gcm.GcmBroadcastReceiver", "analytics"},
  {"com.google.android.gms/.usagereporting.service.UsageReportingIntentService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.mdm.receivers.MdmDeviceAdminReceiver", "sync"},
  {"com.google.android.gms/com.google.android.gms.ads.AdRequestBrokerService", "ads"},
  {"com.google.android.gms/com.google.android.gms.ads.identifier.service.AdvertisingIdService", "ads"},
  {"com.google.android.gms/com.google.android.gms.ads.settings.AdsSettingsActivityService", "ads"},
  {"com.google.android.gms/com.google.android.gms.ads.GservicesValueBrokerService", "ads"},
  {"com.google.android.gms/com.google.android.gms.ads.measurement.GmpConversionTrackingBrokerService", "tracking"},
  {"com.google.android.gms/com.google.android.gms.ads.jams.NegotiationService", "ads"},
  {"com.google.android.gms/com.google.android.gms.ads.MobileAdsSettingManagerService", "ads"},
  {"com.google.android.gms/com.google.android.gms.ads.identifier.service.AdvertisingIdNotificationService", "ads"},
  {"com.google.android.gms/analytics.AnalyticsService", "tracking"},
  {"com.google.android.gms/com.google.android.gms.nearby.mediums.nearfieldcommunication.NfcAdvertisingService", "tracking"},
  {"com.google.android.gms/com.google.android.gms.analytics.service.AnalyticsService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.analytics.AnalyticsService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.analytics.AnalyticsTaskService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.analytics.internal.PlayLogReportingService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.backup.BackupOrchestrationService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.crash.service.CrashReportService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.common.download.DownloadWorkService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.clearcut.service.ClearcutLoggerService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.common.stats.GmsCoreStatsService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.feedback.FeedbackAsyncService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.stats.PlatformStatsCollectorService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.feedback.LegacyBugReportService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.feedback.OfflineReportSendTaskService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.googlehelp.metrics.ReportBatchedMetricsGcmTaskService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.locationsharingreporter.service.reporting.periodic.PeriodicReporterMonitoringService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.common.stats.net.NetworkReportService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.presencemanager.service.PresenceManagerPresenceReportService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.usagereporting.service.UsageReportingIntentService", "reporting"},
  {"com.google.android.gms/com.google.android.gms.stats.eastworld.EastworldService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.stats.service.DropBoxEntryAddedService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.common.stats.StatsUploadService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.backup.stats.BackupStatsService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.checkin.CheckinApiService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.checkin.CheckinService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.tron.CollectionService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.common.config.PhenotypeCheckinService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.romanesco.ContactsLoggerUploadService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.measurement.AppMeasurementService", "background"},
  {"com.google.android.gms/com.google.android.gms.gmscompliance.service.GmsComplianceService", "background"},
  {"com.google.android.gms/com.google.android.gms.common.stats.GmsCoreStatsService", "analytics"},
  {"com.google.android.gms/com.google.android.gms.mdm.services.MdmPhoneWearableListenerService", "update"},
  {"com.google.android.gms/com.google.android.gms.update.OtaSuggestionSummaryProvider", "update"},
  {"com.google.android.gms/com.google.android.gms.update.SystemUpdateService", "update"},
  {"com.google.android.gms/com.google.android.gms.auth.folsom.service.FolsomPublicKeyUpdateService", "update"},
  {"com.google.android.gms/com.google.android.gms.fonts.update.UpdateSchedulerService", "update"},
  {"com.google.android.gms/com.google.android.gms.icing.proxy.IcingInternalCorporaUpdateService", "update"},
  {"com.google.android.gms/com.google.android.gms.instantapps.routing.DomainFilterUpdateService", "update"},
  {"com.google.android.gms/com.google.android.gms.mobiledataplan.service.PeriodicUpdaterService", "update"},
  {"com.google.android.gms/com.google.android.gms.phenotype.service.sync.PackageUpdateTaskService", "update"},
  {"com.google.android.gms/com.google.android.gms.update.SystemUpdateGcmTaskService", "update"},
  {"com.google.android.gms/com.google.android.gms.update.UpdateFromSdCardService", "update"},
  {"com.google.android.gms/com.google.android.gms.magictether.logging.DailyMetricsLoggerService", "background"},
  {"com.google.android.gms/com.google.android.gms.deviceconnection.service.DeviceConnectionServiceBroker", "location"},
  {"com.google.android.gms/com.google.android.location.reporting.service.ReportingAndroidService", "location"},
  {"com.google.android.gms/com.google.android.gms.fido.fido2.pollux.CableAuthenticatorService", "location"},
  {"com.google.android.gms/com.google.android.location.internal.GoogleLocationManagerService", "location"},
  {"com.google.android.gms/com.google.android.gms.locationsharing.service.LocationSharingService", "location"},
  {"com.google.android.gms/com.google.android.gms.places.PlaceDetectionService", "location"},
  {"com.google.android.gms/com.google.android.gms.persistent.offnetwork.service.OffNetworkService", "location"},
  {"com.google.android.gms/com.google.android.gms.geofence.GeofenceApiService", "geofence"},
  {"com.google.android.gms/com.google.android.location.geofencer.service.GeofenceProviderService", "geofence"},
  {"com.google.android.gms/com.google.android.gms.geotimezone.GeoTimeZoneService", "location"},
  {"com.google.android.gms/com.google.android.gms.location.geocode.GeocodeService", "location"},
  {"com.google.android.gms/com.google.android.location.reporting.service.ReportingAndroidService", "location"},
  {"com.google.android.gms/com.google.android.location.reporting.service.ReportingSyncService", "location"},
  {"com.google.android.gms/com.google.android.location.wearable.WearableLocationService", "location"},
  {"com.google.android.gms/com.google.android.location.fused.wearable.GmsWearableListenerService", "location"},
  {"com.google.android.gms/com.google.android.gms.location.reporting.service.GcmBroadcastReceiver", "location"},
  {"com.google.android.gms/com.google.android.gms.thunderbird.EmergencyLocationService", "location"},
  {"com.google.android.gms/com.google.android.gms.thunderbird.EmergencyPersistentService", "location"},
  {"com.google.android.gms/com.google.android.gms.fitness.cache.DataUpdateListenerCacheService", "location"},
  {"com.google.android.gms/com.google.android.gms.fitness.sensors.sample.CollectSensorService", "location"},
  {"com.google.android.gms/com.google.android.gms.nearby.connection.service.NearbyConnectionsAndroidService", "nearby"},
  {"com.google.android.gms/com.google.android.gms.nearby.exposurenotification.service.ExposureNotificationService", "nearby"},
  {"com.google.android.gms/com.google.android.gms.nearby.bootstrap.service.NearbyBootstrapService", "nearby"},
  {"com.google.android.gms/com.google.android.gms.nearby.sharing.service.ShareSheetSessionService", "nearby"},
  {"com.google.android.gms/com.google.android.gms.cast.service.CastService", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.service.media.CastMediaService", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.service.session.CastSessionService", "cast"},
  {"com.google.android.gms/com.google.android.gms.mobiledataplan.service.MobileDataPlanService", "discovery"},
  {"com.google.android.gms/com.google.android.gms.nearby.messages.debug.DebugPokeService", "nearby"},
  {"com.google.android.gms/com.google.android.gms.clearcut.debug.ClearcutDebugDumpService", "nearby"},
  {"com.google.firebase.components.ComponentDiscoveryService", "discovery"},
  {"com.google.android.gms/com.google.android.gms.nearby.discovery.service.DiscoveryService", "discovery"},
  {"com.google.android.gms/com.google.mlkit.common.internal.MlKitComponentDiscoveryService", "discovery"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastMediaRoute2ProviderService", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastMediaRoute2ProviderService_Isolated", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastMediaRoute2ProviderService_Persistent", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastMediaRouteProviderService", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastMediaRouteProviderService_Isolated", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastMediaRouteProviderService_Persistent", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastRemoteDisplayProviderService", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastRemoteDisplayProviderService_Isolated", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.media.CastRemoteDisplayProviderService_Persistent", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.service.CastPersistentService_Persistent", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.service.CastSocketMultiplexerLifeCycleService", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.service.CastSocketMultiplexerLifeCycleService_Isolated", "cast"},
  {"com.google.android.gms/com.google.android.gms.cast.service.CastSocketMultiplexerLifeCycleService_Persistent", "cast"},
  {"com.google.android.gms/com.google.android.gms.chimera.CastPersistentBoundBrokerService", "cast"},
  {"com.google.android.gms/com.google.android.gms.nearby.bootstrap.service.NearbyBootstrapService", "nearby"},
  {"com.google.android.gms/com.google.android.gms.nearby.connection.service.NearbyConnectionsAndroidService", "nearby"},
  {"com.google.location.nearby.direct.service.NearbyDirectService", "nearby"},
  {"com.google.android.gms/com.google.android.gms.nearby.messages.service.NearbyMessagesService", "nearby"},
  {"com.google.android.gms/com.google.android.gms.nearby.fastpair.service.WearableDataListenerService", "nearby"},
  {"com.google.android.gms/com.google.android.gms.auth.api.credentials.credentialsapi.service.CredentialsApiService", "auth"},
  {"com.google.android.gms/com.google.android.gms.auth.api.identity.service.IdentityService", "auth"},
  {"com.google.android.gms/com.google.android.gms.auth.api.phone.service.SmsRetrieverApiService", "auth"},
  {"com.google.android.gms/com.google.android.gms.auth.api.signin.RevocationService", "auth"},
  {"com.google.android.gms/com.google.android.gms.auth.authzen.api.AuthzenAutofillSyncSetupService", "sync"},
  {"com.google.android.gms/com.google.android.gms.auth.cryptauth.cryptauthservice.CryptauthService", "auth"},
  {"com.google.android.gms/com.google.android.gms.backup.component.BackupSnapshotService", "cloud"},
  {"com.google.android.gms/com.google.android.gms.chromesync.sync.BackendMeetingPointService", "sync"},
  {"com.google.android.gms/com.google.android.gms.chimera.GmsApiServiceNoInstantApps", "sync"},
  {"com.google.android.gms/com.google.android.gms.chimera.PersistentApiServiceNoInstantApps", "sync"},
  {"com.google.android.gms/com.google.android.gms.instantapps.service.InstantAppsService", "sync"},
  {"com.google.android.gms/com.google.android.gms.chimera.UiApiServiceNoInstantApps", "sync"},
  {"com.google.android.gms/com.google.android.gms.security.safebrowsing.SafeBrowsingUpdateTaskService", "auth"},
  {"com.google.android.gms/com.google.android.gms.security.verifier.ApkUploadService", "auth"},
  {"com.google.android.gms/com.google.android.gms.security.verifier.InternalApkUploadService", "auth"},
  {"com.google.android.gms/com.google.android.gms.security.snet.SnetIdleTaskService", "auth"},
  {"com.google.android.gms/com.google.android.gms.security.snet.SnetNormalTaskService", "auth"},
  {"com.google.android.gms/com.google.android.gms.security.snet.SnetService", "auth"},
  {"com.google.android.gms/com.google.android.gms.tapandpay.security.StorageKeyCacheService", "auth"},
  {"com.google.android.gms/com.google.android.gms.droidguard.DroidGuardGcmTaskService", "auth"},
  {"com.google.android.gms/com.google.android.gms.fitness.sync.SyncGcmTaskService", "sync"},
  {"com.google.android.gms/com.google.android.gms.fitness.sync.FitnessSyncAdapterService", "sync"},
  {"com.google.android.gms/com.google.android.gms.wallet.service.PaymentsService", "payment"},
  {"com.google.android.gms/com.google.android.gms.pay.service.PayGrpcUiService", "payment"},
  {"com.google.android.gms/com.google.android.gms.wallet.notifications.WalletNotificationsService", "wallet"},
  {"com.google.android.gms/com.google.android.gms.tapandpay.gcmtask.TapAndPayGcmTaskService", "payment"},
  {"com.google.android.gms/com.google.android.gms.wallet.service.WalletGoogleServiceBroker", "wallet"},
  {"com.google.android.gms/com.google.android.gms.tapandpay.hce.service.TpHceService", "payment"},
  {"com.google.android.gms/com.google.android.gms.tapandpay.gcmtask.TapAndPayGcmTaskService", "payment"},
  {"com.google.android.gms/com.google.android.gms.tapandpay.globalactions.QuickAccessWalletService", "wallet"},
  {"com.google.android.gms/com.google.android.gms.tapandpay.globalactions.WalletQuickAccessWalletService", "wallet"},
  {"com.google.android.gms/com.google.android.gms.pay.gcmtask.PayGcmTaskService", "payment"},
  {"com.google.android.gms/com.google.android.gms.pay.hce.service.PayHceService", "payment"},
  {"com.google.android.gms/com.google.android.gms.pay.notifications.PayNotificationService", "payment"},
  {"com.google.android.gms/com.google.android.gms.wallet.service.PaymentService", "payment"},
  {"com.google.android.gms/com.google.android.gms.wallet.service.WalletGcmTaskService", "wallet"},
  {"com.google.android.gms/com.google.android.gms.tapandpay.wear.WearProxyService", "payment"},
  {"com.google.android.gms/com.google.android.gms.pay.security.storagekey.service.StorageKeyCacheService", "payment"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.FitnessSensorService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.GoogleFitnessService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.wearable.service.WearableService", "wear"},
  {"com.google.android.gms/com.google.android.gms.wearable.service.WearableControlService", "wear"},
  {"com.google.android.gms/com.google.android.gms.trustagent.AuthenticatorService", "wear"},
  {"com.google.android.gms/com.google.android.gms.wearables.settings.WearablesSyncService", "wear"},
  {"com.google.android.gms/com.google.android.gms.wearable.service.ListenerService", "wear"},
  {"com.google.android.gms/com.google.android.gms.backup.wear.BackupSettingsListenerService", "wear"},
  {"com.google.android.gms/com.google.android.gms.dck.service.DckWearableListenerService", "wear"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.wearable.WearableSyncAccountService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.wearable.WearableSyncConfigService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.wearable.WearableSyncConnectionService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.wearable.WearableSyncMessageService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.wearables.WearableSyncService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.auth.trustagent.GoogleTrustAgent", "wear"},
  {"com.google.android.gms/com.google.android.gms.trustagent.api.bridge.TrustAgentBridgeService", "wear"},
  {"com.google.android.gms/com.google.android.gms.trustagent.api.state.TrustAgentState", "wear"},
  {"com.google.android.gms/com.google.android.gms.kids.chimera.KidsServiceProxy", "wear"},
  {"com.google.android.gms/com.google.android.gms.personalsafety.service.PersonalSafetyService", "wear"},
  {"com.google.android.gms/com.google.android.gms.fitness.cache.DataUpdateListenerCacheService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.ble.FitBleBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.config.FitConfigBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.goals.FitGoalsBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.history.FitHistoryBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.internal.FitInternalBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.proxy.FitProxyBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.recording.FitRecordingBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.sensors.FitSensorsBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.service.sessions.FitSessionsBroker", "fitness"},
  {"com.google.android.gms/com.google.android.gms.fitness.sensors.sample.CollectSensorService", "fitness"},
  {"com.google.android.gms/com.google.android.gms.games.chimera.GamesSignInIntentServiceProxy", "wear"},
  {"com.google.android.gms/com.google.android.gms.games.chimera.GamesSyncServiceNotificationProxy", "wear"},
  {"com.google.android.gms/com.google.android.gms.games.chimera.GamesUploadServiceProxy", "wear"},
  {"com.google.android.gms/com.google.android.gms.gp.gameservice.GameService", "wear"},
  {"com.google.android.gms/com.google.android.gms.gp.gameservice.GameSessionService", "wear"},
  {"com.google.android.gms/com.google.android.gms.auth.account.be.AccountStatusChecker", "core"},
  {"com.google.android.gms/com.google.android.gms.auth.account.accountstatus.DefaultAccountStatusService", "core"},
  {"com.google.android.gms/com.google.android.gms.auth.be.appcert.AppCertService", "essential"},
  {"com.google.android.gms/com.google.android.gms.auth.folsom.service.FolsomService", "essential"},
  {"com.google.android.gms/com.google.android.gms.auth.managed.ui.DeviceManagementSettingsActivity", "essential"},
  {"com.google.android.gms/com.google.android.gms.auth.account.authenticator.WorkAccountAuthenticatorService", "essential"},
  {"com.google.android.gms/com.google.android.gms.auth.trustagent.ActiveUnlockTrustAgent", "essential"},
  {"com.google.android.gms/com.google.android.gms.measurement.AppMeasurementJobService", "core"},
  {"com.google.android.gms/com.google.android.gms.measurement.AppMeasurementReceiver", "core"},
  {"com.google.android.gms/com.google.android.gms.measurement.PackageMeasurementReceiver", "core"},
  {"com.google.android.gms/com.google.android.gms.mdm.receivers.MdmDeviceAdminReceiver", "essential"},
  {"com.google.android.gms/com.google.android.gms.chimera.PersistentBoundBrokerService", "core"},
  {"com.google.android.gms/com.google.android.gms.chimera.GmsApiService", "core"},
  {"com.google.android.gms/com.google.android.gms.chimera.GmsIntentOperationService", "core"},
  {"com.google.android.gms/com.google.android.gms.chimera.GmsIntentOperationService_AuthAccountIsolated", "core"},
  {"com.google.android.gms/com.google.android.gms.chimera.GmsIntentOperationService_AuthAccountIsolate", "core"},
  {"com.google.android.gms/com.google.android.gms.chimera.PersistentApiService_AuthAccountIsolated", "core"},
  {"com.google.android.gms/com.google.android.gms.chimera.PersistentIntentOperationService_AuthAccountIsolated", "core"},
  {"com.google.android.gms/com.google.android.location.fused.FusedLocationService", "core"},
  {"com.google.android.gms/com.google.android.location.internal.server.GoogleLocationService", "core"},
  {"com.google.android.gms/com.google.android.location.internal.server.HardwareArProviderService", "location"},
  {"com.google.android.gms/com.google.android.location.network.NetworkLocationService", "core"},
  {"com.google.android.gms/com.google.android.location.persistent.LocationPersistentService", "core"},
  {"com.google.android.gms/com.google.android.location.util.LocationAccuracyInjectorService", "core"},
};
#define GMS_LIST_N (sizeof(GMS_LIST) / sizeof(GMS_LIST[0]))

static int walk_gmslist(const char *conf, const char *filter_cat, int force_enable_all) {
  int fail = 0;
  for (size_t i = 0; i < GMS_LIST_N; i++) {
    const char *svc = GMS_LIST[i].svc, *cat = GMS_LIST[i].cat;
    if (filter_cat && strcmp(cat, filter_cat) != 0) continue;
    int disable = force_enable_all ? 0 : gms_should_disable(conf, cat);
    char *argv[] = { (char *)"/system/bin/pm", disable ? (char *)"disable" : (char *)"enable", (char *)svc, NULL };
    if (mt_run(argv, 1) != 0) fail++;
  }
  return fail;
}

static void droidguard(int on) {
  const char *svcs[] = {
    "com.google.android.gms/com.google.android.gms.droidguard.DroidGuardService",
    "com.google.android.gms/com.google.android.gms.droidguard.DroidGuardGcmTaskService",
    NULL
  };
  for (int i = 0; svcs[i]; i++) pm_toggle(svcs[i], on);
  mt_log(1, on ? "droidguard: disabled (breaks SafetyNet/Play Integrity - banking, Wallet, Play Store checks)"
               : "droidguard: re-enabled");
}

static const char *GMS_LOG_KEYS[] = {
  "gmscorestat_enabled","play_store_panel_logging_enabled","clearcut_events","clearcut_gcm",
  "ga_collection_enabled","clearcut_enabled","analytics_enabled","uploading_enabled",
  "bug_report_in_power_menu","usage_stats_enabled","usagestats_collection_enabled", NULL
};

static void settings_put(const char *ns, const char *key, const char *val) {
  char *argv[] = { (char *)"/system/bin/settings", (char *)"put", (char *)ns, (char *)key, (char *)val, NULL };
  mt_run(argv, 1);
}
static void settings_delete(const char *ns, const char *key) {
  char *argv[] = { (char *)"/system/bin/settings", (char *)"delete", (char *)ns, (char *)key, NULL };
  mt_run(argv, 1);
}

static void gms_log_apply(void) {
  for (int i = 0; GMS_LOG_KEYS[i]; i++) settings_put("global", GMS_LOG_KEYS[i], "0");
  settings_put("global", "phenotype__debug_bypass_phenotype", "1");
  settings_put("global", "phenotype_boot_count", "99");
  settings_put("global", "phenotype_flags", "disable_log_upload=1,disable_log_for_missing_debug_id=1");
  mt_log(1, "gms_log_disable: applied");
}
static void gms_log_revert(void) {
  for (int i = 0; GMS_LOG_KEYS[i]; i++) settings_delete("global", GMS_LOG_KEYS[i]);
  settings_delete("global", "phenotype__debug_bypass_phenotype");
  settings_delete("global", "phenotype_boot_count");
  settings_delete("global", "phenotype_flags");
  mt_log(1, "gms_log_disable: reverted");
}

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: gms <conf> <track> early|late|set KEY VALUE|category CAT VALUE\n"); return 2; }
  const char *conf = argv[1], *mode = argv[3];
  

  if (strcmp(mode, "early") == 0) return 0; 

  if (strcmp(mode, "late") == 0) {
    if (mt_is_on(conf, "GMS_MASTER")) {
      int fail = walk_gmslist(conf, NULL, 0);
      if (fail) mt_log(3, "gms_services: %d service(s) failed to apply", fail);
      mt_log(1, "gms_services: applied per category config");
    } else {
      walk_gmslist(conf, NULL, 1); 
    }
    if (mt_is_on(conf, "DISABLE_DROIDGUARD")) droidguard(1);
    if (mt_is_on(conf, "GMS_LOG_DISABLE")) gms_log_apply();
    return 0;
  }

  if (strcmp(mode, "category") == 0) {
    if (argc < 6) { fprintf(stderr, "usage: gms <conf> <track> category CAT VALUE\n"); return 2; }
    const char *cat = argv[4], *val = argv[5];
    if (!mt_is_on(conf, "GMS_MASTER")) return 0;
    int disable = strcmp(val, "1") == 0;
    int fail = 0;
    for (size_t i = 0; i < GMS_LIST_N; i++) {
      if (strcmp(GMS_LIST[i].cat, cat) != 0) continue;
      char *a[] = { (char *)"/system/bin/pm", disable ? (char *)"disable" : (char *)"enable", (char *)GMS_LIST[i].svc, NULL };
      if (mt_run(a, 1) != 0) fail++;
    }
    if (fail) mt_log(3, "gms category %s: %d service(s) failed to apply", cat, fail);
    mt_log(1, "gms category %s: %s", cat, disable ? "disabled" : "enabled");
    return 0;
  }

  if (strcmp(mode, "set") == 0) {
    if (argc < 6) { fprintf(stderr, "usage: gms <conf> <track> set KEY VALUE\n"); return 2; }
    const char *key = argv[4], *val = argv[5];
    int on = strcmp(val, "1") == 0;
    if (strcmp(key, "GMS_MASTER") == 0) {
      if (on) { int fail = walk_gmslist(conf, NULL, 0); if (fail) mt_log(3, "gms_services: %d service(s) failed to apply", fail); mt_log(1, "gms_services: applied per category config"); }
      else { walk_gmslist(conf, NULL, 1); mt_log(1, "gms_services: all re-enabled (GMS_MASTER off)"); }
    } else if (strcmp(key, "DISABLE_DROIDGUARD") == 0) {
      droidguard(on);
    } else if (strcmp(key, "GMS_LOG_DISABLE") == 0) {
      if (on) gms_log_apply(); else gms_log_revert();
    } else {
      mt_log(2, "gms: unknown key %s", key);
      return 1;
    }
    return 0;
  }

  fprintf(stderr, "gms: unknown mode %s\n", mode);
  return 2;
}
