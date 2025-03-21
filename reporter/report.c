/*
 * report.c
 *
 * Copyright (c) 2009-2023, NIPPON TELEGRAPH AND TELEPHONE CORPORATION
 */
#include "pg_statsinfo.h"

#define __TABLE__
#define __USER__
#define __CSV__

#include <time.h>

#define SNAPSHOTID_MIN	"1"
#define SNAPSHOTID_MAX	"9223372036854775807"
#define TIMESTAMP_MIN	"4713-01-01 BC 00:00:00 UTC"
#define TIMESTAMP_MAX	"294276-12-31 23:59:59 UTC"

#define SQL_SELECT_SUMMARY						"SELECT * FROM statsrepo.get_summary($1, $2)"
#define SQL_SELECT_DBSTATS						"SELECT * FROM statsrepo.get_dbstats($1, $2)"
#define SQL_SELECT_XACT_TENDENCY				"SELECT * FROM statsrepo.get_xact_tendency_report($1, $2)"
#define SQL_SELECT_DBSIZE_TENDENCY				"SELECT * FROM statsrepo.get_dbsize_tendency_report($1, $2)"
#define SQL_SELECT_DB_RUSAGE				"SELECT * FROM statsrepo.get_db_rusage_report($1, $2)"
#define SQL_SELECT_HT_INFO						"SELECT * FROM statsrepo.get_ht_info($1, $2)"
#define SQL_SELECT_STAT_REPLICATION_SLOTS		"SELECT * FROM statsrepo.get_stat_replication_slots_report($1, $2)"
#define SQL_SELECT_RECOVERY_CONFLICTS 			"SELECT * FROM statsrepo.get_recovery_conflicts($1, $2)"
#define SQL_SELECT_INSTANCE_PROC_TENDENCY "\
SELECT \
	timestamp, \
	idle, \
	'(' || idle_per || ' %)', \
	idle_in_xact, \
	'(' || idle_in_xact_per || ' %)', \
	waiting, \
	'(' || waiting_per || ' %)', \
	running, \
	'(' || running_per || ' %)' \
FROM statsrepo.get_proc_tendency_report($1, $2) \
UNION ALL \
SELECT \
	'Average', \
	pg_catalog.avg(idle)::numeric(5,1), \
	'(' || pg_catalog.avg(idle_per)::numeric(5,1) || ' %)', \
	pg_catalog.avg(idle_in_xact)::numeric(5,1), \
	'(' || pg_catalog.avg(idle_in_xact_per)::numeric(5,1) || ' %)', \
	pg_catalog.avg(waiting)::numeric(5,1), \
	'(' || pg_catalog.avg(waiting_per)::numeric(5,1) || ' %)', \
	pg_catalog.avg(running)::numeric(5,1), \
	'(' || pg_catalog.avg(running_per)::numeric(5,1) || ' %)' \
FROM \
	statsrepo.get_proc_tendency_report($1, $2)"
#define SQL_SELECT_BGWRITER_STATS				"SELECT * FROM statsrepo.get_bgwriter_stats($1, $2)"
#define SQL_SELECT_WALSTATS						"SELECT * FROM statsrepo.get_wal_stats($1, $2)"
#define SQL_SELECT_WALSTATS_TENDENCY			"SELECT * FROM statsrepo.get_wal_tendency($1, $2)"
#define SQL_SELECT_STAT_WAL						"SELECT * FROM statsrepo.get_stat_wal($1, $2)"
#define SQL_SELECT_XID_INCREASE_TENDENCY		"SELECT * FROM statsrepo.get_xid_tendency($1, $2)"

