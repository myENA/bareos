/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Kern Sibbald, October MMI
 */
/**
 * @file
 * User Agent Prompt and Selection code
 */

#include "bareos.h"
#include "dird.h"

/* Imported variables */
extern struct s_jt jobtypes[];
extern struct s_jl joblevels[];

/**
 * Confirm a retention period
 */
bool confirm_retention(UAContext *ua, utime_t *ret, const char *msg)
{
   bool retval;
   char ed1[100];
   int yes_in_arg;

   yes_in_arg = find_arg(ua, NT_("yes"));
   for ( ;; ) {
       ua->info_msg(_("The current %s retention period is: %s\n"),
                    msg, edit_utime(*ret, ed1, sizeof(ed1)));
       if (yes_in_arg != -1) {
          return true;
       }

       if (!get_cmd(ua, _("Continue? (yes/mod/no): "))) {
          return false;
       }

       if (bstrcasecmp(ua->cmd, _("mod"))) {
          if (!get_cmd(ua, _("Enter new retention period: "))) {
             return false;
          }
          if (!duration_to_utime(ua->cmd, ret)) {
             ua->error_msg(_("Invalid period.\n"));
             continue;
          }
          continue;
       }

       if (is_yesno(ua->cmd, &retval)) {
          return retval;
       }
    }

    return true;
}

/**
 * Given a list of keywords, find the first one that is in the argument list.
 *
 * Returns: -1 if not found
 *          index into list (base 0) on success
 */
int find_arg_keyword(UAContext *ua, const char **list)
{
   for (int i = 1; i < ua->argc; i++) {
      for (int j = 0; list[j]; j++) {
         if (bstrcasecmp(list[j], ua->argk[i])) {
            return j;
         }
      }
   }

   return -1;
}

/**
 * Given one keyword, find the first one that is in the argument list.
 *
 * Returns: argk index (always gt 0)
 *          -1 if not found
 */
int find_arg(UAContext *ua, const char *keyword)
{
   for (int i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(keyword, ua->argk[i])) {
         return i;
      }
   }

   return -1;
}

/**
 * Given a single keyword, find it in the argument list, but it must have a value
 *
 * Returns: -1 if not found or no value
 *           list index (base 0) on success
 */
int find_arg_with_value(UAContext *ua, const char *keyword)
{
   for (int i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(keyword, ua->argk[i])) {
         if (ua->argv[i]) {
            return i;
         } else {
            return -1;
         }
      }
   }

   return -1;
}

/**
 * Given a list of keywords, prompt the user to choose one.
 *
 * Returns: -1 on failure
 *          index into list (base 0) on success
 */
int do_keyword_prompt(UAContext *ua, const char *msg, const char **list)
{
   start_prompt(ua, _("You have the following choices:\n"));
   for (int i = 0; list[i]; i++) {
      add_prompt(ua, list[i]);
   }

   return do_prompt(ua, "", msg, NULL, 0);
}

/**
 * Select a Storage resource from prompt list
 */
STORERES *select_storage_resource(UAContext *ua, bool autochanger_only)
{
   STORERES *store;
   char name[MAX_NAME_LENGTH];

   if (autochanger_only) {
      start_prompt(ua, _("The defined Autochanger Storage resources are:\n"));
   } else {
      start_prompt(ua, _("The defined Storage resources are:\n"));
   }

   LockRes();
   foreach_res(store, R_STORAGE) {
      if (ua->acl_access_ok(Storage_ACL, store->name())) {
         if (autochanger_only && !store->autochanger) {
            continue;
         } else {
            add_prompt(ua, store->name());
         }
      }
   }
   UnlockRes();

   if (do_prompt(ua, _("Storage"),  _("Select Storage resource"), name, sizeof(name)) < 0) {
      return NULL;
   }
   store = ua->GetStoreResWithName(name);

   return store;
}

/**
 * Select a FileSet resource from prompt list
 */
FILESETRES *select_fileset_resource(UAContext *ua)
{
   FILESETRES *fs;
   char name[MAX_NAME_LENGTH];

   start_prompt(ua, _("The defined FileSet resources are:\n"));

   LockRes();
   foreach_res(fs, R_FILESET) {
      if (ua->acl_access_ok(FileSet_ACL, fs->name())) {
         add_prompt(ua, fs->name());
      }
   }
   UnlockRes();

   if (do_prompt(ua, _("FileSet"), _("Select FileSet resource"), name, sizeof(name)) < 0) {
      return NULL;
   }

   fs = ua->GetFileSetResWithName(name);

   return fs;
}

/**
 * Get a catalog resource from prompt list
 */
CATRES *get_catalog_resource(UAContext *ua)
{
   CATRES *catalog = NULL;
   char name[MAX_NAME_LENGTH];

   for (int i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], NT_("catalog")) && ua->argv[i]) {
         catalog = ua->GetCatalogResWithName(ua->argv[i]);
         if (catalog) {
            break;
         }
      }
   }

   if (ua->gui && !catalog) {
      LockRes();
      catalog = (CATRES *)GetNextRes(R_CATALOG, NULL);
      UnlockRes();

      if (!catalog) {
         ua->error_msg(_("Could not find a Catalog resource\n"));
         return NULL;
      } else if (!ua->acl_access_ok(Catalog_ACL, catalog->name())) {
         ua->error_msg(_("You must specify a \"use <catalog-name>\" command before continuing.\n"));
         return NULL;
      }

      return catalog;
   }

   if (!catalog) {
      start_prompt(ua, _("The defined Catalog resources are:\n"));

      LockRes();
      foreach_res(catalog, R_CATALOG) {
         if (ua->acl_access_ok(Catalog_ACL, catalog->name())) {
            add_prompt(ua, catalog->name());
         }
      }
      UnlockRes();

      if (do_prompt(ua, _("Catalog"),  _("Select Catalog resource"), name, sizeof(name)) < 0) {
         return NULL;
      }

      catalog = ua->GetCatalogResWithName(name);
   }

   return catalog;
}

/**
 * Select a job to enable or disable
 */
JOBRES *select_enable_disable_job_resource(UAContext *ua, bool enable)
{
   JOBRES *job;
   char name[MAX_NAME_LENGTH];

   start_prompt(ua, _("The defined Job resources are:\n"));

   LockRes();
   foreach_res(job, R_JOB) {
      if (!ua->acl_access_ok(Job_ACL, job->name())) {
         continue;
      }
      if (job->enabled == enable) {   /* Already enabled/disabled? */
         continue;                    /* yes, skip */
      }
      add_prompt(ua, job->name());
   }
   UnlockRes();

   if (do_prompt(ua, _("Job"), _("Select Job resource"), name, sizeof(name)) < 0) {
      return NULL;
   }

   job = ua->GetJobResWithName(name);

   return job;
}

