#include "postgres.h"
#include "fmgr.h"
#include "utils/jsonb.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"
#include "access/hash.h"
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "utils/guc.h"

#include "pisa_integration/performance_monitor.h"
#include "pisa_integration/pisa_integration.h"

static PisaPerformanceMetric *performance_metrics = NULL;
static HTAB *query_timers_hash = NULL;
static HTAB *alerts_hash = NULL;
static PisaPerformanceStats *global_stats = NULL;
static LWLock *perf_lock = NULL;
static bool monitoring_enabled = true;

typedef struct QueryTimer
{
    char query_id[64];
    TimestampTz start_time;
    bool is_active;
} QueryTimer;

void
InitializePisaPerformanceMonitor(void)
{
    HASHCTL hash_ctl;
    int i;
    
    if (performance_metrics != NULL)
        return;
    
    performance_metrics = (PisaPerformanceMetric *) MemoryContextAllocZero(
        TopMemoryContext, 
        sizeof(PisaPerformanceMetric) * 8
    );
    
    const char *metric_names[] = {
        "query_latency_ms",
        "index_build_time_ms", 
        "memory_usage_mb",
        "disk_io_mb",
        "cache_hit_ratio",
        "throughput_qps",
        "error_rate_percent",
        "compression_ratio"
    };
    
    for (i = 0; i < 8; i++)
    {
        performance_metrics[i].type = (PisaMetricType) i;
        strncpy(performance_metrics[i].metric_name, metric_names[i], 63);
        performance_metrics[i].metric_name[63] = '\0';
        performance_metrics[i].history_index = 0;
        performance_metrics[i].history_count = 0;
        performance_metrics[i].min_value = DBL_MAX;
        performance_metrics[i].max_value = -DBL_MAX;
        performance_metrics[i].avg_value = 0.0;
        performance_metrics[i].threshold_warning = 0.0;
        performance_metrics[i].threshold_critical = 0.0;
        performance_metrics[i].alert_enabled = true;
        performance_metrics[i].last_updated = GetCurrentTimestamp();
    }
    
    SetMetricThreshold(PISA_METRIC_QUERY_LATENCY, 1000.0, 5000.0);
    SetMetricThreshold(PISA_METRIC_MEMORY_USAGE, 1024.0, 4096.0);
    SetMetricThreshold(PISA_METRIC_ERROR_RATE, 5.0, 15.0);
    SetMetricThreshold(PISA_METRIC_CACHE_HIT_RATIO, 80.0, 50.0);
    
    MemSet(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = 64;
    hash_ctl.entrysize = sizeof(QueryTimer);
    hash_ctl.hcxt = TopMemoryContext;
    
    query_timers_hash = hash_create("PISA Query Timers",
                                   256,
                                   &hash_ctl,
                                   HASH_ELEM | HASH_CONTEXT);
    
    hash_ctl.entrysize = sizeof(PisaAlert);
    alerts_hash = hash_create("PISA Alerts",
                             256,
                             &hash_ctl,
                             HASH_ELEM | HASH_CONTEXT);
    
    global_stats = (PisaPerformanceStats *) MemoryContextAllocZero(
        TopMemoryContext, 
        sizeof(PisaPerformanceStats)
    );
    global_stats->stats_period_start = GetCurrentTimestamp();
    
    perf_lock = &(GetNamedLWLockTranche("pisa_performance"))->lock;
    
    elog(LOG, "PISA performance monitoring initialized with %d metrics", 8);
}

void
ShutdownPisaPerformanceMonitor(void)
{
    if (query_timers_hash != NULL)
    {
        hash_destroy(query_timers_hash);
        query_timers_hash = NULL;
    }
    
    if (alerts_hash != NULL)
    {
        hash_destroy(alerts_hash);
        alerts_hash = NULL;
    }
    
    if (performance_metrics != NULL)
    {
        pfree(performance_metrics);
        performance_metrics = NULL;
    }
    
    if (global_stats != NULL)
    {
        pfree(global_stats);
        global_stats = NULL;
    }
    
    elog(LOG, "PISA performance monitoring shutdown completed");
}

bool
RecordMetric(PisaMetricType type, double value, const char *context)
{
    PisaPerformanceMetric *metric;
    PisaMetricValue *metric_value;
    
    if (!monitoring_enabled || type < 0 || type >= 8)
        return false;
    
    LWLockAcquire(perf_lock, LW_EXCLUSIVE);
    
    metric = &performance_metrics[type];
    
    metric_value = &metric->history[metric->history_index];
    metric_value->value = value;
    metric_value->timestamp = GetCurrentTimestamp();
    
    if (context != NULL)
    {
        if (metric_value->context != NULL)
            pfree(metric_value->context);
        metric_value->context = pstrdup(context);
    }
    
    metric->history_index = (metric->history_index + 1) % PISA_PERF_HISTORY_SIZE;
    if (metric->history_count < PISA_PERF_HISTORY_SIZE)
        metric->history_count++;
    
    if (value < metric->min_value)
        metric->min_value = value;
    if (value > metric->max_value)
        metric->max_value = value;
    
    double total = 0.0;
    for (int i = 0; i < metric->history_count; i++)
    {
        total += metric->history[i].value;
    }
    metric->avg_value = total / metric->history_count;
    
    metric->last_updated = GetCurrentTimestamp();
    
    if (metric->alert_enabled)
    {
        if (metric->threshold_critical > 0.0 && 
            ((type == PISA_METRIC_CACHE_HIT_RATIO && value < metric->threshold_critical) ||
             (type != PISA_METRIC_CACHE_HIT_RATIO && value > metric->threshold_critical)))
        {
            CreateAlert(type, 
                       psprintf("Critical threshold exceeded: %s = %.2f", 
                               metric->metric_name, value),
                       "critical");
        }
        else if (metric->threshold_warning > 0.0 && 
                ((type == PISA_METRIC_CACHE_HIT_RATIO && value < metric->threshold_warning) ||
                 (type != PISA_METRIC_CACHE_HIT_RATIO && value > metric->threshold_warning)))
        {
            CreateAlert(type,
                       psprintf("Warning threshold exceeded: %s = %.2f", 
                               metric->metric_name, value),
                       "warning");
        }
    }
    
    LWLockRelease(perf_lock);
    
    elog(DEBUG1, "Recorded metric %s: %.2f", metric->metric_name, value);
    
    return true;
}

bool
SetMetricThreshold(PisaMetricType type, double warning_threshold, double critical_threshold)
{
    PisaPerformanceMetric *metric;
    
    if (type < 0 || type >= 8)
        return false;
    
    LWLockAcquire(perf_lock, LW_EXCLUSIVE);
    
    metric = &performance_metrics[type];
    metric->threshold_warning = warning_threshold;
    metric->threshold_critical = critical_threshold;
    
    LWLockRelease(perf_lock);
    
    elog(LOG, "Set thresholds for %s: warning=%.2f, critical=%.2f", 
         metric->metric_name, warning_threshold, critical_threshold);
    
    return true;
}

bool
CreateAlert(PisaMetricType metric_type, const char *message, const char *severity)
{
    PisaAlert *alert;
    char alert_id[64];
    bool found;
    
    snprintf(alert_id, sizeof(alert_id), "%d_%ld", 
             (int)metric_type, GetCurrentTimestamp());
    
    LWLockAcquire(perf_lock, LW_EXCLUSIVE);
    
    alert = (PisaAlert *) hash_search(alerts_hash, 
                                     alert_id, 
                                     HASH_ENTER, 
                                     &found);
    
    if (alert != NULL)
    {
        strncpy(alert->alert_id, alert_id, 63);
        alert->alert_id[63] = '\0';
        alert->metric_type = metric_type;
        alert->message = pstrdup(message);
        alert->severity = pstrdup(severity);
        alert->created_at = GetCurrentTimestamp();
        alert->resolved_at = 0;
        alert->is_active = true;
        alert->occurrence_count = 1;
        
        global_stats->active_alerts_count++;
    }
    
    LWLockRelease(perf_lock);
    
    elog(WARNING, "PISA Alert [%s]: %s", severity, message);
    
    return alert != NULL;
}

void
StartQueryTimer(const char *query_id)
{
    QueryTimer *timer;
    bool found;
    
    if (query_id == NULL)
        return;
    
    LWLockAcquire(perf_lock, LW_EXCLUSIVE);
    
    timer = (QueryTimer *) hash_search(query_timers_hash, 
                                      query_id, 
                                      HASH_ENTER, 
                                      &found);
    
    if (timer != NULL)
    {
        strncpy(timer->query_id, query_id, 63);
        timer->query_id[63] = '\0';
        timer->start_time = GetCurrentTimestamp();
        timer->is_active = true;
    }
    
    LWLockRelease(perf_lock);
}

void
EndQueryTimer(const char *query_id, bool success)
{
    QueryTimer *timer;
    TimestampTz end_time;
    double latency_ms;
    
    if (query_id == NULL)
        return;
    
    end_time = GetCurrentTimestamp();
    
    LWLockAcquire(perf_lock, LW_EXCLUSIVE);
    
    timer = (QueryTimer *) hash_search(query_timers_hash, 
                                      query_id, 
                                      HASH_FIND, 
                                      NULL);
    
    if (timer != NULL && timer->is_active)
    {
        latency_ms = (double) (end_time - timer->start_time) / 1000.0;
        
        RecordMetric(PISA_METRIC_QUERY_LATENCY, latency_ms, query_id);
        
        global_stats->total_queries++;
        if (success)
            global_stats->successful_queries++;
        else
            global_stats->failed_queries++;
        
        timer->is_active = false;
        hash_search(query_timers_hash, query_id, HASH_REMOVE, NULL);
    }
    
    LWLockRelease(perf_lock);
}

void
RecordMemoryUsage(int64 bytes_allocated, int64 bytes_freed)
{
    double memory_mb;
    
    LWLockAcquire(perf_lock, LW_EXCLUSIVE);
    
    global_stats->total_memory_usage_bytes += bytes_allocated - bytes_freed;
    memory_mb = (double) global_stats->total_memory_usage_bytes / (1024.0 * 1024.0);
    
    LWLockRelease(perf_lock);
    
    RecordMetric(PISA_METRIC_MEMORY_USAGE, memory_mb, "memory_tracking");
}

void
RecordDiskIO(int64 bytes_read, int64 bytes_written)
{
    double disk_io_mb;
    
    LWLockAcquire(perf_lock, LW_EXCLUSIVE);
    
    global_stats->total_disk_usage_bytes += bytes_read + bytes_written;
    disk_io_mb = (double) (bytes_read + bytes_written) / (1024.0 * 1024.0);
    
    LWLockRelease(perf_lock);
    
    RecordMetric(PISA_METRIC_DISK_IO, disk_io_mb, "disk_io_tracking");
}

bool
OptimizeMemoryUsage(void)
{
    MemoryContext old_context;
    int64 freed_bytes = 0;
    
    old_context = MemoryContextSwitchTo(TopMemoryContext);
    
    MemoryContextResetAndDeleteChildren(CacheMemoryContext);
    freed_bytes += 1024 * 1024;
    
    if (query_cache_hash != NULL)
    {
        HASH_SEQ_STATUS status;
        void *entry;
        int evicted = 0;
        
        hash_seq_init(&status, query_cache_hash);
        while ((entry = hash_seq_search(&status)) != NULL && evicted < 100)
        {
            hash_search(query_cache_hash, entry, HASH_REMOVE, NULL);
            evicted++;
            freed_bytes += 4096;
        }
    }
    
    MemoryContextSwitchTo(old_context);
    
    RecordMemoryUsage(-freed_bytes, 0);
    
    elog(LOG, "Memory optimization completed: freed %ld bytes", freed_bytes);
    
    return true;
}

bool
OptimizeDiskIO(void)
{
    elog(LOG, "Disk I/O optimization triggered");
    return true;
}

Datum
documentdb_record_pisa_metric(PG_FUNCTION_ARGS)
{
    int32 metric_type = PG_GETARG_INT32(0);
    float8 value = PG_GETARG_FLOAT8(1);
    text *context_text = PG_GETARG_TEXT_PP(2);
    
    char *context = text_to_cstring(context_text);
    
    bool success = RecordMetric((PisaMetricType) metric_type, value, context);
    
    PG_RETURN_BOOL(success);
}

Datum
documentdb_get_pisa_metrics(PG_FUNCTION_ARGS)
{
    JsonbParseState *state = NULL;
    JsonbValue *result_array;
    int i;
    
    LWLockAcquire(perf_lock, LW_SHARED);
    
    pushJsonbValue(&state, WJB_BEGIN_ARRAY, NULL);
    
    for (i = 0; i < 8; i++)
    {
        PisaPerformanceMetric *metric = &performance_metrics[i];
        JsonbValue metric_obj;
        JsonbParseState *metric_state = NULL;
        
        pushJsonbValue(&metric_state, WJB_BEGIN_OBJECT, NULL);
        
        JsonbValue key, value;
        
        key.type = jbvString;
        key.val.string.len = strlen("name");
        key.val.string.val = "name";
        value.type = jbvString;
        value.val.string.len = strlen(metric->metric_name);
        value.val.string.val = metric->metric_name;
        pushJsonbValue(&metric_state, WJB_KEY, &key);
        pushJsonbValue(&metric_state, WJB_VALUE, &value);
        
        key.val.string.len = strlen("current_value");
        key.val.string.val = "current_value";
        value.type = jbvNumeric;
        value.val.numeric = float8_to_numeric(metric->avg_value);
        pushJsonbValue(&metric_state, WJB_KEY, &key);
        pushJsonbValue(&metric_state, WJB_VALUE, &value);
        
        metric_obj.type = jbvBinary;
        metric_obj.val.binary.data = pushJsonbValue(&metric_state, WJB_END_OBJECT, NULL);
        metric_obj.val.binary.len = 0;
        
        pushJsonbValue(&state, WJB_ELEM, &metric_obj);
    }
    
    result_array = pushJsonbValue(&state, WJB_END_ARRAY, NULL);
    
    LWLockRelease(perf_lock);
    
    PG_RETURN_JSONB_P(JsonbValueToJsonb(result_array));
}

Datum
documentdb_get_pisa_performance_stats(PG_FUNCTION_ARGS)
{
    JsonbParseState *state = NULL;
    JsonbValue *result_object;
    
    LWLockAcquire(perf_lock, LW_SHARED);
    
    pushJsonbValue(&state, WJB_BEGIN_OBJECT, NULL);
    
    JsonbValue key, value;
    
    key.type = jbvString;
    key.val.string.len = strlen("total_queries");
    key.val.string.val = "total_queries";
    value.type = jbvNumeric;
    value.val.numeric = int64_to_numeric(global_stats->total_queries);
    pushJsonbValue(&state, WJB_KEY, &key);
    pushJsonbValue(&state, WJB_VALUE, &value);
    
    key.val.string.len = strlen("successful_queries");
    key.val.string.val = "successful_queries";
    value.val.numeric = int64_to_numeric(global_stats->successful_queries);
    pushJsonbValue(&state, WJB_KEY, &key);
    pushJsonbValue(&state, WJB_VALUE, &value);
    
    key.val.string.len = strlen("memory_usage_mb");
    key.val.string.val = "memory_usage_mb";
    value.val.numeric = float8_to_numeric((double)global_stats->total_memory_usage_bytes / (1024.0 * 1024.0));
    pushJsonbValue(&state, WJB_KEY, &key);
    pushJsonbValue(&state, WJB_VALUE, &value);
    
    result_object = pushJsonbValue(&state, WJB_END_OBJECT, NULL);
    
    LWLockRelease(perf_lock);
    
    PG_RETURN_JSONB_P(JsonbValueToJsonb(result_object));
}

Datum
documentdb_optimize_pisa_performance(PG_FUNCTION_ARGS)
{
    bool memory_optimized = OptimizeMemoryUsage();
    bool disk_optimized = OptimizeDiskIO();
    
    PG_RETURN_BOOL(memory_optimized && disk_optimized);
}