#define SQL_SELECT_CPU_LOADAVG_TENDENCY "\
SELECT * FROM statsrepo.get_cpu_loadavg_tendency($1, $2) \
UNION ALL \
SELECT \
	'Average', \
	pg_catalog.avg(\"user\")::numeric(5,1), \
	pg_catalog.avg(system)::numeric(5,1), \
	pg_catalog.avg(idle)::numeric(5,1), \
	pg_catalog.avg(iowait)::numeric(5,1), \
	pg_catalog.avg(loadavg1)::numeric(6,3), \
	pg_catalog.avg(loadavg5)::numeric(6,3), \
	pg_catalog.avg(loadavg15)::numeric(6,3) \
FROM \
	statsrepo.get_cpu_loadavg_tendency($1, $2)"
#define SQL_SELECT_MEMORY_TENDENCY				"SELECT * FROM statsrepo.get_memory_tendency($1, $2)"
#define SQL_SELECT_IO_USAGE						"SELECT * FROM statsrepo.get_io_usage($1, $2)"
#define SQL_SELECT_IO_USAGE_TENDENCY			"SELECT * FROM statsrepo.get_io_usage_tendency_report($1, $2)"
#define SQL_SELECT_DISK_USAGE_TABLESPACE		"SELECT * FROM statsrepo.get_disk_usage_tablespace($1, $2)"
#define SQL_SELECT_DISK_USAGE_TABLE				"SELECT * FROM statsrepo.get_disk_usage_table($1, $2) LIMIT 10"
#define SQL_SELECT_LONG_TRANSACTIONS			"SELECT * FROM statsrepo.get_long_transactions($1, $2) LIMIT 10"
#define SQL_SELECT_HEAVILY_UPDATED_TABLES		"SELECT * FROM statsrepo.get_heavily_updated_tables($1, $2) LIMIT 20"
#define SQL_SELECT_HEAVILY_ACCESSED_TABLES		"SELECT * FROM statsrepo.get_heavily_accessed_tables($1, $2) LIMIT 20"
#define SQL_SELECT_LOW_DENSITY_TABLES			"SELECT * FROM statsrepo.get_low_density_tables($1, $2) LIMIT 10"
#define SQL_SELECT_CORRELATION					"SELECT * FROM statsrepo.get_correlation($1, $2)"
#define SQL_SELECT_CHECKPOINT_ACTIVITY			"SELECT * FROM statsrepo.get_checkpoint_activity($1, $2)"
#define SQL_SELECT_AUTOVACUUM_ACTIVITY "\
SELECT \
	datname || '.' || nspname || '.' || relname, \
	\"count\", \
	index_scanned, \
	index_skipped, \
	avg_tup_removed, \
	avg_tup_remain, \
	avg_tup_dead, \
	avg_tup_miss_dead, \
	avg_tup_miss_dead_pages, \
	tbl_scan_pages, \
	tbl_scan_pages_ratio, \
	dead_lp_pages, \
	dead_lp_pages_ratio, \
	removed_lp, \
	dead_lp, \
	sum_index_scans, \
	avg_duration, \
	max_duration, \
	cancel, \
	max_cutoff_xid, \
	max_frozen_xid, \
	max_relmin_mxid \
FROM \
	statsrepo.get_autovacuum_activity($1, $2)"
#define SQL_SELECT_AUTOVACUUM_ACTIVITY2 "\
SELECT \
	datname || '.' || nspname || '.' || relname, \
	avg_page_hit, \
	avg_page_miss, \
	avg_page_dirty, \
	avg_read_rate, \
	avg_write_rate, \
	avg_read_duration, \
	avg_write_duration \
FROM \
	statsrepo.get_autovacuum_activity2($1, $2)"

#define SQL_SELECT_AUTOVACUUM_WAL_ACTIVITY "\
SELECT \
	datname || '.' || nspname || '.' || relname, \
	\"count\", \
	wal_records, \
	wal_fpis, \
	wal_bytes \
FROM \
	statsrepo.get_autovacuum_wal_activity($1, $2)"

#define SQL_SELECT_AUTOVACUUM_INDEX_ACTIVITY	"\
SELECT \
	datname || '.' || nspname || '.' || relname, \
	index_name, \
	\"count\", \
	page_total, \
	page_new_del, \
	page_cur_del, \
	page_reuse \
FROM \
	statsrepo.get_autovacuum_index_activity($1,$2)"

#define SQL_SELECT_AUTOANALYZE_STATS "\
SELECT \
	datname || '.' || nspname || '.' || relname, \
	\"count\", \
	total_duration, \
	avg_duration, \
	max_duration, \
	last_analyze, \
	cancels, \
	mod_rows_max \
FROM \
	statsrepo.get_autoanalyze_stats($1, $2)"
#define SQL_SELECT_QUERY_ACTIVITY_FUNCTIONS		"SELECT * FROM statsrepo.get_query_activity_functions($1, $2) LIMIT 20"
#define SQL_SELECT_QUERY_ACTIVITY_STATEMENTS	"SELECT * FROM statsrepo.get_query_activity_statements($1, $2) LIMIT 20"
#define SQL_SELECT_QUERY_ACTIVITY_STATEMENTS_RUSAGE	"SELECT * FROM statsrepo.get_query_activity_statements_rusage($1, $2) LIMIT 20"
#define SQL_SELECT_QUERY_ACTIVITY_PLANS			"SELECT * FROM statsrepo.get_query_activity_plans($1, $2) LIMIT 20"
#define SQL_SELECT_LOCK_CONFLICTS				"SELECT * FROM statsrepo.get_lock_activity($1, $2) LIMIT 20"
#define SQL_SELECT_REPLICATION_STATUS "\
SELECT \
	usename, \
	application_name, \
	client_addr, \
	client_hostname, \
	client_port, \
	backend_start, \
	state, \
	current_location, \
	sent_location, \
	write_location, \
	flush_location, \
	replay_location, \
	sync_priority, \
	sync_state, \
	statsrepo.pg_size_pretty(replay_delay_avg::bigint), \
	statsrepo.pg_size_pretty(replay_delay_peak::bigint), \
	pg_catalog.to_char(write_lag_time, 'HH24:MI:SS.US'), \
	pg_catalog.to_char(flush_lag_time, 'HH24:MI:SS.US'), \
	pg_catalog.to_char(replay_lag_time, 'HH24:MI:SS.US') \
FROM \
	statsrepo.get_replication_activity($1, $2)"
#define SQL_SELECT_SETTING_PARAMETERS			"SELECT * FROM statsrepo.get_setting_parameters($1, $2)"
#define SQL_SELECT_SCHEMA_INFORMATION_TABLES	"SELECT * FROM statsrepo.get_schema_info_tables($1, $2)"
#define SQL_SELECT_SCHEMA_INFORMATION_INDEXES	"SELECT * FROM statsrepo.get_schema_info_indexes($1, $2)"
#define SQL_SELECT_ALERT						"SELECT * FROM statsrepo.get_alert($1, $2)"
#define SQL_SELECT_WAIT_SAMPLING				"SELECT * FROM statsrepo.get_wait_sampling($1, $2)"
#define SQL_SELECT_WAIT_SAMPLING_BY_INSTID		"SELECT * FROM statsrepo.get_wait_sampling_by_instid($1, $2)"
#define SQL_SELECT_WAIT_SAMPLING_BY_DBID		"SELECT * FROM statsrepo.get_wait_sampling_by_dbid($1, $2)"
#define SQL_SELECT_PROFILES						"SELECT * FROM statsrepo.get_profiles($1, $2)"
#define SQL_SELECT_CPU_INFO						"SELECT * FROM statsrepo.get_cpuinfo($1, $2)"
#define SQL_SELECT_MEM_INFO						"SELECT * FROM statsrepo.get_meminfo($1, $2)"
#ifdef __USER__
#define SQL_SELECT_NUM_USER						"SELECT * FROM statsrepo.get_num_user($1, $2)"
#endif
#ifdef __TABLE__
#define SQL_SELECT_TABLE_STAT					 "SELECT * FROM statsrepo.get_table_stat($1, $2)"
#endif

#define SQL_SELECT_REPORT_SCOPE_BY_SNAPID "\
	SELECT \
		i.instid, \
		i.hostname, \
		i.port, \
		i.pg_version, \
		pg_catalog.min(s.snapid), \
		pg_catalog.max(s.snapid) \
	FROM \
		statsrepo.snapshot s \
		LEFT JOIN statsrepo.instance i ON s.instid = i.instid \
	WHERE \
		s.snapid BETWEEN $1 AND $2 \
	GROUP BY \
		i.instid, \
		i.hostname, \
		i.port, \
		i.pg_version \
	ORDER BY \
		i.instid"

#define SQL_SELECT_REPORT_SCOPE_BY_TIMESTAMP "\
	SELECT \
		i.instid, \
		i.hostname, \
		i.port, \
		i.pg_version, \
		pg_catalog.min(s.snapid), \
		pg_catalog.max(s.snapid) \
	FROM \
		statsrepo.snapshot s \
		LEFT JOIN statsrepo.instance i ON s.instid = i.instid \
	WHERE \
		s.time::timestamp(0) BETWEEN $1 AND $2 \
	GROUP BY \
		i.instid, \
		i.hostname, \
		i.port, \
		i.pg_version \
	ORDER BY \
		i.instid"

/* the report scope per instance */
typedef struct ReportScope
{
	char	*instid;		/* instance ID */
	char	*host;			/* host */
	char	*port;			/* port */
	int		 version;		/* PostgreSQL version */
	char	*beginid;		/* begin point of report */
	char	*endid;			/* end point of report */
} ReportScope;

/* function interface of the report builder */
typedef void (*ReportBuild)(PGconn *conn, ReportScope *scope, FILE *out);

/* report builder functions */
static void report_summary(PGconn *conn, ReportScope *scope, FILE *out);
static void report_database_statistics(PGconn *conn, ReportScope *scope, FILE *out);
static void report_instance_activity(PGconn *conn, ReportScope *scope, FILE *out);
static void report_resource_usage(PGconn *conn, ReportScope *scope, FILE *out);
static void report_disk_usage(PGconn *conn, ReportScope *scope, FILE *out);
static void report_long_transactions(PGconn *conn, ReportScope *scope, FILE *out);
static void report_notable_tables(PGconn *conn, ReportScope *scope, FILE *out);
static void report_checkpoint_activity(PGconn *conn, ReportScope *scope, FILE *out);
static void report_autovacuum_activity(PGconn *conn, ReportScope *scope, FILE *out);
static void report_query_activity(PGconn *conn, ReportScope *scope, FILE *out);
static void report_lock_conflicts(PGconn *conn, ReportScope *scope, FILE *out);
static void report_replication_activity(PGconn *conn, ReportScope *scope, FILE *out);
static void report_setting_parameters(PGconn *conn, ReportScope *scope, FILE *out);
static void report_schema_information(PGconn *conn, ReportScope *scope, FILE *out);
static void report_alert(PGconn *conn, ReportScope *scope, FILE *out);
static void report_alert_section(PGconn *conn, ReportScope *scope, FILE *out);
static void report_profiles(PGconn *conn, ReportScope *scope, FILE *out);
static void report_hardware_info(PGconn *conn, ReportScope *scope, FILE *out);
static void report_all(PGconn *conn, ReportScope *scope, FILE *out);
#ifdef __USER__
static void report_num_user(PGconn *conn, ReportScope *scope, FILE *out);
#endif
#ifdef __TABLE__
static void report_table_stat(PGconn *conn, ReportScope *scope, FILE *out);
#endif

static ReportBuild parse_reportid(const char *value);
static List *select_scope_by_snapid(PGconn *conn, const char *beginid, const char *endid);
static List *select_scope_by_timestamp(PGconn *conn, time_t begindate, time_t enddate);
static void destroy_report_scope(ReportScope *scope);
static int get_server_version(PGconn *conn);
static int parse_version(const char *versionString);
static void print_alert_data(PGconn *conn, ReportScope *scope, FILE *out);

#ifdef __CSV__
static char fullfilename[128];

extern char tsd[];

static char *
create_filename(const char* filename)
{
	memset(fullfilename, '\0', sizeof(fullfilename));
	if (tsd[0] == '\0')
		sprintf(fullfilename, "%s", filename);
	else
		sprintf(fullfilename, "%s/%s", tsd, filename);
	return fullfilename;
}

#endif

/*
 * generate a report
 */
void
do_report(PGconn *conn,
		  const char *reportid,
		  const char *instid,
		  const char *beginid,
		  const char *endid,
		  time_t begindate,
		  time_t enddate,
		  const char *filename)
{
	ReportBuild	 reporter;
	List		*scope_list;
	ListCell	*cell;
	FILE		*out = stdout;
	int64		 b_id, e_id, i_id;

	/* validate parameters */
	if (beginid && (!parse_int64(beginid, &b_id) || b_id <= 0))
		ereport(ERROR,
			(errcode(EINVAL),
			 errmsg("invalid snapshot ID (--beginid) : '%s'", beginid)));
	if (endid && (!parse_int64(endid, &e_id) || e_id <= 0))
		ereport(ERROR,
			(errcode(EINVAL),
			 errmsg("invalid snapshot ID (--endid) : '%s'", endid)));
	if (instid && (!parse_int64(instid, &i_id) || i_id <= 0))
		ereport(ERROR,
			(errcode(EINVAL),
			 errmsg("invalid instance ID (--instid) : '%s'", instid)));
	if ((beginid && endid) && b_id > e_id)
		ereport(ERROR,
			(errcode(EINVAL),
			 errmsg("--endid must be greater than --beginid")));
	if ((beginid || endid) && (begindate != -1 || enddate != -1))
		ereport(ERROR,
			(errcode(EINVAL),
			 errmsg("can't specify both snapshot ID and timestamp")));

	/* parse report ID */
	reporter = parse_reportid(reportid);

	/* change the output destination */
	if (filename && (out = fopen(filename, "w")) == NULL)
		ereport(ERROR,
			(errcode_errno(),
			 errmsg("could not open file : '%s'", filename)));

	/* isolate transaction to insure the contents of report */
	pgut_command(conn, "BEGIN ISOLATION LEVEL REPEATABLE READ", 0, NULL);

	/* exclusive control for don't run concurrently with the maintenance */
	if (get_server_version(conn) >= 90000)
	{
		PGresult *res;

		res = pgut_execute(conn, "SELECT pg_catalog.pg_is_in_recovery()", 0, NULL);

		if (strcmp(PQgetvalue(res, 0, 0), "f") == 0)
			pgut_command(conn, "LOCK TABLE statsrepo.instance IN SHARE MODE", 0, NULL);
		PQclear(res);
	}
	else
		pgut_command(conn, "LOCK TABLE statsrepo.instance IN SHARE MODE", 0, NULL);

	/* get the report scope of each instance */
	if (beginid || endid)
		scope_list = select_scope_by_snapid(conn, beginid, endid);
	else if (begindate != -1 || enddate != -1)
		scope_list = select_scope_by_timestamp(conn, begindate, enddate);
	else
		scope_list = select_scope_by_snapid(conn, SNAPSHOTID_MIN, SNAPSHOTID_MAX);

	/* generate report */
	foreach(cell, scope_list)
	{
		ReportScope	*scope = (ReportScope *) lfirst(cell);

		/* if instance ID is specified, skip non-target */
		if (instid && strcmp(instid, scope->instid) != 0)
			continue;
		/* don't generate report from same snapshot */
		if (strcmp(scope->beginid, scope->endid) == 0)
			continue;

		/* report header */
		fprintf(out, "---------------------------------------------\n");
		fprintf(out, "STATSINFO Report (host: %s, port: %s)\n", scope->host, scope->port);
		fprintf(out, "---------------------------------------------\n\n");

		reporter(conn, scope, out);
	}
	pgut_command(conn, "END", 0, NULL);

	/* cleanup */
	list_destroy(scope_list, destroy_report_scope);
	if (out != stdout)
		fclose(out);
}

/*
 * generate a report that corresponds to 'Summary'
 */
static void
report_summary(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Summary */\n");
	fprintf(out, "----------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_SUMMARY, lengthof(params), params);
	if (PQntuples(res) == 0)
	{
		PQclear(res);
		return;
	}
	fprintf(out, "Database System ID   : %s\n", PQgetvalue(res, 0, 0));
	fprintf(out, "Host                 : %s\n", PQgetvalue(res, 0, 1));
	fprintf(out, "Port                 : %s\n", PQgetvalue(res, 0, 2));
	fprintf(out, "PostgreSQL Version   : %s\n", PQgetvalue(res, 0, 3));
	fprintf(out, "Snapshot Begin       : %s\n", PQgetvalue(res, 0, 4));
	fprintf(out, "Snapshot End         : %s\n", PQgetvalue(res, 0, 5));
	fprintf(out, "Snapshot Duration    : %s\n", PQgetvalue(res, 0, 6));
	fprintf(out, "Total Database Size  : %s\n", PQgetvalue(res, 0, 7));
	fprintf(out, "Total Commits        : %s\n", PQgetvalue(res, 0, 8));
	fprintf(out, "Total Rollbacks      : %s\n\n", PQgetvalue(res, 0, 9));
	PQclear(res);
}

/*
 * generate a report that corresponds to 'DatabaseStatistics'
 */
static void
report_database_statistics(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;
#ifdef __CSV__
	FILE		*tout = stdout;
	char *f;
#endif

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Database Statistics */\n");
	fprintf(out, "----------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_DBSTATS, lengthof(params), params);
	for (i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "Database Name              : %s\n", PQgetvalue(res, i, 0));
		fprintf(out, "Database Size              : %s MiB\n", PQgetvalue(res, i, 1));
		fprintf(out, "Database Size Increase     : %s MiB\n", PQgetvalue(res, i, 2));
		fprintf(out, "Commit/s                   : %s\n", PQgetvalue(res, i, 3));
		fprintf(out, "Rollback/s                 : %s\n", PQgetvalue(res, i, 4));
		fprintf(out, "Cache Hit Ratio            : %s %%\n", PQgetvalue(res, i, 5));
		fprintf(out, "Block Read/s (disk+cache)  : %s\n", PQgetvalue(res, i, 6));
		fprintf(out, "Block Read/s (disk)        : %s\n", PQgetvalue(res, i, 7));
		fprintf(out, "Rows Read/s                : %s\n", PQgetvalue(res, i, 8));
		fprintf(out, "Temporary Files            : %s\n", PQgetvalue(res, i, 9));
		fprintf(out, "Temporary Bytes            : %s MiB\n", PQgetvalue(res, i, 10));
		fprintf(out, "Deadlocks                  : %s\n", PQgetvalue(res, i, 11));
		fprintf(out, "Block Read Time            : %s ms\n", PQgetvalue(res, i, 12));
		fprintf(out, "Block Write Time           : %s ms\n\n", PQgetvalue(res, i, 13));
	}
	PQclear(res);

	fprintf(out, "/** Transaction Statistics **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-16s  %12s  %12s\n",
		"DateTime", "Database", "Commit/s", "Rollback/s");
	fprintf(out, "-----------------------------------------------------------------\n");

#ifdef __CSV__
	f = create_filename("TS-transaction-statistics.csv");
	if ((tout = fopen(f, "w+")) == NULL)
		ereport(ERROR,
				(errcode_errno(),
				 errmsg("could not open file : '%s'", f)));

	fprintf(tout, "%s,%s,%s,%s\n",
		"DateTime", "Database", "Commit/s", "Rollback/s");
#endif

	res = pgut_execute(conn, SQL_SELECT_XACT_TENDENCY, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-16s  %12s  %12s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3));
#ifdef __CSV__
		fprintf(tout, "%-16s,%s,%s,%s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3));
#endif
	}
#ifdef __CSV__
	if (tout != stdout)
		fclose(tout);
#endif
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Database Size **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-16s  %14s\n",
		"DateTime", "Database", "Size");
	fprintf(out, "-----------------------------------------------------\n");

#ifdef __CSV__
	f = create_filename("TS-database-size.csv");
	if ((tout = fopen(f, "w+")) == NULL)
		ereport(ERROR,
				(errcode_errno(),
				 errmsg("could not open file : '%s'", f)));

	fprintf(tout, "%s,%s,%s\n",
		"DateTime", "Database", "Size");
#endif

	res = pgut_execute(conn, SQL_SELECT_DBSIZE_TENDENCY, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-16s  %10s MiB\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2));