/**
 * Select a Job resource from prompt list
 */
JOBRES *select_job_resource(UAContext *ua)
{
   JOBRES *job;
   char name[MAX_NAME_LENGTH];

   start_prompt(ua, _("The defined Job resources are:\n"));

   LockRes();
   foreach_res(job, R_JOB) {
      if (ua->acl_access_ok(Job_ACL, job->name())) {
         add_prompt(ua, job->name());
      }
   }
   UnlockRes();

   if (do_prompt(ua, _("Job"), _("Select Job resource"), name, sizeof(name)) < 0) {
      return NULL;
   }

   job = ua->GetJobResWithName(name);

   return job;
}

/**
 * Select a Restore Job resource from argument or prompt
 */
JOBRES *get_restore_job(UAContext *ua)
{
   int i;
   JOBRES *job;

   i = find_arg_with_value(ua, NT_("restorejob"));
   if (i >= 0) {
      job = ua->GetJobResWithName(ua->argv[i]);
      if (job && job->JobType == JT_RESTORE) {
         return job;
      }
      ua->error_msg(_("Error: Restore Job resource \"%s\" does not exist.\n"), ua->argv[i]);
   }

   return select_restore_job_resource(ua);
}

/**
 * Select a Restore Job resource from prompt list
 */
JOBRES *select_restore_job_resource(UAContext *ua)
{
   JOBRES *job;
   char name[MAX_NAME_LENGTH];

   start_prompt(ua, _("The defined Restore Job resources are:\n"));

   LockRes();
   foreach_res(job, R_JOB) {
      if (job->JobType == JT_RESTORE && ua->acl_access_ok(Job_ACL, job->name())) {
         add_prompt(ua, job->name());
      }
   }
   UnlockRes();

   if (do_prompt(ua, _("Job"), _("Select Restore Job"), name, sizeof(name)) < 0) {
      return NULL;
   }

   job = ua->GetJobResWithName(name);

   return job;
}

/**
 * Select a client resource from prompt list
 */
CLIENTRES *select_client_resource(UAContext *ua)
{
   CLIENTRES *client;
   char name[MAX_NAME_LENGTH];

   start_prompt(ua, _("The defined Client resources are:\n"));

   LockRes();
   foreach_res(client, R_CLIENT) {
      if (ua->acl_access_ok(Client_ACL, client->name())) {
         add_prompt(ua, client->name());
      }
   }
   UnlockRes();

   if (do_prompt(ua, _("Client"),  _("Select Client (File daemon) resource"), name, sizeof(name)) < 0) {
      return NULL;
   }

   client = ua->GetClientResWithName(name);

   return client;
}

/**
 * Select a client to enable or disable
 */
CLIENTRES *select_enable_disable_client_resource(UAContext *ua, bool enable)
{
   CLIENTRES *client;
   char name[MAX_NAME_LENGTH];

   start_prompt(ua, _("The defined Client resources are:\n"));

   LockRes();
   foreach_res(client, R_CLIENT) {
      if (!ua->acl_access_ok(Client_ACL, client->name())) {
         continue;
      }
      if (client->enabled == enable) {   /* Already enabled/disabled? */
         continue;                    /* yes, skip */
      }
      add_prompt(ua, client->name());
   }
   UnlockRes();

   if (do_prompt(ua, _("Client"), _("Select Client resource"), name, sizeof(name)) < 0) {
      return NULL;
   }

   client = ua->GetClientResWithName(name);

   return client;
}

/**
 *  Get client resource, start by looking for
 *   client=<client-name>
 *  if we don't find the keyword, we prompt the user.
 */
CLIENTRES *get_client_resource(UAContext *ua)
{
   CLIENTRES *client = NULL;

   for (int i = 1; i < ua->argc; i++) {
      if ((bstrcasecmp(ua->argk[i], NT_("client")) ||
           bstrcasecmp(ua->argk[i], NT_("fd"))) && ua->argv[i]) {
         client = ua->GetClientResWithName(ua->argv[i]);
         if (client) {
            return client;
         }

         ua->error_msg(_("Error: Client resource %s does not exist.\n"), ua->argv[i]);

         break;
      }
   }

   return select_client_resource(ua);
}

/**
 * Select a schedule to enable or disable
 */
SCHEDRES *select_enable_disable_schedule_resource(UAContext *ua, bool enable)
{
   SCHEDRES *sched;
   char name[MAX_NAME_LENGTH];

   start_prompt(ua, _("The defined Schedule resources are:\n"));

   LockRes();
   foreach_res(sched, R_SCHEDULE) {
      if (!ua->acl_access_ok(Schedule_ACL, sched->name())) {
         continue;
      }
      if (sched->enabled == enable) {   /* Already enabled/disabled? */
         continue;                    /* yes, skip */
      }
      add_prompt(ua, sched->name());
   }
   UnlockRes();

   if (do_prompt(ua, _("Schedule"), _("Select Schedule resource"), name, sizeof(name)) < 0) {
      return NULL;
   }

   sched = ua->GetScheduleResWithName(name);

   return sched;
}

/**
 * Scan what the user has entered looking for:
 *
 * client=<client-name>
 *
 * If error or not found, put up a list of client DBRs to choose from.
 *
 * returns: false on error
 *          true on success and fills in CLIENT_DBR
 */
bool get_client_dbr(UAContext *ua, CLIENT_DBR *cr)
{
   if (cr->Name[0]) {                 /* If name already supplied */
      if (ua->db->get_client_record(ua->jcr, cr)) {
         return true;
      }
      ua->error_msg(_("Could not find Client %s: ERR=%s"), cr->Name, ua->db->strerror());
   }

   for (int i = 1; i < ua->argc; i++) {
      if ((bstrcasecmp(ua->argk[i], NT_("client")) ||
           bstrcasecmp(ua->argk[i], NT_("fd"))) && ua->argv[i]) {
         if (!ua->acl_access_ok(Client_ACL, ua->argv[i])) {
            break;
         }
         bstrncpy(cr->Name, ua->argv[i], sizeof(cr->Name));
         if (!ua->db->get_client_record(ua->jcr, cr)) {
            ua->error_msg(_("Could not find Client \"%s\": ERR=%s"), ua->argv[i],
                     ua->db->strerror());
            cr->ClientId = 0;
            break;
         }
         return true;
      }
   }

   if (!select_client_dbr(ua, cr)) {  /* try once more by proposing a list */
      return false;
   }

   return true;
}

/**
 * Select a Client record from the catalog
 *
 * Returns true on success
 *         false on failure
 */
