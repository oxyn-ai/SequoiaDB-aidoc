#ifndef PISA_PERFORMANCE_MONITOR_H
#define PISA_PERFORMANCE_MONITOR_H

#include "postgres.h"
#include "fmgr.h"
#include "utils/jsonb.h"
#include "utils/timestamp.h"
#include "storage/lwlock.h"

#define PISA_PERF_MAX_METRICS 1000
#define PISA_PERF_HISTORY_SIZE 100

typedef enum PisaMetricType
{
    PISA_METRIC_QUERY_LATENCY,
    PISA_METRIC_INDEX_BUILD_TIME,
    PISA_METRIC_MEMORY_USAGE,
    PISA_METRIC_DISK_IO,
    PISA_METRIC_CACHE_HIT_RATIO,
    PISA_METRIC_THROUGHPUT,
    PISA_METRIC_ERROR_RATE,
    PISA_METRIC_COMPRESSION_RATIO
} PisaMetricType;

typedef struct PisaMetricValue
{
    double value;
    TimestampTz timestamp;
    char *context;
} PisaMetricValue;

typedef struct PisaPerformanceMetric
{
    PisaMetricType type;
    char metric_name[64];
    PisaMetricValue history[PISA_PERF_HISTORY_SIZE];
    int history_index;
    int history_count;
    double min_value;
    double max_value;
    double avg_value;
    double threshold_warning;
    double threshold_critical;
    bool alert_enabled;
    TimestampTz last_updated;
} PisaPerformanceMetric;

typedef struct PisaAlert
{
    char alert_id[64];
    PisaMetricType metric_type;
    char *message;
    char *severity;
    TimestampTz created_at;
    TimestampTz resolved_at;
    bool is_active;
    int occurrence_count;
} PisaAlert;

typedef struct PisaPerformanceStats
{
    int64 total_queries;
    int64 successful_queries;
    int64 failed_queries;
    double avg_query_latency_ms;
    double avg_index_build_time_ms;
    int64 total_memory_usage_bytes;
    int64 total_disk_usage_bytes;
    double cache_hit_ratio;
    int active_alerts_count;
    TimestampTz stats_period_start;
    TimestampTz stats_period_end;
} PisaPerformanceStats;

void InitializePisaPerformanceMonitor(void);
void ShutdownPisaPerformanceMonitor(void);

bool RecordMetric(PisaMetricType type, double value, const char *context);
bool SetMetricThreshold(PisaMetricType type, double warning_threshold, double critical_threshold);
PisaPerformanceMetric *GetMetric(PisaMetricType type);
List *GetAllMetrics(void);

bool CreateAlert(PisaMetricType metric_type, const char *message, const char *severity);
bool ResolveAlert(const char *alert_id);
List *GetActiveAlerts(void);
List *GetAlertHistory(TimestampTz start_time, TimestampTz end_time);

PisaPerformanceStats *GetPerformanceStatistics(TimestampTz start_time, TimestampTz end_time);
void ResetPerformanceStatistics(void);

void StartQueryTimer(const char *query_id);
void EndQueryTimer(const char *query_id, bool success);
void RecordMemoryUsage(int64 bytes_allocated, int64 bytes_freed);
void RecordDiskIO(int64 bytes_read, int64 bytes_written);

bool OptimizeMemoryUsage(void);
bool OptimizeDiskIO(void);
void TriggerPerformanceOptimization(void);

void MonitorSystemResources(void);
void CheckPerformanceThresholds(void);
void GeneratePerformanceReport(TimestampTz start_time, TimestampTz end_time, Jsonb **report);

PG_FUNCTION_INFO_V1(documentdb_record_pisa_metric);
PG_FUNCTION_INFO_V1(documentdb_get_pisa_metrics);
PG_FUNCTION_INFO_V1(documentdb_set_pisa_metric_threshold);
PG_FUNCTION_INFO_V1(documentdb_get_pisa_alerts);
PG_FUNCTION_INFO_V1(documentdb_get_pisa_performance_stats);
PG_FUNCTION_INFO_V1(documentdb_optimize_pisa_performance);
PG_FUNCTION_INFO_V1(documentdb_generate_pisa_performance_report);

#endif