#ifdef __CSV__
		fprintf(tout, "%-16s,%s,%s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2));
#endif
	}
#ifdef __CSV__
	if (tout != stdout)
		fclose(tout);
#endif
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Database Resource Usage(rusage) **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %12s  %12s  %14s  %13s  %12s  %12s  %14s  %13s\n",
		"Database", "Plan Reads", "Plan Writes", "Plan User Time", "Plan Sys Time",
		"Exec Reads", "Exec Writes", "Exec User Time", "Exec Sys Time");
	fprintf(out, "----------------------------------------------------------------------------------------------------------------------------------------\n");


	res = pgut_execute(conn, SQL_SELECT_DB_RUSAGE, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %12s  %12s  %14s  %13s  %12s  %12s  %14s  %13s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7),
			PQgetvalue(res, i, 8));
	}
	PQclear(res);
	fprintf(out, "\n");

	fprintf(out, "/** Recovery Conflicts **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %19s  %13s  %17s  %18s  %17s\n",
		"Database", "Conflict Tablespace", "Conflict Lock",
		"Conflict Snapshot", "Conflict Bufferpin", "Conflict Deadlock");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_RECOVERY_CONFLICTS, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %19s  %13s  %17s  %18s  %17s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5));
	}
	PQclear(res);

	fprintf(out, "\n");

	fprintf(out, "/** Wait Sampling per Database **/\n");
	fprintf(out, "-----------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_WAIT_SAMPLING_BY_DBID, lengthof(params), params);
	for (i = 0; i < PQntuples(res); i++)
	{
		if (strcmp(PQgetvalue(res, i, 6), "1") == 0)
		{
			fprintf(out, "\n");
			fprintf(out, "%-16s\n",
			"Database");
			fprintf(out, "---------------------------------------------------------------------------------------\n");
			fprintf(out, "%-16s\n",
			PQgetvalue(res, i, 1));

			fprintf(out, "\n");
			fprintf(out, "\tTop 10 Events:\n");
			fprintf(out, "\t%-12s  %-32s  %8s  %12s\n",
			"Event Type", "Event", "Count", "Ratio(%)");
			fprintf(out, "\t-------------------------------------------------------------------------------\n");
		}

		fprintf(out, "\t%-12s  %-32s  %8s  %12s\n",
		PQgetvalue(res, i, 2),
		PQgetvalue(res, i, 3),
		PQgetvalue(res, i, 4),
		PQgetvalue(res, i, 5));
	}
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'InstanceActivity'
 */
static void
report_instance_activity(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;
#ifdef __CSV__
	FILE		*tout = stdout;
	char *f;
#endif

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Instance Activity */\n");
	fprintf(out, "----------------------------------------\n\n");

	fprintf(out, "/** WAL Statistics **/\n");
	fprintf(out, "-----------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_WALSTATS, lengthof(params), params);
	if (PQntuples(res) == 0)
	{
		PQclear(res);
		return;
	}
	if (PQgetisnull(res, 0, 0))
		fprintf(out, "WAL Write Total    : (N/A)\n");
	else
		fprintf(out, "WAL Write Total    : %s MiB\n", PQgetvalue(res, 0, 0));
	if (PQgetisnull(res, 0, 1))
		fprintf(out, "WAL Write Speed    : (N/A)\n");
	else
		fprintf(out, "WAL Write Speed    : %s MiB/s\n", PQgetvalue(res, 0, 1));
	if (PQgetisnull(res, 0, 2))
		fprintf(out, "WAL Archive Total  : (N/A)\n");
	else
		fprintf(out, "WAL Archive Total  : %s file(s)\n", PQgetvalue(res, 0, 2));
	if (PQgetisnull(res, 0, 3))
		fprintf(out, "WAL Archive Failed : (N/A)\n\n");
	else
		fprintf(out, "WAL Archive Failed : %s file(s)\n\n", PQgetvalue(res, 0, 3));
	PQclear(res);

	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-17s  %-24s  %14s  %14s  %-17s\n",
		"DateTime", "Location", "Segment File", "Write Size", "Write Size/s", "Last Archived WAL");
	fprintf(out, "-------------------------------------------------------------------------------------------------------------------\n");

#ifdef __CSV__
	f = create_filename("TS-wal-statistics.csv");
	if ((tout = fopen(f, "w+")) == NULL)
		ereport(ERROR,
				(errcode_errno(),
				 errmsg("could not open file : '%s'", f)));

	fprintf(tout, "%s,%s,%s,%s,%s,%s\n",
		"DateTime", "Location", "Segment File", "Write Size", "Write Size/s", "Last Archived WAL");
#endif

	res = pgut_execute(conn, SQL_SELECT_WALSTATS_TENDENCY, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-17s  %-24s  %10s MiB  %10s MiB  %-17s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5));
#ifdef __CSV__
		fprintf(tout, "%-16s,%-17s,%-24s,%s,%s, %-17s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5));
#endif
	}
#ifdef __CSV__
	if (tout != stdout)
		fclose(tout);
#endif
	fprintf(out, "\n");
	PQclear(res);
	
	fprintf(out, "----------------------------------------\n");
	fprintf(out, "%13s  %11s  %12s  %16s  %12s  %12s  %14s  %13s  %-20s\n",
		"WAL Records", "WAL Fpi", "WAL Bytes", "WAL Buffers Full", "WAL Write" , "WAL Sync" , "WAL Write Time" , "WAL Sync Time" , "WAL Reset");
	fprintf(out, "----------------------------------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_STAT_WAL, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%13s  %11s  %12s  %16s  %12s  %12s  %11s ms  %10s ms  %-20s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7),
			PQgetvalue(res, i, 8));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Instance Processes **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %12s  %12s  %12s  %12s  %12s  %12s  %12s  %12s\n",
		"DateTime", "Idle", "(%)", "Idle In Xact", "(%)", "Waiting", "(%)", "Running", "(%)");
	fprintf(out, "---------------------------------------------------------------------------------------------------------------------------------\n");