bool select_client_dbr(UAContext *ua, CLIENT_DBR *cr)
{
   DBId_t *ids;
   CLIENT_DBR ocr;
   int num_clients;
   char name[MAX_NAME_LENGTH];

   cr->ClientId = 0;
   if (!ua->db->get_client_ids(ua->jcr, &num_clients, &ids)) {
      ua->error_msg(_("Error obtaining client ids. ERR=%s\n"), ua->db->strerror());
      return false;
   }

   if (num_clients <= 0) {
      ua->error_msg(_("No clients defined. You must run a job before using this command.\n"));
      return false;
   }

   start_prompt(ua, _("Defined Clients:\n"));
   for (int i = 0; i < num_clients; i++) {
      ocr.ClientId = ids[i];
      if (!ua->db->get_client_record(ua->jcr, &ocr) ||
          !ua->acl_access_ok(Client_ACL, ocr.Name)) {
         continue;
      }
      add_prompt(ua, ocr.Name);
   }
   free(ids);

   if (do_prompt(ua, _("Client"),  _("Select the Client"), name, sizeof(name)) < 0) {
      return false;
   }

   memset(&ocr, 0, sizeof(ocr));
   bstrncpy(ocr.Name, name, sizeof(ocr.Name));

   if (!ua->db->get_client_record(ua->jcr, &ocr)) {
      ua->error_msg(_("Could not find Client \"%s\": ERR=%s"), name, ua->db->strerror());
      return false;
   }

   memcpy(cr, &ocr, sizeof(ocr));

   return true;
}

/**
 * Scan what the user has entered looking for:
 *
 * argk=<storage-name>
 *
 * where argk can be : storage
 *
 * If error or not found, put up a list of storage DBRs to choose from.
 *
 * returns: false on error
 *          true  on success and fills in STORAGE_DBR
 */
bool get_storage_dbr(UAContext *ua, STORAGE_DBR *sr, const char *argk)
{
   if (sr->Name[0]) {                 /* If name already supplied */
      if (ua->db->get_storage_record(ua->jcr, sr) &&
          ua->acl_access_ok(Pool_ACL, sr->Name)) {
         return true;
      }
      ua->error_msg(_("Could not find Storage \"%s\": ERR=%s"), sr->Name, ua->db->strerror());
   }

   if (!select_storage_dbr(ua, sr, argk)) {  /* try once more */
      return false;
   }

   return true;
}

/**
 * Scan what the user has entered looking for:
 *
 * argk=<pool-name>
 *
 * where argk can be : pool, recyclepool, scratchpool, nextpool etc..
 *
 * If error or not found, put up a list of pool DBRs to choose from.
 *
 * returns: false on error
 *          true  on success and fills in POOL_DBR
 */
bool get_pool_dbr(UAContext *ua, POOL_DBR *pr, const char *argk)
{
   if (pr->Name[0]) {                 /* If name already supplied */
      if (ua->db->get_pool_record(ua->jcr, pr) &&
          ua->acl_access_ok(Pool_ACL, pr->Name)) {
         return true;
      }
      ua->error_msg(_("Could not find Pool \"%s\": ERR=%s"), pr->Name, ua->db->strerror());
   }

   if (!select_pool_dbr(ua, pr, argk)) {  /* try once more */
      return false;
   }

   return true;
}

/**
 * Select a Pool record from catalog
 * argk can be pool, recyclepool, scratchpool etc..
 */
bool select_pool_dbr(UAContext *ua, POOL_DBR *pr, const char *argk)
{
   POOL_DBR opr;
   DBId_t *ids;
   int num_pools;
   char name[MAX_NAME_LENGTH];

   for (int i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], argk) && ua->argv[i] &&
          ua->acl_access_ok(Pool_ACL, ua->argv[i])) {
         bstrncpy(pr->Name, ua->argv[i], sizeof(pr->Name));
         if (!ua->db->get_pool_record(ua->jcr, pr)) {
            ua->error_msg(_("Could not find Pool \"%s\": ERR=%s"), ua->argv[i], ua->db->strerror());
            pr->PoolId = 0;
            break;
         }
         return true;
      }
   }

   pr->PoolId = 0;
   if (!ua->db->get_pool_ids(ua->jcr, &num_pools, &ids)) {
      ua->error_msg(_("Error obtaining pool ids. ERR=%s\n"), ua->db->strerror());
      return 0;
   }

   if (num_pools <= 0) {
      ua->error_msg(_("No pools defined. Use the \"create\" command to create one.\n"));
      return false;
   }

   start_prompt(ua, _("Defined Pools:\n"));
   if (bstrcmp(argk, NT_("recyclepool"))) {
      add_prompt(ua, _("*None*"));
   }

   for (int i = 0; i < num_pools; i++) {
      opr.PoolId = ids[i];
      if (!ua->db->get_pool_record(ua->jcr, &opr) ||
          !ua->acl_access_ok(Pool_ACL, opr.Name)) {
         continue;
      }
      add_prompt(ua, opr.Name);
   }
   free(ids);

   if (do_prompt(ua, _("Pool"),  _("Select the Pool"), name, sizeof(name)) < 0) {
      return false;
   }

   memset(&opr, 0, sizeof(opr));

   /*
    * *None* is only returned when selecting a recyclepool, and in that case
    * the calling code is only interested in opr.Name, so then we can leave
    * pr as all zero.
    */
   if (!bstrcmp(name, _("*None*"))) {
     bstrncpy(opr.Name, name, sizeof(opr.Name));

     if (!ua->db->get_pool_record(ua->jcr, &opr)) {
        ua->error_msg(_("Could not find Pool \"%s\": ERR=%s"), name, ua->db->strerror());
        return false;
     }
   }

   memcpy(pr, &opr, sizeof(opr));

   return true;
}

/**
 * Select a Pool and a Media (Volume) record from the database
 */
bool select_pool_and_media_dbr(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr)
{
   if (!select_media_dbr(ua, mr)) {
      return false;
   }

   memset(pr, 0, sizeof(POOL_DBR));
   pr->PoolId = mr->PoolId;
   if (!ua->db->get_pool_record(ua->jcr, pr)) {
      ua->error_msg("%s", ua->db->strerror());
      return false;
   }

   if (!ua->acl_access_ok(Pool_ACL, pr->Name, true)) {
      ua->error_msg(_("No access to Pool \"%s\"\n"), pr->Name);
      return false;
   }

   return true;
}

/**
 * Select a Storage record from catalog
 * argk can be storage
 */