#ifdef __CSV__
	f = create_filename("TS-instance-processes.csv");
	if ((tout = fopen(f, "w+")) == NULL)
		ereport(ERROR,
				(errcode_errno(),
				 errmsg("could not open file : '%s'", f)));

	fprintf(tout, "%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
		"DateTime", "Idle", "(%)", "Idle In Xact", "(%)", "Waiting", "(%)", "Running", "(%)");
#endif

	res = pgut_execute(conn, SQL_SELECT_INSTANCE_PROC_TENDENCY, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %12s  %12s  %12s  %12s  %12s  %12s  %12s  %12s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7),
			PQgetvalue(res, i, 8));
#ifdef __CSV__
		fprintf(tout, "%-16s,%s,%s,%s,%s,%s,%s,%s,%s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7),
			PQgetvalue(res, i, 8));
#endif
	}
#ifdef __CSV__
	if (tout != stdout)
		fclose(tout);
#endif
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** BGWriter Statistics **/\n");
	fprintf(out, "-----------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_BGWRITER_STATS, lengthof(params), params);
	fprintf(out, "Written Buffers By BGWriter (Average) : %s buffers/s\n", PQgetvalue(res, 0, 0));
	fprintf(out, "Written Buffers By BGWriter (Maximum) : %s buffers/s\n", PQgetvalue(res, 0, 1));
	fprintf(out, "Written Buffers By Backend (Average)  : %s buffers/s\n", PQgetvalue(res, 0, 2));
	fprintf(out, "Written Buffers By Backend (Maximum)  : %s buffers/s\n", PQgetvalue(res, 0, 3));
	fprintf(out, "Backend Executed fsync (Average)      : %s sync/s\n", PQgetvalue(res, 0, 4));
	fprintf(out, "Backend Executed fsync (Maximum)      : %s sync/s\n\n", PQgetvalue(res, 0, 5));
	PQclear(res);

	fprintf(out, "/** Transaction Increase Tendency **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %12s\n",
		"DateTime", "XID Increase");
	fprintf(out, "---------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_XID_INCREASE_TENDENCY, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %12s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Wait Sampling (Instance) **/\n");
	fprintf(out, "-----------------------------------\n");

	fprintf(out, "\n");
	fprintf(out, "\tTop 10 Events:\n");
	fprintf(out, "\t%-12s  %-32s  %8s  %12s\n",
	"Event Type", "Event", "Count", "Ratio(%)");
	fprintf(out, "\t-------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_WAIT_SAMPLING_BY_INSTID, lengthof(params), params);
	for (i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "\t%-12s  %-32s  %8s  %12s\n",
		PQgetvalue(res, i, 0),
		PQgetvalue(res, i, 1),
		PQgetvalue(res, i, 2),
		PQgetvalue(res, i, 3));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Hash Table  Statistics **/\n");
	fprintf(out, "----------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_HT_INFO, lengthof(params), params);
	fprintf(out, "pg_stat_statements dealloc : %s\n", PQgetvalue(res, 0, 0));
	fprintf(out, "pg_stat_statements reset   : %s\n", PQgetvalue(res, 0, 1));
	fprintf(out, "wait sampling dealloc      : %s\n", PQgetvalue(res, 0, 2));
	fprintf(out, "wait sampling reset        : %s\n", PQgetvalue(res, 0, 3));
	fprintf(out, "rusage dealloc             : %s\n", PQgetvalue(res, 0, 4));
	fprintf(out, "rusage reset               : %s\n", PQgetvalue(res, 0, 5));
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'OSResourceUsage'
 */
static void
report_resource_usage(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;
#ifdef __CSV__
	FILE		*tout = stdout;
	char *f;
#endif

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* OS Resource Usage */\n");
	fprintf(out, "----------------------------------------\n\n");

	fprintf(out, "/** CPU Usage + Load Average **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %8s  %8s  %8s  %8s  %8s  %8s  %9s\n",
		"DateTime", "User", "System", "Idle", "IOwait", "Loadavg1", "Loadavg5", "Loadavg15");
	fprintf(out, "------------------------------------------------------------------------------------------\n");

#ifdef __CSV__
	f = create_filename("TS-resource-usage-load-average.csv");
	if ((tout = fopen(f, "w+")) == NULL)
		ereport(ERROR,
				(errcode_errno(),
				 errmsg("could not open file : '%s'", f)));

	fprintf(tout, "%s,%s,%s,%s,%s,%s,%s,%s\n",
			"DateTime", "User", "System", "Idle", "IOwait", "Loadavg1", "Loadavg5", "Loadavg15");
#endif

	res = pgut_execute(conn, SQL_SELECT_CPU_LOADAVG_TENDENCY, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %6s %%  %6s %%  %6s %%  %6s %%  %8s  %8s  %9s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7));
#ifdef __CSV__
		fprintf(tout, "%-16s,%s,%s,%s,%s,%s,%s,%s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7));
#endif
	}
#ifdef __CSV__
	if (tout != stdout)
		fclose(tout);
#endif
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** IO Usage **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-12s  %-24s  %12s  %12s  %17s  %17s  %16s  %15s\n",
		"Device", "Including TableSpaces", "Total Read", "Total Write",
		"Total Read Time", "Total Write Time", "Current IO Queue", "Total IO Time");
	fprintf(out, "----------------------------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_IO_USAGE, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		if (strcmp(PQgetvalue(res, i, 0), "unknown") == 0)
			fprintf(out, "%-12s  %-24s  %12s  %12s  %17s  %17s  %16s  %15s\n",
				PQgetvalue(res, i, 0),
				PQgetvalue(res, i, 1),
				"(N/A)",
				"(N/A)",
				"(N/A)",
				"(N/A)",
				"(N/A)",
				"(N/A)");
		else
			fprintf(out, "%-12s  %-24s  %8s MiB  %8s MiB  %14s ms  %14s ms  %16s  %12s ms\n",
				PQgetvalue(res, i, 0),
				PQgetvalue(res, i, 1),
				PQgetvalue(res, i, 2),
				PQgetvalue(res, i, 3),
				PQgetvalue(res, i, 4),
				PQgetvalue(res, i, 5),
				PQgetvalue(res, i, 6),
				PQgetvalue(res, i, 7));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-12s  %29s  %29s  %15s  %15s\n",
		"DateTime", "Device", "Read Size/s (Peak)", "Write Size/s (Peak)", "Read Time Rate", "Write Time Rate");
	fprintf(out, "---------------------------------------------------------------------------------------------------------------------------------\n");

#ifdef __CSV__
	f = create_filename("TS-io-usage.csv");
	if ((tout = fopen(f, "w+")) == NULL)
		ereport(ERROR,
				(errcode_errno(),
				 errmsg("could not open file : '%s'", f)));

	fprintf(tout, "%s,%s,%s,%s,%s,%s\n",
		"DateTime", "Device", "Read Size/s (Peak)", "Write Size/s (Peak)", "Read Time Rate", "Write Time Rate");
#endif

	res = pgut_execute(conn, SQL_SELECT_IO_USAGE_TENDENCY, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-12s  %9s KiB (%9s KiB)  %9s KiB (%9s KiB)  %13s %%  %13s %%\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 7),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5));
#ifdef __CSV__
			fprintf(tout, "%-16s,%s,%s,%s,%s,%s,%s,%s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 7),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5));
#endif
}
#ifdef __CSV__
	if (tout != stdout)
		fclose(tout);
#endif
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Memory Usage **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %12s  %12s  %12s  %12s  %12s\n",
		"DateTime", "Memfree", "Buffers", "Cached", "Swap", "Dirty");
	fprintf(out, "-----------------------------------------------------------------------------------------\n");

#ifdef __CSV__
	f = create_filename("TS-memory-usage.csv");
	if ((tout = fopen(f, "w+")) == NULL)
		ereport(ERROR,
				(errcode_errno(),
				 errmsg("could not open file : '%s'", f)));

	fprintf(tout, "%s,%s,%s,%s,%s,%s\n",
		"DateTime", "Memfree", "Buffers", "Cached", "Swap", "Dirty");
#endif

	res = pgut_execute(conn, SQL_SELECT_MEMORY_TENDENCY, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %8s MiB  %8s MiB  %8s MiB  %8s MiB  %8s MiB\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5));
#ifdef __CSV__
			fprintf(tout, "%-16s,%s,%s,%s,%s,%s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5));
#endif
	}
#ifdef __CSV__
	if (tout != stdout)
		fclose(tout);
#endif
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'DiskUsage'
 */
static void
report_disk_usage(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Disk Usage */\n");
	fprintf(out, "----------------------------------------\n\n");

	fprintf(out, "/** Disk Usage per Tablespace **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-32s  %-12s  %12s  %12s  %10s\n",
		"Tablespace", "Location", "Device", "Used", "Avail", "Remain");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_DISK_USAGE_TABLESPACE, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-32s  %-12s  %8s MiB  %8s MiB  %8s %%\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Disk Usage per Table **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-16s  %-16s  %12s  %12s  %12s  %12s\n",
		"Database", "Schema", "Table", "Size", "Table Reads", "Index Reads", "Toast Reads");
	fprintf(out, "---------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_DISK_USAGE_TABLE, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-16s  %-16s  %8s MiB  %12s  %12s  %12s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6));
	}
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'LongTransactions'
 */
static void
report_long_transactions(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Long Transactions */\n");
	fprintf(out, "----------------------------------------\n");
	fprintf(out, "%-8s  %-15s  %20s  %10s  %-32s\n",
		"PID", "Client Address", "Xact Start", "Duration", "Query");
	fprintf(out, "-----------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_LONG_TRANSACTIONS, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-8s  %-15s  %20s  %8s s  %-32s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4));
	}
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'NotableTables'
 */
static void
report_notable_tables(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Notable Tables */\n");
	fprintf(out, "----------------------------------------\n\n");

	fprintf(out, "/** Heavily Updated Tables **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-16s  %-16s  %12s  %12s  %12s  %12s  %12s\n",
		"Database", "Schema", "Table", "INSERT Rows", "UPDATE Rows", "DELETE Rows",
		"Total Rows", "HOT Ratio(%)");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_HEAVILY_UPDATED_TABLES, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-16s  %-16s  %12s  %12s  %12s  %12s  %12s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Heavily Accessed Tables **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-16s  %-16s  %12s  %12s  %14s  %18s\n",
		"Database", "Schema", "Table", "Seq Scans", "Read Rows", "Read Rows/Scan",
		"Cache Hit Ratio(%)");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_HEAVILY_ACCESSED_TABLES, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-16s  %-16s  %12s  %12s  %14s  %18s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Low Density Tables **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-16s  %-16s  %12s  %14s  %14s  %21s\n",
		"Database", "Schema", "Table", "Live Tuples", "Logical Pages", "Physical Pages",
		"Logical Page Ratio(%)");
	fprintf(out, "----------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_LOW_DENSITY_TABLES, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-16s  %-16s  %12s  %14s  %14s  %21s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Correlation **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-16s  %-16s  %-16s  %12s\n",
		"Database", "Schema", "Table", "Column", "Correlation");
	fprintf(out, "---------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_CORRELATION, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-16s  %-16s  %-16s  %12s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4));
	}
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'CheckpointActivity'
 */
static void
report_checkpoint_activity(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Checkpoint Activity */\n");
	fprintf(out, "----------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_CHECKPOINT_ACTIVITY, lengthof(params), params);
	if (PQntuples(res) == 0)
	{
		PQclear(res);
		return;
	}
	fprintf(out, "Total Checkpoints        : %s\n", PQgetvalue(res, 0, 0));
	fprintf(out, "Checkpoints By Time      : %s\n", PQgetvalue(res, 0, 1));
	fprintf(out, "Checkpoints By WAL       : %s\n", PQgetvalue(res, 0, 2));
	fprintf(out, "Written Buffers Average  : %s\n", PQgetvalue(res, 0, 3));
	fprintf(out, "Written Buffers Maximum  : %s\n", PQgetvalue(res, 0, 4));
	fprintf(out, "Write Duration Average   : %s sec\n", PQgetvalue(res, 0, 5));
	fprintf(out, "Write Duration Maximum   : %s sec\n\n", PQgetvalue(res, 0, 6));
	PQclear(res);
}

/*
 * generate a report that corresponds to 'AutovacuumActivity'
 */
static void
report_autovacuum_activity(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Autovacuum Activity */\n");
	fprintf(out, "----------------------------------------\n\n");

	fprintf(out, "/** Vacuum Basic Statistics (Average) **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-40s|%8s|%8s|%8s|%9s|%10s|%10s|%17s|%16s|%16s|%8s|%8s|%8s|%8s|%8s|%8s|%13s|%13s|%13s\n",
		"", "", "#Index", "#Index", "Removed", "Remain", "Remain", "Missed Dead", "Table Scan", "Dead Tuple", 
		"Removed", "Dead", "#Index", "", "Duration", "", "Removable", "New Rel", "New Rel");
	fprintf(out, "%-40s|%8s|%8s|%8s|%9s|%10s|%10s|%8s|%8s|%16s|%16s|%8s|%8s|%8s|%8s|%8s|%8s|%13s|%13s|%13s\n",
		"Table", "#Count", "Scanned", "Skipped", "Rows", "Rows", "Dead", "Rows", "Pages", "Pages(Ratio)", " Pages(Ratio)",
		"Lp", "Lp", "Scans", "Duration", "(Max)", "#Cancels", "CutOff Xid", "Frozen Xid", "Min Mxid");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_AUTOVACUUM_ACTIVITY, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-40s %8s %8s %8s %9s %10s %10s %8s %8s %8s(%5s%%) %8s(%5s%%) %8s %8s %8s %6s s %6s s %8s %13s %13s %13s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7),
			PQgetvalue(res, i, 8),
			PQgetvalue(res, i, 9),
			PQgetvalue(res, i, 10),
			PQgetvalue(res, i, 11),
			PQgetvalue(res, i, 12),
			PQgetvalue(res, i, 13),
			PQgetvalue(res, i, 14),
			PQgetvalue(res, i, 15),
			PQgetvalue(res, i, 16),
			PQgetvalue(res, i, 17),
			PQgetvalue(res, i, 18),
			PQgetvalue(res, i, 19),
			PQgetvalue(res, i, 20),
			PQgetvalue(res, i, 21));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Vacuum I/O Statistics (Average) **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-40s  %10s  %10s  %10s  %13s  %13s  %14s  %14s\n",
		"Table", "Page Hit", "Page Miss", "Page Dirty", "Read Rate", "Write Rate", "Read Duration", "Write Duration");
	fprintf(out, "---------------------------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_AUTOVACUUM_ACTIVITY2, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-40s  %10s  %10s  %10s  %7s MiB/s  %7s MiB/s  %12s s  %12s s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Vacuum WAL Statistics (Average) **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-40s  %8s %10s  %10s  %10s\n",
		"Table", "Count", "WAL Records", "WAL FPIs", "WAL bytes" );
	fprintf(out, "-----------------------------------------------------------------------------------------\n");
	
	res = pgut_execute(conn, SQL_SELECT_AUTOVACUUM_WAL_ACTIVITY, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-40s  %8s  %10s  %10s  %10s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4));
	}
	fprintf(out, "\n");
	PQclear(res);


	fprintf(out, "/** Vacuum Index Statistics (Average) **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-40s  %-32s  %8s  %13s  %13s  %13s  %13s\n",
		"Table", "Index", "Count", "Page Total", "Page New Del", "Page Curr Del", "Page Reuse" );
	fprintf(out, "---------------------------------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_AUTOVACUUM_INDEX_ACTIVITY, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-40s  %-32s  %8s  %13s  %13s  %13s  %13s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Analyze Statistics **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-40s  %8s  %15s  %15s  %15s  %-19s  %7s  %13s\n",
		"Table", "Count", "Duration(Total)", "Duration(Avg)",
		"Duration(Max)", "Last Analyze Time", "Cancels", "Mod Rows(Max)");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_AUTOANALYZE_STATS, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-40s  %8s  %13s s  %13s s  %13s s  %-19s  %7s  %13s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7));
	}
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'QueryActivity'
 */