bool select_storage_dbr(UAContext *ua, STORAGE_DBR *sr, const char *argk)
{
   STORAGE_DBR osr;
   DBId_t *ids;
   int num_storages;
   char name[MAX_NAME_LENGTH];

   for (int i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], argk) && ua->argv[i] &&
          ua->acl_access_ok(Storage_ACL, ua->argv[i])) {
         bstrncpy(sr->Name, ua->argv[i], sizeof(sr->Name));
         if (!ua->db->get_storage_record(ua->jcr, sr)) {
            ua->error_msg(_("Could not find Storage \"%s\": ERR=%s"), ua->argv[i], ua->db->strerror());
            sr->StorageId = 0;
            break;
         }
         return true;
      }
   }

   sr->StorageId = 0;
   if (!ua->db->get_storage_ids(ua->jcr, &num_storages, &ids)) {
      ua->error_msg(_("Error obtaining storage ids. ERR=%s\n"), ua->db->strerror());
      return 0;
   }

   if (num_storages <= 0) {
      ua->error_msg(_("No storages defined.\n"));
      return false;
   }

   start_prompt(ua, _("Defined Storages:\n"));
   if (bstrcmp(argk, NT_("recyclestorage"))) {
      add_prompt(ua, _("*None*"));
   }

   for (int i = 0; i < num_storages; i++) {
      osr.StorageId = ids[i];
      if (!ua->db->get_storage_record(ua->jcr, &osr) ||
          !ua->acl_access_ok(Storage_ACL, osr.Name)) {
         continue;
      }
      add_prompt(ua, osr.Name);
   }
   free(ids);

   if (do_prompt(ua, _("Storage"),  _("Select the Storage"), name, sizeof(name)) < 0) {
      return false;
   }

   memset(&osr, 0, sizeof(osr));

   /*
    * *None* is only returned when selecting a recyclestorage, and in that case
    * the calling code is only interested in osr.Name, so then we can leave
    * sr as all zero.
    */
   if (!bstrcmp(name, _("*None*"))) {
     bstrncpy(osr.Name, name, sizeof(osr.Name));

     if (!ua->db->get_storage_record(ua->jcr, &osr)) {
        ua->error_msg(_("Could not find Storage \"%s\": ERR=%s"), name, ua->db->strerror());
        return false;
     }
   }

   memcpy(sr, &osr, sizeof(osr));

   return true;
}

/**
 * Select a Media (Volume) record from the database
 */
bool select_media_dbr(UAContext *ua, MEDIA_DBR *mr)
{
   int i;
   int retval = false;
   POOLMEM *err = get_pool_memory(PM_FNAME);

   *err = 0;
   memset(mr, 0, sizeof(MEDIA_DBR));
   i = find_arg_with_value(ua, NT_("volume"));
   if (i >= 0) {
      if (is_name_valid(ua->argv[i], err)) {
         bstrncpy(mr->VolumeName, ua->argv[i], sizeof(mr->VolumeName));
      } else {
         goto bail_out;
      }
   }

   if (mr->VolumeName[0] == 0) {
      POOL_DBR pr;

      memset(&pr, 0, sizeof(pr));
      /*
       * Get the pool from pool=<pool-name>
       */
      if (!get_pool_dbr(ua, &pr)) {
         goto bail_out;
      }

      mr->PoolId = pr.PoolId;
      ua->db->list_media_records(ua->jcr, mr, ua->send, HORZ_LIST);

      if (!get_cmd(ua, _("Enter *MediaId or Volume name: "))) {
         goto bail_out;
      }

      if (ua->cmd[0] == '*' && is_a_number(ua->cmd+1)) {
         mr->MediaId = str_to_int64(ua->cmd+1);
      } else if (is_name_valid(ua->cmd, err)) {
         bstrncpy(mr->VolumeName, ua->cmd, sizeof(mr->VolumeName));
      } else {
         goto bail_out;
      }
   }

   if (!ua->db->get_media_record(ua->jcr, mr)) {
      pm_strcpy(err, ua->db->strerror());
      goto bail_out;
   }
   retval = true;

bail_out:
   if (!retval && *err) {
      ua->error_msg("%s", err);
   }
   free_pool_memory(err);

   return retval;
}

/**
 * Select a pool resource from prompt list
 */
POOLRES *select_pool_resource(UAContext *ua)
{
   POOLRES *pool;
   char name[MAX_NAME_LENGTH];

   start_prompt(ua, _("The defined Pool resources are:\n"));
   LockRes();
   foreach_res(pool, R_POOL) {
      if (ua->acl_access_ok(Pool_ACL, pool->name())) {
         add_prompt(ua, pool->name());
      }
   }
   UnlockRes();

   if (do_prompt(ua, _("Pool"), _("Select Pool resource"), name, sizeof(name)) < 0) {
      return NULL;
   }

   pool = ua->GetPoolResWithName(name);

   return pool;
}

/**
 *  If you are thinking about using it, you
 *  probably want to use select_pool_dbr()
 *  or get_pool_dbr() above.
 */
POOLRES *get_pool_resource(UAContext *ua)
{
   int i;
   POOLRES *pool = NULL;

   i = find_arg_with_value(ua, NT_("pool"));
   if (i >= 0 && ua->acl_access_ok(Pool_ACL, ua->argv[i])) {
      pool = ua->GetPoolResWithName(ua->argv[i]);
      if (pool) {
         return pool;
      }
      ua->error_msg(_("Error: Pool resource \"%s\" does not exist.\n"), ua->argv[i]);
   }

   return select_pool_resource(ua);
}

/**
 * List all jobs and ask user to select one
 */
int select_job_dbr(UAContext *ua, JOB_DBR *jr)
{
   ua->db->list_job_records(ua->jcr, jr, "", NULL, 0, 0, NULL, 0, 0, 0, ua->send, HORZ_LIST);
   if (!get_pint(ua, _("Enter the JobId to select: "))) {
      return 0;
   }

   jr->JobId = ua->int64_val;
   if (!ua->db->get_job_record(ua->jcr, jr)) {
      ua->error_msg("%s", ua->db->strerror());
      return 0;
   }

   return jr->JobId;
}

/**
 * Scan what the user has entered looking for:
 *
 * jobid=nn
 *
 * if error or not found, put up a list of Jobs
 * to choose from.
 *
 * returns: 0 on error
 *          JobId on success and fills in JOB_DBR
 */
int get_job_dbr(UAContext *ua, JOB_DBR *jr)
{
   int i;

   for (i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], NT_("ujobid")) && ua->argv[i]) {
         jr->JobId = 0;
         bstrncpy(jr->Job, ua->argv[i], sizeof(jr->Job));
      } else if (bstrcasecmp(ua->argk[i], NT_("jobid")) && ua->argv[i]) {
         jr->JobId = str_to_int64(ua->argv[i]);
         jr->Job[0] = 0;
      } else {
         continue;
      }
      if (!ua->db->get_job_record(ua->jcr, jr)) {
         ua->error_msg(_("Could not find Job \"%s\": ERR=%s"), ua->argv[i],
                  ua->db->strerror());
         jr->JobId = 0;
         break;
      }
      return jr->JobId;
   }

   jr->JobId = 0;
   jr->Job[0] = 0;

   for (i = 1; i < ua->argc; i++) {
      if ((bstrcasecmp(ua->argk[i], NT_("jobname")) ||
           bstrcasecmp(ua->argk[i], NT_("job"))) && ua->argv[i]) {
         jr->JobId = 0;
         bstrncpy(jr->Name, ua->argv[i], sizeof(jr->Name));
         break;
      }
   }

   if (!select_job_dbr(ua, jr)) {  /* try once more */
      return 0;
   }

   return jr->JobId;
}

/**
 * Implement unique set of prompts
 */
void start_prompt(UAContext *ua, const char *msg)
{
  if (ua->max_prompts == 0) {
     ua->max_prompts = 10;
     ua->prompt = (char **)bmalloc(sizeof(char *) * ua->max_prompts);
  }
  ua->num_prompts = 1;
  ua->prompt[0] = bstrdup(msg);
}

/**
 * Add to prompts -- keeping them unique
 */
void add_prompt(UAContext *ua, const char *prompt)
{
   if (ua->num_prompts == ua->max_prompts) {
      ua->max_prompts *= 2;
      ua->prompt = (char **)brealloc(ua->prompt, sizeof(char *) *
         ua->max_prompts);
   }

   for (int i = 1; i < ua->num_prompts; i++) {
      if (bstrcmp(ua->prompt[i], prompt)) {
         return;
      }
   }

   ua->prompt[ua->num_prompts++] = bstrdup(prompt);
}

/**
 * Display prompts and get user's choice
 *
 * Returns: -1 on error
 *           index base 0 on success, and choice is copied to prompt if not NULL
 *           prompt is set to the chosen prompt item string
 */
int do_prompt(UAContext *ua, const char *automsg, const char *msg, char *prompt, int max_prompt)
{
   int item;
   POOL_MEM pmsg(PM_MESSAGE);
   BSOCK *user = ua->UA_sock;

   if (prompt) {
      *prompt = 0;
   }
   if (ua->num_prompts == 2) {
      item = 1;
      if (prompt) {
         bstrncpy(prompt, ua->prompt[1], max_prompt);
      }
      if (!ua->api) {
         ua->send_msg(_("Automatically selected %s: %s\n"), automsg, ua->prompt[1]);
      }
      goto done;
   }

   /*
    * If running non-interactive, bail out
    */
   if (ua->batch) {
      /*
       * First print the choices he wanted to make
       */
      ua->send_msg(ua->prompt[0]);
      for (int i = 1; i < ua->num_prompts; i++) {
         ua->send_msg("%6d: %s\n", i, ua->prompt[i]);
      }

      /*
       * Now print error message
       */
      ua->send_msg(_("Your request has multiple choices for \"%s\". Selection is not possible in batch mode.\n"), automsg);
      item = -1;
      goto done;
   }

   if (ua->api) {
      user->signal(BNET_START_SELECT);
   }

   ua->send_msg(ua->prompt[0]);
   for (int i = 1; i < ua->num_prompts; i++) {
      if (ua->api) {
         ua->send_msg("%s", ua->prompt[i]);
      } else {
         ua->send_msg("%6d: %s\n", i, ua->prompt[i]);
      }
   }

   if (ua->api) {
      user->signal(BNET_END_SELECT);
   }

   while (1) {
      /*
       * First item is the prompt string, not the items
       */
      if (ua->num_prompts == 1) {
         ua->error_msg(_("Selection list for \"%s\" is empty!\n"), automsg);
         item = -1;                    /* list is empty ! */
         break;
      }
      if (ua->num_prompts == 2) {
         item = 1;
         ua->send_msg(_("Automatically selected: %s\n"), ua->prompt[1]);
         if (prompt) {
            bstrncpy(prompt, ua->prompt[1], max_prompt);
         }
         break;
      } else {
         Mmsg(pmsg, "%s (1-%d): ", msg, ua->num_prompts - 1);
      }

      /*
       * Either a . or an @ will get you out of the loop
       */
      if (ua->api) {
         user->signal(BNET_SELECT_INPUT);
      }

      if (!get_pint(ua, pmsg.c_str())) {
         item = -1;                   /* error */
         ua->info_msg(_("Selection aborted, nothing done.\n"));
         break;
      }
      item = ua->pint32_val;
      if (item < 1 || item >= ua->num_prompts) {
         ua->warning_msg(_("Please enter a number between 1 and %d\n"), ua->num_prompts-1);
         continue;
      }
      if (prompt) {
         bstrncpy(prompt, ua->prompt[item], max_prompt);
      }
      break;
   }

done:
   for (int i = 0; i < ua->num_prompts; i++) {
      free(ua->prompt[i]);
   }
   ua->num_prompts = 0;

   return (item > 0) ? (item - 1) : item;
}

/**
 * We scan what the user has entered looking for :
 * - storage=<storage-resource>
 * - job=<job_name>
 * - jobid=<jobid>
 * - ?              (prompt him with storage list)
 * - <some-error>   (prompt him with storage list)
 *
 * If use_default is set, we assume that any keyword without a value
 * is the name of the Storage resource wanted.
 *
 * If autochangers_only is given, we limit the output to autochangers only.
 */