static void
report_query_activity(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Query Activity */\n");
	fprintf(out, "----------------------------------------\n\n");

	fprintf(out, "/** Functions **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-8s  %-16s  %-16s  %-16s  %8s  %13s  %12s  %12s\n",
		"OID", "Database", "Schema", "Function", "Calls", "Total Time",
		"Self Time", "Time/Call");
	fprintf(out, "----------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_QUERY_ACTIVITY_FUNCTIONS, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-8s  %-16s  %-16s  %-16s  %8s  %10s ms  %9s ms  %9s ms\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Statements **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-16s  %8s  %14s  %13s  %11s  %11s  %11s  %11s  %-s\n",
		"", "", "", "", "", "Block", "Block", "Temp Block", "Temp Block", "");
	fprintf(out, "%-16s  %-16s  %8s  %14s  %13s  %11s  %11s  %11s  %11s  %-s\n",
		"User", "Database", "Calls", "Total Time", "Time/Call",
		"Read Time", "Write Time", "Read Time", "Write Time", "Query");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_QUERY_ACTIVITY_STATEMENTS, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  ", PQgetvalue(res, i, 0));
		fprintf(out, "%-16s  ", PQgetvalue(res, i, 1));
		fprintf(out, "%8s  ", PQgetvalue(res, i, 6));
		fprintf(out, "%10s sec  ", PQgetvalue(res, i, 7));
		fprintf(out, "%9s sec  ", PQgetvalue(res, i, 8));
		fprintf(out, "%8s ms  ", PQgetvalue(res, i, 9));
		fprintf(out, "%8s ms  ", PQgetvalue(res, i, 10));
		fprintf(out, "%8s ms  ", PQgetvalue(res, i, 11));
		fprintf(out, "%8s ms  ", PQgetvalue(res, i, 12));
		fprintf(out, "%-s\n", PQgetvalue(res, i, 2));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Statements (rusage) **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-16s  %12s  %12s  %14s  %13s  %12s  %12s  %14s  %13s  %-s\n",
		"User", "Database", "Plan Reads", "Plan Writes", "Plan User Time", "Plan Sys Time",
		"Exec Reads", "Exec Writes", "Exec User Time", "Exec Sys Time", "Query");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------\n");


	res = pgut_execute(conn, SQL_SELECT_QUERY_ACTIVITY_STATEMENTS_RUSAGE, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-16s  %12s  %12s  %14s  %13s  %12s  %12s  %14s  %13s  %-s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7),
			PQgetvalue(res, i, 8),
			PQgetvalue(res, i, 9),
			PQgetvalue(res, i, 10),
			PQgetvalue(res, i, 2));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Plans **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%20s  %10s  %-16s  %-16s  %8s  %14s  %13s  %12s  %12s  %12s  %12s\n",
		"", "", "", "", "", "",
		"", "Block", "Block", "Temp Block", "Temp Block");
	fprintf(out, "%20s  %10s  %-16s  %-16s  %8s  %14s  %13s  %12s  %12s  %12s  %12s\n",
		"Query ID", "Plan ID", "User", "Database", "Calls", "Total Time",
		"Time/Call", "Read Time", "Write Time", "Read Time", "Write Time");
	fprintf(out, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_QUERY_ACTIVITY_PLANS, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%20s  ", PQgetvalue(res, i, 0));
		fprintf(out, "%10s  ", PQgetvalue(res, i, 1));
		fprintf(out, "%-16s  ", PQgetvalue(res, i, 2));
		fprintf(out, "%-16s  ", PQgetvalue(res, i, 3));
		fprintf(out, "%8s  ", PQgetvalue(res, i, 4));
		fprintf(out, "%10s sec  ", PQgetvalue(res, i, 5));
		fprintf(out, "%9s sec  ", PQgetvalue(res, i, 6));
		fprintf(out, "%9s ms  ", PQgetvalue(res, i, 7));
		fprintf(out, "%9s ms  ", PQgetvalue(res, i, 8));
		fprintf(out, "%9s ms  ", PQgetvalue(res, i, 9));
		fprintf(out, "%9s ms\n", PQgetvalue(res, i, 10));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Wait Sampling **/\n");
	fprintf(out, "-----------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_WAIT_SAMPLING, lengthof(params), params);
	for (i = 0; i < PQntuples(res); i++)
	{
		if (strcmp(PQgetvalue(res, i, 11), "1") == 0)
		{
			fprintf(out, "\n");
			fprintf(out, "---------------------------------------------------------------------------------------\n");
			fprintf(out, "%-32s  %-16s  %-16s  %-24s\n",
			"Query ID", "Database", "User Name", "Backend Type");
			fprintf(out, "---------------------------------------------------------------------------------------\n");
			fprintf(out, "%-32s  %-16s  %-16s  %-24s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5));

			fprintf(out, "\n");
			fprintf(out, "\t%-s\n",
			"Query");
			fprintf(out, "\t---------------------------------------------------------------------------------------\n");
			fprintf(out, "\t%-s\n",
			PQgetvalue(res, i, 10));

			fprintf(out, "\n");
			fprintf(out, "\t\tTop 10 Events:\n");
			fprintf(out, "\t\t%-12s  %-32s  %8s  %12s\n",
			"Event Type", "Event", "Count", "Ratio(%)");
			fprintf(out, "\t\t-------------------------------------------------------------------------------\n");
		}
		fprintf(out, "\t\t%-12s  %-32s  %8s  %12s\n",
		PQgetvalue(res, i, 6),
		PQgetvalue(res, i, 7),
		PQgetvalue(res, i, 8),
		PQgetvalue(res, i, 9));
	}
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'LockConflicts'
 */
static void
report_lock_conflicts(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Lock Conflicts */\n");
	fprintf(out, "----------------------------------------\n");
	fprintf(out, "%-16s  %-16s  %-16s  %-8s  %11s  %11s  %-16s\n%-s\n%-s\n",
		"Database", "Schema", "Relation", "Duration", "Blockee PID", "Blocker PID", "Blocker GID",
		"Blockee Query", "Blocker Query");
	fprintf(out, "--------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_LOCK_CONFLICTS, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-16s  %-16s  %-8s  %11s  %11s  %-16s\n%-s\n%-s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7),
			PQgetvalue(res, i, 8));
	}
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'ReplicationActivity'
 */
static void
report_replication_activity(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Replication Activity */\n");
	fprintf(out, "----------------------------------------\n\n");

	fprintf(out, "/** Current Replication Status **/\n");
	fprintf(out, "-----------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_REPLICATION_STATUS, lengthof(params), params);
	for (i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "User Name             : %s\n", PQgetvalue(res, i, 0));
		fprintf(out, "Application Name      : %s\n", PQgetvalue(res, i, 1));
		fprintf(out, "Client Address        : %s\n", PQgetvalue(res, i, 2));
		fprintf(out, "Client Host           : %s\n", PQgetvalue(res, i, 3));
		fprintf(out, "Client Port           : %s\n", PQgetvalue(res, i, 4));
		fprintf(out, "Backend Start         : %s\n", PQgetvalue(res, i, 5));
		fprintf(out, "WAL Sender State      : %s\n", PQgetvalue(res, i, 6));
		fprintf(out, "Current WAL Location  : %s\n", PQgetvalue(res, i, 7));
		fprintf(out, "Sent WAL Location     : %s\n", PQgetvalue(res, i, 8));
		fprintf(out, "Write WAL Location    : %s\n", PQgetvalue(res, i, 9));
		fprintf(out, "Flush WAL Location    : %s\n", PQgetvalue(res, i, 10));
		fprintf(out, "Replay WAL Location   : %s\n", PQgetvalue(res, i, 11));
		fprintf(out, "Sync Priority         : %s\n", PQgetvalue(res, i, 12));
		fprintf(out, "Sync State            : %s\n", PQgetvalue(res, i, 13));
		fprintf(out, "Replay Delay Average  : %s\n", PQgetvalue(res, i, 14));
		fprintf(out, "Replay Delay Peak     : %s\n", PQgetvalue(res, i, 15));
		fprintf(out, "Write Lag Time        : %s\n", PQgetvalue(res, i, 16));
		fprintf(out, "Flush Lag Time        : %s\n", PQgetvalue(res, i, 17));
		fprintf(out, "Replay Lag Time       : %s\n\n", PQgetvalue(res, i, 18));
	}
	if (PQntuples(res) == 0)
		fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Replication Slots Statistics **/\n");
	fprintf(out, "----------------------------------------\n");
	fprintf(out, "%-15s  %-11s  %-15s  %11s  %11s  %12s  %11s  %12s  %12s  %11s  %12s  %-20s\n",
		"Slot Name", "Slot Type", "Slot Database", "Spill Txns", "Spill Count" , "Spill Bytes" , "Stream Txns" , "Stream Count" , "Stream Bytes" , "Total Txns" , "Total Bytes" , "Replication Slots Reset");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_STAT_REPLICATION_SLOTS, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-15s  %-11s  %-15s  %11s  %11s  %12s  %11s  %12s  %12s  %11s  %12s  %-20s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7),
			PQgetvalue(res, i, 8),
			PQgetvalue(res, i, 9),
			PQgetvalue(res, i, 10),
			PQgetvalue(res, i, 11));
	}
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'SettingParameters'
 */
static void
report_setting_parameters(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Setting Parameters */\n");
	fprintf(out, "----------------------------------------\n");
	fprintf(out, "%-32s  %-32s  %-6s  %-s\n",
		"Name", "Setting", "Unit", "Source");
	fprintf(out, "-------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_SETTING_PARAMETERS, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-32s  %-32s  %-6s  %-s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3));
	}
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'SchemaInformation'
 */
static void
report_schema_information(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Schema Information */\n");
	fprintf(out, "----------------------------------------\n\n");

	fprintf(out, "/** Tables **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-16s  %-16s  %8s  %8s  %10s  %10s  %11s  %11s\n",
		"Database", "Schema", "Table", "Columns", "Rows", "Size", "Size Incr",
		"Table Scans", "Index Scans");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_SCHEMA_INFORMATION_TABLES, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-16s  %-16s  %8s  %8s  %6s MiB  %6s MiB  %11s  %11s\n",
			PQgetvalue(res, i, 0),	
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7),
			PQgetvalue(res, i, 8));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Indexes **/\n");
	fprintf(out, "-----------------------------------\n");
	fprintf(out, "%-16s  %-16s  %-16s  %-16s  %10s  %10s  %11s  %9s  %10s  %11s  %-s\n",
		"Database", "Schema", "Index", "Table", "Size", "Size Incr",
		"Index Scans", "Rows/Scan", "Disk Reads", "Cache Reads", "Index Key");
	fprintf(out, "-------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_SCHEMA_INFORMATION_INDEXES, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-16s  %-16s  %-16s  %6s MiB  %6s MiB  %11s  %9s  %10s  %11s  %-s\n",
			PQgetvalue(res, i, 0),	
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7),
			PQgetvalue(res, i, 8),
			PQgetvalue(res, i, 9),
			PQgetvalue(res, i, 10));
	}
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'Alert'
 */