STORERES *get_storage_resource(UAContext *ua, bool use_default, bool autochangers_only)
{
   int i;
   JCR *jcr;
   int jobid;
   char ed1[50];
   char *store_name = NULL;
   STORERES *store = NULL;

   Dmsg1(100, "get_storage_resource: autochangers_only is %d\n", autochangers_only);

   for (i = 1; i < ua->argc; i++) {
      /*
       * Ignore any zapped keyword.
       */
      if (*ua->argk[i] == 0) {
         continue;
      }
      if (use_default && !ua->argv[i]) {
         /*
          * Ignore barcode, barcodes, encrypt, scan and slots keywords.
          */
         if (bstrcasecmp("barcode", ua->argk[i]) ||
             bstrcasecmp("barcodes", ua->argk[i]) ||
             bstrcasecmp("encrypt", ua->argk[i]) ||
             bstrcasecmp("scan", ua->argk[i]) ||
             bstrcasecmp("slots", ua->argk[i])) {
            continue;
         }
         /*
          * Default argument is storage
          */
         if (store_name) {
            ua->error_msg(_("Storage name given twice.\n"));
            return NULL;
         }
         store_name = ua->argk[i];
         if (*store_name == '?') {
            *store_name = 0;
            break;
         }
      } else {
         if (bstrcasecmp(ua->argk[i], NT_("storage")) ||
             bstrcasecmp(ua->argk[i], NT_("sd"))) {
            store_name = ua->argv[i];
            break;
         } else if (bstrcasecmp(ua->argk[i], NT_("jobid"))) {
            jobid = str_to_int64(ua->argv[i]);
            if (jobid <= 0) {
               ua->error_msg(_("Expecting jobid=nn command, got: %s\n"), ua->argk[i]);
               return NULL;
            }
            if (!(jcr = get_jcr_by_id(jobid))) {
               ua->error_msg(_("JobId %s is not running.\n"), edit_int64(jobid, ed1));
               return NULL;
            }
            store = jcr->res.wstore;
            free_jcr(jcr);
            break;
         } else if (bstrcasecmp(ua->argk[i], NT_("job")) ||
                    bstrcasecmp(ua->argk[i], NT_("jobname"))) {
            if (!ua->argv[i]) {
               ua->error_msg(_("Expecting job=xxx, got: %s.\n"), ua->argk[i]);
               return NULL;
            }
            if (!(jcr = get_jcr_by_partial_name(ua->argv[i]))) {
               ua->error_msg(_("Job \"%s\" is not running.\n"), ua->argv[i]);
               return NULL;
            }
            store = jcr->res.wstore;
            free_jcr(jcr);
            break;
         } else if (bstrcasecmp(ua->argk[i], NT_("ujobid"))) {
            if (!ua->argv[i]) {
               ua->error_msg(_("Expecting ujobid=xxx, got: %s.\n"), ua->argk[i]);
               return NULL;
            }
            if (!(jcr = get_jcr_by_full_name(ua->argv[i]))) {
               ua->error_msg(_("Job \"%s\" is not running.\n"), ua->argv[i]);
               return NULL;
            }
            store = jcr->res.wstore;
            free_jcr(jcr);
            break;
        }
      }
   }

   if (store && !ua->acl_access_ok(Storage_ACL, store->name())) {
      store = NULL;
   }

   if (!store && store_name && store_name[0] != 0) {
      store = ua->GetStoreResWithName(store_name);

      if (!store) {
         ua->error_msg(_("Storage resource \"%s\": not found\n"), store_name);
      }
   }

   if (store && !ua->acl_access_ok(Storage_ACL, store->name())) {
      store = NULL;
   }

   /*
    * No keywords found, so present a selection list
    */
   if (!store) {
      store = select_storage_resource(ua, autochangers_only);
   }

   return store;
}

/**
 * Get drive that we are working with for this storage
 */
drive_number_t get_storage_drive(UAContext *ua, STORERES *store)
{
   int i;
   char drivename[10];
   drive_number_t drive = -1;

   /*
    * Get drive for autochanger if possible
    */
   i = find_arg_with_value(ua, NT_("drive"));
   if (i >= 0) {
      drive = atoi(ua->argv[i]);
   } else if (store && store->autochanger) {
      drive_number_t drives;

      drives = get_num_drives(ua, store);

      /*
       * If only one drive, default = 0
       */
      if (drives == 1) {
         drive = 0;
      } else {
         /*
          * Ask user to enter drive number
          */
         start_prompt(ua, _("Select Drive:\n"));
         for (drive_number_t cnt = 0; cnt < drives; cnt++) {
            bsnprintf(drivename, sizeof(drivename), "Drive %hd", cnt);
            add_prompt(ua, drivename);
         }
         if (do_prompt(ua, _("Drive"), _("Select drive"), drivename, sizeof(drivename)) < 0) {
            drive = -1; /* None */
         } else {
            sscanf(drivename, "Drive %hd", &drive);
         }
      }
   } else {
      /*
       * If only one drive, default = 0
       */
      drive = 0;
   }

   return drive;
}

/**
 * Get slot that we are working with for this storage
 */
slot_number_t get_storage_slot(UAContext *ua, STORERES *store)
{
   int i;
   slot_number_t slot = -1;

   /*
    * Get slot for autochanger if possible
    */
   i = find_arg_with_value(ua, NT_("slot"));
   if (i >=0) {
      slot = atoi(ua->argv[i]);
   } else if (store && store->autochanger) {
      /*
       * Ask user to enter slot number
       */
      ua->cmd[0] = 0;
      if (!get_cmd(ua, _("Enter autochanger slot: "))) {
         slot = -1;  /* None */
      } else {
         slot = atoi(ua->cmd);
      }
   }

   return slot;
}

/**
 * Scan looking for mediatype=
 *
 *  if not found or error, put up selection list
 *
 *  Returns: 0 on error
 *           1 on success, MediaType is set
 */
int get_media_type(UAContext *ua, char *MediaType, int max_media)
{
   STORERES *store;
   int i;

   i = find_arg_with_value(ua, NT_("mediatype"));
   if (i >= 0) {
      bstrncpy(MediaType, ua->argv[i], max_media);
      return 1;
   }

   start_prompt(ua, _("Media Types defined in conf file:\n"));

   LockRes();
   foreach_res(store, R_STORAGE) {
      if (ua->acl_access_ok(Storage_ACL, store->name())) {
         add_prompt(ua, store->media_type);
      }
   }
   UnlockRes();

   return (do_prompt(ua, _("Media Type"), _("Select the Media Type"), MediaType, max_media) < 0) ? 0 : 1;
}

bool get_level_from_name(JCR *jcr, const char *level_name)
{
   bool found = false;

   /*
    * Look up level name and pull code
    */
   for (int i = 0; joblevels[i].level_name; i++) {
      if (bstrcasecmp(level_name, joblevels[i].level_name)) {
         jcr->setJobLevel(joblevels[i].level);
         found = true;
         break;
      }
   }

   return found;
}

/**
 * Insert an JobId into the list of selected JobIds when its a unique new id.
 */
static inline bool insert_selected_jobid(alist *selected_jobids, JobId_t JobId)
{
   bool found;
   JobId_t *selected_jobid;

   found = false;
   foreach_alist(selected_jobid, selected_jobids) {
      if (*selected_jobid == JobId) {
         found = true;
         break;
      }
   }

   if (!found) {
      selected_jobid = (JobId_t *)malloc(sizeof(JobId_t));
      *selected_jobid = JobId;
      selected_jobids->append(selected_jobid);
      return true;
   }

   return false;
}

/**
 * Get a job selection, "reason" is used in user messages and can be: cancel, limit, ...
 *
 * Returns: NULL on error
 *          alist on success with the selected jobids.
 */
alist *select_jobs(UAContext *ua, const char *reason)
{
   int i;
   int cnt = 0;
   int njobs = 0;
   JCR *jcr = NULL;
   bool select_all = false;
   bool select_by_state = false;
   alist *selected_jobids;
   const char *lst[] = {
      "job",
      "jobid",
      "ujobid",
      NULL
   };
   enum {
      none,
      all_jobs,
      created_jobs,
      blocked_jobs,
      waiting_jobs,
      running_jobs
   } selection_criterium;

   /*
    * Allocate a list for holding the selected JobIds.
    */
   selected_jobids = New(alist(10, owned_by_alist));

   /*
    * See if "all" is given.
    */
   if (find_arg(ua, NT_("all")) > 0) {
      select_all = true;
   }

   /*
    * See if "state=" is given.
    */
   if (find_arg_with_value(ua, NT_("state")) > 0) {
      select_by_state = true;
   }

   /*
    * See if there are any jobid, job or ujobid keywords.
    */
   if (find_arg_keyword(ua, lst) > 0) {
      for (i = 1; i < ua->argc; i++) {
         if (bstrcasecmp(ua->argk[i], NT_("jobid"))) {
            JobId_t JobId = str_to_int64(ua->argv[i]);
            if (!JobId) {
               continue;
            }
            if (!(jcr = get_jcr_by_id(JobId))) {
               ua->error_msg(_("JobId %s is not running. Use Job name to %s inactive jobs.\n"),  ua->argv[i], _(reason));
               continue;
            }
         } else if (bstrcasecmp(ua->argk[i], NT_("job"))) {
            if (!ua->argv[i]) {
               continue;
            }
            if (!(jcr = get_jcr_by_partial_name(ua->argv[i]))) {
               ua->warning_msg(_("Warning Job %s is not running. Continuing anyway ...\n"), ua->argv[i]);
               continue;
            }
         } else if (bstrcasecmp(ua->argk[i], NT_("ujobid"))) {
            if (!ua->argv[i]) {
               continue;
            }
            if (!(jcr = get_jcr_by_full_name(ua->argv[i]))) {
               ua->warning_msg(_("Warning Job %s is not running. Continuing anyway ...\n"), ua->argv[i]);
               continue;
            }
         }

         if (jcr) {
            if (jcr->res.job && !ua->acl_access_ok(Job_ACL, jcr->res.job->name(), true)) {
               ua->error_msg(_("Unauthorized command from this console.\n"));
               goto bail_out;
            }

            if (insert_selected_jobid(selected_jobids, jcr->JobId)) {
               cnt++;
            }

            free_jcr(jcr);
            jcr = NULL;
         }
      }
   }

   /*
    * If we didn't select any Jobs using jobid, job or ujobid keywords try other selections.
    */
   if (cnt == 0) {
      char buf[1000];
      int tjobs = 0;                   /* Total # number jobs */

      /*
       * Count Jobs running
       */
      foreach_jcr(jcr) {
         if (jcr->JobId == 0) {        /* This is us */
            continue;
         }
         tjobs++;                      /* Count of all jobs */
         if (!ua->acl_access_ok(Job_ACL, jcr->res.job->name())) {
            continue;                  /* Skip not authorized */
         }
         njobs++;                      /* Count of authorized jobs */
      }
      endeach_jcr(jcr);

      if (njobs == 0) {                /* No authorized */
         if (tjobs == 0) {
            ua->send_msg(_("No Jobs running.\n"));
         } else {
            ua->send_msg(_("None of your jobs are running.\n"));
         }
         goto bail_out;
      }

      if (select_all || select_by_state) {
         /*
          * Set selection criterium.
          */
         selection_criterium = none;
         if (select_all) {
            selection_criterium = all_jobs;
         } else {
            i = find_arg_with_value(ua, NT_("state"));
            if (i > 0) {
               if (bstrcasecmp(ua->argv[i], NT_("created"))) {
                  selection_criterium = created_jobs;
               }

               if (bstrcasecmp(ua->argv[i], NT_("blocked"))) {
                  selection_criterium = blocked_jobs;
               }

               if (bstrcasecmp(ua->argv[i], NT_("waiting"))) {
                  selection_criterium = waiting_jobs;
               }

               if (bstrcasecmp(ua->argv[i], NT_("running"))) {
                  selection_criterium = running_jobs;
               }

               if (selection_criterium == none) {
                  ua->error_msg(_("Illegal state either created, blocked, waiting or running\n"));
                  goto bail_out;
               }
            }
         }

         /*
          * Select from all available Jobs the Jobs matching the selection criterium.
          */
         foreach_jcr(jcr) {
            if (jcr->JobId == 0) {  /* This is us */
               continue;
            }

            if (!ua->acl_access_ok(Job_ACL, jcr->res.job->name())) {
               continue;            /* Skip not authorized */
            }

            /*
             * See if we need to select this JobId.
             */
            switch (selection_criterium) {
            case all_jobs:
               break;
            case created_jobs:
               if (jcr->JobStatus != JS_Created) {
                  continue;
               }
               break;
            case blocked_jobs:
               if (!jcr->job_started || jcr->JobStatus != JS_Blocked) {
                  continue;
               }
               break;
            case waiting_jobs:
               if (!job_waiting(jcr)) {
                  continue;
               }
               break;
            case running_jobs:
               if (!jcr->job_started || jcr->JobStatus != JS_Running) {
                  continue;
               }
               break;
            default:
               break;
            }

            insert_selected_jobid(selected_jobids, jcr->JobId);
            ua->send_msg(_("Selected Job %d for cancelling\n") , jcr->JobId);
         }

         if (selected_jobids->empty()) {
            ua->send_msg(_("No Jobs selected.\n"));
            goto bail_out;
         }

         /*
          * Only ask for confirmation when not in batch mode and there is no yes on the cmdline.
          */
         if (!ua->batch && find_arg(ua, NT_("yes")) == -1) {
            if (!get_yesno(ua, _("Confirm cancel (yes/no): ")) || !ua->pint32_val) {
               goto bail_out;
            }
         }
      } else {
         char temp[256];
         char JobName[MAX_NAME_LENGTH];

         /*
          * Interactivly select a Job.
          */
         start_prompt(ua, _("Select Job:\n"));
         foreach_jcr(jcr) {
            char ed1[50];
            if (jcr->JobId == 0) {    /* This is us */
               continue;
            }
            if (!ua->acl_access_ok(Job_ACL, jcr->res.job->name())) {
               continue;              /* Skip not authorized */
            }
            bsnprintf(buf, sizeof(buf), _("JobId=%s Job=%s"), edit_int64(jcr->JobId, ed1), jcr->Job);
            add_prompt(ua, buf);
         }
         endeach_jcr(jcr);

         bsnprintf(temp, sizeof(temp), _("Choose Job to %s"), _(reason));
         if (do_prompt(ua, _("Job"),  temp, buf, sizeof(buf)) < 0) {
            goto bail_out;
         }

         if (bstrcmp(reason, "cancel")) {
            if (ua->api && njobs == 1) {
               char nbuf[1000];

               bsnprintf(nbuf, sizeof(nbuf), _("Cancel: %s\n\n%s"), buf, _("Confirm cancel?"));
               if (!get_yesno(ua, nbuf) || !ua->pint32_val) {
                  goto bail_out;
               }
            } else {
               if (njobs == 1) {
                  if (!get_yesno(ua, _("Confirm cancel (yes/no): ")) || !ua->pint32_val) {
                     goto bail_out;
                  }
               }
            }
         }

         sscanf(buf, "JobId=%d Job=%127s", &njobs, JobName);
         jcr = get_jcr_by_full_name(JobName);
         if (!jcr) {
            ua->warning_msg(_("Job \"%s\" not found.\n"), JobName);
            goto bail_out;
         }

         insert_selected_jobid(selected_jobids, jcr->JobId);
         free_jcr(jcr);
      }
   }

   return selected_jobids;

bail_out:
   delete selected_jobids;

   return NULL;
}