static void
report_alert(PGconn *conn, ReportScope *scope, FILE *out)
{
	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Alert */\n");
	fprintf(out, "----------------------------------------\n");
	print_alert_data(conn, scope, out);
}

/*
 * generate a report that corresponds to 'Alert'
 */
static void
report_alert_section(PGconn *conn, ReportScope *scope, FILE *out)
{
	fprintf(out, "/** Alert **/\n");
	fprintf(out, "-----------------------------------\n");
	print_alert_data(conn, scope, out);
}

static void
print_alert_data(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;

	fprintf(out, "%-19s  %s\n", "DateTime", "Message");
	fprintf(out, "--------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_ALERT, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-19s  %s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1));
	}
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'Profiles'
 */
static void
report_profiles(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Profiles */\n");
	fprintf(out, "----------------------------------------\n");
	fprintf(out, "%-32s  %8s\n",
		"Processing", "Executes");
	fprintf(out, "---------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_PROFILES, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-32s  %8s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1));
	}
	fprintf(out, "\n");
	PQclear(res);
}

/*
 * generate a report that corresponds to 'Hardware Information'
 */
static void
report_hardware_info(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;
#ifdef __CSV__
	FILE		*tout = stdout;
	char *f;
#endif

	fprintf(out, "----------------------------------------\n");
	fprintf(out, "/* Hardware Information */\n");
	fprintf(out, "----------------------------------------\n\n");

	fprintf(out, "/** CPU Information **/\n");
	fprintf(out, "-----------------------------------\n");

	fprintf(out, "%-16s  %-12s  %-48s  %8s  %3s  %12s  %12s  %6s\n",
		"DateTime", "Vendor", "Model name", "CPU MHz", "CPU", "Threads/core", "Cores/socket", "Socket");
	fprintf(out, "-------------------------------------------------------------------------------------------------------------------------------------\n");

	res = pgut_execute(conn, SQL_SELECT_CPU_INFO, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %-12s  %-48s  %8s  %3s  %12s  %12s  %6s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1),
			PQgetvalue(res, i, 2),
			PQgetvalue(res, i, 3),
			PQgetvalue(res, i, 4),
			PQgetvalue(res, i, 5),
			PQgetvalue(res, i, 6),
			PQgetvalue(res, i, 7));
	}
	fprintf(out, "\n");
	PQclear(res);

	fprintf(out, "/** Memory Information **/\n");
	fprintf(out, "-----------------------------------\n");
	
	fprintf(out, "%-16s  %13s\n", "DateTime", "System memory");
	fprintf(out, "---------------------------------\n");

#ifdef __CSV__
	f = create_filename("TS-memory-info.csv");
	if ((tout = fopen(f, "w+")) == NULL)
		ereport(ERROR,
				(errcode_errno(),
				 errmsg("could not open file : '%s'", f)));

	fprintf(tout, "%s,%s\n", "DateTime", "Number of User");
#endif

	res = pgut_execute(conn, SQL_SELECT_MEM_INFO, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %13s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1));
#ifdef __CSV__
		fprintf(tout, "%-16s,%s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1));
#endif
	}
#ifdef __CSV__
	if (tout != stdout)
		fclose(tout);
#endif
	fprintf(out, "\n");
	PQclear(res);
}

#ifdef __USER__
/*
 * generate number of users report
 */
static void
report_num_user(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;
#ifdef __CSV__
	FILE		*tout = stdout;
	char *f;
#endif

	fprintf(out, "---------------------------------\n");
	fprintf(out, "/*   Users                     */\n");
	fprintf(out, "---------------------------------\n\n");

	fprintf(out, "%-16s  %13s\n", "DateTime", "Number of User");
	fprintf(out, "---------------------------------\n");

#ifdef __CSV__
	f = create_filename("TS-num-user.csv");
	if ((tout = fopen(f, "w+")) == NULL)
		ereport(ERROR,
				(errcode_errno(),
				 errmsg("could not open file : '%s'", f)));

	fprintf(tout, "%s,%s\n", "DateTime", "Number of User");
#endif

	res = pgut_execute(conn, SQL_SELECT_NUM_USER, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s  %13s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1));
#ifdef __CSV__
		fprintf(tout, "%-16s,%s\n",
			PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1));
#endif
	}
#ifdef __CSV__
	if (tout != stdout)
		fclose(tout);
#endif
	fprintf(out, "\n");
	PQclear(res);
}
#endif


#ifdef __TABLE__
/*
 *
 */
static void
report_table_stat(PGconn *conn, ReportScope *scope, FILE *out)
{
	PGresult	*res;
	const char	*params[] = { scope->beginid, scope->endid };
	int			 i;
#ifdef __CSV__
	FILE		*tout = stdout;
	char *f;
#endif

	fprintf(out, "---------------------------------\n");
	fprintf(out, "/*   Table                      */\n");
	fprintf(out, "---------------------------------\n\n");

	fprintf(out, "%-16s %13s %13s %20s %13s %13s ",
			"DateTime", "database", "schema", "table", "reltuples", "size");
	fprintf(out, "%13s %13s %13s %15s %13s %13s %13s ",
			"seq_scan",	"seq_tup_read",	"idx_scan", "idx_tup_fetch", "n_tup_ins", "n_tup_upd", "n_tup_del");
	fprintf(out, "%15s %13s %13s %20s %20s %20s ",
			"n_tup_hot_upd", "n_live_tup", "n_dead_tup", "n_mod_since_analyze", "heap_blks_read", "heap_blks_hit");
	fprintf(out, "%15s %15s %20s %20s %20s %20s\n",
			"idx_blks_read", "idx_blks_hit", "toast_blks_read", "toast_blks_hit", "tidx_blks_read", "tidx_blks_hit");
	fprintf(out, "---------------------------------\n");

#ifdef __CSV__
	f = create_filename("TS-table-stat.csv");
	if ((tout = fopen(f, "w+")) == NULL)
		ereport(ERROR,
				(errcode_errno(),
				 errmsg("could not open file : '%s'", f)));

	fprintf(tout, "%s,%s,%s,%s,%s,%s,",
			"DateTime", "database", "schema", "table", "reltuples", "size");
	fprintf(tout, "%s,%s,%s,%s,%s,%s,%s,",
			"seq_scan",	"seq_tup_read",	"idx_scan", "idx_tup_fetch", "n_tup_ins", "n_tup_upd", "n_tup_del");
	fprintf(tout, "%s,%s,%s,%s,%s,%s,",
			"n_tup_hot_upd", "n_live_tup", "n_dead_tup", "n_mod_since_analyze", "heap_blks_read", "heap_blks_hit");
	fprintf(tout, "%s,%s,%s,%s,%s,%s\n",
			"idx_blks_read", "idx_blks_hit", "toast_blks_read", "toast_blks_hit", "tidx_blks_read", "tidx_blks_hit");
#endif

	res = pgut_execute(conn, SQL_SELECT_TABLE_STAT, lengthof(params), params);
	for(i = 0; i < PQntuples(res); i++)
	{
		fprintf(out, "%-16s %13s %13s %20s %13s %13s %13s %13s %13s %15s %13s %13s %13s %15s %13s %13s %20s %20s %20s %15s %15s %20s %20s %20s %20s\n",
				PQgetvalue(res, i, 0),
				PQgetvalue(res, i, 1),
				PQgetvalue(res, i, 2),
				PQgetvalue(res, i, 3),
				PQgetvalue(res, i, 4),
				PQgetvalue(res, i, 5),
				PQgetvalue(res, i, 6),
				PQgetvalue(res, i, 7),
				PQgetvalue(res, i, 8),
				PQgetvalue(res, i, 9),
				PQgetvalue(res, i, 10),
				PQgetvalue(res, i, 11),
				PQgetvalue(res, i, 12),
				PQgetvalue(res, i, 13),
				PQgetvalue(res, i, 14),
				PQgetvalue(res, i, 15),
				PQgetvalue(res, i, 16),
				PQgetvalue(res, i, 17),
				PQgetvalue(res, i, 18),
				PQgetvalue(res, i, 19),
				PQgetvalue(res, i, 20),
				PQgetvalue(res, i, 21),
				PQgetvalue(res, i, 22),
				PQgetvalue(res, i, 23),
				PQgetvalue(res, i, 24));

#ifdef __CSV__
		fprintf(tout, "%-16s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
				PQgetvalue(res, i, 0),
				PQgetvalue(res, i, 1),
				PQgetvalue(res, i, 2),
				PQgetvalue(res, i, 3),
				PQgetvalue(res, i, 4),
				PQgetvalue(res, i, 5),
				PQgetvalue(res, i, 6),
				PQgetvalue(res, i, 7),
				PQgetvalue(res, i, 8),
				PQgetvalue(res, i, 9),
				PQgetvalue(res, i, 10),
				PQgetvalue(res, i, 11),
				PQgetvalue(res, i, 12),
				PQgetvalue(res, i, 13),
				PQgetvalue(res, i, 14),
				PQgetvalue(res, i, 15),
				PQgetvalue(res, i, 16),
				PQgetvalue(res, i, 17),
				PQgetvalue(res, i, 18),
				PQgetvalue(res, i, 19),
				PQgetvalue(res, i, 20),
				PQgetvalue(res, i, 21),
				PQgetvalue(res, i, 22),
				PQgetvalue(res, i, 23),
				PQgetvalue(res, i, 24));
#endif
	}
#ifdef __CSV__
	if (tout != stdout)
		fclose(tout);
#endif
	fprintf(out, "\n");
	PQclear(res);
}
#endif