/**
 * Get a slot selection.
 *
 * Returns: false on error
 *          true on success with the selected slots set in the slot_list.
 */
bool get_user_slot_list(UAContext *ua, char *slot_list, const char *argument, int num_slots)
{
   int i, len, beg, end;
   const char *msg;
   char search_argument[20];
   char *p, *e, *h;

   /*
    * See if the argument given is found on the cmdline.
    */
   bstrncpy(search_argument, argument, sizeof(search_argument));
   i = find_arg_with_value(ua, search_argument);
   if (i == -1) {  /* not found */
      /*
       * See if the last letter of search_argument is a 's'
       * When it is strip it and try if that argument is given.
       */
      len = strlen(search_argument);
      if (len > 0 && search_argument[len - 1] == 's') {
         search_argument[len - 1] = '\0';
         i = find_arg_with_value(ua, search_argument);
      }
   }

   if (i > 0) {
      /*
       * Scan slot list in ua->argv[i]
       */
      strip_trailing_junk(ua->argv[i]);
      for (p = ua->argv[i]; p && *p; p = e) {
         /*
          * Check for list
          */
         e = strchr(p, ',');
         if (e) {
            *e++ = 0;
         }
         /*
          * Check for range
          */
         h = strchr(p, '-');             /* range? */
         if (h == p) {
            msg = _("Negative numbers not permitted\n");
            goto bail_out;
         }
         if (h) {
            *h++ = 0;
            if (!is_an_integer(h)) {
               msg = _("Range end is not integer.\n");
               goto bail_out;
            }
            skip_spaces(&p);
            if (!is_an_integer(p)) {
               msg = _("Range start is not an integer.\n");
               goto bail_out;
            }
            beg = atoi(p);
            end = atoi(h);
            if (end < beg) {
               msg = _("Range end not bigger than start.\n");
               goto bail_out;
            }
         } else {
            skip_spaces(&p);
            if (!is_an_integer(p)) {
               msg = _("Input value is not an integer.\n");
               goto bail_out;
            }
            beg = end = atoi(p);
         }
         if (beg <= 0 || end <= 0) {
            msg = _("Values must be be greater than zero.\n");
            goto bail_out;
         }
         if (end > num_slots) {
            msg = _("Slot too large.\n");
            goto bail_out;
         }

         /*
          * Turn on specified range
          */
         for (i = beg; i <= end; i++) {
            set_bit(i - 1, slot_list);
         }
      }
   } else {
      /*
       * Turn everything on
       */
      for (i = 1; i <= num_slots; i++) {
         set_bit(i - 1, slot_list);
      }
   }

   if (debug_level >= 100) {
      Dmsg0(100, "Slots turned on:\n");
      for (i = 1; i <= num_slots; i++) {
         if (bit_is_set(i - 1, slot_list)) {
            Dmsg1(100, "%d\n", i);
         }
      }
   }

   return true;

bail_out:
   Dmsg1(100, "Problem with user selection ERR=%s\n", msg);

   return false;
}

bool get_user_job_type_selection(UAContext *ua, int *jobtype)
{
   int i;
   char job_type[MAX_NAME_LENGTH];

   /* set returning jobtype to invalid */
   *jobtype = -1;

   if ((i = find_arg_with_value(ua, NT_("jobtype"))) >= 0) {
      bstrncpy(job_type, ua->argv[i], sizeof(job_type));
   } else {
      start_prompt(ua, _("Jobtype to prune:\n"));
      for (i = 0; jobtypes[i].type_name; i++) {
         add_prompt(ua, jobtypes[i].type_name);
      }

      if (do_prompt(ua, _("JobType"),  _("Select Job Type"), job_type, sizeof(job_type)) < 0) {
         return false;
      }
   }

   for (i = 0; jobtypes[i].type_name; i++) {
      if (bstrcasecmp(jobtypes[i].type_name, job_type)) {
         break;
      }
   }

   if (!jobtypes[i].type_name) {
      ua->warning_msg(_("Illegal jobtype %s.\n"), job_type);
      return false;
   }

   *jobtype = jobtypes[i].job_type;

   return true;
}

bool get_user_job_status_selection(UAContext *ua, int *jobstatus)
{
   int i;

   if ((i = find_arg_with_value(ua, NT_("jobstatus"))) >= 0) {
      if (strlen(ua->argv[i]) == 1 && ua->argv[i][0] >= 'A' && ua->argv[i][0] <= 'z') {
         *jobstatus = ua->argv[i][0];
      } else if (bstrcasecmp(ua->argv[i], "terminated")) {
         *jobstatus = JS_Terminated;
      } else if (bstrcasecmp(ua->argv[i], "warnings")) {
         *jobstatus = JS_Warnings;
      } else if (bstrcasecmp(ua->argv[i], "canceled")) {
         *jobstatus = JS_Canceled;
      } else if (bstrcasecmp(ua->argv[i], "running")) {
         *jobstatus = JS_Running;
      } else if (bstrcasecmp(ua->argv[i], "error")) {
         *jobstatus = JS_ErrorTerminated;
      } else if (bstrcasecmp(ua->argv[i], "fatal")) {
         *jobstatus = JS_FatalError;
      } else {
         /* invalid jobstatus */
         return false;
      }
   }
   return true;
}

bool get_user_job_level_selection(UAContext *ua, int *joblevel)
{
   int i;

   if ((i = find_arg_with_value(ua, NT_("joblevel"))) >= 0) {
      if (strlen(ua->argv[i]) == 1 && ua->argv[i][0] >= 'A' && ua->argv[i][0] <= 'z') {
         *joblevel = ua->argv[i][0];
      } else {
         /* invalid joblevel */
         return false;
      }
   }
   return true;
}