/*
 * generate a report that corresponds to 'All'
 */
static void
report_all(PGconn *conn, ReportScope *scope, FILE *out)
{
	report_summary(conn, scope, out);
	report_alert_section(conn, scope, out);
	report_database_statistics(conn, scope, out);
	report_instance_activity(conn, scope, out);
	report_resource_usage(conn, scope, out);
	report_disk_usage(conn, scope, out);
	report_long_transactions(conn, scope, out);
	report_notable_tables(conn, scope, out);
	report_checkpoint_activity(conn, scope, out);
	report_autovacuum_activity(conn, scope, out);
	report_query_activity(conn, scope, out);
	report_lock_conflicts(conn, scope, out);
	report_replication_activity(conn, scope, out);
	report_setting_parameters(conn, scope, out);
	report_schema_information(conn, scope, out);
	report_profiles(conn, scope, out);
	report_hardware_info(conn, scope, out);
#ifdef __USER__
	report_num_user(conn, scope, out);
#endif
#ifdef __TABLE__
	report_table_stat(conn, scope, out);
#endif
}

/*
 * parse the report ID and determine a report builder
 */
static ReportBuild
parse_reportid(const char *value)
{
	const char *v = value;
	size_t		len;

	/* null input is 'All'. */
	if (v == NULL)
		return (ReportBuild) report_all;

	/* skip blank */
	while (IsSpace(*v)) { v++; }
	if ((len = strlen(v)) == 0)
		ereport(ERROR,
			(errcode(EINVAL),
			 errmsg("invalid report ID: '%s'", value)));

	/* Do a prefix match. For example, "su" means 'Summary' */
	if (pg_strncasecmp(REPORTID_SUMMARY, v, len) == 0)
		return (ReportBuild) report_summary;
	else if (pg_strncasecmp(REPORTID_DATABASE_STATISTICS, v, len) == 0)
		return (ReportBuild) report_database_statistics;
	else if (pg_strncasecmp(REPORTID_INSTANCE_ACTIVITY, v, len) == 0)
		return (ReportBuild) report_instance_activity;
	else if (pg_strncasecmp(REPORTID_OS_RESOURCE_USAGE, v, len) == 0)
		return (ReportBuild) report_resource_usage;
	else if (pg_strncasecmp(REPORTID_DISK_USAGE, v, len) == 0)
		return (ReportBuild) report_disk_usage;
	else if (pg_strncasecmp(REPORTID_LONG_TRANSACTIONS, v, len) == 0)
		return (ReportBuild) report_long_transactions;
	else if (pg_strncasecmp(REPORTID_NOTABLE_TABLES, v, len) == 0)
		return (ReportBuild) report_notable_tables;
	else if (pg_strncasecmp(REPORTID_CHECKPOINT_ACTIVITY, v, len) == 0)
		return (ReportBuild) report_checkpoint_activity;
	else if (pg_strncasecmp(REPORTID_AUTOVACUUM_ACTIVITY, v, len) == 0)
		return (ReportBuild) report_autovacuum_activity;
	else if (pg_strncasecmp(REPORTID_QUERY_ACTIVITY, v, len) == 0)
		return (ReportBuild) report_query_activity;
	else if (pg_strncasecmp(REPORTID_LOCK_CONFLICTS, v, len) == 0)
		return (ReportBuild) report_lock_conflicts;
	else if (pg_strncasecmp(REPORTID_REPLICATION_ACTIVITY, v, len) == 0)
		return (ReportBuild) report_replication_activity;
	else if (pg_strncasecmp(REPORTID_SETTING_PARAMETERS, v, len) == 0)
		return (ReportBuild) report_setting_parameters;
	else if (pg_strncasecmp(REPORTID_SCHEMA_INFORMATION, v, len) == 0)
		return (ReportBuild) report_schema_information;
	else if (pg_strncasecmp(REPORTID_ALERT, v, len) == 0)
		return (ReportBuild) report_alert;
	else if (pg_strncasecmp(REPORTID_PROFILES, v, len) == 0)
		return (ReportBuild) report_profiles;
	else if (pg_strncasecmp(REPORTID_HARDWARE_INFO, v, len) == 0)
		return (ReportBuild) report_hardware_info;
#ifdef __USER__
	else if (pg_strncasecmp(REPORTID_NUM_USER, v, len) == 0)
		return (ReportBuild) report_num_user;
#endif
#ifdef __TABLE__
	else if (pg_strncasecmp(REPORTID_TABLE_STAT, v, len) == 0)
		return (ReportBuild) report_table_stat;
#endif
	else if (pg_strncasecmp(REPORTID_ALL, v, len) == 0)
		return (ReportBuild) report_all;

	ereport(ERROR,
		(errcode(EINVAL),
		 errmsg("invalid report ID: '%s'", value)));
	return NULL;
}

/*
 * examine the report scope for each instance from the range
 * specified by snapshot ID
 */
static List *
select_scope_by_snapid(PGconn *conn, const char *beginid, const char *endid)
{
	List		*scope_list = NIL;
	PGresult	*res;
	char		 b_id[64];
	char		 e_id[64];
	const char	*params[2] = { b_id, e_id };
	int			 i;

	if (beginid)
		strncpy(b_id, beginid, sizeof(b_id));
	else
		/* set the oldest snapshot to begin point */
		strncpy(b_id, SNAPSHOTID_MIN, sizeof(b_id));
	if (endid)
		strncpy(e_id, endid, sizeof(e_id));
	else
		/* set the lastest snapshot to end point */
		strncpy(e_id, SNAPSHOTID_MAX, sizeof(e_id));

	res = pgut_execute(conn, SQL_SELECT_REPORT_SCOPE_BY_SNAPID, lengthof(params), params);
	for (i = 0; i < PQntuples(res); i++)
	{
		ReportScope	*scope;

		scope = pgut_malloc(sizeof(ReportScope));
		scope->instid = pgut_strdup(PQgetvalue(res, i, 0));		/* instance ID */
		scope->host = pgut_strdup(PQgetvalue(res, i, 1));		/* host */
		scope->port = pgut_strdup(PQgetvalue(res, i, 2));		/* port */
		scope->version = parse_version(PQgetvalue(res, i, 3));	/* PostgreSQL version */
		scope->beginid = pgut_strdup(PQgetvalue(res, i, 4));	/* begin point of report */
		scope->endid = pgut_strdup(PQgetvalue(res, i, 5));		/* end point of report */
		scope_list = lappend(scope_list, scope);
	}
	PQclear(res);
	return scope_list;
}

/*
 * examine the report scope for each instance from the range
 * specified by timestamp
 */
static List *
select_scope_by_timestamp(PGconn *conn, time_t begindate, time_t enddate)
{
	List		*scope_list = NIL;
	PGresult	*res;
	char		 b_date[64];
	char		 e_date[64];
	const char	*params[2] = { b_date, e_date };
	int			 i;

	if (begindate != -1)
		strftime(b_date, sizeof(b_date),
			"%Y-%m-%d %H:%M:%S", localtime(&begindate));
	else
		/* set the oldest snapshot to begin point */
		strncpy(b_date, TIMESTAMP_MIN, sizeof(b_date));
	if (enddate != -1)
		strftime(e_date, sizeof(e_date),
			"%Y-%m-%d %H:%M:%S", localtime(&enddate));
	else
		/* set the lastest snapshot to end point */
		strncpy(e_date, TIMESTAMP_MAX, sizeof(e_date));

	res = pgut_execute(conn, SQL_SELECT_REPORT_SCOPE_BY_TIMESTAMP, lengthof(params), params);
	for (i = 0; i < PQntuples(res); i++)
	{
		ReportScope	*scope;

		scope = pgut_malloc(sizeof(ReportScope));
		scope->instid = pgut_strdup(PQgetvalue(res, i, 0));		/* instance ID */
		scope->host = pgut_strdup(PQgetvalue(res, i, 1));		/* host */
		scope->port = pgut_strdup(PQgetvalue(res, i, 2));		/* port */
		scope->version = parse_version(PQgetvalue(res, i, 3));	/* PostgreSQL version */
		scope->beginid = pgut_strdup(PQgetvalue(res, i, 4));	/* begin point of report */
		scope->endid = pgut_strdup(PQgetvalue(res, i, 5));		/* end point of report */
		scope_list = lappend(scope_list, scope);
	}
	PQclear(res);
	return scope_list;
}

static void
destroy_report_scope(ReportScope *scope)
{
	free(scope->instid);
	free(scope->host);
	free(scope->port);
	free(scope->beginid);
	free(scope->endid);
	free(scope);
}

static int
get_server_version(PGconn *conn)
{
	PGresult	*res;
	int			 server_version_num;

	res = pgut_execute(conn, "SHOW server_version_num", 0, NULL);
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
		server_version_num = -1;
	else
		server_version_num = atoi(PQgetvalue(res, 0, 0));
	PQclear(res);

	return server_version_num;
}

static int
parse_version(const char *versionString)
{
	int	cnt;
	int	vmaj, vmin, vrev;

	cnt = sscanf(versionString, "%d.%d.%d", &vmaj, &vmin, &vrev);

	if (cnt == 0)
		return -1;

	if (vmaj >= 10)
	{
		if (cnt == 1)
			vmin = 0;
		return 100 * 100 * vmaj + vmin;
	}

	if (cnt == 2)
		vrev = 0;

	return (100 * vmaj + vmin) * 100 + vrev;
}
